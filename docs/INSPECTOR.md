# Runtime scene inspector

`gbtg-inspector` is a separate Windows/D3D11 application for inspecting the scene
that the reconstructed runtime actually constructs. It shares the engine code
and presentation boundary with `gbtg`; it does not assemble an alternate scene
from exported assets. ImGui is included in both Debug and Release inspector builds.

All inspector/editor implementation and shared-runtime hooks are guarded by
`GBTG_INSPECTOR_APP`. CMake builds separate `gbtg-inspector-lib` and
`gbtg-inspector-runtime` variants from the same native sources with that define;
the normal `gbtg_lib` / `gbtg-runtime` variants exclude those hooks, fields and
APIs. This keeps each executable's `Game` layout consistent across its libraries.
Use `cmake -B build -DGBTG_BUILD_INSPECTOR=OFF` to omit inspector targets entirely;
set the option back to `ON` to build them. The `vs` and `ninja-*` presets turn it
off so the game build compiles the engine once; the `vs-inspector`
(`build-inspector/`) and `ninja-inspector` presets turn it on. Game/inspector build-guard checks
verify API exclusion and inclusion respectively.

Audio-capable inspector tests set the mastering voice and isolated preview output
to 10% before native audio boot, following the repository test-volume policy.
The transport checks keep the cue itself silent and also verify restoration of
the application's scene mute after preview playback.

```powershell
cmake --preset vs-inspector
cmake --build --preset inspector
.\build-inspector\Release\gbtg-inspector.exe --game-dir OriginalFiles --scene chapter1_scene1_2playcam
```

## Plugin authoring tools

Builds configured with `-DGBTG_RETAIL_GAME_ONLY=OFF` add commands that make plugin content
(`docs/PLUGINS.md`). They run without opening a window:

```powershell
# Each <language>.txt of [aid_loctext_...] sections becomes <out>/loctext/<language>/<hash>.
.\build-inspector\Release\gbtg-inspector.exe --write-loctext src\plugins\shader_warmup\loctext --out src\plugins\shader_warmup\files
# Rebuilds the game's own loctext tables with the same writer and compares the bytes.
.\build-inspector\Release\gbtg-inspector.exe --check-loctext OriginalFiles\loctext
# Scene recipes (docs/PLUGINS.md, "Scene recipes"):
# rebuilds every disc bundle with the bundle builder and compares the bytes;
.\build-inspector\Release\gbtg-inspector.exe --check-bundle-builder OriginalFiles\bundles
# writes a recipe that remakes a disc scene by reference under a new name;
.\build-inspector\Release\gbtg-inspector.exe --recipe-from-scene aid_script_ghoulies_chapter1_scene1_2playcam --as aid_script_mine_hallway --out mine.scene
# assembles a recipe's bundle (it holds disc resources: keep it local);
.\build-inspector\Release\gbtg-inspector.exe --build-recipe mine.scene --out mine.bnl
# opens a recipe in the scene builder; --resolve builds, loads and resolves its parts, then saves it.
.\build-inspector\Release\gbtg-inspector.exe --recipe mine.scene --resolve --headless --frames 30
```

### Scene builder

In plugin builds, **Scene builder** (beside Sound player) opens the editor for a plugin's scene recipe. The inspector
loads plugins as the game does (`--plugins-dir`, `--no-plugins`), and the builder saves into the plugin folder
`--scene-plugin` names (default `plugins/my_scenes` beside the executable). Its tabs:

- **Scene**: start an empty scene, or remake a disc scene by reference to edit; open, save, and build. **Build &
  load** assembles the recipe from the disc and boots a new runtime on it (bundles and resources are read again).
  **Build, load and resolve parts** first adds every AID the recipe names, then loads the scene; each resource the
  native loader misses is added as a part from the AID index and the scene loads again, until it loads. A marker that
  fails to construct fails the load while resolving, rather than being omitted. **Fallback** writes the parts' bundles
  as fallbacks for the resources gameplay asks for later. **Export built .bnl** writes the assembled bundle into the
  plugin's `bundles/` (it holds disc resources: keep it local).
- **Parts**: the disc resources copied as they are, and the AID index (`cache/aid_index.tsv`: every disc bundle's
  resources and the executable's objparams, built on first use) to add more by name and type.
- **Markers**: the scene's marker rows, named by kind, with their id, position, rotation and field edits; duplicate,
  reorder, delete, or place one at the inspection camera. Rows from any disc scene's markers can be added.
- **Script**: the scene's nodes, named by type where known, with their field edits and bytes; reorder, duplicate,
  delete, add a new node, or copy one from any disc scene's script.
- **Resources**: other resources the scene owns, such as an objparams copy (from `default.xbe`) with edited fields.

The scene must load as the game loads it: the builder never draws or simulates a scene of its own.

The scene argument accepts the game's scene-order index or substring. The
opening gameplay scene is selected by default. The scene browser uses the same
retail scene-order list. Selecting a scene or Reload / Retry clears selections
and requests a new scene through the runtime loader. The boot-owned native
factory and services stay alive across requests; an inspector-only generation
counter identifies completed scene replacement. Cancellation releases the native
transition before requesting another scene. Inspector builds unwind completed
base constructors, release partially built players through their native destructor
before their base actor disappears, and release unfinished transition books when a load throws,
so failed loads do not leave registrations in the persistent factory.

## Navigation and inspection

- Hold the right mouse button over the scene view to fly. WASD moves, Q/E moves
  down/up, Shift increases speed. Releasing the button or losing focus releases
  mouse capture. Speed, position, yaw/pitch, FOV and clipping are adjustable.
- Reset Camera returns to the initial native scene camera. Select an object or
  marker in the list or click its viewport symbol. Frame Selection (or F over
  the viewport) moves the inspection camera to face the selected position.
- Toggle markers, object symbols, grid, axes and verified occupancy bounds.
  Depth-tested overlays use a D3D depth readback and are more expensive than
  always-visible overlays. The grid lies on world Y=0; it is not a floor probe.
- The property inspector shows native addresses, factory types/vtables, verified
  spatial fields, model resource associations when resolved, and raw bytes by
  offset. Marker addresses are explicitly marker IDs and block offsets, not
  native object tokens. Marker poses are stored record poses, before any native
  chain resolution or randomization; instantiated objects show constructed poses.
- The render panel lists recorded commands and their available shader/texture
  bindings and raw render states. A zero texture token means no scene-memory
  address was resolved, not necessarily that the stage was unbound.

## Static presentation and reconstruction failures

Initialization pumps the existing native scene state machine until construction
and activation complete. It then stops simulation and uses a separate camera/view
and geometry presentation entry point. It shares the runtime's native mirror,
visibility bins, background/actor draws, world sprites and billboard flush.
Camera setup runs before native visibility tests;
the game's active-camera pointers and display binding are restored afterward.
Dialogue drawing, frontend drawing, movie playback, retail present timers and
audio updates are excluded. Loading is muted at the host audio boundary.

Rendering still updates visibility bins, matrix/material caches and D3D state.
The inspector checks gameplay clocks, native cutscene clocks, actor poses, script condition records and
fired IDs after every draw and enters Failed if they change. This is a static
view of the constructed scene, not a general save-state or full simulation pause.

The inspector loads supported content even when individual markers cannot be
constructed, activated or drawn. Such a failure abandons the affected scene graph;
the runtime loads a fresh graph with that marker omitted. It does not continue
inside a failed constructor or supply results for missing functions. This policy
is inspector-only; normal gameplay still reports the original reconstruction error.

The status line labels incomplete scenes **PARTIAL**. Omitted marker records stay
in the list with `[not loaded]` and orange viewport symbols. Their properties show
the failure. **Errors / omissions** opens the diagnostic list. Each error, including
scene, marker kind/ID and any available Xbox address, is appended to
`inspector-errors.log` beside the executable. `--log PATH` selects another file.
Reload / Retry clears omissions and tries all content again, so newly translated
functions can be checked. Switching scenes clears the visible error list while
preserving the log file.

If no runtime camera is registered, the inspection view starts from the native
camera-start marker, or the editor origin when there is no such marker. No gameplay
camera is fabricated. Core failures (scene/resource initialization, errors without
an attributable marker, corrupted data, or changed gameplay progression) still
stop loading and are logged. Failed graphs are never presented. Native scenes
draw their backgrounds through the recording D3D boundary; the inspector
uses recorded native draws. A partial view does not establish feature completeness.

## Captures and checks

**Sound player** opens a searchable list of native sound-bank cues. Select a cue
and use **Play / Restart**, **Pause / Resume**, **Stop**, and **Volume**. The player
shows the bank, native cue index, elapsed sample time, and preparation/playback
errors. Native cue preparation and playback use the existing audio decoder and
XAudio2 boundary. The preview has its own source/cue ownership; scene voices and
simulation remain stopped. Authored looping cues continue until Stop. Changing
scenes, rebuilding after an omitted marker, or failing a scene stops the preview.
Preview failures are reported in the panel and inspector log without failing the
scene. Availability remains limited by the current native bank/decoder support.

Capture viewport saves the raw scene render before editor overlays and panels.
The path is editable. Command-line captures work offscreen:

```powershell
.\build\Release\gbtg-inspector.exe --game-dir OriginalFiles --frames 180 --screenshot inspector.bmp
.\build\Release\gbtg-inspector.exe --game-dir OriginalFiles --frames 180 --viewport-screenshot scene.bmp
```

`--width` / `--height` set the workspace size (minimum 800x600); `--vsync` sets
presentation sync. `--headless` uses an offscreen D3D11 target. `--depth-overlays`
enables overlay occlusion at startup. Automated runs return nonzero if the scene
fails or never reaches Ready. Retail files are read only; there is no asset save
or editing function.

For repeatable camera comparisons, `--camera-offset X,Y,Z` offsets the initial
native camera and `--camera-angles yaw,pitch` sets orientation in degrees. Reset
Camera still returns to the unmodified native view.

Run `gbtg-inspector-tests` for record validation and camera projection checks.
Use **Meshes / textures** in Session and rendering to inspect mesh statistics.
Drag the vertical divider between the scene browser and viewport to widen the
Scenes and Objects panels. Scene labels omit the shared `aid_script_ghoulies_`
prefix by default; **Full resource IDs** restores it, and hovering a row always
shows its full ID. The list scrolls horizontally for long names. Search and scene
loading continue to use the original resource IDs.

Selecting a recorded draw opens the Native draw tab with its submitted counts,
triangle topology count (including degenerates), stream addresses/strides, and
previews of its four bound texture stages. Actor submissions also identify their
native actor address and model resource. Unique vertex counts remain unverified.
Fixed-function submissions report raw vertex word counts explicitly.
Selection uses the lists; viewport mesh picking
is still deferred. Texture previews borrow decoded images from the active session
and never edit retail resources.

Passing a retail game directory additionally checks frozen rendering, camera
restoration, native scene reload, switching to another scene, reporting its
reconstruction frontier, and returning to the opening scene.
It also browses the first twelve scene entries and returns to the opening scene
after each, including failed loads. Changing scenes cancels the previous
inspection's scripts, pending object requests, ambient registrations, and loading
transition state before submitting a fresh native request. Unsupported scene
failures remain in the log and do not prevent selecting another scene.
