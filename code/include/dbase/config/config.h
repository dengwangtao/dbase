#pragma once

#include "dbase/config/config_value.h"
#include "dbase/error/error.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace dbase::config
{
class Config
{
    public:
        Config() = default;
        virtual ~Config() = default;

        Config(const Config&) = default;
        Config& operator=(const Config&) = default;

        Config(Config&&) noexcept = default;
        Config& operator=(Config&&) noexcept = default;

        [[nodiscard]] bool has(const std::string& key) const noexcept;

        [[nodiscard]] dbase::Result<const ConfigValue*> get(const std::string& key) const;
        [[nodiscard]] dbase::Result<std::string> getString(const std::string& key) const;
        [[nodiscard]] dbase::Result<std::int64_t> getInt(const std::string& key) const;
        [[nodiscard]] dbase::Result<double> getDouble(const std::string& key) const;
        [[nodiscard]] dbase::Result<bool> getBool(const std::string& key) const;

        [[nodiscard]] std::string getStringOr(const std::string& key, std::string defaultValue) const;
        [[nodiscard]] std::int64_t getIntOr(const std::string& key, std::int64_t defaultValue) const;
        [[nodiscard]] double getDoubleOr(const std::string& key, double defaultValue) const;
        [[nodiscard]] bool getBoolOr(const std::string& key, bool defaultValue) const;

        [[nodiscard]] dbase::Result<void> require(const std::string& key) const;

        [[nodiscard]] const std::unordered_map<std::string, ConfigValue>& values() const noexcept;

    protected:
        void set(std::string key, ConfigValue value);

    protected:
        std::unordered_map<std::string, ConfigValue> m_values;
};
}  // namespace dbase::config