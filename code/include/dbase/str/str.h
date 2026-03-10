#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace dbase::str
{
[[nodiscard]] bool startsWith(std::string_view str, std::string_view prefix) noexcept;
[[nodiscard]] bool endsWith(std::string_view str, std::string_view suffix) noexcept;
[[nodiscard]] bool contains(std::string_view str, std::string_view part) noexcept;

[[nodiscard]] std::string toLower(std::string_view str);
[[nodiscard]] std::string toUpper(std::string_view str);

[[nodiscard]] std::string trimLeft(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string trimRight(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string trim(std::string_view str, std::string_view chars = " \r\n\t");

[[nodiscard]] std::vector<std::string> split(std::string_view str, char delim, bool keepEmpty = false);
[[nodiscard]] std::string join(const std::vector<std::string>& parts, std::string_view delim);
[[nodiscard]] std::string replaceAll(std::string_view str, std::string_view from, std::string_view to);

}  // namespace dbase::str