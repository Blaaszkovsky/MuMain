#include "stdafx.h"
#include "ModelMaterialLoader.h"
#include "ModelMaterials.h"

#include "Core/Platform/PathResolve.h"
#include "Core/Text/Utf8.h"
#include "Render/Models/ZzzBMD.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace Render::Models
{
namespace
{
constexpr const wchar_t* MaterialSuffix = L".materials.json";
static_assert(MaxMaterialMeshCount == MAX_MESH);

std::string ReadMaterialFile(std::ifstream& input)
{
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxMaterialFileBytes))
        throw std::invalid_argument("Material file is empty, unseekable or exceeds 16 KiB");
    input.seekg(0, std::ios::beg);
    std::string content(static_cast<std::size_t>(size), '\0');
    input.read(content.data(), static_cast<std::streamsize>(content.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(content.size()))
        throw std::invalid_argument("Material file was truncated during reading");
    return content;
}

void ApplyMaterials(BMD& model, std::string_view content)
{
    if (model.NumMeshs <= 0 || model.NumMeshs > MAX_MESH)
        throw std::invalid_argument("Model mesh count is outside material limits");
    std::array<std::string_view, MAX_MESH> textures;
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const auto& name = model.Textures[i].FileName;
        textures[i] = {name, static_cast<std::size_t>(std::find(std::begin(name), std::end(name), '\0') - name)};
    }
    const auto meshes = ReadAdditiveMaterials(content, std::span(textures).first(model.NumMeshs));
    std::vector<std::unique_ptr<TextureScript>> scripts;
    for (const int mesh : meshes)
    {
        if (model.Meshs[mesh].Texture != mesh)
            throw std::invalid_argument("Additive material requires matching mesh/texture indexes");
        const auto* previous = model.Meshs[mesh].m_csTScript;
        scripts.push_back(previous ? std::make_unique<TextureScript>(*previous) : std::make_unique<TextureScript>());
        scripts.back()->setBright(true);
    }
    // BMD owns these pointers; all validation/allocation precedes this commit.
    for (std::size_t i = 0; i < meshes.size(); ++i)
    {
        auto& destination = model.Meshs[meshes[i]].m_csTScript;
        delete destination;
        destination = scripts[i].release();
    }
}
} // namespace

void ApplyMaterialFile(BMD& model, const std::wstring& modelFile)
{
    const std::wstring fileName = modelFile + MaterialSuffix;
    auto path = Core::Text::ToUtf8(fileName.c_str());
#ifndef _WIN32
    path = MuResolvePath(path.c_str());
#endif
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        std::error_code error;
        if (std::filesystem::exists(path, error) || error)
            g_ErrorReport.Write(L"Model material unreadable: %ls\r\n", fileName.c_str());
        return;
    }
    try
    {
        ApplyMaterials(model, ReadMaterialFile(input));
    }
    catch (const std::exception& error)
    {
        g_ErrorReport.Write(L"Model material rejected: %ls: %hs\r\n", fileName.c_str(), error.what());
    }
}
} // namespace Render::Models
