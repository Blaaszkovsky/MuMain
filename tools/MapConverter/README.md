# Offline map converter

This tool audits ATT, MAP and OBJ files and optionally writes them into a **new,
separate output directory** for the existing MuMain readers. It never installs
assets or changes the client. Python 3.10+, CMake 3.24+ and a C++20 compiler are
required. Build the standalone helper (no client build needed):

```powershell
cmake -S tools/MapConverter -B <outside-repository-build-directory>
cmake --build <outside-repository-build-directory> --config Release --parallel 2
ctest --test-dir <outside-repository-build-directory> -C Release --output-on-failure
python -m unittest discover -s tools/MapConverter/tests -p "test_*.py" -v
```

CMake downloads a SHA256-pinned Crypto++ 8.9.0 source archive. To use a previously
verified extraction, pass `-DFETCHCONTENT_SOURCE_DIR_CRYPTOPP=<source-directory>`.
No supplied game executable, DLL or static library is used. Binary redistribution
must include the downloaded Crypto++ `License.txt` and `THIRD_PARTY_NOTICES.md`.

Audit one world before converting a larger tree:

```powershell
python tools/MapConverter/convert_maps.py --input <Data/World113> --output <fresh-audit-directory> --decoder <build/Release/map_envelope_decode.exe>
```

Then use a different fresh directory and add `--convert`. ATT/MAP are decrypted,
validated and re-encrypted using the legacy format. Every output is decoded
again and compared byte-for-byte with the intended payload. The manifest records
input/output hashes, decoded payload hashes, script/helper identities and limits.
Existing output directories and input/output overlap are refused. Batch failures
remain in the manifest; successfully validated files can still be written, and
the command returns 1 when any file is withheld, unsupported or invalid.

OBJ v1 has two additional lighting flags per object. The legacy reader cannot
express them. By default conversion is withheld. `--allow-lighting-loss` permits
a limited OBJ downgrade: type, position, angle, scale and order are preserved
exactly; lighting flags are saved in `<name>.obj.lighting.json`. Reconstruction
of the entire source decoded OBJ is checked before writing, and the sidecar
records the associated source/output hashes. Keep the sidecar with the output.
The game does not read it, so it does not restore the visual lighting behavior.
Even zero flags do not establish equivalent renderer defaults.

ATT high attribute bits are **not** permitted by the lighting opt-in: the current
client masks them away. Low attribute bytes >=128 trigger its error path and are
also refused. OBJ v2/v3 are inspected for inventory purposes but not converted.
Unknown versions, invalid lengths/counts, non-finite transforms and a world byte
that differs from its `World<number>` directory are refused. Map IDs are not
automatically remapped to logical server map IDs.

The newer envelope contains no CRC/MAC. SHA256 identifies the bytes read; exact
length, header, dimensions, EOF and record checks detect structural problems.
They cannot authenticate the input or detect every corruption that still forms
a valid map. A converter success is **parser evidence**, not visual, collision,
gameplay or original ENG-client acceptance. Models, textures, terrain height
selection, per-world object behavior and server integration remain separate gates.

Before using a new world, run an isolated visual A/B against a trusted reference
with matching camera, time/light conditions and object placement. Compare shadows,
lighting, texture layers and alpha, height, clipping and collision paths. For OBJ
v1 compare both observed flag groups; sidecar preservation alone is not visual
equivalence. See [third-party notices](THIRD_PARTY_NOTICES.md) for source provenance.
