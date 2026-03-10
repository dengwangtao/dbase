#include "dbase/str/str.h"

#include <algorithm>
#include <cctype>

namespace dbase::str
{
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

std::string toLower(std::string_view str)
{
    std::string out;
    out.reserve(str.size());
    for (unsigned char ch : str)
    {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

std::string toUpper(std::string_view str)
{
    std::string out;
    out.reserve(str.size());
    for (unsigned char ch : str)
    {
        out.push_back(static_cast<char>(std::toupper(ch)));
    }
    return out;
}

std::string trimLeft(std::string_view str, std::string_view chars)
{
    const auto pos = str.find_first_not_of(chars);
    if (pos == std::string_view::npos)
    {
        return {};
    }
    return std::string(str.substr(pos));
}

std::string trimRight(std::string_view str, std::string_view chars)
{
    const auto pos = str.find_last_not_of(chars);
    if (pos == std::string_view::npos)
    {
        return {};
    }
    return std::string(str.substr(0, pos + 1));
}

std::string trim(std::string_view str, std::string_view chars)
{
    const auto begin = str.find_first_not_of(chars);
    if (begin == std::string_view::npos)
    {
        return {};
    }

    const auto end = str.find_last_not_of(chars);
    return std::string(str.substr(begin, end - begin + 1));
}

std::vector<std::string> split(std::string_view str, char delim, bool keepEmpty)
{
    std::vector<std::string> out;

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

std::string join(const std::vector<std::string>& parts, std::string_view delim)
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

}  // namespace dbase::str