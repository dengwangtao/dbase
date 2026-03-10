#include "dbase/log/registry.h"

#include <utility>

namespace dbase::log
{
bool Registry::contains(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loggers.contains(std::string(name));
}

std::shared_ptr<Logger> Registry::get(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = m_loggers.find(std::string(name));
    if (it == m_loggers.end())
    {
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<Logger> Registry::create(std::string name)
{
    return create(std::move(name), PatternStyle::Source);
}

std::shared_ptr<Logger> Registry::create(std::string name, PatternStyle style)
{
    auto logger = std::make_shared<Logger>(style);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto [it, inserted] = m_loggers.emplace(std::move(name), logger);
    if (!inserted)
    {
        return it->second;
    }

    return logger;
}

bool Registry::add(std::string name, std::shared_ptr<Logger> logger)
{
    if (!logger)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto [_, inserted] = m_loggers.emplace(std::move(name), std::move(logger));
    return inserted;
}

bool Registry::remove(std::string_view name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loggers.erase(std::string(name)) > 0;
}

void Registry::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loggers.clear();
}

Registry& registry()
{
    static Registry instance;
    return instance;
}

bool hasLogger(std::string_view name)
{
    return registry().contains(name);
}

std::shared_ptr<Logger> getLogger(std::string_view name)
{
    return registry().get(name);
}

std::shared_ptr<Logger> createLogger(std::string name)
{
    return registry().create(std::move(name));
}

std::shared_ptr<Logger> createLogger(std::string name, PatternStyle style)
{
    return registry().create(std::move(name), style);
}

bool registerLogger(std::string name, std::shared_ptr<Logger> logger)
{
    return registry().add(std::move(name), std::move(logger));
}

bool removeLogger(std::string_view name)
{
    return registry().remove(name);
}

void clearRegistry()
{
    registry().clear();
}

}  // namespace dbase::log