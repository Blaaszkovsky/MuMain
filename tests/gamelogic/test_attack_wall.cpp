#include "stdafx.h"

#include <doctest.h>

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Terrain/ZzzLodTerrain.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace
{
class AttackWallFixture
{
public:
    AttackWallFixture()
        : m_walls(TerrainWall, TerrainWall + TERRAIN_SIZE * TERRAIN_SIZE),
          m_selection(SelectedCharacter), m_previousCharacters(CharactersClient),
          m_characters(std::make_unique<CHARACTER[]>(1))
    {
        CharactersClient = m_characters.get();
        std::fill_n(TerrainWall, TERRAIN_SIZE * TERRAIN_SIZE, WORD{});
        SelectTarget(MODEL_CASTLE_GATE1, 12, 10);
    }

    ~AttackWallFixture()
    {
        std::copy(m_walls.begin(), m_walls.end(), TerrainWall);
        CharactersClient = m_previousCharacters;
        SelectedCharacter = m_selection;
    }

    void SelectTarget(int model, int x, int y)
    {
        SelectedCharacter = 0;
        CharactersClient[0].Object.Type = model;
        CharactersClient[0].Object.Position[0] = (static_cast<float>(x) + 0.5f) * TERRAIN_SCALE;
        CharactersClient[0].Object.Position[1] = (static_cast<float>(y) + 0.5f) * TERRAIN_SCALE;
    }

    void Wall(int x, int y, WORD flags = TW_NOMOVE)
    {
        TerrainWall[TERRAIN_INDEX_REPEAT(x, y)] = flags;
    }

private:
    std::vector<WORD> m_walls;
    int m_selection;
    CHARACTER* m_previousCharacters;
    std::unique_ptr<CHARACTER[]> m_characters;
};
} // namespace

TEST_CASE("Castle Siege gate can be attacked on its blocked destination tile [combat][walls]")
{
    AttackWallFixture fixture;
    SUBCASE("horizontal attack")
    {
        fixture.Wall(12, 10);
        CHECK(CheckWall(10, 10, 12, 10));
    }
    SUBCASE("observed live siege coordinates")
    {
        fixture.SelectTarget(MODEL_CASTLE_GATE1, 93, 114);
        fixture.Wall(93, 114);
        CHECK(CheckWall(92, 113, 93, 114));
    }
}

TEST_CASE("A gate target never permits attacks through intervening walls [combat][walls]")
{
    AttackWallFixture fixture;
    for (const int gate : std::array<int, 5>{MODEL_CASTLE_GATE1, MODEL_EVIL_GATE, MODEL_LION_GATE, MODEL_STAR_GATE, MODEL_RUSH_GATE})
    {
        CAPTURE(gate);
        fixture.SelectTarget(gate, 12, 10);
        fixture.Wall(12, 10);
        fixture.Wall(11, 10);
        CHECK_FALSE(CheckWall(10, 10, 12, 10));
        fixture.Wall(11, 10, 0);
        CHECK(CheckWall(10, 10, 12, 10));
    }
}

TEST_CASE("A selected gate grants no exception for another destination [combat][walls]")
{
    AttackWallFixture fixture;
    for (const int gate : std::array<int, 2>{MODEL_CASTLE_GATE1, MODEL_EVIL_GATE})
    {
        fixture.SelectTarget(gate, 13, 10);
        fixture.Wall(12, 10);
        CHECK_FALSE(CheckWall(10, 10, 12, 10));
    }
}

TEST_CASE("Ordinary targets and invalid selections retain terrain collision [combat][walls]")
{
    AttackWallFixture fixture;
    fixture.Wall(12, 10);
    fixture.SelectTarget(MODEL_MONSTER01, 12, 10);
    CHECK_FALSE(CheckWall(10, 10, 12, 10));
    SelectedCharacter = -1;
    CHECK_FALSE(CheckWall(10, 10, 12, 10));
    SelectedCharacter = MAX_CHARACTERS_CLIENT;
    CHECK_FALSE(CheckWall(10, 10, 12, 10));
    SelectedCharacter = MAX_CHARACTERS_CLIENT + 1;
    CHECK_FALSE(CheckWall(10, 10, 12, 10));
}

TEST_CASE("A blocked source tile is not waived by the gate exception [combat][walls]")
{
    AttackWallFixture fixture;
    fixture.Wall(10, 10);
    fixture.Wall(12, 10);
    CHECK_FALSE(CheckWall(10, 10, 12, 10));
}

TEST_CASE("Attack wall checks preserve their passable terrain flags [combat][walls]")
{
    AttackWallFixture fixture;
    fixture.SelectTarget(MODEL_MONSTER01, 12, 10);
    CHECK(CheckWall(10, 10, 12, 10));
    for (const WORD flag : {WORD(TW_ACTION), WORD(TW_HEIGHT), WORD(TW_CAMERA_UP)})
    {
        CAPTURE(flag);
        fixture.Wall(11, 10, TW_NOMOVE | flag);
        CHECK(CheckWall(10, 10, 12, 10));
    }
}

