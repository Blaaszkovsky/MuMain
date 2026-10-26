# PR #812: review-fix screenshots

Code: [`f4b31f1e`](https://github.com/Blaaszkovsky/MuMain/commit/f4b31f1e358557c3112237f498be77c31c1b5ed4).

Twenty original, unedited 1600 x 900 game captures. Each fresh process starts in the named theme at 75% or 150% UI scale, visits Lorencia -> Kanturu Relics (38) -> Vulcanus (63), switches to the other theme while the banner is visible, then returns to Lorencia. The final two columns therefore use the other theme.

| Starting theme / scale | Lorencia | Kanturu Relics | Vulcanus / Gens | Live theme switch | Return / Gens cleared |
|---|---|---|---|---|---|
| legacy / 75% | [![legacy 75% lorencia](legacy-75-lorencia.jpg)](legacy-75-lorencia.jpg) | [![legacy 75% kanturu](legacy-75-kanturu.jpg)](legacy-75-kanturu.jpg) | [![legacy 75% gens](legacy-75-gens.jpg)](legacy-75-gens.jpg) | [![legacy 75% theme-switch](legacy-75-theme-switch.jpg)](legacy-75-theme-switch.jpg) | [![legacy 75% return](legacy-75-return.jpg)](legacy-75-return.jpg) |
| legacy / 150% | [![legacy 150% lorencia](legacy-150-lorencia.jpg)](legacy-150-lorencia.jpg) | [![legacy 150% kanturu](legacy-150-kanturu.jpg)](legacy-150-kanturu.jpg) | [![legacy 150% gens](legacy-150-gens.jpg)](legacy-150-gens.jpg) | [![legacy 150% theme-switch](legacy-150-theme-switch.jpg)](legacy-150-theme-switch.jpg) | [![legacy 150% return](legacy-150-return.jpg)](legacy-150-return.jpg) |
| modern / 75% | [![modern 75% lorencia](modern-75-lorencia.jpg)](modern-75-lorencia.jpg) | [![modern 75% kanturu](modern-75-kanturu.jpg)](modern-75-kanturu.jpg) | [![modern 75% gens](modern-75-gens.jpg)](modern-75-gens.jpg) | [![modern 75% theme-switch](modern-75-theme-switch.jpg)](modern-75-theme-switch.jpg) | [![modern 75% return](modern-75-return.jpg)](modern-75-return.jpg) |
| modern / 150% | [![modern 150% lorencia](modern-150-lorencia.jpg)](modern-150-lorencia.jpg) | [![modern 150% kanturu](modern-150-kanturu.jpg)](modern-150-kanturu.jpg) | [![modern 150% gens](modern-150-gens.jpg)](modern-150-gens.jpg) | [![modern 150% theme-switch](modern-150-theme-switch.jpg)](modern-150-theme-switch.jpg) | [![modern 150% return](modern-150-return.jpg)](modern-150-return.jpg) |

## Provenance and validation

Built and captured before committing the review fixes. Production sources and runtime assets are identical to the fix commit; only commit metadata changed.

The executable reports base `1a83be2c` and `commit_changed=true`. SHA-256: `d2ee1bc62df5b2b606320ee742c49cfce14378688903998ce7a9f07ef238105e`. No executable or font files are distributed here. See [manifest.json](manifest.json) for per-image hashes and capture metadata.

All 20 original frames independently inspected: readable centered text, no clipping or text/ornament collision. Gens present on battle maps and cleared on return. Cursor overlaps part of the ornament in some frames. The modern 150% return frame was captured after banner timeout.

Windows x64 Release Main and affected test targets built successfully. Map tests: 6 cases / 22,251 assertions, both with real Corsiva and with both optional font locations unavailable in separate processes. Theme/font tests: 20 / 52,203. Real GPU Gens retry test: 1 / 18. Crop: 1 / 20. Stacking: 6 / 391. Selected CTest: 10/10; RML contracts: 30/30; four RML/RCSS guards, CI-aligned formatting and Cppcheck passed.

No Linux/macOS builds, full unrelated test suite, or build of the combined latest-main integration tree. Hosted CI must be assessed separately.

## Earlier PR appearance

These are the already-published pre-review captures at **100%** UI scale. The new captures above use **75% and 150%**, so this is historical context rather than a controlled same-scale comparison.

| Earlier legacy / Gens | Earlier modern / Gens |
|---|---|
| [![Earlier legacy 100%](https://raw.githubusercontent.com/Blaaszkovsky/MuMain/0ee75a71e90e83d0ca08cd6478c5e1200e1dac4a/legacy-vulcanus-gens.jpg)](https://github.com/Blaaszkovsky/MuMain/blob/0ee75a71e90e83d0ca08cd6478c5e1200e1dac4a/legacy-vulcanus-gens.jpg) | [![Earlier modern 100%](https://raw.githubusercontent.com/Blaaszkovsky/MuMain/0ee75a71e90e83d0ca08cd6478c5e1200e1dac4a/modern-vulcanus-gens.jpg)](https://github.com/Blaaszkovsky/MuMain/blob/0ee75a71e90e83d0ca08cd6478c5e1200e1dac4a/modern-vulcanus-gens.jpg) |
