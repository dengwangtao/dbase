#include "dbase/config/xml_config.h"
#include "dbase/fs/fs.h"
#include "ext/pugixml/pugixml.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dbase::config
{
namespace
{
[[nodiscard]] std::string trim(std::string_view input)
{
    std::size_t begin = 0;
    std::size_t end = input.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(input[begin])) != 0)
    {
        ++begin;
    }

    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)
    {
        --end;
    }

    return std::string(input.substr(begin, end - begin));
}

[[nodiscard]] std::string toLower(std::string_view input)
{
    std::string result(input);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
    return result;
}

[[nodiscard]] bool tryParseBool(std::string_view text, bool& out)
{
    const auto lowered = toLower(trim(text));
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on")
    {
        out = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off")
    {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool tryParseInt(std::string_view text, std::int64_t& out)
{
    const auto s = trim(text);
    if (s.empty())
    {
        return false;
    }

    const char* begin = s.data();
    const char* end = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc() && ptr == end;
}

[[nodiscard]] bool tryParseDouble(std::string_view text, double& out)
{
    const auto s = trim(text);
    if (s.empty())
    {
        return false;
    }

    char* parseEnd = nullptr;
    out = std::strtod(s.c_str(), &parseEnd);
    return parseEnd == s.c_str() + s.size();
}

[[nodiscard]] ConfigValue parseScalar(std::string_view raw)
{
    const auto value = trim(raw);

    if (value.empty())
    {
        return ConfigValue(std::string());
    }

    bool boolValue = false;
    if (tryParseBool(value, boolValue))
    {
        return ConfigValue(boolValue);
    }

    std::int64_t intValue = 0;
    if (tryParseInt(value, intValue))
    {
        if (intValue >= 0)
        {
            return ConfigValue(static_cast<std::uint64_t>(intValue));
        }
        return ConfigValue(intValue);
    }

    double doubleValue = 0.0;
    if (tryParseDouble(value, doubleValue))
    {
        return ConfigValue(doubleValue);
    }

    return ConfigValue(value);
}

[[nodiscard]] dbase::Error makeParseError(std::string message)
{
    return dbase::Error(dbase::ErrorCode::ParseError, std::move(message));
}

[[nodiscard]] std::string collectDirectText(const pugi::xml_node& node)
{
    std::string result;

    for (auto child : node.children())
    {
        if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
        {
            result += child.value();
        }
    }

    return trim(result);
}

[[nodiscard]] ConfigValue buildElementValue(const pugi::xml_node& node)
{
    ConfigValue::Object result;

    for (auto attr : node.attributes())
    {
        result.emplace(std::string("@") + attr.name(), parseScalar(attr.value()));
    }

    std::unordered_map<std::string, std::vector<pugi::xml_node>> groupedChildren;
    std::vector<std::string> order;

    for (auto child : node.children())
    {
        if (child.type() != pugi::node_element)
        {
            continue;
        }

        const std::string name = child.name();
        if (groupedChildren.find(name) == groupedChildren.end())
        {
            order.push_back(name);
        }
        groupedChildren[name].push_back(child);
    }

    for (const auto& name : order)
    {
        auto& group = groupedChildren[name];
        if (group.size() == 1)
        {
            result.emplace(name, buildElementValue(group.front()));
            continue;
        }

        ConfigValue::Array arr;
        arr.reserve(group.size());

        for (const auto& child : group)
        {
            arr.push_back(buildElementValue(child));
        }

        result.emplace(name, ConfigValue(std::move(arr)));
    }

    const auto text = collectDirectText(node);

    if (result.empty())
    {
        return parseScalar(text);
    }

    if (!text.empty())
    {
        result.emplace("#text", ConfigValue(text));
    }

    return ConfigValue(std::move(result));
}
}  // namespace

dbase::Result<XmlConfig> XmlConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::Result<XmlConfig>(textRet.error());
    }
    return parse(textRet.value());
}

dbase::Result<XmlConfig> XmlConfig::fromString(std::string_view content)
{
    return parse(content);
}

dbase::Result<XmlConfig> XmlConfig::parse(std::string_view content)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result =
            doc.load_buffer(content.data(), content.size(), pugi::parse_default | pugi::parse_ws_pcdata);

    if (!result)
    {
        return dbase::Result<XmlConfig>(
                makeParseError(std::string(result.description()) + " (offset=" + std::to_string(result.offset) + ")"));
    }

    const auto rootNode = doc.document_element();
    if (!rootNode)
    {
        return dbase::Result<XmlConfig>(makeParseError("xml document has no root element"));
    }

    ConfigValue::Object root;
    root.emplace(rootNode.name(), buildElementValue(rootNode));

    XmlConfig config;
    config.setRoot(ConfigValue(std::move(root)));
    return config;
}
}  // namespace dbase::config