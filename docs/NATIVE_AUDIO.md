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
