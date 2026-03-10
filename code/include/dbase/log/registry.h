#pragma once

#include "dbase/log/log.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dbase::log
{
class Registry
{
    public:
        Registry() = default;

        [[nodiscard]] bool contains(std::string_view name) const;
        [[nodiscard]] std::shared_ptr<Logger> get(std::string_view name) const;

        std::shared_ptr<Logger> create(std::string name);
        std::shared_ptr<Logger> create(std::string name, PatternStyle style);

        bool add(std::string name, std::shared_ptr<Logger> logger);
        bool remove(std::string_view name);
        void clear();

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<std::string, std::shared_ptr<Logger>> m_loggers;
};

Registry& registry();

[[nodiscard]] bool hasLogger(std::string_view name);
[[nodiscard]] std::shared_ptr<Logger> getLogger(std::string_view name);

std::shared_ptr<Logger> createLogger(std::string name);
std::shared_ptr<Logger> createLogger(std::string name, PatternStyle style);

bool registerLogger(std::string name, std::shared_ptr<Logger> logger);
bool removeLogger(std::string_view name);
void clearRegistry();

}  // namespace dbase::log