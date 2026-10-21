#pragma once

#include "Core/Globals/_enum.h"

class CHARACTER;
class OBJECT;

namespace Engine::Monsters
{
bool LoadFereaMonsterModel(EMonsterModelType modelType);
CHARACTER* CreateFereaMonster(EMonsterType monsterType, int positionX, int positionY, int key);
void UpdateFereaMonsterHeight(OBJECT* object);
void MoveFereaMonsterVisual(CHARACTER* character);
} // namespace Engine::Monsters
