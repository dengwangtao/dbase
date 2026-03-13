#include "dbase/config/xml_config.h"

#include "dbase/fs/fs.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <sstream>
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

    bool boolValue = false;
    if (tryParseBool(value, boolValue))
    {
        return ConfigValue(boolValue);
    }

    std::int64_t intValue = 0;
    if (tryParseInt(value, intValue))
    {
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

[[nodiscard]] bool hasElementChildren(const pugi::xml_node& node)
{
    for (auto child : node.children())
    {
        if (child.type() == pugi::node_element)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string nodeXmlString(const pugi::xml_node& node)
{
    std::ostringstream oss;
    node.print(oss, "", pugi::format_raw);
    return oss.str();
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
}  // namespace

dbase::Result<XmlConfig> XmlConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::makeErrorResult<XmlConfig>(textRet.error().code(), textRet.error().message());
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
        std::ostringstream oss;
        oss << result.description() << " (offset=" << result.offset << ")";
        return dbase::Result<XmlConfig>(makeParseError(oss.str()));
    }

    XmlConfig config;
    const auto root = doc.document_element();
    if (!root)
    {
        return dbase::Result<XmlConfig>(makeParseError("xml document has no root element"));
    }

    config.flatten(root, std::string(root.name()));
    return config;
}

void XmlConfig::flatten(const pugi::xml_node& node, std::string path)
{
    if (!node)
    {
        return;
    }

    for (auto attr : node.attributes())
    {
        std::string attrPath = path;
        if (!attrPath.empty())
        {
            attrPath += '.';
        }
        attrPath += '@';
        attrPath += attr.name();
        set(std::move(attrPath), parseScalar(attr.value()));
    }

    const bool hasChildren = hasElementChildren(node);
    const auto text = collectDirectText(node);

    if (!hasChildren)
    {
        set(path, parseScalar(text));
        return;
    }

    set(path, ConfigValue(nodeXmlString(node)));

    std::vector<std::pair<pugi::xml_node, std::string>> elementChildren;
    for (auto child : node.children())
    {
        if (child.type() == pugi::node_element)
        {
            elementChildren.emplace_back(child, std::string(child.name()));
        }
    }

    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& item : elementChildren)
    {
        ++counts[item.second];
    }

    std::unordered_map<std::string, std::size_t> indices;
    for (const auto& item : elementChildren)
    {
        const auto& child = item.first;
        const auto& name = item.second;

        std::string childPath = path;
        if (!childPath.empty())
        {
            childPath += '.';
        }
        childPath += name;

        if (counts[name] > 1)
        {
            const std::size_t index = indices[name]++;
            childPath += '.';
            childPath += std::to_string(index);
        }

        flatten(child, std::move(childPath));
    }

    if (!text.empty())
    {
        std::string textPath = path;
        textPath += ".#text";
        set(std::move(textPath), parseScalar(text));
    }
}
}  // namespace dbase::config