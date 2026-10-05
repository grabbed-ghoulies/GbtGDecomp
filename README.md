# GbtGDecomp

Readable C++ reconstruction of **Grabbed by the Ghoulies** (Xbox, Rare 2003).

This is **not** a matching Xbox decompilation and **not** an emulator. Functions recovered from `default.xbe` in Ghidra are rewritten against a PC layer and load **your** copy of the game files.

## You must own the game

This repository does **not** contain, and will never distribute:

- Xbox executables (`.xbe`)
- Disc images (`.iso` / `.xiso`)
- Bundles, textures, audio, movies, or any other original assets
- Microsoft XDK headers or the RAD Bink SDK

Put a retail dump in a local folder and point the tools at it.

```text
git clone <this-repo>
cmake -B build
cmake --build build --config Release
.\build\Release\gbtg.exe --game-dir path\to\extracted\disc --headless
.\build\Release\gbtg-catalog.exe --game-dir path\to\extracted\disc --out docs\generated
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

Checksums for a known retail layout live in [`manifests/retail_files.json`](manifests/retail_files.json) (hashes and sizes only).

## What is here

| Target | Role |
|--------|------|
| `gbtg` | Reconstructed engine: `WinMain` / `frame_tick`, attract cutscenes, camera, game-select UI plates, `movie/` Bink playback. PC present is DirectX 11 behind `gfx_*` |
| `gbtg-catalog` | Classify files, resolve AID hashes, verify the manifest |
| `gbtg-dump` | Unpack BNL bundles; dump textures, models, loctext, wavebanks |

Ghidra names and recovered VAs: [`docs/XBE.md`](docs/XBE.md), [`docs/GHIDRA.md`](docs/GHIDRA.md), [`docs/generated/ghidra_functions.txt`](docs/generated/ghidra_functions.txt). Also [`docs/AID.md`](docs/AID.md), [`docs/FORMATS.md`](docs/FORMATS.md), [`docs/LEGAL.md`](docs/LEGAL.md).

## Build

Requirements: CMake 3.20+, a C++20 compiler (MSVC 2022 recommended on Windows). zlib is fetched at configure time. On Windows the same configure step fetches an LGPL FFmpeg build and copies its DLLs next to `gbtg.exe`, which is what decodes `movie/` BIKi files. The run command is unchanged.

```text
cmake -B build
cmake --build build --config Release
.\build\Release\gbtg.exe --game-dir OriginalFiles --headless
```

`--headless` runs one `frame_tick` and exits. `--frames N` runs N ticks. `--screenshot file.bmp` writes the last presented frame (DirectX 11 readback). `--scene gameselect` (or a scene-order index) boots that scene instead of the attract reel. A window opens when neither `--headless` nor `--screenshot` is set. SDL is an optional quit window only when the DirectX backend is not built (`-DGBTG_WITH_SDL=ON`, `-DGBTG_GFX=null`).

`gbtg.ini` sits beside `gbtg.exe`. The first launch writes one if it is missing (an existing file in the working directory is used instead). `[Video]` `Width` and `Height` set the window, default 960 by 540. `[Audio]` `MasterVolume` is 0 to 100 and scales all sound. Restart after editing.

Analyze `default.xbe` in Ghidra, rename a function, add it to `docs/XBE.md`, then reconstruct it under `src/engine/`.
