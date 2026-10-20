#pragma once

#include <string>

class BMD;

namespace Render::Models
{
// Optional <model>.bmd.materials.json, applied only after the full file and every
// mesh/texture match are valid. Missing/invalid sidecars preserve existing flags.
void ApplyMaterialFile(BMD& model, const std::wstring& modelFile);
} // namespace Render::Models
