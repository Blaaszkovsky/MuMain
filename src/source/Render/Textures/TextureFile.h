#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <vector>

namespace Render::Textures
{
// Covers the imported 2048-square images with at most 16 MiB of RGBA pixels.
inline constexpr int MaxTextureDimension = 2048;
// Allows a full raw image plus metadata while bounding file allocation separately.
inline constexpr std::size_t MaxTextureFileBytes = 32u * 1024u * 1024u;

constexpr bool IsReadableTextureSize(int width, int height)
{
    return width > 0 && height > 0 && width <= MaxTextureDimension && height <= MaxTextureDimension;
}

// Rejects oversized, unseekable and truncated input before it can reach a decoder.
bool ReadTextureFile(std::istream& input, std::vector<std::uint8_t>& bytes);
} // namespace Render::Textures
