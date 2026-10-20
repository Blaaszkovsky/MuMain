// File envelope adapted from VD's MIT-licensed MuClientTools16.
// See THIRD_PARTY_NOTICES.md. This is an offline tool, not a game loader.
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Tools::Maps
{
    inline constexpr std::size_t EnvelopeBytes = 34;
    inline constexpr std::size_t MaxPayloadBytes = 256 * 256 * 3 + 4;
    std::vector<std::uint8_t> DecodeEnvelope(std::span<const std::uint8_t> source);
}
