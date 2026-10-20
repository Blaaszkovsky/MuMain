#include <doctest.h>

#include "Render/Models/ModelMaterials.h"
#include "Render/Sprites/TextureScript.h"

#include <array>
#include <stdexcept>
#include <string>

namespace
{
constexpr std::array<std::string_view, 3> SwordTextures = {
    "DarkangelSwordA.jpg", "DarkangelSwordB.jpg", "gra2.JPG"};
}

TEST_CASE("explicit material masks select only the sword effect meshes [render][materials]")
{
    const auto meshes = Render::Models::ReadAdditiveMaterials(
        R"({"formatVersion":1,"additiveMeshes":[{"mesh":1,"texture":"DarkangelSwordB.jpg"},{"mesh":2,"texture":"gra2.JPG"}]})",
        SwordTextures);
    CHECK(meshes == std::vector<int>{1, 2});
}

TEST_CASE("material metadata cannot silently target a different model [render][materials]")
{
    for (const std::string_view json : {
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":-1,"texture":"gra2.JPG"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":3,"texture":"gra2.JPG"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":2,"texture":"wrong.jpg"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":2,"texture":"gra2.JPG"},{"mesh":2,"texture":"gra2.JPG"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":1.0,"texture":"DarkangelSwordB.jpg"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":18446744073709551615,"texture":"gra2.JPG"}]})",
             R"({"formatVersion":1,"additiveMeshes":[{"mesh":true,"texture":"gra2.JPG"}]})",
             R"({"formatVersion":2,"additiveMeshes":[]})",
             R"({"formatVersion":1,"additiveMeshes":[],"emission":true})"})
    {
        CHECK_THROWS_AS(Render::Models::ReadAdditiveMaterials(json, SwordTextures), std::invalid_argument);
    }
}

TEST_CASE("empty material metadata leaves existing materials unchanged [render][materials]")
{
    CHECK(Render::Models::ReadAdditiveMaterials(R"({"formatVersion":1,"additiveMeshes":[]})", SwordTextures).empty());
}

TEST_CASE("material parsing is bounded before allocation and recursion [render][materials]")
{
    CHECK_THROWS_AS(Render::Models::ReadAdditiveMaterials(std::string(16385, ' '), SwordTextures), std::invalid_argument);
    CHECK_THROWS_AS(Render::Models::ReadAdditiveMaterials("[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]", SwordTextures), std::invalid_argument);
    CHECK_THROWS_AS(Render::Models::ReadAdditiveMaterials("{", SwordTextures), std::invalid_argument);
    const std::array<std::string_view, 51> tooManyTextures = {};
    CHECK_THROWS_AS(Render::Models::ReadAdditiveMaterials(R"({"formatVersion":1,"additiveMeshes":[]})", tooManyTextures), std::invalid_argument);
}

TEST_CASE("an additive override preserves existing texture flags [render][materials]")
{
    class ExistingScript : public TextureScript
    {
    public:
        ExistingScript()
        {
            m_bHiddenMesh = true;
            m_bStreamMesh = true;
            m_bNoneBlendMesh = true;
            m_byShadowMesh = SHADOW_RENDER_TEXTURE;
        }
    } original;
    TextureScript copy(original);
    copy.setBright(true);
    CHECK_FALSE(original.getBright());
    CHECK(copy.getBright());
    CHECK(copy.getHiddenMesh());
    CHECK(copy.getStreamMesh());
    CHECK(copy.getNoneBlendMesh());
    CHECK(copy.getShadowMesh() == SHADOW_RENDER_TEXTURE);
}
