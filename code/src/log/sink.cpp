#include "dbase/log/sink.h"
#include "dbase/log/log.h"

#include <iostream>

namespace dbase::log
{
void ConsoleSink::write(const LogEvent& event, std::string_view formatted)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (static_cast<int>(event.level) >= static_cast<int>(Level::Error))
    {
        std::cerr << formatted << '\n';
        return;
    }

    std::cout << formatted << '\n';
}
}  // namespace dbase::log