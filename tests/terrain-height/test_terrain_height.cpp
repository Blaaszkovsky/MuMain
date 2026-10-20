#include "Render/Terrain/TerrainHeightSamples.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr std::size_t Side = 256;
    constexpr std::size_t Cells = Side * Side;
    constexpr std::size_t PackedPixelsOffset = 4 + 14 + 40;
    constexpr std::size_t LegacyPixelsOffset = 4 + 1080;
    constexpr std::size_t PackedSampleBytes = 3;
    constexpr float LegacyFactor = 1.5f;
    constexpr float LoginSceneFactor = 3.f;
    constexpr std::uint32_t Crc32Polynomial = 0xEDB88320;
    constexpr std::uint32_t Crc32Initial = 0xFFFFFFFF;
    constexpr std::uint32_t FereaFloatCrc32 = 0x6FB478A8;
    constexpr std::uint32_t LorenciaFloatCrc32 = 0xDB0B4577;
    constexpr std::uint32_t LoginFloatCrc32 = 0x9032C81F;
    constexpr std::array<std::size_t, 12> GoldenIndices{0, 1, 255, 256, 512, 1023, 4096, 8191, 16384, 32767, 49152, 65535};
    constexpr std::array<float, 12> FereaGolden{325, 325, 325, 325, 169, 169, 169, 169, 209, 169, 630, 325};
    constexpr std::array<float, 12> LorenciaGolden{22.5f, 22.5f, 22.5f, 22.5f, 22.5f, 22.5f,
                                                  306, 292.5f, 297, 363, 259.5f, 0};

    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    std::vector<unsigned char> ReadExact(const char* path, std::size_t expected)
    {
        std::ifstream stream(path, std::ios::binary);
        Require(static_cast<bool>(stream), "height input not found");
        std::vector<unsigned char> bytes(expected + 1);
        stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
        Require(static_cast<std::size_t>(stream.gcount()) == expected, "height input size differs from fixture");
        bytes.resize(expected);
        return bytes;
    }

    std::uint32_t HeightCrc32(const std::vector<float>& heights)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        auto crc = Crc32Initial;
        for (const auto value : heights)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                crc ^= (bits >> shift) & 0xFF;
                for (unsigned bit = 0; bit < 8; ++bit)
                    crc = (crc >> 1) ^ ((crc & 1) ? Crc32Polynomial : 0);
            }
        }
        return crc ^ Crc32Initial;
    }

    void CheckSelectionAndSamples()
    {
        for (int map = -1; map <= 255; ++map)
        {
            Require(Render::Terrain::Height::UsesPackedHeight(map, true), "existing packed map lost its selection");
            Require(Render::Terrain::Height::UsesPackedHeight(map, false) == (map == 112), "unexpected map selection change");
        }
        constexpr std::array<unsigned char, 3> Minimum{0, 0, 0};
        constexpr std::array<unsigned char, 3> EndianProbe{1, 2, 3};
        constexpr std::array<unsigned char, 3> Maximum{255, 255, 255};
        Require(Render::Terrain::Height::DecodePackedSample(Minimum.data(), -500) == -500, "minimum sample");
        Require(Render::Terrain::Height::DecodePackedSample(EndianProbe.data(), -500) == 65551, "packed byte order");
        Require(Render::Terrain::Height::DecodePackedSample(Maximum.data(), -500) == 16776715, "maximum sample");
        Require(Render::Terrain::Height::DecodeLegacySample(255, LegacyFactor) == 382.5f, "legacy factor changed");
        Require(Render::Terrain::Height::DecodeLegacySample(255, LoginSceneFactor) == 765.f, "login scene factor changed");
    }

    void CheckFerea(const char* path)
    {
        const auto bytes = ReadExact(path, PackedPixelsOffset + Cells * PackedSampleBytes);
        std::vector<float> heights(Cells);
        for (std::size_t index = 0; index < Cells; ++index)
            heights[index] = Render::Terrain::Height::DecodePackedSample(
                bytes.data() + PackedPixelsOffset + index * PackedSampleBytes, Render::Terrain::Height::PackedHeightOffset);
        for (std::size_t index = 0; index < GoldenIndices.size(); ++index)
            Require(heights[GoldenIndices[index]] == FereaGolden[index], "Ferea golden sample mismatch");
        Require(*std::min_element(heights.begin(), heights.end()) == -281, "Ferea minimum mismatch");
        Require(*std::max_element(heights.begin(), heights.end()) == 1000, "Ferea maximum mismatch");
        Require(std::count_if(heights.begin(), heights.end(), [](float value) { return value < 0; }) == 757,
                "Ferea negative height count mismatch");
        Require(HeightCrc32(heights) == FereaFloatCrc32, "Ferea full-sample reference mismatch");
    }

    void CheckLorencia(const char* path)
    {
        const auto bytes = ReadExact(path, LegacyPixelsOffset + Cells);
        std::vector<float> heights(Cells);
        for (std::size_t index = 0; index < Cells; ++index)
            heights[index] = Render::Terrain::Height::DecodeLegacySample(bytes[LegacyPixelsOffset + index], LegacyFactor);
        for (std::size_t index = 0; index < GoldenIndices.size(); ++index)
            Require(heights[GoldenIndices[index]] == LorenciaGolden[index], "Lorencia golden sample mismatch");
        Require(*std::min_element(heights.begin(), heights.end()) == 0, "Lorencia minimum mismatch");
        Require(*std::max_element(heights.begin(), heights.end()) == 382.5f, "Lorencia maximum mismatch");
        Require(HeightCrc32(heights) == LorenciaFloatCrc32, "Lorencia full-sample reference mismatch");
        for (std::size_t index = 0; index < Cells; ++index)
            heights[index] = Render::Terrain::Height::DecodeLegacySample(bytes[LegacyPixelsOffset + index], LoginSceneFactor);
        Require(HeightCrc32(heights) == LoginFloatCrc32, "login-scene factor full-sample mismatch");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 3, "expected Ferea and Lorencia height paths");
        CheckSelectionAndSamples();
        CheckFerea(argv[1]);
        CheckLorencia(argv[2]);
        std::cout << "selection matrix, sample boundaries, 65536 Ferea and 65536 Lorencia heights passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
