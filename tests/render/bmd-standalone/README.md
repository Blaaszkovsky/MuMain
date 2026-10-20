# Native model loader checks

Build these focused tests without configuring or building the game. Provide an
existing source/header installation and library of Crypto++ (the header root must
contain `cryptopp/tea.h`). Use the same architecture and C++ runtime for both.

```powershell
cmake -S tests/render/bmd-standalone -B out/bmd-loader -DCRYPTOPP_ROOT=C:/deps/include -DCRYPTOPP_LIBRARY=C:/deps/lib/cryptopp.lib
cmake --build out/bmd-loader --config Release --parallel 1
ctest --test-dir out/bmd-loader -C Release --output-on-failure
```

The normal `BUILD_TESTING=ON` test tree also includes these tests. The standalone
build accepts the portable Crypto++ library built with `CRYPTOPP_DISABLE_ASM`;
use a matching library for this target.

For a private collection, use a completed manifest from the independently
validated BmdConverter at `d7a8b73d`. Its successful entries must contain expected
payload SHA-256 values; its failed entries are expected to be rejected. This
checks every selected model's complete native result against those hashes.

```powershell
python tests/render/bmd-standalone/verify_private.py --data C:/private/Data --manifest C:/private/converted/manifest.json --probe out/bmd-loader/Release/bmd_loader_probe.exe --report C:/private/native-report.json
python tests/render/bmd-standalone/verify_legacy.py --data src/bin/Data --probe out/bmd-loader/Release/bmd_loader_probe.exe --report C:/private/legacy-report.json
```

The legacy check independently decodes v10/v12 and compares full payload hashes.
Both scripts process one file at a time, with a 60-second per-process timeout,
and write only the requested JSON report. Keep game assets outside Git. Freeze
the probe executable for a batch; rebuilding an executable while it is running
can interrupt a Windows batch. These checks do not exercise Open2 allocation,
rendering, textures, animation playback or original-client compatibility.

For the repository Data tree at `112cece4`, five existing v12 models are unsafe
and intentionally fail strict loading. `Item/dark_05.bmd` has non-finite animation
values. `Object35/Object36.bmd`, `Object52/Object90.bmd`, `Object57/Object36.bmd`
and `Skill/CW_Bow_Skill.bmd` contain vertex bone index `-8888`. The old Open2
copied these values unchecked, and `CreateBoundingBox` used the negative value
to index its bone arrays. The independent BmdConverter payload parser rejects
these same five. To assert those exact failures while checking all other models:

```powershell
python tests/render/bmd-standalone/verify_legacy.py --data src/bin/Data --probe out/bmd-loader/Release/bmd_loader_probe.exe --report C:/private/legacy-report.json --expected-reject Item/dark_05.bmd --expected-reject Object35/Object36.bmd --expected-reject Object52/Object90.bmd --expected-reject Object57/Object36.bmd --expected-reject Skill/CW_Bow_Skill.bmd
```

No unsafe model is repaired or rewritten by the loader. Missing expected paths
or an expected rejection that starts loading cause the check to fail.
