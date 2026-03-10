#include "dbase/log/log.h"
#include "dbase/log/registry.h"
#include "dbase/log/sink.h"

#include <memory>

int main()
{
    dbase::log::clearRegistry();

    auto netLogger = dbase::log::createLogger("net", dbase::log::PatternStyle::Source);
    netLogger->clearSinks();
    netLogger->addSink(std::make_shared<dbase::log::ConsoleSink>());
    netLogger->addSink(std::make_shared<dbase::log::FileSink>("logs/net.log"));
    netLogger->setLevel(dbase::log::Level::Trace);
    netLogger->setFlushOn(dbase::log::Level::Error);

    auto dbLogger = dbase::log::createLogger("db", dbase::log::PatternStyle::Threaded);
    dbLogger->clearSinks();
    dbLogger->addSink(std::make_shared<dbase::log::ConsoleSink>());
    dbLogger->addSink(std::make_shared<dbase::log::RotatingFileSink>("logs/db.log", 1024 * 8, 3));
    dbLogger->setLevel(dbase::log::Level::Debug);
    dbLogger->setFlushOn(dbase::log::Level::Warn);

    if (auto logger = dbase::log::getLogger("net"))
    {
        logger->logf(
                dbase::log::Level::Info,
                std::source_location::current(),
                "server listen on port={}",
                7001);
    }

    if (auto logger = dbase::log::getLogger("db"))
    {
        logger->logf(
                dbase::log::Level::Debug,
                std::source_location::current(),
                "query sql={}, cost={}ms",
                "select * from player",
                12);

        logger->logf(
                dbase::log::Level::Error,
                std::source_location::current(),
                "connect database failed, code={}",
                -1);

        logger->flush();
    }

    return 0;
}