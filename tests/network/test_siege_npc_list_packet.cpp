#include "stdafx.h"
#include <doctest.h>

#include "GameLogic/Events/SenatusInfo.h"
#include "Network/Server/SiegeNpcListPacket.h"
#include "Network/Server/SiegeNpcReplyPacket.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

void ReceiveBCNPCList(const BYTE* ReceiveBuffer, int Size);
void ReceiveBCNPCBuy(const BYTE* ReceiveBuffer, int Size);
void ReceiveBCNPCRepair(const BYTE* ReceiveBuffer, int Size);
void ReceiveBCNPCUpgrade(const BYTE* ReceiveBuffer, int Size);

namespace
{
constexpr std::size_t HeaderSize = 9;
constexpr std::size_t EntrySize = 27;
constexpr int GateNumber = 277;
constexpr int StatueNumber = 283;
constexpr int GateMaxHp = 1900000;
constexpr int StatueMaxHp = 1500000;

class SenateScope
{
public:
    SenateScope() : m_previous(g_SenatusInfo) { g_SenatusInfo = CSenatusInfo{}; }
    ~SenateScope() { g_SenatusInfo = m_previous; }

private:
    CSenatusInfo m_previous;
};

void WriteLe32(std::vector<BYTE>& packet, std::size_t offset, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < sizeof(value); ++byte)
        packet[offset + byte] = static_cast<BYTE>(value >> (byte * 8));
}

std::vector<BYTE> NpcList(int number, std::size_t count)
{
    const auto size = HeaderSize + count * EntrySize;
    std::vector<BYTE> packet(size);
    packet[0] = 0xC2;
    packet[1] = static_cast<BYTE>(size >> 8);
    packet[2] = static_cast<BYTE>(size);
    packet[3] = 0xB3;
    packet[4] = 1;
    WriteLe32(packet, 5, static_cast<std::uint32_t>(count));
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto offset = HeaderSize + i * EntrySize;
        const int maxHp = number == GateNumber ? GateMaxHp : StatueMaxHp;
        WriteLe32(packet, offset, number);
        WriteLe32(packet, offset + 4, static_cast<std::uint32_t>(i + 1));
        WriteLe32(packet, offset + 8, static_cast<std::uint32_t>(i % 3));
        WriteLe32(packet, offset + 12, static_cast<std::uint32_t>(i % 2));
        WriteLe32(packet, offset + 16, maxHp);
        WriteLe32(packet, offset + 20, i % 2 ? maxHp - static_cast<int>(i) : 0);
        packet[offset + 24] = static_cast<BYTE>(100 + i);
        packet[offset + 25] = static_cast<BYTE>(150 + i);
        packet[offset + 26] = static_cast<BYTE>(i % 2);
    }
    return packet;
}

auto Fields(const PMSG_NPCDBLIST& npc)
{
    return std::tuple(npc.iNpcNumber, npc.iNpcIndex, npc.iNpcDfLevel, npc.iNpcRgLevel,
                      npc.iNpcMaxHp, npc.iNpcHp, npc.btNpcX, npc.btNpcY, npc.btNpcLive);
}

std::array<PMSG_NPCDBLIST, 10> Snapshot()
{
    std::array<PMSG_NPCDBLIST, 10> snapshot;
    for (int i = 0; i < 6; ++i)
        snapshot[i] = g_SenatusInfo.GetGateInfo(i);
    for (int i = 0; i < 4; ++i)
        snapshot[6 + i] = g_SenatusInfo.GetStatueInfo(i);
    return snapshot;
}

void CheckUnchanged(const std::array<PMSG_NPCDBLIST, 10>& previous)
{
    const auto current = Snapshot();
    for (std::size_t i = 0; i < current.size(); ++i)
    {
        CAPTURE(i);
        CHECK(Fields(current[i]) == Fields(previous[i]));
    }
}

std::vector<BYTE> NpcReply(BYTE subcode, int number, int index, int first = 0, int second = 0)
{
    const std::size_t size = subcode == 0x05 ? 13 : 21;
    // Keep the legacy native read in allocated storage during RED; the wire length remains 13/21.
    constexpr std::size_t backingSize = 24;
    std::vector<BYTE> packet(backingSize);
    packet[0] = 0xC1;
    packet[1] = static_cast<BYTE>(size);
    packet[2] = 0xB2;
    packet[3] = subcode;
    packet[4] = 1;
    WriteLe32(packet, 5, number);
    WriteLe32(packet, 9, index);
    if (subcode != 0x05)
    {
        WriteLe32(packet, 13, first);
        WriteLe32(packet, 17, second);
    }
    return packet;
}

void ReceiveNpcReply(const std::vector<BYTE>& packet, int size)
{
    switch (packet[3])
    {
    case 0x05: ReceiveBCNPCBuy(packet.data(), size); break;
    case 0x06: ReceiveBCNPCRepair(packet.data(), size); break;
    case 0x07: ReceiveBCNPCUpgrade(packet.data(), size); break;
    }
}

void ReceiveNpcReply(const std::vector<BYTE>& packet)
{
    ReceiveNpcReply(packet, packet[1]);
}
} // namespace

TEST_CASE("Castle Siege NPC list updates all six gates and the real buy quote [network][siege-npc]")
{
    SenateScope scope;
    const auto packet = NpcList(GateNumber, 6);
    ReceiveBCNPCList(packet.data(), static_cast<int>(packet.size()));

    for (int i = 0; i < 6; ++i)
    {
        CAPTURE(i);
        const auto& gate = g_SenatusInfo.GetGateInfo(i);
        CHECK(gate.iNpcNumber == GateNumber);
        CHECK(gate.iNpcIndex == i + 1);
        CHECK(gate.iNpcDfLevel == i % 3);
        CHECK(gate.iNpcRgLevel == i % 2);
        CHECK(gate.iNpcMaxHp == GateMaxHp);
        CHECK(gate.iNpcHp == (i % 2 ? GateMaxHp - i : 0));
        CHECK(gate.btNpcX == 100 + i);
        CHECK(gate.btNpcY == 150 + i);
        CHECK(gate.btNpcLive == i % 2); // Gates 2, 4 and 6 are alive.
    }
    g_SenatusInfo.SetCurrGate(0);
    CHECK(g_SenatusInfo.GetRepairCost(&g_SenatusInfo.GetCurrGateInfo()) == 9500000);
}

TEST_CASE("Castle Siege NPC list updates all four statues [network][siege-npc]")
{
    SenateScope scope;
    const auto packet = NpcList(StatueNumber, 4);
    ReceiveBCNPCList(packet.data(), static_cast<int>(packet.size()));

    for (int i = 0; i < 4; ++i)
    {
        CAPTURE(i);
        const auto& statue = g_SenatusInfo.GetStatueInfo(i);
        CHECK(statue.iNpcNumber == StatueNumber);
        CHECK(statue.iNpcIndex == i + 1);
        CHECK(statue.iNpcDfLevel == i % 3);
        CHECK(statue.iNpcRgLevel == i % 2);
        CHECK(statue.iNpcMaxHp == StatueMaxHp);
        CHECK(statue.iNpcHp == (i % 2 ? StatueMaxHp - i : 0));
        CHECK(statue.btNpcX == 100 + i);
        CHECK(statue.btNpcY == 150 + i);
        CHECK(statue.btNpcLive == i % 2);
    }
}

TEST_CASE("Castle Siege NPC list rejects malformed frames without partial updates [network][siege-npc]")
{
    SenateScope scope;
    for (int i = 0; i < 6; ++i)
        g_SenatusInfo.GetGateInfo(i).iNpcHp = 123 + i;
    for (int i = 0; i < 4; ++i)
        g_SenatusInfo.GetStatueInfo(i).iNpcHp = 456 + i;
    const auto previous = Snapshot();
    const auto packet = NpcList(GateNumber, 6);

    // The allocation stays complete even when the logical received frame is short.
    for (int size = -1; size < static_cast<int>(packet.size()); ++size)
    {
        CAPTURE(size);
        ReceiveBCNPCList(packet.data(), size);
        CheckUnchanged(previous);
    }
    for (const auto [offset, value] : std::array<std::pair<std::size_t, BYTE>, 7>{
             {{0, 0xC1}, {3, 0xB4}, {1, 1}, {2, 8}, {2, 170}, {5, 5}, {5, 7}}})
    {
        auto malformed = packet;
        malformed[offset] = value;
        ReceiveBCNPCList(malformed.data(), static_cast<int>(malformed.size()));
        CheckUnchanged(previous);
    }
    auto oversizedCount = packet;
    WriteLe32(oversizedCount, 5, UINT32_MAX);
    ReceiveBCNPCList(oversizedCount.data(), static_cast<int>(oversizedCount.size()));
    CheckUnchanged(previous);
}

TEST_CASE("Castle Siege NPC list bounds the declared frame and keeps empty results [network][siege-npc]")
{
    for (const BYTE result : {BYTE(0), BYTE(1), BYTE(2)})
    {
        const std::array<BYTE, HeaderSize> packet{0xC2, 0, HeaderSize, 0xB3, result, 0, 0, 0, 0};
        const auto parsed = Network::Packets::ReadSiegeNpcList(packet);
        REQUIRE(parsed.has_value());
        CHECK(parsed->result == result);
        CHECK(parsed->entries.empty());
        for (std::size_t size = 0; size < packet.size(); ++size)
            CHECK_FALSE(Network::Packets::ReadSiegeNpcList(std::span(packet).first(size)));
    }

    SenateScope scope;
    auto packet = NpcList(GateNumber, 6);
    const auto declaredSize = packet.size();
    packet.resize(packet.size() + EntrySize, 0xFF); // A dirty receive-buffer tail is not another NPC.
    const auto parsed = Network::Packets::ReadSiegeNpcList(packet);
    REQUIRE(parsed.has_value());
    CHECK(parsed->entries.size() == declaredSize - HeaderSize);
    ReceiveBCNPCList(packet.data(), static_cast<int>(packet.size()));
    CHECK(g_SenatusInfo.GetGateInfo(5).iNpcIndex == 6);
    CHECK(g_SenatusInfo.GetGateInfo(5).btNpcLive == 1);
}

TEST_CASE("Castle Siege NPC buy response uses the real C1 B2 wire fields [network][siege-npc]")
{
    SenateScope scope;
    INFO("Legacy buy size=", sizeof(PMSG_ANS_NPCBUY), ", NPC number offset=", offsetof(PMSG_ANS_NPCBUY, iNpcNumber));
    for (const int number : {GateNumber, StatueNumber})
    {
        ReceiveNpcReply(NpcReply(0x05, number, 1));
        const auto* npc = g_SenatusInfo.GetNPCInfo(number, 1);
        CHECK(npc->iNpcNumber == number);
        CHECK(npc->iNpcIndex == 1);
        CHECK(npc->btNpcLive == 1);
        CHECK(npc->iNpcHp == (number == GateNumber ? GateMaxHp : StatueMaxHp));
        CHECK(npc->iNpcMaxHp == npc->iNpcHp);
    }
}

TEST_CASE("Castle Siege NPC repair response uses the real C1 B2 wire fields [network][siege-npc]")
{
    SenateScope scope;
    INFO("Legacy repair size=", sizeof(PMSG_ANS_NPCREPAIR), ", NPC number offset=", offsetof(PMSG_ANS_NPCREPAIR, iNpcNumber));
    for (const int number : {GateNumber, StatueNumber})
    {
        const auto* npc = g_SenatusInfo.GetNPCInfo(number, 1);
        ReceiveNpcReply(NpcReply(0x06, number, 1, 1234567, 2500000));
        CHECK(npc->iNpcHp == 1234567);
        CHECK(npc->iNpcMaxHp == 2500000);
    }
}

TEST_CASE("Castle Siege NPC upgrade response uses the real C1 B2 wire fields [network][siege-npc]")
{
    SenateScope scope;
    INFO("Legacy upgrade size=", sizeof(PMSG_ANS_NPCUPGRADE), ", NPC number offset=", offsetof(PMSG_ANS_NPCUPGRADE, iNpcNumber));
    for (const int number : {GateNumber, StatueNumber})
    {
        const auto* npc = g_SenatusInfo.GetNPCInfo(number, 1);
        ReceiveNpcReply(NpcReply(0x07, number, 1, 1, 2));
        CHECK(npc->iNpcDfLevel == 2);
        ReceiveNpcReply(NpcReply(0x07, number, 1, 2, 3));
        CHECK(npc->iNpcRgLevel == 3);
        ReceiveNpcReply(NpcReply(0x07, number, 1, 3, 2500000));
        CHECK(npc->iNpcHp == 2500000);
        CHECK(npc->iNpcMaxHp == 2500000);
    }
}

TEST_CASE("Castle Siege NPC replies reject truncation and invalid identities without mutation [network][siege-npc]")
{
    SenateScope scope;
    for (int i = 0; i < 6; ++i)
        g_SenatusInfo.GetGateInfo(i).iNpcHp = 123 + i;
    for (int i = 0; i < 4; ++i)
        g_SenatusInfo.GetStatueInfo(i).iNpcHp = 456 + i;
    const auto previous = Snapshot();
    for (const BYTE subcode : {BYTE(0x05), BYTE(0x06), BYTE(0x07)})
    {
        CAPTURE(subcode);
        const auto packet = NpcReply(subcode, GateNumber, 1, 1, 2500000);
        for (int size = -1; size < packet[1]; ++size)
        {
            CAPTURE(size);
            ReceiveNpcReply(packet, size);
            CheckUnchanged(previous);
        }
        for (const auto [number, index] : std::array<std::pair<int, int>, 7>{
                 {{0, 1}, {GateNumber, 0}, {GateNumber, -1}, {GateNumber, 7},
                  {StatueNumber, 0}, {StatueNumber, -1}, {StatueNumber, 5}}})
        {
            ReceiveNpcReply(NpcReply(subcode, number, index, 1, 2500000));
            CheckUnchanged(previous);
        }
    }
}

TEST_CASE("Castle Siege NPC reply framing preserves status codes without exposing short fields [network][siege-npc]")
{
    for (const BYTE subcode : {BYTE(0x05), BYTE(0x06), BYTE(0x07)})
    {
        auto packet = NpcReply(subcode, GateNumber, 1, 1, 2500000);
        for (BYTE result = 0; result <= 7; ++result)
        {
            packet[4] = result;
            const auto parsed = Network::Packets::ReadSiegeNpcReply(packet, subcode);
            REQUIRE(parsed.has_value());
            CHECK(parsed->result == result);
        }
        for (const auto [offset, value] : std::array<std::pair<std::size_t, BYTE>, 4>{
                 {{0, 0xC2}, {1, 4}, {2, 0xB3}, {3, 0xFF}}})
        {
            auto malformed = packet;
            malformed[offset] = value;
            CHECK_FALSE(Network::Packets::ReadSiegeNpcReply(malformed, subcode));
        }
    }
}
