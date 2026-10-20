#pragma once

#include "Core/Globals/_enum.h"
#include "ModelDomain.h"

namespace Render::Models
{
inline constexpr int FereaMap = 112;
inline constexpr int FereaObjectType = 160;
// The editor already uses the first slot past the global enum for imports.
inline constexpr int EditorPreviewSlot = MAX_MODELS;
inline constexpr int FereaObjectSlot = EditorPreviewSlot + 1;
inline constexpr int ModelStorageCount = FereaObjectSlot + 1;
static_assert(FereaObjectType == MAX_WORLD_OBJECTS);
static_assert(MODEL_LOGO == FereaObjectType);

constexpr int WorldTypeCount(int world)
{
    return MAX_WORLD_OBJECTS + (world == FereaMap ? 1 : 0);
}

constexpr ModelDomain PlacementDomain(int world, int type)
{
    return world == FereaMap && type >= MODEL_WORLD_OBJECT && type < WorldTypeCount(world)
               ? ModelDomain::WorldPlacement : ModelDomain::Global;
}

constexpr int WorldSlot(int type)
{
    return type == FereaObjectType ? FereaObjectSlot : type;
}

constexpr int ModelSlot(int type, ModelDomain domain)
{
    return domain == ModelDomain::WorldPlacement ? WorldSlot(type) : type;
}

constexpr int PlacementSlot(int world, int type)
{
    return ModelSlot(type, PlacementDomain(world, type));
}

template<class Object>
constexpr int ObjectSlot(const Object& object)
{
    return ModelSlot(object.Type, object.ModelDomain);
}

template<class Object>
void AssignObjectModel(Object& object, int type, ModelDomain domain)
{
    object.Type = type;
    object.ModelDomain = domain;
}

// Load, texture setup and editor enumeration share this identity mapping.
template<class Visitor>
void ForEachWorldModel(int world, Visitor&& visit)
{
    for (int type = MODEL_WORLD_OBJECT; type < WorldTypeCount(world); ++type)
    {
        visit(type, WorldSlot(type));
    }
}

template<class Model>
void ReleaseWorldModels(Model* models)
{
    if (models == nullptr)
    {
        return;
    }
    ForEachWorldModel(FereaMap, [&](int, int slot) { models[slot].Release(); });
}
}
