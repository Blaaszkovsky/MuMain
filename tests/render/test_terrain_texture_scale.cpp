#include <doctest.h>

#include "Render/Terrain/TerrainTextureScale.h"

#include <cmath>
#include <utility>

TEST_CASE("terrain texture scale preserves ground and animated water tiling [render][terrain]")
{
    float width = 0.f;
    float height = 0.f;
    REQUIRE(Render::Terrain::CalculateTextureScale(256, 128, false, width, height));
    CHECK(width == doctest::Approx(0.25f));
    CHECK(height == doctest::Approx(0.5f));
    REQUIRE(Render::Terrain::CalculateTextureScale(256, 128, true, width, height));
    CHECK(width == doctest::Approx(0.0625f));
    CHECK(height == doctest::Approx(0.125f));
}

TEST_CASE("missing or invalid terrain textures cannot produce non-finite coordinates [render][terrain]")
{
    for (const bool animatedWater : {false, true})
    {
        for (const auto dimensions : {std::pair{0, 0}, {0, 256}, {256, 0}, {-1, 256}, {256, -1}})
        {
            float width = 1.f;
            float height = 1.f;
            CHECK_FALSE(Render::Terrain::CalculateTextureScale(
                dimensions.first, dimensions.second, animatedWater, width, height));
            CHECK(width == 1.f);
            CHECK(height == 1.f);
            CHECK(std::isfinite(width));
            CHECK(std::isfinite(height));
        }
    }
}
