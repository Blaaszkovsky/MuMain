# Ferea: three-blocker investigation, 2026-10-20

Status: **render acceptance blocked**. This bounded investigation closes with
evidence requirements, as authorized when semantics cannot be established.
No runtime mapping, fallback material or lighting behavior is introduced.

## Reproduction baseline

Client source: `b57b005bb633efb9725cf809e2c1a3f1da0b66f9`, branch
`codex/legend-map-format-client`. The report applies to that source and the
previously hashed World113 fixture; revalidate it after relevant source changes.
Logical map identity is 112; the asset directory and embedded world byte are 113.

Private evidence root on the investigation machine:
`C:/Users/blach/AppData/Local/Temp/legend-map-analysis-20261020/`.
The existing `ferea-preview-final/manifest.json` records fixture hashes and
counts; `objects-with-lighting.json` retains every object and both flags.
`ferea-render-readiness.md` records asset provenance and earlier conversion
commands. No proprietary asset is included with this report.

The permitted MIT tools inspected were WorldEditor at
`d1ba08fd1fb5894e128ac6e492a596bb9b7bebc1` and MuClientTools16 at
`90c78cf5876ec43be69f26e2956d3ffbdc26c5ad`. Paths below for WorldEditor are
relative to its `WorldEditor/` directory. Its source exposes calls into an
existing client binary; those calls do not expose that engine's implementation.

## Material slots 24 and 26: intended binding unknown

MuMain's `src/source/World/MapInfra/MapManager.cpp` generic loader maps slots
14..29 to ExtTile01..16. Slot24 requests `ExtTile11.OZJ`; slot26 requests
`ExtTile13.OZJ`. Neither is present in the fixture, which supplies ExtTile01..10.
The MAP stores numeric layer indices and alpha bytes, without a filename table.

The existing CPU audit applies MuMain's four-corner LOD1 alpha selection, before
frustum, terrain-wall and other rendering suppression:

| Slot | Base candidate quads | Blend candidate quads |
|---:|---:|---:|
|24|293|222|
|26|0|3|

Slot26's three blend candidates are (184,77), (185,77), (186,77). Their anchor
alpha is zero but neighboring alpha is nonzero. Therefore ignoring slot26 based
on anchor alpha would change the current selection rule. `AlphaTile01.OZT`
exists, but its presence proves neither its binding nor an S16 alpha rule.
Generic slot10 is TileRock04; substituting AlphaTile there is also unsupported.

WorldEditorUI.cpp:594..609 reads loaded bitmap slots `30256 + i` and displays
`bitmap->FileName`. It can reveal a trusted running client's table, but does
not define the native bindings. WorldEditor.cpp:87..150 reads `Tiles.data` for
separately imported extension tiles. That is not a World113 default table.
WorldEditor.cpp:420 delegates to `__RenderTerrainFace`, a function pointer in
WorldEditorOffset.h:137, rather than implementing the renderer.

**Missing evidence:** a trusted World113 slot-to-file capture resolving 24, 26
and AlphaTile01, or permitted source/configuration defining those bindings.
If its layer rule differs, the corresponding source or controlled reference
render is also required. Similar-looking pixels are insufficient.

## Object type 160: namespace conflict confirmed, routing unimplemented

The fixture contains two type160 placements, selecting Object161.bmd by the
world-object filename convention. MuMain's `Core/Globals/_enum.h:758..760`
defines `MAX_WORLD_OBJECTS = 160` and `MODEL_LOGO = MAX_WORLD_OBJECTS`.
MapManager.cpp:1122..1129 loads models/textures only for slots0..159, and its
release loop at1520 has the same bound. Thus type160 aliases a global model
slot outside the world-object loading range.

Increasing the enum limit shifts subsequent model IDs. Reusing a placement slot
absent from this particular OBJ does not establish that other runtime consumers
leave it unused. `Engine/Object/ZzzObject.cpp` directly indexes `Models[o->Type]`
through creation, animation, collision and drawing paths, and also has culling
logic using the world-object limit. An identity-preserving model lookup must
cover these consumers and the resource lifecycle. Logo-specific creation
overrides observed there are gated to login/character scenes; this report does
not claim those overrides execute during Ferea gameplay.

**Required implementation evidence:** a complete inventory and tested routing
of original type160 to a separately owned world model, preserving the original
type for behavior dispatch, with load/release, animation, collision, culling
and all draw consumers covered. The mechanical conflict needs no further S16
semantic guess. This investigation does not claim a completed routing fix;
with the material and lighting requirements unresolved it closes at this
explicit remaining implementation boundary, without expanding into a partial
model-namespace refactor.

## OBJv1 lighting: storage confirmed, behavior unknown

All4029 records retain two bytes: 2316 have [0,1], 1713 have [1,1]. The second
byte is constant, so these data cannot identify its effect. The first byte
varies across different objects and positions, not a controlled comparison.

WorldEditorStruct.h names the fields `LightEnable1/2`. WorldEditorSave.cpp:
269..270 writes both for version1+, and WorldEditorUI.cpp:524..526 exposes two
checkboxes. Creation delegates to `__CreateObject`, whose function pointer is
declared in WorldEditorOffset.h:149. MuClientTools16's OBJ codec establishes
reversible file encoding, not rendering semantics. These sources establish
storage and editing only.

MuMain's single `LightEnable` in ZzzObject.cpp:236..252 controls a different
known calculation: terrain light plus object light when true, and terrain light
scaled by0.1 plus object light when false. Matching a field name cannot prove
that either S16 byte has that meaning, nor how their combination behaves.

**Missing evidence:** permitted engine source using both fields, or trusted
controlled captures of the same object, camera, world and lighting with flag
pairs00/01/10/11, including defaults and whether flags affect mesh shading,
terrain lighting, alpha or another pass. No supplied Legend binary was run.
The sidecar preserves bytes but is not a runtime implementation of these rules.

## CPU validation and limits

On this baseline, the following command passed: **7 tests, 0 failures, 0 skips**.

```powershell
python -m unittest discover -s tools/MapConverter/tests -p test_ferea_audit.py -v
```

Existing tests cover slot filenames, neighbor-driven blending, full/zero alpha,
edge wrapping, missing-file reporting, and exact type160/transform/flag retention.
They verify the diagnostic behavior; they cannot determine missing S16 semantics.
This follow-up changes documentation only, so no implementation-mirroring tests
were added. No Main build, game render, supplied executable, asset installation,
server/database change or push was performed. All three runtime gates remain
open; the first and third require the precise external evidence above.
