#pragma once

#include <cstdint>

namespace Render::Terrain::Height
{
    // Logical map id112 loads World113. Its stored integers include this bias.
    inline constexpr int FereaMapNumber = 112;
    inline constexpr float PackedHeightOffset = -500.f;

    constexpr bool UsesPackedHeight(int mapNumber, bool existingPackedMap)
    {
        return existingPackedMap || mapNumber == FereaMapNumber;
    }

    // The caller supplies three available bytes in stored order, without a row flip.
    constexpr float DecodePackedSample(const unsigned char* sample, float offset)
    {
        const auto value = (static_cast<std::uint32_t>(sample[0]) << 16)
                         | (static_cast<std::uint32_t>(sample[1]) << 8)
                         | static_cast<std::uint32_t>(sample[2]);
        return static_cast<float>(value) + offset;
    }

    constexpr float DecodeLegacySample(unsigned char sample, float factor)
    {
        return static_cast<float>(sample) * factor;
    }
}
