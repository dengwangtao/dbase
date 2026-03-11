#include "dbase/log/log.h"
#include "dbase/metrics/registry.h"

#include <chrono>
#include <random>
#include <thread>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::metrics::MetricRegistry registry;

    auto& acceptCounter = registry.counter("net.accept.total");
    auto& connGauge = registry.gauge("net.connection.active");
    auto& latencyHist = registry.histogram("rpc.latency.ms", 2048);

    acceptCounter.inc();
    acceptCounter.inc(4);

    connGauge.set(10);
    connGauge.inc(2);
    connGauge.dec(1);

    std::mt19937 rng{12345};
    std::uniform_real_distribution<double> dist(0.2, 15.0);

    for (int i = 0; i < 200; ++i)
    {
        latencyHist.observe(dist(rng));
    }

    DBASE_LOG_INFO("accept total={}", acceptCounter.value());
    DBASE_LOG_INFO("connection active={}", connGauge.value());

    const auto snap = latencyHist.snapshot();
    DBASE_LOG_INFO(
            "latency count={} avg={} p95={} p99={}",
            snap.count,
            snap.avg,
            snap.p95,
            snap.p99);

    DBASE_LOG_INFO("metrics dump:\n{}", registry.dumpText());

    auto found = registry.find("rpc.latency.ms");
    if (found)
    {
        DBASE_LOG_INFO("found metric: {}", found.value()->name());
    }

    return 0;
}