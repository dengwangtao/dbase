#include "dbase/config/json_config.h"
#include "dbase/fs/fs.h"
#include "ext/nlohmann/json.hpp"

#include <limits>
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

[[nodiscard]] dbase::Result<ConfigValue> convertJson(const Json& value)
{
    if (value.is_null())
    {
        return ConfigValue(nullptr);
    }

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
        return ConfigValue(value.get<std::uint64_t>());
    }

    if (value.is_number_float())
    {
        return ConfigValue(value.get<double>());
    }

    if (value.is_string())
    {
        return ConfigValue(value.get<std::string>());
    }

    if (value.is_array())
    {
        ConfigValue::Array arr;
        arr.reserve(value.size());

        for (const auto& item : value)
        {
            auto itemRet = convertJson(item);
            if (!itemRet)
            {
                return dbase::Result<ConfigValue>(itemRet.error());
            }
            arr.push_back(std::move(itemRet.value()));
        }

        return ConfigValue(std::move(arr));
    }

    if (value.is_object())
    {
        ConfigValue::Object obj;

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            auto childRet = convertJson(it.value());
            if (!childRet)
            {
                return dbase::Result<ConfigValue>(childRet.error());
            }
            obj.emplace(it.key(), std::move(childRet.value()));
        }

        return ConfigValue(std::move(obj));
    }

    return dbase::Result<ConfigValue>(makeParseError("unsupported json value type"));
}
}  // namespace

dbase::Result<JsonConfig> JsonConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::Result<JsonConfig>(textRet.error());
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

    auto rootRet = convertJson(root);
    if (!rootRet)
    {
        return dbase::Result<JsonConfig>(rootRet.error());
    }

    JsonConfig config;
    config.setRoot(std::move(rootRet.value()));
    return config;
}
}  // namespace dbase::config