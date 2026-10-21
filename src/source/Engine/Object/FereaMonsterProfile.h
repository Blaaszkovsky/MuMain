#pragma once

#include "Core/Globals/_enum.h"

#include <algorithm>
#include <array>
#include <span>

namespace Engine::Monsters
{
struct FereaMonsterProfile
{
    EMonsterType monster;
    EMonsterModelType model;
    const wchar_t* fileStem;
    const wchar_t* displayName;
    int actionCount;
    float scale;
};

// General's native mesh is 116 units tall; x4 puts it beside the 440-unit Lord.
// Orb's native radius is 31 units and its origin is at the sphere's centre.
inline constexpr float FereaGeneralScale = 4.0f;
inline constexpr float FereaOrbScale = 2.0f;
inline constexpr float FereaOrbHeight = 80.0f;
inline constexpr std::array FereaProfiles{
    FereaMonsterProfile{MONSTER_FEREA_KNIGHT, MONSTER_MODEL_FEREA_KNIGHT, L"Monster286", L"Ferea Knight", 7, 1.0f},
    FereaMonsterProfile{MONSTER_FEREA_ARCHER, MONSTER_MODEL_FEREA_ARCHER, L"Monster287", L"Ferea Archer", 7, 1.0f},
    FereaMonsterProfile{MONSTER_FEREA_FIGHTER, MONSTER_MODEL_FEREA_FIGHTER, L"Monster288", L"Ferea Fighter", 7, 1.0f},
    FereaMonsterProfile{MONSTER_FEREA_GENERAL, MONSTER_MODEL_FEREA_GENERAL, L"Monster289", L"Ferea General", 7, FereaGeneralScale},
    FereaMonsterProfile{MONSTER_LORD_OF_FEREA, MONSTER_MODEL_LORD_OF_FEREA, L"Monster290", L"Lord of Ferea", 12, 1.0f},
    FereaMonsterProfile{MONSTER_FEREA_CRYSTAL_ORB, MONSTER_MODEL_FEREA_CRYSTAL_ORB, L"Monster291", L"Ferea Crystal Orb", 7, FereaOrbScale},
};

// Neutral S6 playback, shared by all six native layouts. Lord's extra clips
// retain the default speed until a server skill has an agreed visual mapping.
inline constexpr std::array FereaPlaySpeeds{0.25f, 0.2f, 0.34f, 0.33f, 0.33f, 0.5f, 0.55f};
inline constexpr float FereaExtraActionSpeed = 0.3f;

// Action_t is supplied by the renderer. Keeping this independent of BMD lets
// the complete playback setup be exercised without starting a GPU or server.
template <typename Action, std::size_t Extent>
bool ConfigureFereaActions(std::span<Action, Extent> actions, const FereaMonsterProfile& profile)
{
    if (actions.size() != static_cast<std::size_t>(profile.actionCount)
        || std::any_of(actions.begin(), actions.end(), [](const auto& action) { return action.NumAnimationKeys <= 1; }))
        return false;
    for (std::size_t action = 0; action < actions.size(); ++action)
    {
        actions[action].PlaySpeed = action < FereaPlaySpeeds.size() ? FereaPlaySpeeds[action] : FereaExtraActionSpeed;
        // BMD's Loop flag holds the final frame, rather than wrapping it.
        actions[action].Loop = action == MONSTER01_DIE;
    }
    return true;
}
} // namespace Engine::Monsters
