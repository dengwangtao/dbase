#pragma once

#include "dbase/config/config_value.h"
#include "dbase/error/error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dbase::config
{
class Config
{
    public:
        Config();
        virtual ~Config() = default;
        Config(const Config&) = default;
        Config& operator=(const Config&) = default;
        Config(Config&&) noexcept = default;
        Config& operator=(Config&&) noexcept = default;

        [[nodiscard]] const ConfigValue& root() const noexcept;
        [[nodiscard]] ConfigValue& root() noexcept;

        [[nodiscard]] bool has(std::string_view path) const noexcept;
        [[nodiscard]] dbase::Result<const ConfigValue*> get(std::string_view path) const;
        [[nodiscard]] dbase::Result<ConfigValue*> get(std::string_view path);

        [[nodiscard]] dbase::Result<std::string> getString(std::string_view path) const;
        [[nodiscard]] dbase::Result<std::int64_t> getInt(std::string_view path) const;
        [[nodiscard]] dbase::Result<std::uint64_t> getUInt(std::string_view path) const;
        [[nodiscard]] dbase::Result<double> getDouble(std::string_view path) const;
        [[nodiscard]] dbase::Result<bool> getBool(std::string_view path) const;
        [[nodiscard]] dbase::Result<const ConfigValue::Array*> getArray(std::string_view path) const;
        [[nodiscard]] dbase::Result<const ConfigValue::Object*> getObject(std::string_view path) const;

        [[nodiscard]] std::string getStringOr(std::string_view path, std::string defaultValue) const;
        [[nodiscard]] std::int64_t getIntOr(std::string_view path, std::int64_t defaultValue) const;
        [[nodiscard]] std::uint64_t getUIntOr(std::string_view path, std::uint64_t defaultValue) const;
        [[nodiscard]] double getDoubleOr(std::string_view path, double defaultValue) const;
        [[nodiscard]] bool getBoolOr(std::string_view path, bool defaultValue) const;

        [[nodiscard]] dbase::Result<void> require(std::string_view path) const;
        [[nodiscard]] const std::unordered_map<std::string, ConfigValue>& values() const noexcept;

    protected:
        void setRoot(ConfigValue root);
        void set(std::string_view path, ConfigValue value);

    private:
        void markFlatCacheDirty() const noexcept;
        void rebuildFlatCache() const;
        void flattenInto(const ConfigValue& value, std::string path) const;

    private:
        ConfigValue m_root;
        mutable bool m_flatCacheDirty{true};
        mutable std::unordered_map<std::string, ConfigValue> m_flatValues;
};
}  // namespace dbase::config