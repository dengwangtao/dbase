#include "dbase/config/ini_config.h"

#include "dbase/fs/fs.h"
#include "dbase/str/str.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>

namespace dbase::config
{
namespace
{
[[nodiscard]] dbase::Error makeParseError(std::size_t lineNo, std::string message)
{
    return dbase::Error(
            dbase::ErrorCode::ParseError,
            "ini parse error at line " + std::to_string(lineNo) + ": " + std::move(message));
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
    const auto lowered = toLower(dbase::str::trim(text));
    if (lowered == "true" || lowered == "yes" || lowered == "on")
    {
        out = true;
        return true;
    }
    if (lowered == "false" || lowered == "no" || lowered == "off")
    {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool tryParseInt(std::string_view text, std::int64_t& out)
{
    const auto s = dbase::str::trim(text);
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
    const auto s = dbase::str::trim(text);
    if (s.empty())
    {
        return false;
    }

    char* parseEnd = nullptr;
    out = std::strtod(s.c_str(), &parseEnd);
    return parseEnd == s.c_str() + s.size();
}

[[nodiscard]] ConfigValue parseValue(std::string_view raw)
{
    const auto value = dbase::str::trim(raw);

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
}  // namespace

dbase::Result<IniConfig> IniConfig::fromFile(const std::filesystem::path& path)
{
    auto textRet = dbase::fs::readText(path);
    if (!textRet)
    {
        return dbase::Result<IniConfig>(textRet.error());
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

        std::string key = dbase::str::trim(std::string_view(trimmed).substr(0, pos));
        const auto value = std::string_view(trimmed).substr(pos + 1);

        if (key.empty())
        {
            return dbase::Result<IniConfig>(makeParseError(lineNo, "empty key"));
        }

        if (!currentSection.empty())
        {
            key = currentSection + "." + key;
        }

        config.set(key, parseValue(value));
    }

    return config;
}
}  // namespace dbase::config