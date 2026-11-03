#include <doctest.h>

#include "Network/Server/SiegeGuildListPacket.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

TEST_CASE("Castle Siege guild list reads the real C2 wire count and complete entries [network][siege]")
{
    for (const std::uint8_t code : {std::uint8_t(0xB4), std::uint8_t(0xB5)})
    {
        CAPTURE(code);
        const std::size_t entrySize = code == 0xB4 ? 14 : 16;
        const auto size = 12 + 2 * entrySize;
        std::vector<std::uint8_t> packet(size);
        const std::array<std::uint8_t, 12> header{0xC2, 0, static_cast<std::uint8_t>(size), code,
                                                1, 0xA1, 0xB2, 0xC3, 2, 0, 0, 0};
        std::copy(header.begin(), header.end(), packet.begin());
        packet[12] = 2;
        packet[13] = 1;
        packet[14] = 'C';
        packet[15] = 'S';
        packet[12 + entrySize] = 1;
        if (code == 0xB5)
        {
            packet[24] = 0x78;
            packet[25] = 0x56;
            packet[26] = 0x34;
            packet[27] = 0x12;
        }
        const auto parsed = Network::Packets::ReadSiegeGuildList(packet);
        REQUIRE(parsed.has_value());
        CHECK(parsed->result == 1);
        REQUIRE(parsed->entries.size() == 2 * entrySize);
        CHECK(parsed->entries[0] == 2);
        CHECK(parsed->entries[2] == 'C');
        CHECK(parsed->entries[entrySize] == 1);
        if (code == 0xB5)
        {
            CHECK(parsed->entries[12] == 0x78);
            CHECK(parsed->entries[15] == 0x12);
        }
    }
}

TEST_CASE("Castle Siege guild list rejects truncated headers counts and entries [network][siege]")
{
    std::array<std::uint8_t, 44> packet{0xC2, 0, 44, 0xB5, 1, 0xA1, 0xB2, 0xC3, 2, 0, 0, 0};
    for (std::size_t length = 0; length < packet.size(); ++length)
        CHECK_FALSE(Network::Packets::ReadSiegeGuildList(std::span(packet).first(length)).has_value());
    packet[8] = 0xFF;
    packet[9] = 0xFF;
    packet[10] = 0xFF;
    packet[11] = 0xFF;
    CHECK_FALSE(Network::Packets::ReadSiegeGuildList(packet).has_value());
}

TEST_CASE("Castle Siege empty and failed guild lists retain their result without rows [network][siege]")
{
    for (const std::uint8_t result : {std::uint8_t(0), std::uint8_t(1), std::uint8_t(2), std::uint8_t(3)})
    {
        std::array<std::uint8_t, 12> packet{0xC2, 0, 12, 0xB5, result, 0xA1, 0xB2, 0xC3, 0, 0, 0, 0};
        const auto parsed = Network::Packets::ReadSiegeGuildList(packet);
        REQUIRE(parsed.has_value());
        CHECK(parsed->result == result);
        CHECK(parsed->entries.empty());
    }
}
