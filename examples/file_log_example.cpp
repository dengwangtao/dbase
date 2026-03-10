#include "dbase/log/log.h"
#include "dbase/log/sink.h"

#include <memory>

int main()
{
    dbase::log::resetDefaultSinks();
    dbase::log::addDefaultSink(std::make_shared<dbase::log::FileSink>("logs/app.log"));

    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);
    dbase::log::setDefaultFlushOn(dbase::log::Level::Error);

    DBASE_LOG_TRACE("trace message");
    DBASE_LOG_DEBUG("debug message");
    DBASE_LOG_INFO("server start, port={}", 8080);
    DBASE_LOG_WARN("config key missing, use default");
    DBASE_LOG_ERROR("open file failed, path={}", "conf/app.toml");
    DBASE_LOG_FATAL("fatal error, code={}", -1);

    dbase::log::flushDefaultLogger();
    return 0;
}