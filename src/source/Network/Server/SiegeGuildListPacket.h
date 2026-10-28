#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
// C2/B4 and C2/B5 share a result and little-endian count followed by 14-byte entries.
struct SiegeGuildListView
{
    std::uint8_t result;
    std::span<const std::uint8_t> entries;
};

inline std::optional<SiegeGuildListView> ReadSiegeGuildList(std::span<const std::uint8_t> packet)
{
    constexpr std::size_t entrySize = 14;
    constexpr std::size_t headerSize = 9;
    if (packet.size() < headerSize)
        return std::nullopt;
    // The count starts at byte5, without native integer alignment padding.
    const std::uint32_t count = std::uint32_t(packet[5]) | (std::uint32_t(packet[6]) << 8)
        | (std::uint32_t(packet[7]) << 16) | (std::uint32_t(packet[8]) << 24);
    const auto available = packet.size() - headerSize;
    if (count > available / entrySize)
        return std::nullopt;
    return SiegeGuildListView{packet[4], packet.subspan(headerSize, count * entrySize)};
}
} // namespace Network::Packets
