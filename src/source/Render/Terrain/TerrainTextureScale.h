#pragma once

namespace Render::Terrain
{
    constexpr bool CalculateTextureScale(int textureWidth, int textureHeight, bool animatedWater,
                                        float& width, float& height)
    {
        if (textureWidth <= 0 || textureHeight <= 0)
        {
            return false;
        }

        constexpr float GroundTexelsPerTile = 64.f;
        constexpr float WaterTexelsPerTile = 16.f;
        const float texelsPerTile = animatedWater ? WaterTexelsPerTile : GroundTexelsPerTile;
        width = texelsPerTile / textureWidth;
        height = texelsPerTile / textureHeight;
        return true;
    }
}
