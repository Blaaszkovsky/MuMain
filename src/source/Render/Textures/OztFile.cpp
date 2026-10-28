#include "OztFile.h"
#include "TextureFile.h"

namespace Render::Textures::Ozt
{
namespace
{
// The TGA header starts after the 4 bytes in front.
constexpr std::size_t TgaHeaderOffset = 4;
constexpr std::size_t WidthOffset = TgaHeaderOffset + 12;
constexpr std::size_t HeightOffset = TgaHeaderOffset + 14;
constexpr std::size_t DepthOffset = TgaHeaderOffset + 16;

constexpr int ReadableBitsPerPixel = 32;

// TGA numbers are little endian.
int ReadUInt16(std::span<const std::uint8_t> file, std::size_t offset)
{
    return file[offset] | (file[offset + 1] << 8);
}

bool IsReadableFormat(const Header& header, int maxWidth, int maxHeight)
{
    return header.bitsPerPixel == ReadableBitsPerPixel && header.width > 0 && header.height > 0 &&
           header.width <= MaxUpscaledTextureDimension && header.height <= MaxUpscaledTextureDimension &&
           header.width <= maxWidth && header.height <= maxHeight;
}

bool HasReadableLayout(std::span<const std::uint8_t> file)
{
    constexpr std::size_t ImageIdOffset = TgaHeaderOffset;
    constexpr std::size_t ColorMapOffset = TgaHeaderOffset + 1;
    constexpr std::size_t ImageTypeOffset = TgaHeaderOffset + 2;
    constexpr std::size_t DescriptorOffset = TgaHeaderOffset + 17;
    constexpr std::uint8_t UncompressedTrueColor = 2;
    constexpr std::uint8_t EightAlphaBits = 8;
    return file[ImageIdOffset] == 0 && file[ColorMapOffset] == 0 &&
           file[ImageTypeOffset] == UncompressedTrueColor &&
           (file[DescriptorOffset] == 0 || file[DescriptorOffset] == EightAlphaBits);
}

bool CanCopyPixels(std::span<const std::uint8_t> file, const Header& header,
                   std::span<std::uint8_t> texture, int textureWidth)
{
    const Header checked = ReadHeader(file, MaxUpscaledTextureDimension, MaxUpscaledTextureDimension);
    if (header.status != Status::Valid || checked.status != Status::Valid ||
        header.width != checked.width || header.height != checked.height ||
        header.bitsPerPixel != checked.bitsPerPixel || textureWidth < header.width ||
        textureWidth > MaxUpscaledTextureDimension)
    {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(textureWidth) * BytesPerPixel;
    return texture.size() / rowBytes >= static_cast<std::size_t>(header.height);
}
} // namespace

Header ReadHeader(std::span<const std::uint8_t> file, int maxWidth, int maxHeight)
{
    Header header;
    if (file.size() < HeaderSize)
    {
        header.status = Status::TooSmall;
        return header;
    }

    header.width = ReadUInt16(file, WidthOffset);
    header.height = ReadUInt16(file, HeightOffset);
    header.bitsPerPixel = file[DepthOffset];
    if (file.size() > MaxTextureFileBytes || !IsReadableFormat(header, maxWidth, maxHeight) ||
        !HasReadableLayout(file))
    {
        header.status = Status::InvalidFormat;
        return header;
    }

    // The pixels follow the header directly; like the original client, the
    // reader skips no image ID field. No shipped file has one.
    const std::size_t pixelBytes =
        static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height) * BytesPerPixel;
    if (file.size() - HeaderSize < pixelBytes)
    {
        header.status = Status::TooSmall;
        return header;
    }

    header.status = Status::Valid;
    return header;
}

void CopyPixels(std::span<const std::uint8_t> file, const Header& header, std::span<std::uint8_t> texture,
                int textureWidth)
{
    if (!CanCopyPixels(file, header, texture, textureWidth))
    {
        return;
    }

    const std::size_t textureRowBytes = static_cast<std::size_t>(textureWidth) * BytesPerPixel;
    const std::uint8_t* source = file.data() + HeaderSize;
    for (int y = 0; y < header.height; ++y)
    {
        // The file's rows go from the bottom up, the texture's from the top down.
        const auto textureRow = static_cast<std::size_t>(header.height - 1 - y);
        std::uint8_t* target = texture.data() + textureRow * textureRowBytes;
        for (int x = 0; x < header.width; ++x)
        {
            // The file stores BGRA.
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            target[3] = source[3];
            source += BytesPerPixel;
            target += BytesPerPixel;
        }
    }
}
} // namespace Render::Textures::Ozt
