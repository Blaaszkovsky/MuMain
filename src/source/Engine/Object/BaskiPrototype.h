#pragma once

class CHARACTER;

namespace Engine::Monsters
{
bool LoadBaskiPrototypeModel();
CHARACTER* CreateBaskiPrototype(int positionX, int positionY, int key);
} // namespace Engine::Monsters
