# Extended item damage: local validation, 2026-10-20

Client branch: `codex/legendmu-bmd-runtime-client`; baseline commit for this change: `a164ffb3`. Server reference branch: `codex/baski-server-prototype` at `e16c2b567a557706e1d7ef79a6d50c8f5a3afed2`.

The client's `ItemDefinition` and gameplay `ITEM_ATTRIBUTE` now retain 16-bit base minimum and maximum weapon damage from JSON. The Season 6 `ITEM_ATTRIBUTE_FILE` and `ITEM_ATTRIBUTE_FILE_LEGACY` keep their original one-byte damage fields and file layout. BMD import zero-extends those fields; either BMD export rejects a runtime value above 255 before writing a file. The editor's runtime field metadata treats the extended fields as 16-bit values. Existing JSON format version 1 remains readable.

Validation:

- Built Release `Main` with CMake target `Main`, two compiler jobs after checking >9 GB free RAM: exit 0.
- Built `test_item_database` and `test_item_database_files`: exit 0. CTest `[data][items]`: 134/134 passed, including 256/500 JSON round-trip, 65536 rejection, 255 legacy BMD import and BMD export refusal at 256/500.
- Python MapConverter suite: 26/26 passed; `git diff --check` was clean.
- In a private client runtime, temporarily set Bone Blade's JSON base damage to 256–500. The equipped +15 Bone Blade tooltip showed `292 ~ 536`, proving the gameplay/UI path did not truncate the larger base values. [Screenshot](C:/Users/blach/.codex/tmp/legendmu-bmd-runtime-20261020/ferea-smoke/stats-bone-blade-256-500.jpg), SHA-256 `208C6C55FA1E1385C445CDCDACBAF45C1591EA3E0F3A96E1DE140C78CF0EDBF0`. The original JSON was restored with a matching SHA-256. The private client and container were stopped; the retained OpenMU server and database were untouched.

OpenMU's Season 6 weapon initialization accepts `int` minimum/maximum damage and creates configurable `BasePowerUpAttributes` with floating-point base values. This change does not alter packet formats or existing server item definitions. A newly balanced item needs matching client JSON and OpenMU item configuration, then a server-side combat check. The supplied newer-client `Data/Local/item.bmd` files are zero bytes, so no v14 item-BMD parser or automatic stat import is claimed.
