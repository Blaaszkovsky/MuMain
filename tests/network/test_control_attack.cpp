// Real Commands::Attack -> Automation::AttackObject -> Action -> real AOT send.
// The only peer is an isolated ephemeral loopback collector, never the game host.
#include "stdafx.h"
#include <winsock2.h>
#include <doctest.h>

#include "App/Control/ControlCommands.h"
#include "App/Control/ControlDispatcher.h"
#include "App/Control/ControlEvents.h"
#include "App/Control/ControlObjects.h"
#include "App/Control/ControlTaps.h"
#include "Dotnet/Connection.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Automation/Attack.h"
#include "GameLogic/Automation/Skill.h"
#include "GameLogic/Skills/SkillManager.h"
#include "Network/Server/WSclient.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Scenes/SceneCore.h"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Supply the two real UI-owned skill inputs without opening a window.
#define private public
#include "UI/Core/WindowSystem.h"
#undef private

extern int TargetX;
extern int TargetY;
extern MovementSkill g_MovementSkill;
extern bool EnableFastInput;

namespace
{
using namespace std::chrono_literals;
namespace Control = App::Control;
constexpr int HeroId = 512;
constexpr int GateId = 532;
constexpr int GateSlot = 1;
constexpr BYTE HitCode = 0x11;
constexpr BYTE TargetedSkillCode = 0x19;
constexpr BYTE AreaSkillCode = 0x1E;

bool TimeoutEnabled()
{
    const char* value = std::getenv("MU_CONTROL_ATTACK_TIMEOUT_TEST");
    return value != nullptr && std::string(value) == "1";
}

class LoopbackCollector
{
public:
    LoopbackCollector()
    {
        WSADATA data{};
        REQUIRE(WSAStartup(MAKEWORD(2, 2), &data) == 0);
        m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(m_listener != INVALID_SOCKET);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(m_listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        REQUIRE(listen(m_listener, 1) == 0);
        int size = sizeof(address);
        REQUIRE(getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
        m_port = ntohs(address.sin_port);
    }

    ~LoopbackCollector()
    {
        if (m_peer != INVALID_SOCKET) closesocket(m_peer);
        if (m_listener != INVALID_SOCKET) closesocket(m_listener);
        WSACleanup();
    }

    int Port() const { return m_port; }
    std::size_t ReceivedBytes() const { return m_bytes.size(); }

    void Accept()
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listener, &readable);
        timeval wait{2, 0};
        REQUIRE(select(0, &readable, nullptr, nullptr, &wait) > 0);
        m_peer = accept(m_listener, nullptr, nullptr);
        REQUIRE(m_peer != INVALID_SOCKET);
        u_long nonblocking = 1;
        REQUIRE(ioctlsocket(m_peer, FIONBIO, &nonblocking) == 0);
    }

    int PacketsAfterSettling(BYTE code)
    {
        // Real managed output is asynchronous. Collect complete packets;
        // a count of Act ACKs is deliberately not the assertion here.
        const auto until = std::chrono::steady_clock::now() + 500ms;
        do
        {
            char buffer[1024];
            const int count = recv(m_peer, buffer, sizeof(buffer), 0);
            if (count > 0)
                m_bytes.insert(m_bytes.end(), buffer, buffer + count);
            else if (count < 0)
                REQUIRE(WSAGetLastError() == WSAEWOULDBLOCK);
            std::this_thread::sleep_for(2ms);
        } while (std::chrono::steady_clock::now() < until);

        int hits = 0;
        std::size_t offset = 0;
        while (offset + 2 <= m_bytes.size())
        {
            REQUIRE((m_bytes[offset] == 0xC1 || m_bytes[offset] == 0xC3));
            const auto size = m_bytes[offset + 1];
            REQUIRE(size >= 3);
            if (offset + size > m_bytes.size()) break;
            if (m_bytes[offset + 2] == code)
            {
                if (code == HitCode)
                {
                    REQUIRE(size == 7);
                    CHECK((m_bytes[offset + 3] << 8 | m_bytes[offset + 4]) == GateId);
                }
                ++hits;
            }
            offset += size;
        }
        return hits;
    }

    int HitsAfterSettling() { return PacketsAfterSettling(HitCode); }

private:
    SOCKET m_listener = INVALID_SOCKET;
    SOCKET m_peer = INVALID_SOCKET;
    int m_port = 0;
    std::vector<unsigned char> m_bytes;
};

class AttackFixture
{
public:
    AttackFixture()
        : m_previousHero(Hero), m_previousCharacters(CharactersClient), m_previousMachine(CharacterMachine),
          m_previousAttribute(CharacterAttribute), m_previousModels(Models), m_previousSocket(SocketClient),
          m_previousScene(SceneFlag), m_previousWorld(gMapManager.WorldActive), m_previousHeroKey(HeroKey),
          m_previousAttacking(Attacking), m_previousTarget(ActionTarget), m_previousSelection(SelectedCharacter),
          m_previousTargetX(TargetX), m_previousTargetY(TargetY),
          m_eventsEnabled(Control::Events::IsEnabled()),
          m_walls(TerrainWall, TerrainWall + TERRAIN_SIZE * TERRAIN_SIZE),
          m_characters(std::make_unique<CHARACTER[]>(MAX_CHARACTERS_CLIENT)),
          m_models(std::make_unique<BMD[]>(MODEL_PLAYER + 1))
    {
        Hero = m_characters.get();
        CharactersClient = m_characters.get();
        CharacterMachine = &m_machine;
        CharacterAttribute = &m_machine.Character;
        Models = m_models.get();
        Models[MODEL_PLAYER].NumActions = MAX_PLAYER_ACTION;
        Models[MODEL_PLAYER].Actions = new Action_t[MAX_PLAYER_ACTION]{};
        SceneFlag = MAIN_SCENE;
        // Suppress sound in this headless action fixture, as SetPlayerAttack does.
        gMapManager.WorldActive = WD_73NEW_LOGIN_SCENE;
        HeroKey = HeroId;
        Hero->Key = HeroId;
        Hero->Object.Live = true;
        Hero->Object.Type = MODEL_PLAYER;
        Hero->Object.Kind = KIND_PLAYER;
        Hero->Object.CurrentAction = PLAYER_STOP_MALE;
        Hero->Helper.Type = -1;
        Hero->Weapon[0].Type = Hero->Weapon[1].Type = -1;
        for (auto& item : m_machine.Equipment) item.Type = -1;
        Place(*Hero, 92, 113);
        auto& gate = CharactersClient[GateSlot];
        gate.Key = GateId;
        gate.Object.Live = true;
        gate.Object.Kind = KIND_MONSTER;
        gate.Object.Type = MODEL_CASTLE_GATE1;
        Place(gate, 93, 114);
        std::fill_n(TerrainWall, TERRAIN_SIZE * TERRAIN_SIZE, WORD{});
        TerrainWall[TERRAIN_INDEX_REPEAT(93, 114)] = TW_NOMOVE;
        // Same setup as OpenFont. SetMapDimensions releases prior scratch arrays;
        // the process-global PATH owns the new arrays until its own destruction.
        InitPath();
        Attacking = ActionTarget = SelectedCharacter = -1;
        m_connection = std::make_unique<Connection>(u"127.0.0.1", m_collector.Port(), false, nullptr);
        REQUIRE(m_connection->IsConnected());
        m_collector.Accept();
        SocketClient = m_connection.get();
    }

    ~AttackFixture()
    {
        SocketClient = m_previousSocket;
        m_connection.reset();
        Hero = m_previousHero;
        CharactersClient = m_previousCharacters;
        CharacterMachine = m_previousMachine;
        CharacterAttribute = m_previousAttribute;
        Models = m_previousModels;
        SceneFlag = m_previousScene;
        gMapManager.WorldActive = m_previousWorld;
        HeroKey = m_previousHeroKey;
        Attacking = m_previousAttacking;
        ActionTarget = m_previousTarget;
        SelectedCharacter = m_previousSelection;
        TargetX = m_previousTargetX;
        TargetY = m_previousTargetY;
        Control::Events::SetEnabled(m_eventsEnabled);
        Control::Events::SetObjectResolver({});
        Control::Events::Reset();
        // PATH retains TerrainWall's stable global address, never fixture storage.
        // Restore the map contents; the next fixture's InitPath resets scratch state.
        std::copy(m_walls.begin(), m_walls.end(), TerrainWall);
    }

    std::unique_ptr<Control::Act> Attack(int times = 1, int interval = 0)
    {
        const auto command = "{\"cmd\":\"attack\",\"target\":532,\"times\":" + std::to_string(times) +
                             ",\"interval\":" + std::to_string(interval) + "}";
        std::unique_ptr<Control::Act> act;
        REQUIRE(Control::Commands::Attack(Control::Request::Parse(command), act).empty());
        REQUIRE(act);
        return act;
    }

    void CheckNoOwnedRepeat()
    {
        CHECK(Attacking == -1);
        CHECK(SelectedCharacter == -1);
        CHECK(Hero->MovementType == MOVEMENT_MOVE);
        // Current in-flight swing's target is retained for outgoing damage attribution.
        CHECK(ActionTarget == GateSlot);
        CHECK(Hero->Object.CurrentAction == PLAYER_ATTACK_FIST);
        CHECK(Hero->AttackTime == 1);
    }

    int Hits() { return m_collector.HitsAfterSettling(); }
    int SkillPackets() { return m_collector.PacketsAfterSettling(TargetedSkillCode); }
    int AreaSkillPackets() { return m_collector.PacketsAfterSettling(AreaSkillCode); }

    std::size_t ReceivedBytes()
    {
        m_collector.HitsAfterSettling();
        return m_collector.ReceivedBytes();
    }

    void MoveOutOfRange(BYTE x = 78)
    {
        // Beyond the separate legacy hunting+range endpoint bug. The near
        // 88->93 product regression is deliberately a separate patch05.
        Place(*Hero, x, 113);
    }

    void CheckOutgoingTapStillRecognizesThisSwing()
    {
        auto& stale = CharactersClient[2];
        stale.Key = 541;
        stale.Object.Live = true;
        stale.Object.Kind = KIND_NPC;
        Control::Events::Reset();
        Control::Events::SetEnabled(true);
        Control::Events::SetObjectResolver(Control::DescribeGameObject);
        Control::Events::RecordAttackDamage(2, GateId, 23, 0, 0);
        const auto events = Control::Events::Since(0);
        REQUIRE(events.size() == 2);
        CHECK(events[0].name == "damage_observed");
        CHECK(events[1].name == "hit");
        const auto hit = nlohmann::json::parse(events[1].fields);
        CHECK(hit["direction"] == "dealt");
        CHECK(hit["attacker"]["id"] == HeroId);
        CHECK(hit["target"]["id"] == GateId);
        CHECK(hit["damage"] == 23);
    }

private:
    static void Place(CHARACTER& character, BYTE x, BYTE y)
    {
        character.PositionX = x;
        character.PositionY = y;
        character.Object.Position[0] = (static_cast<float>(x) + .5f) * TERRAIN_SCALE;
        character.Object.Position[1] = (static_cast<float>(y) + .5f) * TERRAIN_SCALE;
    }

    CHARACTER* m_previousHero;
    CHARACTER* m_previousCharacters;
    CHARACTER_MACHINE* m_previousMachine;
    CHARACTER_ATTRIBUTE* m_previousAttribute;
    BMD* m_previousModels;
    Connection* m_previousSocket;
    EGameScene m_previousScene;
    int m_previousWorld, m_previousHeroKey, m_previousAttacking, m_previousTarget, m_previousSelection;
    int m_previousTargetX, m_previousTargetY;
    bool m_eventsEnabled;
    std::vector<WORD> m_walls;
    std::unique_ptr<CHARACTER[]> m_characters;
    std::unique_ptr<BMD[]> m_models;
    CHARACTER_MACHINE m_machine{};
    LoopbackCollector m_collector;
    std::unique_ptr<Connection> m_connection;
};
} // namespace

TEST_CASE("Control times1 releases real engine repeat intent after issuing one hit [network][control-attack]")
{
    AttackFixture fixture;
    auto act = fixture.Attack();
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(fixture.Hits() == 1);
    fixture.CheckNoOwnedRepeat();
    fixture.CheckOutgoingTapStillRecognizesThisSwing();
    CHECK(act->Tick(response) == Control::Act::Status::Finished);
    act.reset();
    fixture.CheckNoOwnedRepeat();
    CHECK(fixture.Hits() == 1);
}

TEST_CASE("Control multi-hit interval leaves no engine repeat between real swings [network][control-attack]")
{
    AttackFixture fixture;
    auto act = fixture.Attack(2, 1000);
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(fixture.Hits() == 1);
    fixture.CheckNoOwnedRepeat();
    // Fixture advances the animation; the attack and send path remain production.
    Hero->Object.CurrentAction = PLAYER_STOP_MALE;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    CHECK(fixture.Hits() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds{1050});
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(fixture.Hits() == 2);
    fixture.CheckNoOwnedRepeat();
    CHECK(act->Tick(response) == Control::Act::Status::Finished);
    CHECK(fixture.Hits() == 2);
}

TEST_CASE("Cancelling a control attack cannot clear later manual engine intent [network][control-attack]")
{
    AttackFixture fixture;
    auto act = fixture.Attack(2, 1000);
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(fixture.Hits() == 1);
    // Actual globals used by MoveHero; do not replace the production AttackObject.
    Hero->MovementType = MOVEMENT_ATTACK;
    Attacking = 1;
    SelectedCharacter = ActionTarget = GateSlot;
    act.reset();
    CHECK(Attacking == 1);
    CHECK(SelectedCharacter == GateSlot);
    CHECK(ActionTarget == GateSlot);
    CHECK(Hero->MovementType == MOVEMENT_ATTACK);
}

TEST_CASE("Cancelling the real bounded approach clears only its still-owned path [network][control-attack]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange();
    auto act = fixture.Attack();
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Movement);
    REQUIRE(Hero->Path.PathNum >= 2);
    REQUIRE(fixture.Hits() == 0);
    Hero->Run = 7;
    act.reset();
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Run == 0);
    CHECK(Hero->Path.PathNum == 0);
    CHECK(fixture.Hits() == 0);
}

TEST_CASE("Cancelling an inactive real approach clears its path and preserves a later manual emote [network][control-attack]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange();
    auto act = fixture.Attack();
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Movement);
    REQUIRE(Hero->Path.PathNum >= 2);
    REQUIRE(fixture.Hits() == 0);

    // This is the inactive-path state also left by SendMove's early refusal.
    // The path itself was emitted by the real ApproachAlong above, not invented.
    Hero->Movement = false;
    SetActionClass(Hero, &Hero->Object, PLAYER_POSE1, AT_POSE1);
    REQUIRE(Hero->Object.CurrentAction == PLAYER_POSE1);
    Hero->Object.AnimationFrame = 2.5f;
    const auto attackTime = Hero->AttackTime;
    act.reset();

    CHECK(Hero->Path.PathNum == 0);
    CHECK(Hero->Path.CurrentPath == 0);
    CHECK(Hero->Path.CurrentPathFloat == 0);
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Object.CurrentAction == PLAYER_POSE1);
    CHECK(Hero->Object.AnimationFrame == 2.5f);
    CHECK(Hero->AttackTime == attackTime);
    CHECK(fixture.Hits() == 0);
}

TEST_CASE("AbandonConnection preserves a manual emote while clearing an inactive owned approach [network][control-attack]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange();
    Control::Dispatcher dispatcher;
    dispatcher.Handle(Control::Request::Parse(R"({"cmd":"attack","id":1,"target":532})"), 1);
    REQUIRE(dispatcher.OwesAnswer(1));
    REQUIRE(Hero->Movement);
    REQUIRE(Hero->Path.PathNum >= 2);
    REQUIRE(fixture.Hits() == 0);

    Hero->Movement = false;
    SetActionClass(Hero, &Hero->Object, PLAYER_POSE1, AT_POSE1);
    REQUIRE(Hero->Object.CurrentAction == PLAYER_POSE1);
    Hero->Object.AnimationFrame = 2.5f;
    dispatcher.AbandonConnection(1);

    CHECK_FALSE(dispatcher.OwesAnswer(1));
    CHECK(Hero->Path.PathNum == 0);
    CHECK(Hero->Path.CurrentPath == 0);
    CHECK(Hero->Path.CurrentPathFloat == 0);
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Object.CurrentAction == PLAYER_POSE1);
    CHECK(Hero->Object.AnimationFrame == 2.5f);
    CHECK(fixture.Hits() == 0);
}

TEST_CASE("Attack cancel preserves changed path and reused target slot [network][control-attack]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange();
    auto act = fixture.Attack();
    std::string response;
    CHECK(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Movement);
    SUBCASE("a different manual destination") { ++TargetX; }
    SUBCASE("a different manual path") { ++Hero->Path.PathX[Hero->Path.PathNum - 1]; }
    SUBCASE("the old target slot now belongs to another object") { CharactersClient[GateSlot].Key = 999; }
    SUBCASE("the world ended") { SceneFlag = CHARACTER_SCENE; }
    act.reset();
    CHECK(Hero->Movement);
    CHECK(Hero->Path.PathNum > 0);
    CHECK(fixture.Hits() == 0);
}

TEST_CASE("Control HALT destroys a pending real attack before any later swing [network][control-attack]")
{
    AttackFixture fixture;
    Control::Dispatcher dispatcher;
    dispatcher.Handle(Control::Request::Parse(R"({"cmd":"attack","id":1,"target":532,"times":2,"interval":1000})"), 1);
    REQUIRE(fixture.Hits() == 1);
    dispatcher.Handle(Control::Request::Parse(R"({"cmd":"halt","id":2})"), 2);
    for (int frame = 0; frame < 120; ++frame) dispatcher.Tick();
    CHECK_FALSE(dispatcher.OwesAnswer(1));
    CHECK(fixture.Hits() == 1);
    CHECK(Attacking == -1);
    CHECK(Hero->MovementType == MOVEMENT_MOVE);
}

TEST_CASE("Expired real control attack never issues one last swing [network][control-attack-timeout]" * doctest::skip(!TimeoutEnabled()))
{
    // Deliberate opt-in because this exercises the real 60-second production
    // deadline without adding a public clock API or replacing the attack path.
    REQUIRE(TimeoutEnabled());
    AttackFixture fixture;
    Control::Dispatcher dispatcher;
    dispatcher.Handle(Control::Request::Parse(R"({"cmd":"attack","target":532,"times":2,"interval":60000})"), 1);
    REQUIRE(fixture.Hits() == 1);
    Hero->Object.CurrentAction = PLAYER_STOP_MALE;
    std::this_thread::sleep_for(std::chrono::seconds{61});
    dispatcher.Tick();
    CHECK_FALSE(dispatcher.OwesAnswer(1));
    CHECK(fixture.Hits() == 1);
    CHECK(Attacking == -1);
}

// Separate product RED: normal InitPath is already supplied by AttackFixture.
// The existing control cleanup tests start farther away to isolate their cause.
TEST_CASE("A reachable near target approaches instead of returning NoPath [network][approach-radius]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange(88);
    REQUIRE(TerrainWall[TERRAIN_INDEX_REPEAT(88, 113)] == 0);
    REQUIRE(TerrainWall[TERRAIN_INDEX_REPEAT(92, 113)] == 0);
    bool allowPlayers = true;
    SUBCASE("control permits player targets") { allowPlayers = true; }
    SUBCASE("helper excludes player targets") { allowPlayers = false; }

    const auto result = GameLogic::Automation::AttackObject(GateId, allowPlayers);
    CHECK(result == GameLogic::Automation::AttackResult::Approaching);
    CHECK(Hero->Movement);
    CHECK(Hero->Path.PathNum >= 2);
    CHECK(Hero->MovementType == MOVEMENT_MOVE);
    CHECK(Attacking == -1);
    CHECK(fixture.Hits() == 0);
}

namespace
{
// Owns only the skill table/input state added by the range regression. The
// inherited fixture owns the real pathfinder setup, action state and transport.
class SkillRangeFixture : public AttackFixture
{
public:
    SkillRangeFixture()
        : m_previousAttributes(SkillAttribute), m_previousMovementSkill(g_MovementSkill),
          m_attributes(std::make_unique<SKILL_ATTRIBUTE[]>(MAX_SKILLS))
    {
        SkillAttribute = m_attributes.get();
        // AT_SKILL_FIREBALL is declared in _enum.h and handled by the real
        // targeted wizard path. Only its finite fixture range/cost are inputs.
        SkillAttribute[AT_SKILL_FIREBALL].Distance = FixtureSkillRange;
        SkillAttribute[AT_SKILL_FIREBALL].Mana = FixtureManaCost;
        g_MovementSkill = {false, -1, -1};
        CharacterAttribute->Mana = 0;
        MoveOutOfRange(88);
    }

    ~SkillRangeFixture()
    {
        SkillAttribute = m_previousAttributes;
        g_MovementSkill = m_previousMovementSkill;
    }

    static constexpr DWORD FixtureSkillRange = 3;
    static constexpr WORD FixtureManaCost = 123;

private:
    SKILL_ATTRIBUTE* m_previousAttributes;
    MovementSkill m_previousMovementSkill;
    std::unique_ptr<SKILL_ATTRIBUTE[]> m_attributes;
};
} // namespace

TEST_CASE("A targeted skill approaches within its own range before execution [network][approach-radius]")
{
    SkillRangeFixture fixture;
    float expectedRange = SkillRangeFixture::FixtureSkillRange;
    SUBCASE("ordinary range") {}
    SUBCASE("horse range bonus remains authoritative")
    {
        Hero->Helper.Type = MODEL_DARK_HORSE_ITEM;
        expectedRange += 2;
    }
    REQUIRE(gSkillManager.GetSkillDistance(AT_SKILL_FIREBALL, Hero) == expectedRange);
    const auto manaBefore = CharacterAttribute->Mana;
    const auto skillManaBefore = CharacterAttribute->SkillMana;

    const auto result = GameLogic::Automation::CastSkill(AT_SKILL_FIREBALL, true, GateId, true);
    CHECK(result == GameLogic::Automation::SkillResult::Approaching);
    CHECK(Hero->Movement);
    CHECK(Hero->Path.PathNum >= 2);
    CHECK(g_MovementSkill.m_iTarget == GateSlot);
    // AimAtTarget returns before ExecuteSkill. This checks aim's boundary,
    // not a successful cast, MP refusal, skill animation or spell packet.
    CHECK_FALSE(g_MovementSkill.m_bMagic);
    CHECK(g_MovementSkill.m_iSkill == -1);
    CHECK(CharacterAttribute->Mana == manaBefore);
    CHECK(CharacterAttribute->SkillMana == skillManaBefore);
    CHECK(fixture.Hits() == 0);
}

TEST_CASE("Attack relocation preserves a later identical manual path and sends nothing more [network][control-attack-relocation]")
{
    AttackFixture fixture;
    fixture.MoveOutOfRange();
    auto act = fixture.Attack();
    std::string response;
    REQUIRE(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Movement);
    REQUIRE(Hero->Path.PathNum >= 2);
    REQUIRE(fixture.Hits() == 0);
    const auto receivedBefore = fixture.ReceivedBytes();
    REQUIRE(receivedBefore > 0);
    const auto pathSize = Hero->Path.PathNum;
    std::array<BYTE, MAX_PATH_FIND> manualPathX{};
    std::array<BYTE, MAX_PATH_FIND> manualPathY{};
    std::copy_n(Hero->Path.PathX, pathSize, manualPathX.begin());
    std::copy_n(Hero->Path.PathY, pathSize, manualPathY.begin());

    SUBCASE("another map with an identical target slot") { gMapManager.WorldActive = WD_0LORENCIA; }
    SUBCASE("same-map teleport with an identical target slot") { Control::Events::NoteTeleport(); }
    // A new manual walk happens to replay all old ownership fields, including
    // the target key/slot and path. Relocation is the distinguishing evidence.
    std::copy_n(manualPathX.begin(), pathSize, Hero->Path.PathX);
    std::copy_n(manualPathY.begin(), pathSize, Hero->Path.PathY);
    Hero->Path.CurrentPath = 0;
    Hero->Path.CurrentPathFloat = 0;
    Hero->Movement = true;
    Hero->Run = 7;
    CHECK(act->Tick(response) == Control::Act::Status::Finished);
    if (!response.empty())
    {
        const auto decoded = nlohmann::json::parse(response);
        CHECK(decoded.at("ok") == false);
        CHECK(decoded.at("error") == "interrupted");
    }
    act.reset();
    CHECK(Hero->Movement);
    CHECK(Hero->Run == 7);
    CHECK(Hero->Path.PathNum == pathSize);
    CHECK(std::equal(manualPathX.begin(), manualPathX.begin() + pathSize, Hero->Path.PathX));
    CHECK(std::equal(manualPathY.begin(), manualPathY.begin() + pathSize, Hero->Path.PathY));
    CHECK(fixture.ReceivedBytes() == receivedBefore);
    CHECK(fixture.Hits() == 0);
}

namespace
{
class SkillCommandFixture : public AttackFixture
{
public:
    SkillCommandFixture()
        : m_previousAttributes(SkillAttribute), m_previousMovementSkill(g_MovementSkill),
          m_previousSkillList(g_pSkillList), m_previousOption(g_pOption), m_previousFastInput(EnableFastInput),
          m_attributes(std::make_unique<SKILL_ATTRIBUTE[]>(MAX_SKILLS))
    {
        auto* system = mu::ui::window::CSystem::GetInstance();
        system->m_pNewSkillList = &m_skillList;
        system->m_pNewOptionWindow = &m_option;
        m_option.SetAutoAttack(true);
        EnableFastInput = false;
        SkillAttribute = m_attributes.get();
        UseSkill(AT_SKILL_FIREBALL);
        CharacterAttribute->Level = 400;
        CharacterAttribute->Energy = 1000;
        CharacterAttribute->Mana = 1000;
        CharacterAttribute->SkillMana = 1000;
        gSkillManager.InvalidateSkillAttributeRequirementsCache();
        g_MovementSkill = {false, -1, -1};
    }

    ~SkillCommandFixture()
    {
        auto* system = mu::ui::window::CSystem::GetInstance();
        system->m_pNewSkillList = m_previousSkillList;
        system->m_pNewOptionWindow = m_previousOption;
        EnableFastInput = m_previousFastInput;
        SkillAttribute = m_previousAttributes;
        g_MovementSkill = m_previousMovementSkill;
        gSkillManager.InvalidateSkillAttributeRequirementsCache();
    }

    std::unique_ptr<Control::Act> Skill()
    {
        std::unique_ptr<Control::Act> act;
        REQUIRE(Control::Commands::Skill(SkillRequest(), act).empty());
        REQUIRE(act);
        return act;
    }

    Control::Request SkillRequest()
    {
        return Control::Request::Parse("{\"cmd\":\"skill\",\"id\":1,\"skill\":" + std::to_string(m_skill) +
                                       ",\"target\":532}");
    }

    void UseSkill(ActionSkillType skill)
    {
        m_skill = skill;
        SkillAttribute[skill].Distance = 3;
        CharacterAttribute->Skill[0] = skill;
        Hero->Class = skill == AT_SKILL_RAGEFUL_BLOW ? CLASS_KNIGHT : CLASS_WIZARD;
        CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type = ITEM_SHORT_SWORD;
        Hero->Weapon[0].Type = MODEL_SHORT_SWORD;
        gSkillManager.InvalidateSkillAttributeRequirementsCache();
    }

    int SkillPackets()
    {
        return m_skill == AT_SKILL_RAGEFUL_BLOW ? AreaSkillPackets() : AttackFixture::SkillPackets();
    }

private:
    SKILL_ATTRIBUTE* m_previousAttributes;
    MovementSkill m_previousMovementSkill;
    mu::ui::window::CSkillList* m_previousSkillList;
    mu::ui::window::COptionWindow* m_previousOption;
    bool m_previousFastInput;
    ActionSkillType m_skill = AT_SKILL_FIREBALL;
    std::unique_ptr<SKILL_ATTRIBUTE[]> m_attributes;
    mu::ui::window::CSkillList m_skillList;
    mu::ui::window::COptionWindow m_option;
};
} // namespace

TEST_CASE("Control skill issues one real spell with AutoAttack enabled [network][control-skill]")
{
    SkillCommandFixture fixture;
    bool autoAttack = true;
    SUBCASE("Fireball AutoAttack enabled") {}
    SUBCASE("Fireball AutoAttack disabled") { autoAttack = false; }
    SUBCASE("Rageful Blow 42 AutoAttack enabled") { fixture.UseSkill(AT_SKILL_RAGEFUL_BLOW); }
    SUBCASE("Rageful Blow 42 AutoAttack disabled")
    {
        fixture.UseSkill(AT_SKILL_RAGEFUL_BLOW);
        autoAttack = false;
    }
    SUBCASE("Fireball inherits a movement-skill mode")
    {
        Hero->MovementType = MOVEMENT_SKILL;
        autoAttack = false;
    }
    SUBCASE("Rageful Blow 42 inherits a movement-skill mode")
    {
        fixture.UseSkill(AT_SKILL_RAGEFUL_BLOW);
        Hero->MovementType = MOVEMENT_SKILL;
        autoAttack = false;
    }
    g_pOption->SetAutoAttack(autoAttack);
    auto act = fixture.Skill();
    std::string response;
    REQUIRE(act->Tick(response) == Control::Act::Status::Finished);
    REQUIRE(nlohmann::json::parse(response).at("ok") == true);
    REQUIRE(fixture.SkillPackets() == 1);
    const int castAction = Hero->Object.CurrentAction;
    CHECK(GameLogic::Automation::IsSwingInProgress());
    CHECK(Attacking == -1);
    CHECK(Hero->MovementType == MOVEMENT_MOVE);
    CHECK(Hero->Object.CurrentAction == castAction);
    CHECK(g_pOption->IsAutoAttack() == autoAttack);
    act.reset();

    Action(Hero, &Hero->Object, false);
    CHECK(fixture.SkillPackets() == 1);

    // Finish the animation, then run the real engine entry used by MoveHero.
    // The real packet collector, rather than the command ACK, counts casts.
    Hero->Object.CurrentAction = PLAYER_STOP_MALE;
    ::Attack(Hero);
    CHECK(fixture.SkillPackets() == 1);
    CHECK(g_pOption->IsAutoAttack() == autoAttack);
}

TEST_CASE("An interrupted control skill cancels its owned approach [network][control-skill]")
{
    SkillCommandFixture fixture;
    fixture.MoveOutOfRange(88);
    Control::Dispatcher dispatcher;
    dispatcher.Handle(fixture.SkillRequest(), 1);
    REQUIRE(dispatcher.OwesAnswer(1));
    REQUIRE(Hero->Movement);
    REQUIRE(Hero->Path.PathNum >= 2);
    REQUIRE(fixture.SkillPackets() == 0);
    SUBCASE("connection loss destroys the act") { dispatcher.AbandonConnection(1); }
    SUBCASE("a superseding act destroys the old one")
    {
        dispatcher.Handle(Control::Request::Parse(R"({"cmd":"move","id":2,"x":88,"y":113})"), 2);
    }
    SUBCASE("halt interrupts the act")
    {
        dispatcher.Handle(Control::Request::Parse(R"({"cmd":"halt","id":2})"), 2);
    }
    CHECK_FALSE(dispatcher.OwesAnswer(1));
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Path.PathNum == 0);
    CHECK(Hero->MovementType == MOVEMENT_MOVE);
    // Reaching a stale path's end must not dispatch an implicit skill.
    Action(Hero, &Hero->Object, false);
    CHECK(fixture.SkillPackets() == 0);
}

TEST_CASE("A control skill waiting for a ready animation cannot arm an engine cast [network][control-skill]")
{
    SkillCommandFixture fixture;
    Hero->Object.CurrentAction = PLAYER_WALK_MALE;
    auto act = fixture.Skill();
    std::string response;
    REQUIRE(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(fixture.SkillPackets() == 0);
    CHECK(Attacking == -1);
    CHECK(g_pOption->IsAutoAttack());
    Hero->Object.CurrentAction = PLAYER_STOP_MALE;
    ::Attack(Hero);
    CHECK(fixture.SkillPackets() == 0);
    act.reset();
}

TEST_CASE("A failed control skill clears its finished owned path without replacing the pose [network][control-skill]")
{
    SkillCommandFixture fixture;
    fixture.MoveOutOfRange(88);
    auto act = fixture.Skill();
    std::string response;
    REQUIRE(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Path.PathNum >= 2);
    Hero->Path.CurrentPath = Hero->Path.PathNum - 1;
    Hero->Movement = false;
    Hero->Run = 7;
    CharactersClient[GateSlot].Dead = 1;
    std::this_thread::sleep_for(260ms);
    REQUIRE(act->Tick(response) == Control::Act::Status::Finished);
    CHECK(nlohmann::json::parse(response).at("error") == "not_attackable");
    const auto pose = Hero->Object.CurrentAction;
    act.reset();
    CHECK(Hero->Path.PathNum == 0);
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Run == 7);
    CHECK(Hero->Object.CurrentAction == pose);
    CHECK(fixture.SkillPackets() == 0);
}

TEST_CASE("Halt stops an ordinary engine skill repeat without an active act [network][control-skill]")
{
    SkillCommandFixture fixture;
    SelectedCharacter = GateSlot;
    Attacking = 2;
    Hero->MovementType = MOVEMENT_SKILL;
    std::unique_ptr<Control::Act> noAct;
    const auto response = Control::Commands::Halt(Control::Request::Parse(R"({"cmd":"halt","id":2})"), noAct);
    REQUIRE(nlohmann::json::parse(response).at("ok") == true);
    ::Attack(Hero);
    CHECK(fixture.SkillPackets() == 0);
    CHECK(g_pOption->IsAutoAttack());
}

TEST_CASE("A control skill leaves a later manual path alone after relocation [network][control-skill]")
{
    SkillCommandFixture fixture;
    fixture.MoveOutOfRange(88);
    auto act = fixture.Skill();
    std::string response;
    REQUIRE(act->Tick(response) == Control::Act::Status::Running);
    REQUIRE(Hero->Movement);
    const auto pathSize = Hero->Path.PathNum;
    SUBCASE("another map") { gMapManager.WorldActive = WD_0LORENCIA; }
    SUBCASE("same map teleport") { Control::Events::NoteTeleport(); }
    Hero->Run = 7;
    REQUIRE(act->Tick(response) == Control::Act::Status::Finished);
    CHECK(nlohmann::json::parse(response).at("error") == "interrupted");
    act.reset();
    CHECK(Hero->Movement);
    CHECK(Hero->Path.PathNum == pathSize);
    CHECK(Hero->Run == 7);
    CHECK(fixture.SkillPackets() == 0);
}

TEST_CASE("Expired control skill cancels its pending approach [network][control-skill-timeout]" * doctest::skip(!TimeoutEnabled()))
{
    SkillCommandFixture fixture;
    fixture.MoveOutOfRange(88);
    Control::Dispatcher dispatcher;
    dispatcher.Handle(fixture.SkillRequest(), 1);
    REQUIRE(dispatcher.OwesAnswer(1));
    REQUIRE(Hero->Movement);
    std::this_thread::sleep_for(16s);
    dispatcher.Tick();
    CHECK_FALSE(dispatcher.OwesAnswer(1));
    CHECK_FALSE(Hero->Movement);
    CHECK(Hero->Path.PathNum == 0);
    CHECK(fixture.SkillPackets() == 0);
}
