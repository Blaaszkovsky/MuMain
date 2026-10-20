#include "TextureFile.h"

namespace Render::Textures
{
bool ReadTextureFile(std::istream& input, std::vector<std::uint8_t>& bytes)
{
    bytes.clear();
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxTextureFileBytes))
    {
        return false;
    }
    input.seekg(0, std::ios::beg);
    if (!input)
    {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()))
    {
        bytes.clear();
        return false;
    }
    return true;
}
} // namespace Render::Textures
