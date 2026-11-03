#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
inline constexpr std::size_t RegisteredSiegeGuildEntrySize = 14;
inline constexpr std::size_t SiegeGuildEntrySize = 16;

// C2/B4 registered guilds and C2/B5 guild scores have distinct row layouts.
struct SiegeGuildListView
{
    std::uint8_t result;
    std::span<const std::uint8_t> entries;
};

inline std::optional<SiegeGuildListView> ReadSiegeGuildList(std::span<const std::uint8_t> packet)
{
    constexpr std::size_t headerSize = 12;
    if (packet.size() < headerSize)
        return std::nullopt;
    constexpr std::uint8_t registeredGuildCode = 0xB4;
    constexpr std::uint8_t guildCode = 0xB5;
    const std::size_t entrySize = packet[3] == registeredGuildCode ? RegisteredSiegeGuildEntrySize
        : packet[3] == guildCode ? SiegeGuildEntrySize : 0;
    if (entrySize == 0)
        return std::nullopt;
    const std::uint32_t count = std::uint32_t(packet[8]) | (std::uint32_t(packet[9]) << 8)
        | (std::uint32_t(packet[10]) << 16) | (std::uint32_t(packet[11]) << 24);
    const auto available = packet.size() - headerSize;
    if (count > available / entrySize)
        return std::nullopt;
    return SiegeGuildListView{packet[4], packet.subspan(headerSize, count * entrySize)};
}
} // namespace Network::Packets
