#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace dbase::net
{
struct KcpPacketHeader
{
        std::uint32_t conv{0};
        std::uint32_t token{0};
};

class KcpPacket
{
    public:
        static constexpr std::size_t kHeaderSize = 8;

        [[nodiscard]] static bool tryParseHeader(std::span<const std::byte> packet, KcpPacketHeader& header) noexcept;
};
}  // namespace dbase::net