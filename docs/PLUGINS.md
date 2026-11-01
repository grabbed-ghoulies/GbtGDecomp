# Plugins

Plugins add files and features to the port without touching `OriginalFiles/`. They exist only in builds configured
with `-DGBTG_RETAIL_GAME_ONLY=OFF`. The default retail build compiles none of this code, and every call into it is
inside `#if !defined(GBTG_RETAIL_GAME_ONLY)`. Behaviour retail does not have belongs in a plugin, so the retail build
stays the game as shipped.

A plugin is a folder with a `plugin.ini`. It can carry any of:

- **Disc files and AIDs** that replace or add to the game's files, including its own loctext (see [Layout](#layout)).
- **A library** (`.dll` on Windows, `.so` elsewhere) with features, written against the C interface in
  `include/gbtg/plugin_api.h` (see [Plugin libraries](#plugin-libraries)). It can be built in a repository of its own
  (see [A plugin repository of its own](#a-plugin-repository-of-its-own)).
- **Switches for compiled-in features** (see [Compiled-in features](#compiled-in-features)).

## Where plugins live

At boot, before the game opens any disc file, `gbtg` reads every folder under:

1. `plugins/` beside the executable, where the build puts the plugins in `src/plugins/`;
2. `plugins/` in the host data directory (`$GBTG_DATA_DIR` or `~/.local/share/gbtg` off Windows; on Windows it is
   the executable's folder, so this is the same folder). A plugin id already loaded from the first folder is skipped.

| Option | Effect |
|--------|--------|
| `--plugins-dir <dir>` | Read plugins from `<dir>` only. Headless runs load plugins only with this option. |
| `--no-plugins` | Load no plugins. |

`--headless` runs are tests, so they load no plugins unless `--plugins-dir` names a folder.

## Layout

```text
plugins/
  my_plugin/
    plugin.ini                     required
    my_plugin.dll                  optional library, named by [Plugin] Library
    loctext/english/0e591f50       the plugin's own loctext, one file per language
    aid/aid_texture_ghoulies_foo   a loose resource named by its AID
    demand/0123abcd                any disc file, at its disc path
    bundles/aid_script/foo.bnl
```

- **`plugin.ini`** is required. A folder without one is skipped with a warning. A UTF-8 byte order mark is fine.
- **Disc paths.** Any file in a subfolder other than `aid/` replaces or adds the disc file at the same path under the
  game folder. Matching ignores case. Files directly beside `plugin.ini` (the library, a readme) are the plugin's
  own, not disc files. Names starting with `.` are ignored.
- **`aid/`** holds loose resources (the same format as `demand/` files, see [FORMATS.md](FORMATS.md)) named by their
  full AID string instead of its hash. The loader hashes the name ([AID.md](AID.md)) and places the file where the
  game looks for a loose AID: `loctext/<language>/<hash>` for `aid_loctext_*` (the configured language only),
  `font/<hash>` for `aid_font_*`, and `demand/<hash>` for everything else. An `aid/` file whose AID is also an entry
  of a bundle the game loads replaces that entry too: the entry's header and data spans point at the plugin's file,
  the way `AidStore::load_aid` serves demand files.

## plugin.ini

```ini
[Plugin]
Name = My Plugin        ; default: the folder name
Version = 1.0
Enabled = 1             ; 0 skips the plugin
Priority = 0            ; higher loads later, so its files win
Library = my_plugin     ; a .dll / .so beside plugin.ini; without an extension the platform's is added
Features = some_feature ; compiled-in features to switch on, comma separated

[MySettings]            ; any other section: settings the library reads with host->setting
Key = value
```

Plugins load in order of `Priority`, then folder name. When two plugins supply the same disc path, the later one wins
and `--verbose` logs the replacement.

## Plugin libraries

A library exports one C function, called once at boot before `Game::init`:

```c
#include "gbtg/plugin_api.h"

GBTG_PLUGIN_EXPORT int gbtg_plugin_load(const GbtgPluginHost* host)
{
    static const GbtgFeature feature = {"my_feature", "one line for the log", my_boot, my_frame, NULL, NULL, NULL};
    if (host->api_version != GBTG_PLUGIN_API_VERSION)
    {
        return 0; /* Built for another gbtg: stay unloaded. */
    }
    return host->add_feature(host->plugin, &feature);
}
```

A `GbtgFeature` has four hooks, any of them `NULL`:

| Hook | When |
|------|------|
| `boot` | Once, after `Game::init`. |
| `frame` | Each frame, after the retail `frame_tick` returns, so it never changes the order inside a frame. |
| `graphics_ready` | Once, after the graphics device is up and before the first frame. |
| `graphics_shutdown` | Once, after the last frame and before the device is released. |

The game is an opaque `GbtgGame*`. The host table gives the library:

| Call | Does |
|------|------|
| `log`, `setting`, `plugin_dir`, `executable_dir` | Logging, its `plugin.ini` values, its folder and the executable's. |
| `language` | The game's language as its loctext folder name (`english`, `french`, ...). |
| `loctext` | A key from an `aid_loctext_` resource in the game's language, looked up with the game's own loctext code (0x000FF410), as UTF-8. The resource is read through the plugin overlay, so a plugin ships its own text as `loctext/<language>/<hash>`. A missing file returns 0 instead of reaching the retail missing-resource screen. |
| `has_window`, `show_loading_screen` | Whether the run has a window, and a black screen with a line of text in the game's startup font and a progress bar. |
| `prepare_vertex_program`, `prepare_pixel_definition`, `compiled_shaders` | Compile a vertex program or 60-word pixel definition before its first draw, and list what the backend has compiled. |
| `scene_name`, `frame` | The loaded scene script and the frame count. |
| `add_overlay`, `graphics_device` | Register an overlay (below), and the window and D3D11 or Vulkan device handles its renderer needs, from `graphics_ready` until `graphics_shutdown` returns. |
| `runtime_info`, `native_objects`, `delayed_callbacks`, `resource_bundles` | Read-only diagnostics: the clock, script and presenter state, the native scene factory's objects, the delayed callback slots and the cached bundles. |
| `scene_count`, `scene_at`, `request_scene` | The scene order, and a scene request the way the game queues its next script (0x0011EB30). |

An **overlay** (`GbtgOverlay`) is host presentation drawn over each presented frame, such as the debug UI:

| Hook | When |
|------|------|
| `window_event` | Each window message (`GbtgWin32Message`) or `SDL_Event`, before the game. Returning 1 takes it from the game. |
| `captures_input` | Returning 1 keeps the keyboard from the game, and Escape no longer quits. |
| `build` | On the game thread after each tick and before each repeated present, with the host seconds since the last call. The previous build has been presented by then (the Vulkan render thread included). |
| `has_draws`, `render` | When the backend presents: D3D11 with the back buffer bound and a null command, Vulkan with the `VkCommandBuffer` inside dynamic rendering onto the swapchain image. The held frame `present_again` repeats is kept without the overlay, so it is drawn again each time. |

The overlay owns its renderer: it creates it from `graphics_device` in its feature's `graphics_ready` hook and
releases it in `graphics_shutdown`, after the host has waited for the last present. Retail builds have none of these
hooks; their backends present the frame alone.

The interface is plain C, so a library can be written in C or C++ and built with any compiler for the platform; no
C++ exception or standard library type crosses it. Libraries stay loaded until the process exits. Bump
`GBTG_PLUGIN_API_VERSION` for any change that breaks existing libraries; adding members at the end of
`GbtgPluginHost` does not, as a library can check `host->size`.

### Building one

`cmake/GbtgPlugin.cmake` builds a plugin library, in gbtg's build or in a project of its own:

```cmake
gbtg_add_plugin(gbtg_my_plugin ID my_plugin
    SOURCES my_plugin.cpp
    PLUGIN_INI plugin.ini        # default: plugin.ini beside the CMakeLists.txt
    FILES_DIR files)             # optional: loctext, AIDs and other disc files, copied as they are
```

The plugin is laid out as gbtg reads it, in `<output>/<id>/`: the library (no `lib` prefix, as `plugin.ini` names
it), `plugin.ini` and the contents of `FILES_DIR`. `<output>` is `OUTPUT_DIR` if given, else
`GBTG_PLUGIN_OUTPUT_DIR` (for example a game's `plugins` folder), else `plugins/` beside `gbtg` when gbtg is in the
same build (building `gbtg` then builds the plugin), else `<build>/plugins`. `cmake --install` puts it in
`<prefix>/plugins/<id>/`, so `--prefix` is the folder holding `gbtg.exe`.

Plugins that ship with the game live in `src/plugins/<id>/` and are added in the `GBTG_RETAIL_GAME_ONLY` `else()`
block of `CMakeLists.txt`, as `shader_warmup` is.

### A plugin repository of its own

The examples in `examples/plugins/` are standalone CMake projects; copy one to start a repository. Such a project
needs only `cmake/GbtgPlugin.cmake` and `include/gbtg/plugin_api.h`, laid out as in gbtg's tree (a folder holding
`cmake/` and `include/`), not gbtg's sources or build. Point `GBTG_SDK_DIR` at a gbtg checkout or at a copy of those
two files (a submodule, or the files committed into the repository):

```powershell
cmake -S . -B build -DGBTG_SDK_DIR=C:\src\gbtg
cmake --build build --config Release
cmake --install build --config Release --prefix C:\Games\gbtg   # -> C:\Games\gbtg\plugins\<id>\
```

`-DGBTG_PLUGIN_OUTPUT_DIR=C:\Games\gbtg\plugins` builds straight into the game instead. gbtg's own build also builds
the examples (`GBTG_BUILD_PLUGIN_EXAMPLES`, on by default) into `plugins/` beside `gbtg`.

### Plugin loctext

A plugin's text lives in `src/plugins/<id>/loctext/<language>.txt`, one file per language, in `[aid_loctext_...]`
sections of `key = value` lines (UTF-8, with `\n` and `\\` escapes). Keys follow the game's `group__Name` form.
`gbtg-inspector` turns them into the game's loctext format ([INSPECTOR.md](INSPECTOR.md)):

```powershell
gbtg-inspector --write-loctext src\plugins\<id>\loctext --out src\plugins\<id>\files
```

That writes `files/loctext/<language>/<hash>`. Commit the sources and the generated files. The writer makes the
layout of the game's own files without an INFO name table; `gbtg-inspector --check-loctext OriginalFiles\loctext`
rebuilds those 70 files' string and key tables with it and compares them byte for byte.

### Shader warm-up (`src/plugins/shader_warmup`)

The boot shader compilation, which retail does not have, as a plugin. The retail build compiles each shader on its
first draw, as the Xbox did.

- At `graphics_ready`, in a run with a window, it shows its "Compiling Shaders" screen with its own loctext
  (`aid_loctext_plugin_shaderwarmup`, key `shaderwarmup__CompilingShaders`) in the game's language, then compiles
  the vertex programs and pixel definitions in `cache/shaders.gbtg` with the progress bar, and logs
  `shader warm-up: N vertex, M pixel permutations in T ms`.
- At `graphics_shutdown` it adds this run's compiled permutations to `cache/shaders.gbtg`.
- `[ShaderWarmup] Manifest` in its `plugin.ini` moves the file. The file format (`shader_manifest.cpp`) belongs to the
  plugin; bump its translator version when shader generation or pixel-key normalization changes.
- `--render-reference` and `--cpu-vertex` runs skip the warm-up, as before.
- A damaged manifest is logged and skipped; at exit it is replaced.

### Debug UI (`src/plugins/debug_ui`)

The Dear ImGui diagnostics, which retail does not have, as an overlay. **F8** opens the menu bar (Diagnostics:
Runtime, Performance, Native objects, Resource cache, Timing overlay; and Scenes, which requests any scene in the scene
order) and gives it the keyboard and mouse; **F12** toggles the timing overlay. Opened windows stay on screen, without
input, after F8 closes the menu. The plugin compiles ImGui itself with its platform half (Win32, or SDL3 elsewhere)
and a renderer for each backend gbtg has (`renderer_d3d11.cpp`, `renderer_vulkan.cpp`); gbtg has no ImGui.

```ini
[DebugUi]
OpenMenu = 0        ; 1 opens the menu bar at startup (formerly --debug-ui)
TimingOverlay = 1   ; optional: show or hide the timing overlay at startup; default shown in Debug, hidden in Release
```

Off Windows, SDL3 is linked into gbtg statically, so gbtg exports its symbols (`ENABLE_EXPORTS`) and the plugin's
SDL3 calls resolve to gbtg's copy when it loads.

### Scene log example (`examples/plugins/scene_log`)

A feature library in C, as a standalone project: it logs each scene script the game loads, with the prefix from
`[SceneLog] Prefix`. It ships with `Enabled = 0`; set `Enabled = 1` in `plugins/scene_log/plugin.ini` to try it.

## Compiled-in features

A feature can also be compiled into `gbtg` itself and switched on by a plugin's `Features` list, with its settings in
the section named after its id. Add the source to the `GBTG_RETAIL_GAME_ONLY` `else()` block and register it:

```cpp
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugin_features.h"

namespace gbtg
{
namespace
{
void my_boot(Game& game, const PluginSettings& settings) { ... } // Once, after Game::init.
void my_frame(Game& game) { ... }                                // Each frame, after frame_tick.
}

GBTG_PLUGIN_FEATURE(my_feature, "my_feature", "one line for the log", my_boot, my_frame);
}
#endif
```

Either hook may be `nullptr`. Settings keys are lower case. A feature listed by several plugins runs once, with the
last plugin's settings. Unlike a library, a compiled-in feature sees the whole `Game`, and it has no graphics hooks.

Features of either kind are host code and fall under the native-first rule in `AGENTS.md`: they may observe and
adjust game state and add presentation, but must not re-implement retail behavior the native path already provides.

## Where the disc overlay applies

The disc overlay sits on the file boundary, so translated code is unchanged:

| Boundary | Covers |
|----------|--------|
| `xbox_host_path` (`src/engine/saves/save_root.cpp`) | Native `CreateFile`, attributes and movie opens on disc paths |
| `AidStore::bundle_path`, `demand_path` (`src/engine/runtime/aid.cpp`) | Bundles, demand files, and `aid/` bundle entry replacement |
| `SceneDialogueResourceOwner` loctext and font loads | Text and fonts |
| Sound bank file reads (`pcm_out.cpp`) | Sound bank files named by the game |
| The loading screen's font | `font/069563d4` |

Not covered yet: `index_wavebanks` lists `xwavebank/` from the game folder only, so a plugin can't add a new wave
bank, and `default.xbe` reads always use the game folder.

Run with `--verbose` to see each plugin file the game opens (`plugin file ...`).
