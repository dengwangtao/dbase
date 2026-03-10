#pragma once

#include "dbase/error/error.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dbase::fs
{
[[nodiscard]] bool exists(const std::filesystem::path& path);
[[nodiscard]] bool isFile(const std::filesystem::path& path);
[[nodiscard]] bool isDirectory(const std::filesystem::path& path);

[[nodiscard]] dbase::Result<void> createDirectories(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> removeFile(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> removeAll(const std::filesystem::path& path);

[[nodiscard]] dbase::Result<std::string> readText(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> writeText(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] dbase::Result<void> appendText(const std::filesystem::path& path, std::string_view content);

[[nodiscard]] dbase::Result<std::vector<std::byte>> readBytes(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> writeBytes(const std::filesystem::path& path, const std::vector<std::byte>& data);

[[nodiscard]] dbase::Result<std::uintmax_t> fileSize(const std::filesystem::path& path);
}  // namespace dbase::fs