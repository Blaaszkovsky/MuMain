#include "stdafx.h"

#include <doctest.h>

#include "App/Control/ControlCommands.h"
#include "App/Control/ControlEvents.h"
#include "json.hpp"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Models/ZzzBMD.h"
#include "Scenes/SceneCore.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace
{
class HaltFixture
{
public:
    HaltFixture()
        : m_previousHero(Hero), m_previousScene(SceneFlag), m_previousWorld(gMapManager.WorldActive),
          m_previousFactor(FPS_ANIMATION_FACTOR), m_previousModels(Models), m_previousCharacters(CharactersClient),
          m_walls(TerrainWall, TerrainWall + TERRAIN_SIZE * TERRAIN_SIZE),
          m_heights(BackTerrainHeight, BackTerrainHeight + TERRAIN_SIZE * TERRAIN_SIZE),
          m_previousAttacking(Attacking)
    {
        Hero = &m_hero;
        SceneFlag = MAIN_SCENE;
        FPS_ANIMATION_FACTOR = 1.f;
        Models = &m_model;
        std::fill(std::begin(m_model.Sounds), std::end(m_model.Sounds), static_cast<short>(-1));
        std::fill_n(TerrainWall, TERRAIN_SIZE * TERRAIN_SIZE, WORD{});
        std::fill_n(BackTerrainHeight, TERRAIN_SIZE * TERRAIN_SIZE, 0.f);
        // A non-player model keeps this movement-only replay independent of UI windows.
        m_hero.Object.Type = 0;
        m_hero.Object.Live = true;
        m_hero.MoveSpeed = 12;
    }

    ~HaltFixture()
    {
        Hero = m_previousHero;
        CharactersClient = m_previousCharacters;
        SceneFlag = m_previousScene;
        gMapManager.WorldActive = m_previousWorld;
        FPS_ANIMATION_FACTOR = m_previousFactor;
        Models = m_previousModels;
        Attacking = m_previousAttacking;
        std::copy(m_walls.begin(), m_walls.end(), TerrainWall);
        std::copy(m_heights.begin(), m_heights.end(), BackTerrainHeight);
    }

    void BeginWalk(int map, int x, int y)
    {
        gMapManager.WorldActive = map;
        m_hero.PositionX = static_cast<BYTE>(x);
        m_hero.PositionY = static_cast<BYTE>(y);
        m_hero.Object.Position[0] = (static_cast<float>(x) + 0.5f) * TERRAIN_SCALE;
        m_hero.Object.Position[1] = (static_cast<float>(y) + 0.5f) * TERRAIN_SCALE;
        m_hero.Object.Position[2] = 0.f;
        m_hero.Movement = true;
        m_hero.Run = 7;
        m_hero.Path.PathNum = 2;
        m_hero.Path.CurrentPath = 0;
        m_hero.Path.CurrentPathFloat = 1;
    }

    void Halt()
    {
        std::unique_ptr<App::Control::Act> act;
        const auto request = App::Control::Request::Parse(R"({"cmd":"halt","id":1})");
        const auto response = App::Control::Commands::Halt(request, act);
        CHECK(response.find("\"ok\":true") != std::string::npos);
    }

    CHARACTER& Character() { return m_hero; }

    void UseWorldCharacters()
    {
        m_characters = std::make_unique<CHARACTER[]>(MAX_CHARACTERS_CLIENT);
        CharactersClient = m_characters.get();
    }

private:
    CHARACTER* m_previousHero;
    EGameScene m_previousScene;
    int m_previousWorld;
    float m_previousFactor;
    BMD* m_previousModels;
    CHARACTER* m_previousCharacters;
    std::unique_ptr<CHARACTER[]> m_characters;
    std::vector<WORD> m_walls;
    std::vector<float> m_heights;
    int m_previousAttacking;
    CHARACTER m_hero{};
    BMD m_model;
};
} // namespace

TEST_CASE("Control halt stops the model as well as its path [network][control-halt]")
{
    HaltFixture fixture;
    for (const auto location : {std::array{50, 140, 59}, std::array{38, 140, 185}})
    {
        CAPTURE(location[0]);
        fixture.BeginWalk(location[0], location[1], location[2]);
        auto& hero = fixture.Character();
        const float beforeX = hero.Object.Position[0];
        const float beforeY = hero.Object.Position[1];
        fixture.Halt();
        CHECK_FALSE(hero.Movement);
        CHECK(hero.Run == 0);
        CHECK(hero.Path.PathNum == 0);
        CHECK(hero.Path.CurrentPath == 0);
        CHECK(hero.Path.CurrentPathFloat == 0);

        // Replay the movement branch of MoveHero with its real path/position methods.
        // An empty path returns false (not "arrived"), so a stale Movement flag
        // advances Object.Position forever while the reported tile remains unchanged.
        constexpr int ReplayFrames = 60;
        for (int frame = 0; frame < ReplayFrames; ++frame)
        {
            if (hero.Movement && !MovePath(&hero))
                MoveCharacterPosition(&hero);
        }
        CHECK(hero.Object.Position[0] == beforeX);
        CHECK(hero.Object.Position[1] == beforeY);
        CHECK(hero.PositionX == location[1]);
        CHECK(hero.PositionY == location[2]);
        fixture.Halt();
        CHECK_FALSE(hero.Movement);
    }
}

TEST_CASE("Control halt leaves off-world and missing heroes alone [network][control-halt]")
{
    HaltFixture fixture;
    fixture.BeginWalk(38, 140, 185);
    SceneFlag = CHARACTER_SCENE;
    fixture.Halt();
    CHECK(fixture.Character().Movement);
    CHECK(fixture.Character().Path.PathNum == 2);
    SceneFlag = MAIN_SCENE;
    Hero = nullptr;
    fixture.Halt();
    CHECK(fixture.Character().Movement);
}

TEST_CASE("Control halt stops movement without replacing a dead character's action [network][control-halt]")
{
    HaltFixture fixture;
    fixture.BeginWalk(38, 140, 185);
    auto& hero = fixture.Character();
    hero.Dead = 1;
    hero.Object.CurrentAction = MONSTER01_DIE;
    fixture.Halt();
    CHECK_FALSE(hero.Movement);
    CHECK(hero.Path.PathNum == 0);
    CHECK(hero.Run == 7);
    CHECK(hero.Object.CurrentAction == MONSTER01_DIE);
}

TEST_CASE("Control halt disarms engine repeats without an active control act [network][control-halt]")
{
    HaltFixture fixture;
    fixture.BeginWalk(38, 140, 185);
    auto& hero = fixture.Character();
    SUBCASE("ordinary attack")
    {
        Attacking = 1;
        hero.MovementType = MOVEMENT_ATTACK;
    }
    SUBCASE("skill repeat and queued movement skill")
    {
        Attacking = 2;
        hero.MovementType = MOVEMENT_SKILL;
    }
    fixture.Halt();
    CHECK(Attacking == -1);
    REQUIRE(hero.MovementType == MOVEMENT_MOVE);
    CHECK_FALSE(hero.Movement);
    CHECK(hero.Path.PathNum == 0);
    // Action is the real path-arrival dispatcher. A stale attack/skill mode
    // would dereference/send through the absent network client in this fixture.
    Action(&hero, &hero.Object, false);
    fixture.Halt();
    CHECK(Attacking == -1);
}

namespace
{
constexpr int MoveSourceMap = 0;
constexpr int MoveDestinationMap = 30;
constexpr int MoveTargetX = 239;
constexpr int MoveTargetY = 14;

std::unique_ptr<App::Control::Act> BeginGateMove(HaltFixture& fixture)
{
    fixture.BeginWalk(MoveSourceMap, MoveTargetX - 1, MoveTargetY);
    std::unique_ptr<App::Control::Act> act;
    const auto request = App::Control::Request::Parse(R"({"cmd":"move","id":7,"x":239,"y":14})");
    REQUIRE(App::Control::Commands::Move(request, act).empty());
    REQUIRE(act != nullptr);
    act->SetEncodedId(request.EncodedId());
    std::string response;
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(response.empty());
    return act;
}

void CheckRelocationInterrupted(App::Control::Act& act)
{
    std::string response;
    REQUIRE(act.Tick(response) == App::Control::Act::Status::Finished);
    const auto decoded = nlohmann::json::parse(response);
    CHECK(decoded.at("id") == 7);
    CHECK(decoded.at("ok") == false);
    REQUIRE(decoded.contains("error"));
    CHECK(decoded.at("error") == "interrupted");
}
} // namespace

TEST_CASE("Control move cancels across maps before inspecting a preserved path [network][control-move]")
{
    HaltFixture fixture;
    auto act = BeginGateMove(fixture);
    fixture.BeginWalk(MoveDestinationMap, MoveTargetX - 1, MoveTargetY);
    auto& hero = fixture.Character();
    SUBCASE("the teleport stopped the old walk") { hero.Movement = false; }
    SUBCASE("a new manual walk is already active") { hero.Movement = true; }
    const bool wasMoving = hero.Movement;
    CheckRelocationInterrupted(*act);
    act.reset();
    CHECK(hero.Path.PathNum == 2);
    CHECK(hero.Path.CurrentPath == 0);
    CHECK(hero.Path.CurrentPathFloat == 1);
    CHECK(static_cast<bool>(hero.Movement) == wasMoving);
    CHECK(hero.Run == 7);
}

TEST_CASE("Control move cannot arrive at coincident coordinates on another map [network][control-move]")
{
    HaltFixture fixture;
    auto act = BeginGateMove(fixture);
    fixture.BeginWalk(MoveDestinationMap, MoveTargetX, MoveTargetY);
    fixture.Character().Path.PathNum = 0;
    fixture.Character().Movement = false;
    CheckRelocationInterrupted(*act);
}

TEST_CASE("Control move cancels after teleporting within the same map [network][control-move]")
{
    HaltFixture fixture;
    auto act = BeginGateMove(fixture);
    App::Control::Events::NoteTeleport();
    CheckRelocationInterrupted(*act);
    CHECK(fixture.Character().Path.PathNum == 2);
}

TEST_CASE("Control move still arrives after its local walk finishes [network][control-move]")
{
    HaltFixture fixture;
    auto act = BeginGateMove(fixture);
    fixture.Character().Path.CurrentPath = fixture.Character().Path.PathNum - 1;
    fixture.Character().PositionX = MoveTargetX;
    fixture.Character().PositionY = MoveTargetY;
    std::string response;
    REQUIRE(act->Tick(response) == App::Control::Act::Status::Finished);
    CHECK(nlohmann::json::parse(response).at("ok") == true);
}

TEST_CASE("Control attack cancels relocation before accepting a reused target key [network][control-attack-relocation]")
{
    HaltFixture fixture;
    fixture.UseWorldCharacters();
    fixture.BeginWalk(MoveSourceMap, MoveTargetX - 1, MoveTargetY);
    constexpr int TargetKey = 532;
    auto& originalTarget = CharactersClient[0];
    originalTarget.Object.Live = true;
    originalTarget.Key = TargetKey;
    std::unique_ptr<App::Control::Act> act;
    const auto request = App::Control::Request::Parse(R"({"cmd":"attack","id":7,"target":532})");
    REQUIRE(App::Control::Commands::Attack(request, act).empty());
    REQUIRE(act != nullptr);
    act->SetEncodedId(request.EncodedId());
    std::string response;
    CHECK(act->Tick(response) == App::Control::Act::Status::Running);
    CHECK(response.empty());

    SUBCASE("another map") { gMapManager.WorldActive = MoveDestinationMap; }
    SUBCASE("teleport on the same map") { App::Control::Events::NoteTeleport(); }
    originalTarget.Object.Live = false;
    auto& unrelatedTarget = CharactersClient[1];
    unrelatedTarget.Object.Live = true;
    unrelatedTarget.Key = TargetKey;
    REQUIRE(FindCharacterIndex(TargetKey) == 1);
    auto& hero = fixture.Character();
    hero.Path.PathNum = 0;
    hero.Movement = false;
    // Exercise the real AttackObject Busy result without equipment, network or damage.
    hero.Object.CurrentAction = PLAYER_ATTACK_FIST;
    CheckRelocationInterrupted(*act);
    CHECK(hero.Object.CurrentAction == PLAYER_ATTACK_FIST);
    CHECK(hero.Path.PathNum == 0);
    CHECK_FALSE(hero.Movement);
}
