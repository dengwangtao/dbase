#include "dbase/config/toml_config.h"
#include "dbase/fs/fs.h"
#include "ext/toml++/toml.h"

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

[[nodiscard]] std::string formatNode(const toml::node& node)
{
    std::ostringstream oss;
    node.visit([&oss](const auto& concrete)
               { oss << concrete; });
    return oss.str();
}

[[nodiscard]] dbase::Result<ConfigValue> convertToml(const toml::node& node)
{
    if (const auto* value = node.as_boolean())
    {
        return ConfigValue(value->get());
    }

    if (const auto* value = node.as_integer())
    {
        const auto raw = value->get();
        if (raw >= 0)
        {
            return ConfigValue(static_cast<std::uint64_t>(raw));
        }
        return ConfigValue(static_cast<std::int64_t>(raw));
    }

    if (const auto* value = node.as_floating_point())
    {
        return ConfigValue(value->get());
    }

    if (const auto* value = node.as_string())
    {
        return ConfigValue(std::string(value->get()));
    }

    if (node.is_date() || node.is_time() || node.is_date_time())
    {
        return ConfigValue(formatNode(node));
    }

    if (const auto* array = node.as_array())
    {
        ConfigValue::Array result;
        result.reserve(array->size());

        for (const auto& child : *array)
        {
            auto childRet = convertToml(child);
            if (!childRet)
            {
                return dbase::Result<ConfigValue>(childRet.error());
            }
            result.push_back(std::move(childRet.value()));
        }

        return ConfigValue(std::move(result));
    }

    if (const auto* table = node.as_table())
    {
        ConfigValue::Object result;

        for (const auto& [key, child] : *table)
        {
            auto childRet = convertToml(child);
            if (!childRet)
            {
                return dbase::Result<ConfigValue>(childRet.error());
            }
            result.emplace(std::string(key.str()), std::move(childRet.value()));
        }

        return ConfigValue(std::move(result));
    }

    return dbase::Result<ConfigValue>(makeParseError("unsupported toml node type"));
}
}  // namespace

dbase::Result<TomlConfig> TomlConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::Result<TomlConfig>(textRet.error());
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

    auto rootRet = convertToml(root);
    if (!rootRet)
    {
        return dbase::Result<TomlConfig>(rootRet.error());
    }

    TomlConfig config;
    config.setRoot(std::move(rootRet.value()));
    return config;
}
}  // namespace dbase::config