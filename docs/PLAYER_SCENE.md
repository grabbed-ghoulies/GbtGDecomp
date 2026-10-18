# Milestone: playable opening scene

Goal: `aid_script_ghoulies_chapter1_scene1_2playcam` is playable end to end on
the native path:

- the tutorial dialogue holds the script until A;
- Cooper walks under the left stick and attacks under the right stick;
- the imps fight back, take damage, die and are removed;
- the script passes its `scriptConditionId_NumberGhouliesAlive` gate
  ("Kill All");
- the scene ends into the next scene through the native transition.

Everything comes from translated retail functions, called in the retail order
from `frame_tick` and driven by retail state. Read `AGENTS.md` first; every rule
there applies. Host code that still shares the frame is listed in
`docs/LEGACY_HOST_CODE.md`. The previous chronological log (the first
playable-scene push, Oct 8–15) is in git history before this rewrite; don't
copy it forward.

## Retail frame order

`frame_tick` (`src/engine/game.cpp`) is `0x00033B50`:

1. `0x0011E4C0` (`script_pump`, `scene/scene.cpp`). In run state 1 or 2 it calls
   the scene tick `0x0011E7F0` (`scene_tick_pause_counter`): the cutscene stack
   `0x0004EBD0`, the dialogue `0x00057E60`, the fired script `0x00117140`
   (twice), the control list `DAT_0050E438`, **the actor update
   `0x00123020` over `DAT_00511B18`, then `0x00122FE0`**
   (`SceneActorServices::update_scene_actors`), then the script timers
   `0x00117100`.
2. `0x0011E640` (`run_scene_pass`): activation of new actors (`0x00123070`,
   `0x001231A0`, the one-shot first update `0x00123270`), effects, sounds and
   disposal, including the retired-actor removal `0x001232D0`.
3. Present `0x00033A90` → per camera slot `0x00033A20` (`present_views`) →
   `0x0011C240` per view: the actor bins `0x00121730` / `0x00121880` /
   `0x00121900` (passes 0/1) / `0x00121D40` (pass 2), effects, dialogue
   `0x00058150` and the text pools (`docs/NATIVE_SCENE_PRESENT.md`).

Each actor's update is its vtable +0x10. Players (factory type 5: Cooper class
4, the imps class 23, cameron) run `0x0002C2B0`, which ticks the mind (`0x34`
player, `0x35` cinema, `0x36` camera), the strategy, and the body (`0x27` imp,
`0x2A` Cooper, `0x28` camera). The body ticks its action children (body +0x744,
then +0x314) through their vtable +0x10, and walks its attack shapes against
other actors' segment probes (`0x0013F2C0` → `0x0013F7B0` → `0x0013FB50`).

## Current state (2026-10-18)

Verified against the source and live runs (`--player-opening`,
`--player-fight`, headless captures below).

Live and native:

- Scene start, markers, activation and teardown (`--scene-spawn`,
  `--scene-start`).
- **Cooper's scared lock.** Script type 0x2F sends Cooper goal 7 (message 0x4D): his strategy holds in its goal
  state 0x3FB and his body plays the +0x24 action (the scared pose) until type 0x30 (message 0x32) releases him
  after the tutorial.
- The tutorial: the native dialogue holds the script until a fresh A edge, its
  button bat hops, and the script then waits at its "(next bit)" type-0xF gate.
- **Pause behind pages.** Opening a page runs `0x00117400` over DAT_00511B38: the imps get 0x27 and their
  strategies wait in state 1000 until 0x26. DAT_0050E764 / DAT_0050E768 have one owner (`g_pause_references`).
- **HUD behind pages.** The scoreboard's DAT_004EE1EC is the scene dialogue's mode, so the HUD draws nothing
  while a page shows.
- **The ghoul intro.** An imp's first sighting of Cooper (`0x000F4870`) queues it; the scene tick shows it
  (`scene/ghoul_intro.cpp`: `0x000F4A00`, `0x000F4560`, `0x000F4220`, `0x000F4B90`): the imp's goal 9
  ("ghouly_intro"), four camera shots on the imp, the "Imp" name card (aid_loctext_ghoulies_actor_ghoulyintro),
  the GHOULINTRODITTY stream, scene flag 4 (HUD hidden) and the pause. The impcomeincam cutscene belongs to the
  later "Event Door Intro" section (script type 0x13), not to the opening.
- **The chair's section.** After the butler's page, type 0x96 puts Cooper on marker 1 and type 0x62 resets the
  play camera to marker 1 (`0x0013D250`). "Freeplay (Temporary Weapon Explained)" then waits for the chair's
  pickup (ObjectPickedUp, event 0xAD with the chair's tag), for Cooper to leave region 1 (`0x001329A0` over
  `0x00124860`), or for its 5 s TimeOut, which the retail script arms (DAT_0030F144[0] = 1).
- The play camera (cameron's director `0x000AA250`), the actor draw passes and
  the background `0x00042C10`. Cooper and the five imps are drawn.
- Actor updates run every frame (no hold). The imps hunt Cooper: their mind requests `0x2F` / `0x2E` navigate
  toward him by writing their own input axes (`0x00136620`), their walk state `0x8E` reads them back through
  actor +0x960, and request 2 (`0x0007B090`) makes them punch and kick. Cooper walks under the left stick
  (state `0x8E` walk-alert).
- **Attacks.** The right stick makes Cooper's mind request an attack: the body
  selects factory type `0x73` `entityStateHit` (`entities/body_hit_state.cpp`),
  whose tick lunges toward the mind's target, turns toward it, plays the swing
  and picks the next action through the body's vtable +0x20/+0x2C.
- **Contacts and damage.** Cooper's attack shapes reach the imps' segment
  probes. The contact callback `0x0001F8C0` sends the imp message 0x15 and
  reports 0x41 back to Cooper's hit state. The imp's body takes the hit through
  `0x00020020` into the damage processor `0x00020DC0`: the reaction hit
  (`0x00021220` case 1/2) applies the damage, pushes the imp
  (`0x00022400`), and sends Cooper's body the push-back message 0x60.
- **The imp's hit reaction.** The reaction (`0x00024970`) picks a direction
  animation from body +0x268's rows (`hitfromFL`, ...) and enters its action
  row 0x7E (`actorstate_imp_hit`, factory type `0x74`). That state's tick
  (`0x00096890`) waits for the one-shot animation, then picks the next action
  (`0x00133F50`): at 1 health the imp goes to row 0x33
  (`actorstate_imp_dizzy`, factory type `0x64`).
- **Script tallies.** The hit message 0x6E reaches the script filter
  `0x00116950`, which tallies it (`0x00116C10` → `0x0012ACF0` →
  `0x0012AF80`, `scripts/script_event_tallies.cpp`) and drops it.
- **Hit effects.** Type-2 effect particles draw as world sprite quads
  (`0x000F0010` → `0x0005CF90`).
- **Dizzy.** The dizzy state (type `0x64`) circles three star objects over the
  imp for 10 s (body +0x23C), plays the dizzy cue and hands back to the stand
  row.
- **Targets and pursuit.** Each actor's +0x8F8 target record follows the actor
  (the body apply's `0x0012B160`), so Cooper keeps finding imps and chases them
  around the hall under the sticks.
- **The scoreboard** is set up at scene start and torn down at scene end; the
  damage digits over a hit imp draw.
- **The alive count** (`0x00132680`) walks the native `DAT_00511B48` list
  (player `links[2]`, actor +0x944) and reads each body's +0x94.

- **Knockdown and death.** At 0 health the death row builds factory type
  `0x7A` (`entities/body_knockdown_state.cpp`): the imp is launched, sweeps
  for contacts, bounces and lands, and the death finish (`0x00134AE0`) spawns
  its death effect. The alive count drops (5 -> 4) by fight frame ~540, when
  the imp's health is 0; the knockdown plays until ~720, when the death finish
  creates the effect.
- **The death effect** is factory type `0xF`, entityAvatarFxMotionBlur
  (`entities/avatar_motion_blur.cpp`): a copy of the dying imp's pose and model
  (the model clone `0x000F87D0`) that runs the effect stream at its +0x950
  name, takes a 64x64 snapshot of the imp (`0x00116530`,
  `graphics/actor_snapshots.cpp`, rendered into a created texture), then
  burns: the +0x9D0 effect stream and the ONFIRE / BURNDITTY cues in sound
  slots 4 and 5, fading through four timed phases.

- **The glow.** The copy's draw hooks capture it into book page 0 with its depth, stamp that depth into the
  frame's stencil through the DOT_ZW depth-replace shader, and composite the two fading colours where the
  stencil passes.
- **Removal.** Dead imps are removed (`0x001232D0`). Broadcasts (`0x00122C70`) reach every actor still on
  DAT_00511AA4: the 0x31 death notice, the 0x85 anchor release (which clears a mind's +0x30 gaze anchor) and
  the 0x8D record release are delivered, and an actor leaves the list where `0x0003EBA0` unlinks its +0x900
  node.
- **Scenery.** A flying imp or Cooper's hits break background parts (pottery), which award, rumble and remove
  the part.
- **The later fight.** Imps die through entityStateDie (type `0x62`) and pose through type `0x5E`; a dizzy
  Cooper's stick input speeds the stars (`0x0008CC40`'s input branch).
- **"Kill All".** All five imps die and are removed by fight frame ~3260. The gate record (opcode 1, at most 0
  alive, 1.5 s) counts down in `0x001330E0` once the count holds, and `0x00132680` fires; the butler's
  "Well done, Sir!" page takes A, the script requests the ghoulybox through its trigger (message 0xA8) and moves
  to "Event Door Intro" (fight frame ~5660). The next section runs: Cooper stands in the main hall facing the
  stairs, stable for 3600 more frames.

Not live yet: the 64x64 actor snapshot records its draws but rasterizes nothing (its texture stays empty).
See the frontier.

## Controls

The pad path is native from the host packet inward: `platform.cpp` (stands in for `0x001555B0`) → `pad_process`
(`0x000FA3C0`, deadzone 9800, limit 32700) → the device readers in `dialogue_input.cpp` → the player mind,
strategy and camera. The `0x00154xxx` layer in `src/engine/input/controller.cpp` (sticks ×1/74 and ×1/59,
button repeat, rumble) has no caller; its remap hooks `0x00155360` / `0x001553B0` stay null.

`--player-controls` checks the rows marked *tested*.

| Input | Native reader | What it does | Status |
|---|---|---|---|
| Left stick | `0x001085F0` → `0x000A5A60` (walk steer), `0x00140AE0` (camera-relative turn), `0x000784B0` (camera swing) | Walk/run; speed is body +0x32C × the class value `0x000259C0` (actor +0xCF4 class) × surface scale × stick | Works, *tested*: 45 units/s at full tilt, the same at 60, 144 and 240 Hz |
| Right stick | `0x001088D0` → `0x0007D460` / `0x0007D5A0` (gesture) → `0x0007E880` | A flick attacks: the nearest target in the cone, else the mind's own +0xB8 record 80 units along the stick (`0x0007EA20`) | Works, *tested* with no imp in range; also turns the head (`0x00137140`) without a gaze anchor |
| A (0x100) | `0x0007D210` | Picks up the pending attachment (mind +0x134) | Works, *tested* by `--player-fight`: A beside the cook's chair enters the pick-up state 0x7C, which attaches the chair and queues event 0xAD |
| A / B | `0x000BEF40` state 0x3F1 | Accept or refuse the brew prompt | Translated; not reached in the opening |
| B (0x200) | `0x0007D300` | Drops/throws the held attachment | Translated; not reached in the opening |
| A/B/X/Y (0xF00) | `0x000BF6A0` | Any face-button edge counts as "wants to move" for the strategy | Translated |
| Left / right trigger | `0x0013DC40` (camera turn), `0x000AADE0` (both > 0.5: director heading) | Turn the follow camera; both together steer the director | Works, *tested*: 60 frames of one trigger turn the camera ~2 rad, the two triggers in opposite directions |
| Start / Back | frontend (pause) | | Host menu booleans only in the player scene |
| Black / White, stick clicks | no player-path reader found | | |

The class at actor +0xCF4 is written by the player constructor (`0x00028830`) into the actor bytes, and
`Actor::damage_class` is its native alias. Before 2026-10-16 the alias stayed 0, so every class lookup took
the class-0 maximum (Cooper ran at 90 units/s).

## Conventions

- **Address arenas.** Retail addresses held in native state resolve by range:
  resources from `0x40000000` (`resources.memory`), host heap blocks from
  `0x30000000` (`heap_address`, stable per pointer), scene objects from
  `0x10000000` (`scene.resolve_address`), and the XBE image below that
  (`image.memory`; attackdata rows, objparams, string tables). Body +0x84 holds
  an attackdata row address, so it resolves through the image, not as a scene
  object.
- **Target records.** `DAT_00545988` records are rebuilt each update pass by
  `refresh_mind_targets`: a 0x68-byte retail image per live attachment (+0x10
  the actor address, +0x14.. the record fields) at a heap-arena address. Mind
  +0x10 and +0x14 hold these addresses (`target_views`).
- **Messages.** Actor and body messages are 0x42-word records. A sender whose
  retail message is a short stack slot passes the retail stack words that
  follow it and zero-fills the rest.
- **Host structs on the message path** (`RuntimeContactNotice`,
  `RuntimeContactReport`) are converted to retail word layouts at the actor
  boundary; contact pools are bound to scene addresses while the message runs.
- **Vtables** of these classes are BSS filled by getters. When Ghidra has no
  reference, scan the XBE for the immediate (`0x00505940` → getter
  `0x00095740`) and export the range with `range-function-force:`.

## Test entry points

All take the game directory, e.g.
`build\Release\gbtg-transition-tests.exe OriginalFiles --player-fight`.
`gbtg-transition-tests` is not in the default build target; build it with
`--target gbtg-transition-tests`.

| Mode | What it checks |
|---|---|
| `--scene-spawn` | Opening markers create the retail actors and players; activation drains; contact round trip; teardown releases everything |
| `--scene-start` | The above plus the background, navigation, the 0x88/0x7D notices and the 0x6F player wait |
| `--player-opening` | Cooper's goal-7 lock and the imps' pause behind the tutorial page, no HUD heart behind it, tutorial hold, bat hop, A edges, the ghoul intro (scene flag 4 on and off, `cache/player_intro_check.bmp`), Cooper released, the "(next bit)" gate, Cooper walking (`cache/player_walk_check.bmp`), the imp fight music |
| `--player-fight` | The opening flow, then Cooper fights the imps (see Acceptance), a re-hit during an imp's reaction, the HUD heart in fight captures and not behind the butler's page, the chair's section (the ObjectPickedUp record with the chair's tag, Cooper in region 1, Cooper walks to the chair and A picks it up; event 0xAD with the chair's tag is queued and the section ends before its 5 s timeout; `cache/player_chair_check.bmp`, `player_chair_pickup.bmp`, `player_chair_held.bmp`), then the play-on |
| `--player-controls` | The opening flow, then run speed at 60/144/240 Hz, a right-stick swing with no imp in range (`cache/player_air_swing.bmp`) and the trigger camera turn |

Headless captures: `gbtg --headless --frames N --scene chapter1_scene1_2playcam
--width 640 --height 480 --screenshot FILE.bmp`.

## Known traps

- **Headless runs read the real keyboard.** Keys held on the machine (WASD,
  IJKL, A) reach the pad boundary; keep hands off during captures.
- **Bash heredocs mangle backslash escapes.** Python or shell heredocs turn
  `\n` in C++ string literals into real newlines. Use Write/Edit for literals.
- **Release builds have no debug info** (the `.pdb` beside the Release test is
  stale). For a crash stack, use the Debug or RelWithDebInfo build
  (`--config RelWithDebInfo --target gbtg-transition-tests`) with a temporary
  DbgHelp exception handler (`StackWalk64`) in the test, run with
  `_NT_SYMBOL_PATH` at that build directory, then remove it. A temporary
  catch that swallows an update error can leave the run hung or crashing
  later; don't read a later stop as the real one.
- Several callees of `0x0011E7F0` are still skipped with a comment rather than
  raising (`0x0013FFD0`, `0x000D0300`, `0x00049DE0`, `0x000F4A00`,
  `0x001128A0`, `0x0011B670`, `0x000F4560`, `0x0013FD60`, `0x00115BB0`,
  `0x001162F0`, `0x0011B7F0`, `0x0012AA70`, `0x001185E0`, `0x000FDDA0`,
  `0x000FD5C0`, `0x001083F0`). Check them when a gameplay effect is missing.
- Script conditions 23 and 24 and several script actions still walk the empty
  host `g_alt_links` instead of `DAT_00511B48`.
- **Class and state names can mislead.** The factory type table
  `DAT_0030D420` (getter, name, parent per 0xC-byte row) and the body state
  names `0x003390E4` look one entry off from what the rows do: the imp's
  `actorstate_imp_stand` / `_walk` / `_hit` / `_dizzy` objparams build types
  0x87 / 0x8E / 0x74 / 0x64, named StandAlert / WalkAlert / HitFurnace /
  DroneAppear. Go by the row's objparams and the getter, not the name.
- **Body +0x84 is not the only image record.** Image records resolve through
  `record_at(address, size)`; give the size you read (the body +0x268 direction
  rows are 8 bytes each, not the 0x24 of an attackdata row).
- **`scene.objects` is a `std::vector`.** Creating objects mid-update (stars,
  trails) can move every `FactoryObject`; look objects up by their bytes, never
  keep a `FactoryObject&` across a creation.
- **Native views keyed by `Actor*` must go with the view.** A released view's
  memory is reused by the next one; `actor_updates` now drops its entry when
  `NativeActorFactoryOwner::release_view` runs.
- **Attacks need a flick.** The mind's stick gesture (`0x0007D5A0`) fires when
  the right stick comes back after a push, and targets come from each actor's
  +0x8F8 record, refreshed by the body apply (`0x00025C50` -> `0x0012B160`).
- **A run that looks hung may be GPU-starved.** With another program holding
  the GPU (a game at 100%), the D3D11 maps in `gfx_draw_native` wait and
  `--player-fight` takes ~25 minutes. Check the GPU before hunting a loop; a
  temporary watchdog thread that walks the main thread's stack
  (`SuspendThread` + `StackWalk64`) shows where it waits.
- **Recorded draws outside a view are dropped.** Each view clears the
  recording device's commands before it draws. A draw recorded during the
  update (the actor snapshot) has to be presented when it is recorded; see
  `RuntimeActorSnapshots::begin_capture` / `present_capture`.
- **Register arguments.** `0x00020C80`'s last fallback passes the bare direction
  name in EBX, not the printed buffer. Check the instructions, not only the
  decompile, when a callee takes its name or output in a register.

## Frontier

Measured with `--player-fight` (Release and Debug, identical). The fight passes "Kill All"; Cooper picks up the
cook's chair and its section ends through event 0xAD. In the play-on the driver takes the butler's pages, the
second wave (ten imps, `aid_ghoulybox_ghoulies_chapter1_scene1_4playcamimps`: count 10, one every 0.3 s) comes
through the doorway, Cooper fights it down with the chair (its after-images and hit sparks show), the wave's
"Kill All" passes and the run stops at the butler's type 59.

| Path | Stop | What it needs |
|---|---|---|
| Player death (not reached) | `scene factory type 105 getter 0x0008EED0` (seen while the chair's swings missed) | Body state 0x69 (vtable `0x00505800`: sizes `0x0008EEB0`, constructor `0x0008EF30`, destructor `0x0008F1A0`, tick `0x0008F290`, `0x0008F4D0`, `0x0008F650`): Cooper's death and the retry menu (`0x0012C4E0`, `0x0012C2D0`, `0x000496C0`, the "gameselect" loading) |
| Butler strategy | `scene factory type 59 getter 0x000AB040 is not translated/installed` | The strategy class 0x3B over the base strategy 8 (size 0x98; vtable 0x00505D70: sizes `0x000AB020`, constructor `0x000AB090`, message `0x000AB170` with `0x000AB2D0`, update `0x000AB0C0` with `0x000AB260`, `+0x14` `0x000AB1A0`, `+0x18` `0x000AB220`). Callees still missing: `0x00128DF0`, `0x0002A030`, `0x00139AA0`; the rest (`0x0002C130`, `0x0002BC10`, `0x0002BAF0`, `0x0002B8D0`, `0x0002A8E0`, `0x0013D400`) are native. Install it like the imp fire strategy (type 0x46) |
| Actor id lookups | legacy | `find_actor_by_id` / `find_actor_by_group` over the unfilled `g_pulse_nodes` still serve script actions, conditions and the ghoulybox controller; see `docs/LEGACY_HOST_CODE.md` |
| Present (the snapshot) | renders nothing | `0x00116530` records its draws but the texture stays empty |
| HUD draws (not reached) | `0x0012F110` | Scoreboard rows of kinds 4 and 5 (the formatted counter) |

## Work

1. The butler's strategy (type 0x3B), then "Exit Door Opens", "Free Play", the butler's cutscene and the scene end.
2. The death state 0x69 (`0x0008EED0`) and its retry menu, for a run where the imps win.
3. Move the remaining `g_pulse_nodes` callers to the native `DAT_00511AA4` lookups.

## Acceptance

- `--player-fight` drives Cooper with the sticks, defeats every imp, requires
  the script to pass "Kill All" and saves readbacks along the way
  (`cache/player_fight_N.bmp`).
- Release and Debug build; `ctest`; the six transition suites (`--scene-start
  --scene-spawn --dialogue-resources --actor-loading --ghoulybox-decoder
  --scene-models`), `--player-opening`, `--player-fight`, `--title`,
  `--title-a`, `--attract`, `--goop`, `--ui`, `--audio-preparation`. (Debug
  `--actor-loading` used to fail at `check_boot_pixel_shaders`; it passes since the 2026-10-16 combiner fix.)
- `gbtg --headless --frames 900` exits 0, with and without `--scene
  chapter1_scene1_2playcam`.
- Screenshots at the reference frames match or are better.
- `git diff --check` is clean; no temporary probes remain.

## Reference captures (2026-10-16, before the combat work)

`cache/player_ref/`:

| Capture | Screen |
|---|---|
| `opening_180.bmp` | The hall from behind Cooper; three imps on the floor, the staircase and portraits |
| `opening_300.bmp` | Same view a little later: the imps wander, and the HUD heart with its counter (50) shows at the top right (re-taken 2026-10-17 after the HUD update and the script's queue limits; the morning's take is in `before_hud_2026-10-17/`) |
| `opening_900.bmp` | The native tutorial page: Jeeves' head, "I suggest Sir uses the Left Thumbstick to move and HOLDS the Right Thumbstick…", the corner imps and the A-button bat |
| `player_opening_check.bmp` | `--player-opening` at frame 720: the tutorial page (re-taken 2026-10-17; a 52x32 patch near the top changed) |
| `player_intro_check.bmp` | `--player-opening`, 90 frames into the ghoul intro: the intro camera behind an imp and its "Imp" name card (new 2026-10-18, no reference yet) |
| `player_chair_check.bmp` | `--player-fight`, 40 frames into the chair's section: Cooper beside the cook's chair and its hand prompt, the camera reset behind him (new 2026-10-18, no reference yet) |
| `player_chair_pickup.bmp` | `--player-fight`, the frame the chair attaches (0x0001B190): Cooper beside the chair, which still lies on the floor (new 2026-10-18, no reference yet) |
| `player_chair_held.bmp` | `--player-fight`, 100 frames after the pickup: Cooper holding the chair upside down over his left shoulder (2026-10-18, matches the retail idle hold) |
| `player_walk_check.bmp` | `--player-opening` after 120 frames of left stick: Cooper walking toward the stairs with an imp behind him (re-taken 2026-10-17 after the door pass, then after the HUD update; earlier takes are in `before_doors_2026-10-17/` and `before_hud_2026-10-17/`) |

`--player-fight` before this work stopped at frame ~60 of the fight with
"scene factory type 115 is not translated/installed".

## Checkpoints

### 2026-10-18: the scared lock and the ghoul intro (fix 1)

- Script types 0x2F and 0x30 were empty stubs. 0x2F now sends the DAT_00511B40 player 0x00049DB0's message 0x4D
  with goal 7, and 0x30 sends message 0x32. Cooper holds in goal state 0x3FB and plays his body +0x24 action (the
  scared pose, base `0x000269C0`, now bound with the other base slots +0x44..+0x58) until the tutorial ends.
- Type 0x5C sends message 0x36 to the imps through `0x00122D40` over DAT_00511B38.
- The "enemy intro" is the ghoul intro, not a cutscene resource: new `scene/ghoul_intro.cpp` with `0x000F4440`
  (introdata rows, scene start), `0x000F44B0` (scene end), `0x000F4560` / `0x000F4A00` (scene tick; the stale
  `g_506e28` copy is gone), `0x000F4220` (the camera), `0x000F4B90`, `0x000F4C10`, `0x000F4CC0`, `0x000F4D40`,
  `0x000F4DA0`, `0x000F4DF0` and `0x00124860`. The notice removal's stop, next and finish are bound.
- 0x00028360 read `Actor::mask_span`, which nothing sets, so it always saw no goal: the 0x26 resume never drained
  the intro imp's goal 9 and its damage latches (body +0x260, raised for goal 9) stayed up, so it could not be
  damaged. It now reads the player's goal ring.
- The player's body vtable +0x20 view for the attachment re-selection (`0x000277E0`) was never attached.
- Shown: `cache/round1/shots/opening_300.bmp` has Cooper in the scared pose (arms raised, facing the camera) where
  the reference stood him normally; `player_intro_check.bmp` shows the intro camera on an imp with the "Imp" card.
  Heard: the GHOULINTRODITTY cue starts (log); not listened to.
- Verified (covers the four 2026-10-18 fixes): Release and Debug build; ctest 7/7 in both; the six transition
  suites, `--player-opening`, `--title`, `--title-a`, `--attract`, `--goop`, `--ui` and `--audio-preparation` pass
  in both (Debug `--actor-loading` included); `--player-fight` passes every check in both and stops at the
  expected type 59; headless 900 (both scenes) exits 0 in both. Against `cache/player_ref`: 180, 900 and
  `player_opening_check` are pixel-identical; 300 differs (Cooper in the scared pose, the imps elsewhere);
  `player_walk_check` is shifted slightly (the walk now starts after the ghoul intro). The references were not
  re-taken. Nothing here was compared with xemu.

### 2026-10-18: pause and HUD behind pages (fix 2)

- `0x00117400` walked the empty host `g_range_links`; it now walks DAT_00511B38 natively (`g_actor_pause`, the
  services' `pause_groups`, which the strategy's own calls also use and which were never filled). The imps get
  0x27 behind the tutorial and wait in strategy state 1000.
- One owner each for DAT_0050E764/DAT_0050E768 (`g_pause_references`), DAT_0051248C (the services' decay index is
  the script's `g_state_248c`), DAT_00510CEC and DAT_004DEA24 (the transitions' globals). The actor frame update's
  copies of DAT_0050E768 and the decay byte are refreshed each frame.
- The scoreboard's DAT_004EE1EC was a services copy stuck at 0; it is now the scene dialogue's mode.
- Shown: the HUD heart is absent behind the butler's page (`--player-fight` checks a page capture) and present
  in fight captures. The tutorial page capture had no heart before either.

### 2026-10-18: the chair's section (fix 3)

- Condition setup 0x40 / 0x94 / 0x95 / 0x99 now stores 0x001077B0's tag at record +8 (it stored 0); opcode 3's
  update `0x00133170` is translated; conditions 8 and 9 (`0x00132940`, `0x001329A0`) read the native player link
  instead of the unset `g_actor_mark`, which made PlayerLeavesRegion fire at once and skip the section.
- Script types 0x96 (`0x0002B430` / `0x0002B560` on the player, marker 1) and 0x62 (`0x0013D250`, the play camera
  to marker 1) replace empty stubs; Cooper now starts the section beside the chair, inside region 1.
- The section waits for the pickup, region 1 or its 5 s TimeOut. The TimeOut is armed in retail (mode 0,
  DAT_0030F144 = {1, 0, 1, 0}); nothing in the dumps holds DAT_0050E768 there, so the scene does move on after
  5 s without the chair. Not compared with xemu.
- The pickup: weapons now have an attached-actor view (mind +0x134 names the chair), the +0x918 link is read only
  when retail reads it, and body vtable +0x5C is the base `0x00026C60`. Walking Cooper to the chair and
  pressing A (an exploratory run) reaches the pick-up body state, factory type 0x7C (getter `0x0009DDA0`), which is
  not translated; its exit `0x0009E560` is what queues event 0xAD for the script. `--player-fight` walks Cooper to
  the chair without pressing A, and its driver now gives A only on script pages.
- Shown: `player_chair_check.bmp`, Cooper beside the chair with its hand prompt, camera reset behind him.

### 2026-10-18: the chair hold against retail

- Retail (a xemu screenshot from the user): in the two-handed idle the chair lies upside down over Cooper's left
  shoulder, held up by his left hand, while his right hand is at his chin.
- Fixed: the held weapon's animation (`0x00144B80`, message 0x28) never ran. It tested row +8 in the actor bytes, but
  native keeps the row's AID in `accepted_resources`, so the test was always false. It also lost the holder's record
  at message +8. Now the 0x28 receiver binds the holder's slot-0 record as a word for the message, and `0x00144B80`
  follows the dump: the row's animtable, the holder record's name through the aliases (`0x00032210`) and records
  (`0x00032180`), "default" when neither is found, the mix from the holder record (`0x000322A0`) with the weapon
  record's events, message +0x10 position and the old-pose rule, then `0x0003ADA0` with message +0xC.
- Checked on the way, no change: the chair binds to joint 21 (key 12, Cooper's right wrist, at his chin in the
  idle), as retail's bind code and the chair's three class rows (classes 4, 14, 52, all key 12) give. Its transform is
  exactly that joint's matrix. The chair's own animation is what lays it across to the left shoulder.
- Shown: `player_chair_held.bmp`, now 100 frames after the pickup: the chair upside down over the left shoulder,
  the left hand under it, as in the retail screenshot.
- Not compared: the hold while running or swinging (only the standing idle has a retail reference).

### 2026-10-18: logic at 60 Hz

- The interactive loop (`runtime/platform.cpp`) now waits for each 1/60 s slot and ticks with exactly 1/60 s,
  whatever the display refreshes at, as retail does (`0x00033030`: 60 Hz, 50 for PAL, `dt = 1/fps`). It used the
  real elapsed time, so a 240 Hz display ran the logic at 1/240 s steps. Headless and screenshot runs keep their
  fixed 1/60 s without waiting. Measured: 600 windowed frames take 11.5 s (10 s plus start-up).
- An imp was seen staying behind the wave's door in a 240 Hz play. `--player-fight` driven at four 1/240 s steps a
  frame did not show it: all ten imps came into the hall after "Kill All", as at 60 Hz. The cause is not known.
  In both runs the imps hold still behind the door from about frame 420 (the door shuts near frame 400) until
  "Kill All". With the 60 Hz loop the user no longer sees it in play.
- Verified (with the after-image fixes): Release and Debug build; ctest 7/7; every suite passes except
  `--player-fight`, which ends at the expected type 59 in both; headless 900 (both scenes) exits 0.

### 2026-10-18: the swing's after-images and the combiner overrides

- Fixed (presenter): a model draw's raw NV2A combiner writes (the model shader overrides `0x000F84A0` / `0x000F7420`
  and the stage colours `0x000F7FA0`) were recorded but never reached the host shader, which used only the bound
  definition and SetPixelShaderConstant. The registers behind definition words 0..56 (the table at `0x0020A368`)
  now lie over the bound definition's words until the next SetPixelShader, after the constants. The model path's
  SetPixelShader now goes through the device's hook, so a new shader drops them.
- Shown: the swing's after-images are translucent copies trailing Cooper (they drew as solid Coopers before, and as
  nothing when the stage colour was lost). Any model with a +0x1C0 colour override now draws its override stage.
- Checked, no change: holding the chair. Cooper plays the two-handed rows (`stand_wielding_largetwohanded_weapon`,
  `aid_anim_ghoulies_actor_boy_largetwohanded_stand`, the `..._attack_swing*` rows, all slot 0), and the chair's
  bind (joint key 12 -> joint 0x15 through `0x00052FC0`, `0x0003B840`, `0x0003B770`, `0x0003BAB0`, `0x000316B0`)
  follows the dumps. The chair's own +0xDB0 animation variant is empty (its parameter +0x33C is 0), and its holder
  row has no animation table, so 0x28 plays nothing on it. Not compared with xemu.

### 2026-10-18: swinging the cook's chair

- New native type 0x10, the after-image (`entities/avatar_motion_blur.cpp`, getter `0x000690A0`, vtable `0x00504A54`
  over the avatar actor): sizes `0x00069080`, constructor `0x000690F0`, destructor `0x00069220`, update
  `0x00069250`, message `0x00069540`, draw `0x00069410`. The swing trail (`0x0013B030`, from the hit state) leaves a
  copy of the holder's look that fades out over its life (alpha in the model's +0x1C4 word) and is drawn in pass 2.
  Its effect (+0x95C), resource (+0x960) and glow (+0x964) paths raise; the trail leaves them unset.
- Fixed: `0x0013B030` wrote its parameters 4 bytes too low (it pushes the record address before the stores), so the
  copy would have read the life as the source actor.
- New: weapon contact message 0x1D (`0x00141890` -> `0x000240C0`) to a body now has its views: the weapon (its
  holder at +0xD8C), the holder's body +0x84 and damage entry, and the copied contact pool. It was an unbound.
- `--player-fight`: Cooper fights the second wave down with the chair; the wave's "Kill All" passes and the run
  stops at the butler's type 59 again. Shown in the play-on readbacks (`player_after_1200` .. `_1800`): the fading
  copies along a swing, the chair over Cooper's head, a hit spark as it lands, the hearts dropping. Not compared with
  xemu; the copies' alpha and colour word were not checked against the retail look.
- The trail's unit check in `--actor-loading` now expects the retail offsets.
- Verified: Release and Debug (fresh exes): ctest 7/7; every suite passes except `--player-fight`, which ends at the
  expected type 59 in both; headless 900 (both scenes) exits 0.

### 2026-10-18: picking up the cook's chair

- New native type 0x7C, the pick-up body state (`entities/body_pickup_state.cpp`, getter `0x0009DDA0`, vtable `0x00505A60`):
  sizes `0x0009DD80`, constructor `0x0009DE00`, destructor `0x0009DEA0`, tick `0x0009DF90` and its approach `0x0009E2B0`.
  The tick walks the body toward the chair's nearest pick-up sphere and, once the animation passes the chair's grab
  point (its class row +0xC, 0.2 for Cooper), attaches it through the new `0x0001B190` (with `0x0003B460` /
  `0x0003B4C0`). The destructor broadcasts {0x74, actor, weapon}, drops weapon +0xAE4 and, for a controlled player,
  queues {0xAD, weapon tag}. (`0x0009E560` is the same exit in another class; type 0x7C's vtable has `0x0009DEA0`.)
- The chair's message 0x28 (`0x00144B80`) returns for a row without an animation table, as the chair's is; the table
  path raises. Scene weapons now have pick-up views (actor, animation receiver, pose) listed in `attachment_actors`.
- Fixed: `0x00146150` read actor +0x918's node instead of the parent's, so every activated actor (all sit under the
  world bind, DAT_00511B10) looked bound; it now tests the parent node's head and owner. Its unit test now binds
  through a parent node.
- Fixed: the queued script event's +0xC / +0x10 / +0x14 (message +4 / +8 / +0xC) were never filled by `0x00116810`,
  so tag-keyed conditions such as ObjectPickedUp (opcode 3) never matched a tag.
- Fixed: the pick-up context reads the native mind (`mind_present`); actor +0x958 is not kept in the bytes.
- Fixed: the player camera's offset table (`0x0013D000`, DAT_00314EB0) was a 0x30-byte view; armed rows reach 0x80.
- Fixed: the body's animation command 0xB (`0x0001A290`) had no body +0x48 views, so a held weapon's +0x998 shapes
  never activated; the hit state now gets the weapon's +0x998 state too.
- `--player-fight`: Cooper walks to the chair and A picks it up (frame 151 of the section); event 0xAD with the
  chair's tag is queued at frame 190 and the section moves on to "Event Door Intro" at frame 281, before the TimeOut
  (frame 299). The test checks the pickup, the queued event and the early end.
- The play-on then had Cooper holding the chair: his first swing reached the weapon trail's type 0x10
  (`0x000690A0`), so the run stopped there (see "swinging the cook's chair"). Before the shapes fix, the swing never hit and
  the wave killed him (type 0x69, the death state). Not compared with xemu.
- Shown: `player_chair_pickup.bmp` (the attach frame, the chair still on the floor beside Cooper) and
  `player_chair_held.bmp` (Cooper holding the chair on his shoulder). Not shown: the chair in a swing, or any chair hit
  on an imp.
- Verified: Release and Debug (fresh exes): ctest 7/7; every suite passes except `--player-fight`, which passes the
  chair's section and stops at type 16 in the play-on in both; headless 900 (both scenes) exits 0. The Ghidra exports
  came from a temporary local import of `default.xbe` (the Z: project was offline); spot checks matched its dumps.

### 2026-10-18: hitting a reacting imp (fix 4)

- Measured with temporary probes (removed): re-hits during the imp's reaction (action 0x7E) land, and its health
  drops 5 -> 1 within one reaction; Cooper's body +0x6C opens on each hit and his swings chain. At health 1 the
  reaction hit keeps 1 (`0x000215C0` with `0x00024680`: the imp's table has no death rows 0x68..0x6A), so the imp
  only dies once dizzy. That matches the dump; no code changed for this item. The "invincible" imp the user saw
  was the intro imp (fix 1).
- `--player-fight` now requires a health drop while an imp stays in action 0x7E.

### 2026-10-18: the actor +0xCDC / +0x93C aliases

- The damage readers (`0x00020DC0`, `0x00021120`, `0x00021EA0`, `0x00022010`, `0x00022310`, `0x000238E0`..
  `0x00023F70`, `0x0001FF60`, `0x00028D00`/`20`/`50`, `0x001460C0`, `0x00146150`) match their dumps. Their
  `Actor::mask_mode` / `mask_flag` aliases are written beside the words: `0x00028830` (+0xCDC) and `0x00029270` /
  `0x00028A30` (+0x93C) for players. A temporary probe in `0x00020DC0` (removed) read Cooper as mode 2 with +0x93C
  0x00511B3C on all 24 of his hits in `--player-fight`, and every imp as mode 1. So Cooper was already treated as a
  controlled player; his hit reactions do not change.
- Two weapon gaps are closed: `0x00145C80` now mirrors the +0x93D / +0x93F bytes into the +0x93C alias (the alias kept
  only +0x93C, so `0x001460C0` always read state 0 and never reported an unavailable spawn owner), and `0x001495C0`
  mirrors its +0xCDC = 1. `0x00145C80` runs for the chair; `--player-fight` still passes "Kill All" and ends at
  type 59 as before, with no visible change.
- `mask_span` (+0x980) is still never written; the camera mask (`EntityMaskHost`) reads it. See
  `docs/LEGACY_HOST_CODE.md`.

### 2026-10-18: the second wave comes through the door

- The play-on driver (`--player-fight`) gives fresh A edges while a script page shows and logs the wave (imps out of
  reach, the spawn limits `DAT_0050E568` / `DAT_0050E56C`); it plays up to 14400 frames or until the script changes.
- Fixed: the actor startup (0x0003E940) kept parameters `+0x2AA` / `+0x2AC` only in host fields; the instance bytes
  now carry them at actor `+0x1FC` / `+0x1FE`, which triggers, strategies and weapons read.
- New native 0x00129ED0 (`find_runtime_actor_by_id`) over `DAT_00511AA4`; the trigger's 0x7D anchor uses it. Before,
  the wave's trigger had no anchor, so every imp was told to stand at its spawn point behind the door.
- Fixed: the scene class owner's `bytes_at` asked for an empty span (`resolve_address(address, 0)`); it now returns
  the instance from the address on, so the trigger's anchor reads and the weapon's group bind work.
- Fixed: the scene's per-player contact views were never dropped, so an imp built where a released one had been
  reused its dangling actor; the contact report then failed on an unknown actor. This was also the run-to-run
  variation (it depended on heap reuse; cdb's debug heap hid it, `-hd` showed it).
- The script filter's 0x80 / 0x83 arms (0x00116950) with 0x001168C0, which drops the queued 0x7D / 0x81 events.
- Shown: the wave in the hall, Cooper's kicks and hit sparks, the health hearts dropped, the HUD at 40, the cook's
  chair standing in the hall, then the camera turning to the exit door. Not shown yet: the butler (type 0x3B).
- Verified: Release and Debug (fresh exes): ctest 7/7; every suite passes except `--player-fight`, which stops at the
  expected type 59; headless 900 (both scenes) exits 0; the 180 / 300 / 900 captures and both check images match
  `cache/player_ref`.

### 2026-10-17: the chair's update and draw, and the second wave

- The weapon update `0x0014A2E0` and its callees in `entities/weapon_actor.cpp`: the per-state updates
  (`0x0014A220`, the chair's `0x000CF050`), `0x00145A40`, `0x00144E30`, `0x00148510`, `0x00145740` (the falling
  pieces `0x0003FB80` with the sphere probe `0x001225A0`, `0x00040040`, `0x00145820`), `0x0014A6D0`, the hand
  prompt `0x00147160`, the cell update `0x00145C30` and the carried sound `0x0014A770`; the draw `0x001459C0`.
- New `entities/weapon_motion.cpp` (states 2, 5, 8 and the enter/leave of state 10) and
  `entities/weapon_breakage.cpp` (the stat tables `0x001486C0`, the break `0x001454F0` and its pieces, the variant
  `0x00148430`, the scare `0x0013B660`). The chair's messages `0x5A`, `0x91`, `0x92`, `0x97`, its contact test
  `0x000CF5F0` with the report `0x00141890`, the chip `0x001451C0`, the drop `0x000CF670` and the prewait
  `0x00144CC0`.
- Fixed in the trigger's spawn request (`0x00069960`, message `0xA7`): retail passes the message's `+0x184` name in
  EDX (to parameters `+0x53A`) and `+0x104` in ECX (to `+0x5BA`); the translation had them swapped, so the second
  wave's imps read their actorattribs name as a ghoulybox.
- Fixed in the trigger controller: the scheduler's `0xA7` request is `0x82` words and its `0xA9` / `0xAA` requests
  `0x22`; the controller wrapped every message in `0x42` words, which Debug caught as a span assert and Release
  miscompiled into an unbound address later in the play-on.
- Not shown yet: the chair was not seen in the readbacks checked; the second wave is not fought (see Frontier).
- Verified: Release and Debug (fresh exes, 22:57 and 23:01): ctest 7/7; every suite passes, `--player-fight` through "Kill
  All" and the 3600-frame play-on; headless 900 (both scenes) exits 0; the 180 / 300 / 900 captures and both check
  images match `cache/player_ref`.

### 2026-10-17: the weapon class family and the cook's chair

- New `entities/weapon_actor.cpp`: the weapon base (type `0x0B`, getter `0x00145900`, vtable `0x005461A8`) and the
  cook's chair (type `0xAA`, getter `0x000CEF70`, vtable `0x00506438`), installed by the scene actor class owner.
  Sizes `0x001458E0` / `0x000CEF50`; constructors `0x00145C80` / `0x000CEFE0` with `0x00144A80` (accepted classes),
  `0x0013F150` (marker rows), `0x00145150` (names), `0x00146D10` (grab shapes and spawn points), `0x00147E10`,
  `0x00147F00`, `0x00147860` and `0x00146230`; the destructor `0x00145E80` with `0x0014A0D0`; the state slots
  `0x0014A120` / `0x0014A180` / `0x000CF400` / `0x000CF510` and the state change `0x0014A290`; state 16's
  `0x00145BC0`, `0x00144D00`, `0x00144D40` and `0x00146310`; the messages `0x001463D0` and `0x000CF0D0` (ids `0x14`,
  `0x20`, `0x23`, `0x7C`, `0x7D`, `0x93` translated, with `0x00144FD0`, `0x00148600` and `0x001462B0`; the rest
  raise with their callee).
- New `entities/actor_model_pieces.cpp`: `0x0003F170`, the model's piece rows at actor `+0x2A4`.
- Scene services bind the weapon's objparams pointers (XBE addresses), per-model ndMtxArray bindings for
  `0x000400D0`, the shape poses (refreshed before the constructor reads them), `0x0003DF40` through the scene's
  keep, and the active-list owners for `0x00148600`.
- Weapons join the ghouly controller pool (DAT_00512B74) and its distance map, and their `+0x934` link joins
  DAT_00511B24, which the spawn queries walk.
- Not shown yet: the chair has no update or draw, so nothing of it is visible.
- Verified: Release and Debug (fresh test exes, 18:54): ctest 7/7; every suite passes except `--player-fight`, which stops at the expected `0x0014A2E0` with a clean teardown; headless 900 (both scenes) exits 0; the 180 / 300 / 900 captures and both check images match `cache/player_ref`.

### 2026-10-17: the HUD update and draws, and the live spawn tick

- HUD per-frame update `0x00132210` (`frontend/scoreboard_update.cpp`), called from `frame_tick` after
  `0x0011E640` as retail does: the fade state machine `0x00131C50` / `0x00131FC0`, the timers `0x0012B2F0`,
  the pulse `0x00104DF0` and swell `0x0012EF60`, the row update `0x00130E10` and its callback `0x00131B70`, the
  health display `0x001301A0` with `0x00130A50`, `0x00130830`, `0x00130AC0` and `0x001308A0`, the powerup board
  `0x001323E0`, the bonus board `0x0012D690`, the brew prompt `0x0012D290`, the damage counter `0x0012DA10`, the
  meter `0x0012E070` / `0x0012E900` / `0x0012EE00`. Deeper branches (the brew widget, the challenge row, the
  shown meter) raise.
- HUD draws (`frontend/scoreboard_draw.cpp`, `frontend/scoreboard_hud.cpp`): the sprite queue `0x001221B0`, the
  placement `0x00131F00` with its ease `0x00103850`, the digits `0x0012F8F0` / `0x0012F850` / `0x0012F000` /
  `0x0012F060` / `0x0012F0D0`, the health display `0x00130420` and the scoreboard rows `0x00130F20`. The heart
  and its counter now show at the top right of the hall, and the event icon shows on the scoreboard.
- Live spawns: `0x0011B670` (already translated) now runs in the scene tick after the actors, with the
  scene's spawn views. DAT_0050E568..DAT_0050E758 have one owner: the scene's queue limits are views onto the
  script's scene words.
- Event sequence fixes: the kill step walks the side list and the spawn queue by their real record links.
- Before the spawn tick, the second event ran end to end up to its kill step (captures show the door locking, the
  butler's two pages and the tombstone icon); the step then held on the two unspawned side-list records.
- The 300-frame capture and `player_walk_check` now show the heart and counter, and the imps' queue limits are
  the script's; both references were re-taken (the user approved it), the morning's in
  `cache/player_ref/before_hud_2026-10-17/`.
- Verified: Release and Debug build; ctest 7/7 in both; every suite passes in both except `--player-fight`, which stops at type `0xAA` (the frontier above) in both; headless 900 exits 0 both ways; captures 180 and 900 and `player_opening_check` match the references; the 300-frame capture and `player_walk_check` differ in both (the HUD heart and counter now draw, and with the script's queue limits honoured the imps wander at frame 300 instead of crowding Cooper); `git diff --check` clean.

### 2026-10-17: doors and the event sequence

- Doors (`cutscenes/door_registration.cpp`): `0x0005B540` (move a door), `0x0005B780` (its state), `0x0005B7C0`
  (at its event state), `0x0005BDB0` (snap), `0x0005BF30` (the door camera hold), `0x0005AA90` (the per-frame
  door pass, now in the scene tick after `0x0004EBD0`), `0x0005C020` (regions follow door states). Door rows are
  0x154 bytes; the doors have a real DAT_004EE2BC camera node. The script's type 0x29 node (`0x001259B0`) and
  `0x000532E0` replace the old stubs that held the script forever after "Event Door Intro".
- Scene start: `0x00125B80` (in `0x0011CBD0` before `0x0011C980`) applies the script head's door pairs and queued
  region tags and sends 0x66 down DAT_00511B20. Doors now block their navigation regions while closed.
- Door shadows: DAT_005462A8 is BSS nothing writes; the shadow quad's bottom reads it as 0.
- The event sequence (`scripts/script_event_sequence.cpp`): `0x000D0490`, `0x000D0530`, `0x000D0550`,
  `0x000D05D0`, `0x000D0680`, `0x000D09A0`, `0x000D0B10`, `0x000D12E0`, `0x000D0300` (scene tick, between the
  two fired-script passes), `0x000D1200`, `0x000D1260`, `0x000D0F20`, `0x000D1190`, `0x000D0B40`, `0x000D0700`,
  step type 0 `0x000CFA10`, `0x0012FB80`, `0x0012FDC0`. Script types 0x17, 0x18 and 0x19 use them. DAT_005064D8
  has one owner (`g_event_sequence_state`); the HUD reads the same value. Steps of other types raise.
- `--player-fight` plays on after "Kill All" (fights any live imp, A edges otherwise) until the scene's script
  changes, up to 3600 frames.
- The 300-frame opening capture and `player_opening_check` / `player_walk_check` changed: at frame 300 the imps
  close in on Cooper (their health hearts show) where they used to wander, and the walk check's Cooper stops short
  of the stairs. The likely cause is the scene-start door pass blocking the closed doors' navigation regions as
  retail does. 180 and 900 are unchanged. The three references were re-taken (the user approved it); the old
  ones are in `cache/player_ref/before_doors_2026-10-17/`.
- Verified for the door work (before the event sequence), Release and Debug alike: both build; ctest 7/7;
  every suite passes; headless 900 exits 0 both ways; captures as above in both. The event sequence is built
  (Release) but not verified: with it, `--player-fight` passes "Kill All" and then fails at the row draw raise
  above.

### 2026-10-17: controls, class values and moving imps

- Cooper's run speed: `Actor::damage_class` (the native alias of actor +0xCF4) was never set, so every class
  lookup (`0x000259C0`, `0x00133BD0`, `0x00028DA0`) took the class-0 maximum. `0x00028830` now sets it; Cooper
  runs at 45 units/s (was 90), the same at 60, 144 and 240 Hz. The class values also feed both sides' damage,
  pauses and pulses.
- The right stick with no target in range: `0x0007EA20` reads actor +0x8F8 (the first record on the actor's
  +0x8F4 list) and the mind's own +0xB8 record is addressed like a `DAT_00545988` record, so Cooper swings at
  the air 80 units along the stick.
- Imps stood still: `0x00028830` stores `0x00107830`'s base input at actor +0x960 and the port dropped that
  store. The imps' navigation wrote their stick, but their walk state (`0x000A5A60`, input word = actor +0x960)
  read zero and went back to stand every frame. With the store, imps chase Cooper, punch and kick through
  request 2 (`0x0007B090`, its chain selection now bound like the player's `0x0007DF50`), and knock him back.
- `--player-fight` passes ("Kill All" at frame ~3260). The bot now flicks only inside its 10-unit standoff
  (a flick out of reach swings at the air) and does not chase imps more than 80 units away.
- New `--player-controls` mode: run speed at three rates, the air swing, the trigger camera turn.

### 2026-10-16: the glow, broadcasts and "Kill All"

- The glow hooks `0x00066B90` / `0x00066C00` (`entities/avatar_motion_blur.cpp`) over `graphics/glow_pass.cpp`:
  `0x0013BCC0`, `0x00101640`, `0x00062220`, `0x00061D00`, `0x00061EE0`, `0x000623C0` and `0x00062750`. Boot
  `0x000620C0`'s targets are book page 0's surface and, for the depth surface and its LIN_A8R8G8B8 alias, book
  page 1.
- Presenter: native draws take the NV097 stencil state; a texture target's depth surface that aliases a texture
  receives the target's D24S8 words when the target ends; the pixel shader runs DOTPRODUCT (stage 2) and DOT_ZW
  (stage 3) with the HILO_1 dot mapping and writes SV_Depth; `SetRenderState_PSTextureModes` overrides the
  shader's modes until the next shader; vertex programs pass full oT2 / oT3; colour rectangle clears work on
  texture targets; SetRenderTarget resets the viewport (and c-38 / c-37) to the target, as in D3D8.
- `0x00122C70` is native in scenes: `g_actor_broadcast` walks DAT_00511AA4 and sends each active actor the
  retail words (the 0x8D, 0x85 and 0x7E/0x7F payloads carry their pointers as addresses). `0x0003EBA0` takes
  the actor's latch off that list where retail unlinks +0x900 (`ActorReleaseContext::unlink_active_node`);
  the earlier check on the cutscene view link dropped every broadcast to a live actor, so a released model
  anchor never cleared a mind's +0x30.
- New states: type `0x62` (`entities/body_die_state.cpp`: `0x0008C1C0`, `0x0008C240`, `0x0008C4F0`, `0x0008C2D0`,
  `0x0008C4D0`, `0x0008C310`) and type `0x5E` (`entities/body_anim_state.cpp`: `0x00088500`, `0x00088580`,
  `0x00088660`, `0x00088960`, `0x000886B0`); `0x00134720` (`entities/body_orientation.cpp`); the dizzy tick's
  input branch; body slot +0x40 (`0x00026F90`).
- Script: `0x001330E0` (opcode 1's update) counts the "Kill All" delay down; the `0x00117100` walk now covers
  the whole update table and raises for untranslated updates instead of skipping them. The trigger's ghoulybox
  record (message 0xA8, +0xB44) has a heap-arena address.
- Host views fixed on the way: non-player target bodies read their own +0x95C word; the knockdown sweep and
  player contacts reach props and backgrounds as retail words with a retail pool image; the background rumble
  passes its cue name by address; scenery callouts use the actor's own +0x8C records; the body impact 0x1B has
  its contact views; the body tag lookup uses the objTag_Null-based table `0x00133A50` indexes; the player's
  body slot hook (+0x28, +0x38, +0x3C, +0x40) is bound; a freed target record keeps its last bytes and actor for
  the frame a mind still names it; a background resolve of size 0 returns the tail; the knockdown and dizzy
  contexts refresh body views only (a state destroyed with its player ran after the input was freed: a heap
  corruption found with page heap); `~SceneFactoryRuntime` reports a release failure instead of throwing from
  the destructor.
- `graphics/xg_pixel_shader.cpp`: a combiner's four inputs are read in order A, B, C, D. They were packed in
  one `|` expression whose evaluation order C++ leaves open, and `input()` claims constant slots, so Debug
  swapped c0 / c1 (the `--actor-loading` paired RGB/alpha check failed in Debug only).
- `--player-fight` runs up to 7200 frames and gives fresh A edges once every imp is down, for the butler's page.
- Looked at: the fight captures show the hit spark, a knocked-down imp with its health heart (frame 720), the
  butler's "Well done, Sir!" page after the gate, and the main hall for "Event Door Intro". Not confirmed by
  eye: the burning copy's glow (no capture shows it clearly; one frame has an imp high against the far wall
  with no visible glow), and the snapshot (known empty).
- Verified: Release and Debug build; `ctest` 7/7 in both; every transition suite passes in both (`--scene-start`, `--scene-spawn`, `--dialogue-resources`, `--actor-loading`, `--ghoulybox-decoder`, `--scene-models`, `--player-opening`, `--player-fight` through "Kill All", `--title`, `--title-a`, `--attract`, `--goop`, `--ui`, `--audio-preparation`); headless 900 frames (default and the opening scene) exit 0; the opening captures at 180 / 300 / 900 and the opening and walk checks are byte-identical to `cache/player_ref`; `git diff --check` clean.

### 2026-10-16: knockdown, death and the burning copy

- Translated factory type `0x7A` (`entities/body_knockdown_state.cpp`):
  sizes `0x0009C550`, constructor `0x0009C5D0`, destructor `0x0009CA80`,
  message `0x0009D0E0` (0x62 is `0x0009B720` over the collision reply), tick
  `0x0009CBE0`, and the launch, sweep, flight, bounce, landing and end paths
  (`0x0009C150`, `0x0009B350`, `0x00140BF0`, `0x0009D150`, `0x0009BEE0`,
  `0x0009C100`, `0x0009B430`, `0x0009BA40`, `0x0009B780`, `0x0009D5C0`,
  `0x0009D670`, `0x0009D6D0`, `0x0009D7D0`, `0x00025FA0`, `0x0009D810`,
  `0x00136150`, `0x0009C220`, `0x0009BD40`), and the probe notice `0x0013FC50`.
  Its +0x18 draw and +0x1C next key are the shared `0x001628B0` /
  `0x0002B620`.
- Translated factory type `0xF`, entityAvatarFxMotionBlur
  (`entities/avatar_motion_blur.cpp`): sizes `0x000668E0`, constructor
  `0x00066E20`, destructor `0x00066F50`, message `0x00070370`, tick
  `0x00066FE0` with `0x00066940` and `0x00066F70`; the appearance copy
  `0x0013AE30` and model copy `0x00040CA0`; the effect start `0x00066AB0`.
- The model clone `0x000F87D0` with `0x001560A0` and `0x000F8B80`
  (`gbtg/models/model_runtime.cpp`); model nodes now keep the ndMtxArray
  record table.
- The actor snapshot `0x00116530` / `0x00116750` and the cull mode setter
  `0x001565E0` (`graphics/actor_snapshots.cpp`), with `0x0003D900` /
  `0x0003DA00` (`entities/actor_shape_bounds.cpp`). `D3DDevice_CreateTexture2`
  is now a scene D3D boundary: the texture's header lives in the resource
  arena, the presenter renders into it (swizzled for A8R8G8B8) and decodes it
  from its creation arguments when bound.
- `scene_actor_services.cpp` builds with `/bigobj` (Debug passed the COFF
  section limit).
- Verified: Release and Debug build; ctest 7/7 in each; the six transition
  suites, `--player-opening`, `--title`, `--title-a`, `--attract`, `--goop`,
  `--ui` and `--audio-preparation` pass in both (Debug `--actor-loading` fails
  in `check_boot_pixel_shaders` as before). `--player-fight` stops at the
  frontier above in both. Both 900-frame headless runs exit 0. The opening
  captures at 180, 300 and 900, `player_opening_check.bmp` and
  `player_walk_check.bmp` are pixel-identical to `cache/player_ref/` (the
  first Debug 300 capture caught keyboard input; the retake matched). The
  fight readbacks at 600 and 720 show the hits and health hearts, but the
  knockdown's flight and landing, the copy's snapshot texture and its burn
  effects were not checked in a capture, and the cues were not listened to.
  Nothing here was compared with xemu.

### 2026-10-16: dizzy, targets and the death action

- Translated factory type `0x64`, the imp's dizzy state
  (`entities/body_dizzy_state.cpp`): sizes `0x0008C920`, constructor
  `0x0008C9A0`, destructor `0x0008CB30`, message `0x0008CEA0`, tick
  `0x0008CC40`, with the stars `0x0008CEF0` / `0x0008D0E0` / `0x0008D230`. The
  tick's `0x00134720` branch (state +0x50) and its player-input branch raise
  when reached; the imps reach neither.
- Type `0x5A` (getter `0x000801B0`), the first-slot move of the death row: tick
  `0x00080200`, step `0x0002AD30`.
- Body vtable +0x4C is bound to `0x00027500`.
- Fixes on the way:
  - a base actor (type 2) updated through its owner's bind tree before its
    first message had no update slot; vtable +0x10 is fixed by the class, so
    `update_actor` binds it on first use;
  - `actor_updates` kept the slot of a released view, so a star whose view
    reused that memory ran a dead prop's update;
  - the body apply never refreshed the actor's +0x8F8 target record (the native
    record is not a byte image), so the imps' target positions stayed at their
    spawn points and Cooper stopped finding targets once he walked past them;
  - a test that stopped mid-scene aborted in `~SceneFactoryRuntime` (stars
    released before their owner); the destructor now ends a live scene first.
- `--player-fight` flicks the right stick and keeps a short standoff.
- Verified (covers this checkpoint and the HUD one below): Release and Debug
  build; ctest 7/7 in each; the six transition suites, `--player-opening`,
  `--title`, `--title-a`, `--attract`, `--goop`, `--ui` and
  `--audio-preparation` pass in both (Debug `--actor-loading` fails in
  `check_boot_pixel_shaders` as before). `--player-fight` stops at the
  frontier above in both. Both 900-frame headless runs exit 0. The opening
  captures at 180, 300 and 900 and `player_opening_check.bmp` are
  pixel-identical to `cache/player_ref/`. `player_walk_check.bmp` moved
  slightly (Cooper ends his 120-frame walk at (-6.98722, -165.073), not
  (-6.98966, -165.131); the frame looks the same), first seen after commit
  202d952 landed; the reference was updated to the new readback. The dizzy
  stars were seen in `cache/player_fight_240.bmp`; the damage digits and the
  hit sparks were not checked in a capture. Nothing here was compared with
  xemu.

### 2026-10-16: the scoreboard HUD

- Translated the scoreboard set-up `0x00132040` (at the end of the native
  scene start), the teardown `0x00132100` (in the scene end) and the per-view
  draw `0x00132290` (`frontend/scoreboard_hud.cpp`): `0x0012EE60` /
  `0x0012EF10` (the energy digits `DAT_00545A80`), `0x0012D180` / `0x0012D200`
  (brew meter), `0x00130BB0` / `0x00130CE0` (boards), `0x0012D5F0`,
  `0x0012D930` / `0x0012D9B0`, `0x0012FA50`, `0x0012FF80` / `0x00130080`,
  `0x0012E840` and `0x00132350`. The draw's leaves raise at their first
  untranslated callee once their gates open (see the frontier).
- The damage digits over a hit imp now load and draw.

### 2026-10-16: the imp's hit reaction

- Translated factory type `0x74` (`entities/body_hit_state.cpp`, installed in
  `body_state_actions.cpp`): sizes `0x00090410`, constructor `0x00096710`,
  destructor `0x000967F0`, tick `0x00096890` with the recovery pick
  `0x00133F50` and `0x00134060`. Its scorch `0x000965C0` (body +0x268 +0x19C
  above 1) and the goal-ring branch of `0x00134060` (`0x00028360`,
  `0x00135790`, `0x00135540`) raise when reached; neither is reached by the
  imps. Body vtable +0x30/+0x34 (`0x00026B00` / `0x00026BB0`) are bound for
  types 0x27 and 0x2A.
- Fixes on the way:
  - the damage direction rows were an empty host stand-in; `0x00024970` passes
    body +0x268 itself (+0 count, +4 rows in the image), and the animation name
    is printed into `DAT_00508EB8`;
  - `0x00020C80`'s last fallback looked up the printed buffer instead of the
    bare direction name (EBX), so imps never found `hitfromFL` and stayed in
    a looping animation;
  - the script filter `0x00116950` raised on every id outside its accepting
    arms. Ids without an arm are now dropped as in retail, and the 0x31, 0x6B,
    0x6E, 0x70 and 0xAF/0xB3 arms are translated (0x80, 0x83 and 0xB0 raise).
- Type-2 effect particles (`0x000F0010`) and the world sprite quad
  `0x0005CF90` with its depth mode `0x0005DFF0`. The recording device takes
  `SetVertexData2f` texture coordinates, and the presenter draws the `0x102`
  fixed format with its texture.
- `--player-fight` now stops at fight frame ~125 at the HUD digits (present
  path), with type `0x64` behind it on the update path (see the frontier). No
  imp has died yet.
- Tests play at 10% master volume (`AGENTS.md`, Test volume).
- Verified: Release and Debug build; ctest 6/6 in each; the six transition
  suites, `--player-opening`, `--title`, `--title-a`, `--attract`, `--goop`,
  `--ui` and `--audio-preparation` pass in both (Debug `--actor-loading` fails
  in `check_boot_pixel_shaders` as before). `--player-fight` fails at the
  frontier in both. Both 900-frame headless runs exit 0. The opening captures
  at 180, 300 and 900 and the `--player-opening` readbacks are pixel-identical
  to `cache/player_ref/`. The hit
  sparks were not seen in a capture: a clean run stops at the digits before
  the effect pass draws. Nothing here was compared with xemu.

### 2026-10-16: entityStateHit and the contact path

- Translated factory type `0x73` (`entities/body_hit_state.cpp`): sizes
  `0x00095720`, constructor `0x000957A0`, destructor `0x000958A0`, message
  `0x00095C90` (with `0x000961C0`, `0x00095E30`, `0x00095E70`, `0x00028C80`),
  tick `0x00095940` with `0x00095F30`, `0x00096260`, `0x00095EA0`,
  `0x0001D5B0`, `0x001013A0`, `0x00096060`, `0x000963E0`, `0x00096390`,
  `0x00134180`, `0x001346B0` and `0x0003CCB0`. Slots +0x14/+0x18 are the empty
  `0x001628B0`; +0x1C is `0x0002B620`.
- Body vtable +0x20/+0x2C are attached for types 0x2A (`0x00075B90`,
  `0x00075C30`) and 0x27 (`0x00026950`, `0x00026A90`).
- Fixes on the way:
  - the mind and the body handlers read body +0x84 as a scene object or as
    `body_resources[0]`; it is an attackdata row in the image;
  - the player mind's message context kept a stale copy of mind +0x10's target;
  - `DAT_00433C9C` has 0x48 rows, not 0x10;
  - the damage processor's push keys were empty (`DAT_00217DB0`, read by
    `0x000239C0` / `0x00023A50`; the amount keys of `0x000238E0` start at
    `DAT_00217DE8`), and its `attributes` were actor +0x964 instead of body
    +0x268;
  - the alive count read the empty host `g_alt_links`;
  - the damage-number overlay dereferenced the unloaded `DAT_00545A80`; it now
    raises.
- Contact path: player segment probes resolve to their player; `0x0001F8C0` is
  bound with the hit-stat tables `DAT_0053E528` / `DAT_00524E2C`; the imp body
  builds its 0x15 views (contact pool, the attacker's body and attack row, the
  imp's bound, the attacker's damage entry), and Cooper's body takes the 0x60
  push-back with its source view.
- `--player-fight` is now the acceptance test (see Acceptance). It fails at the
  frontier above on Cooper's first hit (type 0x74), with the scoreboard digits
  behind it.
- Next: type `0x74`, then the HUD milestone.
- Verified: Release and Debug build; ctest 6/6 in each; the six transition
  suites, `--player-opening`, `--title`, `--title-a`, `--attract`, `--goop`,
  `--ui` and `--audio-preparation` pass in both (Debug `--actor-loading` fails
  in `check_boot_pixel_shaders` as on the unmodified tree). `--player-fight`
  fails at the frontier in both. Both 900-frame headless runs exit 0. The
  opening captures at 180, 300 and 900 and the `--player-opening` readbacks are
  pixel-identical to `cache/player_ref/`. Nothing here was compared with xemu.
