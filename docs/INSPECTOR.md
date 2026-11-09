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

### Scene editing

In plugin builds the scene view edits the loaded scene's markers through the scene builder's recipe. **Edit this
scene** (under the view toggles) remakes the loaded disc scene by reference as `aid_script_edit_<scene>` in the
plugin folder's `scenes/` (or opens that recipe when it exists) and loads it. While the loaded scene is the open
recipe's:

- **Move (G)** shows axis handles on the selected marker; drag an axis, or the centre square to move it on its
  horizontal plane. **Rotate (R)** shows a yaw ring for markers with a rotation field (+0x18). **Snap** rounds to a step
  or angle.
- **Ctrl+click** moves the selected marker to the scene surface under the cursor (the viewport's depth readback;
  without depth there, the marker's own height plane). **Ctrl+Shift+click** places a copy there. **Ctrl+D** copies
  it one unit along X; **Delete** removes its row.
- The Inspector panel's **Edit marker (recipe)** section edits the row's id, position, rotation and field edits.
- **Ctrl+Z** / **Ctrl+Y** undo and redo; each drag or run of edits is one step.
- **Apply (rebuild)** (**Ctrl+Enter**) saves the recipe, builds it and loads it again with the current camera and
  selection. The view and marker list show edits at once, but the native scene constructs them only after Apply: the
  editor never moves native objects.

`--edit-check` runs this end to end without a window: Edit this scene, move and turn the first marker with a position
and rotation, copy it, Apply, then compare the marker block the rebuilt scene loaded. It returns nonzero unless both
edits are there. Give it a scratch `--scene-plugin` folder; it writes the recipe there.

```powershell
.\build-inspector\Release\gbtg-inspector.exe --game-dir OriginalFiles --no-plugins --scene-plugin $env:TEMP\edit-check --headless --frames 30 --edit-check
```

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

## Asset viewer

**Asset viewer** (beside Sound player) browses models, animations and textures. The list shows what the scene's native
resource registry has loaded; clear **Loaded by the scene only** to list everything in the bundles the AID store has
open (common, the scene's, demand files), and **Open** any disc bundle below the list to add its resources. Opening a
bundle only lets the native provider find its AIDs; the scene is not reloaded.

- **Models** are loaded by the native resource provider (0x00035E90), instanced by 0x000F9840 and drawn alone by
  0x000F9080 in the passes an actor's model gets (kinds 0, 1 and 2, gated by the model header's byte 8 as 0x0003C9E0
  gates them) into the recording device, then presented like the scene view. Drag to orbit,
  right or middle drag to pan, and scroll to zoom. **Skeleton** draws each joint's world point under the current pose
  (0x000316B0); **Full light** draws with the ambient tint at one, as a post model's +0x28 does. The textures the
  model's draws bound are listed under it. A model whose draw reaches an untranslated function reports the error in
  the viewer and the log; the scene is unaffected.
- **Animations** play on the chosen model through the native controller (0x00030740, 0x00030880, 0x00030BA0,
  0x00030C70). **Fits the model** lists animations whose bone count (MINA +0x14) is the skeleton's joint count; others
  are refused. Play, pause, step, scrub (0x0003B2E0), speed, and **Bind pose** to clear it.
- **Textures** are decoded for presentation with the decoder the native texture cache uses: format, size, frames,
  zoom (wheel, or Fit), drag to pan, alpha on a checkerboard, the texel under the cursor, and **Save PNG** (a disc
  texture: keep it local).

Floating windows move by their title bar only, so drags inside the previews and the scene view stay with them.

The preview must leave the scene as it was: after each preview draw the clock, actor poses and script progression are
checked as for the scene view. Command-line captures:

```powershell
.\build-inspector\Release\gbtg-inspector.exe --game-dir OriginalFiles --asset <model aid> --asset-animation <anim aid> --asset-seek 0.5 --preview-screenshot model.bmp --screenshot workspace.bmp
```

## Model export and import

In plugin builds a skinned disc model goes out to FBX (or glTF 2.0) and comes back as a plugin's replacement for it.
The Asset viewer's **Export / import (FBX, glTF)** section does this for the model on show; the file's extension picks
the format. The same runs from the command line:

```powershell
# The boy as binary FBX (or .glb): skeleton, skin, materials, colour textures, and the head's MORPH_TARGET_* shapes.
.\build-inspector\Release\gbtg-inspector.exe --export-model aid_model_ghoulies_actor_boy --out boy.fbx --game-dir OriginalFiles
# An .fbx or .glb skinned to that skeleton, built on the disc model and written to <plugin>/aid/aid_model_ghoulies_actor_boy.
.\build-inspector\Release\gbtg-inspector.exe --import-model boy_new.fbx --as aid_model_ghoulies_actor_boy --plugin plugins\boy_reskin --game-dir OriginalFiles
# The serialized model's directory, node tree and textures as text.
.\build-inspector\Release\gbtg-inspector.exe --describe-model aid_model_ghoulies_actor_boy --game-dir OriginalFiles
```

**Export** reads the serialized model as the loader receives it (`serialized_model.cpp`): the `ndSkeleton` joints
(named from the tree's name table), each material's palette groups (`ndRigidSkinIdx` slot to `ndMtxArray` row to joint;
the bone stream holds `24 + 3 * slot`), the vertex streams by record kind (9 position, 0xA normal, 0xD UV, 0 bones,
8 weights, 0xB colour), and the `ndBlendShape` branch, whose targets hold offsets from its base (0x00157650 adds
`weight * target`). The body and the head are separate meshes, with materials named after the shader
(`shader_jumper`, one per material variant). Joints are unrotated, translated from their parent's bind point.

The FBX (`fbx_export.cpp`, binary version 7.4, written without Autodesk's SDK by `fbx_document.cpp`) holds the joints
as LimbNode models, a Skin deformer with one cluster per joint and a bind pose on each mesh, the head's 12
`MORPH_TARGET_*` shapes as BlendShape channels, and the colour textures as PNG files in `<name>.fbm` beside it as well
as embedded in it. It is Y up in the game's units (one unit is one centimetre to FBX tools).

**Import** keeps the disc model as a template (`model_import.cpp`). Its skeleton, materials, shaders, textures' formats
and blend-shape layout stay; every template node keeps its offset. The new vertex streams join the first GPU chunk
(the tree's vertex data relocates against it), and the index lists, skin palettes, extra palette groups and grown matrix
rows go after the CPU block with tree-relative pointers, so the native loader (0x00052910) relocates the result as it
does the disc model. An FBX is read by `fbx_import.cpp` (binary only: polygons as fans, normals, UVs and colours in any
mapping, Skin clusters, BlendShape channels, embedded or neighbouring PNG textures). Rules for the file:

- Skin every mesh to the exported skeleton; joints match by name. A package that turned the scene to its own up axis or
  units (3ds Max is Z up) is undone: the bones' bind points are fitted to the template's over every axis turn and mirror
  with a uniform scale, and the meshes follow. A joint that still sits away from the template's is reported; the game
  keeps the template skeleton.
- Keep the material names. A copy suffix after a separator (`.001`, ` #2`, `_1`, ` (2)`) is ignored, so
  `shader_jeans (1)` reads as `shader_jeans` but `shader_blinn23` never as `shader_blinn2`. The material picks the
  template's variant: a triangle with a two-bone vertex goes to the two-bone shader, else the one-bone one. Each vertex
  keeps its two strongest weights; a material with only a one-bone shader keeps the strongest joint.
- A mesh named `head...`, or with shapes, goes to the blend-shape branch. Shapes match the template's
  `MORPH_TARGET_*` names (case ignored, or a name containing one); missing ones move nothing. When none of a mesh's
  shapes has a matching name and it has as many as the template, they are taken in the template's order.
- A material's diffuse (base colour) PNG replaces its texture at the image's own size (rounded to a power of two from
  4 to 1024) as DXT1; an unchanged image keeps the disc's data, and other image formats are reported and skipped. A
  texture that another material also uses (the boy's jeans share theirs with `shader_blinn25`) stays as it is for
  that material: the changed one gets a copy of its own. Data that does not fit where the disc's was goes in a GPU
  chunk of its own; postload (0x000534A0) builds the texture's D3D header from the record's size.
- A material name the template does not have makes a **new material**: a copy of one of its textured materials (every
  variant, with copied material records and the arrays the loader rewrites) linked beside it, with its own colour
  texture. Name it `<name>__<base>` to choose the base (`overalls__shader_jumper`); without one the body's base is
  `shader_jeans` (a neutral grey cel ramp) and the head's `shader_blinn24`. The copy keeps the base's cel ramp and its
  soft lighting texture (`RARE_lightmap0`, sampled by the view, not by UV), so those still tint it. It is stored as
  `shader_<name>__<base>` with a name-table entry, so an export of the result names it with its base.
- `--export-textures <aid> --out <dir>` writes every texture of a model as PNG, named by the materials and roles
  (colour, celmap, lightmap) that use it.
- Triangles are regrouped into palettes no larger than the template's largest; joints the template's palettes did not
  use get new matrix rows.

Checked with Blender's FBX and glTF importers and exporters: an unedited export, re-export and import draws
pixel-identical to the disc model in the preview and the scene, and so does a Z-up export scaled by three. Importing from
the Asset viewer registers the file with the plugin overlay (`plugin_register_aid`) and restarts the runtime, so the
scene and the preview load it at once. The written file holds disc data: keep it local.

### 3ds Max

Use FBX (3ds Max's own importer and exporter; the Khronos glTF plug-in crashed 3ds Max on import).

1. Export the model from the inspector as `.fbx`, to a full path (a bare file name lands in the inspector's working
   folder). Its textures go in `<name>.fbm` beside it.
2. In 3ds Max, **Import** the FBX. The body and head come in skinned to the 69 bones, and the head with a Morpher
   holding the 12 `MORPH_TARGET_*` channels. Up axis and units do not matter: the import fits them back.
3. Edit or replace the meshes. Skin new geometry to the existing bones (do not rename or move them; weights must total
   1.0, and the game keeps each vertex's two strongest). Keep the existing materials' names and the Morpher channels'
   names or order; a material with a new name becomes a new material (`luigi_overalls`, or `luigi_overalls__shader_jumper`
   to pick its base) with its Base Color Map as its texture. Textures must be PNG, any power-of-two size up to 1024.
   Apply Reset XForm to mirrored objects.
4. **Export** as FBX, **Binary**, with Skin and Morph deformations on (Embed Media is optional; without it the PNGs must
   stay beside the FBX or where it records them).
5. In the inspector, **Import as this model** with that file.

Checked with 3ds Max 2021 (batch mode, FBX importer and exporter defaults): the boy comes in as two editable meshes with
Skin (49 and 5 bones) and a Morpher with the 12 named channels, with Physical materials whose Base Color Map holds the
PNGs. Materials without a map show white: in the game their colour comes from the cel ramp. Max's binary FBX export (in
inches, Z up) imports and draws as the disc model, apart from shading where Max rebuilds normals from its smoothing
groups. Every Model needs its `DefaultAttributeIndex` property: without it 3ds Max imports a mesh as an empty Dummy.

### 3ds Max tools

`tools/max/gbtg_tools.ms` (3ds Max 2021 or older) does the steps above from inside Max. Run it with **Scripting > Run
Script**, then add the **GbtG Tools** action (Customize User Interface, category `GbtG`) to a toolbar or menu. So that
scenes saved with GbtG Materials open with the material defined, also put a one-line startup script in
`%LOCALAPPDATA%\Autodesk\3dsMax\2021 - 64bit\ENU\scripts\startup`:

```maxscript
fileIn @"F:\path\to\GbtGDecomp\tools\max\gbtg_tools.ms"
```

Keep `gbtg_cel.fx` beside the script. The panel's settings are kept in `gbtg_tools.ini` in Max's plugin configuration
folder.

- **Export from game and load** runs `gbtg-inspector --export-model` for the panel's model AID into the work folder
  and loads the result. Every export also writes `<name>.gbtg.ini` beside the model file: the AID, the inspector's
  path and game folder, and each material with its part (body or head) and its colour, cel ramp and lighting PNGs, all
  of which go in `<name>.fbm`. It holds no disc data.
- **Load exported FBX** imports an export (skin and morphs on) and turns every material the manifest names into a
  **GbtG Material**: a DirectX Shader on `gbtg_cel.fx` with a GbtG rollout. Its **Base** dropdown lists the model's
  materials; **New material on this base** with a **Name** makes it a new material, and **Colour map** picks its PNG. It
  names itself as the importer reads it: the base (`shader_jumper`), or `<name>__<base>` for a new one.
- **New GbtG Material** puts a new one (on the first base) in the active Material Editor slot, to apply to faces.
- **Send to game** swaps each GbtG Material for a Standard material with that name and its colour map, exports binary
  FBX with skin and morphs (embedded media) to `<aid>_send.fbx` in the work folder, puts the materials back, and runs
  `gbtg-inspector --import-model` into the plugin folder (by default the inspector's `plugins\my_models`). The import
  notes show in the panel.

The viewport look is an approximation for authoring, not the game's combiner: the colour map times the cel ramp at the
Lambert term of a fixed light (**Light Direction**, world space) and **Ramp Brightness**, plus the lighting blob looked up
by the view-space normal (**Blob Strength**). It needs Max's DirectX 11 viewport (Nitrous). The inspector draws the real
thing.

An open inspector reloads models live: once a second it looks at the `*/aid/aid_*` files of its plugins folder, and when
one is new or changed (and unchanged at the next look, so a file still being written is left alone), it registers the
file, restarts the scene with the camera kept, and shows the model again in the Asset viewer. The Export / import
section's **Reload when a plugin model file changes** turns it off, as does `--no-live-reload`. Imports write
`<aid>.partial` and rename it over the model, so a reload never reads half a file.

Checked with 3ds Max 2021 in batch mode: loading an export turns 22 material slots of the boy into GbtG Materials with
their ramps and blobs; a new `maxshirt__shader_jumper` with a recoloured PNG saved by Max (16 bits a channel, which the
importer's PNG reader takes along with 1, 2 and 4-bit grey and palette images) is sent, imported as its own material and
texture, and drawn in the inspector; a running inspector picked up the changed file and restarted the scene. The shader
compiles as `fx_5_0` with `d3dcompiler_47`; the Max viewport itself was not looked at.

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
fails or never reaches Ready. Retail files are read only; scene edits go to a plugin
recipe (Scene editing) and texture exports to a file you name.

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
