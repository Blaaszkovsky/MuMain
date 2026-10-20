#include "ModelMaterials.h"
#include "json.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace Render::Models
{
namespace
{
constexpr int FormatVersion = 1;
constexpr int MaxJsonDepth = 8;

int ReadMesh(const nlohmann::json& entry, std::span<const std::string_view> textures)
{
    if (!entry.is_object() || entry.size() != 2 || !entry.contains("mesh") || !entry.contains("texture") ||
        !entry["mesh"].is_number_integer() || entry["mesh"] < 0 || !entry["texture"].is_string())
    {
        throw std::invalid_argument("An additive material needs an integer mesh and its exact texture name");
    }
    const auto mesh = entry["mesh"].get<std::uint64_t>();
    if (mesh >= textures.size() || entry["texture"].get<std::string>() != textures[mesh])
    {
        throw std::invalid_argument("Additive material mesh/texture does not match the loaded model");
    }
    return static_cast<int>(mesh);
}
} // namespace

std::vector<int> ReadAdditiveMaterials(std::string_view content, std::span<const std::string_view> textures)
{
    if (content.size() > MaxMaterialFileBytes || textures.size() > MaxMaterialMeshCount)
    {
        throw std::invalid_argument("Material metadata exceeds its resource limits");
    }
    try
    {
        const auto json = nlohmann::json::parse(content, [](int depth, auto, auto&) {
            if (depth > MaxJsonDepth) throw std::invalid_argument("Material metadata is nested too deeply");
            return true;
        });
        if (!json.is_object() || json.size() != 2 || !json.contains("formatVersion") ||
            !json["formatVersion"].is_number_integer() || json["formatVersion"] != FormatVersion ||
            !json.contains("additiveMeshes") || !json["additiveMeshes"].is_array() ||
            json["additiveMeshes"].size() > textures.size())
        {
            throw std::invalid_argument("Expected formatVersion 1 and an additiveMeshes array");
        }
        std::vector<int> meshes;
        for (const auto& entry : json["additiveMeshes"])
        {
            const int mesh = ReadMesh(entry, textures);
            if (std::find(meshes.begin(), meshes.end(), mesh) != meshes.end())
                throw std::invalid_argument("Additive material mesh is repeated");
            meshes.push_back(mesh);
        }
        return meshes;
    }
    catch (const nlohmann::json::exception& error)
    {
        throw std::invalid_argument(error.what());
    }
}
} // namespace Render::Models
