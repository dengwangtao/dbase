#pragma once

#include "dbase/error/error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace dbase::config
{
class ConfigValue
{
    public:
        using Storage = std::variant<std::string, std::int64_t, double, bool>;

        ConfigValue() = default;
        ConfigValue(std::string value);
        ConfigValue(const char* value);
        ConfigValue(std::int64_t value);
        ConfigValue(double value);
        ConfigValue(bool value);

        [[nodiscard]] bool isString() const noexcept;
        [[nodiscard]] bool isInt() const noexcept;
        [[nodiscard]] bool isDouble() const noexcept;
        [[nodiscard]] bool isBool() const noexcept;

        [[nodiscard]] const std::string& asString() const;
        [[nodiscard]] std::int64_t asInt() const;
        [[nodiscard]] double asDouble() const;
        [[nodiscard]] bool asBool() const;

        [[nodiscard]] std::string toString() const;

        [[nodiscard]] dbase::Result<std::string> tryGetString() const;
        [[nodiscard]] dbase::Result<std::int64_t> tryGetInt() const;
        [[nodiscard]] dbase::Result<double> tryGetDouble() const;
        [[nodiscard]] dbase::Result<bool> tryGetBool() const;

    private:
        Storage m_value{std::string()};
};
}  // namespace dbase::config