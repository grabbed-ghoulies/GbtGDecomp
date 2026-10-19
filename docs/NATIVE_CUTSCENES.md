# Milestone: native cutscenes

Goal: every cutscene plays through the translated cutscene runtime
(`RuntimeCutsceneInstance`: `load_runtime_cutscene`, `tick_runtime_cutscene`,
`draw_runtime_cutscene`, camera sampling `0x0004F770` / `0x0004F8C0`, the event
runtime in `cutscenes/event_runtime.cpp`). The host reel (`parse_cutscene`,
`sample_cutscene_camera`, `g.cutscenes`, `cutscene_queue_tick`,
`draw_cutscene_props`, `pcm_cutscene_sync`) and the host dialogue screen
(`DialogueScreen`) retire once nothing uses them.

This is step 4 of `docs/LEGACY_HOST_CODE.md`. Read `AGENTS.md` first.

## Where retail plays cutscenes

- **Scene cutscenes** (story books, the opening's imps and butler): script
  node 0x13 (`0x001274B0`) calls `0x0004EDA0`, which pushes the clip on the
  scene stack (`0x0004EB20`, DAT_004E1B40..DAT_004E1C54) and links the
  "cutscene" camera. The scene tick `0x0011E7F0` advances it with `0x0004EBD0`.
  Scene teardown releases it (`0x0004ECE0`).
- **Attract**: the attract script (`aid_script_ghoulies_attractsequence_final1`)
  creates its scene control, factory type 0x9B (getter `0x00080970`), which
  plays the clips of `aid_aidlist_ghoulies_scenecontrol_attract1` through
  `0x0004EDA0` (`0x00080B7D`, `0x00080D7E`) and shows the speech bubbles
  (`0x00080DA0`) and the Start banner (`0x00080A30`).
- **Book and map transitions**: the transition owner (`0x0012B560`,
  `0x0012BE60`, `0x0012BA10`) plays the book-opening clips and the map. These
  are step 5 (the transitions), which owns the book render.
- **Dialogue pages**: `0x000584D0` plays the dialogue page cutscene through the
  same runtime. This is already native (`docs/NATIVE_DIALOGUE_PRESENT.md`).

## Reference screenshots

`gbtg --headless --frames N --scene chapter1_scene0_1book --width 640 --height 480 --screenshot …`,
taken before this step (`cache/cutscene_ref/book0_N.bmp`):

| Frame | Screen before | Screen after |
|---|---|---|
| 60 | Slime transition | Same |
| 180, 400, 900, 1800, 3600 | Black: the native scene presented nothing, and the host queue's props were no longer drawn | The comic book page under the cutscene camera, with the panels playing their movies (400, 900), then the story caption rendered into the page (1800+), waiting for A |

The opening (`--scene chapter1_scene1_2playcam`, frames 120–900) and the
900-frame boot run are pixel-identical to the step 3 references
(`cache/audio_step/shots/`).

The transition fade (handler 0) lasts 1/60 s (`0x0032B150`) and advances only once its
timer is below zero. `0x0011E4C0` begins the transition and ticks it in the same frame, so a
1/60 s step lands on exactly 0.0 and the fade never ends: the next scene stays black. That was
true for every scene change while the port stepped 1/60 s (2026-10-19: final1 to final2).

Retail never steps 1/60 s. It creates the device with PresentationInterval 2 (`0x000FC310`
stores 2 at `DAT_00509700`; `0x001548B0` passes it to Direct3D_CreateDevice), so it swaps on
every second vertical blank. `0x000FC030` sets `DAT_005096C0` to the vblanks between the
last two swaps (`DAT_005096D0`, counted by the vertical-blank callback `0x000FC3B0`) over 60,
capped at 5 fields: 1/30 s, or more after a slow frame. `run_platform` now ticks the logic the
same way: windowed runs tick once two 60 Hz fields have passed since the last tick, with the
fields that actually passed (counted from `PlatformConfig::boot_started`, before `Game::init`
loads the first script), and present the last frame again in between (`gfx_present_again`).
Headless and screenshot runs step two fields a pass, so they reach the attract too.
`gbtg-transition-tests` still drives `game.tick(1/60)` itself; `--title` uses 1/59 s steps.

## Done (2026-10-15)

- **Scene cutscene stack live.** `SceneActorServices` installs
  `SceneCutsceneRuntime` over the scene's cutscene context. The scene tick
  calls `0x0004EBD0`, and scene teardown releases the stack. `g_50e438` (an
  empty host list) no longer receives the 0x7E/0x7F notices.
- **Script node 0x13 retranslated** on that stack (`0x001274B0` case 0x13),
  with `0x0004EFE0`, `0x000F2BB0`, `0x00087830` and the skip input
  `0x00127AC0` / `0x00127B10` / `0x00127B50`. The skip input uses the scene
  dialogue's input contexts: script mode 3 (the story books) pushes a
  "cutscene skip" entry. The host `cutscene_queue_begin_script_clip` and
  `g_script_cutscene_pending` are gone.
- **Cutscene events reach their owners.** The cutscene context's dialogue is the
  scene dialogue. The event runtime raises if a dialogue or movie event finds
  no owner, instead of dereferencing null.
- **Native movies.** `NativeMovieRuntime` (`video/native_movies.cpp`), owned
  by `Game` from boot like the audio core, holds the retail movie and file
  lists (`RuntimeMovieSession`), the event slots DAT_0050DD90 and the mode
  DAT_0050DD8C (boot value 3).
  - In `frame_tick`: 0x000E9E60 (replacing the empty host `file_pump`), the
    movie tick walk, and `0x00106C70`.
  - In present: the tick walk, `0x00106CD0` (frames into the hooked
    textures), the views, then the tick walk again.
  - `0x00106E90` / `0x00106EC0` now act on these slots (they used a duplicate
    host array), and scene teardown runs `0x00106E30`.
  - The movie decoder boundary reports Bink's 1-based `FrameNum`; finished
    movies used to stay in their slots for ever.
- **Texture hooks (DAT_00545DC0).** The scene's `special_textures` (filled by
  `0x000527C0`) are the hook table for movie frames and for dialogue pages
  rendered into a texture (`0x001417A0`). The dialogue's separate empty hook
  span is gone.
- **Presenter support:**
  - movie frames are written into the texture's scene memory (linear 32-bit
    targets only) and the texture is decoded again in place with a new serial;
  - linear (`LIN_`) textures are sampled in texel coordinates (per-stage UV
    scale in the combiner shader);
  - `SetRenderTarget` onto a texture renders into an offscreen target and is
    read back into that texture;
  - line primitives (list, loop, strip) draw as one-pixel lines.
- **Audio fix found on the way.** Native cue voices shared the host `g_bed`
  list, and the host owned-cue cleanup (still used by the book transition)
  destroyed finished native voices under the native core. This caused an
  intermittent crash in the cue notifications. Native voices are now marked
  and left to the native core.
- **Tests.** `--goop` waits for the story's native cutscene rather than the
  host queue.

### Story book lighting (2026-10-15)

The story books were about half as bright as they should be (paper about
115,81,36 against about 207,178,115 on the host-drawn map page). The
screen-space dialogue was unaffected. The page's vertex lighting had a white
directional light (c189) and no ambient (c190). Its direction (c188) was
(−1, 0, 0) in view space, edge-on to a page that faces the camera. Three port
faults caused it:

- **The light's camera was never set.** `0x000FD9C0` makes a camera-relative
  light direction from DAT_005077A8 +0xD8, the current display view's
  rotation, read live. The port kept its own copy, which nothing filled, and
  inverted a zero matrix. `RuntimeSceneLightDirection::camera` now reads the
  current display view.
- **Boot light defaults were never set.** Boot (`0x00033030` → `0x0011EA50` →
  `0x0011C3F0` → `0x000FD620`) sets pitch 0.349, yaw 0.698, camera-relative 1,
  the ambient tint DAT_00509B20..2C and the intensity DAT_00509B4C. The port
  had `0x000FD620` but never called it, and wrote duplicate globals. It is now
  called from `Game::init`; the rest of `0x0011C3F0` is still untranslated.
  DAT_00509B10..5C is one global (`g_scene_light_direction`) that the scene
  binds by reference.
- **DAT_00509B4C was a constant 0.** The light construction context took it by
  value. It now reads the global that scene load `0x0011CAC0` sets to 1.0. The
  duplicate in `lighting.cpp` is gone.

With these, the paper reads about 189–207,165–181,86–93. The opening changes
too: Cooper and the tutorial head are now lit from the camera side instead of
edge-on (frames 180, 300 and 900 differ from the step 3 references in about
5,000–8,000 pixels). This follows the retail light setup; it has not been
compared with xemu.

## Work

1. **Attract.** Done (2026-10-15); see "Native attract" below.
2. **Book transitions** (step 5): the book-opening clips and their sound windows
   in `transition_book.cpp` play on a host view. They move with the transition
   owner.
3. **Presenter gaps:** line width (render state 0x10050) is ignored, and
   swizzled render or movie targets raise.
4. **Intermittent Release crash in `--goop`.** Fixed (2026-10-15). It was at process exit,
   not in the scene reload. Static destruction freed `g_bed`'s PCM while the XAudio2 mixer
   thread still read it through voices that were still playing. stdout buffering made the log
   seem to stop mid-run. `pcm_out.cpp` now stops the engine and destroys the source voices
   before the voice lists are destroyed. 20 of 20 Release runs pass; before the fix, 4 to 5 of
   20 crashed. Details: `docs/NATIVE_AUDIO.md`, "Native cue voice lifetime and the exit crash".
5. **Native cue voices are never destroyed.** Fixed (2026-10-15). The XACTEngineDoWork
   boundary destroys a native cue's voice after its stopped notification (1) has been
   delivered. The retail callback frees the cue there (`0x000398E0` → `0x0015CE90`), and the
   handle is not used again.

## Acceptance

- The story books play natively. Done for `chapter1_scene0_1book` and, through
  `--goop`, `chapter1_scene1_1book`.
- The attract plays natively and `--title` / `--title-a` still pass. Done.
- No host cutscene code remains except what step 5 owns. Done: the main game's reel, the
  captions and `DialogueScreen` are deleted; the book transition's private reel view remains.
- Release and Debug build; ctest; the six transition suites plus
  `--player-opening`, `--title`, `--title-a`, `--goop` and `--ui`; both
  900-frame headless runs; screenshots compared; `git diff --check`.
  Debug `--actor-loading` fails at `check_boot_pixel_shaders` as on the
  unmodified tree.

## Native attract (2026-10-15)

The attract scripts (`aid_script_ghoulies_attractsequence_final1` and `_final2`) now start
natively and play the whole reel through the attract scene control, factory type 0x9B.

### Reference captures

Taken before this change with `gbtg-transition-tests OriginalFiles --title`, which now writes
`cache/title_check_N.bmp` (copied to `cache/attract_ref/`). The default headless boot can't be
used: its fixed 1/60 s step never lets the boot transition start the attract scene.

| Frame | Screen |
|---|---|
| 30, 60, 119 | Attract clip 0: bedside drawer, candle, Ghoulie hand, "Press START" |
| 300 | Page turn into game select (Start pressed at frame 120) |

The new `--attract` mode runs 8000 frames with no input and writes
`cache/attract_check_N.bmp` every 1000 frames from 500.

### Translated

- **Factory type 0x9B** (`scene/attract_control.cpp`, installed by `NativeSceneControlOwner`;
  getter `0x00080970`, vtable `0x00505548`). It covers:
  - sizes `0x00080950`, constructor `0x00080A30`, destructor `0x00080BB0`, tick `0x00080C10`;
  - banner `0x000809C0`, Start `0x00080CB0`, next clip `0x00080D20`, speech bubbles `0x00080DA0`;
  - the input mode `0x00143070` / `0x001430E0` (a keyboard device pushed as "gameselect") and
    the Start test `0x001431B0`, with the new-edge port `0x00107F50`
    (`frontend/input_activation.cpp`);
  - message `0x00115EE0` and the empty +0x14 come from type 3.

  `SceneActorServices` binds `AttractControlServices`: resources, localtext, the text pool
  registry, input, the scene cutscene stack, the actor views' secondary models and
  `script_request`.
- **`0x0004EA50`** (`cutscenes/actor_tracks.cpp`): find a cutscene actor by model name.
  `0x000357D0` (`transition_aid_equal`) moved to `runtime/name_lookup.cpp`.
- **Pooled text drawing**, which the banner needs:
  - `0x0005FC30` is now complete. It used to raise at its glyph-animation and draw tails.
  - `0x00060070` (glyph frame), `0x000602E0` and `0x0005E950` are in
    `frontend/dialogue_text_pool.cpp`.
  - Centred text `0x0005EF40` with its line runs `0x0005EC60`, and the glyph seed `0x000606A0`,
    are in `frontend/dialogue_draw_text.cpp`.
  - A record with a panel (+0x1224) still raises at `0x00060160` / `0x00061360`.
- **Effect particles `0x000ED390` / `0x000EFEA0`** (`graphics/actor_effect_draw.cpp`): type-0
  particles are queued as billboards (the attract's sparkle trail). Type 2 particles
  (`0x000F0010`) draw as world sprite quads (`0x0005CF90`) between `0x0005CF20` /
  `0x0005DFF0` and the projection rebind. Type 4 (`0x000F0160`) raises when reached.

### Fixes found on the way

- **Text pool registry rows** (`dialogue_text_system.cpp`) were 4 bytes but held 8-byte host
  pointers. A second live pool (the banner beside the dialogue's) overwrote the first, and the
  reel crashed in `0x0005F330`. The rows are pointer-width now.
- **Effect binding addresses**: a locator or shape inside an actor model's CPU view (the host's
  serialized copy) now maps to the same offset in the model's relocated block.
- **Screen ownership**: under a native scene, `frame_tick` no longer runs the host front-end tick.
  The host reel no longer starts, so its clips and sounds no longer play a second time over the
  native ones.

### Deleted

- The main game's host reel: `load_cutscene_reel`, `cutscene_queue_start`, `cutscene_queue_tick`,
  the reel camera events, wobble and bubble code, `pcm_cutscene_sync` with its event list and
  `pcm_dialog_hold` / `pcm_dialog_advance`, and `movie_sync_cutscene`.
- The host attract banner (`frontend_attract.cpp`; `banner_delay` moved to `frontend_text.cpp`
  for the game-select goo text), the host captions (`frontend_captions.cpp`), `DialogueScreen`
  (`frontend/dialogue.cpp`, `gbtg/dialogue.h`), the mode-0 Start path, `attract_confirm_pressed`
  and `Game::attract_banner_latched`.
- Kept for the host book transition (step 5): `parse_cutscene`, `sync_cutscene_props`,
  `draw_cutscene_props`, `release_props` and `actor_curve`, which run on that transition's
  private `Game` view.

### Results

- `--title` and `--title-a` pass on the native attract.
- `--attract` plays all 7 clips of final1 and moves to final2 at frame 6821 (9 clip starts in
  8000 frames). Its frames show the Rare book, the logo with its glyphs assembling, the
  game-footage pages, the Ghoulies tearing the page, the glow-in-the-dark logo and the Microsoft
  book, with the flashing goo "Press START".
- Against the references, frames 30–119 differ throughout:
  - the native banner assembles with the retail glyph delays, so it isn't up yet at frame 30;
  - the drawer's keyhole plates show;
  - the candle saucer is lit darker under the scene light.

  None of this has been compared with xemu.
- Each attract cue plays once; before the switch the host reel doubled them.
- The opening (frames 180, 300 and 900) and the story book (400, 900, 1800) are pixel-identical to
  the step-4 references in `cache/book_light`.
- In the default fixed-step headless boot, the attract scene never starts. It used to show the
  host banner over the host intro background; now it shows the background alone. That host
  background is the legacy front-end path for a scene that never started.
- Release: build; ctest 6/6; the six transition suites plus `--player-opening`, `--title`,
  `--title-a`, `--attract` (3 of 3 runs), `--goop`, `--ui` and `--audio-preparation`; both
  900-frame headless runs.
- Debug: build; ctest 6/6; the same suites without `--player-opening`; `--attract` 3 of 3. Debug
  `--actor-loading` still fails as on the unmodified tree.
- Not listened to on hardware.

### Candle lighting: fog (2026-10-18)

The attract's room is dark until the hand lights the candle. The clip does this with black fog,
not lights: event 0x1A at t=0 sets colour `ff000000`, start 0, end 44.6, and event 0x1B ramps the
end to 4419.6 between 3.08 s and 18.75 s. The native events already set the fog; the presenter
ignored it, so the room was lit from the first frame.

- The game writes D3DRS_FOGENABLE directly (`fog_render_state`, 0x00158A80 / 0x00156EA0, dirty bit
  0x2000). `SceneD3DDevice` flushes it as `kFogEnable` at each vertex-program draw, the way D3D's
  lazy update does. `kFogColor` is the recorded 0x10061.
- The vertex program's `oFog.x` (the model programs compute `(end - z) / (end - start)` from
  `fog_scaled_end` and `fog_reciprocal`) goes per vertex through both the CPU and GPU paths.
- `ps_model` exposes the FOG register (fog colour, factor in alpha). A definition without a final
  combiner gets D3D's spec/fog blend: FOG.a × R0 + (1 − FOG.a) × FOG.rgb. Draws without a pixel
  shader and fixed-function draws are not fogged.
- Also fixed: the scene light builder (0x000FD3F0) read its own empty copy of DAT_00509B7C instead of
  the light records 0x000FDF60 registers. It now reads the registered records. The attract registers
  none, so this doesn't change the attract.
- Checked: windowed `--scene 0` captures (dark, candle, room brightening, books lit); headless
  `chapter1_scene1_2playcam` and `chapter1_scene0_1book` at frame 300 look normal; ctest 7/7.
  Not compared with xemu.

### Fog between scenes (2026-10-19)

The attract's black fog (`ff000000`, end 44.6 at its last 0x1A) stayed in the fog record after the attract
ended, so game select's page cutscene, which sets no fog, drew its background into black fog: the delete,
naming and features screens had no page behind them.

- Retail resets the record at scene teardown: 0x000FD710 (from 0x0011C6C0) drops the light records
  (0x000FDED0), clears the shadow-volume counts and sets the fog record (DAT_00509AFC..DAT_00509B0C) to off,
  start 1, end 1000, colour `ffc0c0c0`, and the base light colour to (0, 0, 0, 1). It is
  `reset_runtime_light_environment` (`scene/lighting.cpp`), called from the actor services' `end_scene`.
- The scene tick applies the record each frame through 0x000FD5C0 (`configure_scene_fog`), while the scene's
  actors are started, as retail's 0x0011E7F0 does.
- Checked: `--attract` passes; the five game-select modes, `--gameselect-features` and `--scene6` pass, and the
  scene6 captures are byte-identical. The game-select captures that changed are the delete and naming screens
  (now on the page) and the chapter 1 book after Play, which is no longer darkened. Headless 900 frames exits 0.
  Not compared with xemu.

- Game select (`aid_script_ghoulies_gameselect_gameselect`) stops at factory type 154 (0x9A) and
  falls back to the host front end. That's step 5.
- The book and map transitions are still host (step 5).
