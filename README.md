# GbtGDecomp

Readable C++ reconstruction of **Grabbed by the Ghoulies** (Xbox, Rare 2003) that compiles for PC.

This is **not** a matching Xbox decompilation and **not** an emulator. The original `default.xbe` is reverse-engineered; engine systems are rewritten against a PC platform layer and load **your** copy of the game files.

## You must own the game

This repository does **not** contain, and will never distribute:

- Xbox executables (`.xbe`)
- Disc images (`.iso` / `.xiso`)
- Bundles, textures, audio, movies, or any other original assets
- Microsoft XDK headers or RAD Bink

Put a retail dump in a local folder (or extract an ISO later) and point the tools at it.

```text
git clone <this-repo>
cmake -B build
cmake --build build --config Release
.\build\Release\gbtg-catalog.exe --game-dir path\to\extracted\disc --out docs\generated
.\build\Release\gbtg-dump.exe --game-dir path\to\extracted\disc --out dump
.\build\Release\gbtg.exe --game-dir path\to\extracted\disc
.\build\Release\gbtg.exe --game-dir path\to\extracted\disc --headless
```

The folder should look like the Xbox disc data:

```text
game-dir/
  default.xbe
  bundles/common.bnl
  bundles/aid_script/...
  demand/
  loctext/
  movie/
  font/
  xwavebank/
```

`cdx.xbe`, `Videos/`, and `Other PNGs/` are a Microsoft kiosk trailer player. The port ignores them.

Checksums for a known retail layout live in [`manifests/retail_files.json`](manifests/retail_files.json) (hashes and sizes only). `gbtg-catalog` can verify your copy against that list.

## What is here

| Target | Role |
|--------|------|
| `gbtg-catalog` | Classify files, resolve AID hashes, verify the manifest |
| `gbtg-dump` | Unpack BNL bundles; dump textures, models, loctext, wavebanks |
| `gbtg` | PC engine slice: load `common.bnl` + chapter 1 + player, draw a room |

See [`docs/LEGAL.md`](docs/LEGAL.md), [`docs/AID.md`](docs/AID.md), [`docs/FORMATS.md`](docs/FORMATS.md), [`docs/XBE.md`](docs/XBE.md), [`docs/GHIDRA.md`](docs/GHIDRA.md).

## Build

Requirements: CMake 3.20+, a C++20 compiler (MSVC 2022 recommended on Windows).

zlib is fetched at configure time. The engine optionally fetches SDL3 for a window, gamepad orbit camera, and a decoded WBND cue; `--headless` still writes `dump/preview.bmp`.

```text
cmake -B build -DGBTG_WITH_SDL=ON
cmake --build build --config Release
```
