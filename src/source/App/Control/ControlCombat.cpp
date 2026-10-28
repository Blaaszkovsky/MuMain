#include "stdafx.h"
#include "App/Control/ControlCombat.h"

#include "App/Control/ControlEvents.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Network/Server/WSclient.h"
#include "Scenes/SceneCore.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <mutex>

extern int TargetX;
extern int TargetY;

namespace
{
void ClearWalkingPath()
{
    std::lock_guard lock(Hero->Path.Lock);
    Hero->Path.PathNum = 0;
    Hero->Path.CurrentPath = 0;
    Hero->Path.CurrentPathFloat = 0;
}
} // namespace

namespace App::Control
{
void StopWalking()
{
    if (SceneFlag != MAIN_SCENE || Hero == nullptr)
        return;

    ClearWalkingPath();
    // An empty path never reports arrival to MoveHero.
    Hero->Movement = false;
    if (Hero->Dead == 0)
        SetPlayerStop(Hero);
}

void DisarmCombatRepeat()
{
    if (SceneFlag != MAIN_SCENE || Hero == nullptr)
        return;

    Attacking = -1;
    if (Hero->MovementType == MOVEMENT_ATTACK || Hero->MovementType == MOVEMENT_SKILL)
        Hero->MovementType = MOVEMENT_MOVE;
    // Preserve the issued animation, its timing and target for damage attribution.
}

OwnedApproach::OwnedApproach(int targetKey)
    : m_targetKey(targetKey), m_fromMap(gMapManager.WorldActive), m_teleportsBefore(Events::TeleportCount())
{
}

OwnedApproach::~OwnedApproach()
{
    Stop();
}

bool OwnedApproach::WasRelocated() const
{
    return gMapManager.WorldActive != m_fromMap || Events::TeleportCount() != m_teleportsBefore;
}

void OwnedApproach::Remember()
{
    m_hero = Hero;
    m_heroKey = HeroKey;
    m_map = gMapManager.WorldActive;
    m_targetIndex = FindCharacterIndex(m_targetKey);
    m_x = TargetX;
    m_y = TargetY;
    std::lock_guard lock(Hero->Path.Lock);
    m_pathSize = Hero->Path.PathNum;
    std::copy_n(Hero->Path.PathX, m_pathSize, m_pathX.begin());
    std::copy_n(Hero->Path.PathY, m_pathSize, m_pathY.begin());
}

void OwnedApproach::Forget()
{
    m_hero = nullptr;
}

void OwnedApproach::Stop()
{
    if (WasRelocated() || SceneFlag != MAIN_SCENE || Hero == nullptr || Hero != m_hero || HeroKey != m_heroKey ||
        gMapManager.WorldActive != m_map || Hero->MovementType != MOVEMENT_MOVE || TargetX != m_x || TargetY != m_y ||
        FindCharacterIndex(m_targetKey) != m_targetIndex)
        return;

    // shortcut: no engine intent generation exists. An identical manual
    // replay of this exact path cannot be distinguished from our approach.
    bool samePath;
    {
        std::lock_guard lock(Hero->Path.Lock);
        samePath = Hero->Path.PathNum == m_pathSize &&
                   std::equal(m_pathX.begin(), m_pathX.begin() + m_pathSize, Hero->Path.PathX) &&
                   std::equal(m_pathY.begin(), m_pathY.begin() + m_pathSize, Hero->Path.PathY);
    }
    if (!samePath)
        return;

    // A queued path after SendMove refusal/arrival does not own a later pose.
    if (Hero->Movement)
        StopWalking();
    else
        ClearWalkingPath();
}
} // namespace App::Control
