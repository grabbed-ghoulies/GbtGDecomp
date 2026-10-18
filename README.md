# Grabbed by the Ghoulies

A C++20 reconstruction of **Grabbed by the Ghoulies** (Rare, Xbox, 2003) for PC.

The project translates functions recovered from the retail Xbox executable into
readable C++, preserving their behavior and running them against the original
game data. It is not a binary-matching Xbox decompilation or an emulator.

## How it works

Game behavior comes from translated retail functions, driven by retail state in
the original call order from `frame_tick`. Host code provides platform boundaries
for file IO, graphics, input, memory, thread-local storage and audio.

Native rendering records Xbox D3D commands through `SceneD3DDevice`. A Direct3D 11
presenter turns those records into PC draws. Movie playback uses FFmpeg's Bink
decoder; the RAD Bink SDK is not required.

The repository also includes a separate runtime scene inspector, asset inspection
tools, reconstruction tests and Ghidra/xemu tooling.

## Game data

You must supply your own extracted retail copy of the game. This repository does
not distribute Xbox executables, disc images, original assets, Microsoft XDK
headers or the RAD Bink SDK.

Place the extracted disc files in `OriginalFiles/`, or pass another directory
with `--game-dir`. The runtime expects an extracted directory, not an ISO:

```text
OriginalFiles/
  default.xbe
  bundles/
    common.bnl
    aid_script/
    aid_objparams/
  demand/
  font/
  loctext/
  movie/
  xwavebank/
```

See [Disc layout](docs/DISC_LAYOUT.md) for details. The
[retail manifest](manifests/retail_files.json) records hashes and sizes for a
reference dump without including its contents. Kiosk content such as `cdx.xbe`
and promotional videos is separate from the game data used by the port.

## Build

The Windows runtime uses Direct3D 11, XInput and XAudio2. Use a C++20 toolchain
with the Windows SDK (Visual Studio 2022 is recommended), CMake 3.20 or newer,
and Git. Run these commands from the repository root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target gbtg
.\build\Release\gbtg.exe --game-dir OriginalFiles
```

These examples use the Visual Studio generator. With a single-configuration
generator, set `CMAKE_BUILD_TYPE` when configuring and adjust executable paths.

### Faster builds with presets

`CMakePresets.json` keeps the day-to-day game build separate from the scene
inspector. A plain `cmake -S . -B build` still builds everything, including the
inspector, which compiles the engine a second time with `GBTG_INSPECTOR_APP`.
The presets turn the inspector off except in their own inspector directories:

```powershell
cmake --preset vs                     # build/: game, tools and tests
cmake --build --preset game           # Release gbtg
cmake --build --preset tests          # everything plus gbtg-transition-tests
ctest --preset tests

cmake --preset vs-inspector           # build-inspector/: adds gbtg-inspector
cmake --build --preset inspector
```

Ninja presets (`ninja-release`, `ninja-relpdb`, `ninja-debug` and
`ninja-inspector`, under `out/`) need the MSVC x64 environment.
`tools\build\ninja.ps1` loads it through `vswhere` and then configures and builds:

```powershell
tools\build\ninja.ps1                                         # ninja-release, gbtg
tools\build\ninja.ps1 -Preset ninja-relpdb -Build ninja-relpdb-tests
```

Visual Studio builds compile each project's sources in parallel (`/MP`). The
library and runtime targets use the precompiled header `src/pch/gbtg_pch.h`;
it may hold only stable standard headers, because editing it rebuilds every TU.
Copies of the fetched sources in `cache/_deps/<name>-src` (zlib, imgui,
ffmpeg_lgpl) are used in place of a download, so new build directories configure
offline. Seed them once from `build/_deps/*-src`.

CMake fetches zlib and, for the D3D11 backend, Dear ImGui. Windows builds also
fetch an LGPL FFmpeg build and copy its DLLs beside the executable. Initial
configuration requires network access unless dependencies are already cached.

Useful build options:

- `-DGBTG_GFX=null` selects the non-rendering backend. D3D11 is the default on
  Windows; the null backend is the default elsewhere.
- `-DGBTG_WITH_SDL=ON` adds an optional SDL3 window loop for the null backend.
- `-DGBTG_BUILD_INSPECTOR=OFF` omits the separate inspector targets.
- `-DBUILD_TESTING=OFF` omits test targets.

The null backend is useful for development checks; it does not provide the
Windows runtime's graphics presentation.

## Run and capture

With no scene override, the runtime follows the retail boot sequence. `--scene`
accepts a scene-order index or a matching scene name:

```powershell
.\build\Release\gbtg.exe --game-dir OriginalFiles --scene gameselect
.\build\Release\gbtg.exe --game-dir OriginalFiles --scene chapter1_scene1_2playcam
```

For a bounded run and a capture through the D3D11 backend:

```powershell
.\build\Release\gbtg.exe --game-dir OriginalFiles --headless --frames 900 --screenshot capture.bmp
```

- `--headless` hides the window and defaults to one tick unless `--frames N`
  is supplied. Headless runs use 10% master volume.
- `--frames N` limits the number of logic ticks. Like retail, the logic ticks at 30 Hz (1/30 s a tick), so
  900 frames is 30 seconds.
- `--screenshot file.bmp` saves the final presented frame and hides the window.
  Supply `--frames N` to capture beyond the default single tick.
- `--width W`, `--height H` and `--vsync N` override video settings.

A successful headless run is a crash check, not proof that rendering, audio or
reconstructed behavior is correct.

### Configuration and diagnostics

The runtime reads `gbtg.ini` beside the executable, falling back to an existing
file in the working directory. If neither exists, it creates a default beside
the executable. Edit it and restart:

```ini
[Video]
Width=960
Height=540
VSync=1

[Audio]
MasterVolume=100

[Game]
Language=English
```

`MasterVolume` accepts 0–100. Languages are English, French, German, Spanish and
Italian; a missing localized text directory falls back to English.

Debug D3D11 game builds include ImGui diagnostics: **F8** opens the menu and
**F12** toggles the timing overlay. `--debug-ui` opens diagnostics at startup.
Release game builds and the null backend omit this UI.

## Scene inspector

`gbtg-inspector` is a separate Windows/D3D11 application for inspecting scenes
constructed by the native runtime. It provides a free-flight camera, object and
marker inspection, render diagnostics, captures and sound preview. Its ImGui
interface is available in both Debug and Release builds.

```powershell
cmake --build build --config Release --target gbtg-inspector
.\build\Release\gbtg-inspector.exe --game-dir OriginalFiles --scene chapter1_scene1_2playcam
```

The inspector presents a static view after scene initialization. It can label
scenes as partial and report omitted content when reconstruction errors occur;
this inspection policy is separate from normal gameplay. See
[Runtime scene inspector](docs/INSPECTOR.md) for controls and limitations.

## Data tools

Build the tools separately, or omit `--target` when building to build all default
targets:

```powershell
cmake --build build --config Release --target gbtg-catalog gbtg-dump gbtg-model-check
```

- **`gbtg-catalog`** classifies disc files, resolves AID hashes and verifies a
  supplied manifest.
- **`gbtg-dump`** extracts BNL resources, including textures, models, localized
  text and wavebanks. `--dialogue-heads` exports animated dialogue head frames
  as PNGs with transparency.
- **`gbtg-model-check`** scans bundled models and reports decoding issues.

```powershell
.\build\Release\gbtg-catalog.exe --game-dir OriginalFiles --out docs\generated
.\build\Release\gbtg-dump.exe --game-dir OriginalFiles --out dump --dialogue-heads
```

Keep extracted assets local; they remain original game content.

## Tests

Build and run the CTest suite:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Runtime transition tests are built explicitly and accept the game directory as
a positional argument:

```powershell
cmake --build build --config Release --target gbtg-transition-tests
.\build\Release\gbtg-transition-tests.exe OriginalFiles --scene-start
```

Tests that open an audio device must set master volume to 10% before creating
the master voice. Visible or audible changes also require captures and direct
inspection; passing tests alone does not establish feature completeness.

## Contributing

Read [AGENTS.md](AGENTS.md) before changing reconstructed code.

Work inward from a live native entry point and translate the first missing
callee reached. Use the decompile in `cache/ghidra_frontier/<VA>.txt`, preserving
its branches, comparisons and offsets. Export missing dumps using the read-only
Ghidra workflow in `AGENTS.md`; do not rename Ghidra symbols or write the project.
Missing callees must raise when reached rather than return invented values.

Keep game logic on the native path. Host code supplies boundaries and
presentation; older host implementations are tracked in
[Legacy host code](docs/LEGACY_HOST_CODE.md) for replacement. Keep
`src/engine/game.cpp` as the thin `frame_tick` caller, organize code by subsystem,
use `.h` headers and Allman braces, and retain Xbox address comments on
reconstructed declarations and definitions.

Never stage or distribute `OriginalFiles/`, retail payloads (`*.xbe`, `*.bnl`,
`*.iso`, `*.original`) or Ghidra project databases. Before editing any file in
`OriginalFiles/`, preserve an untouched `.original` backup and document the edit
and restoration steps in [Original file changes](docs/ORIGINAL_FILES.md).

## Reference

- [Executable map](docs/XBE.md) and [Ghidra workflow](docs/GHIDRA.md)
- [AID resource identifiers](docs/AID.md) and [File formats](docs/FORMATS.md)
- [Native scene presentation](docs/NATIVE_SCENE_PRESENT.md),
  [Transitions](docs/NATIVE_TRANSITIONS.md) and [Audio](docs/NATIVE_AUDIO.md)
- [xemu runtime oracle](docs/XEMU.md) and [Oracle tooling](tools/oracle/README.md)
- [Legal notes](docs/LEGAL.md)
