#include "stdafx.h"
#include "Engine/Object/FereaMonsters.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/DataHandler/LoadData.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Models/ZzzBMD.h"

#include <array>
#include <span>
#include <vector>

namespace
{
struct FereaMonsterProfile
{
    EMonsterType monster;
    EMonsterModelType model;
    const wchar_t* fileStem;
    const wchar_t* displayName;
    int actionCount;
};

// The model files are independently identified by their embedded model and texture names.
constexpr std::array Profiles{
    FereaMonsterProfile{MONSTER_FEREA_KNIGHT, MONSTER_MODEL_FEREA_KNIGHT, L"Monster286", L"Ferea Knight", 7},
    FereaMonsterProfile{MONSTER_FEREA_ARCHER, MONSTER_MODEL_FEREA_ARCHER, L"Monster287", L"Ferea Archer", 7},
    FereaMonsterProfile{MONSTER_FEREA_FIGHTER, MONSTER_MODEL_FEREA_FIGHTER, L"Monster288", L"Ferea Fighter", 7},
    FereaMonsterProfile{MONSTER_FEREA_GENERAL, MONSTER_MODEL_FEREA_GENERAL, L"Monster289", L"Ferea General", 7},
    FereaMonsterProfile{MONSTER_LORD_OF_FEREA, MONSTER_MODEL_LORD_OF_FEREA, L"Monster290", L"Lord of Ferea", 12},
    FereaMonsterProfile{MONSTER_FEREA_CRYSTAL_ORB, MONSTER_MODEL_FEREA_CRYSTAL_ORB, L"Monster291", L"Ferea Crystal Orb", 7},
};

const FereaMonsterProfile* FindProfile(EMonsterModelType model)
{
    for (const auto& profile : Profiles)
        if (profile.model == model)
            return &profile;
    return nullptr;
}

const FereaMonsterProfile* FindProfile(EMonsterType monster)
{
    for (const auto& profile : Profiles)
        if (profile.monster == monster)
            return &profile;
    return nullptr;
}

bool ConfigurePlayback(BMD& model, const FereaMonsterProfile& profile)
{
    if (model.NumActions != profile.actionCount)
        return false;
    for (int action = 0; action < model.NumActions; ++action)
    {
        if (model.Actions[action].NumAnimationKeys <= 0)
            return false;
        model.Actions[action].PlaySpeed = 0.3f;
        model.Actions[action].Loop = action == MONSTER01_DIE && profile.actionCount == 7;
    }
    return true;
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
    character->Object.Scale = 1.0f;
    character->Weapon[0].Type = -1;
    character->Weapon[1].Type = -1;
    return character;
}
} // namespace Engine::Monsters
