#include "dbase/log/log.h"
#include "dbase/log/sink.h"

#include <memory>

int main()
{
    dbase::log::resetDefaultSinks();
    dbase::log::addDefaultSink(std::make_shared<dbase::log::ConsoleSink>());
    dbase::log::addDefaultSink(std::make_shared<dbase::log::DailyFileSink>("logs/daily_app.log"));

    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);
    dbase::log::setDefaultFlushOn(dbase::log::Level::Error);

    DBASE_LOG_TRACE("trace message");
    DBASE_LOG_DEBUG("debug message");
    DBASE_LOG_INFO("daily logger started, port={}", 8080);
    DBASE_LOG_WARN("warn message");
    DBASE_LOG_ERROR("error message");
    DBASE_LOG_FATAL("fatal message");

    dbase::log::flushDefaultLogger();
    return 0;
}