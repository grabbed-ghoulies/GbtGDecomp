# Player scenes

Every scene of the game plays on the native path: translated retail functions, called in the retail order from
`frame_tick` and driven by retail state. This page began as the milestone for the opening scene
(`aid_script_ghoulies_chapter1_scene1_2playcam`) and now holds the retail frame order, the state of the actor,
body and mind work, the controls and conventions, the test entry points (a mode per driven scene) and the
frontier. Dated checkpoints are in [Player scene checkpoints](PLAYER_SCENE_CHECKPOINTS.md); the project-wide count
is the completion tracker in the [README](../README.md#completion).

Read `AGENTS.md` first; every rule there applies.

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
| `--player-fight` | The opening flow, then Cooper fights the imps (see Acceptance), an imp finished from its hit reaction (0x7E straight to 0 health), the HUD heart in fight captures and not behind the butler's page, the chair's section (the ObjectPickedUp record with the chair's tag, Cooper in region 1, Cooper walks to the chair and A picks it up; event 0xAD with the chair's tag is queued and the section ends before its 5 s timeout; `cache/player_chair_check.bmp`, `player_chair_pickup.bmp`, `player_chair_held.bmp`), then the play-on, which fails if the opening script is still running when it gives up |
| `--player-death` | The opening flow, then Cooper stands still in the first fight: the imps empty his health, the death state 0x69 plays his dying animations and the fail stream, starts the transition (row 2) and restarts the scene ("Free Play (Fighting Intro)"), whose script then moves past its player wait |
| `--player-brew` | `--player-death` with the brew unlocked (profile unlock bit 8): the death shows the `intros_brewoffer` page, the restarted scene (DAT_00430D0C 1) sends its 0x7D and Cooper waits in 0x3F1 on the Butler's Brew prompt; the meter (0x0012D300) and the A/B widget (0x0012D290 / 0x0012D540) draw (`cache/player_brew_N.bmp`), and A drinks, doubling his health |
| `--player-controls` | The opening flow, then run speed at 30/60/144/240 Hz, a right-stick swing with no imp in range (`cache/player_air_swing.bmp`) and the trigger camera turn |
| `--scene6-save` | `--scene6` from a save: a child `--gameselect-new-game` save pointed at chapter1_scene2_1viewcam, played from game select (Start, A on the slot, A on Play); the trophy heads' award and the rare book collected in a driven scene 3 must write challenges 1 and 2 into its signed progress.xsv; then runs `--scene6-save-reload` |
| `--scene4` | Boots scene 4 (chapter1_scene4_1book, "Partner and the False Fireplace"): its transition ends, A closes the book cutscene's three pages and the script moves on to chapter1_scene4_2playcam. That transition ends too, and scene 4_2 ("Find The Key") is driven: Cooper walks to the bg-break key's part and swings until it breaks, the key's show must run (type 0x1E), then he walks into the exit volume and the script must move on to chapter1_scene5_1playcam. Captures: `cache/scene4_*.bmp`, `cache/scene4_2_*.bmp`, `cache/scene4_2_key_*.bmp` |
| `--scene5` | Boots scene 5 (chapter1_scene5_1playcam, the snooker room) and drives it: Cooper walks to a cue (type 0xAA) and presses A beside it, attacks the nearest live skeleton (class 52) while any is up, and walks into the exit volume once it opens. Fails today: the script holds at "stage 2" (see the 2026-10-20 scene 5 checkpoint) |
| `--scene6-save-reload` | Run by `--scene6-save`: its save, with the script set to scene 3, loads again from game select, both challenge bits must come back, and scene 3 must reach Free Play with no rare book |
| `--scene6` | Boots scene 6 (chapter1_scene2_1viewcam), A on the chapter title, the transition ends; the trophy heads attack, the first Super Scary challenge fails unanswered into Crivens' page, later ones are won by pressing the shown buttons, the heads' cutscene resumes, and the script moves on to scene 3 (`cache/scene6_c*_s*_*.bmp`); then scene 3's transition ends (no chapter title) and its walk-in and Free Play run with the sticks centred (the scene 3 checks below, passive) |
| `--scene6-1` | Boots scene 6 (chapter1_scene6_1playcam, the Lower Corridors): its transition ends (no chapter title), A closes each page once it has been up a second, and Cooper walks to the scare figure (type 0x1D, its +0x934 trigger), then, in "Ghouly Generator Introduction", to the nearest ghouly setup with a radius (type 0x24, +0xA38) and fights any live ghouly within 50 units. Once the section holds at its end (type 0xF, DAT_00512518), he walks down the corridor (south along x -168, east along z -620) into the exit volume (mask 1), fighting off any spider within 20 units. The scene's objects are logged at each title, the section gate every 300 frames, and the volumes, pending conditions and live script nodes on a failure. Passes when the script moves on to scene 7 |
| `--scene8` | Boots scene 8 (chapter1_scene8_1playcam, the gaming room) and plays it with the scene 7 driver: during "Event 'Kill All'" Cooper fights any live ghouly in the room, and in "Player Leaves Through Door N" he lines up in front of that door before walking into the exit volume. Passes when the script moves on to scene 9 (frame 4644 in the 2026-10-22 run) |
| `--scene9` | Boots scene 9 (chapter1_scene9_1playcam, the basement cellar) and plays it with the scene 7 driver, doors first: a live PlayerApproachesDoor sends Cooper to that door before any region, and a walk held in place for 1.5 s avoids the grid cell it was walking into (coffins and crates the grid leaves open). Cooper fights the mummy in its room, burns it with the candle Crivens hands over, and leaves through door 8. Passes when the script moves on to scene 10 (frame 9600 in the 2026-10-22 run; up to 18000 frames are allowed) |
| `--scene10` | Boots scene 10 (chapter1_scene10_1playcam, the relics store) and plays it with the scene 7 driver, doors first. The walk cutscene and the mummy's Super Scary challenge play by themselves; during "Event: Kill All" Cooper fights any live ghouly at any range (1000), and nothing outside it. Passes when the script moves on to `aid_script_ghoulies_chapter1_scene11_2book` (frame 6078 in the 2026-10-22 run). The mummy's marker is drawn at random, so runs differ |
| `--scene11` | Boots scene 11 (chapter1_scene11_2book) and passes its book to `aid_script_ghoulies_chapter1_scene11_3viewcam`, then plays that scene with the scene 7 driver: it presses the shown buttons of a running Super Scary challenge (type 0x98, states 5 and 6), fights any live ghouly at any range (1000) except the ugly girl (tag 0x37, Cooper's partner) and otherwise walks to the regions and the exit. Passes when the script moves on to `aid_script_ghoulies_chapter1_scene12_1playcam` (frame 3041 in the 2026-10-22 run) |
| `--scene12` | Boots scene 12 (chapter1_scene12_1playcam, the wine cellar) and plays it with the scene 7 driver, doors first: Cooper fights any live ghouly at any range (1000), and a ghouly too far for the right stick is walked to like a goal (held cells avoided). The sleeping haunted door wakes as he comes near, and the fight ends with Cooper's one-liner. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene1_1playcam` (frame 5837 in the 2026-10-23 run) |
| `--scene2a1` | Boots chapter 2a scene 1 (chapter2a_scene1_1playcam, the kitchen stairwell) and plays it with the scene 7 driver, doors first, at any fight range. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene1_2book` ("Fire extinguihser weapon Intro"; frame 9132 in the 2026-10-23 run) |
| `--scene2a1b` | Boots the extinguisher's book (chapter2a_scene1_2book), plays it to `aid_script_ghoulies_chapter2a_scene1_3playcam` (the kitchen) and plays that with the scene 7 driver, doors first, fighting only in "Event - Kill All" (the cook, tag 8, is not a ghouly). A held weapon is aimed with the right stick held (the extinguisher's spray), bare hands tap it. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene2_1playcam` (frame 4115 in the 2026-10-23 run) |
| `--scene2a2` | Boots chapter 2a scene 2 (chapter2a_scene2_1playcam, the dishwashing room) and plays it with the scene 7 driver, doors first, at any fight range: "Event - 'Kill All/Time Limit'" is one coward zombie against the reaper's countdown. The driver fights a ghouly within 15 in height (the hurt zombie climbs onto the plate stacks). Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene3_1playcam` (frame 5976 in the 2026-10-23 run) |
| `--scene2a2-reaper` | Chapter 2a scene 2 with no fighting (fight range 0), so the time limit runs out and the Grim Reaper comes. Fails by design once its 18000 frames are up: the reaper takes Cooper's life, the retry restarts the scene and the time limit runs out again (four times in the 2026-10-23 run). It is for looking at the spider web, the reaper and the fade to grey |
| `--scene2a3` | Boots chapter 2a scene 3 (chapter2a_scene3_1playcam) and plays it with the scene 7 driver. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene4_2playcam` (frame 2037 in the 2026-10-23 run) |
| `--scene2a4` | Boots chapter 2a scene 4 (chapter2a_scene4_2playcam, the ballroom) and plays it with the scene 7 driver, not fighting during "free play". Script type 0x67 opens the balcony gate's collision (category 0xFE) through 0x0012C8A0, Cooper walks onto the dance floor (region 11), the skeleton and zombie event is fought to its end and he leaves through the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene5_1playcam` (frame 12503 in the 2026-10-24 run) |
| `--scene2a5` | Boots chapter 2a scene 5 (chapter2a_scene5_1playcam, the conservatory) and plays it with the scene 7 driver, not fighting during "FREE PLAY" (it ends at door 0xF): the door closes, the ninja imps (factory type 0x49) come, the key carrier's death drops the ghouly key (type 0x18) under "ghoulykeycam" and Cooper leaves through the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene6_1book` (frame 7164 in the 2026-10-24 run) |
| `--scene2a6` | Boots chapter 2a scene 6's book (chapter2a_scene6_1book), passes it to `chapter2a_scene6_2playcam` (the boathouse) and plays that with the scene 7 driver. In a one-type event (step type 0xD) the driver goes for that class and fights off any other ghouly within 10. Fails today: Cooper is mobbed in a pocket by the barrels (around 28, -49) during "EVENT - ONE TYPE" and dies, and the scene restarts (2026-10-24). Played by hand the same day, the scene completes and moves on to the next one |
| `--scene2a7` | Boots chapter 2a scene 7 (chapter2a_scene7_2playcam) and plays it with the scene 7 driver at any fight range: the Medusa (factory type 0x4B) gazes at Cooper, whose shock starts the Super Scary challenge, and the event's key candidates are thinned by step type 1. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene8_1playcam` (frame 4504 in the 2026-10-24 run) |
| `--scene2a8` | Boots chapter 2a scene 8 (chapter2a_scene8_1playcam, the lighthouse living quarters) and plays it with the scene 7 driver: after the zombies, region 1's ghoulybox spawns the pirate (factory type 0x4E), whom Cooper beats with the water pistol. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene9_1playcam` (frame 4205 in the 2026-10-24 run) |
| `--scene2a9` | Boots chapter 2a scene 9 (chapter2a_scene9_1playcam, the lighthouse lantern room) and plays it with the scene 7 driver: Cooper climbs the spiral stair to the glow worms (the driver walks a goal on another floor through the stair's end), the exit door opens and the imp from the ghoulybox jumps onto Cooper's head (factory type 0x79), which the driver shakes off with the left stick. Cameron (tag 6) is not fought. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene10_1playcam` (frame 2593 in the 2026-10-24 run) |
| `--scene2a10` | Boots chapter 2a scene 10 (chapter2a_scene10_1playcam, the lighthouse living quarters again) and plays it with the scene 7 driver: "Event: Required Number (Zombies)" counts zombie kills against a time limit while the haunted TV makes more, then the second "Free Play" waits for every ghouly to be gone. The driver goes after a kill-count step's class (step type 3, its tag at +0x1C) and lines the exit up through the door it came in by. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene11_1playcam` (frame 5493 in the 2026-10-24 run) |
| `--scene2a11` | Boots chapter 2a scene 11 (chapter2a_scene11_1playcam, the lighthouse store) and plays it with the scene 7 driver: Cooper loses the water pistol, comes down from the balcony (the driver's walk keeps to steps of 8 in floor height), fights the Medusas bare-handed through a Super Scary shock and leaves by the exit; the groundskeeper (tag 0xB) is not fought. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene12_2playcam` (frame 3383 in the 2026-10-24 run) |
| `--scene2a12` | Boots chapter 2a scene 12 (chapter2a_scene12_2playcam, the boathouse) and plays it with the scene 7 driver: "EVENT - ONE TYPE" is the hunchback (factory type 0x45), who taunts, evades, blocks and charges his special attack until Cooper beats him. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene13_2playcam` (frame 6527 in the 2026-10-24 run) |
| `--scene2a13` | Boots chapter 2a scene 13 (chapter2a_scene13_2playcam, the dining room) and plays it with the scene 7 driver: "EVENT - NUMBER" wants the pirate leader (tag 0x25), who holds a chest, and the pirates (tag 0x24) that keep bursting in through the windows. The driver goes for the leader first and, with no ghouly within reach, drinks soups from the smashed ghoulyboxes when Cooper is below half his health. Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene14_1playcam` (frames 11420, 12005 and 18075 in three 2026-10-24 runs; the waves vary) |
| `--scene2a14` | Boots chapter 2a scene 14 (chapter2a_scene14_1playcam, the ballroom) and plays it with the scene 7 driver. Does not pass yet: "Event - One type (Skeleton) and one type (zombie)" puts eight skeletons (tag 0x34) and eight zombies (tag 0x3C) on the dance floor against Cooper's 15 energy, and the driver dies before both kill-all steps are done; the event then restarts from "free play" |
| `--scene2a15` | Boots chapter 2a scene 15 (chapter2a_scene15_1playcam) and plays it with the scene 7 driver at any fight range: a ghoulybox request (0x0013E8D0) reaches the chair weapon's message (0x000CF0D0). Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene16_1playcam` (frame 4731 in the 2026-10-25 run) |
| `--scene2a16` | Boots chapter 2a scene 16 (chapter2a_scene16_1playcam, the dishwashing room) and plays it with the scene 7 driver at any fight range through "EVENT - ONE TYPE/DON'T KILL". Passes when the script moves on to `aid_script_ghoulies_chapter2a_scene17_1playcam` (frame 4002 in the 2026-10-25 run) |
| `--scene2a17` | Boots chapter 2a scene 17 (chapter2a_scene17_1playcam, the kitchen) and plays it with the scene 7 driver at any fight range: the cook reveals the second ingredient and the ghoulybox pirate enters. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene1_1playcam` (frames 3159 and 3218 in two 2026-10-25 runs) |
| `--scene2b1` | Boots chapter 2b scene 1 (chapter2b_scene1_1playcam, the freezer) and plays its "FREE PLAY" with the scene 7 driver. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene2_1playcam` (frame 1494 in the 2026-10-25 run) |
| `--scene2b2` | Boots chapter 2b scene 2 (chapter2b_scene2_1playcam, the garbage yard) and plays it with the scene 7 driver at any fight range: "Event: Survive" spawns three waves of spiders behind cutscenes, each wave waiting on the kill count (condition 4). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene3_2playcam` (frame 14881 in the 2026-10-25 run; the driver spends about 6000 frames of the last "Free Play" held by the exit around (236, 84)) |
| `--scene2b3` | Boots chapter 2b scene 3 (chapter2b_scene3_2playcam, the wood shed) and plays its "Free Play" with the scene 7 driver at any fight range; the scene ends at the exit volume (-166, 636). Does not pass yet: the flying imps (factory type 0x48) and the Medusa (class 0x1F) by the exit wear Cooper down, he dies and the scene restarts (2026-10-25). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 4688 (Release) |
| `--scene2b4` | Boots chapter 2b scene 4 (chapter2b_scene4_1playcam, the farmyard) and plays it with the scene 7 driver at any fight range. Does not pass yet: "event - kill all/time limit" (pirates, ninja imps, skeletons with eggs, flying imps and zombies from ghoulyboxes) runs out of time before the driver kills them all, the Grim Reaper comes and the event restarts (2026-10-25) |
| `--scene2b4b` | Boots chapter 2b scene 4's book (chapter2b_scene4_2book): the groundskeeper's pages (Ol' Edna, the garlic gun). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene5_1playcam` (frame 1713 in the 2026-10-25 run) |
| `--scene2b5` | Boots chapter 2b scene 5 (chapter2b_scene5_1playcam, the chicken shed) and plays it with the scene 7 driver at any fight range: the Baron's page, "EVENT - TIME LIMIT/SURVIVE" against vampire chickens with the blunderbuss, the event's completion and the exit door opening. Does not pass yet: in the last "FREE PLAY" Cooper dies fighting by the exit, the scene restarts from its first page, and the run ends in the second survive event (2026-10-25). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 15451 (Release) |
| `--scene2b6` | Boots chapter 2b scene 6's book page (chapter2b_scene6_1book) and plays it until the script moves on to `aid_script_ghoulies_chapter2b_scene6_2playcam` (frame 2226 in the 2026-10-26 run) |
| `--scene2b6b` | Boots chapter 2b scene 6 (chapter2b_scene6_2playcam, the farmyard) and plays it with the scene 7 driver: the driver chases the class a pending PlayerHitGhouly names (Mr Ribs, tag 0x31, by the barn door), leaves each penned vampire chicken after a minute out of reach, and walks into region 1. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene7_1playcam` (frame 18716 in the 2026-10-26 run) |
| `--scene2b7` | Boots chapter 2b scene 7 (chapter2b_scene7_1playcam, the kiln rooms) and plays it with the scene 7 driver: the walk-in, "EXIT DOOR CLOSES", "EVENT - REQUIRED NUMBER/FIND THE KEY" with the furnace burning skeletons, then "FREE PLAY", where the driver leaves the workshop spawn's mummies and walks out of the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene8_1playcam` (frame 12242 in the 2026-10-26 run). Since 2026-11-01 (enemies provoked by hits) it does not pass: the skeletons fight back and Cooper faints 5 times in the event. Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 56540 (Release). The occasional access violation near frame 46560 was another player's mind reading a released weapon through a stale copy of the target's body +0x48; fixed 2026-11-08, and an ASan run plus five Release runs are clean |
| `--scene2b8` | Boots chapter 2b scene 8 (chapter2b_scene8_1playcam, the lower corridors) and plays it with the scene 7 driver: the giant hand grabs Cooper (its Super Scary challenge, mode 2), the driver leaves Mr Ribs, who takes no hits, and hits a haunted chair (PlayerHitGhouly, tag 0x10). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene9_1playcam` (frame 4791 in the 2026-10-26 run) |
| `--scene2b9` | Boots chapter 2b scene 9 (chapter2b_scene9_1playcam, the west wing pantry) and plays it with the scene 7 driver, calm in FREE PLAY: the haunted coats' event (EVENT: ONE TYPE - HOODED FIGURE) and the haunted doors' (ONE TYPE (HAUNTED DOOR) / DON'T KILL (IMP)). Cooper faints once in the first door event in the 2026-10-26 run and the room restarts. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene10_1playcam` (frame 7373 in the 2026-10-26 rerun) |
| `--scene2b10` | Boots chapter 2b scene 10 (chapter2b_scene10_1playcam, the kitchen) and plays it with the scene 7 driver, calm in FREE PLAY: Cooper fights Mr Ribs until his energy is gone (scriptConditionId_ActorHasNoEnergy). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene10_2book` (frame 4551 in the 2026-10-26 run) |
| `--scene2b10b` | Boots chapter 2b scene 10's book (chapter2b_scene10_2book, "Fire extinguisher weapon Intro") and plays its four pages (A closes each). Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene1_1playcam` (frame 2716 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the "Phew! Mr. Ribs okay" page (frame 1500) and the goop transition over the last page (2400) |
| `--scene2c1` | Boots chapter 2c scene 1 (chapter2c_scene1_1playcam) and plays it with the scene 7 driver, calm in FREE PLAY: the haunted paintings' introduction, then Cooper walks to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene2_1playcam` (frame 1480 in the 2026-10-26 run) |
| `--scene2c2` | Boots chapter 2c scene 2 (chapter2c_scene2_1playcam, the cloakroom). In FREE PLAY the driver walks to the bg-break setup holding `aid_ghoulybox_powerups_supersoups_invisibility` (the suitcase, part 7 at 219, -77), breaks it, drinks the invisibility soup (powerup kind 9) and walks out unseen: no coat or picture wakes, so the exit stays open. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene3_1playcam` (frame 2092 in the 2026-10-26 run) |
| `--scene2c3` | Boots chapter 2c scene 3 (chapter2c_scene3_1playcam) and plays it with the scene 7 driver, calm in FREE PLAY: the event page ("Keep your eye on the cross"), then "EVENT - KILL ALL/NEXT ONE DIFFERENT" against zombies (tag 0x3C) and skeletons (0x34) with no faints. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene4_1book` (frame 4388 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the event page (frame 1200) and Cooper fighting a zombie (2400) |
| `--scene2c4` | Boots chapter 2c scene 4's book (chapter2c_scene4_1book) and plays its four pages. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene4_2playcam` (frame 2421 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the comic panels with Cooper holding a new weapon (frame 1800) |
| `--scene2c4b` | Boots chapter 2c scene 4_2 (chapter2c_scene4_2playcam, the music room) and plays it with the scene 7 driver: the untitled fight against mummies, skeletons and zombies with the popcan gun, and an SSS door (type 0x20) that springs a Super Scary challenge (the dragon scare) near the end. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene5_1playcam` (frame 9621 in the 2026-10-26 run). Since 2026-11-01 (enemies provoked by hits) it does not pass: Cooper faints 8 times. Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 22035 (Release) |
| `--scene2c5` | Boots chapter 2c scene 5 (chapter2c_scene5_1playcam, the walled garden) and plays it with the scene 7 driver. While a PlayerHitGhouly watches (the pirate's skeletons dig for the dungweed), Cooper fights nothing and walks to the nearest record of the dungweed's region 1; the angry baron's fight follows. **Fails**: Cooper finds the dungweed every time (page 3, frame 2478) but faints in the fight, where the pirate's shovel takes all five hearts in one hit and the flying imp's dive three, and the scene restarts until frame 24000 |
| `--scene2c6` | Boots chapter 2c scene 6 (chapter2c_scene6_1playcam, the study) and plays it with the scene 7 driver: "EVENT - FIND THE KEY" arms one of three bg-break keys and the driver punches its part open; "EXIT DOOR OPENS" lets the ninja imps in and FREE PLAY is fought, Jessie Clyde (types 0x29 and 0x4A) transforming as Cooper comes near. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene7_1playcam` (frame 6040 in the 2026-10-27 run). Checked in the captures: the sleeping Jessie Clyde (frame 2400), her transformation (3600) and her fight with Cooper (4800) |
| `--scene2c7` | Boots chapter 2c scene 7 (chapter2c_scene7_1playcam) and plays it with the scene 7 driver: "EVENT - NUMBER" (generic ghoulies, type 0x3C), a cursed mummy section (type 0x4D) and "EVENT - ONE TYPE". Does not pass yet: the cursed mummy's slab whacks make Cooper faint once, and the replayed scene reaches its last FREE PLAY at frame 35772 of the 36000 the driver allows (2026-10-27). Checked in the captures: the curse's glyphs around Cooper (frames 9600, 10200) and the mummy's raised slab (11100). Passes since 2026-11-01 (enemies provoked by hits): the script moves on to `aid_script_ghoulies_chapter2c_scene8_1playcam` |
| `--scene2c8` | Boots chapter 2c scene 8 (chapter2c_scene8_1playcam, the haunted paintings) and plays it with the scene 7 driver: "kill all/limited attacks" against the paintings, then FREE PLAY to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene9_1playcam` (frame 5467 in the 2026-10-27 run; the paintings make Cooper faint once and the scene replays). Checked in the captures: Cooper punching among the paintings (900), a painting knocked off the wall (3900), and Cooper at the exit door (5400) |
| `--scene2c9` | Boots chapter 2c scene 9 (chapter2c_scene9_1playcam, the cloakroom) and plays it with the scene 7 driver, calm in FREE PLAY: the event (script type 0x28, step type 12) fails once any points are scored, so Cooper walks to the exit without fighting. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene10_1playcam` (frame 1097 in the 2026-10-27 run). Checked in the captures: Cooper running through the cloakroom at full energy with the event icon up (600, 900) |
| `--scene2c10` | Boots chapter 2c scene 10 (chapter2c_scene10_1playcam, the central hallway) and plays it with the scene 7 driver: FREE PLAY until the haunted door (type 0x2A ghoulybox) wakes (event 0xB4, condition opcode 7), "EXIT DOOR CLOSES", "EVENT - ONE TYPE/WEAPONS ONLY" against skeletons, then FREE PLAY to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene11_1playcam` (frames 13217 and 24524 in two 2026-10-27 runs; Cooper faints and the scene restarts several times). Not reliable: one earlier run stopped with `invalid unordered_map<K, T> key` on its second pass (not reproduced, no stack), and one left the door unspawned on its sixth pass, after a pass that beat the door and fainted (not reproduced). Checked in the captures: Cooper fighting the haunted door (2700, 6900) and the skeletons breaking (23400) |
| `--scene2c11` | Boots chapter 2c scene 11 (chapter2c_scene11_1playcam, the kitchen) and plays it with the scene 7 driver: FREE PLAY, "EXIT DOOR CLOSES", "EVENT - KILL ALL/TIMELIMIT" against the imps, then an untitled closing section. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene11_2book` (frame 7401 in the 2026-10-27 run, no faint). Checked in the captures: Cooper among the imps with the 1:17 timer and the gauge up (1500), and the closing view of the corridor and its clock pickups (7200) |
| `--scene2c11b` | Boots chapter 2c scene 11's book (chapter2c_scene11_2book, "cook mixes cure") and plays its three pages (A closes each). Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene12_1playcam` (frame 1911 in the 2026-10-27 run). Checked in the captures: the cook's first page, her portrait and text and the A prompt (page1) |
| `--scene2c12` | Boots chapter 2c scene 12 (chapter2c_scene12_1playcam, the wine cellar) and plays it with the scene 7 driver: "EVENT - GHOULY FIND THE KEY". The driver first breaks the barrel whose ghoulybox is `aid_ghoulybox_ghoulies_chapter2_winercellarkeyholder` (a `powerup_kind` of 0: it holds ghoulies, not a powerup; on its rack it is some 21 from open floor, so from within 24 Cooper stands and punches), then fights what comes out; the key carrier step (type 0xF) ends the event, then FREE PLAY to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene13_1book` (frame 4292 in the 2026-10-27 run; the barrel broke at 1216). Checked in the captures: Cooper in the cellar with the key-holder event icon up (1800) and the key camera down the corridor to the door (2400) |
| `--scene2c13` | Boots chapter 2c scene 13's first book (chapter2c_scene13_1book) and plays its page (A closes it). Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene13_2playcam` (2026-10-27). Checked in the captures: Cooper's page to Amber about the cook's potion, with its portrait and the A prompt (page1) |
| `--scene2c13b` | Boots chapter 2c scene 13_2 (chapter2c_scene13_2playcam, the basement archives) and plays it with the scene 7 driver: Cooper against Amber as a ghoulie (entityStrategyGhoulieGirl, type 0x3D, class 0xC). Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene13_3book` (frame 6623 in the 2026-10-27 run; her knockdowns make Cooper faint once and the scene replays). Checked in the captures: Cooper knocked down by her (300) and her beside Cooper in the archives (6300) |
| `--scene2c13c` | Boots chapter 2c scene 13's closing book (chapter2c_scene13_3book, "Cook's assistant reveals prisoner's plight") and plays its two pages (A closes each). Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene1_2book`, the end of chapter 2c (frame 3341 in the 2026-10-27 run). Checked in the captures: the cook's first page (page1) |
| `--scene3a1` | Boots chapter 3a scene 1's book (chapter3a_scene1_2book) and plays its page (A closes it). Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene1_3playcam` (frame 1212 in the 2026-10-27 run). Checked in the captures: Mr. Ribs' page, his portrait and text and the A prompt (page1) |
| `--scene3a1b` | Boots chapter 3a scene 1_3 (chapter3a_scene1_3playcam): the chapter 3a title (A moves it on), then FREE PLAY, where Cooper walks out through the exit (region 1). Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene2_1playcam` (frame 358 of play in the 2026-10-27 run). Cooper's energy reads 1 throughout (not checked against the script). Checked in the captures: the chapter title's sketch (title) and Cooper running for the open door (300) |
| `--scene3a2` | Boots chapter 3a scene 2 (chapter3a_scene2_1playcam, the haunted television) and plays it with the scene 7 driver, calm in FREE PLAY: the "EVENT" breaks the bg-break key loose, then Cooper walks past the television to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene3_1playcam` (frame 1652 in the 2026-10-27 run, no faint). Fighting in FREE PLAY instead lets the class 0x15 ghoulie hit Cooper for 3 about every 100 frames; that run fainted eleven times, and a class 0x17 ghoulie threw the homing projectile (type 0xA8) on the sixth pass. Checked in the captures: the "Haunted Television" introduction (900) and Cooper running past the glowing set to the door (1500) |
| `--scene3a3` | Boots chapter 3a scene 3 (chapter3a_scene3_1playcam) and plays it with the scene 7 driver: "Event - Kill All/Weapons only", then "Exit door opens". Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene3_2book` (frame 11284 in the 2026-10-27 run; Cooper faints once and the scene replays). Checked in the captures: Cooper among the ghoulies with a hit flash (2400) and the cleared room, debris on the floor and the far door lit (11100) |
| `--scene3a3b` | Boots chapter 3a scene 3's book (chapter3a_scene3_2book, "Butler explains about the scrolls") and plays its six pages (A closes each). Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene3_3playcam` (2026-10-27). Checked in the captures: the butler's first page, his portrait and text and the A prompt (page1) |
| `--scene3a3c` | Boots chapter 3a scene 3_3 (chapter3a_scene3_3playcam) and plays it with the scene 7 driver: FREE PLAY to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene4_1playcam` (frame 1817 in the 2026-10-27 run). Checked in the captures: the butler jostling Cooper in the room (900) |
| `--scene3a4` | Boots chapter 3a scene 4 (chapter3a_scene4_1playcam, the storeroom with the brambled door) and plays it with the scene 7 driver, which first breaks `aid_ghoulybox_ghoulies_chapter3a_warlockscene4` (a box of ghoulies) and then fights the warlock it holds. Breaking the box clears the brambles ("Exit door revealed"); the door opens once the warlock is beaten (the section waits on GhoulyExistsInScene for tag 0x3A). Passes when the script moves on to `aid_script_ghoulies_chapter3a_scene5_1playcam` (frames 5797 and 4009 in two 2026-10-27 runs). Not reliable: a third run took the freeze soup mid-fight, the warlock stayed gone, the driver gave up on him, and Cooper fainted at the locked door. The restarted scene has no warlock (the box stays broken), so it never ends. Checked in the captures: the warlock beside Cooper as the door is revealed (900), Cooper knocking him back with his meter at 37 (1500), and the plain door after he is beaten (4800) |
| `--scene3a5` | Boots chapter 3a scene 5 (chapter3a_scene5_1playcam, the two-floor hall) and plays it with the scene 7 driver, calm in "free play", toward the exit on the upper floor at (160, 85, -239). Does not pass: Cooper starts with 5 energy, and the two pirates by the top of the stairs (around (141, -146) and (142, -113)) knock him out with one 6-energy hit as he passes, so the scene restarts until the frame limit. With fighting allowed the pirates and skeletons win too. Not settled: whether the 6-energy pirate hit and the 5-energy start are retail's. Checked in the captures: Cooper knocked down by the skeletons by the brambled door (1200), and the two pirates raising their chests at Cooper (14400). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 1970 (Release) |
| `--scene3a6` | Boots chapter 3a scene 6 (chapter3a_scene6_1playcam, the mummies' corridor) and plays it with the scene 7 driver. Cooper starts with 1 energy, so each hit from the nine mummies (class 0x22) or the haunted pictures (class 0x14) ends the attempt; the scene restarts 10 times and moves on to `aid_script_ghoulies_chapter3a_scene7_1playcam` at frame 29820 in the 2026-10-28 run. Not settled: whether the 1-energy start is retail's. Checked in the captures: Cooper and a haunted picture in the corridor with the mummies beyond (600), and Cooper inside the ring of mummies by the door (1800) |
| `--scene3a7` | Boots chapter 3a scene 7 (chapter3a_scene7_1playcam) and plays it with the scene 7 driver: free play, then "event - one type/time limit", then free play to the exit. Cooper faints once (frame 11274) and the replay finishes; the script moves on to `aid_script_ghoulies_chapter3a_scene8_1playcam` at frame 21899 in the 2026-10-28 run. Checked in the captures: Cooper running past the cupboard doors (3600) and breaking through a folding screen during the event (6000) |
| `--scene3a8` | Boots chapter 3a scene 8 (chapter3a_scene8_1playcam, the schoolroom) and plays it with the scene 7 driver. Does not pass: "EVENT - NO DAMAGE" means no damage to the room. Its step type 12 (`0x000D0140`) fails once DAT_00545980, the points from broken background parts, is nonzero. Any box Cooper punches open (message 0x15, category 1, the invisibility soups included) or furniture a ghouly he hits smashes into (category 2, the hitter at body +0x270) fails the event, and the Grim Reaper then takes Cooper's life on touch (10 -> 0). The driver fights next to the furniture, so every attempt fails. Driver changes: with several boxes of one name it goes to the nearest, and a broken box's powerup comes before the next box. Checked in the captures: the Medusa's gaze and the imps by the desks (2100), and the Grim Reaper striking Cooper among the bookshelves (2400) |
| `--scene3a9` | Boots chapter 3a scene 9 (chapter3a_scene9_1playcam, the infirmary) and plays it with the scene 7 driver. Reaches no untranslated code but does not pass. In FREE PLAY the patients take 4 energy a hit. "EXIT DOOR CLOSES" puts Cooper at 183.7, -35.6, and "EVENT - SURVIVE/FIND THE KEY" starts a timer (0:57 at frame 3300) with the Medusa and the imps; Cooper faints after about 1000 frames of it every time and the scene restarts in FREE PLAY. The driver also stalls on soups and boxes it cannot reach: the soup near 280, 67, the free kind 4 powerup at -60, -150 and the `energyboost10` box at 242, 148. A run with that box as `powerup_box` (kind 4) still fainted eight times. Checked in the captures: the infirmary beds and patients (600), the exit door (1200), and the Medusa and the imps under the event timer (1800, 3300). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 21263 (Release) |
| `--scene3a10` | Boots chapter 3a scene 10 (chapter3a_scene10_2playcam, the village; no `_1` script exists) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire body (type 0x2C) and strategy (type 0x54) are installed, but does not pass. The club-wielding class 0x24 ghoulies hit for 5 and knock Cooper down, so he faints four times in 36000 frames. The end conditions wait on two PlayerEntersRegion steps (words 0x3EC and 0x494). Checked in the captures: the village path, the cottages, and a class 0x24 ghouly over a knocked-down Cooper (5100, 12600). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 24700 (Release) |
| `--scene3a11` | Boots chapter 3a scene 11 (chapter3a_scene11_1playcam, the vegetable garden) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire's knock-back hit (`0x00020690`) and front hit (`0x00022FB0`) are translated, but does not pass. "EXIT DOOR OPENS" comes about 400 frames in, and the class 0x38 ghoulies then wear Cooper down 4 energy a hit. He faints 18 times in 36000 frames, and the scene restarts each time. Checked in the captures: the garden and a ghouly at the plants (1200), and the vampire out of his open coffin at Cooper, with bats (3000). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 49940 (Release) |
| `--scene3a12` | Boots chapter 3a scene 12 (chapter3a_scene12_1playcam, the greenhouse) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire's bite (type 0x61) and the grabbed body's state (type 0x71) are installed, but does not pass. "EVENT - KILL ALL/TIME LIMIT" starts about 1550 frames in. The vampire's bites take one energy a hold, and the class 0x34 ghoulies wear Cooper down until he faints. He faints 3 times in 36000 frames, and the event restarts each time. Checked in the captures: the rhyme paper (1500), Cooper among the skeletons (4500), and the vampire holding Cooper up out of his coffin (19500) |
| `--scene3b1` | Boots chapter 3b scene 1 (chapter3b_scene1_1playcam) and plays it with the scene 7 driver. Passes: free play from frame 358, the script moves on to chapter3b_scene2_1playcam at frame 7924, and Cooper's energy stays at 10. Checked in the captures: Cooper fighting class 0x1B ghoulies among the plants (1200, 4800) and walking to the exit door (7800) |
| `--scene3b2` | Boots chapter 3b scene 2 (chapter3b_scene2_1playcam, the garden) and plays it with the scene 7 driver at a fight range of 50. Reaches no untranslated code but does not pass. The script's head opens the picket gates' collision (type 0x67, category 0xFB, word 0), which scene start replays (`0x00127250`); Cooper walks through the gate into region 1 (frame 17400 in the 2026-11-07 run) and the script moves to "picket gates and exit door closes" (17552), then "event - number/time limit" (17838). The driver then heads for the shut exit door (about -230, 373) instead of playing the event, and the scene restarts. Checked in the captures: the worm's explosion (7991), Cooper inside the cottage garden with the gate behind him (17400) and the event's icon with the exit shut (17700) |
| `--scene3b3` | Boots chapter 3b scene 3 (chapter3b_scene3_1playcam, Dunfiddlin Cottage) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0). Reaches no untranslated code but does not pass: after "free play (find warlock)" (frame 1409) the warlock reappears downstairs, and the driver's 15-unit height rule keeps Cooper upstairs by the shut exit. Checked in the captures: the warlock fought in the hallway (1200) and his intro card (1500) |
| `--scene3b4` | Boots chapter 3b scene 4 (chapter3b_scene4_2playcam, the workshop) and plays it with the scene 7 driver: an energy boost soup first (powerup kind 3, fetched again after a faint), fighting only during "event - survive/no weapons". Does not pass since 2026-11-08 (issue #52): the skeletons now fetch the workshop's weapons, and armed they make Cooper faint in every attempt until the frame limit. Before, it passed at frame 15585 because no skeleton ever found a weapon. Checked in the captures: a skeleton swinging a chair (3000) and a lit bulb over one that found a weapon (4200). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes, once a dead skeleton's mind no longer stops the run on the address of an actor 0x001232D0 released: the script moves on at frame 21761 (Release) |
| `--scene3b5` | Boots chapter 3b scene 5 (chapter3b_scene5_2playcam, the ogres' lane) and plays it with the scene 7 driver, fighting nothing (the class 0x1E ogres sleep) and taking an energy boost soup first. Passes once the grab and throw state (type 0x72) is installed: Cooper is thrown three times and the script moves on to chapter3b_scene6_1playcam at frame 4933. Checked in the captures: the ogres asleep and over Cooper (1800, 2100). By 2026-11-08 it no longer passed, on the commit before these changes too: the ogres knock Cooper out within some 1800 frames each time. Since then it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 8246 (Release) |
| `--scene3b6` | Boots chapter 3b scene 6 (chapter3b_scene6_1playcam, the foundry) and plays it with the scene 7 driver. Passes: the bg-break key behind the barrel comes out at frame 1457 (the driver punches from 24 once on the key's approach cell), Cooper fights the class 0x12 haunted doors in the time-limit event, and the script moves on to chapter3b_scene7_1playcam at frame 5947. Checked in the captures: the key coming out (1500) and a haunted door fight (3600) |
| `--scene3b7` | Boots chapter 3b scene 7 (chapter3b_scene7_1playcam, the barn) and plays it with the scene 7 driver. Reaches no untranslated code once bundle lookups ignore case (the skeleton's ..._noegganims_turnStep is listed as ..._turnstep), but does not pass: Cooper is held at a pen wall at about (150, 175) and never reaches region 10 at (942, 73). Checked in the captures: Cooper with an extinguisher at the wall (3000) |
| `--scene3c1` | Boots chapter 3c scene 1 (chapter3c_scene1_1playcam, the foundry) and plays it with the scene 7 driver. Reaches no untranslated code once the script's event steps 0x25 (type 8) and 0x5F (type 0xE) are queued and run, but does not pass: the RIP count stays at 6 (no mummy dies to punches) and the cursed mummy wears Cooper down until he faints. Checked in the captures: the challenge HUD and the furnace (4500, 6000). Since 2026-11-01 the mummies fight once the event sets their range (node type 8) and bind Cooper; still does not pass (5 faints). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes, luring the mummies (class 0x22) into the furnace's sphere (37.5), where they burn: without the candle they die no other way: the script moves on at frame 82425 (Release) |
| `--scene3c2` | Boots chapter 3c scene 2 (chapter3c_scene2_1playcam, the farmyard) and plays it with the scene 7 driver. Passes: the script moves on to chapter3c_scene3_2playcam at frame 10748, Cooper keeping all 5 energy. Checked in the captures: the farmyard (3000, 9000) |
| `--scene3c3` | Boots chapter 3c scene 3 (chapter3c_scene3_2playcam) and plays it with the scene 7 driver at a fight range of 50. Reaches no untranslated code but does not pass: region 1 is entered, but Cooper never gets within 50 of door 24 (closest 136) and is twice returned to the start, seemingly caught by the groundskeeper. Checked in the captures: a gassing ghouly (4500) and Cooper caught (12000) |
| `--scene3c4` | Boots chapter 3c scene 4 (chapter3c_scene4_1playcam) and plays it with the scene 7 driver. Passes: "event - ghouly find the key" and the script moves on to chapter3c_scene5_1playcam at frame 7184. Checked in the captures: Cooper and the groundskeeper (1200) |
| `--scene3c5` | Boots chapter 3c scene 5 (chapter3c_scene5_1playcam) and plays it with the scene 7 driver. Does not pass since 2026-11-08 (issue #50): "event - kill all/limited attacks" now counts Cooper's attacks, and the driver uses them up before the ghoulies are dead, so the event restarts until the frame limit. Before, it passed at frame 6513 because no attack was counted. Checked in the captures: the attacks left counting down 10, 8, 5, 2 (1800 to 2700). Since 2026-11-08 the driver takes the one-hit-kill soup first (`aid_ghoulybox_powerups_supersoups_onehitkill`, kind 8), and it passes: the script moves on at frame 18640 (Release) |
| `--scene3c6` | Boots chapter 3c scene 6 (chapter3c_scene6_1playcam, the kitchen) and plays it with the scene 7 driver: an energy boost soup first (powerup kind 3), then the exit without fighting (fight range 0). Passes: the script moves on to chapter3c_scene7_1playcam at frame 4812. Free play spawns `aid_ghoulyspawn_ghoulies_chapter3_kitchenfireimps` once Cooper enters region 1 and waits on nothing else, so fighting the imps only costs energy (10 to 12 faints in 35700 frames). The crash it hit came from `0x0001A2D0`'s fireball scale (see the 2026-10-28 checkpoint) |
| `--scene3c7` | Boots chapter 3c scene 7 (chapter3c_scene7_1playcam, the laundry) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0). Passes with no new translation: the unnamed section waits until no objTag_Actor_Warlock exists (condition opcode 16, then node type 0x11); "exit door revealed" follows at frame 31736 and the script moves on to chapter3c_scene8_1playcam at frame 32348. Most of the run is the warlock fight (class 0x3A, no faints). Without the box the driver walks to the shut exit and holds |
| `--scene3c8` | Boots chapter 3c scene 8 (chapter3c_scene8_1playcam, the servants' dormitory) and plays it with the scene 7 driver. Does not pass. "event - no weapons/don't get hit" queues no event step: it breaks `aid_ghoulybox_ghoulies_chapter3c_haunteddoordormitory`, sends the Jessie Clydes (class 0x1E) actorStrategyId_ForceAngry (script node 0x58, now translated), and waits for the haunted door (class 0x12, at -510, 20.7, 338) to wake (condition opcode 7, event 0xB4 tagged 0x12), which it does with a target within 60 (state 0x413). The driver now walks to the ghouly a pending opcode-7 wait names, fighting off others within 10, but the angry Jessie Clydes stop Cooper at about (300, -50) and he lands back near (200, -270); he faints and the event restarts about every 3500 frames |
| `--scene3c9` | Boots chapter 3c scene 9 (chapter3c_scene9_1playcam, the servants' bathroom) and plays it with the scene 7 driver, keeping away from the Grim Reaper (class 0xF) without fighting during "event - one type(random)/time limit". Does not pass. The event runs a 20 s step (type 4), after which the Reaper hunts Cooper, and an 80 s step (type 5) until "exit door opens, cursed mummy activated"; the idea is to kite the Reaper until the door opens and then escape. The driver's kiting lasts some 2400 to 5000 frames of the 4800 needed (once about 100 short), and the event restarts after each faint |
| `--scene3c10` | Boots chapter 3c scene 10 (chapter3c_scene10_1playcam, the attic furniture store) and plays it with the scene 7 driver, without fighting, after breaking the `aid_ghoulybox_powerups_supersoups_energyboost1` crate that blocks the way at (64.5, 108.7) (powerup kind 0: the errand ends at the first broken box). Does not pass: Cooper starts with 5 energy under the Grim Reaper's clock (some 75 s), and the walk to region 1, the exit volume at (-652, -282), shuttles between chests the grid takes for open floor near (-35, 20) and (50, 85) until the clock takes him (5 -> 0 at frame 5004). Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 13288 (Release) |
| `--scene3c11` | Boots chapter 3c scene 11 (chapter3c_scene11_1playcam, the Grand Hallway) and plays it with the scene 7 driver, shooting from 60 with a held weapon. Passes with no new translation: Cooper has 1 energy and the housekeeper's popcan gun (node 0x31); after `aid_ghoulybox_ghoulies_chapter3c_hallwayskeletons` and the skeleton drone cutscene he shoots the skeleton (class 0x34) from range, and the script moves on to chapter3c_scene12_1playcam at frame 2172. Punching from 16, the skeletons' one hit fainted him every 1300 frames |
| `--scene3c12` | Boots chapter 3c scene 12 (chapter3c_scene12_1playcam, the experiments chamber) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0) and not fighting during "free play". Does not pass, though no untranslated code is reached: the warlock event ("event - minimum damage") and free play run, region 1 (behind a glass partition) plays chapter3c_scene12_hkeepertakegun, and region 2 starts an untitled kill-8 event with the exit webbed over; Cooper faints with one kill left (RIP 1 and 4 energy at frame 7800, 0 at 7935), and the second attempt faints too |
| `--scene3c13` | Boots chapter 3c scene 13's book (chapter3c_scene13_2book; the scene order has no chapter3c_scene13_1playcam) and plays it with the scene 7 driver. Passes: four pages with the professor ("C'mon, take it then!", "What the..?!") and the script moves on to chapter3c_scene13_3playcam at frame 2815 |
| `--scene3c13b` | Boots chapter 3c scene 13 (chapter3c_scene13_3playcam, the laboratory) and plays it with the scene 7 driver, taking the one-hit-kill soup first (`aid_ghoulybox_powerups_supersoups_onehitkill`, powerup kind 8). Does not pass. Cooper has 30 energy; the event spares the skeletons (step type 7, node 0x21), finds the key behind a background part (step type 1) and then wants the key carrier among the bursting Jessie Clydes (step type 0xF); a Jessie (with 84 energy on the HUD) takes 20 a hit, and Cooper faints before the carrier falls |
| `--scene3c14` | Boots chapter 3c scene 14 (chapter3c_scene14_1playcam, the experiments chamber) and plays it with the scene 7 driver, taking the energy boost soup first (`aid_ghoulybox_powerups_supersoups_energyboost5`, kind 3) with a weapon reach of 60. Does not pass. A free-play section with twelve skeletons; Cooper has 10 energy, a skeleton takes 5 or 6 a hit, and he faints over and over before the script moves on. Since 2026-11-08 it runs invincible (a driver cheat that holds Cooper at 30 energy or more), and passes: the script moves on at frame 4022 (Release) |
| `--scene3c15` | Boots chapter 3c scene 15 (chapter3c_scene15_1playcam, the Grand Hallway) and plays it with the scene 7 driver. Passes: "PLAYER ENTERS", the theresthedoor cutscene, then free play, and the script moves on to chapter3c_scene15_2book at frame 1715 |
| `--scene3c15b` | Boots chapter3c_scene15_2book and turns its three pages. Passes: the script moves on to chapter3c_scene15_3playcam at frame 1902 |
| `--scene3c15c` | Boots chapter3c_scene15_3playcam (the hallway imps) and plays it with the scene 7 driver. Passes: the round haunted door's event (the impsenter cutscene, the hallway imps box, five imps against the clock) ends, and the script moves on to chapter4_scene1_1book |
| `--scene4_1` | Boots chapter4_scene1_1book (chapter 4's title and four pages). Passes: the script moves on to chapter4_scene1_1playcam |
| `--scene4_1b` | Boots chapter4_scene1_1playcam (the Baron's quarters) and plays it with the scene 7 driver, invincible (a driver cheat) and taking dropped weapons. Does not pass: the driver brings the Baron down to the phase-1 threshold with his own cane and the plane through all four landings and its spin, then cannot reach the plane for its last 40 energy within 108000 frames. Played by hand, the scene plays through |
| `--scene4_1c` | Boots chapter4_scene1_2book and turns its four pages. Passes: the script moves on to chapter5_scene1_1playcam at frame 2183 |
| `--scene5_1` | Boots chapter5_scene1_1playcam (the Grand Hallway, the countdown's start) and plays it with the scene 7 driver. Passes: chapter 5's title, "PLAYER ENTERS", free play with the 13-minute countdown on the scoreboard, Mr Ribs' page, and the script moves on to chapter5_scene2_1playcam |
| `--scene5_2` | Boots chapter5_scene2_1playcam (the bathroom) and plays it with the scene 7 driver. Passes: the dronein cutscene, Mr Ribs' first prisoner page, the Hunchback's event, and the script moves on to chapter5_scene3_1playcam at frame 10734 |
| `--scene5_3` | Boots chapter5_scene3_1playcam (the conservatory) and plays it with the scene 7 driver. Passes: the prisoner cutscene and Mr Ribs' page, the worm and spider waves, the fiddler box, the groundskeeper's water pistol page, and the script moves on to chapter5_scene4_1playcam at frame 16691 |
| `--scene5_4` | Boots chapter5_scene4_1playcam (Dunfiddlin Cottage) and plays it with the scene 7 driver. Passes: Mr Ribs' page, the haunted TV's event, the prisoner cutscene, and the script moves on to chapter5_scene5_1playcam at frame 12808 (Cooper ends on 5 of 50 energy) |
| `--scene5_5` | Boots chapter5_scene5_1playcam (the dining room) and plays it with the scene 7 driver. Does not pass: the pirates' event (from `aid_ghoulybox_ghoulies_chapter5_diningpirates`) includes a no-hits step (node 0x23, step type 9), so each hit fails it and summons the Grim Reaper; the driver retries it until the 36000-frame limit |
| `--scene5_6` | Boots chapter5_scene6_1playcam (the pantry) and plays it with the scene 7 driver. Passes: the prisoner cutscene, two pages, Mr Ribs at the door, and the script moves on to chapter5_scene7_1playcam at frame 3904 |
| `--scene5_7` | Boots chapter5_scene7_1playcam (the relics store) and plays it with the scene 7 driver. Passes: the prisoner cutscene, the mummy and zombie event (a cursed mummy box), two pages, and the script moves on to chapter5_scene8_1playcam at frame 8470 |
| `--scene5_8` | Boots chapter5_scene8_1playcam (the laundry) and plays it with the scene 7 driver, breaking every `aid_ghoulybox_single_skeleton` washing machine in turn once the event runs. Does not pass: the event (a next-different step and node 0x88's kill-three-skeletons step) needs skeletons from the washing machines, and Cooper's kicks do not reach them from the nearest open floor |
| `--scene5_9` | Boots chapter5_scene9_1playcam (the walled garden) and plays it with the scene 7 driver. Does not pass: the event (no points, type 0xC; next-different Jessie Clydes, type 8; spare the skeletons, type 7; four kills, type 0xE) fails on Cooper's fights, the Grim Reaper takes his energy, and the scene restarts until the 36000-frame limit |
| `--scene5_10` | Boots chapter5_scene10_1playcam (the TV room) and plays it with the scene 7 driver. Passes: the haunted-object event (after two faints), four pages, Mr Ribs leaving, and the script moves on to chapter5_scene11_1playcam at frame 19888 |
| `--scene5_11` | Boots chapter5_scene11_1playcam (the lower corridors) and plays it with the scene 7 driver. Passes: a page, the haunted pictures, and the script moves on to chapter5_scene12_1playcam at frame 3937 |
| `--scene5_12` | Boots chapter5_scene12_1playcam (the Grand Hallway, the countdown's end) and plays it with the scene 7 driver. Passes: two pages, script type 0x9A's gold finish of the countdown, and the script moves on to chapter5_scene13_1playcam at frame 4551 |
| `--scene5_13` | Boots chapter5_scene13_1playcam (the front gate, chapter 5's last play scene) and plays it with the scene 7 driver. Passes: Cooper walks straight ahead into the exit (the mask-1 volume at 223, -84) and the script moves on to chapter5_scene13_1book at frame 1679 |
| `--scene5_13b` | Boots chapter5_scene13_1book. Passes: two pages, and the script moves on to chapter5_scene13_2playcam |
| `--scene5_13c` | Boots chapter5_scene13_2playcam (the front gate, played as the cook against the imps) and plays it with the scene 7 driver. Passes: the script moves on to chapter5_scene13_3book at frame 29482 |
| `--scene5_13d` | Boots chapter5_scene13_3book. Passes: seven pages, and the script moves on to finalscore_finalscore |
| `--scenefinal` | Boots finalscore_finalscore and plays the final score (factory type 0x99) to its end: the cook's intro line, the four scores, the Super Scary Score and its OK prompt. The script then moves on to aid_script_ghoulies_credits_final |
| `--scenecredits` | Boots credits_final and plays its 18 pages; the script moves on to aid_script_ghoulies_attractsequence_final1 at frame 17051 |
| `--scene7` | Boots scene 7 (chapter1_scene7_1playcam, the east wing's cinema): its transition ends (no chapter title), A closes each page once it has been up a second, and Cooper fights any live ghouly within 50 units (not the prisoners, tags 0x26..0x30). Otherwise he walks into the region a live PlayerEntersRegion (condition 8) names or, while a section holds at its end, into the exit volume, steered along the navigation grid (`nav_waypoint`, a test-side search over `g_nav_grids`). Titles, objects, Cooper and the section gate are logged; the volumes and pending conditions on a failure. The same driver then plays scene 7's book (`scene7_2`, its pages) and scene 7_3 (`scene7_3`) to their ends; scene 7_3 restarts each time Cooper loses to the TV (three times in the 2026-10-21 run) and moves on to scene 8 at frame 7786 |
| `--scene3` | Boots scene 3 (chapter1_scene3_1playcam, "Player Enters Room"): the transition (row 1, no title) ends, the walk-in cutscene (script type 0x53) and the door cutscene play, A closes Crivens' two pages, Cooper spawns at the player marker, "Free Play" holds on its conditions with the player camera; then Cooper walks to the skeletons, which wake, fall apart into bones, and he lifts and throws the trophy, collects the rare book and walks out through the open door; the script moves on to scene 4 |

Headless captures: `gbtg --headless --frames N --scene chapter1_scene1_2playcam
--width 640 --height 480 --screenshot FILE.bmp`.

The player modes tick 1/60 s, the step their checkpoints were taken at. A trailing `--retail-step`
(`--player-fight --retail-step`) ticks 1/30 s, the step retail and `gbtg` use. The drivers' waits and
captures count time, not ticks, so the captures show the same moments at both steps. Headless runs always
step 1/30 s, so frame N is N/30 s; the references in `cache/player_ref/` were taken at 1/60 s, so
`opening_180` there is frame 90 now.

## Known traps

- **Headless runs read the real keyboard.** Keys held on the machine (WASD,
  IJKL, A) reach the pad boundary; keep hands off during captures.
- **Bash heredocs mangle backslash escapes.** Python or shell heredocs turn
  `\n` in C++ string literals into real newlines. Use Write/Edit for literals.
- **Release `gbtg` writes a pdb** (`/Zi`, `/DEBUG:FULL` with `/OPT:REF` and
  `/OPT:ICF`), so a player's crash report names functions and lines: an
  `std::out_of_range` prints its stack where it is thrown, and a `gbtg::Error`
  that ends the run prints where it was thrown after the `error:` line
  (`runtime/throw_trace.cpp`). The Release test executable still has no debug
  info. For a test crash stack, use the Debug or RelWithDebInfo build
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
  names `0x003390E4` look one entry off from what the rows do. A type's parent is the word just before its getter
  (`DAT_0030D41C`, the previous row's third word), and its real name is the previous row's: type 0x2D (getter
  `0x000761A0`) is entityBodyZombie, not the entityFxCustomElectric its row names. For example, the imp's
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

Measured with `--player-fight`. The whole opening scene now plays to its end: the fight passes "Kill All", Cooper
picks up the cook's chair, the second wave comes through the doorway and its "Kill All" passes, the exit door opens,
"Free Play" brings the butler in (`aid_ghoulybox_single_crivens`, strategy type 0x3B), Cooper walks into region 2
for the butler's "leave room" pages (the butler takes the chair), then out through the far door. The exit volume
queues event 0xB0, the script ends, the book transition shows the chapter title and the next scene
(`aid_script_ghoulies_chapter1_scene2_1viewcam`, "Trophy Heads Super Scary Shock") starts. `--scene6` plays that
scene to its end, through the trophy heads and their Super Scary challenges, and the script moves on to
`aid_script_ghoulies_chapter1_scene3_1playcam` (see the scene 6 checkpoint). `--scene3` plays that scene's start,
walk-in and "Free Play" natively. Driven at the skeletons, they walk up and fight Cooper and fall apart into bones
when they die. Cooper then lifts and throws the trophy, collects the rare book and leaves through the open door, and
the script moves on to scene 4 (see the scene 3 checkpoints). `--scene4` and `--scene5` play scenes 4 and 5 to their
ends. `--scene6-1` plays scene 6 (`aid_script_ghoulies_chapter1_scene6_1playcam`, the Lower Corridors) from its
walk-in through the telephone's scare and Crivens' scare page into "Ghouly Generator Introduction", where the ghouly
setups spawn spiders that hunt and leap at Cooper. Cooper fights his way down the corridor and out through its south
doorway, and the script moves on to `aid_script_ghoulies_chapter1_scene7_1playcam` (see the 2026-10-21 checkpoints).
`--scene7` plays that scene (the east wing's cinema): up the stairs to the projection booth, where the film-reel
prisoner's page plays, down into the cinema to fight its skeletons and imps, and out. The script moves on to
`aid_script_ghoulies_chapter1_scene7_2book` ("Partner Down the Trapdoor"). The book's three pages play and the
script moves on to `aid_script_ghoulies_chapter1_scene7_3playcam`. That scene plays natively to its end: the haunted
TV's ghoul intro, its electric attack that shocks Cooper, the ghoulies it summons through its beam, and the exit. The
script moves on to `aid_script_ghoulies_chapter1_scene8_1playcam` (the gaming room). `--scene8` plays that scene to
its end: Cooper walks in, the door shuts behind him, the chairs flip off their spots and "Kill All" asks him to
break every haunted chair. Its event completes, the exit door opens, Cooper leaves through it and the script moves
on to `aid_script_ghoulies_chapter1_scene9_1playcam`. `--scene9` plays that scene (the basement cellar) to its end:
Cooper walks the long way round to the mummy's room, the mummy comes through its door, Crivens hands over the special
candle, and its fire burns the mummy; the exit door opens and the script moves on to
`aid_script_ghoulies_chapter1_scene10_1playcam` ("Relics Store"). `--scene10` plays that scene to its end: the walk
cutscene carries the lit relic candle, the mummy's Super Scary challenge plays, "Event: Kill All" is won with the
candle and the script moves on to `aid_script_ghoulies_chapter1_scene11_2book`. `--scene11` plays that book and
the archives after it: the ugly girl's Super Scary shock is won, she joins Cooper for "Ugly Partner Kickabout", the
skeletons come in and are beaten, and the script moves on to `aid_script_ghoulies_chapter1_scene12_1playcam`.
`--scene12` plays the wine cellar: the spiders on the walls are fought, the haunted door by the exit wakes, knocks
Cooper flying and is beaten, and the script moves on to `aid_script_ghoulies_chapter2a_scene1_1playcam` (chapter 2a's
kitchen). `--scene2a1` plays that scene: Cooper climbs the stairwell in Free Play while the fire imps fly about, and
the script moves on to `aid_script_ghoulies_chapter2a_scene1_2book`, which nothing drives yet.

| Path | Stop | What it needs |
|---|---|---|
| The brew offer after a death | not reached | The death state's state 4 (`0x0008F290` with the unlock bit 3, enough brews and a chapter below 8): "intros_brewoffer" and its answer; the opening scene's runs have no brews |
| Script actions | legacy | `0x00126E10`'s other cases (0x4D, 0x4E, 0x62, 0x67, 0x6E, 0x73, 0x8E, 0x9D) and `0x00127BC0`'s cases still compute values and skip their callees; this scene does not reach them |
| Present (the snapshot) | renders nothing | `0x00116530` records its draws but the texture stays empty |
| Scene 3's Free Play branches (`--scene3`) | native | Free Play's node 0x51 on the skeletons sends message 1 over the native DAT_00511B38 walk (2026-10-24) |
| Script condition nodes | partial | `0x00125450` registers types 0x3D..0x3F, 0x40..0x44, 0x47, 0x48, 0x5A, 0x5B, 0x66, 0x68, 0x6A, 0x6C, 0x72, 0x92, 0x93, 0x94, 0x95 and 0x99; retail also takes 0x65, 0x75, 0x8B, 0x8C and 0xA3, which raise |
| Script event steps | native | Rechecked 2026-11-08: the builders (script types 0x19..0x27, 0x50, 0x5F, 0x87 and 0x88) and the step runners of the `0x003039CC` table (`0x000D0B40`) are translated, up to type 0x12 |
| The Grim Reaper's dizziness | native | `0x000AD600` turns him dizzy (0x3EA) while DAT_00512170 is set: the DizzyReaper powerup row (kind 2), which `0x00123E70` sets |
| Blob shadows | off | DAT_0050EF18 is never set, so 0x00119A60 hands out no shadow (the reaper's included) and the draw 0x00119AD0 is not reached |
| The spider web in captures | unchecked | The web (type 0x1B) lands and snares by its messages; no capture has caught one yet |
| Background cloths | partial | 0x001153A0 builds the background model's parts at scene load: kinds 1, 2 and 8 (flags, 0x00050880, with the player and actor push hook 0x00114910), kind 3 (no cloth), kind 4 (collision planes) and kind 5 (curtain pairs) are translated; kinds 6 (0x000509E0) and 7 (0x00050940) raise, as do the hooks 0x001145F0 and 0x00114580, the specular shading 0x0004BA90, the second strip draw 0x0005D710 and 0x00064820's part pass. The 2026-10-23 full regression found only kinds 2 and 5 in use (scenes 3, 4, 5, 6 and 8 have kind 2) |
| Script type 0x16 | native | Waits for `0x0004EFE0` while DAT_00512470 is set, then releases the cutscenes, as retail does |
| Chapter 2a scene 3 (`aid_script_ghoulies_chapter2a_scene3_1playcam`) | not driven | `--scene2a2` stops once the script moves on to it |
| The coward zombie on the plates | unchecked | Hurt, scene 2a2's zombie climbs onto a plate stack and stays there without moving (y about 8.6) until Cooper comes within reach; not compared with retail |
| The scene 2a1 stairwell walk | slow | The driver finds no grid route from the stairwell floor (y -40) and walks "straight" into the wall until about frame 8000 |
| A death during an event | as dumped | `0x00134AE0` sets DAT_00430D0C with the restart, so the reloaded script passes its handlers through (0x00128A00's third argument) and skips the event; the kitchen's exit door then stays shut because the event's end (state 4) never opens it. Not compared with retail |
| The camera near scene 9's door 8 | unchecked | From frame ~11400 of a long scene 9 run the camera looked down on the cellar from outside its walls; not compared with retail |

## Acceptance

- `--player-fight` drives Cooper with the sticks, defeats every imp, requires
  the script to pass "Kill All", plays the rest of the scene to its end and the
  next scene's start, and saves readbacks along the way
  (`cache/player_fight_N.bmp`, `cache/player_after_N.bmp`). `--player-death`
  requires the death and the restart. Both also run with `--retail-step`.
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

The dated log of this work, newest first, is in
[Player scene checkpoints](PLAYER_SCENE_CHECKPOINTS.md). Add new checkpoints there.
