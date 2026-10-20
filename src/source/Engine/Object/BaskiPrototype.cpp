#include "stdafx.h"
#include "Engine/Object/BaskiPrototype.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/DataHandler/LoadData.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Monsters/BaskiPrototypeProfile.h"
#include "Render/Models/ZzzBMD.h"

#include <array>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace Profile = GameLogic::Monsters::BaskiPrototypeProfile;

static_assert(MONSTER_BASKI_PROTOTYPE == Profile::MonsterNumber);
static_assert(MONSTER_MODEL_BASKI_PROTOTYPE == Profile::ModelOffset);
static_assert(MONSTER01_DIE == Profile::DeathAction);
static_assert(MONSTER01_STOP1 == 0 && MONSTER01_STOP2 == 1 && MONSTER01_WALK == 2);
static_assert(MONSTER01_ATTACK1 == 3 && MONSTER01_ATTACK2 == 4 && MONSTER01_SHOCK == 5);
static_assert(MODEL_BASKI_PROTOTYPE < MODEL_MONSTER_END);

namespace
{
bool IsEnabled()
{
    const char* value = std::getenv("MU_BASKI_PROTOTYPE");
    return value != nullptr && std::string_view(value) == "1";
}

bool ConfigurePlayback(BMD& model)
{
    if (model.NumActions != Profile::PlaySpeeds.size())
        return false;
    std::array<short, Profile::PlaySpeeds.size()> keys{};
    for (std::size_t action = 0; action < keys.size(); ++action)
        keys[action] = model.Actions[action].NumAnimationKeys;
    if (!Profile::HasRequiredActions(keys))
        return false;
    for (std::size_t action = 0; action < keys.size(); ++action)
    {
        model.Actions[action].PlaySpeed = Profile::PlaySpeeds[action];
        model.Actions[action].Loop = action == Profile::DeathAction;
    }
    return true;
}

bool LoadTextures()
{
    const std::wstring folder = L"Monster\\";
    std::vector<TextureProblem> problems;
    gLoadData.OpenTexture(MODEL_BASKI_PROTOTYPE, std::span<const std::wstring>(&folder, 1), problems);
    if (problems.empty())
        return true;
    MU_LOG_ERROR(mu::log::Get("data"), "Baski prototype has {} missing textures", problems.size());
    return false;
}
} // namespace

namespace Engine::Monsters
{
bool LoadBaskiPrototypeModel()
{
    if (!IsEnabled())
        return false;
    BMD& model = Models[MODEL_BASKI_PROTOTYPE];
    if (model.NumMeshs > 0)
        return ConfigurePlayback(model);
    if (!gLoadData.AccessModel(MODEL_BASKI_PROTOTYPE, L"Data\\Monster\\", L"monster300"))
        return false;
    if (model.NumMeshs <= 0 || !ConfigurePlayback(model))
    {
        MU_LOG_ERROR(mu::log::Get("data"), "Baski prototype requires seven nonempty actions");
        model.Release();
        return false;
    }
    if (!LoadTextures())
    {
        model.Release();
        return false;
    }
    MU_LOG_INFO(mu::log::Get("data"), "Baski prototype Open2 version={} monster={} model={} actions={}",
                static_cast<int>(model.Version), Profile::MonsterNumber, static_cast<int>(MODEL_BASKI_PROTOTYPE), model.NumActions);
    return true;
}

CHARACTER* CreateBaskiPrototype(int positionX, int positionY, int key)
{
    if (!LoadBaskiPrototypeModel())
        return nullptr;
    CHARACTER* character = CreateCharacter(key, MODEL_BASKI_PROTOTYPE, positionX, positionY);
    if (character == nullptr)
        return nullptr;
    wcscpy_s(character->ID, L"Baski (prototype)");
    character->Object.Scale = Profile::Scale;
    character->Weapon[0].Type = -1;
    character->Weapon[1].Type = -1;
    return character;
}
} // namespace Engine::Monsters
