#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace Render::Models
{
// Material sidecars are small, explicit overrides, not a replacement model format.
inline constexpr std::size_t MaxMaterialFileBytes = 16u * 1024u;
inline constexpr std::size_t MaxMaterialMeshCount = 50;

// Validates every entry before returning any mesh indexes. Throws invalid_argument
// on malformed metadata, unsupported fields or a mismatched model/texture index.
std::vector<int> ReadAdditiveMaterials(std::string_view content, std::span<const std::string_view> textures);
} // namespace Render::Models
