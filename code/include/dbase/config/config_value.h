#pragma once

#include "dbase/error/error.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace dbase::config
{
class ConfigValue
{
    public:
        using Array = std::vector<ConfigValue>;
        using Object = std::unordered_map<std::string, ConfigValue>;
        using Storage = std::variant<
                std::monostate,
                bool,
                std::int64_t,
                std::uint64_t,
                double,
                std::string,
                Array,
                Object>;

        ConfigValue() = default;
        ConfigValue(std::nullptr_t);
        ConfigValue(bool value);
        ConfigValue(std::int64_t value);
        ConfigValue(std::uint64_t value);
        ConfigValue(int value);
        ConfigValue(unsigned int value);
        ConfigValue(double value);
        ConfigValue(std::string value);
        ConfigValue(const char* value);
        ConfigValue(Array value);
        ConfigValue(Object value);

        [[nodiscard]] bool isNull() const noexcept;
        [[nodiscard]] bool isBool() const noexcept;
        [[nodiscard]] bool isInt() const noexcept;
        [[nodiscard]] bool isUInt() const noexcept;
        [[nodiscard]] bool isDouble() const noexcept;
        [[nodiscard]] bool isString() const noexcept;
        [[nodiscard]] bool isArray() const noexcept;
        [[nodiscard]] bool isObject() const noexcept;
        [[nodiscard]] bool isNumber() const noexcept;

        [[nodiscard]] bool asBool() const;
        [[nodiscard]] std::int64_t asInt() const;
        [[nodiscard]] std::uint64_t asUInt() const;
        [[nodiscard]] double asDouble() const;
        [[nodiscard]] const std::string& asString() const;
        [[nodiscard]] const Array& asArray() const;
        [[nodiscard]] Array& asArray();
        [[nodiscard]] const Object& asObject() const;
        [[nodiscard]] Object& asObject();

        [[nodiscard]] std::string toString() const;

        [[nodiscard]] dbase::Result<bool> tryGetBool() const;
        [[nodiscard]] dbase::Result<std::int64_t> tryGetInt() const;
        [[nodiscard]] dbase::Result<std::uint64_t> tryGetUInt() const;
        [[nodiscard]] dbase::Result<double> tryGetDouble() const;
        [[nodiscard]] dbase::Result<std::string> tryGetString() const;
        [[nodiscard]] dbase::Result<const Array*> tryGetArray() const;
        [[nodiscard]] dbase::Result<const Object*> tryGetObject() const;

    private:
        Storage m_value{std::monostate{}};
};
}  // namespace dbase::config