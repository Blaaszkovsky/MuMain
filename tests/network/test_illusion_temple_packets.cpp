#include <doctest.h>

#include "Network/Server/IllusionTemplePacketReader.h"
#include "Dotnet/PacketFunctions_ClientToServer.h"
#include "UI/Events/CursedTempleScenePolicy.h"

#include <array>
#include <string>
#include <type_traits>

namespace
{
using Reader = Network::Packets::IllusionTemplePacketReader;
constexpr std::array<std::uint8_t, 24> StatePacket{
    0xC1, 24, 0xBF, 1, 0x34, 0x12, 0xCD, 0xAB, 40, 41, 2, 3, 1, 2,
    0x23, 1, 45, 50, 51, 0x56, 4, 46, 60, 61};
constexpr std::array<std::uint8_t, 41> ResultPacket{
    0xC1, 41, 0xBF, 4, 2, 3, 2,
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 45, 0, 0x18, 0x87, 0xD6, 0x12, 0,
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 46, 1, 0x30, 0xFE, 0xFF, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 11> SkillPacket{
    0xC1, 11, 0xBF, 2, 1, 0, 210, 0x34, 0x12, 0xCD, 0xAB};

template<std::size_t Size, typename Read>
void CheckTruncated(const std::array<std::uint8_t, Size>& packet, Read read)
{
    for (std::size_t length = 0; length < Size; ++length)
    {
        CAPTURE(length);
        CHECK_FALSE(read(std::span(packet).first(length)).has_value());
    }
}
}

TEST_CASE("Illusion Temple state reads five-byte party records and high object IDs [network][illusion_temple]")
{
    const auto state = Reader::ReadState(StatePacket);
    REQUIRE(state);
    CHECK(state->remainingSeconds == 0x1234);
    CHECK(state->relicHolder == 0xABCD);
    CHECK(state->x == 40);
    CHECK(state->y == 41);
    CHECK(state->alliedScore == 2);
    CHECK(state->illusionScore == 3);
    CHECK(state->team == 1);
    REQUIRE(state->members.size() == 10);
    const auto first = Reader::ReadPartyMember(state->members, 0);
    const auto second = Reader::ReadPartyMember(state->members, 1);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->id == 0x123);
    CHECK(second->id == 0x456);
    CHECK(second->map == 46);
    CHECK(second->x == 60);
    CHECK(second->y == 61);
    CHECK_FALSE(Reader::ReadPartyMember(state->members, 2));
}

TEST_CASE("Illusion Temple state rejects incomplete records and retains empty-team sentinel [network][illusion_temple]")
{
    CheckTruncated(StatePacket, Reader::ReadState);
    auto packet = StatePacket;
    packet[13] = 255;
    CHECK_FALSE(Reader::ReadState(packet));
    packet[13] = 0;
    packet[6] = packet[7] = 0xFF;
    const auto empty = Reader::ReadState(std::span(packet).first(14));
    REQUIRE(empty);
    CHECK(empty->relicHolder == 0xFFFF);
    CHECK(empty->members.empty());
    CHECK_FALSE(Reader::ReadPartyMember(empty->members, 0));
}

TEST_CASE("Illusion Temple result reads seventeen-byte rows and signed experience [network][illusion_temple]")
{
    const auto result = Reader::ReadResult(ResultPacket);
    REQUIRE(result);
    CHECK(result->alliedScore == 2);
    CHECK(result->illusionScore == 3);
    REQUIRE(result->players.size() == 34);
    const auto first = Reader::ReadPlayerResult(result->players, 0);
    const auto second = Reader::ReadPlayerResult(result->players, 1);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(std::string(first->name.data(), first->name.size()) == "ABCDEFGHIJ");
    CHECK(first->map == 45);
    CHECK(first->team == 0);
    CHECK(first->playerClass == 0x18);
    CHECK(first->addedExperience == 1234567);
    CHECK(std::string(second->name.data(), second->name.size()) == "KLMNOPQRST");
    CHECK(second->map == 46);
    CHECK(second->team == 1);
    CHECK(second->playerClass == 0x30);
    CHECK(second->addedExperience == -2);
    CHECK_FALSE(Reader::ReadPlayerResult(result->players, 2));
}

TEST_CASE("Illusion Temple result rejects incomplete rows and accepts empty result [network][illusion_temple]")
{
    CheckTruncated(ResultPacket, Reader::ReadResult);
    auto packet = ResultPacket;
    packet[6] = 255;
    CHECK_FALSE(Reader::ReadResult(packet));
    packet[6] = 0;
    const auto empty = Reader::ReadResult(std::span(packet).first(7));
    REQUIRE(empty);
    CHECK(empty->players.empty());
    CHECK_FALSE(Reader::ReadPlayerResult(empty->players, 0));
}

TEST_CASE("Illusion Temple skill response keeps mixed endian fields without native padding [network][illusion_temple]")
{
    const auto result = Reader::ReadSkillResult(SkillPacket);
    REQUIRE(result);
    CHECK(result->result == 1);
    CHECK(result->skill == 210);
    CHECK(result->source == 0x1234);
    CHECK(result->target == 0xABCD);
    CheckTruncated(SkillPacket, Reader::ReadSkillResult);
    const std::array<std::uint8_t, 8> end{0xC1, 8, 0xBF, 7, 0, 210, 0xCD, 0xAB};
    const auto ended = Reader::ReadSkillEnd(end);
    REQUIRE(ended);
    CHECK(ended->skill == 210);
    CHECK(ended->target == 0xABCD);
    CheckTruncated(end, Reader::ReadSkillEnd);
}

TEST_CASE("Illusion Temple decoders ignore dirty receive-buffer tail [network][illusion_temple]")
{
    std::array<std::uint8_t, 64> buffer;
    buffer.fill(0xA5);
    std::copy(StatePacket.begin(), StatePacket.end(), buffer.begin());
    const auto state = Reader::ReadState(buffer);
    REQUIRE(state);
    REQUIRE(state->members.size() == 10);
    CHECK(Reader::ReadPartyMember(state->members, 1)->id == 0x456);
    std::copy(ResultPacket.begin(), ResultPacket.end(), buffer.begin());
    const auto result = Reader::ReadResult(buffer);
    REQUIRE(result);
    REQUIRE(result->players.size() == 34);
    CHECK(Reader::ReadPlayerResult(result->players, 0)->addedExperience == 1234567);
    std::copy(SkillPacket.begin(), SkillPacket.end(), buffer.begin());
    const auto skill = Reader::ReadSkillResult(buffer);
    REQUIRE(skill);
    CHECK(skill->source == 0x1234);
    CHECK(skill->target == 0xABCD);
}

TEST_CASE("Illusion Temple fixed messages reject all truncated prefixes [network][illusion_temple]")
{
    const std::array<std::uint8_t, 5> entry{0xC1, 5, 0xBF, 0, 7};
    CHECK(Reader::ReadEntryResult(entry) == 7);
    CheckTruncated(entry, Reader::ReadEntryResult);
    const std::array<std::uint8_t, 10> counts{0xC1, 10, 0xBF, 3, 1, 2, 3, 4, 5, 6};
    const auto entered = Reader::ReadEntryCounts(counts);
    REQUIRE(entered);
    CHECK((*entered)[0] == 1);
    CHECK((*entered)[5] == 6);
    CheckTruncated(counts, Reader::ReadEntryCounts);
    const std::array<std::uint8_t, 5> points{0xC1, 5, 0xBF, 6, 20};
    CHECK(Reader::ReadSkillPoints(points) == 20);
    CheckTruncated(points, Reader::ReadSkillPoints);
    const std::array<std::uint8_t, 6> phase{0xC1, 6, 0xBF, 9, 2, 3};
    CHECK(Reader::ReadPhase(phase) == 3);
    CheckTruncated(phase, Reader::ReadPhase);
}

TEST_CASE("Illusion Temple generated sender preserves sixteen-bit target ABI [network][illusion_temple]")
{
    using Sender = void (PacketFunctions_ClientToServer::*)(std::uint16_t, std::uint16_t, BYTE);
    CHECK((std::is_same_v<decltype(&PacketFunctions_ClientToServer::SendIllusionTempleSkillRequest), Sender>));
}

TEST_CASE("Illusion Temple network updates belong only to the world scene [network][illusion_temple]")
{
    CHECK(UI::CursedTemple::CanApplyNetworkUpdate(MAIN_SCENE));
    for (const auto scene : {SERVER_LIST_SCENE, WEBZEN_SCENE, LOG_IN_SCENE, LOADING_SCENE, CHARACTER_SCENE})
    {
        CAPTURE(scene);
        CHECK_FALSE(UI::CursedTemple::CanApplyNetworkUpdate(scene));
    }
}
