#include "dbase/log/log.h"
#include "dbase/time/time.h"

#include <chrono>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    DBASE_LOG_INFO("nowMs={}", dbase::time::nowMs());
    DBASE_LOG_INFO("nowUs={}", dbase::time::nowUs());
    DBASE_LOG_INFO("nowNs={}", dbase::time::nowNs());

    DBASE_LOG_INFO("steadyNowMs={}", dbase::time::steadyNowMs());
    DBASE_LOG_INFO("steadyNowUs={}", dbase::time::steadyNowUs());
    DBASE_LOG_INFO("steadyNowNs={}", dbase::time::steadyNowNs());

    DBASE_LOG_INFO("formatNow={}", dbase::time::formatNow());
    DBASE_LOG_INFO("formatNow.custom={}", dbase::time::formatNow("%Y-%m-%d %H:%M:%S"));

    const auto ts = dbase::time::Timestamp::now();
    DBASE_LOG_INFO("timestamp.us={}", ts.unixUs());
    DBASE_LOG_INFO("timestamp.ms={}", ts.unixMs());
    DBASE_LOG_INFO("timestamp.format={}", ts.format());

    dbase::time::Stopwatch sw;
    dbase::time::sleepForMs(120);

    DBASE_LOG_INFO("elapsedNs={}", sw.elapsedNs());
    DBASE_LOG_INFO("elapsedUs={}", sw.elapsedUs());
    DBASE_LOG_INFO("elapsedMs={}", sw.elapsedMs());
    DBASE_LOG_INFO("elapsedSeconds={}", sw.elapsedSeconds());

    const auto duration = std::chrono::milliseconds(2500);
    DBASE_LOG_INFO("toNs={}", dbase::time::toNs(duration));
    DBASE_LOG_INFO("toUs={}", dbase::time::toUs(duration));
    DBASE_LOG_INFO("toMs={}", dbase::time::toMs(duration));
    DBASE_LOG_INFO("toSeconds={}", dbase::time::toSeconds(duration));

    return 0;
}