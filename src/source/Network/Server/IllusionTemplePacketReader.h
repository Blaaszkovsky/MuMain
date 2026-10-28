#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>

namespace Network::Packets
{
// Original Season 6 C1/BF packets. Views borrow only complete wire records;
// native WORD/int alignment must never determine a network offset or stride.
class IllusionTemplePacketReader
{
public:
    struct PartyMember
    {
        std::uint16_t id;
        std::uint8_t map;
        std::uint8_t x;
        std::uint8_t y;
    };

    struct State
    {
        std::uint16_t remainingSeconds;
        std::uint16_t relicHolder;
        std::uint8_t x;
        std::uint8_t y;
        std::uint8_t alliedScore;
        std::uint8_t illusionScore;
        std::uint8_t team;
        std::span<const std::uint8_t> members;
    };

    struct Result
    {
        std::uint8_t alliedScore;
        std::uint8_t illusionScore;
        std::span<const std::uint8_t> players;
    };

    struct PlayerResult
    {
        std::array<char, 10> name;
        std::uint8_t map;
        std::uint8_t team;
        std::uint8_t playerClass;
        std::int32_t addedExperience;
    };

    struct SkillResult
    {
        std::uint8_t result;
        std::uint16_t skill;
        std::uint16_t source;
        std::uint16_t target;
    };

    struct SkillEnd
    {
        std::uint16_t skill;
        std::uint16_t target;
    };

    static constexpr std::size_t StateHeaderSize = 14;
    static constexpr std::size_t PartyMemberSize = 5;
    static constexpr std::size_t ResultHeaderSize = 7;
    static constexpr std::size_t PlayerResultSize = 17;

    static std::optional<State> ReadState(std::span<const std::uint8_t> packet)
    {
        if (packet.size() < StateHeaderSize)
            return std::nullopt;
        const auto members = ReadEntries(packet, StateHeaderSize, packet[13], PartyMemberSize);
        if (!members)
            return std::nullopt;
        return State{Little16(packet, 4), Little16(packet, 6), packet[8], packet[9], packet[10], packet[11],
                     packet[12], *members};
    }

    static std::optional<PartyMember> ReadPartyMember(std::span<const std::uint8_t> members, std::size_t index)
    {
        if (index >= members.size() / PartyMemberSize)
            return std::nullopt;
        const auto member = members.subspan(index * PartyMemberSize, PartyMemberSize);
        return PartyMember{Little16(member, 0), member[2], member[3], member[4]};
    }

    static std::optional<Result> ReadResult(std::span<const std::uint8_t> packet)
    {
        if (packet.size() < ResultHeaderSize)
            return std::nullopt;
        const auto players = ReadEntries(packet, ResultHeaderSize, packet[6], PlayerResultSize);
        if (!players)
            return std::nullopt;
        return Result{packet[4], packet[5], *players};
    }

    static std::optional<PlayerResult> ReadPlayerResult(std::span<const std::uint8_t> players, std::size_t index)
    {
        if (index >= players.size() / PlayerResultSize)
            return std::nullopt;
        const auto player = players.subspan(index * PlayerResultSize, PlayerResultSize);
        PlayerResult result{};
        std::copy_n(player.begin(), result.name.size(), result.name.begin());
        result.map = player[10];
        result.team = player[11];
        result.playerClass = player[12];
        result.addedExperience = std::bit_cast<std::int32_t>(Little32(player, 13));
        return result;
    }

    static std::optional<SkillResult> ReadSkillResult(std::span<const std::uint8_t> packet)
    {
        constexpr std::size_t wireSize = 11;
        if (packet.size() < wireSize)
            return std::nullopt;
        return SkillResult{packet[4], Big16(packet, 5), Little16(packet, 7), Little16(packet, 9)};
    }

    static std::optional<SkillEnd> ReadSkillEnd(std::span<const std::uint8_t> packet)
    {
        constexpr std::size_t wireSize = 8;
        if (packet.size() < wireSize)
            return std::nullopt;
        return SkillEnd{Big16(packet, 4), Little16(packet, 6)};
    }

    static std::optional<std::uint8_t> ReadEntryResult(std::span<const std::uint8_t> packet)
    {
        return ReadByte(packet, 5, 4);
    }

    static std::optional<std::span<const std::uint8_t, 6>> ReadEntryCounts(std::span<const std::uint8_t> packet)
    {
        constexpr std::size_t wireSize = 10;
        constexpr std::size_t countOffset = 4;
        if (packet.size() < wireSize)
            return std::nullopt;
        return std::span<const std::uint8_t, 6>(packet.data() + countOffset, 6);
    }

    static std::optional<std::uint8_t> ReadSkillPoints(std::span<const std::uint8_t> packet)
    {
        return ReadByte(packet, 5, 4);
    }

    static std::optional<std::uint8_t> ReadPhase(std::span<const std::uint8_t> packet)
    {
        return ReadByte(packet, 6, 5);
    }

private:
    static std::uint16_t Little16(std::span<const std::uint8_t> bytes, std::size_t offset)
    {
        return std::uint16_t(bytes[offset]) | (std::uint16_t(bytes[offset + 1]) << 8);
    }

    static std::uint16_t Big16(std::span<const std::uint8_t> bytes, std::size_t offset)
    {
        return (std::uint16_t(bytes[offset]) << 8) | std::uint16_t(bytes[offset + 1]);
    }

    static std::uint32_t Little32(std::span<const std::uint8_t> bytes, std::size_t offset)
    {
        return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
            | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
    }

    static std::optional<std::span<const std::uint8_t>> ReadEntries(std::span<const std::uint8_t> packet,
        std::size_t header, std::size_t count, std::size_t stride)
    {
        if (count > (packet.size() - header) / stride)
            return std::nullopt;
        return packet.subspan(header, count * stride);
    }

    static std::optional<std::uint8_t> ReadByte(std::span<const std::uint8_t> packet,
        std::size_t wireSize, std::size_t offset)
    {
        if (packet.size() < wireSize)
            return std::nullopt;
        return packet[offset];
    }
};
} // namespace Network::Packets
