# Legacy host code to replace

The project's workflow is native-first (`AGENTS.md`, "Native-first workflow").
Game behavior comes from translated retail functions running on retail state, in
the retail order from `frame_tick`. Host code supplies only the boundaries and
presents what the native draw records.

The runtime inspector also uses the native background/actor bins, mirror pass,
world sprites and billboard flush through the shared view-context adapter. It
uses recorded native draws. Its separate presentation entry
point excludes simulation, dialogue, frontend and transition progression.

Code written before that rule re-implements retail behavior on the host: its own
parsers, sampling, timing and menu logic, drawn straight through `gfx_*`. It
often cites Xbox addresses but isn't a branch-for-branch translation. It still
runs every frame outside the native scene (attract, game select, book
transitions), and it shares the frame with the native scene. This document
lists it and the order to replace it in. **Do not extend anything listed here.**
When native code covers an item, move its callers over, delete the host version
once nothing uses it, and update this file.

Scale: roughly 10k lines across about 15 files at the start; about 5.7k were deleted on 2026-10-30. This is a multi-session job for a
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
| Model rendering inside the renderer | Host model path deleted (2026-10-30): `gfx_set_camera`, `gfx_set_dialog_camera`, the dialogue texture calls, `gfx_draw_screen_mesh`, `gfx_set_bones`, `gfx_draw_mesh` and `gfx_release_mesh` are gone from `gfx.h` and all three backends, with the D3D11 mesh upload, material shader path and skinning. The uncalled retail copies 0x0015AD20, 0x0015ABB0, 0x00159AC0, 0x0005E0C0, 0x0005E2A0, 0x000F9250, 0x00058150, 0x00151E64, 0x00151F62, 0x00171BB0, 0x00175840, 0x00175930, 0x00176EE0..0x00177220, 0x001782C0 and 0x001799A0 are deleted. What stays in `gfx_d3d11.cpp` is the present path: 0x0005E110's device state in `present_draw_frame`, the 0x000F6F70 / 0x000F6170 viewports, the 0x00066080 depth list and the 0x00033A90 clear and swap | — | The native translations (`model_shader_parameter_draw`, `model_tree_dispatch`, `screen_model_draw`, `dialogue_draw_state`) draw into `SceneD3DDevice`; the backends present that output |
| Cutscene playback | Deleted (`cutscene_runtime.cpp`, `g.cutscenes`, the host props and the debug "Cutscenes" window). `parse_cutscene` (`src/gbtg/scripts/cutscene.cpp`) stays as a format parser with no game caller | — | Every cutscene, the attract and the transition books included, runs on the native stack (`docs/NATIVE_CUTSCENES.md`) |
| Dialogue screen | Deleted (`DialogueScreen`, `frontend_captions.cpp`) | — | Native dialogue (`dialogue_*.cpp`) in every native scene; the attract is native |
| Button bat | Deleted (`button_bat_draw` in `entity_model.cpp`, 2026-10-30) | — | `src/engine/entities/button_bat_draw.cpp` (`draw_runtime_button_bat`, native 0x0006D6C0 → 0x000F9250) |
| Front end | Deleted (`frontend.cpp`, `frontend_contents.cpp`, `frontend_keyboard.cpp`, `frontend_slots.cpp`, `frontend_text.cpp`, `frontend_types.h` with `MenuCursor`) | — | Game select runs on the translated control (factory type 0x9A, `docs/NATIVE_FRONTEND.md`); the attract is native (0x9B) |
| Book and slime transitions | Deleted (`transition_book.cpp`, `transition_data.h`) | — | The translated transition core and handlers (`scene_transitions.cpp`, `native_book.cpp`, `book_map.cpp`) on the boot-owned scene stack (`docs/NATIVE_TRANSITIONS.md`) |
| Entity system | Deleted (2026-10-30): `register_entity_types` and the type table, `Entity` / `EntityType` / `EntityMethods`, `g.entities`, `g.actors`, `g.camera` and the host body camera's tick, message, 0x00072B10 and 0x00072CB0 copies. `frame_tick` no longer ticks a host entity list. The host camera globals in `camera.cpp` (wash, pairs, channels, pulse, timers, focus, anchors) had no caller and are deleted too | — | The native object factory (`ObjectFactoryRegistry`, the factory owners and scene actor services) and the native camera slots (`g_cam_active`, display setup) |
| Audio | The movie clip's own audio (`pcm_play_pcm` / `pcm_stop_cue` in `movie.cpp`'s Bink clip). `pcm_play_cue` and `pcm_play_owned_cue` are deleted (2026-10-30); `pcm_stop_owned_cue` stays as the release of the prepare boundary | Bink clip sound | The boot-owned native audio core (`NativeAudioRuntime`) runs every frame; the transitions' cues play through the native voices (`docs/NATIVE_AUDIO.md`) |
| Script alt-list walks | Done (2026-10-25); `g_alt_links` and `AltLink` deleted (2026-10-30) | — | Conditions 23 and 24 (`0x00132F30`, `0x00132FA0`) and script types 0x08, 0x6B, 0x78 and 0x97 (`0x00128050`, `0x00126A10`) walk the native `DAT_00511B48` list through `g_alt_node_class`, `g_alt_node_body`, `g_alt_node_actor` and `g_alt_node_actor_bytes` |
| Script range-link walks | Done (2026-10-30): the loading fallback, `g_range_links`, `RangeLink` and `CamFocus` are deleted. `runtime_loading_actors()` and the script actor queries raise with no scene bound | — | Condition 18 (`0x00132DB0`) and script type 0x90 read the native DAT_00511B38 walk (`g_loading_actors`). The queries 0x001291A0, 0x00128DF0 and 0x00128F70 walk `RuntimeScriptActorQueryContext::actors`, which `SceneActorServices` builds from `g_world.mark` (players' actor +0x934): body +0x30..+0x96, +0x48, +0x15C and +0x5C's +0x2C, actor +6, +0xCE0 and the +0x958 mind's +0xC source and +0x10 record (a freed record resolves through `retired_targets`). In scene5 the skeletons now find Cooper (his energy falls to 7, not 13) and the kill count reaches its 8 |
| Script player mark | Done: `g_actor_mark` had no reader and is deleted (2026-10-30) | — | The scene's player link (`SceneActorServices::player_link`), as conditions 8 and 9 (`0x00132940`, `0x001329A0`) and the exit volumes (`0x00140E30`) read it; script type 0x9F walks the native DAT_00511B38 |
| Actor host aliases | `Actor::mask_mode`, `mask_flag`, `mask_loading`, `mask_span` (actor +0xCDC, +0x93C, +0x984, +0x980) | Host copies of retail actor words. `mask_mode` / `mask_flag` are written where retail writes +0xCDC / +0x93C: 0x00028830 (player mode), 0x00029270 and 0x00028A30 (the player's case link), 0x00145C80 / 0x00146230 / 0x000CF670 / 0x001454F0 (weapon +0x93C..+0x93F) and 0x001495C0 (weapon +0xCDC). A 2026-10-18 probe of 0x00020DC0 read Cooper as mode 2, +0x93C 0x00511B3C and the imps as mode 1, so the damage readers see the retail words. `mask_loading` and `mask_span` are never written | `0x00028360` and the type-0x6F wait read a scene player's own ring and +0x984 (`RuntimeScriptActorQueryContext::goals`); `mask_loading` is read only when no scene player matches (unit tests). `mask_span` is still read by the camera mask (`EntityMaskHost`: `entity_camera.cpp`, `camera_body_slots.cpp`, `camera_mind_rig.cpp`, `scene_body_messages.cpp`, `scene_actor_services.cpp`), which therefore always reads code 0 |
| Actor id lookups | Deleted (2026-10-30): `g_pulse_nodes`, `PulseNode`, the host `find_actor_by_id` / `find_actor_by_group` | — | `find_runtime_actor_by_id` / `find_runtime_actor_by_class` over the native latch list. The spawn class limit `0x0013E860` counted the empty host list in native scenes; it now counts DAT_00511AA4 nodes by their +6 tag, as the dump does |
| Ghouly box controller | Deleted (2026-10-30, `ghouly_box_controller.*`, `ghouly_box_messages.*`) | — | The native ghouly swap setup (type 0x26, `ghouly_swap_setup.cpp`, `0x0006BB10`..`0x0006C0A0`); the `--actor-loading` checks of 0x0006BB70, 0x0006BC70, 0x0006BCA0 and 0x0006C0A0 now drive it |
| Script VM fallback | Deleted (2026-10-30): `script_queue_scene_start` no longer catches a failed native start (`actor_start = -1`) or queues the scene-start event itself, and `Game::strict_native_start` is gone. A full regression run (about 140 modes) logged no stopped native start | — | `SceneActorServices::start_scene` (0x0011CBD0) queues the 0x7D event; a failure raises |
| Script pair updates | Done (2026-10-30): script type 0x98 with a live 0x7D (`0x00124FC0`) walked a host `g_body_links` (DAT_00511B20) that nothing filled; `BodyLink` and `CamPairObject` are deleted | — | `SceneActorServices::set_body_list_model_nodes` walks the native DAT_00511B20 (backgrounds and players) and runs 0x000F96F0 on each node's +0x204 and +0x310 models, as type 0x9F does |

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
- **Step 6 is mostly done (2026-10-30).** The host entity table and body camera, the host
  model path in the renderers, the host ghouly box controller, the pulse-node lookups, the
  host camera globals and the host pair and range lists are deleted; see the inventory rows.
  Verified: Release builds (D3D11 and Vulkan), ctest, `--actor-loading`,
  `--ghoulybox-decoder`, `--audio-preparation`, `gbtg --headless --frames 900` with and
  without the opening scene, and screenshots at frames 300 and 900 of the attract and the
  opening scene, pixel-identical to the previous commit's build. The full regression set
  (about 140 modes) matches the previous commit's build: the same 33-36 scene modes fail on
  both (fights that never end while the actor queries find nothing, `scene2a2-reaper`'s
  untranslated contact payload 79, and a stale player-mind target in script type 0x6B), and
  scene2a16 / scene2c7 / scene2b10 / scene3a12 pass or fail from run to run on both. The
  script VM fallback is deleted, and the script actor queries walk native DAT_00511B38
  views. Open: the actor mask aliases and the Bink clip sound.
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
