#include <doctest.h>

#include "Network/Server/CrownAccessStatePacket.h"

#include <array>
#include <cstdint>

TEST_CASE("Crown access replays actual nine-byte accumulated millisecond packets [network][siege][crown]")
{
    for (const std::uint8_t state : {std::uint8_t(0), std::uint8_t(2)})
    {
        for (const std::uint32_t elapsed : {0U, 1000U, 29000U})
        {
            CAPTURE(state);
            CAPTURE(elapsed);
            const std::array<std::uint8_t, 9> packet{0xC1, 9, 0xB2, 0x15, state,
                std::uint8_t(elapsed), std::uint8_t(elapsed >> 8), std::uint8_t(elapsed >> 16), std::uint8_t(elapsed >> 24)};
            const auto parsed = Network::Packets::ReadCrownAccessState(packet);
            CHECK(parsed.has_value());
            if (parsed)
            {
                CHECK(parsed->state == state);
                CHECK(parsed->accumulatedTimeMs == elapsed);
                CHECK(parsed->accumulatedTimeMs / 1000 == elapsed / 1000);
            }
        }
    }
}

TEST_CASE("Crown access rejects every truncated wire packet [network][siege][crown]")
{
    const std::array<std::uint8_t, 9> packet{0xC1, 9, 0xB2, 0x15, 0, 0xE8, 3, 0, 0};
    for (std::size_t length = 0; length < packet.size(); ++length)
    {
        CAPTURE(length);
        CHECK_FALSE(Network::Packets::ReadCrownAccessState(std::span(packet).first(length)).has_value());
    }
}

TEST_CASE("Crown access ignores dirty receive-buffer bytes after the wire end [network][siege][crown]")
{
    for (const std::uint8_t tail : {std::uint8_t(0), std::uint8_t(0xA5), std::uint8_t(0xFF)})
    {
        CAPTURE(tail);
        const std::array<std::uint8_t, 12> buffer{0xC1, 9, 0xB2, 0x15, 0, 0xE8, 3, 0, 0, tail, tail, tail};
        const auto parsed = Network::Packets::ReadCrownAccessState(buffer);
        REQUIRE(parsed.has_value());
        CHECK(parsed->accumulatedTimeMs == 1000);
        CHECK(parsed->accumulatedTimeMs / 1000 == 1);
        const auto bounded = Network::Packets::ReadCrownAccessState(std::span(buffer).first(9));
        CHECK(bounded.has_value());
        if (bounded)
            CHECK(bounded->accumulatedTimeMs == 1000);
    }
}
