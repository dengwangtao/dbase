#pragma once

#include "dbase/error/error.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dbase::fs
{
[[nodiscard]] bool exists(const std::filesystem::path& path);
[[nodiscard]] bool isFile(const std::filesystem::path& path);
[[nodiscard]] bool isDirectory(const std::filesystem::path& path);

[[nodiscard]] dbase::Result<std::filesystem::path> absolute(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<std::filesystem::path> canonical(const std::filesystem::path& path);

[[nodiscard]] dbase::Result<void> createDirectories(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> removeFile(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> removeAll(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> rename(const std::filesystem::path& from, const std::filesystem::path& to);
[[nodiscard]] dbase::Result<void> copyFile(
        const std::filesystem::path& from,
        const std::filesystem::path& to,
        bool overwrite = true);

[[nodiscard]] dbase::Result<std::string> readText(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<std::vector<std::string>> readLines(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> writeText(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] dbase::Result<void> writeTextAtomic(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] dbase::Result<void> appendText(const std::filesystem::path& path, std::string_view content);

[[nodiscard]] dbase::Result<std::vector<std::byte>> readBytes(const std::filesystem::path& path);
[[nodiscard]] dbase::Result<void> writeBytes(const std::filesystem::path& path, const std::vector<std::byte>& data);
[[nodiscard]] dbase::Result<void> writeBytesAtomic(const std::filesystem::path& path, const std::vector<std::byte>& data);

[[nodiscard]] dbase::Result<std::uintmax_t> fileSize(const std::filesystem::path& path);

[[nodiscard]] dbase::Result<std::vector<std::filesystem::path>> listFiles(
        const std::filesystem::path& path,
        bool recursive = false);
}  // namespace dbase::fs