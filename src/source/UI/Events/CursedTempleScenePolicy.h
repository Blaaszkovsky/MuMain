#pragma once

#include "Core/Globals/_define.h"

namespace UI::CursedTemple
{
// Network event updates belong to the selected character's world session.
constexpr bool CanApplyNetworkUpdate(EGameScene scene)
{
    return scene == MAIN_SCENE;
}
}
