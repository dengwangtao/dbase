#include "dbase/net/kcp_packet.h"

namespace dbase::net
{
namespace
{
[[nodiscard]] std::uint32_t decode32uLE(const std::byte* data) noexcept
{
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) | (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}
}  // namespace

bool KcpPacket::tryParseHeader(std::span<const std::byte> packet, KcpPacketHeader& header) noexcept
{
    if (packet.size() < KcpPacket::kHeaderSize)
    {
        return false;
    }

    header.conv = decode32uLE(packet.data());
    header.token = decode32uLE(packet.data() + 4);
    return true;
}
}  // namespace dbase::net