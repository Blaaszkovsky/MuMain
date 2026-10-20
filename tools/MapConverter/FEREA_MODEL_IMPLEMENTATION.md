# Ferea Object161 model namespace fix

2026-10-20, implemented on `codex/legend-map-format-client` from source
`27cf934e391b8998714a61ad3905e6d2313bedf2`. The commit containing this report
contains the implementation. The earlier API investigation is historical;
this report supersedes its conclusion that no runtime change was delivered.

## Result and boundaries

Ferea's source object type160 is preserved. Its model and textures load from
Object113/Object161 into a reserved slot distinct from both global MODEL_LOGO160
and the editor import preview. Existing global enum values are unchanged.
Ferea is not installed or launched by this change. Material slots24/26 and
OBJv1 lighting semantics remain separate unresolved gates.

The existing fixed global model store is extended by two explicitly initialized
slots: editor preview at MAX_MODELS (already used by the editor), and Ferea
Object161 at MAX_MODELS+1. This avoids relying on random allocation padding and
retains existing loader/texture/provenance APIs: they receive the resolved
storage slot, so their sequence IDs and filename records remain distinct.
MAX_MODELS itself is unchanged. Existing world types0..159 still alias their
original slots, preserving all direct per-world model adjustments.

## Paths covered

- `Render/Models/ModelDomain.h` and `WorldModelSlots.h`: named Global and
  WorldPlacement domains, source-type-to-slot policy, enumeration and release.
- `OBJECT` initialization resets the domain to Global. Existing CreateObject
  calls default to Global. The object readers and map placement entry points
  use CreateWorldObject, which records Ferea ownership at construction.
  Domain is not inferred from the active map or scene during object drawing.
- Generic map loading uses the same enumeration for BMD and texture loading.
  Only logical map112 includes type160/Object161. Missing assets use the
  existing loader failure reporting; there is no logo fallback.
- Shared world-object drawing, pose calculation, visual effects, animation,
  picking/mesh collision, after-image rendering and editor outlines in
  ZzzObject resolve the stored object domain. Type-based behavior stays intact.
  The normal world culling path does not reject type160. The existing numeric
  MAX_WORLD_OBJECTS comparison is confined to the old login-world branch.
- Editor model lists, thumbnails, import/free-slot selection and placement
  previews resolve world slots; import scratch previews remain independent.
  Reposition, undo and legacy editor cloning preserve the original domain.
  SaveObjects continues to serialize source Type, not the storage slot.
- DeleteObjects destroys placements before releasing world models, including
  the extra slot. Release never touches MODEL_LOGO or the editor preview slot.
  The existing animation worker dispatch accepts CHARACTER instances, not
  world placements; no extra asynchronous world-model consumer is introduced.

Character/item-only model reads remain global, as do effect/UI asset IDs.
Map-specific handlers for existing maps retain their existing integer slots.
No model, texture, lighting flag, BMD decoder or server behavior is changed.

## Tests and commands

The new standalone target also registers under the normal test tree. From this
worktree, using the bundled Visual Studio CMake tools:

```powershell
cmake -S tests/world-models -B <external-build-dir> -G "Visual Studio 17 2022" -A x64
cmake --build <external-build-dir> --config Release --parallel 1
ctest --test-dir <external-build-dir> -C Release --output-on-failure
python -m unittest discover -s tools/MapConverter/tests -p 'test_*.py' -v
```

External build directory used:
`C:/Users/blach/AppData/Local/Temp/legend-map-analysis-20261020/world-model-native-build`.
Native result: **1/1 CTest passed**. Python result: **26 passed,0 skipped**.
The initial native attempt exposed the enum header's platform flag macro
dependency; the isolated test supplies its unused platform aliases/macros and
then compiled successfully with MSVC19.40. MSBuild warned about using a Temp
output directory; no compiler error remains in this target.

The CPU regression compiles the actual slot policy and enum definitions. It
checks all global model IDs, world IDs-1..255, identical loader/editor/object
selection, simultaneous global160/world160 identity, preserved source Type,
copy/reset, three Ferea -> old world -> Ferea load/release cycles, global and
preview-slot survival, and an empty world slot without fallback to the logo.
Resource callbacks use small fake model records; no graphics or asset is needed.

These tests prove the policy/lifecycle helpers, not a complete game render.
Main and MuEditor translation units were not compiled, as requested while a
separate client build runs. Full integration compilation and visual/pick/editor
smoke remain required when authorized and when trustworthy material bindings
are available. No runtime installation, heavy process, asset copy, push or
server/database change was performed.
