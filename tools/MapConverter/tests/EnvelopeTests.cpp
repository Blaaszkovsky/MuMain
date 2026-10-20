#include "MapEnvelope.h"
#include <3way.h>
#include <cast.h>
#include <gost.h>
#include <idea.h>
#include <mars.h>
#include <rc5.h>
#include <rc6.h>
#include <tea.h>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
    constexpr std::string_view MasterKey = "webzen#@!01webzen#@!01webzen#@!0";
    constexpr std::size_t KeyBytes = 32;
    constexpr std::size_t KeyOffset = 2;
    constexpr std::size_t WindowBytes = 1024;

    template<class Algorithm>
    std::unique_ptr<CryptoPP::BlockCipher> MakeEncryptor(const std::uint8_t* key)
    {
        auto cipher = std::make_unique<typename Algorithm::Encryption>();
        cipher->SetKey(key, Algorithm::DEFAULT_KEYLENGTH);
        return cipher;
    }

    std::unique_ptr<CryptoPP::BlockCipher> Encryptor(unsigned algorithm, const std::uint8_t* key)
    {
        switch (algorithm)
        {
        case 0: return MakeEncryptor<CryptoPP::TEA>(key);
        case 1: return MakeEncryptor<CryptoPP::ThreeWay>(key);
        case 2: return MakeEncryptor<CryptoPP::CAST128>(key);
        case 3: return MakeEncryptor<CryptoPP::RC5>(key);
        case 4: return MakeEncryptor<CryptoPP::RC6>(key);
        case 5: return MakeEncryptor<CryptoPP::MARS>(key);
        case 6: return MakeEncryptor<CryptoPP::IDEA>(key);
        case 7: return MakeEncryptor<CryptoPP::GOST>(key);
        default: throw std::runtime_error("test algorithm");
        }
    }

    void EncryptRange(std::vector<std::uint8_t>& bytes, CryptoPP::BlockCipher& cipher,
                      std::size_t offset, std::size_t count)
    {
        for (auto index = offset; index < offset + count; index += cipher.BlockSize())
            cipher.ProcessBlock(bytes.data() + index);
    }

    std::vector<std::uint8_t> Fixture(const std::vector<std::uint8_t>& payload, unsigned outerId, unsigned innerId)
    {
        std::array<std::uint8_t, KeyBytes> key{};
        for (std::size_t index = 0; index < key.size(); ++index)
            key[index] = static_cast<std::uint8_t>(index * 11 + innerId);
        std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(innerId), static_cast<std::uint8_t>(outerId)};
        bytes.insert(bytes.end(), key.begin(), key.end());
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        auto inner = Encryptor(innerId, key.data());
        EncryptRange(bytes, *inner, Tools::Maps::EnvelopeBytes, payload.size() - payload.size() % inner->BlockSize());
        auto outer = Encryptor(outerId, reinterpret_cast<const std::uint8_t*>(MasterKey.data()));
        const auto window = WindowBytes - WindowBytes % outer->BlockSize();
        if (payload.size() > window)
        {
            EncryptRange(bytes, *outer, KeyOffset, window);
            EncryptRange(bytes, *outer, bytes.size() - window, window);
        }
        if (payload.size() > window * 4)
            EncryptRange(bytes, *outer, KeyOffset + payload.size() / 2, window);
        return bytes;
    }

    void CheckInvalidSize(std::size_t size)
    {
        try
        {
            Tools::Maps::DecodeEnvelope(std::vector<std::uint8_t>(size));
        }
        catch (const std::runtime_error&)
        {
            return;
        }
        throw std::runtime_error("decoder accepted an invalid size");
    }
}

int main()
{
    try
    {
        constexpr std::array<std::size_t, 13> Sizes{0, 1, 1019, 1020, 1021, 1024, 1025,
                                                   4080, 4081, 4097, 65540, 131076, 196610};
        std::size_t cases = 0;
        for (unsigned outer = 0; outer < 8; ++outer)
            for (unsigned inner = 0; inner < 8; ++inner)
                for (const auto size : Sizes)
                {
                    std::vector<std::uint8_t> payload(size);
                    for (std::size_t index = 0; index < size; ++index)
                        payload[index] = static_cast<std::uint8_t>(index * 19 + size);
                    if (Tools::Maps::DecodeEnvelope(Fixture(payload, outer, inner)) != payload)
                        throw std::runtime_error("cipher envelope roundtrip mismatch");
                    ++cases;
                }
        CheckInvalidSize(Tools::Maps::EnvelopeBytes - 1);
        CheckInvalidSize(Tools::Maps::MaxPayloadBytes + Tools::Maps::EnvelopeBytes + 1);
        std::cout << cases << " cipher/size combinations passed; invalid sizes rejected\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
