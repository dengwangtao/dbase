#include "dbase/config/config_value.h"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dbase::config
{
namespace
{
[[nodiscard]] dbase::Error makeTypeError(std::string expected, std::string actual)
{
    return dbase::Error(
            dbase::ErrorCode::InvalidArgument,
            "config value type mismatch, expected " + std::move(expected) + ", actual " + std::move(actual));
}

[[nodiscard]] std::string typeName(const ConfigValue& value)
{
    if (value.isNull())
    {
        return "null";
    }
    if (value.isBool())
    {
        return "bool";
    }
    if (value.isInt())
    {
        return "int64";
    }
    if (value.isUInt())
    {
        return "uint64";
    }
    if (value.isDouble())
    {
        return "double";
    }
    if (value.isString())
    {
        return "string";
    }
    if (value.isArray())
    {
        return "array";
    }
    if (value.isObject())
    {
        return "object";
    }
    return "unknown";
}

[[nodiscard]] std::string escapeString(std::string_view input)
{
    std::string out;
    out.reserve(input.size() + 8);

    for (const char ch : input)
    {
        switch (ch)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += ch;
                break;
        }
    }

    return out;
}
}  // namespace

ConfigValue::ConfigValue(std::nullptr_t)
    : m_value(std::monostate{})
{
}

ConfigValue::ConfigValue(bool value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(std::int64_t value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(std::uint64_t value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(int value)
    : m_value(static_cast<std::int64_t>(value))
{
}

ConfigValue::ConfigValue(unsigned int value)
    : m_value(static_cast<std::uint64_t>(value))
{
}

ConfigValue::ConfigValue(double value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(std::string value)
    : m_value(std::move(value))
{
}

ConfigValue::ConfigValue(const char* value)
    : m_value(std::string(value == nullptr ? "" : value))
{
}

ConfigValue::ConfigValue(Array value)
    : m_value(std::move(value))
{
}

ConfigValue::ConfigValue(Object value)
    : m_value(std::move(value))
{
}

bool ConfigValue::isNull() const noexcept
{
    return std::holds_alternative<std::monostate>(m_value);
}

bool ConfigValue::isBool() const noexcept
{
    return std::holds_alternative<bool>(m_value);
}

bool ConfigValue::isInt() const noexcept
{
    return std::holds_alternative<std::int64_t>(m_value);
}

bool ConfigValue::isUInt() const noexcept
{
    return std::holds_alternative<std::uint64_t>(m_value);
}

bool ConfigValue::isDouble() const noexcept
{
    return std::holds_alternative<double>(m_value);
}

bool ConfigValue::isString() const noexcept
{
    return std::holds_alternative<std::string>(m_value);
}

bool ConfigValue::isArray() const noexcept
{
    return std::holds_alternative<Array>(m_value);
}

bool ConfigValue::isObject() const noexcept
{
    return std::holds_alternative<Object>(m_value);
}

bool ConfigValue::isNumber() const noexcept
{
    return isInt() || isUInt() || isDouble();
}

bool ConfigValue::asBool() const
{
    return std::get<bool>(m_value);
}

std::int64_t ConfigValue::asInt() const
{
    return std::get<std::int64_t>(m_value);
}

std::uint64_t ConfigValue::asUInt() const
{
    return std::get<std::uint64_t>(m_value);
}

double ConfigValue::asDouble() const
{
    return std::get<double>(m_value);
}

const std::string& ConfigValue::asString() const
{
    return std::get<std::string>(m_value);
}

const ConfigValue::Array& ConfigValue::asArray() const
{
    return std::get<Array>(m_value);
}

ConfigValue::Array& ConfigValue::asArray()
{
    return std::get<Array>(m_value);
}

const ConfigValue::Object& ConfigValue::asObject() const
{
    return std::get<Object>(m_value);
}

ConfigValue::Object& ConfigValue::asObject()
{
    return std::get<Object>(m_value);
}

std::string ConfigValue::toString() const
{
    if (isNull())
    {
        return "null";
    }

    if (isBool())
    {
        return asBool() ? "true" : "false";
    }

    if (isInt())
    {
        return std::to_string(asInt());
    }

    if (isUInt())
    {
        return std::to_string(asUInt());
    }

    if (isDouble())
    {
        std::ostringstream oss;
        oss << asDouble();
        return oss.str();
    }

    if (isString())
    {
        return asString();
    }

    if (isArray())
    {
        std::ostringstream oss;
        oss << '[';

        const auto& arr = asArray();
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0)
            {
                oss << ',';
            }
            oss << '"' << escapeString(arr[i].toString()) << '"';
        }

        oss << ']';
        return oss.str();
    }

    if (isObject())
    {
        std::ostringstream oss;
        oss << '{';

        bool first = true;
        for (const auto& [key, value] : asObject())
        {
            if (!first)
            {
                oss << ',';
            }
            first = false;
            oss << '"' << escapeString(key) << "\":\"" << escapeString(value.toString()) << '"';
        }

        oss << '}';
        return oss.str();
    }

    return {};
}

dbase::Result<bool> ConfigValue::tryGetBool() const
{
    if (!isBool())
    {
        return dbase::Result<bool>(makeTypeError("bool", typeName(*this)));
    }
    return asBool();
}

dbase::Result<std::int64_t> ConfigValue::tryGetInt() const
{
    if (isInt())
    {
        return asInt();
    }

    if (isUInt())
    {
        const auto value = asUInt();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            return dbase::Result<std::int64_t>(
                    dbase::Error(dbase::ErrorCode::InvalidArgument, "uint64 value out of int64 range"));
        }
        return static_cast<std::int64_t>(value);
    }

    return dbase::Result<std::int64_t>(makeTypeError("int64", typeName(*this)));
}

dbase::Result<std::uint64_t> ConfigValue::tryGetUInt() const
{
    if (isUInt())
    {
        return asUInt();
    }

    if (isInt())
    {
        const auto value = asInt();
        if (value < 0)
        {
            return dbase::Result<std::uint64_t>(
                    dbase::Error(dbase::ErrorCode::InvalidArgument, "negative int64 cannot convert to uint64"));
        }
        return static_cast<std::uint64_t>(value);
    }

    return dbase::Result<std::uint64_t>(makeTypeError("uint64", typeName(*this)));
}

dbase::Result<double> ConfigValue::tryGetDouble() const
{
    if (isDouble())
    {
        return asDouble();
    }

    if (isInt())
    {
        return static_cast<double>(asInt());
    }

    if (isUInt())
    {
        return static_cast<double>(asUInt());
    }

    return dbase::Result<double>(makeTypeError("double", typeName(*this)));
}

dbase::Result<std::string> ConfigValue::tryGetString() const
{
    if (!isString())
    {
        return dbase::Result<std::string>(makeTypeError("string", typeName(*this)));
    }
    return asString();
}

dbase::Result<const ConfigValue::Array*> ConfigValue::tryGetArray() const
{
    if (!isArray())
    {
        return dbase::Result<const Array*>(makeTypeError("array", typeName(*this)));
    }
    return &asArray();
}

dbase::Result<const ConfigValue::Object*> ConfigValue::tryGetObject() const
{
    if (!isObject())
    {
        return dbase::Result<const Object*>(makeTypeError("object", typeName(*this)));
    }
    return &asObject();
}
}  // namespace dbase::config