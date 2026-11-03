#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
inline constexpr std::size_t SiegeNpcEntrySize = 28;

struct SiegeNpcListView
{
    std::uint8_t result;
    std::span<const std::uint8_t> entries;
};

inline std::optional<SiegeNpcListView> ReadSiegeNpcList(std::span<const std::uint8_t> packet)
{
    // C2/B3: result at 4, little-endian count at 8, then aligned 28-byte NPCs at 12.
    constexpr std::size_t headerSize = 12;
    constexpr std::uint8_t headerCode = 0xC2;
    constexpr std::uint8_t packetCode = 0xB3;
    if (packet.size() < headerSize || packet[0] != headerCode || packet[3] != packetCode)
        return std::nullopt;

    const auto frameSize = (std::size_t(packet[1]) << 8) | std::size_t(packet[2]);
    if (frameSize < headerSize || frameSize > packet.size())
        return std::nullopt;

    const auto count = std::uint32_t(packet[8]) | (std::uint32_t(packet[9]) << 8)
        | (std::uint32_t(packet[10]) << 16) | (std::uint32_t(packet[11]) << 24);
    const auto entriesSize = frameSize - headerSize;
    if (entriesSize % SiegeNpcEntrySize != 0 || count != entriesSize / SiegeNpcEntrySize)
        return std::nullopt;

    return SiegeNpcListView{packet[4], packet.subspan(headerSize, entriesSize)};
}
} // namespace Network::Packets
