#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
struct SiegeNpcReply
{
    std::uint8_t result;
    std::uint32_t npcNumber;
    std::uint32_t npcIndex;
    // Repair: current/max HP. Upgrade: upgrade type/value. Buy: no extra fields.
    std::uint32_t firstValue;
    std::uint32_t secondValue;
};

inline std::optional<SiegeNpcReply> ReadSiegeNpcReply(std::span<const std::uint8_t> packet, std::uint8_t subcode)
{
    constexpr std::uint8_t buySubcode = 0x05;
    constexpr std::uint8_t repairSubcode = 0x06;
    constexpr std::uint8_t upgradeSubcode = 0x07;
    constexpr std::size_t buySize = 13;
    constexpr std::size_t updateSize = 21;
    if (subcode != buySubcode && subcode != repairSubcode && subcode != upgradeSubcode)
        return std::nullopt;
    const auto wireSize = subcode == buySubcode ? buySize : updateSize;
    constexpr std::uint8_t headerCode = 0xC1;
    constexpr std::uint8_t packetCode = 0xB2;
    if (packet.size() < wireSize || packet[0] != headerCode || packet[1] != wireSize
        || packet[2] != packetCode || packet[3] != subcode)
        return std::nullopt;

    const auto little32 = [packet](std::size_t offset)
    {
        return std::uint32_t(packet[offset]) | (std::uint32_t(packet[offset + 1]) << 8)
            | (std::uint32_t(packet[offset + 2]) << 16) | (std::uint32_t(packet[offset + 3]) << 24);
    };
    return SiegeNpcReply{packet[4], little32(5), little32(9),
                         subcode == buySubcode ? 0 : little32(13), subcode == buySubcode ? 0 : little32(17)};
}
} // namespace Network::Packets
