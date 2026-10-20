# BMD model reader notices

The native v14 cipher wrapper, dependency wiring and initial cipher tests are
adapted from [MuMain PR #804](https://github.com/sven-n/MuMain/pull/804), commit
`2adb7e6aa6dd262cddd1b7b05ded9d74ab03e063` by ezequielzacca. This is an upstream
MuMain contribution. The wrapper uses Crypto++ implementations of eight ciphers.

`LegacyMarsDecryption`, its independent synthetic vector, and the bounded body
validation rules are adapted from the local MIT-licensed BmdConverter at commit
`d7a8b73dc0c3eb8389a889da77bfc63af207c5c5`. Its [MIT notice](licenses/BmdConverter-MIT.txt)
is retained. The compatibility mask is expressed as runs of equal bits; key
expansion uses public-domain Crypto++ code. S-box and cipher rounds stay in
Crypto++. The diagnostic oracle was xulek/muonline-bmd-viewer commit
`3f11bf89f3702261174d8efa1436d56a15c7d97a`; it is not linked or required.

The Crypto++ [license](licenses/CryptoPP.txt) is retained. Individual source files
are public domain; the compilation is distributed under Boost Software License
1.0. Tests used Crypto++ 8.9.0 built from source. Production builds resolve
Crypto++ through vcpkg or the system package manager, as documented for building.

No decompiled LegendMU server implementation, game binaries, model bytes or
textures are incorporated. Private model files are external test inputs only.
These software notices do not grant any rights to redistribute game assets.
