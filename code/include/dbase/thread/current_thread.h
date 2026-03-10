#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace dbase::thread
{
namespace current_thread
{
[[nodiscard]] std::uint64_t tid() noexcept;
[[nodiscard]] const std::string& name() noexcept;

void setName(std::string_view name);
void sleepForMs(std::uint64_t ms);
void sleepForUs(std::uint64_t us);
}  // namespace current_thread
}  // namespace dbase::thread