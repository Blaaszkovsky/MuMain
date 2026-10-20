#include "MapEnvelope.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace
{
    constexpr std::size_t SignatureBytes = 4;
    constexpr std::size_t ChunkBytes = 4096;

    std::vector<std::uint8_t> ReadInput()
    {
        std::vector<std::uint8_t> bytes;
        std::array<char, ChunkBytes> chunk{};
        while (std::cin.read(chunk.data(), chunk.size()) || std::cin.gcount())
        {
            const auto count = static_cast<std::size_t>(std::cin.gcount());
            if (bytes.size() + count > Tools::Maps::MaxPayloadBytes + Tools::Maps::EnvelopeBytes + SignatureBytes)
                throw std::runtime_error("input exceeds map size limit");
            bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
        }
        if (!std::cin.eof()) throw std::runtime_error("failed reading input");
        return bytes;
    }
}

int main()
{
    try
    {
#ifdef _WIN32
        if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
            throw std::runtime_error("cannot set binary stream mode");
#endif
        const auto source = ReadInput();
        if (source.size() < SignatureBytes) throw std::runtime_error("missing signature");
        const std::string_view signature(reinterpret_cast<const char*>(source.data()), SignatureBytes);
        if (signature != std::string_view("ATT\1", SignatureBytes) && signature != std::string_view("MAP\1", SignatureBytes))
            throw std::runtime_error("expected ATT/MAP version 1 envelope");
        const auto payload = Tools::Maps::DecodeEnvelope(std::span(source).subspan(SignatureBytes));
        std::cout.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        if (!std::cout) throw std::runtime_error("failed writing output");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
