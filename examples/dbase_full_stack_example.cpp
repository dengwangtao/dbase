#include "dbase/app/application.h"
#include "dbase/cache/lru_cache.h"
#include "dbase/config/ini_config.h"
#include "dbase/error/error.h"
#include "dbase/fs/fs.h"
#include "dbase/log/async_logger.h"
#include "dbase/log/log.h"
#include "dbase/log/registry.h"
#include "dbase/log/sink.h"
#include "dbase/memory/object_pool.h"
#include "dbase/metrics/registry.h"
#include "dbase/net/event_loop_thread.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_client.h"
#include "dbase/net/tcp_connection.h"
#include "dbase/net/tcp_server.h"
#include "dbase/platform/process.h"
#include "dbase/random/random.h"
#include "dbase/str/str.h"
#include "dbase/sync/blocking_queue.h"
#include "dbase/sync/count_down_latch.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/sharded_executor.h"
#include "dbase/thread/thread.h"
#include "dbase/thread/thread_pool.h"
#include "dbase/thread/timer_queue.h"
#include "dbase/time/time.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace
{
template <typename T>
T mustValue(dbase::Result<T> result, std::string_view what)
{
    if (!result)
    {
        throw std::runtime_error(std::format("{}: {}", what, result.error().toString()));
    }
    return std::move(result).value();
}

void mustOk(dbase::Result<void> result, std::string_view what)
{
    if (!result)
    {
        throw std::runtime_error(std::format("{}: {}", what, result.error().toString()));
    }
}

struct MessageContext
{
        std::string user;
        std::string text;
        std::int64_t seq{0};
        dbase::time::Timestamp createdAt;
};

struct DemoState
{
        std::filesystem::path workDir;
        std::filesystem::path configPath;
        std::filesystem::path metricsPath;
        std::filesystem::path historyPath;
        std::filesystem::path asyncLogPath;

        dbase::config::IniConfig config;
        dbase::metrics::MetricRegistry metrics;
        dbase::cache::LRUCache<std::string, std::string> cache{256};
        dbase::memory::ObjectPool<MessageContext> contextPool{256};

        dbase::sync::BlockingQueue<std::string> auditQueue{1024};
        dbase::sync::CountDownLatch auditReady{1};

        dbase::thread::ThreadPool workerPool{4, "demo-worker", 4096};
        std::unique_ptr<dbase::thread::ShardedExecutor> sharded;
        dbase::thread::TimerQueue timerQueue{&workerPool, "demo-timer"};
        dbase::thread::Thread auditThread;

        dbase::net::EventLoopThread serverLoopThread{"demo-server-loop"};
        dbase::net::EventLoopThread clientLoopThread{"demo-client-loop"};
        dbase::net::EventLoop* serverLoop{nullptr};
        dbase::net::EventLoop* clientLoop{nullptr};

        std::shared_ptr<dbase::net::LengthFieldCodec> codec;
        std::unique_ptr<dbase::net::TcpServer> server;
        std::unique_ptr<dbase::net::TcpClient> client;

        dbase::log::AsyncLogger asyncLogger{dbase::log::PatternStyle::Threaded, 8192, dbase::log::AsyncOverflowPolicy::Block};
        std::shared_ptr<dbase::log::Logger> workerLogger;

        std::mutex sentMutex;
        std::unordered_map<std::int64_t, std::int64_t> sentAtUs;

        std::atomic<bool> stopping{false};
        std::atomic<std::int64_t> nextSeq{1};
        std::atomic<int> plannedRequests{12};
        std::atomic<int> sentRequests{0};
        std::atomic<int> recvResponses{0};
};

std::string defaultConfigText()
{
    return R"ini(
[app]
name = dbase-full-stack-demo
requests = 12

[net]
host = 127.0.0.1
port = 19781
server_threads = 1
heartbeat_ms = 2000
idle_timeout_ms = 8000

[demo]
users = alice,bob,charlie,diana
metrics_flush_ms = 1000
send_interval_ms = 400
)ini";
}

std::vector<std::string> splitUsers(const dbase::config::IniConfig& config)
{
    auto text = config.getString("demo.users");
    if (!text)
    {
        return {"alice", "bob", "charlie", "diana"};
    }
    std::vector<std::string> users;
    for (auto& item : dbase::str::split(text.value(), ',', false))
    {
        auto t = dbase::str::trim(item);
        if (!t.empty())
        {
            users.emplace_back(std::move(t));
        }
    }
    if (users.empty())
    {
        users = {"alice", "bob", "charlie", "diana"};
    }
    return users;
}

void warmupBufferAndCodec(DemoState& state)
{
    dbase::net::Buffer buffer;
    buffer.appendUInt32(2026);
    buffer.append("warmup");
    const auto year = buffer.readUInt32();
    const auto text = buffer.retrieveAllAsString();

    dbase::net::Buffer framed = state.codec->encode(std::format("buffer-ok-{}-{}", year, text));
    auto decoded = state.codec->tryDecode(framed);
    if (decoded.status != dbase::net::LengthFieldCodec::DecodeStatus::Ok)
    {
        throw std::runtime_error("codec warmup failed");
    }

    DBASE_LOG_INFO("codec warmup: {}", decoded.payload);
}

void persistMetrics(DemoState& state)
{
    mustOk(dbase::fs::writeTextAtomic(state.metricsPath, state.metrics.dumpText()), "write metrics");
}

void appendHistory(DemoState& state, std::string_view line)
{
    mustOk(dbase::fs::appendText(state.historyPath, std::string(line) + "\n"), "append history");
}

void startAuditThread(DemoState& state)
{
    state.auditThread = dbase::thread::Thread(
            [&state](std::stop_token stopToken)
            {
                dbase::thread::current_thread::setName("demo-audit");
                state.auditReady.countDown();

                while (!stopToken.stop_requested())
                {
                    try
                    {
                        auto item = state.auditQueue.popFor(200);
                        if (!item.has_value())
                        {
                            continue;
                        }
                        mustOk(dbase::fs::appendText(state.historyPath, item.value() + "\n"), "audit append");
                    }
                    catch (const std::runtime_error&)
                    {
                        break;
                    }
                }
            },
            "demo-audit");

    state.auditThread.start();
    state.auditReady.wait();
}

void stopAuditThread(DemoState& state)
{
    state.auditQueue.stop();
    state.auditThread.requestStop();
    if (state.auditThread.joinable())
    {
        state.auditThread.join();
    }
}

void scheduleClientTraffic(dbase::app::Application& app, const std::shared_ptr<DemoState>& state)
{
    const auto users = splitUsers(state->config);
    const auto sendIntervalMs = state->config.getIntOr("demo.send_interval_ms", 400);
    const auto total = state->plannedRequests.load(std::memory_order_acquire);

    std::atomic<int> fired{0};

    state->timerQueue.runEvery(
            std::chrono::milliseconds(sendIntervalMs),
            [&, state, users]()
            {
                const int current = fired.fetch_add(1, std::memory_order_acq_rel);
                if (current >= total)
                {
                    return;
                }

                auto conn = state->client->connection();
                if (!conn || !conn->connected())
                {
                    state->auditQueue.tryPush(std::format("[{}] client not connected yet", dbase::time::formatNow()));
                    return;
                }

                const auto& user = users[static_cast<std::size_t>(
                        dbase::random::uniformInt(0, static_cast<std::int32_t>(users.size() - 1)))];

                const auto seq = state->nextSeq.fetch_add(1, std::memory_order_acq_rel);
                const auto salt = dbase::random::uniformInt(1000, 9999);
                const auto payload = std::format("echo|{}|{}| hello-{}-{} ", user, seq, user, salt);

                {
                    std::lock_guard<std::mutex> lock(state->sentMutex);
                    state->sentAtUs.emplace(seq, dbase::time::nowUs());
                }

                conn->sendFrame(payload);
                state->sentRequests.fetch_add(1, std::memory_order_acq_rel);
                state->metrics.counter("client.sent_frames").inc();
                state->metrics.gauge("client.pending_requests").set(static_cast<double>(state->sentRequests.load(std::memory_order_acquire) - state->recvResponses.load(std::memory_order_acquire)));

                state->asyncLogger.logf(
                        dbase::log::Level::Info,
                        std::source_location::current(),
                        "client send seq={} payload={}",
                        seq,
                        payload);

                if (current + 1 == total)
                {
                    state->timerQueue.runAfter(
                            3s,
                            [&app, state]()
                            {
                                if (!state->stopping.exchange(true, std::memory_order_acq_rel))
                                {
                                    app.requestStop();
                                }
                            });
                }
            });
}

void setupServer(const std::shared_ptr<DemoState>& state)
{
    state->server->setThreadCount(static_cast<std::size_t>(state->config.getIntOr("net.server_threads", 1)));
    state->server->setLengthFieldCodec(state->codec);
    state->server->setHeartbeatInterval(std::chrono::milliseconds(state->config.getIntOr("net.heartbeat_ms", 2000)));
    state->server->setIdleTimeout(std::chrono::milliseconds(state->config.getIntOr("net.idle_timeout_ms", 8000)));
    state->server->enableAutoReadFlowControl(512 * 1024, 128 * 1024);
    state->server->setMaxOutputBufferBytes(2 * 1024 * 1024);
    state->server->setOutputOverflowPolicy(dbase::net::TcpConnection::OutputOverflowPolicy::ReportError);

    state->server->setConnectionCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                if (conn->connected())
                {
                    DBASE_LOG_INFO("server connected: {}", conn->name());
                    state->metrics.counter("server.connections").inc();
                }
                else
                {
                    DBASE_LOG_INFO("server disconnected: {}", conn->name());
                }
            });

    state->server->setHighWaterMarkCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn, std::size_t bytes)
            {
                state->metrics.counter("server.high_watermark_events").inc();
                DBASE_LOG_WARN("high watermark: conn={} bytes={}", conn->name(), bytes);
            });

    state->server->setHeartbeatCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                state->metrics.counter("server.heartbeat_sent").inc();
                conn->sendFrame("ping");
            });

    state->server->setIdleCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                state->metrics.counter("server.idle_close").inc();
                DBASE_LOG_WARN("server idle close: {}", conn->name());
                conn->forceClose();
            });

    state->server->setFrameMessageCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn, std::string&& frame)
            {
                state->metrics.counter("server.recv_frames").inc();
                state->metrics.gauge("server.input_frame_bytes").set(static_cast<double>(frame.size()));

                auto parts = dbase::str::split(frame, '|', false);
                if (parts.size() == 1 && dbase::str::equalsIgnoreCase(dbase::str::trim(parts[0]), "ping"))
                {
                    conn->sendFrame("pong");
                    return;
                }

                if (parts.size() != 4 || !dbase::str::equalsIgnoreCase(dbase::str::trim(parts[0]), "echo"))
                {
                    state->metrics.counter("server.bad_frames").inc();
                    conn->sendFrame(std::format("error|bad_frame|{}", frame));
                    return;
                }

                auto seqResult = dbase::str::toInt64(dbase::str::trim(parts[2]));
                if (!seqResult)
                {
                    state->metrics.counter("server.bad_frames").inc();
                    conn->sendFrame(std::format("error|bad_seq|{}", frame));
                    return;
                }

                auto ctx = state->contextPool.makeShared();
                ctx->user = dbase::str::trim(parts[1]);
                ctx->seq = seqResult.value();
                ctx->text = dbase::str::trim(parts[3]);
                ctx->createdAt = dbase::time::Timestamp::now();

                state->sharded->submit(
                        ctx->user,
                        [state, conn, ctx]()
                        {
                            dbase::time::Stopwatch sw;

                            std::string cacheKey = std::format("{}|{}", ctx->user, ctx->text);
                            std::optional<std::string> cached = state->cache.get(cacheKey);

                            std::string replyBody;
                            if (cached.has_value())
                            {
                                state->metrics.counter("server.cache_hit").inc();
                                replyBody = *cached;
                            }
                            else
                            {
                                state->metrics.counter("server.cache_miss").inc();
                                dbase::thread::current_thread::sleepForMs(
                                        static_cast<std::uint64_t>(dbase::random::uniformInt(10, 40)));

                                replyBody = std::format(
                                        "user={} text={} at={} pid={}",
                                        dbase::str::toUpper(ctx->user),
                                        dbase::str::replaceAll(ctx->text, " ", "_"),
                                        ctx->createdAt.format("%H:%M:%S"),
                                        dbase::platform::pid());

                                state->cache.put(cacheKey, replyBody);
                            }

                            state->metrics.histogram("server.process_ms").observe(static_cast<double>(sw.elapsedMs()));

                            if (state->workerLogger)
                            {
                                state->workerLogger->logf(
                                        dbase::log::Level::Info,
                                        std::source_location::current(),
                                        "worker seq={} user={} cache_size={}",
                                        ctx->seq,
                                        ctx->user,
                                        state->cache.size());
                            }

                            conn->sendFrame(std::format("reply|{}|{}|{}", ctx->user, ctx->seq, replyBody));
                        });
            });
}

void setupClient(const std::shared_ptr<DemoState>& state)
{
    state->client->setLengthFieldCodec(state->codec);
    state->client->setHeartbeatInterval(std::chrono::milliseconds(state->config.getIntOr("net.heartbeat_ms", 2000)));
    state->client->setIdleTimeout(std::chrono::milliseconds(state->config.getIntOr("net.idle_timeout_ms", 8000)));
    state->client->enableRetry(false);

    state->client->setConnectionCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                if (conn->connected())
                {
                    DBASE_LOG_INFO("client connected: {}", conn->name());
                }
                else
                {
                    DBASE_LOG_WARN("client disconnected: {}", conn->name());
                }
            });

    state->client->setHeartbeatCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                state->metrics.counter("client.heartbeat_sent").inc();
                conn->sendFrame("ping");
            });

    state->client->setIdleCallback(
            [state](const dbase::net::TcpConnection::Ptr& conn)
            {
                state->metrics.counter("client.idle_events").inc();
                DBASE_LOG_WARN("client idle detected: {}", conn->name());
                conn->forceClose();
            });

    state->client->setFrameMessageCallback(
            [state](const dbase::net::TcpConnection::Ptr&, std::string&& frame)
            {
                state->metrics.counter("client.recv_frames").inc();

                if (dbase::str::equalsIgnoreCase(dbase::str::trim(frame), "pong"))
                {
                    state->metrics.counter("client.recv_pong").inc();
                    return;
                }

                auto parts = dbase::str::split(frame, '|', false);
                if (parts.size() < 4 || !dbase::str::equalsIgnoreCase(dbase::str::trim(parts[0]), "reply"))
                {
                    state->metrics.counter("client.bad_frames").inc();
                    DBASE_LOG_WARN("client got unexpected frame: {}", frame);
                    return;
                }

                auto seqResult = dbase::str::toInt64(dbase::str::trim(parts[2]));
                if (!seqResult)
                {
                    state->metrics.counter("client.bad_frames").inc();
                    return;
                }

                const auto seq = seqResult.value();
                std::int64_t sentAtUs = 0;
                {
                    std::lock_guard<std::mutex> lock(state->sentMutex);
                    const auto it = state->sentAtUs.find(seq);
                    if (it != state->sentAtUs.end())
                    {
                        sentAtUs = it->second;
                        state->sentAtUs.erase(it);
                    }
                }

                if (sentAtUs > 0)
                {
                    const auto latencyMs = static_cast<double>(dbase::time::toMs(
                            std::chrono::microseconds(dbase::time::nowUs() - sentAtUs)));
                    state->metrics.histogram("client.rtt_ms").observe(latencyMs);
                }

                state->recvResponses.fetch_add(1, std::memory_order_acq_rel);
                state->metrics.gauge("client.pending_requests").set(static_cast<double>(state->sentRequests.load(std::memory_order_acquire) - state->recvResponses.load(std::memory_order_acquire)));

                const auto line = std::format(
                        "[{}] {}",
                        dbase::time::formatNow("%H:%M:%S"),
                        frame);

                state->auditQueue.tryPush(line);
                state->asyncLogger.logf(
                        dbase::log::Level::Info,
                        std::source_location::current(),
                        "client recv {}",
                        frame);
            });
}

}  // namespace

int main(int argc, char** argv)
{
    dbase::app::Application app(
            argc,
            argv,
            {
                    .name = "dbase-full-stack-demo",
                    .version = "0.1.0",
                    .workingDirectory = "demo_runtime",
                    .pidFile = "demo_runtime/dbase_full_stack_example.pid",
                    .configFile = "demo_runtime/demo.ini",
                    .handleSignals = true,
                    .createPidFile = true,
            });

    auto state = std::make_shared<DemoState>();

    app.setStartupCallback(
            [state](dbase::app::Application& appRef) -> dbase::Result<void>
            {
                try
                {
                    mustOk(dbase::net::SocketOps::initialize(), "SocketOps initialize");

                    state->workDir = appRef.workingDirectory().empty() ? std::filesystem::path("demo_runtime")
                                                                       : appRef.workingDirectory();
                    state->configPath = appRef.configFile().empty() ? state->workDir / "demo.ini"
                                                                    : appRef.configFile();
                    state->metricsPath = state->workDir / "metrics.txt";
                    state->historyPath = state->workDir / "history.log";
                    state->asyncLogPath = state->workDir / "async.log";

                    mustOk(dbase::fs::createDirectories(state->workDir), "create work dir");

                    if (!dbase::fs::exists(state->configPath))
                    {
                        mustOk(dbase::fs::writeTextAtomic(state->configPath, defaultConfigText()), "write default config");
                    }

                    state->config = mustValue(dbase::config::IniConfig::fromFile(state->configPath), "load config");
                    mustOk(state->config.require("net.port"), "require net.port");

                    state->plannedRequests.store(
                            static_cast<int>(state->config.getIntOr("app.requests", 12)),
                            std::memory_order_release);

                    if (appRef.hasOption("requests"))
                    {
                        auto n = dbase::str::toInt(appRef.getOption("requests"));
                        if (n)
                        {
                            state->plannedRequests.store(n.value(), std::memory_order_release);
                        }
                    }

                    dbase::log::resetDefaultSinks();
                    dbase::log::addDefaultSink(std::make_shared<dbase::log::DailyFileSink>(state->workDir / "demo.log"));
                    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::SourceFunction);
                    dbase::log::setDefaultLevel(dbase::log::Level::Info);

                    state->workerLogger = dbase::log::createLogger("demo.worker", dbase::log::PatternStyle::Threaded);
                    state->workerLogger->addSink(std::make_shared<dbase::log::FileSink>(state->workDir / "worker.log", false));

                    state->asyncLogger.addSink(std::make_shared<dbase::log::RotatingFileSink>(state->asyncLogPath, 1024 * 1024, 3, false));
                    state->asyncLogger.setLevel(dbase::log::Level::Info);

                    state->contextPool.setResetCallback(
                            [](MessageContext& ctx)
                            {
                                ctx.user.clear();
                                ctx.text.clear();
                                ctx.seq = 0;
                                ctx.createdAt = dbase::time::Timestamp{};
                            });
                    state->contextPool.reserve(32);

                    state->cache.setEvictCallback(
                            [](const std::string& key, const std::string&)
                            {
                                DBASE_LOG_DEBUG("cache evict: {}", key);
                            });

                    state->workerPool.start();
                    state->sharded = std::make_unique<dbase::thread::ShardedExecutor>(state->workerPool, 8, 1024);
                    state->sharded->start();
                    state->timerQueue.start();
                    startAuditThread(*state);

                    auto exe = dbase::platform::executablePath();
                    if (exe)
                    {
                        DBASE_LOG_INFO("executable: {}", exe->string());
                    }

                    state->codec = std::make_shared<dbase::net::LengthFieldCodec>(
                            4,
                            dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
                            1024 * 1024);

                    warmupBufferAndCodec(*state);

                    state->serverLoop = state->serverLoopThread.startLoop();
                    state->clientLoop = state->clientLoopThread.startLoop();

                    const auto host = state->config.getStringOr("net.host", "127.0.0.1");
                    const auto port = static_cast<std::uint16_t>(state->config.getIntOr("net.port", 19781));
                    const dbase::net::InetAddress serverAddr(host, port);

                    state->server = std::make_unique<dbase::net::TcpServer>(
                            state->serverLoop,
                            serverAddr,
                            "demo-server",
                            false,
                            false);

                    state->client = std::make_unique<dbase::net::TcpClient>(
                            state->clientLoop,
                            serverAddr,
                            "demo-client");

                    setupServer(state);
                    setupClient(state);

                    state->serverLoop->runInLoop([state]()
                                                 { state->server->start(); });

                    state->clientLoop->runInLoop([state]()
                                                 { state->client->connect(); });

                    const auto metricsFlushMs = state->config.getIntOr("demo.metrics_flush_ms", 1000);
                    state->timerQueue.runEvery(
                            std::chrono::milliseconds(metricsFlushMs),
                            [state]()
                            {
                                persistMetrics(*state);
                                state->metrics.gauge("cache.size").set(static_cast<double>(state->cache.size()));
                                state->metrics.gauge("pool.idle_context").set(static_cast<double>(state->contextPool.idleCount()));
                                state->metrics.gauge("worker.pending").set(static_cast<double>(state->workerPool.pendingTaskCount()));
                                state->metrics.gauge("sharded.pending").set(static_cast<double>(state->sharded->pendingTaskCount()));

                                state->asyncLogger.logf(
                                        dbase::log::Level::Info,
                                        std::source_location::current(),
                                        "metrics flushed: sent={} recv={} cache={} pool_idle={}",
                                        state->sentRequests.load(std::memory_order_acquire),
                                        state->recvResponses.load(std::memory_order_acquire),
                                        state->cache.size(),
                                        state->contextPool.idleCount());
                            });

                    scheduleClientTraffic(appRef, state);

                    DBASE_LOG_INFO(
                            "startup done: pid={} ppid={} tid={} workdir={}",
                            dbase::platform::pid(),
                            dbase::platform::ppid(),
                            dbase::platform::tid(),
                            state->workDir.string());

                    return {};
                }
                catch (const std::exception& ex)
                {
                    return dbase::makeError(dbase::ErrorCode::InvalidState, ex.what());
                }
            });

    app.setShutdownCallback(
            [state](dbase::app::Application&)
            {
                try
                {
                    if (state->stopping.exchange(true, std::memory_order_acq_rel))
                    {
                        return;
                    }

                    persistMetrics(*state);

                    if (state->clientLoop != nullptr && state->client)
                    {
                        state->clientLoop->runInLoop([state]()
                                                     {
                        state->client->stop();
                        auto conn = state->client->connection();
                        if (conn)
                        {
                            conn->forceClose();
                        } });
                    }

                    state->timerQueue.stop();
                    stopAuditThread(*state);

                    state->serverLoopThread.stop();
                    state->clientLoopThread.stop();

                    state->server.reset();
                    state->client.reset();

                    if (state->sharded)
                    {
                        state->sharded->stop();
                    }

                    state->workerPool.stop();
                    state->asyncLogger.flush();
                    state->asyncLogger.stop();

                    auto files = dbase::fs::listFiles(state->workDir, false);
                    if (files)
                    {
                        for (const auto& path : files.value())
                        {
                            DBASE_LOG_INFO("generated file: {}", path.string());
                        }
                    }

                    dbase::net::SocketOps::cleanup();
                    dbase::log::flushDefaultLogger();
                }
                catch (const std::exception& ex)
                {
                    DBASE_LOG_ERROR("shutdown error: {}", ex.what());
                }
            });

    return app.run();
}