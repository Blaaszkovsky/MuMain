# Ferea private client-server smoke, 2026-10-20

This is diagnostic evidence for the local LegendMU asset integration, not an original-client compatibility gate or a complete Ferea render pass. The client source was commit `e59ab676` plus the namespace fix subsequently committed as `a994b675` (`codex/legendmu-bmd-runtime-client`); the executable reported `e59ab676` and `commit_changed: true` because it was built before that fix was committed. The server branch was `codex/baski-server-prototype` at `e16c2b567a557706e1d7ef79a6d50c8f5a3afed2`. Nothing was installed into the retained OpenMU environment.

## Reproduction and outcome

- Built `Main` in Release with `cmake --build out/build/windows-x64-mueditor --config Release --target Main --parallel 1`: exit 0. The executable copied into the private runtime had SHA-256 `83A02B6E518FB7042A1B18F13148C85BA8B600379109D19776FEF8D1F4961972`.
- Ran `python -m unittest discover -s tools/MapConverter/tests -p 'test_*.py' -q`: 26 passed. `git diff --check`: clean.
- Ran the private demo image `openmu-baski-ferea-prototype:20261020` on `127.127.127.127:56934`, with no database or volume. The retained server and database were left running.
- Launched the private runtime with `MU_BASKI_PROTOTYPE=1`, logged in as the demo `testgmDk` character, and sent `/move testgmDk 112 236 152`. The client entered world map 112 at `[236,152]` and rendered terrain and several world objects. [Entry screenshot](C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/ferea-entry.jpg) SHA-256: `6642142CACCBDB6CFCFD375984B7C9374BD5F5AD0057E05C17ED1C2A009E8508`.
- Initial world load raised an `Error` modal with the text `LoadBitmap Failed: Data\`. The control socket did not answer `state` while this modal was open. Pressing its `Continue` button let the scene run. The empty path is not yet traced to its caller.
- Teleported within map 112 to `[215,165]`. The resulting scene contained a large white rectangle in place of a terrain or object texture. [Offset screenshot](C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/ferea-offset.jpg) SHA-256: `9B18930CCA8AD56E75163EAF04CF1580F4D2EBC90DBE8477C480F5F8E35D98F4`. This is consistent with, but does not prove, the separately documented missing World113 texture bindings.
- `/createmonster 900 true` returned `Monster with number 900 created; id: 515.` in chat. The client's `nearby` telemetry remained empty, so this run does not establish Baski's Ferea visibility or animation. The earlier isolated Baski Lorencia test is separate evidence and must not be carried over as a Ferea pass.
- The private client exited through its control command, and the private Compose project was stopped and removed. No global Docker prune was used.

## Remaining gates

Trace the empty-path `LoadBitmap` caller; establish authoritative World113 slots 24/26 and alpha-layer behavior; establish the two OBJv1 lighting-byte semantics; then rerun the same map and Baski scenario without a modal, white patch, or missing mob telemetry. The OBJv1 lighting flags are retained in a sidecar, while this smoke runtime uses the derived legacy OBJv0 placement file, so this test cannot validate lighting fidelity.

The private command trace is at `C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/commands.jsonl`, SHA-256 `9935007694C5E681C34EF4726A4B60D6701718975F80F7949092E0FE7BFA4E39`.

## Follow-up: playable prototype, 2026-10-20

The earlier white rectangle was caused by an incomplete private runtime: `monster300.bmd`, `Baski_Body.OZT` and `Baski_Fire.OZJ` were absent from `Data/Monster`. With those source-hash-verified files staged, the natural Baski spawn (monster 900, runtime id 513) is visible on Ferea, moves, attacks and can be killed. Three attacks reduced its health to 85.2%; a later 18-attack sequence killed it. The stable Baski screenshot is `C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/ferea-baski-gm-stable.jpg`.

The entry modal was unrelated to World113 materials. Diagnostic logging identified `LoadBitmap` index 31254, the map-name image slot. `CUIMapName::ShowMapName` indexed an absent map112 entry, inserted an empty path and attempted to load `Data\\`. It now hides the unavailable banner and leaves the map active. `CMapManager::GetMapName(112)` now returns `Ferea`, avoiding an unrelated localized-string lookup. Release `Main` built successfully with the UI checks and single compiler job. The private runtime executable SHA-256 is `E8B7678A177BC3A564E83CE12A29B2B5D590E900168835C77645130CC95E6B90`. Login returned world map112 at [215,170] without a modal; control `state` and screenshots remained responsive. Final-entry screenshot: `C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/ferea-final-entry.jpg`.

The map is playable in the tested region, but this is not a full render-fidelity gate. The supplied World113 pack has no `ExtTile11` or `ExtTile13`, even though the legacy generic loader maps slots 24 and 26 to them. The meaning of the two OBJv1 lighting bytes is also unverified; the runtime still uses the converted OBJv0 placement data. Do not claim full-map visual parity or native v14 lighting from this smoke.

A further GM move to [185,77], where slot26 appears in the blend layer, stayed on map112 and produced [a screenshot](C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/ferea-slot26.jpg) without a visible white patch. Moves aimed at slot24 cells [85,152] and [141,180] were returned by the server to [236,152], so those cells were not visually accepted. This narrows the observed problem but does not resolve the missing material binding.
