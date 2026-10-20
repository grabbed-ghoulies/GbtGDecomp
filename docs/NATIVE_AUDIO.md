# Milestone: native audio in the opening scene

Goal: the opening scene (`chapter1_scene1_2playcam`) plays its retail audio
through translated code: background music, tutorial dialogue voice, actor and
effect sounds. The XACT/XAudio2 calls are the host boundary (`pcm_out.cpp`);
everything above them is translated.

Read `AGENTS.md` first. `docs/LEGACY_HOST_CODE.md` lists the host audio
stand-in (`audio_tick`, `pcm_play_cue`), which keeps frontend and transition
cues working until they move to the native path.

## Findings (2026-10-14)

The opening plays only two host cues (`GMISC_SLIMEON/OFF`). Nothing native in
the audio stack is live:

- **Bank and cue names are never registered.** Boot `0x00033030` calls
  `0x0003A280`, which:
  - registers the XACT notification callbacks (`0x0015B730`, `0x0015B750`)
    and runs `0x0015B960` and `0x00039EF0`;
  - parses the `bank<TAB>cue` text list into `DAT_004E0CDC` through
    `0x000392D0` → `0x000390A0` (bank) and `0x00039170` (cue);
  - loads the soundbanks `aid_xsoundbank_ghoulies_<bank>` (`0x00039F50`
    → `0x0015CB10`) into `DAT_004E13B0`;
  - then runs `0x00039DD0`, `0x0003A1F0` (translated), and `0x0015C1C0` over
    `DAT_00421C7C` (translated).

  The port fills none of it, so `find_audio_bank_name` never resolves.
- **No native audio contexts exist at runtime.** `AudioEventVoices`,
  `AudioBankStreams`, `AudioCoreCues` and `AudioCoreSources` are translated
  but never instantiated outside tests. The frame's audio tick is the host
  `audio_tick` stand-in.
- **The scene's music object is never created.** The opening queues
  `aid_objparams_ghoulies_scenecontrol_*` through the script (`0x0011C950`).
  Scene start `0x0011CBD0` creates them in `0x0011C980` (untranslated) and
  activates them with `0x00115FF0`. The port has `0x00115FF0`, `0x00116090`
  and `0x001160F0` translated but calls none of them; the scene tick walks a
  host `g_50e438` list that is never filled.
  - The opening's control is type 0x91, `entitySceneControlChapter1Scene2`
    (getter `0x00080520`, vtable `0x00505530`). It is derived from type 3
    (getter `0x00115D50`, vtable `0x0050E41C`): sizes `0x00115D30`,
    constructor `0x00115DA0`, destructor `0x00115DE0`, message `0x00115EE0`,
    tick `0x00115E30`.
  - Type 0x91: sizes `0x00080500`, constructor `0x00080570` (builds a
    shuffled 0x2A-track playlist with `0x00080670`), destructor
    `0x000805B0`, tick `0x00080600`. It plays the walkabout music through
    `0x000808B0` → `0x00038910` from the name table at `0x002FFFB8`
    (`XACT_SOUNDBANK_GSTREAMSWALK_WALKABOUTn`); `0x00080810` releases the
    current track.
- **Dialogue voice** (`0x00057D10`) asks `AudioEventVoices` for the page's
  cue, which needs the same registry, banks and contexts.

## Work

1. **Boot registration.** Translate `0x0003A280` and its callees, run it
   from game init in boot order, and own the native audio state for the
   game's lifetime. Untranslated XACT engine calls are host boundaries in
   `pcm_out.cpp`.
2. **Live contexts and frame tick.** Instantiate the event voices, bank
   streams and core cues over the `pcm_out` boundaries. Run the native frame
   audio (`0x0015B780`, `0x0003A040`, `0x000396F0`, `0x00038640`,
   `0x00038C20`) in place of the host `audio_tick` for native objects.
3. **Scene controls.** Translate `0x0011C980` and wire `0x00115FF0`,
   `0x00116090` and the per-frame update into scene start and the scene tick.
   Install type 3 and type 0x91 with their methods.
4. **Dialogue voice.** Bind the native dialogue's voice request to the live
   event voices.
5. **Actor sounds.** Route `RuntimeActorSoundQueues` (`0x0012A150`) to the
   live core.

## Acceptance

- In the opening, the walkabout music starts, the tutorial lines are voiced,
  and actor sounds play. Confirm by listening and with a log of each cue
  started through the native core.
- Release/Debug builds, CTest, all transition suites, both 900-frame headless
  runs, and the hardware check `--audio-preparation`.

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
- Each source voice and each 3D source submix sends dry to the master voice and
  wet to `g_reverb`.
- The submix stays muted until the game sets listener parameters.

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

- The shadow step `0x00120EF0` is held in `run_scene_pass`. It is translated,
  but its volumes reach the draw `0x000FE960`, whose per-record draw
  (`0x000FE5A0`, `0x000644F0`, `0x00064820`, render-to-texture shadows) is
  not translated and raises. The sort `0x000FE430` is translated.
- The boot light setup `0x000FD620` never runs, so `DAT_00509B74` (easing)
  stays 0.

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
- The Xbox banks carry each sound's own distance curve; the host uses one
  inverse-distance curve, full level within 40 world units
  (`kSpatialFullLevelDistance`). This is a host choice, not a retail value.

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

Step 3 of `docs/LEGACY_HOST_CODE.md`.

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

### Still on the host

These cues belong to host screens that later steps replace. Moving only their
audio would leave each screen half on each path:

- captions (`frontend_captions.cpp`): the motif loop, the page cue, and
  `pcm_dialog_hold` / `pcm_dialog_advance` (steps 4–5, with `DialogueScreen`);
- the game-select cursor bat (`MenuCursor` in `frontend_types.h`): its own
  actor sound queue over host voices, instead of the global queues that
  `0x0012A150` dispatches (step 5);
- book cutscene record cues (event types 1, 2 and 0x1E) and their sound
  windows (`transition_book.cpp`), and `pcm_cutscene_sync`: these are host
  cutscene playback (step 4, `RuntimeCutsceneInstance` and
  `event_runtime.cpp`);
- `movie.cpp` stops a clip's cue by name (the movie boundary).

Also still to do from the earlier list: scene start's ambient sounds
`0x0012A1E0` (with `0x00132040` and `0x00049BA0`).

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
