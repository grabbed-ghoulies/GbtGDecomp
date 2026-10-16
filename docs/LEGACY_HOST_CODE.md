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
| Frame present | Deleted (the host branch of `frame_tick`, `g.models`, `load_scene_models`) | — | Every frame presents through 0x00033A20 (`present_views`), whose per-view draw is the translated 0x0011C240: the mirror pass, the actor bins, 0x00121900, the background's 0x00042C10, model sprites and the billboard flush, debris, dialogue, the text pools (every frame, as 0x0005F330), the transition book (0x00048550) and the transition view (0x0012C490) (`docs/NATIVE_SCENE_PRESENT.md`, `docs/NATIVE_TRANSITIONS.md`). Still needed: the bodies of the HUD, decal, shadow-volume, refraction, screen-effect and effect passes as their gates open |
| Model rendering inside the renderer | `src/engine/graphics/gfx_d3d11.cpp` (3.6k lines) | Besides being the D3D11 backend, it holds its own versions of retail functions (0x0015AD20, 0x0015ABB0, 0x00158A80, 0x000F9250, 0x00058150, 0x0005E0C0/E110/E2A0, …) over host pointer casts, its own material shader and its own skinning | Native translations already exist for most of these (`model_shader_parameter_draw`, `model_tree_dispatch`, `screen_model_draw`, `dialogue_draw_state`). Reduce `gfx_d3d11` to a presenter of `SceneD3DDevice` output (vertex/index upload, texture decode, state mapping), as in `docs/NATIVE_DIALOGUE_PRESENT.md` |
| Cutscene playback | Deleted (`cutscene_runtime.cpp`, `g.cutscenes`, the host props and the debug "Cutscenes" window). `parse_cutscene` (`src/gbtg/scripts/cutscene.cpp`) stays as a format parser with no game caller | — | Every cutscene, the attract and the transition books included, runs on the native stack (`docs/NATIVE_CUTSCENES.md`) |
| Dialogue screen | Deleted (`DialogueScreen`, `frontend_captions.cpp`) | — | Native dialogue (`dialogue_*.cpp`) in every native scene; the attract is native |
| Button bat | `src/engine/entities/entity_model.cpp` (`button_bat_draw`) | 0x0006D6C0 over host `gfx_clear_ui_depth` and `gfx_draw_screen_mesh` | `src/engine/entities/button_bat_draw.cpp` (`draw_runtime_button_bat`, native 0x0006D6C0 → 0x000F9250) |
| Front end | Deleted (`frontend.cpp`, `frontend_contents.cpp`, `frontend_keyboard.cpp`, `frontend_slots.cpp`, `frontend_text.cpp`, `frontend_types.h` with `MenuCursor`) | — | Game select runs on the translated control (factory type 0x9A, `docs/NATIVE_FRONTEND.md`); the attract is native (0x9B) |
| Book and slime transitions | Deleted (`transition_book.cpp`, `transition_data.h`) | — | The translated transition core and handlers (`scene_transitions.cpp`, `native_book.cpp`, `book_map.cpp`) on the boot-owned scene stack (`docs/NATIVE_TRANSITIONS.md`) |
| Entity system | `src/engine/entities/entity.cpp` (`register_entity_types`, `g.entities`), `entity_camera.cpp` (`g.camera`) | A host entity table (44 types) ticked from `frame_tick`, including the host body camera that feeds `g.camera` | The native object factory (`ObjectFactoryRegistry`, the factory owners and scene actor services) and the native camera slots (`g_cam_active`, display setup). The native scene already uses these |
| Audio | The movie clip's own audio (`pcm_play_pcm` / `pcm_stop_cue` in `movie.cpp`'s Bink clip); `pcm_play_owned_cue` / `pcm_stop_owned_cue` remain defined with no caller | Bink clip sound | The boot-owned native audio core (`NativeAudioRuntime`) runs every frame; the transitions' cues play through the native voices (`docs/NATIVE_AUDIO.md`) |
| Script alt-list walks | `g_alt_links` (`camera.cpp`) in script conditions 23/24 (`script_conditions.cpp`) and four script actions (`script_actions.cpp`) | Walk a host `AltLink` list that nothing fills, so they see no actors | The native `DAT_00511B48` list (`entity_detail::g_world.alt`, player `links[2]`) through `g_alt_node_body`, as condition 1 (`0x00132680`) now does (`docs/PLAYER_SCENE.md`) |
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

Status (2026-10-16):

- **Step 1 is done** (`docs/NATIVE_DIALOGUE_PRESENT.md`, "Results").
- **Step 2 is mostly done** (`docs/NATIVE_SCENE_PRESENT.md`,
  `docs/PLAYER_SCENE.md`). Native scenes draw nothing on the host
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
- **Step 5 is done** (`docs/NATIVE_TRANSITIONS.md`, "Progress"). The
  transitions run on translated code on the boot-owned scene stack: the core
  and its handler table, the fade, the book with its map route, and the
  slime. The host transition, the host branch of `frame_tick`, the host
  cutscene props, `g.models` and the host movie list (`movie_alloc`,
  `movie_present`, `movie_apply`) are deleted. Still untranslated (raising
  when reached): the book's chapter title (clip 2) and page texture, handler 3,
  and the map menu.
  Game select is switched (`docs/NATIVE_FRONTEND.md`, "Progress"): it runs on the native control in every run (slot select,
  naming and the keyboard, the save manager with saves in the `saves` folder
  beside the executable, delete, the menu book, the contents screen, the stats
  dialog, and Play into the first scene), and the host front end is deleted.
  Still untranslated (raising when reached): challenges, features and its
  screens, the final score, the bonus collection and the app error screen.
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
Read AGENTS.md and docs/LEGACY_HOST_CODE.md, then the milestone docs it references (NATIVE_DIALOGUE_PRESENT.md, PLAYER_SCENE.md, NATIVE_AUDIO.md, RENDERING_PERFORMANCE_PLAN.md). The project is native-first: game behavior comes only from translated retail functions; host code is boundaries plus the D3D11 presentation of recorded native draws. Everything in the LEGACY_HOST_CODE inventory predates that rule. Your job is to retire all of it, so the whole game (attract, game select, book/slime transitions, scenes) runs on the native path. You own the whole checkout for this.

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
