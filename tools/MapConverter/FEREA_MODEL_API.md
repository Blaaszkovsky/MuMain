# Ferea Object161: required model API change

Investigation date: 2026-10-20. Source baseline:
`3b797b8373bdc3e9df52eb38ebe1030e02829a33`.

Outcome: **no runtime fix delivered**. The authorized report alternative is
used because a safe correction crosses model ownership, object creation and
shared render/pick APIs. The previous slot and lighting findings are unchanged.
This is a concrete implementation boundary, not missing S16 semantics: filename
Object161 for source type160 is established, and the namespace collision is
fully reproducible from source.

## Reproduction and affected paths

`src/source/Core/Globals/_enum.h:758..760` makes global MODEL_LOGO equal160.
The generic model and texture loops in `World/MapInfra/MapManager.cpp:1122..1129`
visit only0..159. Its release loop at1520 has the same bound. Loading Object161
with AccessModel(160, ...) would overwrite the global logo. Increasing the enum
bound shifts later global IDs; borrowing an unused placement slot changes model
identity without establishing that slot's other consumers.

The issue cannot be fixed solely inside these loops:

| Boundary | Current source evidence | Required change |
|---|---|---|
| Load and provenance | `Data/DataHandler/LoadData.cpp:49..58` indexes Models[Type], sets m_iBMDSeqID, and remembers filenames by that integer | Distinguish model ownership and provenance keys |
| Textures | LoadData.cpp:225..239 resolves the integer to Models[Model] | Accept an explicit model reference/handle |
| Object creation | `Engine/Object/ZzzObject.cpp:4514..4550` stores only Type; both object file readers call CreateObject with the serialized type at5005/5057 | Record model domain at the creation boundary |
| Object defaults | `Engine/Object/w_ObjectInfo.cpp:124` initializes Type to0; `w_ObjectInfo.h:160` exposes a plain int | Initialize and propagate an independent model handle through every lifetime/copy path |
| Picking and collision | ZzzObject.cpp:178..205 animates and tests Models[o->Type] | Resolve the object's model before OBB and mesh tests |
| Rendering and animation | ZzzObject.cpp:266,398,445,2752,3528,3756 and mesh-state accesses at3917/4020/4024 | Use the same resolver while retaining Type for behavior dispatch |
| Culling | ZzzObject.cpp:3368 compares Type to MAX_WORLD_OBJECTS | Classify world placements using explicit ownership |
| Editor | ZzzObject.cpp:7471/7545 resolves outlines and placement preview by raw type | Carry world domain for preview even before an OBJECT exists |
| Unload | MapManager.cpp:1511..1523 releases global slots0..159 | Release only owned extra world models, after dependent work completes |

A source search for `Models[...]` containing `o->Type`, `pObj->Type`,
`Object.Type` or `Object->Type` finds31 files, including AnimationTaskPool,
BoneManager, world handlers, characters and effects. This is a candidate audit
set, not a claim that every occurrence handles world placements. Replacing all
of them mechanically would alter unrelated consumers. Global versus world
ownership must be traced from callers before migrating each shared path.

## Proposed API contract

Names below are a proposal, not implemented declarations.

1. Add a tagged `ModelHandle { ModelDomain domain; int index; }`, with domains
   Global and WorldPlacement. Keep `OBJECT::Type` unchanged for serialized ID,
   switches and world-specific behavior. Default existing non-placement object
   initialization to a global handle; do not infer domain from numeric type160,
   current world or current scene during drawing.
2. Provide `ResolveModel(ModelHandle)` and `ResolveObjectModel(const OBJECT&)`
   returning a checked model reference (or explicit failure for an unloaded
   model). Route all shared rendering, animation, picking and editor consumers
   that can receive a world placement through these functions.
3. Preserve existing world slots0..159 as aliases of the existing Models entries
   initially, to preserve current world code that intentionally edits Models[n].
   Own WorldPlacement160 in separate stable storage. Global160 continues to
   resolve MODEL_LOGO. Do not append it to the global enum or reuse another ID.
4. Introduce `CreateWorldObject(sourceType, ...)` for terrain-object readers and
   world-editor placement; leave global construction explicit and compatible.
   Both constructors set Type to the original value and set the domain before
   calling any model-dependent initialization. Reinitialization/copying must
   preserve or reset the handle deliberately. Existing login/character scene
   behavior remains global and must not select a world asset.
5. Extract model-target operations from AccessModel/OpenTexture into overloads
   accepting a model reference plus its handle for provenance. Keep existing
   integer entry points as Global wrappers. Audit m_iBMDSeqID's diagnostic/cache
   consumers before assigning it a world identity; a raw160 sequence ID alone
   must not collapse the two domains in metadata or caches.
6. Give the map asset owner explicit load/reset methods for extra world models.
   For logical Ferea112 only, load WorldPlacement160 from Object113/Object161
   and its textures. Loading failure must remain explicit. Complete worker/pose
   jobs before releasing extra models and clear references on world transition.

This requires coordinated edits to the model resolver/owner, LoadData, OBJECT
initialization, object-file/editor creation and all applicable shared consumers.
A helper that changes only a handful of draw reads would leave picking and
ownership incorrect. No partial helper or unconsumed field is committed here.

## Required CPU acceptance before runtime integration

Use fake model records and loader/release callbacks so the resolver and lifecycle
tests do not require SDL, graphics, game assets or a Main build:

- Global160 and WorldPlacement160 resolve distinct records simultaneously;
  loading or releasing the world model leaves the logo and its provenance intact.
- For every existing world ID0..159, WorldPlacement resolves the same record as
  the previous global slot; all global enum values and behavior IDs are unchanged.
- Creating a world type160 retains Type160 and selects Object161; creating a
  global type160 retains Type160 and selects the logo, regardless of active scene.
- The same injected resolver supplies bounds/mesh/animation data to render,
  picking, outline and placement preview paths. Exercise both domains and detect
  a remaining direct Models[Type] access in the migrated paths during review.
- Transition Ferea -> old world -> Ferea releases/reloads only the extra model,
  handles a failed load explicitly and prevents use after reset. Include copy,
  reinitialize and outstanding-animation-job lifetime cases.

Those tests cannot be claimed until the production APIs and their consumers
exist. A standalone hypothetical resolver test would not prove the current
client fixed and was deliberately not substituted for implementation evidence.

## Validation actually executed

```powershell
python -m unittest discover -s tools/MapConverter/tests -p test_ferea_audit.py -v
```

Result: **7 tests passed,0 failed,0 skipped**. In particular, the existing
`test_raw_flags_type160_and_transform_are_preserved` confirms retained source
type160 and Object161 naming in diagnostics. This does **not** validate separate
runtime model domains; no such implementation is delivered. This follow-up
changes documentation only. No Main build, asset copy, game launch, server
change or push was performed. The conflict remains an open implementation task.
