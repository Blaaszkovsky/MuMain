#include <doctest.h>
#include "Render/Models/BmdModelFile.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;
template <class T> void Append(Bytes& data, T value)
{
    const auto start = data.size();
    data.resize(start + sizeof(T));
    std::memcpy(data.data() + start, &value, sizeof(T));
}
Bytes MinimalBody()
{
    Bytes body(32, 0);
    Append<std::int16_t>(body, 0);
    Append<std::int16_t>(body, 1);
    Append<std::int16_t>(body, 1);
    Append<std::int16_t>(body, 1);
    body.push_back(0);
    body.push_back(1);
    return body;
}
Bytes Encode(const Bytes& body, int version)
{
    Bytes file{'B', 'M', 'D', static_cast<std::uint8_t>(version)};
    if (version == 10)
    {
        file.insert(file.end(), body.begin(), body.end());
        return file;
    }
    Append<std::int32_t>(file, static_cast<std::int32_t>(body.size()));
    constexpr std::array<std::uint8_t, 16> key = {0xd1, 0x73, 0x52, 0xf6, 0xd2, 0x9a, 0xcb, 0x27,
                                                  0x3e, 0xaf, 0x59, 0x31, 0x37, 0xb3, 0xe7, 0xa2};
    std::uint8_t state = 0x5e;
    for (std::size_t i = 0; i < body.size(); ++i)
    {
        const auto encrypted = static_cast<std::uint8_t>(body[i] + state) ^ key[i % key.size()];
        file.push_back(static_cast<std::uint8_t>(encrypted));
        state = static_cast<std::uint8_t>(encrypted + 0x3d);
    }
    return file;
}
} // namespace
TEST_CASE("BMD v10 and v12 preserve their complete model bodies")
{
    const auto body = MinimalBody();
    for (const auto version : {10, 12})
    {
        Bytes decoded;
        CHECK(Render::Models::BmdModelFile::Decode(Encode(body, version), decoded));
        CHECK(decoded == body);
    }
}
TEST_CASE("BMD rejects every truncation and invalid envelope without stale output")
{
    for (const auto version : {10, 12})
    {
        const auto file = Encode(MinimalBody(), version);
        for (std::size_t size = 0; size < file.size(); ++size)
        {
            Bytes decoded{1};
            CHECK_FALSE(Render::Models::BmdModelFile::Decode(std::span(file).first(size), decoded));
            CHECK(decoded.empty());
        }
        auto trailing = file;
        trailing.push_back(0);
        Bytes decoded;
        CHECK_FALSE(Render::Models::BmdModelFile::Decode(trailing, decoded));
    }
    for (std::size_t size = 0; size < 80; ++size)
    {
        Bytes file(size, 0), decoded{1};
        if (size >= 8)
        {
            file[0] = 'B';
            file[1] = 'M';
            file[2] = 'D';
            file[3] = 14;
            const std::int32_t payload = static_cast<std::int32_t>(size - 8);
            std::memcpy(file.data() + 4, &payload, sizeof(payload));
        }
        CHECK_FALSE(Render::Models::BmdModelFile::Decode(file, decoded));
        CHECK(decoded.empty());
    }
}
TEST_CASE("BMD validates signed counts boolean flags and parent cycles")
{
    using Render::Models::BmdModelFile::ValidateBody;
    const auto valid = MinimalBody();
    for (const auto offset : {32, 34, 36})
    {
        auto body = valid;
        body[offset] = 255;
        body[offset + 1] = 255;
        CHECK_FALSE(ValidateBody(body));
    }
    auto body = valid;
    body[36] = 0;
    CHECK_FALSE(ValidateBody(body));
    body = valid;
    body[40] = 2;
    CHECK_FALSE(ValidateBody(body));
    body = valid;
    body[41] = 2;
    CHECK_FALSE(ValidateBody(body));
    body = valid;
    body[41] = 0;
    body.resize(74, 0);
    Append<std::int16_t>(body, 0);
    CHECK_FALSE(ValidateBody(body));
}

TEST_CASE("BMD rejects empty animation keys and negative mesh bone nodes")
{
    using Render::Models::BmdModelFile::ValidateBody;
    auto body = MinimalBody();
    body[38] = 0;
    CHECK_FALSE(ValidateBody(body));
    body = MinimalBody();
    // Insert one mesh containing one vertex; the node is at byte 48.
    body[32] = 1;
    std::vector<std::uint8_t> mesh;
    for (const auto count : {1, 0, 0, 0, 0})
        Append<std::int16_t>(mesh, static_cast<std::int16_t>(count));
    Append<std::int16_t>(mesh, -1);
    mesh.resize(26 + 32, 0);
    body.insert(body.begin() + 38, mesh.begin(), mesh.end());
    CHECK_FALSE(ValidateBody(body));
    body[48] = 0;
    body[49] = 0;
    CHECK(ValidateBody(body));
}

TEST_CASE("BMD rejects non-finite vertices unterminated textures and negative normal nodes")
{
    using Render::Models::BmdModelFile::ValidateBody;
    auto body = MinimalBody();
    body[32] = 1;
    std::vector<std::uint8_t> mesh;
    for (const auto count : {1, 1, 0, 0, 0})
        Append<std::int16_t>(mesh, static_cast<std::int16_t>(count));
    mesh.resize(10 + 16 + 20 + 32, 0);
    body.insert(body.begin() + 38, mesh.begin(), mesh.end());
    CHECK(ValidateBody(body));
    auto corrupt = body;
    corrupt[64] = 255;
    corrupt[65] = 255;
    CHECK_FALSE(ValidateBody(corrupt));
    corrupt = body;
    const float nonfinite = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(corrupt.data() + 52, &nonfinite, sizeof(nonfinite));
    CHECK_FALSE(ValidateBody(corrupt));
    corrupt = body;
    std::fill_n(corrupt.begin() + 84, 32, 'x');
    CHECK_FALSE(ValidateBody(corrupt));
    corrupt = body;
    corrupt[46] = 1;
    CHECK_FALSE(ValidateBody(corrupt));
}
