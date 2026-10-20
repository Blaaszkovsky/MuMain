#include "MapEnvelope.h"
#include <3way.h>
#include <cast.h>
#include <gost.h>
#include <idea.h>
#include <mars.h>
#include <rc5.h>
#include <rc6.h>
#include <tea.h>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace Tools::Maps
{
    namespace
    {
        constexpr std::string_view MasterKey = "webzen#@!01webzen#@!01webzen#@!0";
        constexpr std::size_t PartialBytes = 1024;
        constexpr std::size_t MiddleThreshold = 4;
        constexpr std::size_t KeyOffset = 2;
        constexpr unsigned AlgorithmMask = 7;
        static_assert(MasterKey.size() == EnvelopeBytes - KeyOffset);

        template<class Algorithm>
        std::unique_ptr<CryptoPP::BlockCipher> MakeCipher(const std::uint8_t* key)
        {
            auto cipher = std::make_unique<typename Algorithm::Decryption>();
            cipher->SetKey(key, Algorithm::DEFAULT_KEYLENGTH);
            return cipher;
        }

        std::unique_ptr<CryptoPP::BlockCipher> SelectCipher(unsigned algorithm, const std::uint8_t* key)
        {
            switch (algorithm & AlgorithmMask)
            {
            case 0: return MakeCipher<CryptoPP::TEA>(key);
            case 1: return MakeCipher<CryptoPP::ThreeWay>(key);
            case 2: return MakeCipher<CryptoPP::CAST128>(key);
            case 3: return MakeCipher<CryptoPP::RC5>(key);
            case 4: return MakeCipher<CryptoPP::RC6>(key);
            case 5: return MakeCipher<CryptoPP::MARS>(key);
            case 6: return MakeCipher<CryptoPP::IDEA>(key);
            case 7: return MakeCipher<CryptoPP::GOST>(key);
            default: throw std::runtime_error("invalid algorithm");
            }
        }

        void DecryptRange(CryptoPP::BlockCipher& cipher, std::span<std::uint8_t> bytes,
                          std::size_t offset, std::size_t count)
        {
            if (offset > bytes.size() || count > bytes.size() - offset || count % cipher.BlockSize())
                throw std::runtime_error("envelope block outside input");
            for (auto index = offset; index < offset + count; index += cipher.BlockSize())
                cipher.ProcessBlock(bytes.data() + index);
        }

        void RecoverKey(std::span<std::uint8_t> bytes, CryptoPP::BlockCipher& cipher)
        {
            const auto payloadSize = bytes.size() - EnvelopeBytes;
            const auto partialSize = PartialBytes - PartialBytes % cipher.BlockSize();
            if (payloadSize > MiddleThreshold * partialSize)
                DecryptRange(cipher, bytes, KeyOffset + payloadSize / 2, partialSize);
            if (payloadSize > partialSize)
            {
                DecryptRange(cipher, bytes, bytes.size() - partialSize, partialSize);
                DecryptRange(cipher, bytes, KeyOffset, partialSize);
            }
        }
    }

    std::vector<std::uint8_t> DecodeEnvelope(std::span<const std::uint8_t> source)
    {
        if (source.size() < EnvelopeBytes || source.size() > MaxPayloadBytes + EnvelopeBytes)
            throw std::runtime_error("invalid envelope size");
        std::vector<std::uint8_t> bytes(source.begin(), source.end());
        auto outer = SelectCipher(bytes[1], reinterpret_cast<const std::uint8_t*>(MasterKey.data()));
        RecoverKey(bytes, *outer);
        auto inner = SelectCipher(bytes[0], bytes.data() + KeyOffset);
        const auto size = bytes.size() - EnvelopeBytes;
        DecryptRange(*inner, bytes, EnvelopeBytes, size - size % inner->BlockSize());
        bytes.erase(bytes.begin(), bytes.begin() + EnvelopeBytes);
        return bytes;
    }
}
