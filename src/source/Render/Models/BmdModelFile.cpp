#include "BmdModelFile.h"
#include "BmdBodyReader.h"
#include "BmdModulusCipher.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Render::Models::BmdModelFile
{
namespace
{
constexpr int NameBytes = 32;
constexpr int MaxMeshes = 50;
constexpr int MaxBones = 200;
constexpr int MaxActions = 1024;
constexpr int MaxVertices = 15000;
constexpr int MaxSignedCount = 32767;
constexpr int TriangleBytes = 64;
constexpr int TriangleSlots = 4;
constexpr int FileHeaderBytes = 4;
constexpr int EncryptedHeaderBytes = 8;
constexpr std::array<std::uint8_t, 16> MapKey = {0xd1, 0x73, 0x52, 0xf6, 0xd2, 0x9a, 0xcb, 0x27,
                                                 0x3e, 0xaf, 0x59, 0x31, 0x37, 0xb3, 0xe7, 0xa2};

void CheckIndex(int value, int count, bool optional = false)
{
    if (value < (optional ? -1 : 0) || value >= count)
        throw std::runtime_error("invalid BMD index");
}

void ReadTriangle(BmdBodyReader& reader, int vertices, int normals, int texcoords)
{
    BmdBodyReader triangle(reader.Take(TriangleBytes));
    const auto polygon = triangle.Read<std::uint8_t>();
    if (polygon != 3 && polygon != TriangleSlots)
        throw std::runtime_error("invalid BMD polygon");
    triangle.Take(1);
    for (const int count : {vertices, normals, texcoords})
        for (int slot = 0; slot < TriangleSlots; ++slot)
        {
            const int index = triangle.Read<std::int16_t>();
            if (slot < polygon)
                CheckIndex(index, count);
        }
}

void ReadMesh(BmdBodyReader& reader, int bones, int meshes)
{
    const int vertices = reader.Count(MaxVertices);
    const int normals = reader.Count(MaxVertices);
    const int texcoords = reader.Count(MaxSignedCount);
    const int triangles = reader.Count(MaxSignedCount);
    CheckIndex(reader.Read<std::int16_t>(), meshes);
    for (int i = 0; i < vertices; ++i)
    {
        CheckIndex(reader.Read<std::int16_t>(), bones);
        reader.Take(2);
        reader.Floats(3);
    }
    for (int i = 0; i < normals; ++i)
    {
        CheckIndex(reader.Read<std::int16_t>(), bones);
        reader.Take(2);
        reader.Floats(3);
        CheckIndex(reader.Read<std::int16_t>(), vertices, true);
        reader.Take(2);
    }
    reader.Floats(static_cast<std::size_t>(texcoords) * 2);
    for (int i = 0; i < triangles; ++i)
        ReadTriangle(reader, vertices, normals, texcoords);
    const auto texture = reader.Take(NameBytes);
    if (std::find(texture.begin(), texture.end(), 0) == texture.end())
        throw std::runtime_error("unterminated BMD texture name");
}

void ReadBones(BmdBodyReader& reader, int bones, std::size_t totalKeys)
{
    std::array<int, MaxBones> parents;
    parents.fill(-1);
    for (int bone = 0; bone < bones; ++bone)
    {
        if (reader.Flag())
            continue;
        reader.Take(NameBytes);
        parents[bone] = reader.Read<std::int16_t>();
        CheckIndex(parents[bone], bones, true);
        reader.Floats(totalKeys * 6);
    }
    for (int bone = 0; bone < bones; ++bone)
    {
        int ancestor = bone;
        int depth = 0;
        while (ancestor != -1)
        {
            if (++depth > bones)
                throw std::runtime_error("BMD bone parent cycle");
            ancestor = parents[ancestor];
        }
    }
}

bool DecodeModulus(std::span<const std::uint8_t> envelope, std::vector<std::uint8_t>& body)
{
    const bool hasMars = envelope.size() >= 2 && ((envelope[0] & 7) == 5 || (envelope[1] & 7) == 5);
    bool found = false;
    for (int variant = 0; variant < (hasMars ? 2 : 1); ++variant)
    {
        std::vector<std::uint8_t> candidate(envelope.begin(), envelope.end());
        if (!BmdModulusCipher::DecryptInPlace(candidate.data(), candidate.size(), variant != 0))
            continue;
        candidate.erase(candidate.begin(), candidate.begin() + BmdModulusCipher::BodyOffset);
        if (!ValidateBody(candidate))
            continue;
        if (found && body != candidate)
            return false;
        if (!found)
            body = std::move(candidate);
        found = true;
    }
    return found;
}

bool DecodeFile(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& body)
{
    if (file.size() < FileHeaderBytes || file.size() > MaxFileBytes || file[0] != 'B' || file[1] != 'M' ||
        file[2] != 'D')
        return false;
    if (file[3] == 10)
    {
        body.assign(file.begin() + FileHeaderBytes, file.end());
        return ValidateBody(body);
    }
    if ((file[3] != 12 && file[3] != 14) || file.size() < EncryptedHeaderBytes)
        return false;
    BmdBodyReader header(file.subspan(FileHeaderBytes));
    const auto size = header.Read<std::int32_t>();
    if (size < 0 || static_cast<std::size_t>(size) != file.size() - EncryptedHeaderBytes)
        return false;
    const auto payload = file.subspan(EncryptedHeaderBytes);
    if (file[3] == 14)
        return DecodeModulus(payload, body);
    body.resize(payload.size());
    std::uint8_t state = 0x5e;
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        body[i] = static_cast<std::uint8_t>((payload[i] ^ MapKey[i % MapKey.size()]) - state);
        state = static_cast<std::uint8_t>(payload[i] + 0x3d);
    }
    return ValidateBody(body);
}
} // namespace

bool ValidateBody(std::span<const std::uint8_t> body)
{
    if (body.size() > MaxFileBytes)
        return false;
    try
    {
        BmdBodyReader reader(body);
        reader.Take(NameBytes);
        const int meshes = reader.Count(MaxMeshes);
        const int bones = reader.Count(MaxBones);
        const int actions = reader.Count(MaxActions);
        if (actions == 0)
            return false;
        for (int mesh = 0; mesh < meshes; ++mesh)
            ReadMesh(reader, bones, meshes);
        std::size_t totalKeys = 0;
        for (int action = 0; action < actions; ++action)
        {
            const int keys = reader.Count(MaxSignedCount);
            if (keys == 0)
                return false;
            totalKeys += keys;
            if (reader.Flag())
                reader.Floats(static_cast<std::size_t>(keys) * 3);
        }
        ReadBones(reader, bones, totalKeys);
        return reader.Empty();
    }
    catch (const std::runtime_error&)
    {
        return false;
    }
}

bool Decode(std::span<const std::uint8_t> file, std::vector<std::uint8_t>& body)
{
    body.clear();
    try
    {
        if (DecodeFile(file, body))
            return true;
    }
    catch (const std::exception&)
    {
    }
    body.clear();
    return false;
}
} // namespace Render::Models::BmdModelFile
