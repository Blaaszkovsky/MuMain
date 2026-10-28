# PR #812: sharper map-name lettering

Clean Windows x64 Release client [`4b7da7d5`](https://github.com/Blaaszkovsky/MuMain/commit/4b7da7d55b2684eeed2df9e82643fc0378baa3e8); runtime reports `commit_changed=false`.

Text uses direct dp layout and glyphs rasterized at their final screen size, with no parent scaling or horizontal squeezing. A separate 384dp text field (maximum 90% viewport width) lets long names retain their proportions. Names exceeding that field fit by font size only. A single 2px outline replaces the glow and layered outlines; the silver gradient remains. The original 166 × 90dp ornament and 166 × 28dp Gens label keep their layout.

[Before/after comparison with the previous 6b80fa42 build](sharpness-comparison.md). Open the full images at 100% zoom to judge glyph edges; browser thumbnail resizing introduces its own interpolation.

All images below are raw, unedited client JPEG captures. Gameplay positions and lighting can differ between runs. Screenshots are stored separately from the code diff.

| English, 1600 × 900 / 100% | Legacy | Modern |
|---|---|---|
| lorencia | [![legacy lorencia](screenshots/main-legacy-en-100-1600x900-lorencia.jpg)](screenshots/main-legacy-en-100-1600x900-lorencia.jpg) | [![modern lorencia](screenshots/main-modern-en-100-1600x900-lorencia.jpg)](screenshots/main-modern-en-100-1600x900-lorencia.jpg) |
| devias | [![legacy devias](screenshots/main-legacy-en-100-1600x900-devias.jpg)](screenshots/main-legacy-en-100-1600x900-devias.jpg) | [![modern devias](screenshots/main-modern-en-100-1600x900-devias.jpg)](screenshots/main-modern-en-100-1600x900-devias.jpg) |
| dungeon | [![legacy dungeon](screenshots/main-legacy-en-100-1600x900-dungeon.jpg)](screenshots/main-legacy-en-100-1600x900-dungeon.jpg) | [![modern dungeon](screenshots/main-modern-en-100-1600x900-dungeon.jpg)](screenshots/main-modern-en-100-1600x900-dungeon.jpg) |
| noria | [![legacy noria](screenshots/main-legacy-en-100-1600x900-noria.jpg)](screenshots/main-legacy-en-100-1600x900-noria.jpg) | [![modern noria](screenshots/main-modern-en-100-1600x900-noria.jpg)](screenshots/main-modern-en-100-1600x900-noria.jpg) |
| kanturu38 | [![legacy kanturu38](screenshots/main-legacy-en-100-1600x900-kanturu38.jpg)](screenshots/main-legacy-en-100-1600x900-kanturu38.jpg) | [![modern kanturu38](screenshots/main-modern-en-100-1600x900-kanturu38.jpg)](screenshots/main-modern-en-100-1600x900-kanturu38.jpg) |
| balgass42 | [![legacy balgass42](screenshots/main-legacy-en-100-1600x900-balgass42.jpg)](screenshots/main-legacy-en-100-1600x900-balgass42.jpg) | [![modern balgass42](screenshots/main-modern-en-100-1600x900-balgass42.jpg)](screenshots/main-modern-en-100-1600x900-balgass42.jpg) |
| gens63 | [![legacy gens63](screenshots/main-legacy-en-100-1600x900-gens63.jpg)](screenshots/main-legacy-en-100-1600x900-gens63.jpg) | [![modern gens63](screenshots/main-modern-en-100-1600x900-gens63.jpg)](screenshots/main-modern-en-100-1600x900-gens63.jpg) |

## Scale and long-name checks

| Resolution / UI setting / map | Legacy | Modern |
|---|---|---|
| 1280x720 / 75% / kanturu38 | [![legacy kanturu38 75%](screenshots/main-legacy-en-75-1280x720-kanturu38.jpg)](screenshots/main-legacy-en-75-1280x720-kanturu38.jpg) | [![modern kanturu38 75%](screenshots/main-modern-en-75-1280x720-kanturu38.jpg)](screenshots/main-modern-en-75-1280x720-kanturu38.jpg) |
| 1280x720 / 75% / balgass42 | [![legacy balgass42 75%](screenshots/main-legacy-en-75-1280x720-balgass42.jpg)](screenshots/main-legacy-en-75-1280x720-balgass42.jpg) | [![modern balgass42 75%](screenshots/main-modern-en-75-1280x720-balgass42.jpg)](screenshots/main-modern-en-75-1280x720-balgass42.jpg) |
| 1920x1080 / 150% / kanturu38 | [![legacy kanturu38 150%](screenshots/main-legacy-en-150-1920x1080-kanturu38.jpg)](screenshots/main-legacy-en-150-1920x1080-kanturu38.jpg) | [![modern kanturu38 150%](screenshots/main-modern-en-150-1920x1080-kanturu38.jpg)](screenshots/main-modern-en-150-1920x1080-kanturu38.jpg) |
| 1920x1080 / 150% / balgass42 | [![legacy balgass42 150%](screenshots/main-legacy-en-150-1920x1080-balgass42.jpg)](screenshots/main-legacy-en-150-1920x1080-balgass42.jpg) | [![modern balgass42 150%](screenshots/main-modern-en-150-1920x1080-balgass42.jpg)](screenshots/main-modern-en-150-1920x1080-balgass42.jpg) |
| 800x600 / 150% / kanturu38 | [![legacy kanturu38 150%](screenshots/main-legacy-en-150-800x600-kanturu38.jpg)](screenshots/main-legacy-en-150-800x600-kanturu38.jpg) | [![modern kanturu38 150%](screenshots/main-modern-en-150-800x600-kanturu38.jpg)](screenshots/main-modern-en-150-800x600-kanturu38.jpg) |
| 800x600 / 150% / balgass42 | [![legacy balgass42 150%](screenshots/main-legacy-en-150-800x600-balgass42.jpg)](screenshots/main-legacy-en-150-800x600-balgass42.jpg) | [![modern balgass42 150%](screenshots/main-modern-en-150-800x600-balgass42.jpg)](screenshots/main-modern-en-150-800x600-balgass42.jpg) |
| 1366x768 / 125% / kanturu38 | [![legacy kanturu38 125%](screenshots/main-legacy-en-125-1366x768-kanturu38.jpg)](screenshots/main-legacy-en-125-1366x768-kanturu38.jpg) | [![modern kanturu38 125%](screenshots/main-modern-en-125-1366x768-kanturu38.jpg)](screenshots/main-modern-en-125-1366x768-kanturu38.jpg) |
| 1366x768 / 125% / balgass42 | [![legacy balgass42 125%](screenshots/main-legacy-en-125-1366x768-balgass42.jpg)](screenshots/main-legacy-en-125-1366x768-balgass42.jpg) | [![modern balgass42 125%](screenshots/main-modern-en-125-1366x768-balgass42.jpg)](screenshots/main-modern-en-125-1366x768-balgass42.jpg) |
| 1280x720 / 75% / Lorencia | [![legacy Lorencia 75%](screenshots/main-legacy-en-75-1280x720-lorencia.jpg)](screenshots/main-legacy-en-75-1280x720-lorencia.jpg) | [![modern Lorencia 75%](screenshots/main-modern-en-75-1280x720-lorencia.jpg)](screenshots/main-modern-en-75-1280x720-lorencia.jpg) |

The shared window-fit policy can cap the requested UI percentage at small window sizes.

## Locale fallback

| Locale / map, 1600 × 900 / 100% | Legacy | Modern |
|---|---|---|
| pl / losttower4 | [![legacy pl](screenshots/locale-legacy-pl-100-1600x900-losttower4.jpg)](screenshots/locale-legacy-pl-100-1600x900-losttower4.jpg) | [![modern pl](screenshots/locale-modern-pl-100-1600x900-losttower4.jpg)](screenshots/locale-modern-pl-100-1600x900-losttower4.jpg) |
| ru / kanturu38 | [![legacy ru](screenshots/locale-legacy-ru-100-1600x900-kanturu38.jpg)](screenshots/locale-legacy-ru-100-1600x900-kanturu38.jpg) | [![modern ru](screenshots/locale-modern-ru-100-1600x900-kanturu38.jpg)](screenshots/locale-modern-ru-100-1600x900-kanturu38.jpg) |
| ja / kanturu38 | [![legacy ja](screenshots/locale-legacy-ja-100-1600x900-kanturu38.jpg)](screenshots/locale-legacy-ja-100-1600x900-kanturu38.jpg) | [![modern ja](screenshots/locale-modern-ja-100-1600x900-kanturu38.jpg)](screenshots/locale-modern-ja-100-1600x900-kanturu38.jpg) |
| zh-tw / devias | [![legacy zh-tw](screenshots/locale-legacy-zh-tw-100-1600x900-devias.jpg)](screenshots/locale-legacy-zh-tw-100-1600x900-devias.jpg) | [![modern zh-tw](screenshots/locale-modern-zh-tw-100-1600x900-devias.jpg)](screenshots/locale-modern-zh-tw-100-1600x900-devias.jpg) |

## Transition checks

[![Gens after live switch to Modern](screenshots/main-switch-modern-en-100-1600x900.jpg)](screenshots/main-switch-modern-en-100-1600x900.jpg)
[![Return to Lorencia: Gens cleared](screenshots/main-legacy-en-100-1600x900-return-lorencia.jpg)](screenshots/main-legacy-en-100-1600x900-return-lorencia.jpg)
[![Logout: no map banner](screenshots/main-legacy-en-100-1600x900-logout.jpg)](screenshots/main-legacy-en-100-1600x900-logout.jpg)

## Verification and provenance

- Windows Main and affected test targets built successfully. Map/text-fit/GPU tests: 17 cases / 49,363 assertions; theme/font: 20 / 52,203; image crop: 1 / 20; stacking: 6 / 391. All passed.
- 30 RML contracts, four UI guards, clang-format 21.1.8, Cppcheck 2.22.0 and whitespace checks passed.
- Layout tests cover eight viewport sizes and 75/100/125/150% in both themes, actual font metrics, unchanged glyph proportions, theme/locale reloads, long-to-short text and stable pixel/em/calc letter spacing.
- In-game checks cover the maps/scales/locales above, Gens and live theme switches, world movement through the banner, inventory I/Esc, chat text entry and logout. The known existing chat-Esc behavior was not changed.
- Independent source review found no remaining actionable findings. The branch merges without conflicts with main 55a7b445; the combined merge tree was not built.
- Fixture server source: ef41c19335ea7c9cb19627114b5d98761ac1a6be; image: sha256:0d99ebf1ccad15037b5c98c9fe8979d6b8dbac23812feae2277a0b277b5c5ffa. This is banner validation against that isolated fixture.
- Linux/macOS, the full unrelated suite and current-fork OpenMU integration were not tested. Existing unrelated Modern RCSS warnings and missing lucky_coin.rcss remain.
- Mosch0512 contribution authorship and MU-derived font/ornament provenance remain intact. The asset shipping decision belongs to maintainers; Noto Sans JP retains its separate OFL license.

Main SHA-256: `ea491dba701b93ec0f23dbf61b9baa1c3d133efceb4a031d1766a855805ef058`. Screenshot SHA-256 values are in [manifest.json](manifest.json).
