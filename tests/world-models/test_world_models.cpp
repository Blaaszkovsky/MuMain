#include <cstdlib>
#include <iostream>
#include <vector>

// The enum header uses the platform's byte alias; no platform SDK is needed.
using BYTE = unsigned char;
#define DEFINE_ENUM_FLAG_OPERATORS(EnumType)
#include "Render/Models/WorldModelSlots.h"
#undef DEFINE_ENUM_FLAG_OPERATORS

namespace
{
using namespace Render::Models;

struct TestObject
{
    int Type = 0;
    Render::Models::ModelDomain ModelDomain = Render::Models::ModelDomain::Global;
};

struct TestModel
{
    int mesh = 0;
    int releases = 0;
    void Release() { mesh = 0; ++releases; }
};

void Require(bool condition, const char* reason)
{
    if (!condition)
    {
        std::cerr << reason << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void CheckIdentity()
{
    static_assert(MODEL_LOGO == 160);
    static_assert(EditorPreviewSlot == MAX_MODELS);
    static_assert(FereaObjectSlot != MODEL_LOGO && FereaObjectSlot != EditorPreviewSlot);
    for (int id = 0; id < MAX_MODELS; ++id)
    {
        Require(ModelSlot(id, ModelDomain::Global) == id, "global enum ID remapped");
    }
    for (int world = -1; world <= 255; ++world)
    {
        int count = 0;
        ForEachWorldModel(world, [&](int type, int slot)
        {
            ++count;
            Require(slot == ModelSlot(type, PlacementDomain(world, type)), "loader/object lookup mismatch");
            Require(slot == PlacementSlot(world, type), "editor/loader lookup mismatch");
            Require(type == FereaObjectType || slot == type, "legacy placement remapped");
        });
        Require(count == (world == FereaMap ? 161 : 160), "world model load count changed");
        Require(PlacementDomain(world, MODEL_LOGO) == (world == FereaMap ? ModelDomain::WorldPlacement
                                                                                      : ModelDomain::Global),
                "logo domain inferred outside Ferea");
    }
    Require(PlacementDomain(FereaMap, -1) == ModelDomain::Global, "negative type accepted as placement");
    Require(PlacementDomain(FereaMap, 161) == ModelDomain::Global, "unowned type accepted as placement");
}

void CheckObjectLifetime()
{
    TestObject world;
    AssignObjectModel(world, FereaObjectType, PlacementDomain(FereaMap, FereaObjectType));
    TestObject logo;
    AssignObjectModel(logo, MODEL_LOGO, ModelDomain::Global);
    Require(world.Type == logo.Type, "original object type was changed");
    Require(ObjectSlot(world) == FereaObjectSlot && ObjectSlot(logo) == MODEL_LOGO,
            "world/global type160 aliases");
    const TestObject copy = world;
    Require(ObjectSlot(copy) == FereaObjectSlot, "copy lost model domain");
    AssignObjectModel(world, 0, ModelDomain::Global);
    Require(world.Type == 0 && world.ModelDomain == ModelDomain::Global, "reinitialize retained world domain");
    Require(ObjectSlot(logo) == MODEL_LOGO, "reinitialize affected global logo");
}

void LoadWorld(std::vector<TestModel>& models, int world)
{
    ForEachWorldModel(world, [&](int type, int slot) { models[slot].mesh = type + 1; });
}

void CheckResourceLifetime()
{
    std::vector<TestModel> models(ModelStorageCount);
    for (int slot = MODEL_LOGO; slot < ModelStorageCount; ++slot)
    {
        models[slot].mesh = -slot;
    }
    TestObject world;
    AssignObjectModel(world, FereaObjectType, ModelDomain::WorldPlacement);
    LoadWorld(models, FereaMap);
    Require(models[ObjectSlot(world)].mesh == 161, "Object161 selected a global model");
    Require(models[MODEL_LOGO].mesh == -MODEL_LOGO, "world load replaced logo");
    for (int pass = 0; pass < 3; ++pass)
    {
        ReleaseWorldModels(models.data());
        Require(models[ObjectSlot(world)].mesh == 0, "world model survived unload");
        for (int slot = MODEL_LOGO; slot <= EditorPreviewSlot; ++slot)
        {
            Require(models[slot].mesh == -slot && models[slot].releases == 0,
                    "world unload released global model or editor scratch");
        }
        LoadWorld(models, 0);
        Require(models[FereaObjectSlot].mesh == 0, "old world loaded Object161");
        ReleaseWorldModels(models.data());
        LoadWorld(models, FereaMap);
        Require(models[ObjectSlot(world)].mesh == 161, "Ferea reload lost source identity");
    }
    // An empty/failed world load resolves to that empty world model, never logo.
    models[FereaObjectSlot].Release();
    Require(models[ObjectSlot(world)].mesh == 0 && models[MODEL_LOGO].mesh == -MODEL_LOGO,
            "failed world model fell back to global logo");
    ReleaseWorldModels<TestModel>(nullptr);
}
}

int main()
{
    CheckIdentity();
    CheckObjectLifetime();
    CheckResourceLifetime();
    std::cout << "World/global IDs, Object161 selection, copy/reset and load/unload cycles passed\n";
}
