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

   Record which native path replaces each, and what is still missing before it
   can, in `docs/LEGACY_HOST_CODE.md`; this milestone is step 1 of that plan.
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

## Progress (2026-10-13, handoff; milestone NOT complete)

Work stopped at a session limit. The tree builds (Release `gbtg`), has no
temporary traces, and both headless 900-frame runs (with and without
`--scene chapter1_scene1_2playcam`) exit 0. Nothing is committed. Debug,
the transition suites and ctest have not been re-run since these changes.

Other uncommitted changes in the tree (`queue/` deletions, `AGENTS.md`,
`.gitignore`, `docs/LEGACY_HOST_CODE.md`, `tools/decomp/supervisor.*`, ...) are
not from this milestone. Leave them alone.

### Step 1: inventory (one tutorial frame, view 0)

Draw order: the cutscene (0x58150 → `draw_runtime_cutscene`, passes 1 and 2
per actor), then the head and text quads, then the widget's button bat
(pass 3). Each frame records about 461 indexed draws:

| Model | Draws | Primitive | VS handles | PS handles | Textures |
|---|---|---|---|---|---|
| `dialogs_backdrop1` (pass 2 only) | 81 | fan (7), 4 indices each | 1 | 136 | 1 |
| `actor_dialogbat` ×4 actors (pass 1 ≈92 draws, pass 2 1 draw) | ~372 | strips (6) and fans | 2–11 | 137–151 | 2 stages |
| `actor_buttonbat` (pass 3, 0x6D6C0 → 0xF9250) | 8 | strips | 12–14 | 131–135 | 2 stages |

- **Streams:** each stream is `stream_descriptors[n] + 4`. That is a resource
  the relocation registered (`SceneD3DResources::resources[res].data`); its
  kind stays 0 because only `register_resource` runs. Typical declaration:
  v0 FLOAT3 position, v1 FLOAT3 normal, v2 D3DCOLOR, v3 FLOAT2 uv,
  v4 FLOAT4 matrix indices, v5 FLOAT4 weights. The blendshape variants also
  read v6..v13 (streams 6..13).
- **Vertex programs:** spliced NV2A microcode (header 0x2078). They use
  `arl`/`c[a0.x+n]` palette skinning with the matrix-array constants;
  projection rows are at c3..c6, light terms at c92..c95, fog at c7/c9. Every
  program ends with the XDK screen-space fixup
  `mul oPos.xyz, r12, c[-38]` / `rcc r1.x, r12.w` /
  `mad oPos.xyz, r12, r1.x, c[-37]`.
  `tools/nv2a_vsh_disasm.py` disassembles `TRACE prog <handle>: <words>`
  lines (dump `SceneD3DDevice::vertex_shaders[h].program` to get them).
- **Render-state keys** are NV2A push headers (0x40000 | method):
  - 0x40300 alpha test, 0x40304 blend, 0x4033C alpha func, 0x40340 alpha ref
  - 0x40344 / 0x40348 blend factors, 0x40350 equation
  - 0x40354 depth func, 0x40358 colour mask, 0x4035C depth write
  - Typical values: cull 0x900, depth LEQUAL, blend SRCALPHA/INVSRCALPHA
    (enabled on some bat draws).
  - The viewport is (0, 22, 640, 436, 0, 1).

### Step 2: what is implemented

- **Shader choice: translate the bound program.** `xbox_vertex_program.{h,cpp}`
  runs the bound NV2A microcode per vertex (MAC/ILU ops, swizzles, negation,
  a0 relative addressing, paired-ILU→R1, R12 = oPos, constant writes).
  - Why: the programs carry skinning, blendshape targets and lighting. Mapping
    their registers onto the host shader's matrices would mean
    re-deriving each splice by hand.
  - Skinning: `dialogue_skin`'s CPU skinning is not needed. The program
    consumes the matrix-array constants that the native nodes already upload.
- **Viewport constants.** `SceneD3DDevice`'s `viewport` boundary now writes
  c-38/c-37, as D3D SetViewport does (W/2, -H/2, (2^24-1)·(maxZ-minZ);
  X+W/2, Y+H/2, (2^24-1)·minZ). The presenter divides those back out to
  640x480 clip space.
- **`SceneD3DDevice`** keeps one ordered `commands` list: `SceneD3DIndexedDraw`,
  `SceneD3DQuad`, `SceneD3DClear` and `SceneD3DTarget`.
  - Each draw snapshots the constants, stage textures, render states, streams,
    pixel constants (with a set mask) and stage ADDRESSU/V (from
    `deferred_states`; 0 means the D3D default, wrap).
  - The `clear`, `render_target` and `pixel_constants` boundaries now record
    instead of discarding.
  - The cull and ZEnable keys are unified (`kCullMode`, `kZEnable`).
- **`scene_d3d_present.{h,cpp}`** presents the command list in order:
  - It fetches vertices from the declaration (all D3DVSDT types), runs the
    program, and expands list/strip/fan/quad-list primitives into triangles.
  - It maps states to `GfxNativeDraw`.
  - The pixel shader is the 240-byte definition at
    `d3d.pixel_shaders[handle]`. SetPixelShaderConstant values are folded in
    through PSC0Mapping/PSC1Mapping/PSFinalCombinerConstants.
  - Quads use the c0x60 rows as a clip transform, colour c0x64, and texture ×
    diffuse.
  - A non-back-buffer render target raises; the page-flag path is not bound.
- **`gfx_draw_native` / `gfx_clear_native`** (`gfx.h`, `gfx_d3d11.cpp`, null
  stubs):
  - A pass-through VS feeds the existing `ps_model` combiner interpreter (or
    a texture×diffuse PS when there is no definition).
  - Blend, depth, rasterizer (with viewport scissor) and sampler states are
    cached by key. Textures upload through the sprite cache.
  - `ps_model` gained general alpha functions (`native_alpha_pass`); the host
    behaviour for func 516 is unchanged.
- **Textures:** `SceneActorServices::State::texture_image` (replaces
  `quad_image`) first resolves relocated model records. It walks back to the
  28-byte header, reads the data from the record's registered address, and
  decodes with `decode_xbox_texture`. It falls back to the registry rows for
  fonts and texture resources. `SceneResourceMemory` gained `block_start` and
  `find_address`.
- **`present_views`** now does: the host world draw, clear the device commands,
  `draw_scene_dialogue`, then `present_scene_d3d_commands`.
  `present_dialogue_quads` and `take_quads` are gone. Quads no longer go
  through `gfx_sprite_submit`.

### Screenshot state

Frame 300 shows the backdrop (dark page with cobweb corners), the butler head,
the text and the A icon, all from the native path. A temporary side-by-side
against the host `DialogueScreen` drawn in the same scene matched the backdrop
exactly. The host also shows no dialog bats at frame 300 (they are dark
against the dark page there).

**Gap 1: the button bat renders black (invisible).** Its geometry is
correct: about 48 px wide, directly above the A icon. Its combiner is
`c7 × T0 × T1`, and c6/c7 (base/ambient from 0x157090 → `state.base_color` /
`ambient_color`) are zero.

- **Cause found:** DAT_00509B5C is modelled twice.
  - `RuntimeDialogueDrawContext::override_count` binds to
    `State::light_override`.
  - `SceneAmbientContext::override_count` is a separate value, initialised to
    0 in `scene_actor_services.cpp`, around `ambient_context = ...`.
  - So the dialogue's `++override_count` never makes
    `configure_runtime_scene_environment` (0xFD2A0) pick
    `override_base_color` (DAT_0042A570), or makes the ambient sampler pick
    `override_color` (DAT_0042A560).
- **Fix:** make `SceneAmbientContext::override_count` a `const u32&`, bound to
  `light_override`. Update the two aggregate inits in
  `tests/scene_render_tests.cpp` (lines ~139 and ~237) to pass a local
  `u32`. Then re-check that the button bat and the dialog bats are lit.

### Remaining steps

1. Fix gap 1, then take screenshots at frames 120, 300 and 600.
   - Check the bats (they animate and may be off-page at some frames).
   - Check the text drop shadow. It is not visible yet; check whether
     the shadow pass (`draw_runtime_dialogue_text` with `shadow`) records
     quads, and whether their colour or alpha is lost in `present_quad`.
2. Check the depth interaction with the host world draw, which runs first
   with a different projection. Turning depth off changed the cobweb
   coverage slightly. Retail clears only when a page flag is set, so compare
   the host world draw's depth. Turning culling off also changed the
   cobwebs; confirm the 0x900 → `D3D11_CULL_FRONT` mapping against xemu.
3. Do step 4 (switch over). Verify with a temporary trace that `DialogueScreen`
   (`frontend_captions.cpp`, through `draw_ui` → `draw_move_captions`),
   `parse_cutscene` and `sample_cutscene_camera` never run while
   `actor_start == 1`, then remove the trace. List the remaining users
   (`frontend_captions.cpp`, `cutscene_runtime.cpp` `draw_cutscene_props`,
   `transition_book.cpp`) and record which native path replaces each.
4. Run the full acceptance list: Release and Debug, the 6 transition suites,
   ctest, both 900-frame runs, and `git diff --check`.
5. Not handled yet:
   - Fog (oFog is computed but ignored; the dialogue disables fog anyway).
   - Projective texture coordinates.
   - The page render-target path (raises).
   - Colour clears of a rectangle (raise).
   - XDK pixel-centre bias in c-37 (omitted; the presenter's inversion uses
     the same values).
- Reference shots: the default attract run and `chapter1_scene1_1book` show no
  dialogue prompt headlessly. The quickest host reference was to call
  `DialogueScreen::open/tick/draw(false, true, 1.5f)` temporarily in the
  `present_views` lambda.

## Results (2026-10-14)

The native dialogue drawing and the opening-scene switch-over are complete.
The ordered `SceneD3DDevice` / `present_scene_d3d_commands` / `gfx_draw_native`
design is retained. No host dialogue logic or second renderer was introduced.
There is one qualification to the literal screenshot acceptance wording:
the normal frame-120 screenshot is covered by the existing slime transition.
The native prompt is already fully drawn beneath it; the transition was not
changed to make the screenshot pass.

### Lighting and text

- `SceneAmbientContext::override_count` is now a `const u32&` bound to
  `State::light_override`, the same DAT_00509B5C storage used by the dialogue
  context. The existing ambient/environment tests now mutate the referenced
  local variable and verify both override colours. The four pink dialog bats
  and the orange button bat are lit in the native screenshots.
- The shadow already records and presents correctly. A temporary probe at
  frame 300 counted 95 black shadow quads, with alpha 0.501961 (128/255).
  `prepare_quad` uploads the colour to c0x64, `decode_push` snapshots it, and
  `present_quad` copies all four channels to vertex diffuse. The recursive
  shadow-first glyph draw matches `cache/ghidra_frontier/0005E9F0.txt`.
  Inspection of the full-resolution screenshots confirms the black offset
  shadow. No shadow implementation change was needed; the probe was removed.

### Culling and depth evidence

- Exported `cache/ghidra_frontier/00171420.txt` read-only. Retail CullMode
  disables culling for 0; otherwise it emits NV097_SET_CULL_FACE as
  `0x404 + (CullMode != D3DRS_FrontFace)`. A paused xemu run of the existing
  `GbtG_render_oracle.iso` measured D3DRS_FrontFace at 0x00181DB8 as 0x900.
  NV_PGRAPH_SETUPRASTER at 0xFD401990 was 0x00200000: front-face bit clear,
  cull selector FRONT. Thus 0x900 selects FRONT, and 0x901 selects BACK.
  The D3D11 mapping is retained and its comment now records this evidence.
- [xemu's draw boundary](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/draw.c)
  reverses GL winding for inverted clip Y. The presenter retains Xbox screen
  orientation, so D3D11's default clockwise front face is equivalent.
- A bounded xemu NV2A method trace confirmed depth-test enable, LEQUAL
  (0x203), depth-mask changes between 0 and 1, clear 0xF3, and cull-face
  0x404/0x405. This was a rendering-state oracle check, not a pixel comparison
  of the tutorial: its dialogue-active globals were zero at the paused sample.
  xemu 0.8.136's QEMU monitor reported `unknown command: 'screendump'`, so
  `tools/xemu/capture.ps1` could not produce an oracle image in this session.
- Omitting only the host world draw on frame 300 produced a pixel-identical
  screenshot: zero changed RGB pixels across 1920x1080. Host-world depth
  does not interfere with this prompt at that frame. Depth testing and
  culling remain enabled according to the recorded states. The temporary
  omission was removed. The button bat's depth/stencil rectangle clear still
  comes from retail 0x0006D6C0, before its orthographic screen-model draw.

### Switch-over and remaining users

A temporary probe covered all five `DialogueScreen` methods, `parse_cutscene`
and `sample_cutscene_camera`, checking the live game's
`factory_scene->actor_start == 1` on every call. The opening-scene 900-frame
run recorded **zero calls total**; the default attract run recorded seven
calls, confirming that the probe was active. Both exited 0, with no forbidden
call. All probe code was removed. No additional caller switch was required:
the native scene already activates and draws its own dialogue resources.

The remaining host users stay unchanged:

- `frontend_captions.cpp`: `g_dialogue_screen` serves host captions outside
  this native opening scene. Its replacement here is `dialogue_activation.cpp`
  (load/tick of the detached `RuntimeCutsceneInstance`), `draw_scene_dialogue`
  / `draw_runtime_dialogue_outer` (0x58150), translated page/head/text/widget
  draws, and ordered D3D presentation. Retiring it globally still requires
  native front-end/caption ownership for attract and menus.
- `cutscene_runtime.cpp`: host `g.cutscenes` loading, camera sampling and
  `draw_cutscene_props` still support attract/legacy scenes and camera wobble.
  The replacement is `load_runtime_cutscene`, `tick_runtime_cutscene`,
  `draw_runtime_cutscene`, native camera setup/sampling and scene actor
  services. Attract sequencing and the rest of native world/actor drawing
  remain prerequisites for removing those host users. The native world's
  host callback still reaches `draw_cutscene_props`, whose host queue is
  inactive in the measured opening run.
- `transition_book.cpp`: book page loading, sampled cameras and page views
  still use the host parser/sampler and `draw_cutscene_props`. Replacement
  requires the translated transition owner, native cutscene/page views and
  render-target presentation. The book/slime transition was left intact.

This inventory is recorded here because the task explicitly requires leaving
the existing changes to `docs/LEGACY_HOST_CODE.md` alone.

### Captures and acceptance

All captures below are local, ignored `cache/` artifacts. The BMPs were
converted with PIL, and the PNGs were opened and inspected.

- `cache/native_dialogue_120.bmp` / `.png`: the normal final frame includes
  the slime transition, which obscures almost all of the prompt.
- `cache/native_dialogue_underlay_120.bmp` / `.png`: diagnostic capture with
  only that frame's final transition draw temporarily omitted. It shows the
  backdrop, four lit dialog bats, head, shadowed text, A icon and lit button
  bat. This is evidence of the underlying native draw, not a substitute for
  the normal frame-120 screenshot. The diagnostic code was removed.
- `cache/native_dialogue_300.bmp` / `.png` and
  `cache/native_dialogue_600.bmp` / `.png`: normal final frames show all the
  required elements entirely through native draws.
- `cache/host_dialogue_reference_300.bmp` / `.png`: temporary
  `DialogueScreen::open/tick/draw(false, true, 1.5f)` inside `present_views`,
  replacing the native model commands for that frame while retaining native
  quads. Host model depth occluded the quads in this diagnostic, so it is a
  model/backdrop reference only. Backdrop and bat placement agree visually;
  animation poses differ because the host reference ticks once with total
  elapsed time. The reference call and command filter were removed.
- The old vertex-stream ownership test was updated to inspect a recorded
  indexed draw's stream snapshot. It also rebinds the device afterwards to
  verify that the recorded draw retains the original bindings. The test
  target is `EXCLUDE_FROM_ALL`, so it must be built explicitly before running
  the acceptance suites; an older executable is not acceptance evidence.
- An extra Debug `--actor-loading` run exposed an undersized factory fixture:
  it reserved types through 0x15 although `NativeActorFactoryOwner::install`
  also registers type 0x1A. The fixture now reserves through 0x1A. Debug's
  checked span no longer asserts, and `--actor-loading` passes. The temporary
  assertion-report/stack diagnostic was removed; production actor logic was
  unchanged.

Final validation:

- `cmake --build build --config Release` and the Debug equivalent passed.
  After removing diagnostics, `gbtg` was rebuilt in both configurations.
- `gbtg-transition-tests` was explicitly rebuilt in Release and Debug.
  The freshly rebuilt Release executable passed all six required switches:
  `--scene-start`, `--scene-spawn`, `--dialogue-resources`, `--actor-loading`,
  `--ghoulybox-decoder`, and `--scene-models` (exit 0 each).
  The Debug `--actor-loading` run also passed.
- `ctest --test-dir build -C Release`: all six tests passed.
- Final Release `gbtg --headless --frames 900 --game-dir OriginalFiles`,
  with and without `--scene chapter1_scene1_2playcam`: exit 0 each.
- `git diff --check` passed. Temporary call/shadow probes, the host reference,
  world-draw omission and transition omission were removed. No temporary
  instrumentation remains in the source; the large raw xemu trace was deleted.
- No commit or staging was performed. The task's excluded files, including
  `docs/LEGACY_HOST_CODE.md` and retail payloads, were left alone.

### Remaining gaps outside this opening dialogue draw

Fog output, projective texture coordinates, non-back-buffer page render
targets, rectangle colour clears and the omitted XDK pixel-centre bias remain
as listed in the handoff. Voice-bank name registration is still outside this
milestone. Exact tutorial pixels against xemu were not captured; the runtime
oracle established rendering-state semantics instead. Strict final-frame
visibility at frame 120 remains obscured by the existing transition; frames
300/600 and the diagnostic frame-120 underlay show the complete native prompt.

## Windowed-launch regression corrected (2026-10-14)

A normal launch of `build/Release/gbtg.exe` showed a black attract scene once
the native actor factory started. The fast default 900-frame headless check
had not reached that native start, so it did not exercise the failing path.

The cause was an integration assumption in `SceneActorServices::present_views`:
`actor_start == 1` means that actors started, not that a native display camera
exists. The temporary live probe measured all four `g_cam_active` slots as
null in attract. Retail `present_runtime_display_views` correctly made no
view callback, but this also skipped the supplied legacy world/front-end draw.

The presentation bridge now tracks whether a native view callback ran. If
none did, it preserves the existing legacy camera state and world/front-end
callback. It does not add a camera to the native slots, change the translated
view loop, parse a new camera, or implement front-end behavior. The native
opening scene has a registered native camera and continues through its
translated draw commands; the legacy callback is only used when no native
view was presented. Retiring the attract owners still requires the native
cutscene/front-end work listed above.

The actual window was inspected before and after the correction: the black
attract scene became the rendered room/book shot, and the native opening
dialogue also rendered correctly in a real window. The initial speculative
alpha-test shader edit did not fix the problem and was reverted. All camera
probe code was removed before the final rebuild.

After that rebuild, normal windowed launch was inspected again and showed the
animated book and Press START banner. Release/Debug `gbtg` builds, all six
freshly rebuilt Release transition suites, ctest and `git diff --check` passed.
The final native frame-300 BMP is pixel-identical to the earlier accepted
native frame-300 capture. No speculative shader change remains.

## Native widget timing and input correction (2026-10-14)

The detached button bat was consuming the actor-frame context's boot-time
zero delta. Its view adapter now publishes the current dialogue update clock
to the shared actor clock before calling retail `0x0006D5C0`. The live opening
test observes the 4.5-second idle period followed by the hop state with an
advancing animation frame.

The scene input contexts were bound to private zero-filled arrays rather than
the samples written by `pad_tick`. They now share `runtime_pad_state_views()`.
The widget callback `0x00058B80` is bound to the existing translated completion
setter. A fresh A edge reaches that callback and advances the tutorial.

Subsequent pages also needed the retail text-pool presenter `0x0005F330`, called
after the queued dialogue sprites in `0x0011C240`. Its pool traversal and empty
record removal are now translated, allowing the icon-only tutorial widget to
finish its fade and accept the next page. Nonempty pooled glyph animation and
pooled text drawing remain untranslated and explicitly raise if reached; they
are not replaced with host text behavior.

The Release `--player-opening` regression now observes the bat hop, verifies
the tutorial holds without input, and uses fresh A edges to advance both pages
to the terminal page state. It invokes the existing native dialogue update for
the final close without advancing unrelated transitions. Continuing the whole
game beyond that point reaches the existing untranslated chapter-title dialog
in `transition_book.cpp`; the test no longer expects a host cutscene owner.

Release build, the six required transition suites, ctest (6/6), and both
900-frame headless runs passed. The native frame-900 capture was converted with
PIL and inspected: the lit button bat has an animated wing pose. The capture
also shows an existing text layout overflow at the right edge, which remains
a visible gap. No input or animation probe is retained.

Debug's full build and ctest (6/6) also passed. The excluded
`gbtg-transition-tests` target was explicitly rebuilt in Debug, and its fresh
`--player-opening` run passed the same bat/input regression. Final captures at
120/300/600 were converted with PIL and inspected again; frame 120 is covered
by the slime transition. An additional frame-400 capture visibly shows the
button bat raised during the hop. `git diff --check` passed. No commit or
staging was performed.

## Text spacing regression and Debug presentation cost (2026-10-14)

The overflow reported above was incorrectly called an existing gap. The saved
pre-widget-change frame-300 image shows the text fitting correctly. Adding the
text-pool draw exposed a translation error in `0x00058C30`: the space advance
used the previous draw's font scale rather than the page's scale. Assembly at
`0x00058CC4..0x00058D68` leaves the page scale in XMM1 for `0x000607F0`.
The translation now passes that explicit scale. Release and Debug frame-300
captures are pixel-identical; their text region is also pixel-identical to the
pre-change image. The live opening test now checks that drawing reaches the
last visible word of this tutorial page after repeated widget draws. A trailing
retail page marker means that requiring the text cursor to reach the final NUL
would be an incorrect assertion.

At 1920x1080, bounded 600-frame windowed runs measured 6.59 seconds in Release
with the widget clock/text-pool additions temporarily disabled, and 6.63 seconds
with them enabled and spacing corrected. Debug measured 27.81 and 28.51 seconds
respectively. These runs reproduce the Debug/Release speed difference, but do
not establish a slowdown caused by those widget additions. All diagnostic
omissions were restored before final validation.

A temporary per-frame timer measured Debug native presentation at approximately
67 ms, including 44 ms in the CPU vertex-program interpreter. For the MSVC
Visual Studio build, only `xbox_vertex_program.cpp` and `scene_d3d_present.cpp`
now compile with optimisation in Debug. Debug symbols and assertions remain;
MSVC basic runtime checks are disabled for these two files because they are
incompatible with optimisation, and stepping through them may be less direct.
Game code keeps its normal Debug build settings. The same probe then measured
approximately 20 ms for presentation and 9 ms for vertex programs. After probe
removal, the real Debug window took 15.32 seconds for 600 frames. Debug remains
slower than Release; this is an improvement, not a claim of 60 fps throughout.

The six required Release transition suites, Release/Debug ctest (6/6 each),
and freshly rebuilt Release/Debug `--player-opening` checks passed. Both
900-frame Release headless runs passed. Final Release screenshots at
120/300/600 and Debug at 300 were converted with PIL and inspected. The frame
120 slime obstruction and untranslated chapter-title dialog remain separate
gaps. Temporary timing probes and A/B omissions were removed; no commit or
staging was performed.
