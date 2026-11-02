# Architecture

This page is the map: how a frame runs, where each subsystem lives, and where
translated retail code ends and host code begins. The rules behind it are in
[`AGENTS.md`](../AGENTS.md); terms are defined in the [glossary](GLOSSARY.md).

## The one rule that shapes everything

Game behavior comes only from **translated retail functions**, running on
retail state in the retail call order. Each translated function carries an
`// Xbox 0x........` comment naming the address it was translated from, and
mirrors the Ghidra dump in `cache/ghidra_frontier/<VA>.txt` branch for branch.

**Host code** exists only at boundaries the retail code itself crosses (file
IO, D3D, XInput, heap, thread-local storage, XACT audio) and for presenting
what the retail code produced. Older host code that still re-implements game
behavior is tracked in [Legacy host code](LEGACY_HOST_CODE.md) for removal.

## Boot and the frame

```text
main()                        src/engine/main.cpp      command line, gbtg.ini, plugins
 └─ Game::init()              src/engine/game.cpp      retail boot: WinMain 0x00032390
     ├─ AIDs, executable resources, movies, audio, input, frontend
     ├─ SceneFactoryRuntime   the object factory and actor services (boot-owned)
     └─ script_request()      queue the first scene's aid_script
 └─ run_platform()            src/engine/runtime/platform.cpp  window + loop
     └─ Game::tick() → frame_tick()                    retail 0x00033B50
         ├─ clock, pads                    0x000FC030, 0x000FA3C0
         ├─ script_pump                    0x0011E4C0: scene load / script / dialogue
         ├─ run_scene_pass, HUD, effects   0x0011E640, 0x00132210, 0x00060D50
         ├─ audio, movies, file copies, saves
         └─ present_views                  0x00033A90 → each camera slot's view
```

`src/engine/game.cpp` is deliberately thin: it is the retail `frame_tick`
caller and nothing more. Logic belongs in the subsystem modules it calls.

## Rendering path

```text
translated draw code ──► SceneD3DDevice ──► recorded draws ──► presenter ──► D3D11 / Vulkan / null
 (retail D3D calls)     graphics/scene_d3d_device.h          graphics/scene_d3d_present.cpp
                                                             graphics/gfx_d3d11.cpp, graphics/vulkan/
```

The translated code issues Xbox D3D calls. `SceneD3DDevice` keeps the device
state the retail code reads back and records each draw in issue order. A
presenter replays those records with PC draws. New rendering work extends the
presenter; game code never calls `gfx_*` directly. See
[Native scene presentation](NATIVE_SCENE_PRESENT.md) and [Vulkan](VULKAN.md).

## Source layout

| Folder | What lives there |
|---|---|
| `include/gbtg/` | Public headers shared across targets: types, resources, models, `retail_field.h`, the plugin API |
| `src/gbtg/` | `gbtg_lib`: standalone parsers for disc formats (bundles, models, textures, text). No game logic. See [File formats](FORMATS.md) |
| `src/engine/main.cpp`, `game.cpp` | Entry point and the `frame_tick` caller |
| `src/engine/runtime/` | Platform loop, clock, AID lookup, the CRT and XAPI functions the retail code linked against |
| `src/engine/scene/` | Scene loading, teardown, scene controls, backgrounds, lighting, and the actor services that wire it all together |
| `src/engine/entities/` | Actors: bodies, minds, weapons, players, ghoulies, factory owners, collision. The largest subsystem |
| `src/engine/navigation/` | Grids, path search, floor tracking, and mind navigation |
| `src/engine/scripts/` | The scene script VM: conditions, actions, event sequences, dialogue actions |
| `src/engine/camera/` | Cameras, camera shakes, display views |
| `src/engine/frontend/` | Menus, game select, HUD, scoreboard, dialogue, the virtual keyboard |
| `src/engine/transitions/` | The chapter-book and slime transitions between scenes. See [Transitions](NATIVE_TRANSITIONS.md) |
| `src/engine/cutscenes/` | The translated cutscene runtime. See [Cutscenes](NATIVE_CUTSCENES.md) |
| `src/engine/audio/` | Native audio and the XAudio2 PCM boundary. See [Audio](NATIVE_AUDIO.md) |
| `src/engine/video/` | Bink movie playback through FFmpeg, and the queued file copies the frame pumps |
| `src/engine/graphics/` | The recording D3D device, shader translation, render passes, and the D3D11/Vulkan presenters |
| `src/engine/input/` | Pad input boundary |
| `src/engine/saves/` | Save games |
| `src/engine/inspector/` | The separate `gbtg-inspector` app. See [Inspector](INSPECTOR.md) |
| `src/engine/plugins/`, `src/plugins/` | Plugin host and the bundled plugins (non-retail builds only). See [Plugins](PLUGINS.md) |
| `src/tools/` | `gbtg-catalog`, `gbtg-dump`, `gbtg-model-check`, `gbtg-xemu-patch` |
| `tests/` | Test executables; `gbtg-transition-tests` covers scene runs |
| `tools/` | Ghidra scripts, the retail-code [oracle](../tools/oracle/README.md), xemu tooling, layout census scripts |

Each module is a `.cpp` with a narrow `.h` beside it. Headers are `.h`, never
`.hpp`.

## Retail objects in memory

Translated code still reads many retail objects as byte buffers laid out as on
the Xbox. Fields are accessed through named `RetailField` / `RetailRecord`
descriptors declared beside the owning type, not literal offsets. Once a type
is fully named it becomes a plain struct with `+0x...` comments. See
[Retail layouts](RETAIL_LAYOUTS.md).

## Where to go next

- Translating a function: [`AGENTS.md`](../AGENTS.md), [Ghidra workflow](GHIDRA.md), [Executable map](XBE.md)
- Checking against the original: [xemu](XEMU.md) and the [oracle](../tools/oracle/README.md)
- Every other document: the [docs index](README.md)
