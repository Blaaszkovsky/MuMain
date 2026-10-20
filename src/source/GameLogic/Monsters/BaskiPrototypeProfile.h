#pragma once

#include <algorithm>
#include <array>
#include <span>

namespace GameLogic::Monsters::BaskiPrototypeProfile
{
// Private integration-test identity, not an original Season 16 monster number.
inline constexpr int MonsterNumber = 900;
inline constexpr int ModelOffset = 217;
inline constexpr int DeathAction = 6;
inline constexpr float Scale = 1.0f;
// Neutral S6 playback values for validation, independent of Spider's overrides.
inline constexpr std::array PlaySpeeds{0.25f, 0.2f, 0.34f, 0.33f, 0.33f, 0.5f, 0.55f};

constexpr bool HasRequiredActions(std::span<const short> animationKeys)
{
    return animationKeys.size() == PlaySpeeds.size()
        && std::all_of(animationKeys.begin(), animationKeys.end(), [](short keys) { return keys > 0; });
}
} // namespace GameLogic::Monsters::BaskiPrototypeProfile
