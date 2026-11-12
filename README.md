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

For a guided tour of the code, see [Architecture](docs/ARCHITECTURE.md). Terms
such as AID, body, mind and "unbound" are defined in the [Glossary](docs/GLOSSARY.md).

## Completion

Last counted 2026-11-12. The game plays from boot through the attract, game select and every chapter to the final
score and credits on translated code. It is not complete: code the tests and playthroughs have not reached is still
untranslated, and raises when reached.

**Translated game code.** Game code is `.text` below `0x0014A840`. The statically linked XAPI and C runtime begin
there, and they, D3D8, XGRAPHICS, DirectSound, XACT and Bink are boundaries, translated only where the game calls
them.

| Measure | Count |
|---|---|
| Game functions known | 5,091 (Ghidra's 4,078, plus 1,013 more found while translating) |
| Translated | 4,720 (92.7%) |
| Untranslated, known | 371, about 48 KB of code |

Ghidra's analysis missed about a fifth of the functions translated so far, so the untranslated count is a lower
bound. The known untranslated functions, grouped by the source area of their nearest translated neighbour:

| Area | Translated | Untranslated | Untranslated bytes |
|---|---:|---:|---:|
| entities | 2,441 | 141 | 14,120 |
| frontend | 617 | 99 | 20,090 |
| scene | 450 | 22 | 2,086 |
| include (shared headers) | 247 | 24 | 2,456 |
| graphics | 235 | 11 | 1,164 |
| scripts | 169 | 21 | 1,209 |
| video | 47 | 18 | 2,558 |
| saves | 91 | 1 | 63 |
| camera, navigation, audio, cutscenes, transitions, runtime, gbtg, engine and other | 423 | 34 | 4,663 |
| **Total** | **4,720** | **371** | **48,409** |

**Where the untranslated code sits.** `tools/census/reachability.py` classes each of the 371 by the call graph
Ghidra resolves (counted 2026-11-12):

| Class | Functions | Bytes | Meaning |
|---|---:|---:|---|
| Frontier | 158 | 15,488 | A translated function calls or references it. 142 of these are named in a comment in `src` (most are small helpers translated inline in their caller), 7 on a line that raises, 9 nowhere |
| Reachable | 43 | 5,597 | WinMain reaches it, but only through other untranslated functions |
| Table | 65 | 15,263 | Reached only from a vtable or handler table, or from the C runtime's initializer list |
| Unreached | 97 | 10,245 | Nothing resolved reaches it: dead code, or a computed call Ghidra did not resolve |
| Fragment | 8 | 1,816 | Not a function: Ghidra split a translated function's body |

The largest group still missing is the retail resource loader, which the host replaces with a synchronous bundle
store and its own registry (the boot's provider registrations `0x00035A90`, `0x00034C30`, `0x00033CF0` and
`0x00035010`, the `common.bnl` load `0x001243B0`, and the pending-record path `0x00034290`, `0x000348D0`,
`0x00034110`).
The boot's resets (`0x000FAD20`, `0x000FA210`, `0x000FF040`, `0x00038600`, `0x00048980`, `0x000F2B60`,
`0x0011EA50`, `0x00133B10`, `0x00133B70`) and the language choice `0x000FF1C0` run in `Game::init` in retail
order. The model library setup `0x000F8740` and the render targets `0x000620C0` run in that order, each whole, when
the boot builds the scene services. Each scene starts through the whole of its startup `0x0011CBD0`.

**Object factory types.** 164 of the 168 types with a getter (`0x0030D41C`) are installed. Not installed: 0x0E,
0x11, 0x2F and 0x6C (by the table's name rows, an effect emitter, the explode effect, the dust-up effect and the
floored state). Nothing in retail makes them: no objparams template, bundle or code names types 0x0E, 0x11 or 0x6C,
and nothing names type 0x2F's only template, `aid_objparams_ghoulies_fx_bandagedustup`. Any of them raises if a
scene creates one.

**What is known to remain.**

- Front end: game-select rows 5 and 7 and the cross-sell kiosk rows 12 to 15 (nothing in the retail game requests
  them).
- Rendering: any model node draw slot or background cloth hook without a translation.
- Unreachable in retail and left untranslated: the depth probe sampling (`0x00065F90`; nothing adds a probe), an
  effect's mode 2 sprite (`0x0005CA10`; `0x000ED390` sends mode 2 elsewhere), a weapon's piece blocks
  (`0x001450A0`; every write of weapon +0xD9C stores zero) and the four factory types above.
- Region: NTSC-J starts its first scene but does not run yet ([Regional releases](docs/REGIONS.md)).
- One deliberate deviation from retail, in the widescreen mirror pass ([Retail deviations](docs/RETAIL_DEVIATIONS.md)).
- Retail objects still read through raw offsets rather than named fields ([Retail layouts](docs/RETAIL_LAYOUTS.md)).

**Test coverage.** 108 of the 125 driven scene modes in `gbtg-transition-tests` play their scene to its end. The 17
others fail because the scripted driver loses fights, gets stuck or breaks an event's rule, not on untranslated
code; see [Player scenes](docs/PLAYER_SCENE.md). No host stand-in for game behavior remains.

To recount, list Ghidra's functions with the read-only headless run of `tools/ghidra_scripts/ListFunctions.java`
(the command form is in `AGENTS.md`), then run `python tools/census/completion.py`. For the reachability classes,
also run `tools/ghidra_scripts/ListReferences.java` the same way, then `python tools/census/reachability.py`
(`--all` lists every function with a path from WinMain or a table; the full list is written to
`cache/census/reachability.txt`).

## Game data

You must supply your own extracted retail copy of the game. This repository does
not distribute Xbox executables, disc images, original assets, Microsoft XDK
headers or the RAD Bink SDK.

The PAL and NTSC (USA) releases are supported; they are the same build. The
NTSC-J (Japan) release is a later build with a different executable layout. It
is read through an address map and starts its first scene, but does not run yet.
See [Regional releases](docs/REGIONS.md).

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

cmake --preset vs-retail              # build-retail/: the game as shipped
cmake --build --preset retail-game
```

Builds include plugin support by default. The retail presets (`vs-retail` and
`ninja-retail`) set `GBTG_RETAIL_GAME_ONLY=ON`, which compiles out plugins and
every other non-retail feature.

Ninja presets (`ninja-release`, `ninja-retail`, `ninja-relpdb`, `ninja-debug`
and `ninja-inspector`, under `out/`) need the MSVC x64 environment.
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

CMake fetches zlib and, for the D3D11 or Vulkan backend, Dear ImGui (used by the
inspector and the debug UI plugin). Windows builds also
fetch an LGPL FFmpeg build and copy its DLLs into `thirdparty/` beside the
executable, which delay-loads them from there. Initial
configuration requires network access unless dependencies are already cached.

Useful build options:

- `-DGBTG_GFX="d3d11;vulkan"` lists the graphics backends compiled into `gbtg`;
  the first is the default. D3D11 is the default list on Windows and nothing
  elsewhere. The non-rendering null backend is always built. Choose one at run
  time with `--gfx` or `Backend=` in `gbtg.ini`. Vulkan needs the LunarG SDK; see `docs/VULKAN.md`.
- `-DGBTG_WITH_SDL=ON` adds an optional SDL3 window loop for the null backend. It is on, and required, off
  Windows, where SDL3 owns the window and input. Linux and Steam Deck builds are described in `docs/LINUX.md`.
- `-DGBTG_BUILD_INSPECTOR=OFF` omits the separate inspector targets.
- `-DGBTG_OPTIMIZE=ON` compiles the host presenter optimizations (see
  `docs/RENDERING_PERFORMANCE_PLAN.md`). The default, OFF, builds the retail
  presenter path. Both paths record the same native draws, and their output matches.
  Use a separate build tree, for example `cmake -B build-opt -DGBTG_OPTIMIZE=ON`.
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
- `--fullscreen` and `--windowed` override `[Video] Fullscreen` in `gbtg.ini`. Fullscreen covers the monitor the
  cursor is on (a borderless window on Windows); `Width` and `Height` stay the render size.
- `--gfx d3d11|vulkan|null` picks a compiled-in graphics backend.

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
Widescreen=0
Backend=

[Audio]
MasterVolume=100

[Game]
Language=English
```

`Widescreen=1` (or `--widescreen`) plays in 16:9, as the Xbox's widescreen video
setting does: the game's views take its 16:9 aspect row and the presenter shows
the 640x480 frame at 16:9 (anamorphic). Views the game fits as 4:3, such as the
front end's book, stay pillarboxed, as on the Xbox. `MasterVolume` accepts 0–100. Languages are English, French, German, Spanish and
Italian; a missing localized text directory falls back to English.

Console output is also written to `gbtg.log` beside the executable
(`gbtg-inspector.log` for the inspector). The file is replaced at every boot, and
each line starts with `[INFO]`, `[WARN]` or `[ERROR]` and the time since boot.
`--verbose` also shows `[VERBOSE]` lines, which are dropped otherwise.

An error or crash that ends an interactive run also shows a dialog with the message and the log's path. Runs that
are tests or captures (`--headless`, `--screenshot`, `--frames`, `--render-profile`,
`--validate-render-boundary`) never show one, nor does any run with the `GBTG_NO_ERROR_DIALOG` environment
variable set.

The ImGui diagnostics are the `debug_ui` plugin, so they exist only in plugin
builds (not the retail presets) with a D3D11 or Vulkan backend: **F8**
opens the menu and **F12** toggles the timing overlay (shown at start in Debug,
hidden in Release). `OpenMenu = 1` in `plugins/debug_ui/plugin.ini` opens the menu
at startup. See [Plugins](docs/PLUGINS.md).

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
presentation; the older host implementations are retired. Keep
`src/engine/game.cpp` as the thin `frame_tick` caller, organize code by subsystem,
use `.h` headers and Allman braces, and retain Xbox address comments on
reconstructed declarations and definitions.

Never stage or distribute `OriginalFiles/`, retail payloads (`*.xbe`, `*.bnl`,
`*.iso`, `*.original`) or Ghidra project databases. Before editing any file in
`OriginalFiles/`, preserve an untouched `.original` backup and document the edit
and restoration steps in [Original file changes](docs/ORIGINAL_FILES.md).

## Reference

Start with [Architecture](docs/ARCHITECTURE.md) and the [Glossary](docs/GLOSSARY.md).
The [documentation index](docs/README.md) lists every document.

- [Executable map](docs/XBE.md) and [Ghidra workflow](docs/GHIDRA.md)
- [AID resource identifiers](docs/AID.md) and [File formats](docs/FORMATS.md)
- [Plugins](docs/PLUGINS.md) (non-retail builds only) and [example plugins](examples/plugins/README.md)
- [Native scene presentation](docs/NATIVE_SCENE_PRESENT.md),
  [Transitions](docs/NATIVE_TRANSITIONS.md) and [Audio](docs/NATIVE_AUDIO.md)
- [xemu runtime oracle](docs/XEMU.md) and [Oracle tooling](tools/oracle/README.md)
- [Legal notes](docs/LEGAL.md)
