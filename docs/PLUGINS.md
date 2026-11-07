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
- **Stories**: entries in game select's Features menu that play the plugin's scenes (see [Stories](#stories)).
- **Scenes of its own**, as recipes that gbtg assembles from parts of the player's disc (see
  [Scene recipes](#scene-recipes)); the inspector's scene builder makes them ([INSPECTOR.md](INSPECTOR.md)).
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
    scenes/my_scene.scene          a scene recipe (see Scene recipes)
```

- **`plugin.ini`** is required. A folder without one is skipped with a warning. A UTF-8 byte order mark is fine.
- **`scenes/`** holds scene recipes: each makes the bundle its `[Bundle] Path` names, from the disc, when the game
  opens it. A bundle file the same plugin ships at that path wins over its recipe.
- **Disc paths.** Any file in a subfolder other than `aid/` and `scenes/` replaces or adds the disc file at the same path under the
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
| `add_story` | A Features menu entry (`GbtgStory`), as a `[Story]` section adds one (see [Stories](#stories)). |
| `object_view`, `object_fields`, `type_name`, `type_schema`, `schema_count`, `schema_at`, `actors` | Read-only views of the named retail structs: a native object's type, retail name and struct, its fields (`GbtgField`, in retail order, nested structs as groups, unnamed bytes as `GBTG_FIELD_BYTES` rows), the struct list, and the scene's actors (`GbtgActorInfo`). Added after `add_story`; test `size` first. |

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

Without `SOURCES` the plugin is a data plugin: no library, only its `plugin.ini` and files, as the `dlc_story`
example is. `EXTRA_DIRS scenes` copies folders as they are, such as its recipes.

The plugin is laid out as gbtg reads it, in `<output>/<id>/`: the library (no `lib` prefix, as `plugin.ini` names
it), `plugin.ini`, the contents of `FILES_DIR` and each of `EXTRA_DIRS`. `<output>` is `OUTPUT_DIR` if given, else
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
Runtime, Performance, Native objects, Actors, Resource cache, Timing overlay; and Scenes, which requests any scene in
the scene order) and gives it the keyboard and mouse; **F12** toggles the timing overlay. Opened windows stay on screen, without
input, after F8 closes the menu. The plugin compiles ImGui itself with its platform half (Win32, or SDL3 elsewhere)
and a renderer for each backend gbtg has (`renderer_d3d11.cpp`, `renderer_vulkan.cpp`); gbtg has no ImGui.

Native objects lists each factory object with its type's retail name (from the factory table, whose row T-1 names
type T) and the named struct that lays it out. Actors lists the native actors with their class, marker ids, position,
euler, age and model. Clicking a row in either opens an inspector for that object: its fields from the struct in
retail order, with offsets, values, types and the declaration's evidence comment as a tooltip; nested structs and long
arrays expand, unnamed ranges show as bytes, and **View as** shows the bytes as any other struct. A type without a
struct of its own is shown as its nearest parent's (the actor, strategy, body-state or scene-control base).

The struct list is `src/engine/plugins/object_schemas_generated.inc`, generated from every struct declared with
`static_assert(sizeof(...))` beside it (docs/RETAIL_LAYOUTS.md). The factory type of each struct is listed by hand in
`object_schemas.cpp`. After naming or renaming fields, run `python tools/layout/object_schemas.py`; a stale list fails
to compile.

```ini
[DebugUi]
OpenMenu = 0        ; 1 opens the menu bar at startup (formerly --debug-ui)
Windows = objects, actors ; optional: windows to open at startup (runtime, performance, objects, actors, resources, scenes)
TimingOverlay = 1   ; optional: show or hide the timing overlay at startup; default shown in Debug, hidden in Release
```

Off Windows, SDL3 is linked into gbtg statically, so gbtg exports its symbols (`ENABLE_EXPORTS`) and the plugin's
SDL3 calls resolve to gbtg's copy when it loads.

### Rare Replay (`src/plugins/rare_replay`)

The game's logic ticks at 60 Hz, as the Rare Replay release does, instead of retail's 30 Hz. Retail presents with
`D3DPRESENT_INTERVAL_TWO` (0x000FC310 stores 2, 0x001548B0 passes it to the device), so a tick is two 60 Hz fields and
0x000FC030 makes the step 1/30 s. This data plugin switches on the compiled-in `rare_replay` feature
(`runtime/rare_replay.cpp`): the platform loop ticks every field, so the step is 1/60 s. Rare Replay's own code is not
available, so this follows the retail code.

Most of the game scales by the step (`DAT_005096C0`: 43 files, about 130 reads), and those parts need nothing:
movement, gravity and hit pushes, animation, cutscenes, the script's timer conditions (0x00117100), the body timers
(0x0001F600), the pad's repeat timers (0x00154BA0), the camera, effects and spray particles, the cloth solver, the
spawn queue's intervals, ambient steps and scare timers, transition durations. The code that counts ticks instead asks
`rare_replay_retail_tick()`, true on each tick that completes one of retail's 1/30 s ticks (every second tick at
60 Hz, every tick without the plugin), so its counts keep their retail meaning:

| Site | Counts | With Rare Replay |
|------|--------|------------------|
| `entities/body_apply.cpp` (body +0x880) | A body's frame countdown that clears +0x87E | Counts retail ticks |
| `scene/scene_controls.cpp`, walkabout control +0x34 (0x00080600) | The ticks until the next walkabout track | Counts retail ticks |
| `transitions/book_map.cpp`, `map_countdown` | 5 ticks before the book map sets `DAT_0050EA9C` 0x2000 | Counts retail ticks |
| `transitions/scene_transitions.cpp`, `flag_delay` | 15 ticks before a transition sets `DAT_0050EA9C` 0x2000 | Counts retail ticks |
| `scene/background_morphs.cpp` (0x001137E0) | A fixed pull of 0.008 a tick over 100 steps | Steps on retail ticks |
| `frontend/scoreboard_update.cpp`, `damage_6e0` and `meter_bc8` | The HUD's damage flash and meter countdowns | Count retail ticks |
| `entities/bg_break_key.cpp` | The broken piece's spin, added to its yaw each tick and decayed by 0.992 | Half a turn and the square root of the decay a 60 Hz tick |
| `graphics/effect_notifications.cpp` (0x000EE560) | Effect sounds at most every 5 frames (`DAT_004DEA58`) | 5 retail ticks |
| `scene/lighting.cpp` via `animate_light_records` (0x000FDDA0) | Light flicker and pulses on frame-counter masks, a random roll each | Animate on retail ticks, with the retail frame count |
| `runtime/clock.cpp`, `update_holds` (0x000F9F60) | The frames each pad button is held | Counts retail ticks |
| `scene/scene_gameplay_time.cpp` (0x0012B1F0), timers +0x10 and +0xC | The frame count beside the gameplay time (its seconds scale by the step) | Counts retail ticks |
| `graphics/point_graph_solver.cpp`, graph +0x1C8 | 10 frames of cloth warm-up | Counts retail ticks |
| `transitions/scene_transitions.cpp`, `count_down` | A transition's time counts down by the step; the fade (0x0012BC80) and slime handlers wait for it to be below zero and not exactly zero | A step that lands exactly on zero goes just below it: at 1/60 s the attract's first fade landed on 0.0 and never ended |

Counted but left alone:

- **Reference, pause and hold counts** (the body's +0x309..+0x30C, +0x258, +0x338, strategy +0x1C, `pause_count`,
  `override_count`, players' `paused`, door and ghoul intro `camera_hold`): they count requests, not time.
- **Input stamps**: `refresh_runtime_input_device` (0x00109470) stores the frame number to tell one tick's input from
  the next. Every 60 Hz tick has its own number, so it works as before.
- **The frame counter itself** (`DAT_004DEA58`, `clock.frame`): it still counts every tick. Its time-keeping readers
  are above; the sound bank choice (`pcm_out.cpp`) only uses it as a varying number.
- **Actor tick counts** (`actor_frame_update.cpp` +0x928's `ticks`, `player_update.cpp` +0x928, `weapon_actor.cpp`):
  counted but nothing translated reads them yet. A reader that treats them as time needs the gate.
- **One-tick waits** (the transition's `loading_frames`, `g_50484c`): they hold a step for one tick, so they last
  1/60 s instead of 1/30 s. The scenes start a fraction of a second sooner.
- **The retail `clock.delayed` callback slots** (`DAT_004DEA64`, counted in ticks): nothing schedules one.

Random rolls happen at events (spawns, hits, breaks, choices), not as chances each tick, apart from the light flicker
above; a roll found later inside a per-tick update needs the gate too. Each tick waits for its 60 Hz field, so VSync
at 60 Hz or more shows every tick.

The attract reel at 30 Hz and 60 Hz shows the same shots at the same game times; its second script starts at 116.03 s
at 30 Hz and 115.71 s at 60 Hz, as each of its waits ends on a tick (up to 1/30 s late at 30 Hz, 1/60 s at 60 Hz).

### DLC story example (`examples/plugins/dlc_story`)

A data plugin with no library. Its `[Story]` adds "DLC Story" (its own loctext, `aid_loctext_plugin_dlcstory`, in
five languages) to the bottom of game select's Features menu, and plays its scene, `scenes/impambush.scene`: the
central hallway's imp fight from chapter 1 scene 1 without its tutorials, the imps placed closer in, two more imps on
the plugin's own objparams (`aid_objparams_dlcstory_imp`, a copy of the imp's), and a skeleton from chapter 1 scene 3.
The recipe was written with the scene builder's formats and resolved with `gbtg-inspector --recipe ... --resolve`;
it holds references and edits only. The menu entry and the scene are checked by `gbtg-transition-tests
--gameselect-feature` with `GBTG_PLUGINS_DIR`, `GBTG_FEATURE="DLC Story"` and `GBTG_FEATURE_PLAY=1500`, which plays
the scene up to its door event (the event then waits for A).

### Xbox One buttons example (`examples/plugins/xbox_one_buttons`)

A data plugin with no library. The host maps the Xbox controller's White button to LB and Black to RB, so its
`aid/` textures replace the White and Black button pictures (`aid_texture_joypad_buttons_white` and `_black` in
`common.bnl`, 64x64 DXT3) with the game's Black button labelled "LB" and "RB". `make_buttons.py --game-dir <game
folder>` reads the Black button from `common.bnl`, draws the label on its dome and writes both as loose textures
with the disc entry's texture header (Pillow and numpy). The other button pictures are unchanged. It ships with
`Enabled = 0`; set `Enabled = 1` in `plugins/xbox_one_buttons/plugin.ini` to try it.

### Scene log example (`examples/plugins/scene_log`)

A feature library in C, as a standalone project: it logs each scene script the game loads, with the prefix from
`[SceneLog] Prefix`. It ships with `Enabled = 0`; set `Enabled = 1` in `plugins/scene_log/plugin.ini` to try it.

## Stories

A story is an entry in game select's Features menu (`gameselect_features.cpp`, 0x000D85A0) that plays a list of
scenes and returns to game select, the way the menu's Credits play (0x000D8420): the held scene order becomes game
select, the story's scenes and game select again, and the first scene starts. A plugin adds one with a `[Story]`
section (`[Story2]` and so on for more), or a library with `add_story`:

```ini
[Story]
Name = DLC Story                         ; the label when the loctext is missing (UTF-8)
Loctext = aid_loctext_plugin_dlcstory    ; the label: Key in this loctext, in the game's language
Key = dlcstory__MenuItem
Scenes = aid_script_dlcstory_impambush   ; comma separated, played in order
```

Stories are listed below the retail features. A scene is any `aid_script_` name the game can load: a disc scene, a
bundle the plugin ships, or a recipe's.

## Scene recipes

A recipe (`scenes/*.scene` in a plugin) is a plugin's scene, or any bundle, as text. gbtg builds the bundle from the
player's own disc when the game opens it (`AidStore::load_bundle_rel`), so a recipe holds references and edits, never
retail bytes, and a plugin with recipes can be shared. The inspector's scene builder makes and edits them
([INSPECTOR.md](INSPECTOR.md)); a recipe is also plain text to write by hand.

```ini
[Bundle]
Path = aid_script/dlcstory_impambush.bnl                    ; the bundle it makes, under bundles/
Transition = aid_script_ghoulies_chapter1_scene1_2playcam   ; whose transition and book clip it uses
Fallback = aid_script/ghoulies_chapter1_scene1_2playcam.bnl ; bundles opened with it, comma separated

[Parts]                                                     ; resources copied as they are: <bundle> <aid>
aid_script/ghoulies_chapter1_scene1_2playcam.bnl aid_model_ghoulies_background_centralhallway_all

[Script aid_script_dlcstory_impambush]                      ; the scene's script, node by node
copy aid_script/ghoulies_chapter1_scene1_2playcam.bnl aid_script_ghoulies_chapter1_scene1_2playcam 0
new 0x3d 1e000000                                           ; a new node: its type, then the bytes after its head

[Markers aid_marker_dlcstory_impambush]                     ; the scene's markers, row by row
copy aid_script/ghoulies_chapter1_scene1_2playcam.bnl aid_marker_ghoulies_chapter1_scene1_2playcam 2
  id 30                                                     ; edits apply to the line above
  position -15 -2 175
  rotation 0 1.57 0
  set 0x28 str aid_objparams_dlcstory_imp

[Resource aid_objparams_dlcstory_imp]                       ; any other resource: one copy or one new
copy default.xbe aid_objparams_ghoulies_actor_imp
  set 0x10 u32 5
```

- **Items.** `copy <bundle> <aid> <n>` copies script node `n` (counting from 0) or the marker row with id `n`;
  `new <type> [<hex>]` makes one. A `[Resource]` is one `copy <bundle> <aid>` or one `new <type> <hex>`. The script
  ends with the `{8, 0}` terminator and the markers with their kind-0 row; the recipe adds both.
- **Edits.** `set <offset> <kind> <value>` writes over the item, from its first byte (its size word). Kinds: `u8`,
  `u16`, `u32`, `i32`, `f32`, `hex` and `str` (the text and a zero; the item keeps its size). `id`, `position` and
  `rotation` write a marker row's id (+6), +0xC and +0x18. A `;` starts a comment, also inside a `str` value.
- **Names.** The scene is its `[Script]` resource's name; its markers are `aid_marker_` and the rest of that name, as
  0x0011CB80 names them. The executable keys some tables by scene name, so a plugin's scene takes their defaults: no
  chapter (no chapter setup script unless named `aid_script_ghoulies_chapter<N>_...`), the default player, and no row
  in the scene directory (0x00116220), which `Transition` fills with a disc scene's row. Without it the directory's
  first row is used, as retail does for an unknown name.
- **Objparams** live in the executable, not in bundles (pools 3 and 5). The source `default.xbe` stands for them, and
  a `[Resource]` named `aid_objparams_...` joins the executable's last objparams pool instead of the bundle, where the
  game finds it by name like its own. A retail name still resolves to the retail record, so give a copy a new name and
  point a marker or script string at it with a `str` edit.
- **Fallback.** Gameplay asks for some resources later than the load (an attack's animation, an event's cutscene).
  The bundles `Fallback` names open with the scene, as the player bundle opens with a retail scene (0x00124420), and
  the game finds those resources there. Without them, every resource the scene ever uses must be a part.
- **Building.** The bundle is laid out as retail bundles are: the file table sorted by name and each chunk packed in
  table order (`BundleBuilder`). `gbtg-inspector --check-bundle-builder OriginalFiles\bundles` rebuilds every disc
  bundle with it and compares the bytes (161 of 161 match). Sources are disc bundles or bundle files plugins ship,
  never other recipes, so a recipe may remake the disc bundle of its own name.
- **Built bundles.** The scene builder can export the built `.bnl` into the plugin's `bundles/`, which then wins over
  the recipe. It holds disc resources: keep it local, never commit or share it.

`gbtg --scene <aid_script_...>` starts any scene by its full name, a plugin's included, in builds with plugins.

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
| `AidStore::load_bundle_rel`, `bundle_exists` | Scene recipes and their fallbacks |
| The executable's objparams pools (`executable_resources.cpp`) | Recipes' `aid_objparams_` resources |
| The scene directory lookup (0x00116220) | Recipes' `Transition` |
| The Features menu (0x000D85A0) | Stories |
| The platform loop's fields a tick, and the tick-counted sites in "Rare Replay" | Rare Replay |

Not covered yet: `index_wavebanks` lists `xwavebank/` from the game folder only, so a plugin can't add a new wave
bank, and `default.xbe` reads always use the game folder.

Run with `--verbose` to see each plugin file the game opens (`plugin file ...`).
