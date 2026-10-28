#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
struct CrownAccessState
{
    std::uint8_t state;
    std::uint32_t accumulatedTimeMs;
};

inline std::optional<CrownAccessState> ReadCrownAccessState(std::span<const std::uint8_t> packet)
{
    // C1/B2:15 has a byte state followed immediately by a little-endian millisecond count.
    constexpr std::size_t packetSize = 9;
    constexpr std::size_t timeOffset = 5;
    if (packet.size() < packetSize)
        return std::nullopt;
    const auto time = std::uint32_t(packet[timeOffset]) | (std::uint32_t(packet[timeOffset + 1]) << 8)
        | (std::uint32_t(packet[timeOffset + 2]) << 16) | (std::uint32_t(packet[timeOffset + 3]) << 24);
    return CrownAccessState{packet[4], time};
}
} // namespace Network::Packets
