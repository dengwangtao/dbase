#include "dbase/str/str.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace dbase::str
{
namespace
{
[[nodiscard]] char lowerAscii(char ch) noexcept
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

[[nodiscard]] std::string_view trimImplLeft(std::string_view str, std::string_view chars) noexcept
{
    const auto pos = str.find_first_not_of(chars);
    if (pos == std::string_view::npos)
    {
        return {};
    }
    return str.substr(pos);
}

[[nodiscard]] std::string_view trimImplRight(std::string_view str, std::string_view chars) noexcept
{
    const auto pos = str.find_last_not_of(chars);
    if (pos == std::string_view::npos)
    {
        return {};
    }
    return str.substr(0, pos + 1);
}

template <typename T>
[[nodiscard]] dbase::Result<T> makeParseError(std::string_view input, std::string_view typeName)
{
    return dbase::makeErrorResult<T>(
            dbase::ErrorCode::ParseError,
            "failed to parse '" + std::string(input) + "' as " + std::string(typeName));
}
}  // namespace

bool startsWith(std::string_view str, std::string_view prefix) noexcept
{
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view str, std::string_view suffix) noexcept
{
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

bool contains(std::string_view str, std::string_view part) noexcept
{
    return str.find(part) != std::string_view::npos;
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        if (lowerAscii(lhs[i]) != lowerAscii(rhs[i]))
        {
            return false;
        }
    }

    return true;
}

std::string toLower(std::string_view str)
{
    std::string out;
    out.reserve(str.size());
    for (char ch : str)
    {
        out.push_back(lowerAscii(ch));
    }
    return out;
}

std::string toUpper(std::string_view str)
{
    std::string out;
    out.reserve(str.size());
    for (char ch : str)
    {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::string_view trimLeftView(std::string_view str, std::string_view chars)
{
    return trimImplLeft(str, chars);
}

std::string_view trimRightView(std::string_view str, std::string_view chars)
{
    return trimImplRight(str, chars);
}

std::string_view trimView(std::string_view str, std::string_view chars)
{
    return trimImplRight(trimImplLeft(str, chars), chars);
}

std::string trimLeft(std::string_view str, std::string_view chars)
{
    return std::string(trimLeftView(str, chars));
}

std::string trimRight(std::string_view str, std::string_view chars)
{
    return std::string(trimRightView(str, chars));
}

std::string trim(std::string_view str, std::string_view chars)
{
    return std::string(trimView(str, chars));
}

std::string_view removePrefixView(std::string_view str, std::string_view prefix) noexcept
{
    if (startsWith(str, prefix))
    {
        return str.substr(prefix.size());
    }
    return str;
}

std::string_view removeSuffixView(std::string_view str, std::string_view suffix) noexcept
{
    if (endsWith(str, suffix))
    {
        return str.substr(0, str.size() - suffix.size());
    }
    return str;
}

std::vector<std::string_view> splitView(std::string_view str, char delim, bool keepEmpty)
{
    std::vector<std::string_view> out;

    std::size_t begin = 0;
    while (begin <= str.size())
    {
        const auto pos = str.find(delim, begin);
        const auto len = (pos == std::string_view::npos) ? (str.size() - begin) : (pos - begin);

        if (len != 0 || keepEmpty)
        {
            out.emplace_back(str.substr(begin, len));
        }

        if (pos == std::string_view::npos)
        {
            break;
        }

        begin = pos + 1;
    }

    return out;
}

std::vector<std::string_view> splitView(std::string_view str, std::string_view delim, bool keepEmpty)
{
    std::vector<std::string_view> out;

    if (delim.empty())
    {
        if (!str.empty() || keepEmpty)
        {
            out.emplace_back(str);
        }
        return out;
    }

    std::size_t begin = 0;
    while (begin <= str.size())
    {
        const auto pos = str.find(delim, begin);
        const auto len = (pos == std::string_view::npos) ? (str.size() - begin) : (pos - begin);

        if (len != 0 || keepEmpty)
        {
            out.emplace_back(str.substr(begin, len));
        }

        if (pos == std::string_view::npos)
        {
            break;
        }

        begin = pos + delim.size();
    }

    return out;
}

std::vector<std::string> split(std::string_view str, char delim, bool keepEmpty)
{
    auto views = splitView(str, delim, keepEmpty);
    std::vector<std::string> out;
    out.reserve(views.size());
    for (auto part : views)
    {
        out.emplace_back(part);
    }
    return out;
}

std::vector<std::string> split(std::string_view str, std::string_view delim, bool keepEmpty)
{
    auto views = splitView(str, delim, keepEmpty);
    std::vector<std::string> out;
    out.reserve(views.size());
    for (auto part : views)
    {
        out.emplace_back(part);
    }
    return out;
}

std::string join(std::span<const std::string> parts, std::string_view delim)
{
    if (parts.empty())
    {
        return {};
    }

    std::size_t total = 0;
    for (const auto& part : parts)
    {
        total += part.size();
    }
    total += (parts.size() - 1) * delim.size();

    std::string out;
    out.reserve(total);

    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (i != 0)
        {
            out.append(delim);
        }
        out.append(parts[i]);
    }

    return out;
}

std::string join(std::span<const std::string_view> parts, std::string_view delim)
{
    if (parts.empty())
    {
        return {};
    }

    std::size_t total = 0;
    for (const auto& part : parts)
    {
        total += part.size();
    }
    total += (parts.size() - 1) * delim.size();

    std::string out;
    out.reserve(total);

    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (i != 0)
        {
            out.append(delim);
        }
        out.append(parts[i]);
    }

    return out;
}

std::string replaceFirst(std::string_view str, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        return std::string(str);
    }

    const auto pos = str.find(from);
    if (pos == std::string_view::npos)
    {
        return std::string(str);
    }

    std::string out;
    out.reserve(str.size() - from.size() + to.size());
    out.append(str.substr(0, pos));
    out.append(to);
    out.append(str.substr(pos + from.size()));
    return out;
}

std::string replaceAll(std::string_view str, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        return std::string(str);
    }

    std::string out;
    out.reserve(str.size());

    std::size_t begin = 0;
    while (begin < str.size())
    {
        const auto pos = str.find(from, begin);
        if (pos == std::string_view::npos)
        {
            out.append(str.substr(begin));
            break;
        }

        out.append(str.substr(begin, pos - begin));
        out.append(to);
        begin = pos + from.size();
    }

    return out;
}

dbase::Result<std::int32_t> toInt(std::string_view str)
{
    const auto s = trimView(str);
    if (s.empty())
    {
        return makeParseError<std::int32_t>(str, "int32");
    }

    std::int32_t value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size())
    {
        return makeParseError<std::int32_t>(str, "int32");
    }

    return value;
}

dbase::Result<std::int64_t> toInt64(std::string_view str)
{
    const auto s = trimView(str);
    if (s.empty())
    {
        return makeParseError<std::int64_t>(str, "int64");
    }

    std::int64_t value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size())
    {
        return makeParseError<std::int64_t>(str, "int64");
    }

    return value;
}

dbase::Result<double> toDouble(std::string_view str)
{
    const auto s = trim(str);
    if (s.empty())
    {
        return makeParseError<double>(str, "double");
    }

    char* end = nullptr;
    const double value = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size())
    {
        return makeParseError<double>(str, "double");
    }

    return value;
}

dbase::Result<bool> toBool(std::string_view str)
{
    const auto s = toLower(trimView(str));

    if (s == "true" || s == "1" || s == "yes" || s == "on")
    {
        return true;
    }

    if (s == "false" || s == "0" || s == "no" || s == "off")
    {
        return false;
    }

    return makeParseError<bool>(str, "bool");
}

}  // namespace dbase::str