#pragma once

#include "dbase/error/error.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dbase::str
{
[[nodiscard]] bool startsWith(std::string_view str, std::string_view prefix) noexcept;
[[nodiscard]] bool endsWith(std::string_view str, std::string_view suffix) noexcept;
[[nodiscard]] bool contains(std::string_view str, std::string_view part) noexcept;
[[nodiscard]] bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept;

[[nodiscard]] std::string toLower(std::string_view str);
[[nodiscard]] std::string toUpper(std::string_view str);

[[nodiscard]] std::string_view trimLeftView(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string_view trimRightView(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string_view trimView(std::string_view str, std::string_view chars = " \r\n\t");

[[nodiscard]] std::string trimLeft(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string trimRight(std::string_view str, std::string_view chars = " \r\n\t");
[[nodiscard]] std::string trim(std::string_view str, std::string_view chars = " \r\n\t");

[[nodiscard]] std::string_view removePrefixView(std::string_view str, std::string_view prefix) noexcept;
[[nodiscard]] std::string_view removeSuffixView(std::string_view str, std::string_view suffix) noexcept;

[[nodiscard]] std::vector<std::string> split(std::string_view str, char delim, bool keepEmpty = false);
[[nodiscard]] std::vector<std::string> split(std::string_view str, std::string_view delim, bool keepEmpty = false);
[[nodiscard]] std::vector<std::string_view> splitView(std::string_view str, char delim, bool keepEmpty = false);
[[nodiscard]] std::vector<std::string_view> splitView(std::string_view str, std::string_view delim, bool keepEmpty = false);

[[nodiscard]] std::string join(std::span<const std::string> parts, std::string_view delim);
[[nodiscard]] std::string join(std::span<const std::string_view> parts, std::string_view delim);

[[nodiscard]] std::string replaceFirst(std::string_view str, std::string_view from, std::string_view to);
[[nodiscard]] std::string replaceAll(std::string_view str, std::string_view from, std::string_view to);

[[nodiscard]] dbase::Result<std::int32_t> toInt(std::string_view str);
[[nodiscard]] dbase::Result<std::int64_t> toInt64(std::string_view str);
[[nodiscard]] dbase::Result<double> toDouble(std::string_view str);
[[nodiscard]] dbase::Result<bool> toBool(std::string_view str);

}  // namespace dbase::str