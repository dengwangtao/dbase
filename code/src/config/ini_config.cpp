#include "dbase/config/ini_config.h"
#include "dbase/str/str.h"

#include "dbase/fs/fs.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace dbase::config
{
namespace
{

[[nodiscard]] ConfigValue parseValue(std::string_view raw)
{
    const auto value = dbase::str::trim(raw);

    if (auto boolValue = dbase::str::toBool(value); boolValue)
    {
        return ConfigValue(boolValue.value());
    }

    if (auto intValue = dbase::str::toInt64(value); intValue)
    {
        return ConfigValue(intValue.value());
    }

    if (auto doubleValue = dbase::str::toDouble(value); doubleValue)
    {
        return ConfigValue(doubleValue.value());
    }

    return ConfigValue(value);
}

[[nodiscard]] dbase::Error makeParseError(std::size_t lineNo, std::string message)
{
    std::ostringstream oss;
    oss << "line " << lineNo << ": " << message;
    return dbase::Error(dbase::ErrorCode::ParseError, oss.str());
}
}  // namespace

ConfigValue::ConfigValue(std::string value)
    : m_value(std::move(value))
{
}

ConfigValue::ConfigValue(const char* value)
    : m_value(std::string(value == nullptr ? "" : value))
{
}

ConfigValue::ConfigValue(std::int64_t value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(double value)
    : m_value(value)
{
}

ConfigValue::ConfigValue(bool value)
    : m_value(value)
{
}

bool ConfigValue::isString() const noexcept
{
    return std::holds_alternative<std::string>(m_value);
}

bool ConfigValue::isInt() const noexcept
{
    return std::holds_alternative<std::int64_t>(m_value);
}

bool ConfigValue::isDouble() const noexcept
{
    return std::holds_alternative<double>(m_value);
}

bool ConfigValue::isBool() const noexcept
{
    return std::holds_alternative<bool>(m_value);
}

const std::string& ConfigValue::asString() const
{
    return std::get<std::string>(m_value);
}

std::int64_t ConfigValue::asInt() const
{
    return std::get<std::int64_t>(m_value);
}

double ConfigValue::asDouble() const
{
    return std::get<double>(m_value);
}

bool ConfigValue::asBool() const
{
    return std::get<bool>(m_value);
}

std::string ConfigValue::toString() const
{
    if (isString())
    {
        return asString();
    }

    if (isInt())
    {
        return std::to_string(asInt());
    }

    if (isDouble())
    {
        std::ostringstream oss;
        oss << asDouble();
        return oss.str();
    }

    return asBool() ? "true" : "false";
}

dbase::Result<std::string> ConfigValue::tryGetString() const
{
    if (!isString())
    {
        return dbase::makeErrorResult<std::string>(dbase::ErrorCode::InvalidArgument, "config value is not string");
    }

    return asString();
}

dbase::Result<std::int64_t> ConfigValue::tryGetInt() const
{
    if (!isInt())
    {
        return dbase::makeErrorResult<std::int64_t>(dbase::ErrorCode::InvalidArgument, "config value is not int");
    }

    return asInt();
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

    return dbase::makeErrorResult<double>(dbase::ErrorCode::InvalidArgument, "config value is not double");
}

dbase::Result<bool> ConfigValue::tryGetBool() const
{
    if (!isBool())
    {
        return dbase::makeErrorResult<bool>(dbase::ErrorCode::InvalidArgument, "config value is not bool");
    }

    return asBool();
}

bool Config::has(const std::string& key) const noexcept
{
    return m_values.find(key) != m_values.end();
}

dbase::Result<const ConfigValue*> Config::get(const std::string& key) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
    {
        return dbase::makeErrorResult<const ConfigValue*>(dbase::ErrorCode::NotFound, "config key not found: " + key);
    }

    return &it->second;
}

dbase::Result<std::string> Config::getString(const std::string& key) const
{
    auto ret = get(key);
    if (!ret)
    {
        return dbase::makeErrorResult<std::string>(ret.error().code(), ret.error().message());
    }

    return ret.value()->tryGetString();
}

dbase::Result<std::int64_t> Config::getInt(const std::string& key) const
{
    auto ret = get(key);
    if (!ret)
    {
        return dbase::makeErrorResult<std::int64_t>(ret.error().code(), ret.error().message());
    }

    return ret.value()->tryGetInt();
}

dbase::Result<double> Config::getDouble(const std::string& key) const
{
    auto ret = get(key);
    if (!ret)
    {
        return dbase::makeErrorResult<double>(ret.error().code(), ret.error().message());
    }

    return ret.value()->tryGetDouble();
}

dbase::Result<bool> Config::getBool(const std::string& key) const
{
    auto ret = get(key);
    if (!ret)
    {
        return dbase::makeErrorResult<bool>(ret.error().code(), ret.error().message());
    }

    return ret.value()->tryGetBool();
}

std::string Config::getStringOr(const std::string& key, std::string defaultValue) const
{
    auto ret = getString(key);
    if (!ret)
    {
        return defaultValue;
    }

    return ret.value();
}

std::int64_t Config::getIntOr(const std::string& key, std::int64_t defaultValue) const
{
    auto ret = getInt(key);
    if (!ret)
    {
        return defaultValue;
    }

    return ret.value();
}

double Config::getDoubleOr(const std::string& key, double defaultValue) const
{
    auto ret = getDouble(key);
    if (!ret)
    {
        return defaultValue;
    }

    return ret.value();
}

bool Config::getBoolOr(const std::string& key, bool defaultValue) const
{
    auto ret = getBool(key);
    if (!ret)
    {
        return defaultValue;
    }

    return ret.value();
}

dbase::Result<void> Config::require(const std::string& key) const
{
    if (!has(key))
    {
        return dbase::makeError(dbase::ErrorCode::NotFound, "required config key not found: " + key);
    }

    return {};
}

const std::unordered_map<std::string, ConfigValue>& Config::values() const noexcept
{
    return m_values;
}

void Config::set(std::string key, ConfigValue value)
{
    m_values[std::move(key)] = std::move(value);
}

dbase::Result<IniConfig> IniConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::makeErrorResult<IniConfig>(textRet.error().code(), textRet.error().message());
    }

    return parse(textRet.value());
}

dbase::Result<IniConfig> IniConfig::fromString(std::string_view content)
{
    return parse(content);
}

dbase::Result<IniConfig> IniConfig::parse(std::string_view content)
{
    IniConfig config;

    std::istringstream iss{std::string(content)};
    std::string line;
    std::string currentSection;
    std::size_t lineNo = 0;

    while (std::getline(iss, line))
    {
        ++lineNo;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const auto trimmed = dbase::str::trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        if (trimmed[0] == '#' || trimmed[0] == ';')
        {
            continue;
        }

        if (trimmed.front() == '[')
        {
            if (trimmed.back() != ']')
            {
                return dbase::Result<IniConfig>(makeParseError(lineNo, "invalid section syntax"));
            }

            currentSection = dbase::str::trim(std::string_view(trimmed).substr(1, trimmed.size() - 2));
            if (currentSection.empty())
            {
                return dbase::Result<IniConfig>(makeParseError(lineNo, "empty section name"));
            }

            continue;
        }

        const auto pos = trimmed.find('=');
        if (pos == std::string::npos)
        {
            return dbase::Result<IniConfig>(makeParseError(lineNo, "missing '='"));
        }

        auto key = dbase::str::trim(std::string_view(trimmed).substr(0, pos));
        auto value = std::string_view(trimmed).substr(pos + 1);

        if (key.empty())
        {
            return dbase::Result<IniConfig>(makeParseError(lineNo, "empty key"));
        }

        if (!currentSection.empty())
        {
            key = currentSection + "." + key;
        }

        config.set(std::move(key), parseValue(value));
    }

    return config;
}

}  // namespace dbase::config