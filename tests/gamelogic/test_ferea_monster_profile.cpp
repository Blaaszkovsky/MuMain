#include "doctest.h"
// Match the existing headless world-model tests; the enum header needs these.
using BYTE = unsigned char;
#define DEFINE_ENUM_FLAG_OPERATORS(EnumType)
#include "Engine/Object/FereaMonsterProfile.h"
#undef DEFINE_ENUM_FLAG_OPERATORS

namespace Ferea = Engine::Monsters;

namespace
{
struct TestAction
{
    short NumAnimationKeys = 0;
    float PlaySpeed = -1.0f;
    bool Loop = false;
};

// Counts measured from Monster286-291 through the native v14 decoder.
constexpr std::array<std::array<short, 12>, 6> NativeKeys{{
    {8, 8, 8, 9, 9, 6, 13},
    {8, 8, 8, 11, 11, 6, 9},
    {8, 8, 8, 11, 11, 6, 8},
    {8, 8, 8, 11, 11, 6, 9},
    {8, 8, 8, 10, 25, 6, 9, 8, 15, 13, 8, 12},
    {15, 15, 15, 15, 15, 15, 15},
}};
}

TEST_CASE("All native Ferea layouts hold death, including the twelve-action Lord")
{
    for (std::size_t model = 0; model < Ferea::FereaProfiles.size(); ++model)
    {
        const auto& profile = Ferea::FereaProfiles[model];
        std::array<TestAction, 12> storage{};
        const auto actions = std::span(storage).first(profile.actionCount);
        for (std::size_t action = 0; action < actions.size(); ++action)
            actions[action].NumAnimationKeys = NativeKeys[model][action];
        REQUIRE(Ferea::ConfigureFereaActions(actions, profile));
        CHECK(actions[MONSTER01_DIE].Loop);
        for (std::size_t action = 0; action < actions.size(); ++action)
        {
            CHECK(actions[action].PlaySpeed > 0.0f);
            CHECK(actions[action].Loop == (action == MONSTER01_DIE));
        }
        CHECK(actions[MONSTER01_SHOCK].PlaySpeed > actions[MONSTER01_STOP1].PlaySpeed);
        CHECK(actions[MONSTER01_ATTACK1].NumAnimationKeys > 5);
        CHECK(actions[MONSTER01_ATTACK2].NumAnimationKeys > 5);
    }
}

TEST_CASE("Ferea rejects missing, truncated and single-frame clips before modifying playback")
{
    const auto& profile = Ferea::FereaProfiles[0];
    std::array<TestAction, 7> actions{};
    for (auto& action : actions)
        action.NumAnimationKeys = 8;
    CHECK_FALSE(Ferea::ConfigureFereaActions(std::span(actions).first(6), profile));
    for (const short invalidKeys : {short(0), short(1), short(-1)})
    {
        actions[MONSTER01_DIE].NumAnimationKeys = invalidKeys;
        CHECK_FALSE(Ferea::ConfigureFereaActions(std::span(actions), profile));
        CHECK(actions[MONSTER01_STOP1].PlaySpeed == -1.0f);
    }
}

TEST_CASE("General and Orb fit the native geometry without changing ordinary Ferea models")
{
    const auto& general = Ferea::FereaProfiles[3];
    const auto& lord = Ferea::FereaProfiles[4];
    const auto& orb = Ferea::FereaProfiles[5];
    CHECK(general.monster == MONSTER_FEREA_GENERAL);
    // Independently measured native idle mesh heights, in world units.
    CHECK(116.64f * general.scale > 439.88f * lord.scale);
    CHECK(116.64f * general.scale < 439.88f * lord.scale * 1.1f);
    CHECK(Ferea::FereaOrbHeight - 38.0f * orb.scale > 0.0f);
    for (int ordinary = 0; ordinary < 3; ++ordinary)
    {
        CHECK(Ferea::FereaProfiles[ordinary].scale == 1.0f);
    }
}
