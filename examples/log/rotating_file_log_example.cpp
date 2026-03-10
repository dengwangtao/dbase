#include "dbase/log/log.h"
#include "dbase/log/sink.h"

#include <memory>

int main()
{
    dbase::log::resetDefaultSinks();
    dbase::log::addDefaultSink(
            std::make_shared<dbase::log::RotatingFileSink>("logs/rotating_app.log", 1024, 3));

    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);
    dbase::log::setDefaultFlushOn(dbase::log::Level::Error);

    for (int i = 0; i < 2000; ++i)
    {
        DBASE_LOG_INFO("rotating log line {}", i);
    }

    DBASE_LOG_ERROR("final error line");
    dbase::log::flushDefaultLogger();

    return 0;
}