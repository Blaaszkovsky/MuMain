// Calls the production tap and production event resolver.
#include "stdafx.h"
#include <doctest.h>

#include "App/Control/ControlEvents.h"
#include "App/Control/ControlCommands.h"
#include "App/Control/ControlObjects.h"
#include "App/Control/ControlTaps.h"
#include "GameLogic/Automation/Attack.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "Network/Server/WSclient.h"
#include "Render/Models/ZzzBMD.h"
#include "Scenes/SceneCore.h"
#include "UI/Options/OptionWindow.h"
#include "json.hpp"

#include <algorithm>
#include <cwchar>
#include <limits>
#include <memory>

// Supply the existing option input without opening a window, as the skill fixture does.
#define private public
#include "UI/Core/WindowSystem.h"
#undef private

extern int AttackPlayer;
void ReceiveAttackDamageExtended(const BYTE* receiveBuffer);

namespace
{
namespace Events = App::Control::Events;
constexpr int HeroId = 512;
constexpr int StaleNpcSlot = 1;
constexpr int StaleNpcId = 541;
constexpr int OtherSlot = 2;
constexpr int OtherId = 524;

class TapFixture
{
public:
    TapFixture()
        : m_previousHero(Hero), m_previousCharacters(CharactersClient), m_previousAttribute(CharacterAttribute),
          m_previousHeroKey(HeroKey), m_previousAttackPlayer(AttackPlayer), m_previousTarget(ActionTarget),
          m_enabled(Events::IsEnabled()), m_characters(std::make_unique<CHARACTER[]>(MAX_CHARACTERS_CLIENT))
    {
        CharactersClient = m_characters.get();
        Hero = &CharactersClient[0];
        HeroKey = HeroId;
        Hero->Key = HeroId;
        Hero->Object.Live = true;
        Hero->Object.Kind = KIND_PLAYER;
        Hero->Object.CurrentAction = PLAYER_STOP_MALE;
        std::wcscpy(Hero->ID, L"GDB00");
        auto& npc = CharactersClient[StaleNpcSlot];
        npc.Key = StaleNpcId;
        npc.Object.Live = true;
        npc.Object.Kind = KIND_NPC;
        std::wcscpy(npc.ID, L"Slingshot");
        auto& other = CharactersClient[OtherSlot];
        other.Key = OtherId;
        other.Object.Live = true;
        other.Object.Kind = KIND_PLAYER;
        std::wcscpy(other.ID, L"GDA00");
        AttackPlayer = StaleNpcSlot;
        ActionTarget = -1;
        CharacterAttribute = &m_attribute;
        m_attribute.Life = 2795;
        m_attribute.LifeMax = 2899;
        m_attribute.Shield = 8617;
        m_attribute.ShieldMax = 8641;
        Events::Reset();
        Events::SetEnabled(true);
        Events::SetObjectResolver(App::Control::DescribeGameObject);
    }

    ~TapFixture()
    {
        Events::SetObjectResolver({});
        Events::SetEnabled(m_enabled);
        Events::Reset();
        Hero = m_previousHero;
        CharactersClient = m_previousCharacters;
        CharacterAttribute = m_previousAttribute;
        HeroKey = m_previousHeroKey;
        AttackPlayer = m_previousAttackPlayer;
        ActionTarget = m_previousTarget;
    }

private:
    CHARACTER* m_previousHero;
    CHARACTER* m_previousCharacters;
    CHARACTER_ATTRIBUTE* m_previousAttribute;
    int m_previousHeroKey, m_previousAttackPlayer, m_previousTarget;
    bool m_enabled;
    std::unique_ptr<CHARACTER[]> m_characters;
    CHARACTER_ATTRIBUTE m_attribute{};
};

std::vector<Events::Record> AttributedRecords()
{
    auto records = Events::Since(0);
    std::erase_if(records, [](const auto& record) { return record.name == "damage_observed"; });
    return records;
}
} // namespace

TEST_CASE("Production damage tap never names stale NPC as incoming attacker [network][control-damage]")
{
    TapFixture fixture;
    Events::RecordAttackDamage(AttackPlayer, HeroKey, 2, 24, 2);
    const auto records = AttributedRecords();
    REQUIRE(records.size() == 5);
    REQUIRE(records.front().name == "hit");
    const auto hit = nlohmann::json::parse(records.front().fields);
    CHECK(hit["direction"] == "received");
    CHECK(hit["attacker"]["id"] == -1);
    CHECK(hit["attacker"]["kind"] == "unknown");
    CHECK(hit["attacker"]["name"] == "");
    CHECK(hit["target"]["id"] == HeroId);
    CHECK(hit["target"]["name"] == "GDB00");
    CHECK(hit["damage"] == 2);
    CHECK(hit["shield_damage"] == 24);
    CHECK(hit["critical"] == true);
    CHECK(hit["missed"] == false);
    CHECK(AttackPlayer == StaleNpcSlot);
    const auto life = nlohmann::json::parse(records[1].fields);
    const auto sd = nlohmann::json::parse(records[3].fields);
    CHECK(life["stat"] == "life");
    CHECK(life["value"] == 2795);
    CHECK(sd["stat"] == "sd");
    CHECK(sd["value"] == 8617);
}

TEST_CASE("Production damage tap retains outgoing swing inference with stale NPC action [network][control-damage]")
{
    TapFixture fixture;
    Hero->Object.CurrentAction = PLAYER_ATTACK_FIST;
    ActionTarget = OtherSlot;
    Events::RecordAttackDamage(AttackPlayer, OtherId, 2, 24, 0);
    const auto records = AttributedRecords();
    REQUIRE(records.size() == 1);
    const auto hit = nlohmann::json::parse(records.front().fields);
    CHECK(hit["direction"] == "dealt");
    CHECK(hit["attacker"]["id"] == HeroId);
    CHECK(hit["target"]["id"] == OtherId);
    CHECK(hit["damage"] == 2);
    CHECK(hit["shield_damage"] == 24);
    CHECK(AttackPlayer == StaleNpcSlot);
}

TEST_CASE("Production damage tap ignores a stale outgoing target outside its swing [network][control-damage]")
{
    TapFixture fixture;
    ActionTarget = OtherSlot;
    Events::RecordAttackDamage(AttackPlayer, OtherId, 7, 9, 0);
    CHECK(AttributedRecords().empty());
}

TEST_CASE("Production damage observation survives an unowned skill target [network][control-damage]")
{
    TapFixture fixture;
    Hero->Object.CurrentAction = PLAYER_ATTACK_SKILL_FURY_STRIKE;
    SUBCASE("unset action target") { ActionTarget = -1; }
    SUBCASE("stale action target") { ActionTarget = StaleNpcSlot; }

    Events::RecordAttackDamage(AttackPlayer, OtherId, 7, 9, 2);
    const auto records = Events::Since(0);
    REQUIRE(records.size() == 1);
    CHECK(records.front().name == "damage_observed");
    const auto fields = nlohmann::json::parse(records.front().fields);
    CHECK(fields["target"]["id"] == OtherId);
    CHECK(fields["damage"] == 7);
    CHECK(fields["shield_damage"] == 9);
    CHECK(fields["attacker"]["id"] == -1);
    CHECK(fields["cause"] == "unknown");
    CHECK(AttributedRecords().empty());
}

TEST_CASE("Production damage observation survives a real HALT [network][control-damage]")
{
    TapFixture fixture;
    const auto previousScene = SceneFlag;
    auto* const previousModels = Models;
    const int previousAttacking = Attacking;
    BMD model;
    model.NumActions = MONSTER01_STOP1 + 1;
    std::fill(std::begin(model.Sounds), std::end(model.Sounds), static_cast<short>(-1));
    Models = &model;
    SceneFlag = MAIN_SCENE;
    Hero->Object.Type = 0;
    Hero->Object.CurrentAction = PLAYER_ATTACK_SKILL_FURY_STRIKE;
    ActionTarget = OtherSlot;
    Attacking = 2;
    std::unique_ptr<App::Control::Act> act;
    const auto request = App::Control::Request::Parse(R"({"cmd":"halt","id":1})");
    const auto response = App::Control::Commands::Halt(request, act);
    // Restore globals before assertions, including the fixture's failure paths.
    SceneFlag = previousScene;
    Models = previousModels;
    CHECK(response.find("\"ok\":true") != std::string::npos);
    CHECK(Attacking == -1);
    CHECK_FALSE(GameLogic::Automation::IsSwingInProgress());
    Attacking = previousAttacking;

    Events::RecordAttackDamage(AttackPlayer, OtherId, 7, 9, 0);
    const auto records = Events::Since(0);
    REQUIRE(records.size() == 1);
    CHECK(records.front().name == "damage_observed");
    CHECK(AttributedRecords().empty());
}

TEST_CASE("Production extended damage observation preserves decoded fields exactly once [network][control-damage]")
{
    TapFixture fixture;
    constexpr auto MaximumDamage = std::numeric_limits<std::uint32_t>::max();
    Events::RecordAttackDamageExtended(AttackPlayer, OtherId, MaximumDamage, MaximumDamage, 3, 255, 0);
    const auto records = Events::Since(0);
    REQUIRE(records.size() == 1);
    CHECK(records.front().name == "damage_observed");
    const auto fields = nlohmann::json::parse(records.front().fields);
    CHECK(fields["target"]["id"] == OtherId);
    CHECK(fields["target"]["known"] == true);
    CHECK(fields["target"]["name"] == "GDA00");
    CHECK(fields["damage"] == MaximumDamage);
    CHECK(fields["shield_damage"] == MaximumDamage);
    CHECK(fields["damage_type"] == 3);
    CHECK(fields["health_status"] == 255);
    CHECK(fields["shield_status"] == 0);
    CHECK(fields["attacker"]["id"] == -1);
    CHECK(fields["cause"] == "unknown");
    CHECK_FALSE(fields.contains("direction"));
    CHECK(AttackPlayer == StaleNpcSlot);
    CHECK(ActionTarget == -1);
}

TEST_CASE("Production extended zero damage observation preserves ordinary attribution [network][control-damage]")
{
    TapFixture fixture;
    int target = OtherId;
    SUBCASE("incoming") { target = HeroId; }
    SUBCASE("outgoing") { AttackPlayer = 0; }
    Events::RecordAttackDamageExtended(AttackPlayer, target, 0, 0, 0, 250, 255);
    const auto records = Events::Since(0);
    REQUIRE_FALSE(records.empty());
    CHECK(std::count_if(records.begin(), records.end(), [](const auto& record) {
        return record.name == "damage_observed";
    }) == 1);
    const auto observed = nlohmann::json::parse(records.front().fields);
    CHECK(observed["target"]["id"] == target);
    CHECK(observed["damage"] == 0);
    CHECK(observed["shield_damage"] == 0);
    CHECK(observed["health_status"] == 250);
    const auto attributed = AttributedRecords();
    REQUIRE_FALSE(attributed.empty());
    const auto hit = nlohmann::json::parse(attributed.front().fields);
    CHECK(hit["direction"] == (target == HeroId ? "received" : "dealt"));
    CHECK(hit["missed"] == true);
    CHECK(observed["attacker"]["id"] == -1);
}

TEST_CASE("Production damage observation is absent while control recording is disabled [network][control-damage]")
{
    TapFixture fixture;
    Events::SetEnabled(false);
    Events::RecordAttackDamage(AttackPlayer, OtherId, 7, 9, 0);
    Events::RecordAttackDamageExtended(AttackPlayer, OtherId, 7, 9, 0, 250, 250);
    CHECK(Events::LastSequence() == 0);
    CHECK(Events::Since(0).empty());
}

TEST_CASE("Production extended receive records its decoded status before attribution loss [network][control-damage]")
{
    TapFixture fixture;
    auto* const system = mu::ui::window::CSystem::GetInstance();
    auto* const previousOption = g_pOption;
    const int previousMap = gMapManager.WorldActive;
    mu::ui::window::COptionWindow option;
    option.SetRenderAllEffects(false);
    system->m_pNewOptionWindow = &option;
    gMapManager.WorldActive = WD_0LORENCIA;
    // SetPlayerShock leaves this existing pose alone; number effects are off.
    CharactersClient[OtherSlot].Object.CurrentAction = PLAYER_ATTACK_SKILL_FURY_STRIKE;
    PRECEIVE_ATTACK_EXTENDED packet{};
    packet.TargetId = OtherId;
    packet.HealthDamage = 7;
    packet.ShieldDamage = 9;
    packet.DamageType = 2;
    packet.HealthStatus = 230;
    packet.ShieldStatus = 245;
    SUBCASE("independent known health and shield") {}
    SUBCASE("unknown health with known shield") { packet.HealthStatus = 255; }
    SUBCASE("known health with unknown shield") { packet.ShieldStatus = 255; }
    SUBCASE("empty health and shield") { packet.HealthStatus = packet.ShieldStatus = 0; }
    ReceiveAttackDamageExtended(reinterpret_cast<const BYTE*>(&packet));
    system->m_pNewOptionWindow = previousOption;
    gMapManager.WorldActive = previousMap;

    const auto records = Events::Since(0);
    REQUIRE(records.size() == 1);
    CHECK(records.front().name == "damage_observed");
    const auto fields = nlohmann::json::parse(records.front().fields);
    CHECK(fields["target"]["id"] == OtherId);
    CHECK(fields["damage"] == packet.HealthDamage);
    CHECK(fields["shield_damage"] == packet.ShieldDamage);
    CHECK(fields["damage_type"] == packet.DamageType);
    CHECK(fields["health_status"] == packet.HealthStatus);
    CHECK(fields["shield_status"] == packet.ShieldStatus);
    CHECK(fields["attacker"]["id"] == -1);
    const float expectedHealth = packet.HealthStatus == 255 ? -1.0f : packet.HealthStatus / 250.0f;
    const float expectedShield = packet.ShieldStatus == 255 ? -1.0f : packet.ShieldStatus / 250.0f;
    CHECK(CharactersClient[OtherSlot].HealthStatus == doctest::Approx(expectedHealth));
    CHECK(CharactersClient[OtherSlot].ShieldStatus == doctest::Approx(expectedShield));
    CHECK(AttributedRecords().empty());
}
