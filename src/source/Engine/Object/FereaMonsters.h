#pragma once

#include "Core/Globals/_enum.h"

class CHARACTER;

namespace Engine::Monsters
{
bool LoadFereaMonsterModel(EMonsterModelType modelType);
CHARACTER* CreateFereaMonster(EMonsterType monsterType, int positionX, int positionY, int key);
} // namespace Engine::Monsters
