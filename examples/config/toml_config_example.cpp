#include "dbase/config/toml_config.h"
#include "dbase/log/log.h"

#include <string_view>
#include <utility>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    constexpr std::string_view tomlText = R"toml(
[server]
host = "127.0.0.1"
port = 9781
workers = 4
enable_ssl = true
tags = ["gateway", "internal"]
listen_ports = [8080, 8081]

[server.limits]
max_conn = 10000
timeout_ms = 1500

[log]
level = "info"
flush_interval = 3

[app]
name = "dbase-demo"
pi = 3.14159
version = "1.0.0"
build = 20260313
)toml";

    auto configRet = dbase::config::TomlConfig::fromString(tomlText);
    if (!configRet)
    {
        DBASE_LOG_ERROR("load toml config failed: {}", configRet.error().toString());
        return 1;
    }

    auto config = std::move(configRet.value());

    auto serverObj = config.getObject("server");
    auto tags = config.getArray("server.tags");
    auto listenPorts = config.getArray("server.listen_ports");
    auto limits = config.getObject("server.limits");

    auto host = config.getString("server.host");
    auto port = config.getInt("server.port");
    auto workers = config.getInt("server.workers");
    auto ssl = config.getBool("server.enable_ssl");
    auto tag0 = config.getString("server.tags.0");
    auto tag1 = config.getString("server.tags.1");
    auto listenPort0 = config.getUInt("server.listen_ports.0");
    auto listenPort1 = config.getUInt("server.listen_ports.1");
    auto maxConn = config.getUInt("server.limits.max_conn");
    auto timeoutMs = config.getUInt("server.limits.timeout_ms");
    auto logLevel = config.getString("log.level");
    auto flushInterval = config.getInt("log.flush_interval");
    auto appName = config.getString("app.name");
    auto pi = config.getDouble("app.pi");
    auto version = config.getString("app.version");
    auto build = config.getUInt("app.build");

    if (!serverObj || !tags || !listenPorts || !limits || !host || !port || !workers || !ssl || !tag0 || !tag1 || !listenPort0 || !listenPort1 || !maxConn || !timeoutMs || !logLevel || !flushInterval || !appName || !pi || !version || !build)
    {
        DBASE_LOG_ERROR("read toml config failed");
        return 1;
    }

    DBASE_LOG_INFO("server object field count={}", serverObj.value()->size());
    DBASE_LOG_INFO("server.tags size={}", tags.value()->size());
    DBASE_LOG_INFO("server.listen_ports size={}", listenPorts.value()->size());
    DBASE_LOG_INFO("server.limits field count={}", limits.value()->size());

    DBASE_LOG_INFO("server.host={}", host.value());
    DBASE_LOG_INFO("server.port={}", port.value());
    DBASE_LOG_INFO("server.workers={}", workers.value());
    DBASE_LOG_INFO("server.enable_ssl={}", ssl.value());
    DBASE_LOG_INFO("server.tags.0={}", tag0.value());
    DBASE_LOG_INFO("server.tags.1={}", tag1.value());
    DBASE_LOG_INFO("server.listen_ports.0={}", listenPort0.value());
    DBASE_LOG_INFO("server.listen_ports.1={}", listenPort1.value());
    DBASE_LOG_INFO("server.limits.max_conn={}", maxConn.value());
    DBASE_LOG_INFO("server.limits.timeout_ms={}", timeoutMs.value());
    DBASE_LOG_INFO("log.level={}", logLevel.value());
    DBASE_LOG_INFO("log.flush_interval={}", flushInterval.value());
    DBASE_LOG_INFO("app.name={}", appName.value());
    DBASE_LOG_INFO("app.pi={}", pi.value());
    DBASE_LOG_INFO("app.version={}", version.value());
    DBASE_LOG_INFO("app.build={}", build.value());

    DBASE_LOG_INFO("missing.string default={}", config.getStringOr("missing.string", "fallback"));
    DBASE_LOG_INFO("missing.int default={}", config.getIntOr("missing.int", 42));
    DBASE_LOG_INFO("missing.uint default={}", config.getUIntOr("missing.uint", 7));
    DBASE_LOG_INFO("missing.double default={}", config.getDoubleOr("missing.double", 6.28));
    DBASE_LOG_INFO("missing.bool default={}", config.getBoolOr("missing.bool", true));

    auto requireRet = config.require("server.host");
    if (!requireRet)
    {
        DBASE_LOG_ERROR("require server.host failed: {}", requireRet.error().toString());
        return 1;
    }

    auto requireTagRet = config.require("server.tags.1");
    if (!requireTagRet)
    {
        DBASE_LOG_ERROR("require server.tags.1 failed: {}", requireTagRet.error().toString());
        return 1;
    }

    DBASE_LOG_INFO("toml config item count={}", config.values().size());
    for (const auto& [key, value] : config.values())
    {
        DBASE_LOG_INFO("item: {} = {}", key, value.toString());
    }

    return 0;
}