#include "stdafx.h"
#include "Engine/Object/FereaMonsters.h"
#include "Engine/Object/FereaMonsterProfile.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/DataHandler/LoadData.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <cmath>
#include <span>
#include <vector>

namespace
{
using Engine::Monsters::FereaMonsterProfile;
using Engine::Monsters::FereaProfiles;

const FereaMonsterProfile* FindProfile(EMonsterModelType model)
{
    for (const auto& profile : FereaProfiles)
        if (profile.model == model)
            return &profile;
    return nullptr;
}

const FereaMonsterProfile* FindProfile(EMonsterType monster)
{
    for (const auto& profile : FereaProfiles)
        if (profile.monster == monster)
            return &profile;
    return nullptr;
}

bool ConfigurePlayback(BMD& model, const FereaMonsterProfile& profile)
{
    if (model.Actions == nullptr || model.NumActions <= 0)
        return false;
    return Engine::Monsters::ConfigureFereaActions(std::span(model.Actions, model.NumActions), profile);
}
} // namespace

namespace Engine::Monsters
{
bool LoadFereaMonsterModel(EMonsterModelType modelType)
{
    const auto* profile = FindProfile(modelType);
    if (profile == nullptr)
        return false;

    const int slot = MODEL_MONSTER01 + static_cast<int>(modelType);
    static_assert(MODEL_FEREA_CRYSTAL_ORB < MODEL_MONSTER_END);
    BMD& model = Models[slot];
    if (model.NumMeshs > 0)
        return ConfigurePlayback(model, *profile);
    if (!gLoadData.AccessModel(slot, L"Data\\Monster\\", profile->fileStem))
        return false;
    if (model.NumMeshs <= 0 || !ConfigurePlayback(model, *profile))
    {
        MU_LOG_ERROR(mu::log::Get("data"), "Ferea monster model has invalid actions: {}", static_cast<int>(modelType));
        model.Release();
        return false;
    }
    const std::wstring folder = L"Monster\\";
    std::vector<TextureProblem> problems;
    gLoadData.OpenTexture(slot, std::span<const std::wstring>(&folder, 1), problems);
    if (!problems.empty())
    {
        MU_LOG_ERROR(mu::log::Get("data"), "Ferea monster model {} has {} missing textures",
                     static_cast<int>(modelType), problems.size());
        model.Release();
        return false;
    }
    return true;
}

CHARACTER* CreateFereaMonster(EMonsterType monsterType, int positionX, int positionY, int key)
{
    const auto* profile = FindProfile(monsterType);
    if (profile == nullptr || !LoadFereaMonsterModel(profile->model))
        return nullptr;
    CHARACTER* character = CreateCharacter(key, MODEL_MONSTER01 + static_cast<int>(profile->model), positionX, positionY);
    if (character == nullptr)
        return nullptr;
    wcscpy_s(character->ID, profile->displayName);
    character->Object.Scale = profile->scale;
    UpdateFereaMonsterHeight(&character->Object);
    character->Weapon[0].Type = -1;
    character->Weapon[1].Type = -1;
    return character;
}

void UpdateFereaMonsterHeight(OBJECT* object)
{
    if (object->Type != MODEL_FEREA_CRYSTAL_ORB)
        return;
    object->Position[2] = RequestTerrainHeight(object->Position[0], object->Position[1]) + FereaOrbHeight;
}

void MoveFereaMonsterVisual(CHARACTER* character)
{
    OBJECT* object = &character->Object;
    if (object->Type != MODEL_FEREA_CRYSTAL_ORB || !object->Live || !object->Visible)
        return;

    constexpr float PulseTimeScaleMs = 400.0f;
    constexpr float GlowScale = 2.5f;
    constexpr float PulseScale = 0.25f;
    constexpr float HitGlowScale = 3.5f;
    constexpr vec3_t HitLight = {1.0f, 0.3f, 0.1f};
    constexpr vec3_t LivingLight = {0.35f, 0.65f, 1.0f};
    vec3_t light;
    // Native Orb hit/death clips duplicate its idle clip. These colours expose
    // the existing combat actions without guessing a boss phase or buff state.
    if (object->CurrentAction == MONSTER01_SHOCK)
    {
        VectorCopy(HitLight, light);
    }
    else
    {
        VectorCopy(LivingLight, light);
    }
    for (float& channel : light)
        channel *= object->Alpha;
    const float scale = object->CurrentAction == MONSTER01_SHOCK
        ? HitGlowScale : GlowScale + PulseScale * static_cast<float>(std::sin(WorldTime / PulseTimeScaleMs));
    CreateSprite(BITMAP_LIGHT, object->Position, scale, light, object);
}
} // namespace Engine::Monsters
