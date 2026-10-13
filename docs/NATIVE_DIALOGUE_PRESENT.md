# Milestone: native dialogue presentation

Goal: the tutorial dialogue in the native opening scene
(`aid_script_ghoulies_chapter1_scene1_2playcam`) renders entirely from the
translated retail draw path. That includes the page backdrop, the dialog bat,
the speaker head, the text, the A button and the button bat. It should look the
same as the existing host cutscene prompts. Once it does, the host stand-in
(`DialogueScreen`) is no longer used for native-scene dialogue.

Read `AGENTS.md` first; every rule there applies. Background history and
addresses are in `docs/PLAYER_SCENE.md`. The sections that matter here are "Live
opening runs: 0x6F player wait enabled" and "Native views presented: camera and
dialogue quads".

## Why there are two implementations

| | Host stand-in (Oct 2–6) | Translated native path (Oct 9 onwards) |
|---|---|---|
| Files | `src/gbtg/scripts/cutscene.cpp` (`parse_cutscene`, `sample_cutscene_camera`), `src/engine/frontend/dialogue.cpp` (`DialogueScreen`, `dialogue_skin`), `src/engine/frontend/frontend_captions.cpp` (`g_dialogue_screen`) | `src/engine/cutscenes/*`, `src/engine/frontend/dialogue_*.cpp`, `src/engine/scene/scene_actor_services.cpp` |
| Logic | Hand-written approximation of the binary (heuristic curve parsing, assumed FOV), not a dump translation | Branch-for-branch translations of the Ghidra dumps |
| Game state | Its own | The real script, cutscene instance, actor, input and audio state |
| Output | Draws straight to D3D11 (`gfx_draw_mesh`, `gfx_draw_screen_mesh`, CPU skinning) | Xbox-style draws recorded by `SceneD3DDevice` (`src/engine/graphics/scene_d3d_device.*`) |

The native path is the source of truth. The host stand-in's rendering pieces
(mesh upload, skinning, texture decode in `gfx_d3d11.cpp` and `dialogue.cpp`) are
reusable presentation code. Its logic (cutscene parsing, sampling, timing) must
not be reused for the native scene.

## Current state

- **Scene start.** The native scene starts, the script's 0x6F player wait is
  live, and the tutorial dialogue activates and updates. A 900-frame headless
  run exits 0.
- **Frame present.** `frame_tick` (`src/engine/game.cpp`) calls
  `SceneActorServices::present_views` while `factory_scene->actor_start == 1`.
  That runs the 0x33A20 per-view loop (`present_runtime_display_views`). For each
  active camera slot, the view draw does four things:
  - `gfx_set_camera` from the slot's display view (`g_display_view`; the field
    of view is in degrees there).
  - The host world draw (legacy meshes, props, UI).
  - The native dialogue draw: `draw_scene_dialogue`, which is 0x58150 plus the
    0x122230 sprite flush.
  - `present_dialogue_quads`: the recorded push-buffer quads (text, head, icons)
    go out as HUD sprites through `gfx_sprite_submit`.
- **Texture recovery.** The presenter's textures come from `quad_image`. It
  recovers the texture header from a frame record
  (header + 28 + frame × 0x14) and decodes it with `decode_xbox_texture`.
  Non-model bundle rows now record their first GPU chunk.
- **Dialogue camera.** During the tutorial, slot 0 is the dialogue camera node
  "dialog". Its page cutscene (`dialogs_dialogpage1`) is a fixed studio camera
  at (3359.6, 0, 2454.5) with focal 35. That is correct; the curves have a
  single key.
- **Not presented.**
  - Model draws recorded by `SceneD3DDevice`: `draw_indexed` (primitive, count,
    index address), `set_vertex_stream` (stream, resource address, stride),
    `create_vertex_shader` (declaration and program), `set_constant_words`
    (vertex constants), plus textures and render states. The page backdrop
    (`aid_model_ghoulies_dialogs_backdrop1`), the dialog bat
    (`aid_model_ghoulies_actor_dialogbat`) and the button bat
    (`aid_model_ghoulies_actor_buttonbat`, through 0x6D6C0 → 0xF9250 → 0xF9080)
    all arrive this way, so the background is black.
  - Sound: `g_audio_bank_names` is never populated, so voice cues don't
    resolve. This is outside this milestone unless it is cheap.

## Work

1. **Inventory the recorded model draws** for one tutorial frame. List each
   draw's source model and node, vertex streams (resource addresses, strides,
   declaration), index data, the vertex shader's constant layout (projection,
   world and camera registers; see `model_vertex_shader_draw.cpp` and
   `model_transform_draw.cpp`), textures and states.
2. **Present the model draws in D3D11** from what the native renderer
   recorded. Resolve addresses through `SceneResourceMemory`
   (`resources.memory.pointer`). Reuse the host renderer's existing pieces
   (vertex upload, texture decode and caching, blend/depth state) rather than
   writing a second renderer. Two choices to make here:
   - **Shader:** either translate the bound Xbox vertex program, or map its
     constant registers to the existing D3D11 shader's matrices. State which one
     you chose and why.
   - **Skinning:** decide whether `dialogue_skin`'s CPU skinning is still needed.
     The native matrix-array and rigid-skin nodes already upload bone matrices as
     constants (`model_matrix_array_draw.cpp`).

   This is a host boundary. The draw calls are D3D; nothing the dump tests may be
   invented.
3. **Draw order and render targets.** Match 0x58150:
   - Scene world draw first, then the dialogue cutscene, then the speaker head,
     text and widget, then the sprite flush.
   - Honour the clear (0xF3) and the render-target path when a page entry's flag
     is nonzero (`begin_runtime_dialogue_target`).
   - Depth: the button bat clears its rectangle's depth (0x6D6C0) and draws in
     the 640×480 orthographic space (0xF9250).
4. **Switch over.** In the native scene, nothing may call `DialogueScreen` or
   `sample_cutscene_camera`/`parse_cutscene`. Verify this with a breakpoint-free
   check, such as a temporary assert or trace that you remove afterwards.
   Afterwards, list the remaining host stand-in users and leave them unchanged:
   - `frontend_captions.cpp` (`g_dialogue_screen`).
   - `cutscene_runtime.cpp` (`g.cutscenes`, `draw_cutscene_props`).
   - `transition_book.cpp`.

   Record in `docs/NATIVE_DIALOGUE_PRESENT.md` which native path replaces each,
   and what is still missing before it can.
5. **Document** results and remaining gaps at the end of this file.

## Acceptance

- Headless screenshots of `chapter1_scene1_2playcam` at frames 120, 300 and 600
  (`--screenshot`) show these elements, matching a host cutscene prompt:
  - the backdrop and the dialog bat;
  - the speaker head and the text with its drop shadow;
  - the A button and the button bat.
- No host stand-in dialogue or cutscene code runs while the native scene is
  active.
- Release and Debug build.
- `build\Release\gbtg-transition-tests.exe OriginalFiles` passes with each of
  `--scene-start`, `--scene-spawn`, `--dialogue-resources`, `--actor-loading`,
  `--ghoulybox-decoder` and `--scene-models`.
- `ctest --test-dir build -C Release` passes.
- `gbtg --headless --frames 900 --game-dir OriginalFiles` exits 0, with and
  without `--scene chapter1_scene1_2playcam`.
- `git diff --check` is clean.
- No temporary traces are left in the tree.

## Constraints and tips

- **Do not commit.** Do not stage `OriginalFiles/`, `*.xbe`, `*.bnl`, `*.iso` or
  `ghidra_projects/`.
- **Translation rules.** Translate `cache/ghidra_frontier/<VA>.txt` with the
  same branches. Raise when an untranslated callee is reached; never stub it.
  Use Allman braces, `// Xbox 0x........` on definitions and declarations, and
  `.h` headers.
- **Ghidra export.** The project is read-only:
  `cmd /c "tools\ghidra\support\analyzeHeadless.bat Z:\GhouliesStuff\ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra\ghidra_scripts -postScript ExportPlayerScene.java <VA...>"`
- **Finding where something throws.** A temporary vectored exception handler in
  `src/engine/main.cpp` that logs `RtlCaptureStackBackTrace` module offsets works
  well. Pair it with a link map (in Git Bash: `LINK=-MAP cmake --build build
  --config Release --target gbtg`) and `dumpbin -disasm` from the MSVC
  toolchain. Remove both afterwards.
- **Screenshots** are 1920×1080 BMPs; convert them with PIL to view.
- **Bash heredocs** mangle backslash escapes in Python; write scripts with the
  Write tool instead.
