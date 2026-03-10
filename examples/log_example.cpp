#include "dbase/log/log.h"
#include <thread>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);

    std::thread t([]()
                  {
        DBASE_LOG_TRACE("trace message: {}", 1);
        DBASE_LOG_DEBUG("debug message: {}", 2);
        DBASE_LOG_INFO("info message: {}", 3);
        DBASE_LOG_WARN("warn message: {}", 4);
        DBASE_LOG_ERROR("error message: {}", 5);
        DBASE_LOG_FATAL("fatal message: {}", 6); });

    DBASE_LOG_TRACE("trace message: {}", 1);
    DBASE_LOG_DEBUG("debug message: {}", 2);
    DBASE_LOG_INFO("info message: {}", 3);
    DBASE_LOG_WARN("warn message: {}", 4);
    DBASE_LOG_ERROR("error message: {}", 5);
    DBASE_LOG_FATAL("fatal message: {}", 6);

    t.join();

    return 0;
}