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
| Frame present | `src/engine/game.cpp` (`frame_tick`) | Draws `g.models` (the background as host meshes), positions the camera from `g.camera`, calls `draw_cutscene_props` and `draw_ui`, then `draw_scene_dialogue` outside the native scene | 0x00033A20 is translated (`present_runtime_display_views`). Still needed: the rest of 0x0011C240 (world, actors, HUD) and its presenter. Native scenes already go through `present_views`; the host world draw is still passed into it as `draw_world` |
| Model rendering inside the renderer | `src/engine/graphics/gfx_d3d11.cpp` (3.6k lines) | Besides being the D3D11 backend, it holds its own versions of retail functions (0x0015AD20, 0x0015ABB0, 0x00158A80, 0x000F9250, 0x00058150, 0x0005E0C0/E110/E2A0, …) over host pointer casts, its own material shader and its own skinning | Native translations already exist for most of these (`model_shader_parameter_draw`, `model_tree_dispatch`, `screen_model_draw`, `dialogue_draw_state`). Reduce `gfx_d3d11` to a presenter of `SceneD3DDevice` output (vertex/index upload, texture decode, state mapping), as in `docs/NATIVE_DIALOGUE_PRESENT.md` |
| Cutscene playback | `src/gbtg/scripts/cutscene.cpp` (`parse_cutscene`, `sample_cutscene_camera`), `src/engine/cutscenes/cutscene_runtime.cpp` (`g.cutscenes`, `cutscene_queue_tick`, `draw_cutscene_props`) | A heuristic parser (channel-duration checks, assumed FOV) with host prop and camera drawing | `src/engine/cutscenes/*`: `RuntimeCutsceneInstance`, `load_runtime_cutscene` / `tick_runtime_cutscene` / `draw_runtime_cutscene`, camera sampling 0x0004F770 / 0x0004F8C0 |
| Dialogue screen | `src/engine/frontend/dialogue.cpp` (`DialogueScreen`, `dialogue_skin`); `frontend_captions.cpp` (`g_dialogue_screen`) | Draws the page cutscene, bats and button bat with CPU skinning | Native dialogue (0x00058150 and the rest, `dialogue_*.cpp`) plus the presenter. The native scene is handled by `docs/NATIVE_DIALOGUE_PRESENT.md`; captions and attract still use the host version |
| Button bat | `src/engine/entities/entity_model.cpp` (`button_bat_draw`) | 0x0006D6C0 over host `gfx_clear_ui_depth` and `gfx_draw_screen_mesh` | `src/engine/entities/button_bat_draw.cpp` (`draw_runtime_button_bat`, native 0x0006D6C0 → 0x000F9250) |
| Front end | `src/engine/frontend/frontend.cpp`, `frontend_attract.cpp`, `frontend_captions.cpp`, `frontend_contents.cpp`, `frontend_keyboard.cpp`, `frontend_slots.cpp`, `frontend_text.cpp` (about 3.8k lines) | Attract reel, game-select menus, keyboard and captions, with host state and `gfx_sprite_submit` draws. Some leaves inside are translated (see `docs/GAMESELECT_RENDERING.md`), but the screens are driven and drawn on the host | Translate the front-end owners and their draw path (sprites through 0x0005E380 and related functions, recorded and presented). The native dialogue sprite path (`dialogue_draw_state.cpp`) can serve as the model |
| Book and slime transitions | `src/engine/transitions/transition_book.cpp` (1.4k lines) | Host book render, page captures, the slime effect and direct cue playback | A translated transition owner presented natively; render targets go through the outer D3D boundary (`DialogueOuterD3DBoundaries`) |
| Entity system | `src/engine/entities/entity.cpp` (`register_entity_types`, `g.entities`), `entity_camera.cpp` (`g.camera`) | A host entity table (44 types) ticked from `frame_tick`, including the host body camera that feeds `g.camera` | The native object factory (`ObjectFactoryRegistry`, the factory owners and scene actor services) and the native camera slots (`g_cam_active`, display setup). The native scene already uses these |
| Audio | `src/engine/audio/audio.cpp` (`audio_tick`: `xact_tick_voices`, `xact_tick_streams`, `pcm_cutscene_sync`); `pcm_play_cue` / `pcm_play_owned_cue` callers in the frontend and transitions | Host cue playback by name and host cutscene sync | The native audio core (`core_source`, `core_cue`, `event_voices`, `bank_streams`) on the XAudio2 boundaries in `pcm_out.cpp` (`native_audio_core_*`). Missing: sound-bank name registration (`g_audio_bank_names` is never filled), so native cues never resolve |
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

Each step must meet the same bar as any change:

- Release and Debug build.
- All transition suites and ctest pass.
- Headless runs exit 0.
- Screenshots of the affected screens show the same result as before or better.
- Leave nothing half-switched: a screen runs either fully on the native path or
  fully on the host version.

## Prompt for a dedicated agent

```
Read AGENTS.md and docs/LEGACY_HOST_CODE.md. The project is native-first: game behavior comes only from translated retail functions; host code is boundaries plus the D3D11 presentation of recorded native draws. The files listed in docs/LEGACY_HOST_CODE.md predate that rule and re-implement retail behavior on the host.

Work the "Suggested order" in that document, one step at a time. Step 1 is docs/NATIVE_DIALOGUE_PRESENT.md, so skip it if that milestone is done. For each step:
1. Inventory the host code involved and the native translations that already exist.
2. Translate whatever is still missing from cache/ghidra_frontier dumps; export more with the read-only Ghidra command in AGENTS.md.
3. Switch the callers to the native path and delete the host version once nothing uses it.
4. Verify with Release and Debug builds, the transition suites, ctest, headless runs, and screenshots of the affected screens compared with before.
5. Update docs/LEGACY_HOST_CODE.md: mark the step done and list what remains.

Never leave a screen half-switched. Don't commit.
```
