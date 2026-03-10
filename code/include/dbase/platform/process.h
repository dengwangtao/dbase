#pragma once

#include "dbase/error/error.h"

#include <cstdint>
#include <filesystem>

namespace dbase::platform
{
[[nodiscard]] std::uint32_t pid() noexcept;
[[nodiscard]] std::uint64_t tid() noexcept;
[[nodiscard]] dbase::Result<std::filesystem::path> executablePath();
}  // namespace dbase::platform