# Audio

The game's audio plays through translated code: the scene and front-end music, dialogue voice, actor and effect
sounds, and the front-end and transition cues. The boot-owned native audio core (`NativeAudioRuntime`) ticks every
frame. The XACT and XAudio2 calls are the host boundary (`pcm_out.cpp`). A Bink clip's own sound track is a library
boundary: FFmpeg decodes it and XAudio2 plays it at the clip's retail volume.

Status (2026-11-08): the 3D attenuation leaves out the buffer's mute-past-maximum flag (+0x58 bit 2), whose source
was not found, and the cone and doppler factors (see "Frame audio order and 3D positioning"). Tests run under the
virtual audio clock (`AGENTS.md`, "Test volume").

Read `AGENTS.md` first.

## Progress (2026-10-14)

- **Boot.** `boot_native_audio` (`pcm_out.cpp`) runs the translated
  `0x0003A280` chain in `audio/native_audio.cpp` (`0x000FC120`, `0x000390A0`,
  `0x00039170`, `0x000392D0`, `0x00039F50`, `0x0015CB10`): 72 banks and 1389
  cue names. XACT engine creation, the DSP image, wave banks, bank creation
  and notification registration are host boundaries. `0x0003A1F0` and the
  `0x0015C1C0` category loop are not run at boot yet.
- **Scene audio contexts.** The scene's event voices use the boot banks
  (`DAT_004E13B0`), and its cue boundary names cues from the boot registry.
  `0x00038BF0` initialises the tracked channels (boot runs it in retail; the
  port runs it when the scene's bank streams are created).
- **Notifications.** `dispatch_native_audio_notifications` is the host side
  of XACTEngineDoWork's cue notifications: 3 prepared (the event voice then
  plays its cue), 0 playing, 1 stopped. `SceneActorServices::update_audio`
  runs it and `0x000396F0` each frame after the host `audio_tick`.
- **Music.** Script node 0x45 (`0x00127340`) now calls `0x00116C90`, which
  creates `scenecontrol_ambient_tune`, type 0x91. Types 3 and 0x91 are
  installed by `SceneFactoryRuntime`, because the script creates the control
  while the scene loads; the control owner's clock and streams are bound when
  the actor services start. DAT_0054DD7C now lives in the factory runtime and
  is shared with the actor services. Scene start runs `0x0011C980`,
  `0x00115FF0` and `0x00116090`, and the scene tick walks the native active
  list. Script node type 2 now creates its object immediately when the script
  is running (the retail `0x00107720` / `0x00107520` branch). The opening
  prepares `GSTREAMSWALK_WALKABOUT<n>` from the shuffled playlist and holds it.
- **Dialogue voice.** The tutorial's Crivens line prepares through the native
  event voices.

Still to do:
- scene start's ambient sounds `0x0012A1E0` (and `0x00132040`, `0x00049BA0`),
  which `start_scene` does not run yet;
- the stream slot tick (`0x00038640`) and idle channel release (`0x00038C20`)
  over the native bank streams;
- fight music (`0x00116C50` → `0x00116D20`) for script node 0x55;
- actor sounds: `0x0012A150` now runs every frame inside `0x0011E640`, but no
  actor queues a sound in the opening before the fight.

### Listener reverb (2026-10-14)

Opening dialogue calls the reverb preset (`0x0003A1F0`), which reaches the host
listener boundary. The boundary used to put a fully wet (WetDryMix 100) XAudio2
reverb on the mastering voice. That replaced the whole mix with reverb, so audio
went almost silent once the dialogue box opened. On Xbox the I3DL2 listener
reverb is a separate wet path added on top of each voice's dry signal. The
boundary now copies that:

- `pcm_out.cpp` keeps a reverb submix (`g_reverb`).
- Each 3D source submix sends dry to the master voice and wet to `g_reverb`
  (2D voices do not; see "Default mix bins" below).
- The submix stays muted until the game sets listener parameters.

#### Default mix bins (2026-11-11)

On an 8-channel device a recording of the ballroom had most of its level on the centre speaker alone.

- DirectSound gives a voice created without a mix bin list (XACT's 2D voices, `0x001C38BB`) the list from the
  table at `0x001D09F4` by its channel count / 2 (`0x001C7493`): mono and stereo voices go to bins 0 and 1, front
  left and right, at 0 mB; a mono voice plays at full level on both. A voice played into a 2D source keeps that
  list (`0x001C437F`), as the source's left and right. The game's own 2D sources (modes 5..8) take bins {0, 1} and
  no reverb; only its 3D sources call `0x0015C170`.
- No default list has bin 10, the I3DL2 reverb's input. Only the 3D lists name it (`0x001D0B34`: 3D front and
  back left and right and 10; `0x001D0B24` / `0x001D0B2C`, the 5-channel 3D lists XACT gives its 3D sources, add
  front centre).
- Host: `route_to_master` sends 2D cue voices, host PCM voices and 2D source submixes to the master only, at the
  default bins (XAudio2's own default puts a mono voice on the centre speaker alone). Only 3D sources send to the
  reverb. Cue voices in a 2D source take the default bins into it.
- The ballroom's centre level did not change: it is the disco's 3D source, straight ahead of the listener, which
  X3DAudio pans to the centre (0.80 centre, 0.01 front left in a logged sample). Retail's 5-channel 3D list
  includes front centre, so a 3D source ahead reaching the centre speaker matches it. The back channels' reverb
  from 2D voices is gone.

### Scene pass `0x0011E640` (2026-10-14)

`SceneActorServices::run_scene_pass` runs every frame after `script_pump`.
`frame_tick` calls it while `DAT_0050EA9C` has bits 0x80 and 0x40000 clear.
In retail order it runs:

1. Control activation (`0x00115FF0`, `0x00116090`, `0x001160F0`; the last one
   runs each new control's +0x10).
2. Actor activation, 0x7D and first update (`0x00123070`, `0x001231A0`,
   `0x00123270`).
3. In run states 1 and 2: the scare timers `0x00123BA0`, global effects
   `0x000ED480` and scene times `0x0012B1F0`.
4. Actor sounds `0x0012A150`.
5. Control disposal `0x00115F70`.
6. Retired actors `0x001232D0`.
7. The shadow volumes `0x00120EF0`.

Scene start now also drains the update and start lists (`0x001160C0`,
`0x00123240`), as `0x0011CBD0` does, so the first pass does not repeat 0x7D.

New translations:

- `scene/scene_shadow_casters.cpp`: `0x00120EF0`, `0x00120FE0`, `0x001211E0`,
  `0x00121160`, `0x00121E30`, `0x001238F0`, `0x00052270`, `0x000FE4D0`,
  `0x00040B20` and `0x00106B90`.
- `graphics/light_volume_records.cpp`: `0x000FE320` and `0x000FE260`.
- `camera/view_frustum.cpp`: `0x00159BA0`.
- `scene/scene_actor_removal.cpp`: `0x001232D0` and `0x000F4970`.

`0x000F4C10`, `0x000F4CC0` and `0x000F4B90` raise when a showing notice's
actor is removed. `0x001287F0`'s light defaults (offset 0, 30, -30; limit 5)
are set when the scene name resets. The opening registers six shadow volumes
before the tutorial.

Known gaps:

- The shadow step `0x00120EF0` was held in `run_scene_pass` until its draw `0x000FE960` and the per-record draw
  (`0x000FE5A0`, `0x000644F0`, `0x00064820`) were translated; all of them are now.
- The boot light setup `0x000FD620` did not run then; it is translated now (`scene/lighting.cpp`).

### Camera fix found through the scene pass (2026-10-14)

Once `0x001232D0` started freeing retired actors, the opening rendered as loose
polygons.

- **Cause.** `0x0013CCD0` sends `{0xBA, source}` to the new camera (the word
  after 0xBA is written as 0, then overwritten with ESI). The port sent
  `{0xBA, 0, actor}`, so the director (`0x000AA250`) had no target and removed
  itself on its first update. Freeing it removed the only display camera, so
  present fell back to the host camera at the origin.
- **Next frontier.** With the target fixed, the camera body enters its stand
  state, a type 0x5D `entityStateAnim2Stand` action (vtable `0x005054D0`).
- **Translated for it:**
  - `0x0007FDF0`, `0x0007FE60`, `0x0007FE70`, `0x0007FEC0`, `0x0007FFB0`,
    `0x000800A0` and `0x001405E0`;
  - bound in `SceneBodyMessages` (message, tick and step slots).
- **Result.** The gameplay camera now follows Cooper from above, before and
  after the tutorial.

### Camera height fix (2026-10-14)

With the director live, the camera rose to about 190 units above Cooper. The
rig (`0x00078840`) adds DAT_00434014 to the eye height, and the port read the
.data sentinel 999.999 there. Retail resets DAT_00434008..DAT_00434030 and
DAT_00545AEC..DAT_00545AF8 in `0x0013CBD0`, called from scene load
(`0x0011CAC0`).

- `0x0013CBD0` is now translated as `reset_runtime_camera_view_tunables` and
  called from `scene_reset_runtime_parameters`.
- The rig, the script camera tunes (type 0x61, `0x00140E30`) and that reset
  now share one block, `g_camera_view_tunables` (DAT_00434008), seeded with
  the retail .data words.
- `g_tune_5aec` is now the same storage as `g_look_profile` (DAT_00545AEC).

### Actor sounds start (2026-10-14)

Footsteps were formatted and queued, but no actor sound ever started:

- **Capacities.** `0x0011CBD0` sets the actor sound group capacities
  (DAT_00512520 = 0x14, DAT_00512534 = 6) right after its marker load. The port
  left them at 0, so `0x0012A7D0` started nothing. `State::start_background`
  now sets them.
- **Reverb curves.** The scene's event voices had an empty reverb table, so the
  first 3D voice (`0x00039620`) read out of bounds. They are now bound to the
  retail DAT_00421A38 pairs.
- **Stopping actor sound voices.** `0x0012A3D0`, `0x00129F70` and the intro
  sound reset stop a voice of DAT_004E0CE8. In the scene,
  `RuntimeActorSoundQueues::stop_voice` routes that to the event voices, as the
  bank streams do; tests still bind the voice span.
- **Mind targets.** `refresh_mind_targets` replaced every target object on each
  call, including calls nested in message handlers, so an imp engaging a
  target used a freed object. A live record now keeps its target.

In the opening walk, imp and Cooper footsteps (stone, wood, carpet), imp
chatter and laughs now prepare and play through the event voices.

### Frame audio order and 3D positioning (2026-10-14)

`SceneActorServices::update_audio` now runs the retail frame audio in order:

1. the cue notifications `0x0015B780`;
2. the listener `0x0003A040` (`audio/core_listener.cpp`, with the XACT listener
   calls `0x0015BCB0`, `0x0015B7F0` and `0x0015B840`);
3. the event voices `0x000396F0`;
4. the stream slots `0x00038640`;
5. the idle channel release `0x00038C20`.

`0x000389C0` and `0x00038EF0` moved from `scene/scene_controls` to
`audio/bank_streams`.

Engine values. `0x0015B960` stores DAT_004D97F0 = 1.0 and DAT_004D97EC = 1
(DAT_00336140) beside its engine boundary. The scene's sources used 0, which
put every 3D source at the origin.

Host 3D. The XAudio2 boundary had no 3D, so every 3D source played centred and
at full level.

- Each mode-1 (mono) sound source submix is now panned and attenuated with
  X3DAudio from its XACT position and the XACT listener.
- Retail passes a top vector of (0, -1, 0) for an upright camera. That is the
  conversion from the game's right-handed view (identity looks down -Z, +X on
  screen right) into the audio engine's left-handed space, so the host uses
  the vectors as given; a source at +X plays on the right.
- Each 3D sound's distance level is its sound bank record's, as on Xbox (below). The source submix only pans
  (X3DAudio with a flat distance curve).

#### Retail 3D distance (2026-11-07, issues #31 and #36)

The ballroom DJ (chapter 2a scene 4) plays `GSTREAMSROOMS_DISCO_MONO` and chapter 1 scene 6's dancers
`GSTREAMSROOMS_DISCO_MUFFLED_MONO`, both on 3D sources. The host had given every 3D source one inverse-distance
curve at full level within 40 units, so both faded out a little way from their source.

- The record: each sound bank has a table of 0x28-byte 3D records at header +0x10. When a sound entry's flags
  (+0xB) have bit 0, XACT applies record `+0xC` of that entry as the voice is set up (`0x001C210A` ->
  `0x001C085E`): +0 / +2 cone angles, +4 outside volume, +8 min distance, +0xC max distance (after `FLT_MAX`),
  +0x10 distance factor, +0x14 rolloff factor, +0x18 doppler factor, +0x1C mode (2 turns 3D off), +0x1D curve
  count and +0x1E the curve's bytes in 1/255 steps (DAT_0032B154). They land in the voice's 3D block (+0x7C min,
  +0x80 max, +0x88, +0x8C rolloff, +0x90, +0x84 mode, +0x94 / +0x98 curve) that
  `CDirectSound3DCalculator_GetVoiceData` (`0x001C67F7`) reads. Footsteps: min 50, max 300. Gstreamsrooms:
  the disco min 200 / max 500 / rolloff 1.75; the muffled disco min 40 / max 320 with a one-point curve to 0.
- The level (`0x001C5941` -> `DAT_001E3B74` = `0x001C90CF`): none within the minimum distance; the distance is
  clamped to the maximum; with no curve, `-2000 * log10(1 + (d / min - 1) * rolloff)` mB (`0x001C8D85`); with a
  curve of n points, equal steps from the minimum to the maximum starting at 1, the step `trunc((d - min) / step)`
  (`_ftol`, capped at n - 1) interpolated and turned into `2000 * log10` mB (`0x001C58CB`, 0 or less is -10000).
  The rolloff is the listener's (DirectSound's default 1; the game never sets it) times the record's. The distance
  is the plain source-to-listener length (`0x001C8E42`).
- Host: `pcm_out.cpp` keeps each owned cue's record (`Sound3D`) and, whenever its source or the listener moves,
  sets the cue's send into the source to that level. A cue without the 3D flag routed into a 3D source is not
  attenuated. The cone (+0 to +4) and doppler factors are not applied; every retail record has 360/360 cones and a
  doppler factor of 1, and source velocities are always zero (below).

#### Mute past maximum, cones and doppler (2026-11-11)

- The mute-at-maximum flag that `0x001C67F7` passes to the level (`+0x58` of the 3D block, the source's +0x9C
  bit 2) is never set. A scan of every write to +0x9C in the executable finds `0x001C414B` (reset: bit 0 from
  DAT_001C5528, bit 1 for a 3D source), `0x001C38BB` (the same at buffer creation), `0x001C4240` (bit 0) and the
  listener's own +0x9C in `0x001C8705`. A sound past its maximum keeps the maximum's level, as the host does.
- The 3D records of all 75 sound banks (1,177 3D sound entries) have inside and outside cones of 360, outside
  volume 0, a doppler factor of 1 and mode 0. The game sets every source's velocity to DAT_0050DD80 (zero) and the
  listener's to DAT_00421C70 (0, 1, 0). The cone and velocity boundaries stay empty.

#### Source obstruction (2026-11-11)

`0x00039620` gives each 3D event voice's source an I3DL2 obstruction from the nine pairs at DAT_00421A38, picked
by DAT_004E13A8, and the defaults' pair (0, 0) as its occlusion. Only chapter 2 scene 4 (the ballroom) picks a
pair other than 0: index 8, -1200 mB HF and an LF ratio of 0.15, for every 3D sound started while it is set.

- `IXACTSoundSource_SetI3DL2Source` (`0x001C4A63`) stores the nine words at the source's +0xC4..+0xE4.
  DirectSound's 3D calculation (`0x001C67F7` -> `0x001CE923`) makes from them:
  - the direct level: obstruction HF x LF ratio plus occlusion HF x LF ratio, plus lDirect;
  - the direct HF level: obstruction HF x (1 - LF ratio) plus occlusion's, plus lDirectHF, clamped to -10000..0;
  - the room level: the occlusion's LF part plus lRoom; and the room HF level, which retail sums with lRoom
    (+0xC) again where lRoomHF would be expected;
  - each HF level as a one-pole low-pass pole (`0x001CE78B`) at the listener's HF reference (word 11 of the
    listener's I3DL2, 5000 Hz in the game's presets) and 48000 Hz. A 3D source's +0x9C bit 1 gives the room path
    the direct path's filter.
- The ballroom's pair gives a direct level of -180 mB and a pole of 53201/65536: -1020 mB at 5 kHz. The room path
  is unchanged in level and takes the same filter.
- Host: `pcm_out.cpp` keeps each 3D source's words (`SourceI3DL2`). `apply_spatial_source` scales the source's
  dry send by the direct level and sets the one-pole filter (`LowPassOnePoleFilter`, the pole raised to
  48000 / the mix rate) on its dry and reverb sends, and scales the reverb send by the room level. A room rolloff
  other than 0 raises: it multiplies a distance term (the 3D block's +0x68) whose writer was not traced, and the
  game always passes 0.
- FAudio (Linux) has XAudio2's state-variable filter but not the one-pole types. There, a send with no pole
  bypasses its filter (the EXT parameters' WetDryMix 0; FAudio otherwise filters every USEFILTER send), and a
  pole becomes the state-variable low-pass (Q 0.707) cut off where it gives the pole's level at the HF reference:
  the same level there, a steeper slope above it.
- Checked in `--scene2a4` with a temporary log: 5,723 voices took the ballroom's pair and computed the values
  above; every other voice computed no change. The Linux build (sniper SDK) compiles and runs the ballroom
  headless through the intro under SDL's disk audio driver without errors. Not heard on Linux.
- Checked in `--scene2a4` and `--scene6-1` with a temporary log: the disco at 657 units -1118 mB (0.28; the old
  curve gave about 0.06); the muffled disco silent past 320 and 0.89 at 70 units; footsteps full level at 5 units.

#### Cue variations (2026-11-07, issue #37)

Book cutscene voice lines (and every other varied cue) always picked the same variation: the host chose by
`frame % 10000` against the weights, and a cue at the same point of a cutscene lands on the same frame.

- XACT chooses with `0x001C2A3B` from the variation table's header word: count (bits 0..12), type (13..16), last
  choice (17..29), a pending choice (bit 30), each entry's weight range (+4 / +6) and the shuffle's played bits
  after the entries, all kept in the bank's memory. Types 0 (random, never the last choice), 2 (shuffle until
  each has played) and 4 (random) draw over 10001; 1 plays in order; 5 in order from a random start; 3 follows a
  runtime parameter (read as 0 here).
- A cue with several sounds picks its sound from its own table first (`0x001C2DFC`), then the sound picks its
  wave.
- The draw is `0x001BD3A2`: `RtlRandom` (`0x0016FCC1`) over the 128-word vector at 0x00338750, seeded once with
  `GetTickCount()` (`0x001BAC09`). Test runs (`pcm_use_virtual_audio_clock`) seed 1, so they repeat.
- Host: `pcm_out.cpp` keeps each table's state (`VariationTable`) and tags each listed wave with its tables and
  entries. Checked with a temporary log: `--scene3a1`'s stone footsteps spread over all four entries without an
  immediate repeat, and the cook's and Mr Ribs' lines vary.

### Scene music nodes and fight music (2026-10-14)

- Translated in `scene/scene_controls.cpp`:
  - `0x00116D20`: switch the scene's named stream (DAT_0050E4E0).
  - `0x00116C50`: fight music by actor tag, from the 25-row table at
    DAT_0030E490 (for example tag 23, the imps, gives
    `GSTREAMSFIGHT_0003_IMPS`); tag 1's row is the fallback.
- Script node 0x3A plays a named stream (priority 3), 0x3B releases it,
  0x55 plays fight music for an actor tag, 0x57 plays a named stream
  (priority 2), and 0x8D queues a stream slot (id 6).

- At each scene start, `0x0011CAC0` → `0x00116E00` releases the stream DAT_0050E4E0 still names (the last
  scene's 0x3A / 0x55 / 0x57 music), as 0x00116D20's first half does (2026-10-20). Before, a room's music
  played on into the next scene. Scene end (`0x00117330`) clears DAT_0050E560 so the next scene's node 0x45
  makes its own ambient tune.

Stopped cues:

- XACT reports a stopped cue (notification 1) even while it releases. The
  host now records `IXACTSoundBank_Stop` and reports it, and flushes the
  XAudio2 source buffers on every stop.
- Without this, stopped actor sounds stayed in state 9 or 10 with their
  source "in use". After about a hundred footsteps all 48 event voices were
  taken, and no stream could start.

`--player-opening` now also runs the opening's imp fight node after the walk.
It checks that the fight stream plays, that the walkabout stream pauses under
it (`0x00038AD0`), and that finished actor sounds give their voices back.

## Boot-owned audio core and front-end cues (2026-10-15)

Goal: the audio core runs every frame from boot, as in retail, and the front
end and transitions request their cues through it instead of the host
`pcm_play_cue` path.

Reference screenshots: the step-2 captures in `cache/view_present/`
(`--scene chapter1_scene1_2playcam` frames 120, 180, 240, 300, 450, 600, 750,
900 and a 900-frame boot run; see `docs/NATIVE_SCENE_PRESENT.md`).

### Done

- **One audio core for the game.** `NativeAudioRuntime`
  (`audio/native_audio.h`) holds the sound sources, cues, the 48 event voices
  (DAT_004E0CE8) and the tracked banks, channels and stream slots
  (DAT_004DF370, DAT_004E030C, DAT_004DFFA0). `boot_native_audio` builds it
  after `0x0003A280` and `Game` owns it (declared before the scene, so the
  scene is destroyed first). The scene's actor services, scene controls and
  actor sounds use it; they no longer build their own.
- **Frame audio from boot.** `frame_tick` calls `tick_native_audio`
  (`0x0015B780`, `0x0003A040`, `0x000396F0`, `0x00038640` gated by
  DAT_004E02E8, `0x00038C20`) every frame, in the place of the old host
  `audio_tick`. The host `audio.cpp` is deleted: its XACT stream, channel and
  voice tables were never filled. `pcm_cutscene_sync` (host cutscene sound)
  now runs at the start of the host `cutscene_queue_tick` (step 4).
- **Retail values that were wrong or missing:**
  - DAT_0042A840, the default cone, is (1, 1, 1) in .data; the port used
    zeros.
  - The event voices' DAT_005077A8 view (`AudioEventVoices::listener`) was
    never bound, so `0x00039B30` would have read null. It now points at a
    retail-layout copy of the display view (position, rotation, forward),
    refreshed at the start of each frame's audio tick. Cues requested later in
    the same frame use the start-of-frame camera.
  - DAT_0054DD7C is one global (`g_shared_random`,
    `runtime/random_state.h`). The factory runtime used to restart it at 0
    for every scene. `scene/ambient.cpp` still keeps a private copy.
- **Front-end and transition cues on the native voices:**

  | Cue | Host site | Retail call |
  |---|---|---|
  | `GSTREAMSDITTIES_PAUSE` | game select opens / closes | `0x000E5B40` / `0x000E5B70` (translated, `frontend/frontend_sounds.cpp`): bank stream `0x00038910` id 1, release `0x00038970` |
  | `GBARON_STARTBUTTONLAUGH` | attract Start | `0x0013BAF0` (translated): random pick, mode-0 voice, stop 1 |
  | `GSUPERSCARYSTUF_RIGHTBUTTON` | keyboard key | `0x00015260`'s tail: mode-0 voice, stop 1, `0x00039A20` |
  | `GSUPERSCARYSTUF_RIGHTBUTTON` | prompt accept (slots, contents, captions) | `0x00142460`: `0x000395E0(widget +0x1034, 1)` |
  | `GMISC_SLIMEON` | slime start | `0x0012C1D0`: mode-1 voice, stop 1, `0x00039B30(5.0)`, `0x00039A20` |
  | `GMISC_SLIMEOFF` | slime end | `0x0012BDD0`: `0x00039510(name, 1, 0)` |
  | `GSTREAMSDITTIES_CHAPTERSCREEN` | map start / release | `0x00039510(name, 0, 0)` kept at book +0x168; `0x00047370` stops it with `0x00039690(voice, 0)` |
  | `GFOOTSTEPGRAVEL_MAPGRAVEL` | map footsteps | `0x00039510(name, 1, 0)` (`0x000476A0`) |

### Results

- The opening prepares the same 71 cues. SLIMEON is prepared during
  `Game::init` and starts on the first frame's audio tick (a temporary probe on
  the play boundary showed it, then was removed).
- `--title` and `--title-a` prepare STARTBUTTONLAUGH and the PAUSE ditty
  natively. `--goop` plays SLIMEON/OFF, CHAPTERSCREEN and the map footsteps
  natively (the number of footsteps varies between runs with the map's frame
  count, as before).
- Every reference frame is pixel-identical.
- Release and Debug build; ctest 6/6 in each; the six transition suites plus
  `--player-opening`, `--title`, `--title-a`, `--goop`, `--ui` and
  `--audio-preparation` pass in Release. Debug `--actor-loading` still fails at
  `check_boot_pixel_shaders`, as on the unmodified tree. Both 900-frame
  headless runs exit 0 in both builds.
- Not listened to on hardware in this pass.

### Ambient sounds

Scene start's ambient sounds `0x0012A1E0` run from `SceneActorServices::start_scene`
before `0x00132040` and `0x00049BA0`, as 0x0011CBD0 calls them: each kind 7 marker's named
sound (DAT_0051254C) starts at its position (fires, cauldrons, the furnace, the projector,
the muffled disco), and 0x00129F70 stops them at teardown. 26 of the 134 scenes have one or two.

## Native cue voice lifetime and the exit crash (2026-10-15)

- **Exit crash (the intermittent Release `--goop` segfault).** It was not in
  the scene reload. The test had already passed and returned from `main`; the
  crash came during static destruction. `~vector<PcmVoice>` for `g_bed` freed
  the voices' PCM while XAudio2's mixer thread was still reading it through
  voices that had never been destroyed. Those voices were looping beds and
  native cues that were still playing. stdout was block-buffered, so the log
  seemed to stop in the middle of the second phase. The crash was caught with
  a Release build that had a PDB (`/O2 /Ob2 /Zi`, in `cache/build-relpdb`),
  run under a small debug-event loop over dbghelp. The faulting thread was in
  `xaudio2_9`, and the main thread was in `execute_onexit_table` →
  `~PcmVoice` → `free`. The run had to end while the mixer was reading a
  freed buffer, so only some runs crashed. In Debug, RelWithDebInfo and ASan
  builds the timing of `free` is different, so it did not reproduce there.
  Fix: `pcm_out.cpp`'s `AudioDeviceTeardown` is a static object defined after
  the voice lists, so it is destroyed before them. It calls
  `IXAudio2::StopEngine` and destroys every source voice in `g_voices` and
  `g_bed` before their storage is freed. Before the fix, 4 to 5 of 20 Release
  runs crashed. After it, 20 of 20 pass, with and without the PDB.
- **Native cue voices are released.** XACT reclaims a cue once its stopped
  notification (1) has been delivered. In retail, the callback frees the cue
  wrapper there (`0x000398E0` → `0x0015CE90`: stop if not yet stopped, then
  unlink and free), and nothing uses the handle again.
  `0x0015CE90` is the only caller, and only on that path. After the callback
  returns, `dispatch_native_audio_notifications` (the XACTEngineDoWork
  boundary) now destroys the cue's host voice and removes it from `g_bed`
  (`release_native_cue_voice`). The translated code is unchanged. With a
  temporary probe, one `--goop` run released 113 native cue voices. 13 were
  still in `g_bed` at exit: cues that were still prepared or playing. Before,
  every native cue stayed in `g_bed` until exit.

### Looping cues (2026-10-15)

Looping cues such as `GSTREAMSTEXTUNE_TEXTLOOP1` (the cutscene dialogue
backing), rain, wind and the bat flight loop played only once. That's because
the host XACT boundary (`pcm_out.cpp`) never read the play-wave event's loop
count.

- In the XACT 1.x sound bank, each play-wave event (kind 0 or 1) has an
  8-byte header. Its word at +6 is the loop count. Across all banks it is
  either 0 (play once) or 0xFFFF (loop until the cue is stopped); 120 events
  use 0xFFFF.
- `WaveChoice` now carries that count. The play path loops the wave bank
  entry's loop region (+0x10/+0x14): it plays from frame 0 once, then
  repeats the region. 0xFFFF maps to `XAUDIO2_LOOP_INFINITE`.
- TEXTLOOP1 (Gdvddemo[19]) repeats frames 585692–777216 of 822272. In
  `--goop`, each start is stopped by the dialogue through `IXACTSoundBank_Stop`
  when its page closes. Rain and wind loop and stop the same way. This was
  checked with temporary probes, which have been removed.
- A looping cue drains only when it is stopped, so its event voice stays busy
  until then, as on Xbox.
