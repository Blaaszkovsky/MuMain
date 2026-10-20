#include "LegacyMarsDecryption.h"
#include <cryptopp/misc.h>
#include <array>
#include <bit>

namespace Render::Models
{
namespace
{
using Word = CryptoPP::word32;
constexpr std::size_t StateWords = 15;
constexpr std::size_t KeyWords = 40;
constexpr unsigned ExpansionRounds = 4;
constexpr unsigned KeysPerRound = 10;
constexpr unsigned StirRounds = 4;
constexpr unsigned RotationMask = 31;
constexpr unsigned SboxBase = 265;
constexpr Word TopBit = 0x80000000U;

constexpr unsigned WordBits = 32;
constexpr unsigned MinimumRunBits = 10;
constexpr Word AllowedMaskBits = 0xfffffffcU;

// Independently expressed as runs of equal bits, checked against 11,056
// oracle cases. Mark the interior of runs >=10 bits; a top zero run also
// marks bit 31. This is the key-mask interoperability difference.
Word LegacyMask(Word word)
{
    Word mask = 0;
    for (unsigned start = 0; start < WordBits;)
    {
        unsigned end = start + 1;
        const Word value = (word >> start) & 1U;
        while (end < WordBits && ((word >> end) & 1U) == value)
            ++end;
        if (end - start >= MinimumRunBits)
        {
            for (unsigned bit = start + 1; bit + 1 < end; ++bit)
                mask |= Word{1} << bit;
            if (end == WordBits && value == 0)
                mask |= TopBit;
        }
        start = end;
    }
    return mask & AllowedMaskBits;
}

// Standard MARS expansion before its multiplication-word tweak. Based on
// Crypto++ mars.cpp (public domain), not on any game/server implementation.
std::array<Word, KeyWords> ExpandRawKeys(const CryptoPP::byte* key, unsigned length, const Word* sbox)
{
    std::array<Word, StateWords> state{};
    std::array<Word, KeyWords> keys{};
    CryptoPP::GetUserKey(CryptoPP::LITTLE_ENDIAN_ORDER, state.data(), static_cast<unsigned>(state.size()), key, length);
    const auto keyWords = length / static_cast<unsigned>(sizeof(Word));
    state[keyWords] = keyWords;
    for (unsigned round = 0; round < ExpansionRounds; ++round)
    {
        for (unsigned i = 0; i < StateWords; ++i)
            state[i] ^= std::rotl(state[(i + 8) % StateWords] ^ state[(i + 13) % StateWords], 3) ^ (4 * i + round);
        for (unsigned stir = 0; stir < StirRounds; ++stir)
            for (unsigned i = 0; i < StateWords; ++i)
                state[i] = std::rotl(state[i] + sbox[state[(i + 14) % StateWords] % 512], 9);
        for (unsigned i = 0; i < KeysPerRound; ++i)
            keys[KeysPerRound * round + i] = state[4 * i % StateWords];
    }
    return keys;
}
} // namespace

void LegacyMarsDecryption::UncheckedSetKey(const CryptoPP::byte* key, unsigned length,
                                           const CryptoPP::NameValuePairs& parameters)
{
    CryptoPP::MARS::Decryption::UncheckedSetKey(key, length, parameters);
    const auto raw = ExpandRawKeys(key, length, Sbox);
    // Only odd multiplication words 5..35 are tweaked; all other words and
    // the full upstream decryption routine remain unchanged.
    for (unsigned i = 5; i < 37; i += 2)
    {
        const Word word = raw[i] | 3U;
        const auto rotation = static_cast<int>(raw[i - 1] & RotationMask);
        m_k[i] = word ^ (std::rotl(Sbox[SboxBase + (raw[i] & 3U)], rotation) & LegacyMask(word));
    }
}
} // namespace Render::Models
