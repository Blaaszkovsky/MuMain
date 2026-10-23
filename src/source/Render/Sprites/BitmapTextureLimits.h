#pragma once

#include "Render/Textures/TextureFile.h"

namespace Render::Sprites
{
// Whole-image RGBA uploads require a fixed staging allocation sized from these
// decoder limits. Raising either limit also raises the per-slot GPU budget.
inline constexpr int kBitmapMaximumWidth = Render::Textures::MaxTextureDimension;
inline constexpr int kBitmapMaximumHeight = Render::Textures::MaxTextureDimension;
// Upscaled art (Render/Textures/UpscaleMarker.h) is up to this times the limits
// above; such an image is staged in bands of rows.
inline constexpr int kBitmapMaximumScale = 4;
} // namespace Render::Sprites
