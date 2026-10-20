# Isolated terrain-height checks

This standalone target compiles only the height sample helpers and their test.
It does not configure SDL, the game client, networking or a database. The real
Ferea asset stays outside Git; the legacy Lorencia asset comes from the checkout.

Run the lightweight Python fixture preflight first. Choose a new report filename:

```powershell
python tests/terrain-height/fixture_preflight.py --ferea <external-World113/TerrainHeight.OZB> --legacy src/bin/Data/World1/TerrainHeight.OZB --report <external-new-report.json>
```

It requires the exact reviewed fixture hashes, reads both files with size limits,
and records independent references for every decoded height as little-endian
float32 CRCs. The old map retains the reader's fixed1080-byte header offset;
normalizing its historic offset is outside this change.

When the host owner permits a small native build:

```powershell
cmake -S tests/terrain-height -B <external-build> -DMU_FEREA_HEIGHT_FILE=<external-World113/TerrainHeight.OZB>
cmake --build <external-build> --config Release --parallel 1
ctest --test-dir <external-build> -C Release --output-on-failure
```

Both real fixtures are mandatory; a missing input cannot silently skip its test.
The test covers map selection preservation, packed sample byte order/boundaries,
all65536 Ferea heights, all65536 legacy heights, and the existing login-scene
factor. No Legend executable is used. This target does not establish rendered
terrain, movement/collision or whole-client integration acceptance.
