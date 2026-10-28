#include <doctest.h>

#include "Network/Server/SiegeGuildListPacket.h"

#include <array>
#include <cstdint>

TEST_CASE("Castle Siege guild list reads the real C2 wire count and complete entries [network][siege]")
{
    for (const std::uint8_t code : {std::uint8_t(0xB4), std::uint8_t(0xB5)})
    {
        CAPTURE(code);
        // C2 length37/code/result/count2LE, then two actual 14-byte records.
        std::array<std::uint8_t, 37> packet{0xC2, 0, 37, code, 1, 2, 0, 0, 0};
        packet[9] = 2;
        packet[10] = 1;
        packet[11] = 'C';
        packet[12] = 'S';
        packet[23] = 1;
        const auto parsed = Network::Packets::ReadSiegeGuildList(packet);
        REQUIRE(parsed.has_value());
        CHECK(parsed->result == 1);
        REQUIRE(parsed->entries.size() == 28);
        CHECK(parsed->entries[0] == 2);
        CHECK(parsed->entries[2] == 'C');
        CHECK(parsed->entries[14] == 1);
    }
}

TEST_CASE("Castle Siege guild list rejects truncated headers counts and entries [network][siege]")
{
    std::array<std::uint8_t, 37> packet{0xC2, 0, 37, 0xB5, 1, 2, 0, 0, 0};
    for (std::size_t length = 0; length < packet.size(); ++length)
        CHECK_FALSE(Network::Packets::ReadSiegeGuildList(std::span(packet).first(length)).has_value());
    packet[5] = 0xFF;
    packet[6] = 0xFF;
    packet[7] = 0xFF;
    packet[8] = 0xFF;
    CHECK_FALSE(Network::Packets::ReadSiegeGuildList(packet).has_value());
}

TEST_CASE("Castle Siege empty and failed guild lists retain their result without rows [network][siege]")
{
    for (const std::uint8_t result : {std::uint8_t(0), std::uint8_t(1), std::uint8_t(2), std::uint8_t(3)})
    {
        std::array<std::uint8_t, 9> packet{0xC2, 0, 9, 0xB5, result, 0, 0, 0, 0};
        const auto parsed = Network::Packets::ReadSiegeGuildList(packet);
        REQUIRE(parsed.has_value());
        CHECK(parsed->result == result);
        CHECK(parsed->entries.empty());
    }
}
