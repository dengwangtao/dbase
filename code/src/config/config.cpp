#include "dbase/config/config.h"

#include <cctype>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace dbase::config
{
namespace
{
[[nodiscard]] bool isIndexSegment(std::string_view segment) noexcept
{
    if (segment.empty())
    {
        return false;
    }

    for (const char ch : segment)
    {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<std::string_view> splitPath(std::string_view path)
{
    std::vector<std::string_view> parts;

    if (path.empty())
    {
        return parts;
    }

    std::size_t begin = 0;
    while (begin <= path.size())
    {
        const auto pos = path.find('.', begin);
        if (pos == std::string_view::npos)
        {
            parts.emplace_back(path.substr(begin));
            break;
        }
        parts.emplace_back(path.substr(begin, pos - begin));
        begin = pos + 1;
    }

    return parts;
}

[[nodiscard]] std::size_t parseIndex(std::string_view segment)
{
    std::size_t value = 0;
    for (const char ch : segment)
    {
        value = value * 10 + static_cast<std::size_t>(ch - '0');
    }
    return value;
}

const ConfigValue* getByPathConst(const ConfigValue& root, std::string_view path)
{
    if (path.empty())
    {
        return &root;
    }

    const auto parts = splitPath(path);
    const ConfigValue* current = &root;

    for (const auto part : parts)
    {
        if (isIndexSegment(part))
        {
            if (!current->isArray())
            {
                return nullptr;
            }

            const auto index = parseIndex(part);
            const auto& arr = current->asArray();
            if (index >= arr.size())
            {
                return nullptr;
            }

            current = &arr[index];
            continue;
        }

        if (!current->isObject())
        {
            return nullptr;
        }

        const auto& obj = current->asObject();
        const auto it = obj.find(std::string(part));
        if (it == obj.end())
        {
            return nullptr;
        }

        current = &it->second;
    }

    return current;
}

ConfigValue* getByPathMutable(ConfigValue& root, std::string_view path)
{
    if (path.empty())
    {
        return &root;
    }

    const auto parts = splitPath(path);
    ConfigValue* current = &root;

    for (const auto part : parts)
    {
        if (isIndexSegment(part))
        {
            if (!current->isArray())
            {
                return nullptr;
            }

            const auto index = parseIndex(part);
            auto& arr = current->asArray();
            if (index >= arr.size())
            {
                return nullptr;
            }

            current = &arr[index];
            continue;
        }

        if (!current->isObject())
        {
            return nullptr;
        }

        auto& obj = current->asObject();
        const auto it = obj.find(std::string(part));
        if (it == obj.end())
        {
            return nullptr;
        }

        current = &it->second;
    }

    return current;
}

void assignPath(ConfigValue& current, const std::vector<std::string_view>& parts, std::size_t index, ConfigValue value)
{
    const auto part = parts[index];
    const bool isLast = index + 1 == parts.size();

    if (isIndexSegment(part))
    {
        const auto arrayIndex = parseIndex(part);

        if (!current.isArray())
        {
            current = ConfigValue(ConfigValue::Array{});
        }

        auto& arr = current.asArray();
        if (arrayIndex >= arr.size())
        {
            arr.resize(arrayIndex + 1);
        }

        if (isLast)
        {
            arr[arrayIndex] = std::move(value);
            return;
        }

        assignPath(arr[arrayIndex], parts, index + 1, std::move(value));
        return;
    }

    if (!current.isObject())
    {
        current = ConfigValue(ConfigValue::Object{});
    }

    auto& obj = current.asObject();
    auto& child = obj[std::string(part)];

    if (isLast)
    {
        child = std::move(value);
        return;
    }

    assignPath(child, parts, index + 1, std::move(value));
}

[[nodiscard]] dbase::Error makeNotFoundError(std::string_view path)
{
    return dbase::Error(dbase::ErrorCode::NotFound, "config path not found: " + std::string(path));
}
}  // namespace

Config::Config()
    : m_root(ConfigValue::Object{})
{
}

const ConfigValue& Config::root() const noexcept
{
    return m_root;
}

ConfigValue& Config::root() noexcept
{
    markFlatCacheDirty();
    return m_root;
}

bool Config::has(std::string_view path) const noexcept
{
    return getByPathConst(m_root, path) != nullptr;
}

dbase::Result<const ConfigValue*> Config::get(std::string_view path) const
{
    const auto* value = getByPathConst(m_root, path);
    if (value == nullptr)
    {
        return dbase::Result<const ConfigValue*>(makeNotFoundError(path));
    }
    return value;
}

dbase::Result<ConfigValue*> Config::get(std::string_view path)
{
    auto* value = getByPathMutable(m_root, path);
    if (value == nullptr)
    {
        return dbase::Result<ConfigValue*>(makeNotFoundError(path));
    }
    return value;
}

dbase::Result<std::string> Config::getString(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<std::string>(ret.error());
    }
    return ret.value()->tryGetString();
}

dbase::Result<std::int64_t> Config::getInt(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<std::int64_t>(ret.error());
    }
    return ret.value()->tryGetInt();
}

dbase::Result<std::uint64_t> Config::getUInt(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<std::uint64_t>(ret.error());
    }
    return ret.value()->tryGetUInt();
}

dbase::Result<double> Config::getDouble(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<double>(ret.error());
    }
    return ret.value()->tryGetDouble();
}

dbase::Result<bool> Config::getBool(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<bool>(ret.error());
    }
    return ret.value()->tryGetBool();
}

dbase::Result<const ConfigValue::Array*> Config::getArray(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<const ConfigValue::Array*>(ret.error());
    }
    return ret.value()->tryGetArray();
}

dbase::Result<const ConfigValue::Object*> Config::getObject(std::string_view path) const
{
    auto ret = get(path);
    if (!ret)
    {
        return dbase::Result<const ConfigValue::Object*>(ret.error());
    }
    return ret.value()->tryGetObject();
}

std::string Config::getStringOr(std::string_view path, std::string defaultValue) const
{
    auto ret = getString(path);
    if (!ret)
    {
        return defaultValue;
    }
    return ret.value();
}

std::int64_t Config::getIntOr(std::string_view path, std::int64_t defaultValue) const
{
    auto ret = getInt(path);
    if (!ret)
    {
        return defaultValue;
    }
    return ret.value();
}

std::uint64_t Config::getUIntOr(std::string_view path, std::uint64_t defaultValue) const
{
    auto ret = getUInt(path);
    if (!ret)
    {
        return defaultValue;
    }
    return ret.value();
}

double Config::getDoubleOr(std::string_view path, double defaultValue) const
{
    auto ret = getDouble(path);
    if (!ret)
    {
        return defaultValue;
    }
    return ret.value();
}

bool Config::getBoolOr(std::string_view path, bool defaultValue) const
{
    auto ret = getBool(path);
    if (!ret)
    {
        return defaultValue;
    }
    return ret.value();
}

dbase::Result<void> Config::require(std::string_view path) const
{
    if (!has(path))
    {
        return dbase::Result<void>(makeNotFoundError(path));
    }
    return {};
}

const std::unordered_map<std::string, ConfigValue>& Config::values() const noexcept
{
    if (m_flatCacheDirty)
    {
        rebuildFlatCache();
    }
    return m_flatValues;
}

void Config::setRoot(ConfigValue root)
{
    m_root = std::move(root);
    markFlatCacheDirty();
}

void Config::set(std::string_view path, ConfigValue value)
{
    if (path.empty())
    {
        setRoot(std::move(value));
        return;
    }

    const auto parts = splitPath(path);
    if (parts.empty())
    {
        setRoot(std::move(value));
        return;
    }

    assignPath(m_root, parts, 0, std::move(value));
    markFlatCacheDirty();
}

void Config::markFlatCacheDirty() const noexcept
{
    m_flatCacheDirty = true;
}

void Config::rebuildFlatCache() const
{
    m_flatValues.clear();
    flattenInto(m_root, {});
    m_flatCacheDirty = false;
}

void Config::flattenInto(const ConfigValue& value, std::string path) const
{
    if (!path.empty())
    {
        m_flatValues[path] = value;
    }

    if (value.isObject())
    {
        for (const auto& [key, child] : value.asObject())
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += key;
            flattenInto(child, std::move(childPath));
        }
        return;
    }

    if (value.isArray())
    {
        const auto& arr = value.asArray();
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            std::string childPath = path;
            if (!childPath.empty())
            {
                childPath += '.';
            }
            childPath += std::to_string(i);
            flattenInto(arr[i], std::move(childPath));
        }
    }
}
}  // namespace dbase::config