#include "dbase/config/toml_config.h"

#include "dbase/fs/fs.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace dbase::config
{
namespace
{
[[nodiscard]] dbase::Error makeParseError(std::string message)
{
    return dbase::Error(dbase::ErrorCode::ParseError, std::move(message));
}

[[nodiscard]] std::string formatValue(const toml::node& node)
{
    std::ostringstream oss;
    node.visit([&oss](const auto& concrete)
               { oss << concrete; });
    return oss.str();
}

[[nodiscard]] ConfigValue toScalarConfigValue(const toml::node& node)
{
    if (const auto* value = node.as_boolean())
    {
        return ConfigValue(value->get());
    }

    if (const auto* value = node.as_integer())
    {
        return ConfigValue(static_cast<std::int64_t>(value->get()));
    }

    if (const auto* value = node.as_floating_point())
    {
        return ConfigValue(value->get());
    }

    if (const auto* value = node.as_string())
    {
        return ConfigValue(std::string(value->get()));
    }

    if (const auto* value = node.as_date())
    {
        return ConfigValue(formatValue(*value));
    }

    if (const auto* value = node.as_time())
    {
        return ConfigValue(formatValue(*value));
    }

    if (const auto* value = node.as_date_time())
    {
        return ConfigValue(formatValue(*value));
    }

    return ConfigValue(formatValue(node));
}
}  // namespace

dbase::Result<TomlConfig> TomlConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::makeErrorResult<TomlConfig>(textRet.error().code(), textRet.error().message());
    }
    return parse(textRet.value());
}

dbase::Result<TomlConfig> TomlConfig::fromString(std::string_view content)
{
    return parse(content);
}

dbase::Result<TomlConfig> TomlConfig::parse(std::string_view content)
{
    toml::table root;
    try
    {
        root = toml::parse(std::string(content));
    }
    catch (const toml::parse_error& ex)
    {
        return dbase::Result<TomlConfig>(makeParseError(std::string(ex.description())));
    }
    catch (const std::exception& ex)
    {
        return dbase::Result<TomlConfig>(makeParseError(ex.what()));
    }

    TomlConfig config;
    for (const auto& [key, child] : root)
    {
        config.flatten(child, std::string(key.str()));
    }
    return config;
}

void TomlConfig::flatten(const toml::node& node, std::string path)
{
    if (const auto* table = node.as_table())
    {
        if (!path.empty())
        {
            set(path, ConfigValue(formatValue(node)));
        }

        for (const auto& [key, child] : *table)
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += std::string(key.str());
            flatten(child, std::move(childPath));
        }
        return;
    }

    if (const auto* array = node.as_array())
    {
        if (!path.empty())
        {
            set(path, ConfigValue(formatValue(node)));
        }

        for (std::size_t i = 0; i < array->size(); ++i)
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += std::to_string(i);
            flatten(*array->get(i), std::move(childPath));
        }
        return;
    }

    if (!path.empty())
    {
        set(std::move(path), toScalarConfigValue(node));
    }
}
}  // namespace dbase::config