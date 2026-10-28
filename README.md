# PR #812: bundled font and per-map ornament integration

Client: [`6b80fa42`](https://github.com/Blaaszkovsky/MuMain/commit/6b80fa4241a660f41a23081b101822bc963ba1eb), built from a clean checkout.

Raw, unedited screenshots from the Windows client. The code includes Mosch0512’s 11 commits plus configuration validation, map aliases, proportional effect scaling, cached fitting after layout and a Noto Sans JP fallback for characters missing from the TC subset.

| Map, English, 1600 × 900, 100% | Legacy | Modern |
|---|---|---|
| lorencia | [![legacy lorencia](main-legacy-en-100-1600x900-lorencia.jpg)](main-legacy-en-100-1600x900-lorencia.jpg) | [![modern lorencia](main-modern-en-100-1600x900-lorencia.jpg)](main-modern-en-100-1600x900-lorencia.jpg) |
| devias | [![legacy devias](main-legacy-en-100-1600x900-devias.jpg)](main-legacy-en-100-1600x900-devias.jpg) | [![modern devias](main-modern-en-100-1600x900-devias.jpg)](main-modern-en-100-1600x900-devias.jpg) |
| dungeon | [![legacy dungeon](main-legacy-en-100-1600x900-dungeon.jpg)](main-legacy-en-100-1600x900-dungeon.jpg) | [![modern dungeon](main-modern-en-100-1600x900-dungeon.jpg)](main-modern-en-100-1600x900-dungeon.jpg) |
| noria | [![legacy noria](main-legacy-en-100-1600x900-noria.jpg)](main-legacy-en-100-1600x900-noria.jpg) | [![modern noria](main-modern-en-100-1600x900-noria.jpg)](main-modern-en-100-1600x900-noria.jpg) |
| kanturu38 | [![legacy kanturu38](main-legacy-en-100-1600x900-kanturu38.jpg)](main-legacy-en-100-1600x900-kanturu38.jpg) | [![modern kanturu38](main-modern-en-100-1600x900-kanturu38.jpg)](main-modern-en-100-1600x900-kanturu38.jpg) |
| balgass42 | [![legacy balgass42](main-legacy-en-100-1600x900-balgass42.jpg)](main-legacy-en-100-1600x900-balgass42.jpg) | [![modern balgass42](main-modern-en-100-1600x900-balgass42.jpg)](main-modern-en-100-1600x900-balgass42.jpg) |
| gens63 | [![legacy gens63](main-legacy-en-100-1600x900-gens63.jpg)](main-legacy-en-100-1600x900-gens63.jpg) | [![modern gens63](main-modern-en-100-1600x900-gens63.jpg)](main-modern-en-100-1600x900-gens63.jpg) |

## Scale checks

| Resolution / UI setting | Legacy, Kanturu Relics | Modern, Kanturu Relics |
|---|---|---|
| 1280x720 / 75% | [![legacy 1280x720 75%](main-legacy-en-75-1280x720-kanturu38.jpg)](main-legacy-en-75-1280x720-kanturu38.jpg) | [![modern 1280x720 75%](main-modern-en-75-1280x720-kanturu38.jpg)](main-modern-en-75-1280x720-kanturu38.jpg) |
| 1920x1080 / 150% | [![legacy 1920x1080 150%](main-legacy-en-150-1920x1080-kanturu38.jpg)](main-legacy-en-150-1920x1080-kanturu38.jpg) | [![modern 1920x1080 150%](main-modern-en-150-1920x1080-kanturu38.jpg)](main-modern-en-150-1920x1080-kanturu38.jpg) |
| 800x600 / 150% | [![legacy 800x600 150%](main-legacy-en-150-800x600-kanturu38.jpg)](main-legacy-en-150-800x600-kanturu38.jpg) | [![modern 800x600 150%](main-modern-en-150-800x600-kanturu38.jpg)](main-modern-en-150-800x600-kanturu38.jpg) |

UI scale is capped by the shared window-fit rules; 150% is the configured preference, not a promise of 1.5× growth at every resolution.

## Localization

| Locale | Legacy | Modern |
|---|---|---|
| pl | [![legacy pl](locale-legacy-pl-100-1600x900-losttower4.jpg)](locale-legacy-pl-100-1600x900-losttower4.jpg) | [![modern pl](locale-modern-pl-100-1600x900-losttower4.jpg)](locale-modern-pl-100-1600x900-losttower4.jpg) |
| ru | [![legacy ru](locale-legacy-ru-100-1600x900-kanturu38.jpg)](locale-legacy-ru-100-1600x900-kanturu38.jpg) | [![modern ru](locale-modern-ru-100-1600x900-kanturu38.jpg)](locale-modern-ru-100-1600x900-kanturu38.jpg) |
| ja | [![legacy ja](locale-legacy-ja-100-1600x900-kanturu38.jpg)](locale-legacy-ja-100-1600x900-kanturu38.jpg) | [![modern ja](locale-modern-ja-100-1600x900-kanturu38.jpg)](locale-modern-ja-100-1600x900-kanturu38.jpg) |
| zh-TW | [![legacy zh-TW](locale-legacy-zh-tw-100-1600x900-devias.jpg)](locale-legacy-zh-tw-100-1600x900-devias.jpg) | [![modern zh-TW](locale-modern-zh-tw-100-1600x900-devias.jpg)](locale-modern-zh-tw-100-1600x900-devias.jpg) |

## Lifecycle

[![Gens during live theme switch](main-switch-modern-en-100-1600x900.jpg)](main-switch-modern-en-100-1600x900.jpg)

[![Lorencia after leaving Gens](main-legacy-en-100-1600x900-return-lorencia.jpg)](main-legacy-en-100-1600x900-return-lorencia.jpg)

[![Character selection after logout](main-legacy-en-100-1600x900-logout.jpg)](main-legacy-en-100-1600x900-logout.jpg)

[![Visible modern Lorencia after leaving Gens](interactions-return-modern-lorencia-banner.jpg)](interactions-return-modern-lorencia-banner.jpg)

Visible Lorencia return captures in both themes show the Gens warning cleared; the modern return was captured after map loading and same-map reactivation.

## Validation and provenance

Windows x64 Release Main and affected targets built. Map tests: 13 cases / 31,871 assertions; theme tests: 20 / 52,203; opt-in real-GPU Gens retry: 1 / 19; crop: 1 / 20; stacking: 6 / 391. Selected CTest: 17/17; RML contracts: 30/30; four asset guards, changed-line clang-format 21.1.8 and Cppcheck 2.22.0 passed. Asset validation covers all 35 OZT files and 67 map mappings.

Fixture server source: `ef41c19335ea7c9cb19627114b5d98761ac1a6be`, image `sha256:0d99ebf1ccad15037b5c98c9fe8979d6b8dbac23812feae2277a0b277b5c5ffa`. These tests validate the banner against that isolated fixture. They do not establish compatibility with the latest OpenMU fork.

The MU Map Name font and cut-out ornaments derive from original client artwork. Attribution and generation sources are retained in the code PR; the maintainers decide whether these assets may ship.

Linux/macOS builds, the full unrelated test suite and a build of the combined latest-main merge tree were not run. Hosted CI is reported separately in the PR. Earlier Corsiva captures are [available for comparison](https://github.com/Blaaszkovsky/MuMain/blob/08ef148f39d3e099cc2886d82038fa1576d8df74/README.md); their sizes/settings differ.
