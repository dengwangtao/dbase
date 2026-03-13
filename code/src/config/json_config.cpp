#include "dbase/config/json_config.h"

#include "dbase/fs/fs.h"

#include <cstdint>
#include <string>
#include <utility>

namespace dbase::config
{
namespace
{
using Json = nlohmann::json;

[[nodiscard]] dbase::Error makeParseError(std::string message)
{
    return dbase::Error(dbase::ErrorCode::ParseError, std::move(message));
}

[[nodiscard]] bool isIntegralNumber(const Json& value) noexcept
{
    return value.is_number_integer() || value.is_number_unsigned();
}

[[nodiscard]] ConfigValue toScalarConfigValue(const Json& value)
{
    if (value.is_boolean())
    {
        return ConfigValue(value.get<bool>());
    }

    if (value.is_number_integer())
    {
        return ConfigValue(value.get<std::int64_t>());
    }

    if (value.is_number_unsigned())
    {
        return ConfigValue(static_cast<std::int64_t>(value.get<std::uint64_t>()));
    }

    if (value.is_number_float())
    {
        return ConfigValue(value.get<double>());
    }

    if (value.is_string())
    {
        return ConfigValue(value.get<std::string>());
    }

    if (value.is_null())
    {
        return ConfigValue(std::string());
    }

    return ConfigValue(value.dump());
}
}  // namespace

dbase::Result<JsonConfig> JsonConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::makeErrorResult<JsonConfig>(textRet.error().code(), textRet.error().message());
    }
    return parse(textRet.value());
}

dbase::Result<JsonConfig> JsonConfig::fromString(std::string_view content)
{
    return parse(content);
}

dbase::Result<JsonConfig> JsonConfig::parse(std::string_view content)
{
    Json root;
    try
    {
        root = Json::parse(content.begin(), content.end());
    }
    catch (const Json::parse_error& ex)
    {
        return dbase::Result<JsonConfig>(makeParseError(ex.what()));
    }
    catch (const std::exception& ex)
    {
        return dbase::Result<JsonConfig>(makeParseError(ex.what()));
    }

    JsonConfig config;

    if (root.is_object())
    {
        for (auto it = root.begin(); it != root.end(); ++it)
        {
            config.flatten(it.value(), it.key());
        }
        return config;
    }

    if (root.is_array())
    {
        for (std::size_t i = 0; i < root.size(); ++i)
        {
            config.flatten(root[i], std::to_string(i));
        }
        return config;
    }

    config.set("value", toScalarConfigValue(root));
    return config;
}

void JsonConfig::flatten(const Json& value, std::string path)
{
    if (value.is_object())
    {
        if (!path.empty())
        {
            set(path, ConfigValue(value.dump()));
        }

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += it.key();
            flatten(it.value(), std::move(childPath));
        }
        return;
    }

    if (value.is_array())
    {
        if (!path.empty())
        {
            set(path, ConfigValue(value.dump()));
        }

        for (std::size_t i = 0; i < value.size(); ++i)
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += std::to_string(i);
            flatten(value[i], std::move(childPath));
        }
        return;
    }

    if (!path.empty())
    {
        set(std::move(path), toScalarConfigValue(value));
    }
}
}  // namespace dbase::config