# Legacy host code to replace

The project's workflow is native-first (`AGENTS.md`, "Native-first workflow").
Game behavior comes from translated retail functions running on retail state, in
the retail order from `frame_tick`. Host code supplies only the boundaries and
presents what the native draw records.

Code written before that rule re-implements retail behavior on the host: its own
parsers, sampling, timing and menu logic, drawn straight through `gfx_*`. It
often cites Xbox addresses but isn't a branch-for-branch translation. It still
runs every frame outside the native scene (attract, game select, book
transitions), and it shares the frame with the native scene. This document
lists it and the order to replace it in. **Do not extend anything listed here.**
When native code covers an item, move its callers over, delete the host version
once nothing uses it, and update this file.

Scale: roughly 10k lines across about 15 files. This is a multi-session job for a
dedicated agent; the prompt is at the end.

## How to find it

- `gfx_*` calls outside `src/engine/graphics/gfx_*`. These are game code drawing
  around the native presenter:
  `grep -rlE "\bgfx_(draw|set|sprite|clear|begin_dialog|end_dialog)" src --include=*.cpp`
- `pcm_play_cue` / `pcm_play_owned_cue` outside `pcm_out.cpp`. These are host
  cue playback by name, bypassing the native audio core.
- Uses of `g.models`, `g.camera`, `g.entities`, `g.cutscenes` and
  `parse_cutscene`, which are host-side game state.

## Inventory

| Area | Files | What it does on the host | Native replacement |
|---|---|---|---|
| Frame present | `src/engine/game.cpp` (`frame_tick`) | Outside native scenes only (the legacy front end): draws `g.models`, positions the camera from `g.camera`, calls `draw_cutscene_props`, `draw_ui` and `draw_scene_dialogue` | Native scenes present only through 0x00033A20 (`present_views`), whose per-view draw is the translated 0x0011C240: the mirror pass, the actor bins, 0x00121900, the background's 0x00042C10, model sprites and the billboard flush, debris, dialogue and text pools, with the remaining passes' gates translated (`docs/NATIVE_SCENE_PRESENT.md`). The native start releases `g.models`. Still needed: the bodies of the HUD, decal, shadow-volume, refraction, screen-effect and effect passes as their gates open, and the transition draws (step 5). The host branch goes when the front end is native (step 5) |
| Model rendering inside the renderer | `src/engine/graphics/gfx_d3d11.cpp` (3.6k lines) | Besides being the D3D11 backend, it holds its own versions of retail functions (0x0015AD20, 0x0015ABB0, 0x00158A80, 0x000F9250, 0x00058150, 0x0005E0C0/E110/E2A0, …) over host pointer casts, its own material shader and its own skinning | Native translations already exist for most of these (`model_shader_parameter_draw`, `model_tree_dispatch`, `screen_model_draw`, `dialogue_draw_state`). Reduce `gfx_d3d11` to a presenter of `SceneD3DDevice` output (vertex/index upload, texture decode, state mapping), as in `docs/NATIVE_DIALOGUE_PRESENT.md` |
| Cutscene playback | `src/gbtg/scripts/cutscene.cpp` (`parse_cutscene`), `src/engine/cutscenes/cutscene_runtime.cpp` (`sync_cutscene_props`, `draw_cutscene_props`, `release_props`) | Only the host book and map transitions use these, on their own private `Game` view (step 5). The main game's reel, its audio sync and the attract reel are deleted | Every scene cutscene, the attract included, runs on the native stack (`docs/NATIVE_CUTSCENES.md`). The rest goes with the transition owner (step 5) |
| Dialogue screen | Deleted (`DialogueScreen`, `frontend_captions.cpp`) | — | Native dialogue (`dialogue_*.cpp`) in every native scene; the attract is native |
| Button bat | `src/engine/entities/entity_model.cpp` (`button_bat_draw`) | 0x0006D6C0 over host `gfx_clear_ui_depth` and `gfx_draw_screen_mesh` | `src/engine/entities/button_bat_draw.cpp` (`draw_runtime_button_bat`, native 0x0006D6C0 → 0x000F9250) |
| Front end | `src/engine/frontend/frontend.cpp`, `frontend_contents.cpp`, `frontend_keyboard.cpp`, `frontend_slots.cpp`, `frontend_text.cpp`, `frontend_types.h` (`MenuCursor`) | Game select only: slot and contents menus, keyboard and cursor bat, with host state and `gfx_sprite_submit` draws. It runs only while no native scene runs. The attract is native (factory type 0x9B) | Game select stops natively at factory type 154 (0x9A, the game-select scene control). Translate it and its widgets on the native text pools and dialogue widgets (`docs/NATIVE_CUTSCENES.md`, "Native attract", shows the pattern) |
| Book and slime transitions | `src/engine/transitions/transition_book.cpp` (1.4k lines) | Host book render, page captures, the slime effect and direct cue playback | A translated transition owner presented natively; render targets go through the outer D3D boundary (`DialogueOuterD3DBoundaries`) |
| Entity system | `src/engine/entities/entity.cpp` (`register_entity_types`, `g.entities`), `entity_camera.cpp` (`g.camera`) | A host entity table (44 types) ticked from `frame_tick`, including the host body camera that feeds `g.camera` | The native object factory (`ObjectFactoryRegistry`, the factory owners and scene actor services) and the native camera slots (`g_cam_active`, display setup). The native scene already uses these |
| Audio | `pcm_play_owned_cue` / `pcm_stop_owned_cue` callers in `frontend_types.h` (`MenuCursor`) and the book cutscene events in `transition_book.cpp`; the clip-cue stop in `movie.cpp` | Host cue playback by name for the game-select cursor bat and the host book transition | The boot-owned native audio core (`NativeAudioRuntime`) runs every frame. The host `audio_tick`, the reel sync and the caption cues are deleted (`docs/NATIVE_AUDIO.md`). The remaining callers go with their screens in step 5 |
| Script VM fallback | `src/engine/scripts/script_vm.cpp` (`script_queue_scene_start` "legacy event") | Queues a host scene-start event when the native start didn't | Delete it once every scene starts natively |

**Format parsers that stay:** `src/gbtg/` (models, graphics, text, resources and
so on) holds format parsers that `gbtg-dump` and `gbtg-catalog` also use. They
are fine as tooling. They should not feed gameplay or rendering once the native
loaders cover a resource. The native loaders are `SceneDialogueResourceOwner`,
`SceneResourceMemory` and the relocation passes.

## Suggested order

1. **Native model presentation.** This is `docs/NATIVE_DIALOGUE_PRESENT.md`. It
   unblocks every later step that draws models.
2. **In-scene world, actors and HUD.** Translate the rest of 0x0011C240. Then
   drop `draw_world` (`g.models`, `g.camera`, `draw_cutscene_props`, `draw_ui`)
   from native scenes.
3. **Audio.** Register bank names and route frontend and transition cues
   through the native audio core. Then retire `pcm_play_cue` callers and the
   host XACT tick.
4. **Cutscenes.** Move the attract reel and book cutscenes onto
   `RuntimeCutsceneInstance`. Retire `parse_cutscene`, `g.cutscenes` and
   `DialogueScreen`.
5. **Front end and transitions.** Port game select, keyboard, captions and the
   book/slime transitions to translated owners and the presenter.
6. **Entity system and renderer.** Retire `g.entities` and the host body camera.
   Strip the retail-function copies out of `gfx_d3d11.cpp`, leaving a presenter.

Status (2026-10-15):

- **Step 1 is done** (`docs/NATIVE_DIALOGUE_PRESENT.md`, "Results").
- **Step 2 is mostly done** (`docs/NATIVE_SCENE_PRESENT.md`,
  `docs/NATIVE_GAMEPLAY_FRAME.md`). Native scenes draw nothing on the host
  except the step-5 transition. Each view runs the translated 0x0011C240 in
  retail order, with the mirror pass, model sprites and billboards live in the
  opening. Remaining: the pass bodies whose gates stay closed in the opening
  (HUD 0x00132290, decals, shadow volumes, refraction, screen effects, effect
  and debris quads), which raise when a later scene opens them.
- **Step 3 is done except for host-screen cues** (`docs/NATIVE_AUDIO.md`,
  "Boot-owned audio core and front-end cues"). The audio core is boot-owned
  and ticks every frame. The host `audio_tick` is gone, and the front-end and
  transition one-shots and streams play through the native voices. The
  remaining `pcm_*` callers are the game-select cursor bat and the host book
  transition's clip sound windows (step 5), and a movie clip's cue stop.
- **Step 4 is done** (`docs/NATIVE_CUTSCENES.md`). Scene cutscenes and the
  attract (factory type 0x9B) play on the native cutscene stack. The main
  game's host reel, the host captions and `DialogueScreen` are deleted. Only
  the host book/map transitions still use the host cutscene parser (step 5).
- **Step 5 is in progress** (`docs/NATIVE_FRONTEND.md`, "Progress"). Game
  select still runs on the host. The native game-select control (type 0x9A),
  slot select, naming and the virtual keyboard run only under the
  `--gameselect-native` test gate. Keyboard Done, saving, occupied slots, the
  menu book and the contents and play rows are next; the host front end goes
  when they are native. The transitions are not started.
- **Step 6 is open.**
- Presenter performance is tracked separately in
  `docs/RENDERING_PERFORMANCE_PLAN.md`.

Each step must meet the same bar as any change:

- Release and Debug build.
- All transition suites and ctest pass.
- Headless runs exit 0.
- Screenshots of the affected screens show the same result as before or better.
- Leave nothing half-switched: a screen runs either fully on the native path or
  fully on the host version.

## Prompt for a dedicated agent

```
Read AGENTS.md and docs/LEGACY_HOST_CODE.md, then the milestone docs it references (NATIVE_DIALOGUE_PRESENT.md, NATIVE_GAMEPLAY_FRAME.md, NATIVE_AUDIO.md, RENDERING_PERFORMANCE_PLAN.md). The project is native-first: game behavior comes only from translated retail functions; host code is boundaries plus the D3D11 presentation of recorded native draws. Everything in the LEGACY_HOST_CODE inventory predates that rule. Your job is to retire all of it, so the whole game (attract, game select, book/slime transitions, scenes) runs on the native path. You own the whole checkout for this.

Work the "Suggested order", resuming each step from its Status line. For each step:
1. Write or extend a milestone doc (docs/NATIVE_<AREA>.md, in the style of the existing ones) with goal, current state, work list and acceptance. Before changing anything, take reference screenshots of the affected screens with --headless --frames N --screenshot; record the frame numbers and which screen each shows.
2. Trace what retail runs for that screen from boot (0x00033030; frame_tick 0x00033B50 → 0x0011E4C0 → 0x0011E640 → present 0x00033480 / 0x00032AB0 / 0x00033A20 → 0x0011C240) and find the first untranslated or unbound callee the native path reaches. Work from that live entry point inward. Read decompiles from cache/ghidra_frontier and export missing ones with the read-only Ghidra command in AGENTS.md. Translate with the same branches; raise on untranslated callees, never stub.
3. Switch a screen's callers over only when the native path renders and sounds it fully. Then delete the host code nobody uses any more, including its gfx_* draw helpers, pcm_play_cue callers, g.* state and src/gbtg parsers that only fed gameplay. Keep the parsers gbtg-dump and gbtg-catalog use.
4. Verify:
   - Release and Debug build.
   - The six transition suites (--scene-start --scene-spawn --dialogue-resources --actor-loading --ghoulybox-decoder --scene-models) and ctest pass.
   - gbtg --headless --frames 900 exits 0, with and without --scene chapter1_scene1_2playcam.
   - Screenshots at the recorded frames match the references or are better. Convert them with PIL and look at them, and state any difference.
   - git diff --check is clean, and no temporary probes remain.
5. Update the milestone doc and this file: the step's status, the remaining users and the gaps.

Rules for the whole job:
- Never leave a screen half on each path.
- Don't regress what already works natively (the opening scene's dialogue, gameplay frame and audio).
- Commit nothing unless asked.
- At the end, game.cpp's frame_tick should call only native owners, gfx_d3d11.cpp should be a presenter of SceneD3DDevice output, and this inventory should be empty or list only what is blocked, with the reason.
```
