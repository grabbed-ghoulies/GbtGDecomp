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
| `--player-fight` | The opening flow, then Cooper fights the imps (see Acceptance), an imp finished from its hit reaction (0x7E straight to 0 health), the HUD heart in fight captures and not behind the butler's page, the chair's section (the ObjectPickedUp record with the chair's tag, Cooper in region 1, Cooper walks to the chair and A picks it up; event 0xAD with the chair's tag is queued and the section ends before its 5 s timeout; `cache/player_chair_check.bmp`, `player_chair_pickup.bmp`, `player_chair_held.bmp`), then the play-on, which fails if the opening script is still running when it gives up |
| `--player-death` | The opening flow, then Cooper stands still in the first fight: the imps empty his health, the death state 0x69 plays his dying animations and the fail stream, starts the transition (row 2) and restarts the scene ("Free Play (Fighting Intro)"), whose script then moves past its player wait |
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
| `--scene2b3` | Boots chapter 2b scene 3 (chapter2b_scene3_2playcam, the wood shed) and plays its "Free Play" with the scene 7 driver at any fight range; the scene ends at the exit volume (-166, 636). Does not pass yet: the flying imps (factory type 0x48) and the Medusa (class 0x1F) by the exit wear Cooper down, he dies and the scene restarts (2026-10-25) |
| `--scene2b4` | Boots chapter 2b scene 4 (chapter2b_scene4_1playcam, the farmyard) and plays it with the scene 7 driver at any fight range. Does not pass yet: "event - kill all/time limit" (pirates, ninja imps, skeletons with eggs, flying imps and zombies from ghoulyboxes) runs out of time before the driver kills them all, the Grim Reaper comes and the event restarts (2026-10-25) |
| `--scene2b4b` | Boots chapter 2b scene 4's book (chapter2b_scene4_2book): the groundskeeper's pages (Ol' Edna, the garlic gun). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene5_1playcam` (frame 1713 in the 2026-10-25 run) |
| `--scene2b5` | Boots chapter 2b scene 5 (chapter2b_scene5_1playcam, the chicken shed) and plays it with the scene 7 driver at any fight range: the Baron's page, "EVENT - TIME LIMIT/SURVIVE" against vampire chickens with the blunderbuss, the event's completion and the exit door opening. Does not pass yet: in the last "FREE PLAY" Cooper dies fighting by the exit, the scene restarts from its first page, and the run ends in the second survive event (2026-10-25) |
| `--scene2b6` | Boots chapter 2b scene 6's book page (chapter2b_scene6_1book) and plays it until the script moves on to `aid_script_ghoulies_chapter2b_scene6_2playcam` (frame 2226 in the 2026-10-26 run) |
| `--scene2b6b` | Boots chapter 2b scene 6 (chapter2b_scene6_2playcam, the farmyard) and plays it with the scene 7 driver: the driver chases the class a pending PlayerHitGhouly names (Mr Ribs, tag 0x31, by the barn door), leaves each penned vampire chicken after a minute out of reach, and walks into region 1. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene7_1playcam` (frame 18716 in the 2026-10-26 run) |
| `--scene2b7` | Boots chapter 2b scene 7 (chapter2b_scene7_1playcam, the kiln rooms) and plays it with the scene 7 driver: the walk-in, "EXIT DOOR CLOSES", "EVENT - REQUIRED NUMBER/FIND THE KEY" with the furnace burning skeletons, then "FREE PLAY", where the driver leaves the workshop spawn's mummies and walks out of the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene8_1playcam` (frame 12242 in the 2026-10-26 run) |
| `--scene2b8` | Boots chapter 2b scene 8 (chapter2b_scene8_1playcam, the lower corridors) and plays it with the scene 7 driver: the giant hand grabs Cooper (its Super Scary challenge, mode 2), the driver leaves Mr Ribs, who takes no hits, and hits a haunted chair (PlayerHitGhouly, tag 0x10). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene9_1playcam` (frame 4791 in the 2026-10-26 run) |
| `--scene2b9` | Boots chapter 2b scene 9 (chapter2b_scene9_1playcam, the west wing pantry) and plays it with the scene 7 driver, calm in FREE PLAY: the haunted coats' event (EVENT: ONE TYPE - HOODED FIGURE) and the haunted doors' (ONE TYPE (HAUNTED DOOR) / DON'T KILL (IMP)). Cooper faints once in the first door event in the 2026-10-26 run and the room restarts. Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene10_1playcam` (frame 7373 in the 2026-10-26 rerun) |
| `--scene2b10` | Boots chapter 2b scene 10 (chapter2b_scene10_1playcam, the kitchen) and plays it with the scene 7 driver, calm in FREE PLAY: Cooper fights Mr Ribs until his energy is gone (scriptConditionId_ActorHasNoEnergy). Passes when the script moves on to `aid_script_ghoulies_chapter2b_scene10_2book` (frame 4551 in the 2026-10-26 run) |
| `--scene2b10b` | Boots chapter 2b scene 10's book (chapter2b_scene10_2book, "Fire extinguisher weapon Intro") and plays its four pages (A closes each). Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene1_1playcam` (frame 2716 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the "Phew! Mr. Ribs okay" page (frame 1500) and the goop transition over the last page (2400) |
| `--scene2c1` | Boots chapter 2c scene 1 (chapter2c_scene1_1playcam) and plays it with the scene 7 driver, calm in FREE PLAY: the haunted paintings' introduction, then Cooper walks to the exit. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene2_1playcam` (frame 1480 in the 2026-10-26 run) |
| `--scene2c2` | Boots chapter 2c scene 2 (chapter2c_scene2_1playcam, the cloakroom). In FREE PLAY the driver walks to the bg-break setup holding `aid_ghoulybox_powerups_supersoups_invisibility` (the suitcase, part 7 at 219, -77), breaks it, drinks the invisibility soup (powerup kind 9) and walks out unseen: no coat or picture wakes, so the exit stays open. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene3_1playcam` (frame 2092 in the 2026-10-26 run) |
| `--scene2c3` | Boots chapter 2c scene 3 (chapter2c_scene3_1playcam) and plays it with the scene 7 driver, calm in FREE PLAY: the event page ("Keep your eye on the cross"), then "EVENT - KILL ALL/NEXT ONE DIFFERENT" against zombies (tag 0x3C) and skeletons (0x34) with no faints. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene4_1book` (frame 4388 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the event page (frame 1200) and Cooper fighting a zombie (2400) |
| `--scene2c4` | Boots chapter 2c scene 4's book (chapter2c_scene4_1book) and plays its four pages. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene4_2playcam` (frame 2421 in the 2026-10-26 run); it needed no new translation. Checked in the captures: the comic panels with Cooper holding a new weapon (frame 1800) |
| `--scene2c4b` | Boots chapter 2c scene 4_2 (chapter2c_scene4_2playcam, the music room) and plays it with the scene 7 driver: the untitled fight against mummies, skeletons and zombies with the popcan gun, and an SSS door (type 0x20) that springs a Super Scary challenge (the dragon scare) near the end. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene5_1playcam` (frame 9621 in the 2026-10-26 run) |
| `--scene2c5` | Boots chapter 2c scene 5 (chapter2c_scene5_1playcam, the walled garden) and plays it with the scene 7 driver. While a PlayerHitGhouly watches (the pirate's skeletons dig for the dungweed), Cooper fights nothing and walks to the nearest record of the dungweed's region 1; the angry baron's fight follows. **Fails**: Cooper finds the dungweed every time (page 3, frame 2478) but faints in the fight, where the pirate's shovel takes all five hearts in one hit and the flying imp's dive three, and the scene restarts until frame 24000 |
| `--scene2c6` | Boots chapter 2c scene 6 (chapter2c_scene6_1playcam, the study) and plays it with the scene 7 driver: "EVENT - FIND THE KEY" arms one of three bg-break keys and the driver punches its part open; "EXIT DOOR OPENS" lets the ninja imps in and FREE PLAY is fought, Jessie Clyde (types 0x29 and 0x4A) transforming as Cooper comes near. Passes when the script moves on to `aid_script_ghoulies_chapter2c_scene7_1playcam` (frame 6040 in the 2026-10-27 run). Checked in the captures: the sleeping Jessie Clyde (frame 2400), her transformation (3600) and her fight with Cooper (4800) |
| `--scene2c7` | Boots chapter 2c scene 7 (chapter2c_scene7_1playcam) and plays it with the scene 7 driver: "EVENT - NUMBER" (generic ghoulies, type 0x3C), a cursed mummy section (type 0x4D) and "EVENT - ONE TYPE". Does not pass yet: the cursed mummy's slab whacks make Cooper faint once, and the replayed scene reaches its last FREE PLAY at frame 35772 of the 36000 the driver allows (2026-10-27). Checked in the captures: the curse's glyphs around Cooper (frames 9600, 10200) and the mummy's raised slab (11100) |
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
| `--scene3a5` | Boots chapter 3a scene 5 (chapter3a_scene5_1playcam, the two-floor hall) and plays it with the scene 7 driver, calm in "free play", toward the exit on the upper floor at (160, 85, -239). Does not pass: Cooper starts with 5 energy, and the two pirates by the top of the stairs (around (141, -146) and (142, -113)) knock him out with one 6-energy hit as he passes, so the scene restarts until the frame limit. With fighting allowed the pirates and skeletons win too. Not settled: whether the 6-energy pirate hit and the 5-energy start are retail's. Checked in the captures: Cooper knocked down by the skeletons by the brambled door (1200), and the two pirates raising their chests at Cooper (14400) |
| `--scene3a6` | Boots chapter 3a scene 6 (chapter3a_scene6_1playcam, the mummies' corridor) and plays it with the scene 7 driver. Cooper starts with 1 energy, so each hit from the nine mummies (class 0x22) or the haunted pictures (class 0x14) ends the attempt; the scene restarts 10 times and moves on to `aid_script_ghoulies_chapter3a_scene7_1playcam` at frame 29820 in the 2026-10-28 run. Not settled: whether the 1-energy start is retail's. Checked in the captures: Cooper and a haunted picture in the corridor with the mummies beyond (600), and Cooper inside the ring of mummies by the door (1800) |
| `--scene3a7` | Boots chapter 3a scene 7 (chapter3a_scene7_1playcam) and plays it with the scene 7 driver: free play, then "event - one type/time limit", then free play to the exit. Cooper faints once (frame 11274) and the replay finishes; the script moves on to `aid_script_ghoulies_chapter3a_scene8_1playcam` at frame 21899 in the 2026-10-28 run. Checked in the captures: Cooper running past the cupboard doors (3600) and breaking through a folding screen during the event (6000) |
| `--scene3a8` | Boots chapter 3a scene 8 (chapter3a_scene8_1playcam, the schoolroom) and plays it with the scene 7 driver. Does not pass: "EVENT - NO DAMAGE" means no damage to the room. Its step type 12 (`0x000D0140`) fails once DAT_00545980, the points from broken background parts, is nonzero. Any box Cooper punches open (message 0x15, category 1, the invisibility soups included) or furniture a ghouly he hits smashes into (category 2, the hitter at body +0x270) fails the event, and the Grim Reaper then takes Cooper's life on touch (10 -> 0). The driver fights next to the furniture, so every attempt fails. Driver changes: with several boxes of one name it goes to the nearest, and a broken box's powerup comes before the next box. Checked in the captures: the Medusa's gaze and the imps by the desks (2100), and the Grim Reaper striking Cooper among the bookshelves (2400) |
| `--scene3a9` | Boots chapter 3a scene 9 (chapter3a_scene9_1playcam, the infirmary) and plays it with the scene 7 driver. Reaches no untranslated code but does not pass. In FREE PLAY the patients take 4 energy a hit. "EXIT DOOR CLOSES" puts Cooper at 183.7, -35.6, and "EVENT - SURVIVE/FIND THE KEY" starts a timer (0:57 at frame 3300) with the Medusa and the imps; Cooper faints after about 1000 frames of it every time and the scene restarts in FREE PLAY. The driver also stalls on soups and boxes it cannot reach: the soup near 280, 67, the free kind 4 powerup at -60, -150 and the `energyboost10` box at 242, 148. A run with that box as `powerup_box` (kind 4) still fainted eight times. Checked in the captures: the infirmary beds and patients (600), the exit door (1200), and the Medusa and the imps under the event timer (1800, 3300) |
| `--scene3a10` | Boots chapter 3a scene 10 (chapter3a_scene10_2playcam, the village; no `_1` script exists) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire body (type 0x2C) and strategy (type 0x54) are installed, but does not pass. The club-wielding class 0x24 ghoulies hit for 5 and knock Cooper down, so he faints four times in 36000 frames. The end conditions wait on two PlayerEntersRegion steps (words 0x3EC and 0x494). Checked in the captures: the village path, the cottages, and a class 0x24 ghouly over a knocked-down Cooper (5100, 12600) |
| `--scene3a11` | Boots chapter 3a scene 11 (chapter3a_scene11_1playcam, the vegetable garden) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire's knock-back hit (`0x00020690`) and front hit (`0x00022FB0`) are translated, but does not pass. "EXIT DOOR OPENS" comes about 400 frames in, and the class 0x38 ghoulies then wear Cooper down 4 energy a hit. He faints 18 times in 36000 frames, and the scene restarts each time. Checked in the captures: the garden and a ghouly at the plants (1200), and the vampire out of his open coffin at Cooper, with bats (3000) |
| `--scene3a12` | Boots chapter 3a scene 12 (chapter3a_scene12_1playcam, the greenhouse) and plays it with the scene 7 driver. Reaches no untranslated code once the vampire's bite (type 0x61) and the grabbed body's state (type 0x71) are installed, but does not pass. "EVENT - KILL ALL/TIME LIMIT" starts about 1550 frames in. The vampire's bites take one energy a hold, and the class 0x34 ghoulies wear Cooper down until he faints. He faints 3 times in 36000 frames, and the event restarts each time. Checked in the captures: the rhyme paper (1500), Cooper among the skeletons (4500), and the vampire holding Cooper up out of his coffin (19500) |
| `--scene3b1` | Boots chapter 3b scene 1 (chapter3b_scene1_1playcam) and plays it with the scene 7 driver. Passes: free play from frame 358, the script moves on to chapter3b_scene2_1playcam at frame 7924, and Cooper's energy stays at 10. Checked in the captures: Cooper fighting class 0x1B ghoulies among the plants (1200, 4800) and walking to the exit door (7800) |
| `--scene3b2` | Boots chapter 3b scene 2 (chapter3b_scene2_1playcam, the garden) and plays it with the scene 7 driver at a fight range of 50. Reaches no untranslated code once the worm's body (type 0x2B), its strategy (type 0x57) and the explode state (type 0x68) are installed, but does not pass. Cooper enters region 3, but from frame 8760 he is held at the garden gate (about -257, 75) and never reaches region 1. The driver logs a body that enters type 0x68 and captures every 10 frames after (`scene3b2_explode_<frame>.bmp`). Checked in the captures: the worm up and hitting Cooper (7200 in an earlier run), its explosion at frame 7991 (the blast at 8040, Cooper thrown back, 16 to 9 energy), the worm gone (8400) and Cooper at the gate (20100) |
| `--scene3b3` | Boots chapter 3b scene 3 (chapter3b_scene3_1playcam, Dunfiddlin Cottage) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0). Reaches no untranslated code but does not pass: after "free play (find warlock)" (frame 1409) the warlock reappears downstairs, and the driver's 15-unit height rule keeps Cooper upstairs by the shut exit. Checked in the captures: the warlock fought in the hallway (1200) and his intro card (1500) |
| `--scene3b4` | Boots chapter 3b scene 4 (chapter3b_scene4_2playcam, the workshop) and plays it with the scene 7 driver: an energy boost soup first (powerup kind 3, fetched again after a faint), fighting only during "event - survive/no weapons". Passes: Cooper faints in the first event, survives the second (90 s, ending at frame 15093), and the script moves on to chapter3b_scene5_2playcam at frame 15585. Checked in the captures: the 0:57 clock and the skeletons (11400) and Cooper leaving (15300) |
| `--scene3b5` | Boots chapter 3b scene 5 (chapter3b_scene5_2playcam, the ogres' lane) and plays it with the scene 7 driver, fighting nothing (the class 0x1E ogres sleep) and taking an energy boost soup first. Passes once the grab and throw state (type 0x72) is installed: Cooper is thrown three times and the script moves on to chapter3b_scene6_1playcam at frame 4933. Checked in the captures: the ogres asleep and over Cooper (1800, 2100) |
| `--scene3b6` | Boots chapter 3b scene 6 (chapter3b_scene6_1playcam, the foundry) and plays it with the scene 7 driver. Passes: the bg-break key behind the barrel comes out at frame 1457 (the driver punches from 24 once on the key's approach cell), Cooper fights the class 0x12 haunted doors in the time-limit event, and the script moves on to chapter3b_scene7_1playcam at frame 5947. Checked in the captures: the key coming out (1500) and a haunted door fight (3600) |
| `--scene3b7` | Boots chapter 3b scene 7 (chapter3b_scene7_1playcam, the barn) and plays it with the scene 7 driver. Reaches no untranslated code once bundle lookups ignore case (the skeleton's ..._noegganims_turnStep is listed as ..._turnstep), but does not pass: Cooper is held at a pen wall at about (150, 175) and never reaches region 10 at (942, 73). Checked in the captures: Cooper with an extinguisher at the wall (3000) |
| `--scene3c1` | Boots chapter 3c scene 1 (chapter3c_scene1_1playcam, the foundry) and plays it with the scene 7 driver. Reaches no untranslated code once the script's event steps 0x25 (type 8) and 0x5F (type 0xE) are queued and run, but does not pass: the RIP count stays at 6 (no mummy dies to punches) and the cursed mummy wears Cooper down until he faints. Checked in the captures: the challenge HUD and the furnace (4500, 6000) |
| `--scene3c2` | Boots chapter 3c scene 2 (chapter3c_scene2_1playcam, the farmyard) and plays it with the scene 7 driver. Passes: the script moves on to chapter3c_scene3_2playcam at frame 10748, Cooper keeping all 5 energy. Checked in the captures: the farmyard (3000, 9000) |
| `--scene3c3` | Boots chapter 3c scene 3 (chapter3c_scene3_2playcam) and plays it with the scene 7 driver at a fight range of 50. Reaches no untranslated code but does not pass: region 1 is entered, but Cooper never gets within 50 of door 24 (closest 136) and is twice returned to the start, seemingly caught by the groundskeeper. Checked in the captures: a gassing ghouly (4500) and Cooper caught (12000) |
| `--scene3c4` | Boots chapter 3c scene 4 (chapter3c_scene4_1playcam) and plays it with the scene 7 driver. Passes: "event - ghouly find the key" and the script moves on to chapter3c_scene5_1playcam at frame 7184. Checked in the captures: Cooper and the groundskeeper (1200) |
| `--scene3c5` | Boots chapter 3c scene 5 (chapter3c_scene5_1playcam) and plays it with the scene 7 driver. Passes: "event - kill all/limited attacks" is won and the script moves on to chapter3c_scene6_1playcam at frame 6513. Checked in the captures: the fight with the event HUD (2400) |
| `--scene3c6` | Boots chapter 3c scene 6 (chapter3c_scene6_1playcam, the kitchen) and plays it with the scene 7 driver: an energy boost soup first (powerup kind 3), then the exit without fighting (fight range 0). Passes: the script moves on to chapter3c_scene7_1playcam at frame 4812. Free play spawns `aid_ghoulyspawn_ghoulies_chapter3_kitchenfireimps` once Cooper enters region 1 and waits on nothing else, so fighting the imps only costs energy (10 to 12 faints in 35700 frames). The crash it hit came from `0x0001A2D0`'s fireball scale (see the 2026-10-28 checkpoint) |
| `--scene3c7` | Boots chapter 3c scene 7 (chapter3c_scene7_1playcam, the laundry) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0). Passes with no new translation: the unnamed section waits until no objTag_Actor_Warlock exists (condition opcode 16, then node type 0x11); "exit door revealed" follows at frame 31736 and the script moves on to chapter3c_scene8_1playcam at frame 32348. Most of the run is the warlock fight (class 0x3A, no faints). Without the box the driver walks to the shut exit and holds |
| `--scene3c8` | Boots chapter 3c scene 8 (chapter3c_scene8_1playcam, the servants' dormitory) and plays it with the scene 7 driver. Does not pass. "event - no weapons/don't get hit" queues no event step: it breaks `aid_ghoulybox_ghoulies_chapter3c_haunteddoordormitory`, sends the Jessie Clydes (class 0x1E) actorStrategyId_ForceAngry (script node 0x58, now translated), and waits for the haunted door (class 0x12, at -510, 20.7, 338) to wake (condition opcode 7, event 0xB4 tagged 0x12), which it does with a target within 60 (state 0x413). The driver now walks to the ghouly a pending opcode-7 wait names, fighting off others within 10, but the angry Jessie Clydes stop Cooper at about (300, -50) and he lands back near (200, -270); he faints and the event restarts about every 3500 frames |
| `--scene3c9` | Boots chapter 3c scene 9 (chapter3c_scene9_1playcam, the servants' bathroom) and plays it with the scene 7 driver, keeping away from the Grim Reaper (class 0xF) without fighting during "event - one type(random)/time limit". Does not pass. The event runs a 20 s step (type 4), after which the Reaper hunts Cooper, and an 80 s step (type 5) until "exit door opens, cursed mummy activated"; the idea is to kite the Reaper until the door opens and then escape. The driver's kiting lasts some 2400 to 5000 frames of the 4800 needed (once about 100 short), and the event restarts after each faint |
| `--scene3c10` | Boots chapter 3c scene 10 (chapter3c_scene10_1playcam, the attic furniture store) and plays it with the scene 7 driver, without fighting, after breaking the `aid_ghoulybox_powerups_supersoups_energyboost1` crate that blocks the way at (64.5, 108.7) (powerup kind 0: the errand ends at the first broken box). Does not pass: Cooper starts with 5 energy under the Grim Reaper's clock (some 75 s), and the walk to region 1, the exit volume at (-652, -282), shuttles between chests the grid takes for open floor near (-35, 20) and (50, 85) until the clock takes him (5 -> 0 at frame 5004) |
| `--scene3c11` | Boots chapter 3c scene 11 (chapter3c_scene11_1playcam, the Grand Hallway) and plays it with the scene 7 driver, shooting from 60 with a held weapon. Passes with no new translation: Cooper has 1 energy and the housekeeper's popcan gun (node 0x31); after `aid_ghoulybox_ghoulies_chapter3c_hallwayskeletons` and the skeleton drone cutscene he shoots the skeleton (class 0x34) from range, and the script moves on to chapter3c_scene12_1playcam at frame 2172. Punching from 16, the skeletons' one hit fainted him every 1300 frames |
| `--scene3c12` | Boots chapter 3c scene 12 (chapter3c_scene12_1playcam, the experiments chamber) and plays it with the scene 7 driver, breaking `aid_ghoulybox_single_warlock` first (powerup kind 0) and not fighting during "free play". Does not pass, though no untranslated code is reached: the warlock event ("event - minimum damage") and free play run, region 1 (behind a glass partition) plays chapter3c_scene12_hkeepertakegun, and region 2 starts an untitled kill-8 event with the exit webbed over; Cooper faints with one kill left (RIP 1 and 4 energy at frame 7800, 0 at 7935), and the second attempt faints too |
| `--scene3c13` | Boots chapter 3c scene 13's book (chapter3c_scene13_2book; the scene order has no chapter3c_scene13_1playcam) and plays it with the scene 7 driver. Passes: four pages with the professor ("C'mon, take it then!", "What the..?!") and the script moves on to chapter3c_scene13_3playcam at frame 2815 |
| `--scene3c13b` | Boots chapter 3c scene 13 (chapter3c_scene13_3playcam, the laboratory) and plays it with the scene 7 driver, taking the one-hit-kill soup first (`aid_ghoulybox_powerups_supersoups_onehitkill`, powerup kind 8). Does not pass. Cooper has 30 energy; the event spares the skeletons (step type 7, node 0x21), finds the key behind a background part (step type 1) and then wants the key carrier among the bursting Jessie Clydes (step type 0xF); a Jessie (with 84 energy on the HUD) takes 20 a hit, and Cooper faints before the carrier falls |
| `--scene3c14` | Boots chapter 3c scene 14 (chapter3c_scene14_1playcam, the experiments chamber) and plays it with the scene 7 driver, taking the energy boost soup first (`aid_ghoulybox_powerups_supersoups_energyboost5`, kind 3) with a weapon reach of 60. Does not pass. A free-play section with twelve skeletons; Cooper has 10 energy, a skeleton takes 5 or 6 a hit, and he faints over and over before the script moves on |
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
| Actor id lookups | legacy | `find_actor_by_id` / `find_actor_by_group` over the unfilled `g_pulse_nodes` still serve script actions, conditions and the ghoulybox controller; see `docs/LEGACY_HOST_CODE.md` |
| Script actions | legacy | `0x00126E10`'s other cases (0x4D, 0x4E, 0x62, 0x67, 0x6E, 0x73, 0x8E, 0x9D) and `0x00127BC0`'s cases still compute values and skip their callees; this scene does not reach them |
| Present (the snapshot) | renders nothing | `0x00116530` records its draws but the texture stays empty |
| Scene 3's Free Play branches (`--scene3`) | native | Free Play's node 0x51 on the skeletons sends message 1 over the native DAT_00511B38 walk (2026-10-24) |
| Script condition nodes | partial | `0x00125450` registers types 0x3D..0x3F, 0x40..0x44, 0x47, 0x48, 0x5A, 0x5B, 0x66, 0x68, 0x6A, 0x6C, 0x72, 0x92, 0x93, 0x94, 0x95 and 0x99; retail also takes 0x65, 0x75, 0x8B, 0x8C and 0xA3, which raise |
| Script event steps | partial | `0x00126140` queues step types 0 to 5 and 0xF (script types 0x19..0x1F) and `0x000D0B40` runs types 0 to 5, 0xD (one type, `0x000CFDA0` as type 2) and 0xF (the key carrier, `0x000CFAE0`); script type 0x50 builds type 0xD (`0x00125FC0`, with `0x00126940` and the fight music); the other builders from `0x00125E00` on (script types 0x20 and up) and the ticks of types 6 to 0xE and 0x10 up are not translated. Script types 0x5F and 0x87 are still empty stand-ins of the same kind |
| The Grim Reaper's dizziness | no writer | `0x000AD600` turns him dizzy (0x3EA) while DAT_00512170 is set; no translated code writes it (`g_grim_reaper_dizzy`) |
| Blob shadows | off | DAT_0050EF18 is never set, so 0x00119A60 hands out no shadow (the reaper's included) and the draw 0x00119AD0 is not reached |
| The spider web in captures | unchecked | The web (type 0x1B) lands and snares by its messages; no capture has caught one yet |
| Background cloths | partial | 0x001153A0 builds the background model's parts at scene load: kinds 1, 2 and 8 (flags, 0x00050880, with the player and actor push hook 0x00114910), kind 3 (no cloth), kind 4 (collision planes) and kind 5 (curtain pairs) are translated; kinds 6 (0x000509E0) and 7 (0x00050940) raise, as do the hooks 0x001145F0 and 0x00114580, the specular shading 0x0004BA90, the second strip draw 0x0005D710 and 0x00064820's part pass. The 2026-10-23 full regression found only kinds 2 and 5 in use (scenes 3, 4, 5, 6 and 8 have kind 2) |
| Script type 0x16 | legacy | Still the old shortcut; retail waits for `0x0004EFE0` with DAT_00512470 set, then releases the cutscenes |
| Chapter 2a scene 3 (`aid_script_ghoulies_chapter2a_scene3_1playcam`) | not driven | `--scene2a2` stops once the script moves on to it |
| The coward zombie on the plates | unchecked | Hurt, scene 2a2's zombie climbs onto a plate stack and stays there without moving (y about 8.6) until Cooper comes within reach; not compared with retail |
| The scene 2a1 stairwell walk | slow | The driver finds no grid route from the stairwell floor (y -40) and walks "straight" into the wall until about frame 8000 |
| A death during an event | as dumped | `0x00134AE0` sets DAT_00430D0C with the restart, so the reloaded script passes its handlers through (0x00128A00's third argument) and skips the event; the kitchen's exit door then stays shut because the event's end (state 4) never opens it. Not compared with retail |
| The camera near scene 9's door 8 | unchecked | From frame ~11400 of a long scene 9 run the camera looked down on the cellar from outside its walls; not compared with retail |

## Work

1. Chapter 2a scene 3 (`aid_script_ghoulies_chapter2a_scene3_1playcam`): drive it after `--scene2a2` and
   translate what it reaches.
2. Move the remaining `g_pulse_nodes` callers to the native `DAT_00511AA4` lookups.

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

### 2026-10-30: the credits

- `--scenecredits` boots credits_final. It passes on the existing native path: 18 pages, then the script moves on to
  aid_script_ghoulies_attractsequence_final1 (frame 17051).
- The credits music (XACT_SOUNDBANK_GSTREAMSCUTSCEN_CREDITS, Gdvd0 wave 24) was skipped. It is 58 MB of 16-bit
  stereo 44.1 kHz PCM, above the host wave loader's 48 MiB guard in `pcm_out.cpp`. The guard is now 128 MiB, and the
  skip warning is gone. I have not listened to the track.
- Checked in the captures: the scrapbook pages of scene photos and the name cards, for example "Phil Baker, Michael
  Boulton, Ed Bryan, Will Bryan" and "Korea Loc Team & Taiwan Loc Team".

### 2026-10-30: the final score, stages 2 and 3: the scores and the Super Scary Score

- Page 2 (0x000D41E0, then 0x000D1540..0x000D1DA0) works through the four tally rows. For each row, the dialogue bat flies to its icon
  (0x000D2730) and the row's sound loops (0x000D27D0). The page then shows these in turn on timers:
  - the label, with the snare roll;
  - the value, with the fanfare;
  - the rank over 25;
  - the Baron's line for the rank (0x000D4570, XACT_SOUNDBANK_GBOY_0TO6 .. _21TO25);
  - the running "Score".
- The rows' text and icons are laid out by 0x000D24B0 and 0x000D1DC0. They draw as a panel, the four icons and the bat.
- Page 3 (0x000D4230, then 0x000D4610..0x000D4BE0) runs in order:
  - It shows "Super Scary Score" and "total / 100", and picks the reallysad, reallyaverage or reallyhappy dialogue cutscene
    (below 33, below 66, else).
  - It plays the total's challenge ditty (winnowt, bronze, silver, gold or platinum), then the Baron's line for it.
  - When the save's +0x19C word is set, it shows the butler's brew icon with that count.
  - It opens the OK prompt. Its "totalscore_accept" row (0x000D4E70) moves the page to state 6, and the prompt's close
    ends the page.
- The update then goes from page 3 straight to the credits (mode 5, 0x000D4500, aid_script_ghoulies_credits_final).
  Pages 4 and 5 (0x000D2FE0.., 0x000D35A0..) are not reached from here, so they still raise.
- The butler's brew sprite reuses the button-bat menu's translations of 0x00144780, 0x00144820 and 0x00144880, now
  public in `button_bat_menu.h`. The final score's keyboard device (+0x170) is mapped in the scene's input registry
  for the prompt's 0x00142460.
- Driver: on the Super Scary Score page (state 5), the scene 7 driver presses A for the OK prompt.
- `--scenefinal` passes; the script moves on at frame 2960.
- Checked in the captures: the four rows counting up to Score 52 (Time Taken 1 sec 25/25, Ghoulies Defeated 1 1/25,
  Times Fainted 0 25/25, Prisoners Rescued 0 1/25), the bat at each icon, and "Super Scary Score 52 / 100" with the OK
  prompt. The test has no current game, so the values are the empty game's defaults.
- Not checked: the butler's brew icon (the save word is 0) and the sounds.

### 2026-10-30: the final score, stage 1: the save and page 1

- Factory type 0x99 (entitySceneControlFinalScore) is now native, in `src/engine/frontend/final_score.cpp`. The
  scene's actor services bind its services over the game-select services and the challenge save context.
- Its construct (0x00082DC0) holds the pause count (DAT_00510CEC) and sets flag 0x100000. It then runs these in order:
  - It stores the finished game in its save (0x000F2D70). With scene flag 2 set, that covers the five times (0x0010E360..0x0010E4A0) and
    the finished flag (0x0010E310). It also covers the challenges, the progress bits (0x000F3290, 0x000F2A50, 0x0010E4F0) and the
    +0x184 word, and it queues the write.
  - It builds the final score's state (0x000D3E30): the "finalscore" view, the final score text, a keyboard device and the
    tally.
  - It starts the pause ditty (0x000D44D0).
  - It opens page 1 (0x000D4190).
- The tally (0x000D2880) ranks the four rows of the 0x00303F60 table from 1 to 25 (0x000D2950, 0x000D2B00, 0x000D2CB0
  and 0x000D2E10):
  - the time taken against 180 minutes plus 25-minute steps;
  - the ghoulies killed against 700 minus 20 per step;
  - the faints against 20 plus 10 per step;
  - the prisoners against the literal table.
- The pages run through the table at 0x00304EC4, five entries each: start, free, tick, draw and done.
  - Page 1 (0x000D3450..0x000D34F0) queues one of finalscore_intro1..6 and finishes once the dialogue closes.
  - Pages 2 to 5 raise. Page 2 is 0x000D41E0, the four scores counted up.
- The page's dialogue cutscene setter (0x000D4280) is translated with its jump-table cases (0x000D44C0). The default case
  swaps in the gold bat twice.
- `--scenefinal` reaches 0x000D41E0 and raises there.
- Checked in the captures: the cook's portrait with "Well done, young 'un! ... Let's 'ave a look at yer final score"
  and the A prompt (`scenefinal_page1`). The save itself is not checked: the test runs with no current game, so
  0x000F2D70 returns at the empty name.

### 2026-10-29: chapter 5 scene 13's end and the final score

- `--scene5_13b` (two pages) and `--scene5_13d` (seven pages, the cook's farewell) pass.
- chapter5_scene13_2playcam is played as the cook (DAT_00511B40's actor is not class 4). The scene 7 driver now takes
  the player's class from DAT_00511B40 (link - 0x93C) instead of assuming Cooper's 4; it logs the cook's energy under
  "Cooper's energy". `--scene5_13c` then passes, after some 29000 frames among the imps.
- finalscore_finalscore stops at its start: factory type 0x99 is entitySceneControlFinalScore (the type table's name
  one row off; vtable 0x005055F0: sizes 0x00082D50, construct 0x00082DC0, destroy 0x00082E20, the scene control's
  message 0x00115EE0, update 0x00082E50 and draw 0x00082F70). Its update steps pages 1..4 through the handlers at
  0x00304ED4.. (0x000D41E0, 0x000D4230, ...), then requests aid_script_ghoulies_credits_final (0x0011EB30, mode 1).
  None of 0x000D3D80..0x000D4500 or 0x000F2D70 is translated yet.
- Checked in the captures: the cook with his ladle among the imps by the doghouse (600, 2400), and the cook's
  farewell page (5_13d page 3).

### 2026-10-29: chapter 5 scene 13: the front gate

- chapter5_scene13_1playcam waits only on PlayerEntersRegion 1 (a mask-0 volume at 338, -56, beyond the garden's
  fence). It reaches nothing untranslated.
- The driver no longer chases Mr Ribs (class 0x33, who takes no hits) when a region or exit is its goal.
- The region's volume is off the navigation grid (no floor cell), so a straight walk at it runs along the fence
  (6600). The exit lies straight ahead of Cooper's start; with a live region off the grid the driver now takes the
  exit instead, and `--scene5_13` passes, moving on at frame 1679.

### 2026-10-29: chapter 5 scene 12: the countdown's gold finish

- Script type 0x9A threw on 0x000F2740. It is now translated in `chapter5_countdown.cpp`: unless the countdown is
  already in mode 4, mode 4 sets DAT_00506C5C and plays the gold challenge ditty, then 0x000F27C0 runs with mode 0
  (the clock's message fades and the seconds go to -1).
- `--scene5_12` then passes. Checked in the captures: the hallway's great doors open, the green fire on the floor
  (4500).

### 2026-10-29: chapter 5 scene 11: the lower corridors

- `--scene5_11` passes on the existing native path. Checked in the captures: Cooper kicking a haunted picture, with
  Mr Ribs down the corridor (1500).

### 2026-10-29: chapter 5 scene 10: the TV room

- `--scene5_10` passes on the existing native path; Cooper faints twice in its event first. Checked in the captures:
  the event's door and plant icons (9000) and the smashed furniture in the TV room (16200).

### 2026-10-29: chapter 5 scene 9: the walled garden

- `--scene5_9` reaches its event on the existing native path; it does not pass. The event's steps (node 0x28 no
  points, 0x25 the next different of the Jessie Clydes, 0x21 the skeletons spared, 0x5F four kills) fail on the
  driver's fights, the Grim Reaper comes, and Cooper faints and restarts every 3000 to 4000 frames.
- One run went to a black screen after the second restart (frame 8100 on): the scene had started again, but nothing
  more was drawn or logged until the limit. A second run with the same build did not hang (Cooper stayed among the
  marked actors through seven restarts). Like 5_5's one `at()` miss and 3c10's one black page, it does not
  reproduce.
- Checked in the captures: the event's RIP stone with 3 and a skeleton in the garden (4200).

### 2026-10-29: chapter 5 scene 8: script type 0x88

- Script type 0x88 read its five tag / count pairs and queued nothing. It now translates the dump (0x00126140's
  case): 0x00126100 builds step type 0x12 with the 1.5 s delay, each set pair (node+0x4C, +0x94, +0xDC, +0x124,
  +0x16C) is a choice, one is drawn (0x0001BB60) for +0x18 (the tag, 0x001077B0) and +0x1C (its kills), node+0x170
  marks the tag's actors (0x00126940), then the step is queued (0x000D05D0) and the tag's fight music plays
  (0x00116C50).
- Step type 0x12's runner (0x000CFDF0, table 0x003039CC) is translated: it holds while DAT_00512B78[tag] kills fall
  short of +0x1C, then waits out its delay. Its scoreboard count (0x000D0F20) is the kills still owed.
- The driver gains an every-box errand (`powerup_kind` 0xFFFFFFFF): once an event's steps run, each box of the
  name is broken in turn, with what comes out fought first; Mr Ribs (class 0x33), who takes no hits, does not hold
  it up.
- `--scene5_8` does not pass: the skeletons come only from four washing machines on a raised floor among zombie
  ones, and from the nearest open cell (some 20 away) Cooper's kicks never break one.
- Checked in the captures: the event's skull icon with 3 (the type 0x12 step) and the red cross of its
  next-different step (2400), and Mr Ribs with his key beside Cooper.

### 2026-10-29: chapter 5 scene 7: the relics store

- `--scene5_7` passes on the existing native path. Checked in the captures: Cooper with a torch among the mummies
  and zombies, the event's icons up (3000).

### 2026-10-29: chapter 5 scene 6: the pantry

- `--scene5_6` passes on the existing native path. Checked in the captures: Mr Ribs and Cooper at the pantry door
  (3600).

### 2026-10-29: chapter 5 scene 5: the dining room

- `--scene5_5` reaches the pirates' event and runs without new untranslated code; it does not pass. The event's
  no-hits step (type 9) fails on Cooper's first hit, the Grim Reaper comes (his touch takes Cooper's energy to 0),
  and the event starts again: some 22 to 25 tries in 36000 frames.
- One Release run failed at frame 34848 with "invalid unordered_map<K, T> key" (an `at()` miss) just after a page.
  It did not happen again in a second Release run, nor under cdb in RelWithDebInfo with /Ob1 or /Ob2 (the runs'
  timing differs from the first); its source is not found.
- Checked in the captures: the pirates and the Grim Reaper in the dining room, with the RIP stone (33300).

### 2026-10-29: chapter 5 scene 4: a player's spawn controller

- chapter5_scene4_1playcam crashed at frame 360 calling address zero: 0x000F3E90 (`request_spawn_actor`) sent an
  imp's spawn request to the record's DAT_00512B74 controller, a player's (0x00028640 attaches the player's
  ghoulybox record to it), whose message (actor vtable +0xC) was only bound once something first messaged the
  actor. The player factory now binds each player's controller as it constructs the player, through the scene's
  `controller_message` hook (0x00028640 can request a spawn before the actor is ever messaged).
- 0x000F3E90, 0x000F3FC0 and 0x000F4030 now raise, naming the spawn, if a record's controller has no message.
- `--scene5_4` then passes.
- Checked in the captures: Cooper in Dunfiddlin Cottage with the water pistol (600), and the haunted TV's event icon
  with an imp on Cooper's head (6000).

### 2026-10-29: chapter 5 scene 3: the conservatory

- `--scene5_3` passes on the existing native path; nothing new is reached. Cooper ends at 29 of 50 energy.
- Checked in the captures: a worm grabbing Cooper among the spiders (6000) and the groundskeeper with the water
  pistol at the exit (16500).

### 2026-10-29: chapter 5 scene 2: the exit at the start

- chapter5_scene2_1playcam moved on at frame 3: Cooper starts inside the scene's exit volume (mask 1), and 0x00140E30
  queued {0xB0, door} on his first frame. The scene start (0x0011C72F) sets DAT_00545D20, the "already in an exit"
  latch, to 1 so a player who starts in one must leave it first, but the port kept two globals for that address:
  the reset set `g_545d20` while 0x00140E30 read `g_flag_5d20`. DAT_00545D28 had the same split. The reset now sets
  `g_flag_5d20` and `g_flag_5d28`, and the duplicates are gone.
- `--scene5_2` then passes: two pages, the Hunchback's event, and the move on at frame 10734.
- Checked in the captures: the Hunchback fight in the bathroom with its event icon (4500, 9000). The countdown clock
  is not shown: booted directly, the scene never runs chapter 5 scene 1's type 0x89.

### 2026-10-29: chapter 5 scene 1: the countdown

- chapter5_scene1_1playcam stopped at its start on factory type 157 (getter 0x00082CB0). With the type table's
  names one row off, type 0x9D is entitySceneControlChapter5Countdown (parent 3, vtable 0x005055D8): the scene
  control's sizes (0x00115D30) and message (0x00115EE0), construct 0x00082D00 and update 0x00082D10.
- `chapter5_countdown.cpp` translates the countdown: 0x000F2580 (the scoreboard clock, message kind 5, icon 0x4C),
  0x000F25E0 (mode 1 runs it; under 11 seconds mode 2 sets the message's +0x74 warning, 0x0012FB40, newly in
  `scoreboard_messages.cpp`; at none mode 3 queues "chapter5_failchapter", pauses the scene and a second later
  requests chapter5_scene13_1bookfail, or sets DAT_0050EA9C bit 0x200000 with the scene ending), 0x000F2920 (the
  seconds, DAT_00506C60, and the clock digits) and 0x000F27C0 (the mode changes; mode 1 clears story bits 0x33 to
  0x3C, mode 4 plays the gold challenge ditty).
- Script type 0x89 threw on 0x000F27C0; it now starts the countdown (mode 1) after storing its minutes * 60 + 0.99.
  This scene sets 13 minutes.
- The countdown's words (DAT_00506C58 the mode, DAT_00506C5C, DAT_00506C64, DAT_00506C68) are now game globals,
  not scene state, so the countdown runs on through chapter 5's scenes; the scoreboard's challenge mode reads the
  same DAT_00506C58.
- Checked in the captures: the padlock clock at 12:54 (1200) and 12:29 (2700), with Mr Ribs and his key.

### 2026-10-29: chapter 4 scene 1: the Baron

- Chapter 4 is chapter4_scene1_1book, chapter4_scene1_1playcam and chapter4_scene1_2book, then chapter 5.
  `--scene4_1` and `--scene4_1c` (the books) pass.
- chapter4_scene1_1playcam stopped at its start: factory type 55 (getter 0x000A7800) was untranslated. The retail
  type table's name column is one row off (row 0x36 names it), so type 0x37 is entityStrategyBaron, parent 8; the
  plane, entityStrategyBaronPlane, is type 0x38 (getter 0x000A8C10).
- `baron_strategy.cpp` translates its vtable 0x00505D00: sizes 0x000A77E0 (0x108 / 0x218), the constructor 0x000A7850
  over 0x000A73A0 (the effect template at parameters +0x18 and three timelines by energy tier), the destructor
  0x000A7870 over 0x000A7440, update 0x000A7880, messages 0x000A7D30, enter 0x000A80B0 and leave 0x000A8590, with
  the helpers 0x000A7470 (the effect's timeline), 0x000A75B0 / 0x000A76C0 (the effect off / on, the inlined
  0x0003DF40, 0x0001FC40 and 0x0001FD20), 0x000A8700, 0x000A8750 / 0x000A87E0, 0x000A8870 / 0x000A88A0 (the tiers:
  body +0x94 above 180, 161 to 180, or less) and 0x00134430 (a target within a radius and angle of the facing). The
  pirate's 0x000BEB80 is shared as `runtime_strategy_weapon_free` over strategy +0x38. The update, messages, enter and
  leave switches were exported again with their jump-table targets as seeds.
- `baron_plane_strategy.cpp` translates the plane, vtable 0x00505D1C: sizes 0x000A8BF0 (0x204 / 0x198), the
  constructor 0x000A8C60 (the effect, the engine loop and the cutscene names; four landings at +0x4C), update
  0x000A8CD0, messages 0x000A91A0, enter 0x000A9430 and leave 0x000A9980, and the helpers 0x000A88C0 / 0x000A8960 /
  0x000A89C0 / 0x000A8AD0 (the effect and its modes, mode 2 by body bit 0x35), 0x000A9AA0 (the energy rows at
  0x00302AA0 with other ghoulies out, 0x00302AF0 alone), 0x000A9B20, 0x000A9B50 / 0x000A9BA0 (body bits 36, 53, 31),
  0x000A9BE0 / 0x000A9C90 (flight: contact shapes 0x13 and attack row 4), 0x000A9CF0 (the engine loop on sound slot
  4), 0x000A9D40 (the walk to the nearest kind-0x1B marker, with 0x0011F260), 0x000A9DB0 (the inlined 0x0004EF20 and
  0x0004EF80 on the playing cutscene), 0x000A9EA0 and 0x000A9F90. The scene binds the markers (DAT_0051108C) for it.
- The fight, as the driver plays it: phase 1 ends on condition 24 (GhoulyHasEnergyLEQ, the Baron at 140). Only his
  cane hurts him: knocked down (0x409) he drops it, Cooper takes it with A (0x0007D210's button 0x100 edge) and the
  Baron, worried (0x411), loses about 15 a hit. Phase 2 waits on condition 10 (SpecificGhouliesKilled, the plane):
  each knock-down with message 0x6E +0xC set lands the plane (0x404, the scene paused, {0xB9, tag} to the script,
  20 energy off) and a wave of burst-picture ghoulies follows; after the fourth, at 60, it spins (0x414).
- Driver additions (`invincible`, `take_dropped`): Cooper's energy is put back to his most each frame (a cheat, not
  game behaviour), an empty-handed Cooper taps A beside a weapon a ghouly dropped (strategy +0x38 not in its body
  +0x48), a weapon he took is swung from 16, and invincible runs get 108000 frames. The driver then fails only on
  the plane's last 40 energy: knocked back onto the raised floor, it never reaches the plane again.
- The "Super Weapons" soup did nothing: every reader of DAT_00512278 (the bob, candle, container, gun, projectile
  and chair weapons and three scoreboard meters) took it for BSS no code writes and used 0. It is the powerup
  table's kind 0xD row (0x00512140 + 0xD * 0x18) +0 active, which 0x00123E70 writes through its indexed row store.
  `powerup_flags.cpp` binds it to the scene's powerup rows, as DAT_00512218 (invisibility) already was.
- Checked in the captures: the red, cane-swinging Baron (1500), the plane diving at Cooper with the wave icons on
  the HUD (15300), a burst-picture skeleton (16200) and the Baron's spin (26100). The superweapon fix is not checked
  on screen: the driver's soup errand took a reverse-controls soup instead.

### 2026-10-29: chapter 3c scene 15: the end of chapter 3c

- The order runs chapter3c_scene15_1playcam, chapter3c_scene15_2book, chapter3c_scene15_3playcam, then
  chapter4_scene1_1book. All three pass with the existing native path; nothing new is reached.
- `--scene3c15`: the Grand Hallway with "PLAYER ENTERS", the chapter3c_scene15_theresthedoor cutscene and free play;
  it moves on at frame 1715.
- `--scene3c15b`: three pages; it moves on at frame 1902.
- `--scene3c15c`: an event on the round haunted door (objTag_Actor_HauntedDoorRound), the impsenter cutscene and
  `aid_ghoulybox_ghoulies_chapter3c_hallwayimps`, then node 0x5A for five imps against the clock. Cooper ends at 5 of
  20 energy, and the script moves on to chapter 4.
- Checked in the captures: the hallway cutscene view (3c15 300), Cooper on the stairs and the landing (900, 1500),
  the book's first page, and the 3c15c HUD with the event icon, the RIP stone, the 0:18 clock and "Energy Boost!" (3900).

### 2026-10-29: chapter 3c scene 14: the experiments chamber

- chapter3c_scene14_1playcam is a free-play section: the script places the experiments chamber, its twelve skeletons
  (tag 0x34) and soup boxes, shows "free play" at frame 235 and holds until it ends. Nothing untranslated is reached.
- `--scene3c14` does not pass. Cooper starts with 10 energy and a skeleton (a punk skeleton in the 33600 capture)
  takes 5 or 6 a hit. Fighting at once, he faints at frame 768. Aiming for the invisibility soup (kind 9, across the
  chamber at -27, -80), he faints 26 times on the way. With the energy boost soup beside the start (kind 3, 10 to 15)
  and a weapon reach of 60, he faints 18 times in 36000 frames. The `aid_ghoulybox_triples_ninjaimps` box also
  lets out ninja imps (tag 0x1B) near the start.
- Checked in the captures: the HUD heart and energy, and skeletons coming out of the chamber at the entrance (600,
  33600).

### 2026-10-29: chapter 3c scene 13: the event steps 6, 7, 9 and 10

- The scene order goes from chapter3c_scene12_1playcam to chapter3c_scene13_2book, then chapter3c_scene13_3playcam:
  `--scene3c13` boots the book (four pages, passes at frame 2815) and `--scene3c13b` the laboratory.
- Script nodes 0x20 to 0x23 (in `script_handle_scene_actions`, 0x00126140) were placeholders that built nothing. They
  now queue their steps as the dump does, each with XMM0 = 1.5 (0x0032B210) at +4: 0x20 step type 6 (0x00125E00) with
  the word at node+8 at +0x18; 0x21 step type 7 (0x00125E40) with the tag of the name at node+8 at +0x18 and -1 at
  +0x1C; 0x22 step type 10 (0x00125F00) with -1 at +0x18; 0x23 step type 9 (0x00125EC0).
- Their runners (table 0x003039CC) are translated in `script_event_sequence.cpp`: type 6 (0x000CFFD0) is done at
  once; type 7 (0x000CFF60) keeps the player's kills of its tag (DAT_005138D0 + DAT_00512F48 by tag) on its first run
  and fails once they grow; type 9 (0x000D0090) fails once the player has taken a kind-4 hit (DAT_00522160 +
  DAT_005217D8, the kind-4 tallies with tag 0xE and tag 4 as the second actor) while the steps do not wait
  (DAT_0050653C); type 10 (0x000D00C0) is the same for kind-3 hits counted from the total it keeps at +0x18.
- Scene 3c12's kill-8 section queues node 0x23, so its event now also fails on a hit.
- Powerup kind 8 (InstantDeath by the alphabetical powerupId_ order) is what the onehitkill soup box drops.
- `--scene3c13b` does not pass. "event" here: spare the skeletons (type 7), the key from a background part (type 1,
  out at about frame 910), then the key carrier (type 0xF) among the Jessie Clydes from
  `aid_ghoulybox_triples_jessies`. A Jessie shows 84 energy on the HUD and takes 20 of Cooper's 30 a hit. With the
  one-hit-kill soup taken first (frame 2852) a Jessie still lands the first hit, and Cooper spends the soup walking to
  far soups (he stands on a free invulnerability soup, kind 0xA, without collecting it).
- Checked in the captures: the book's pages (page 2 and 4), and the laboratory HUD with the crossed-out skull, the key
  carrier icon and 84 beside a Jessie Clyde (2400).

### 2026-10-29: chapter 3c scene 12: one kill short

- `--scene3c12` (chapter3c_scene12_1playcam, the experiments chamber) reaches no untranslated code but does not pass.
  The first unnamed section waits for objTag_Actor_Warlock (as in scene 3c7), so the mode breaks
  `aid_ghoulybox_single_warlock` first. "event - minimum damage" (the warlock) and free play follow; region 1, behind
  a glass partition the walk takes some time to get round, plays chapter3c_scene12_hkeepertakegun (the housekeeper
  takes the popcan gun back); region 2 starts an untitled kill-8 event (node 0x5F's count, 8 to 8, with
  `aid_ghoulyspawn_ghoulies_chapter3_expchambermix`) with the exit door webbed over.
- Fighting through free play sent Cooper away from region 2 after the ghoulies; fighting only in the warlock event
  left the kill-8 event unfought. With "free play" as the calm title he fights the event but faints with one kill
  left (RIP 1, 4 energy at frame 7800). The big hits (7 and 5) come from several classes (0x3B, which explodes, 0x3C,
  0x1F, 0x20, 0x24). The room also has an EventComplete soup box (powerup kind 5) at (-1, 4.4, 163) and an
  energyboost5 box at (-35, 12, 79), which the driver's one powerup errand (spent on the warlock box) cannot reach.
- Checked in the captures: Cooper behind the housekeeper's glass partition (4200), the webbed exit door with RIP 8
  (18000), and RIP 1 with 4 energy in the chamber (7800).

### 2026-10-29: chapter 3c scene 11: the popcan gun

- `--scene3c11` (chapter3c_scene11_1playcam, the Grand Hallway) passes with no new translation. Cooper comes in with 1
  energy and the popcan gun (script node 0x31); the hallway skeletons (class 0x34) faint him with one hit once he walks
  into punching range.
- Driver change: `weapon_reach` (default 16) is how near a ghouly counts as in reach while Cooper holds a weapon
  (body +0x48); the mode uses 60, so he shoots before the skeleton gets to him. The script moves on to
  chapter3c_scene12_1playcam at frame 2172 with no faint.
- Checked in the captures: the shot skeleton's bones across the landing, Cooper with the gun on 1 energy (1500), and
  Cooper at the archway exit (2100).

### 2026-10-29: chapter 3c scene 10: the attic's clutter

- `--scene3c10` (chapter3c_scene10_1playcam) reaches no untranslated code but does not pass. Free play waits for region 1
  (condition opcode 8), the exit volume at (-652, -282); on the way, the volume at (64, 192) plays
  chapter3c_scene11_hkeepergivegun, the housekeeper handing back the popcan gun (node 0x31). Cooper has 5 energy and
  the Grim Reaper's clock (0:17 left at frame 3000) takes them all at about frame 5000.
- Driver changes: a powerup-kind-0 errand ends once the first box of its name breaks, and a soup errand once its
  soup has come out, instead of visiting every box of that name (the attic has three energyboost1 crates). The
  crate at (64.5, 108.7) blocks the gap the grid routes through, so the mode breaks it first.
- Past it, the walk shuttles between chests and crates that the grid takes for open floor, near (-35, 20) and
  (50, 85): each held cell is avoided, the walk reroutes through the other, and the avoided cells are forgotten when no
  path is left.
- One run held a dialogue page (the housekeeper's first) with the screen black from frame 883 to the end; the next run
  of the same build paged through it, so that hang is intermittent and not yet explained.
- Checked in the captures: the housekeeper's "give yer this thing back" page (page 1), and Cooper against the chests
  with the Reaper's 0:17 (3000).

### 2026-10-29: chapter 3c scene 9: kiting the Grim Reaper

- `--scene3c9` (chapter3c_scene9_1playcam) reaches no untranslated code but does not pass. "event - one type(random)/
  time limit" queues a 20 s step (type 4) and an 80 s step (type 5): when the first runs out the Grim Reaper (tag and
  class 0xF) hunts Cooper, and when the second does the exit opens ("exit door opens, cursed mummy activated", then
  `aid_ghoulybox_ghoulies_chapter3c_cursedmummybook` and free play). The Reaper's touch drains 2 at a time or takes
  all 15 at once, and a faint restarts the event.
- Powerup kinds follow the alphabetical powerupId_ names with Null 0 (EnergyBoost 3, EventComplete 5, Invisibility 9,
  MiniCooper 0xC all agree). The bathroom has EnergyBoost (3), KnockDownMania (0xB), PlayerSlowDown (0xE),
  PlayerSpeedUp (0xF) and DizzyReaper (2, on a ledge at -289, 38, -154).
- Driver changes: `flee_class` (with the calm title) sends Cooper every half second to the walkable spot 80 out, of
  eight around him, farthest from the nearest ghouly of that class, skipping for two seconds a direction he gained
  under 10 in; a chase of one class gives up on a ghouly after ten seconds instead of a minute; a landed powerup more
  than 30 away is approached through the nearest open cell. The kite lasts some 2400 to 5000 frames of the 4800
  needed (one attempt fainted about 100 frames short). Taking the speed-up soup first failed: from the start Cooper
  pushes on the laundry cart at z 84.7 and never reaches it. Sixteen directions at 60 and 120 did worse.
- Checked in the captures: the HUD's two clocks, 0:06 and 1:06, with the laundry cart ahead of Cooper (1500), and the
  Grim Reaper on Cooper at the bathroom's corner door (5400).

### 2026-10-28: chapter 3c scene 8: the dormitory's haunted door

- Script node 0x58 (in `script_handle_actor_actions`, 0x00126A10) was a placeholder that built its message and
  dropped it. It now matches the dump: the message {0x65, `find_actor_strategy_id(node+0x48)`} goes to each marked
  actor of the tag at node+8 through 0x00122D40, as node 0x5C does. `find_actor_strategy_id` (0x0002B710, new
  `src/engine/entities/strategy_names.cpp`) searches the 0x30 actorStrategyId names at 0x00339924 with 0x000FBFB0 and
  returns the index plus 1001, or 1000. In scene 3c8 it sends the Jessie Clydes actorStrategyId_ForceAngry (1027,
  state 0x403, which `jessie_clyde_strategy.cpp` takes as struck and moves to 0x401).
- Class keys are the script's actor tags (object +6): HauntedDoor is 0x12 and JessieClyde 0x1E here.
- `--scene3c8` does not pass. The event section waits for the haunted door's wake (condition opcode 7, a live 0xB4
  event tagged 0x12); the door wakes once a target is within 60 (state 0x413 -> 0x417 posts {0xB4, tag}).
  Driver changes: a pending opcode-7 wait names the ghouly to go for, like PlayerHitGhouly (opcode 14), and with no
  other target Cooper walks to it. He never gets past the angry Jessie Clydes: at about (300, -50) he is sent back
  near (200, -270) within 120 frames, and after four or five hits he faints and the event restarts.
- Checked in the captures: a Jessie Clyde beside Cooper among the bunks (1200) and Cooper knocked flying with 10
  energy (2100).

### 2026-10-28: chapter 3c scene 7: the laundry's warlock

- `--scene3c7` (chapter3c_scene7_1playcam) passes with no new translation once the driver breaks
  `aid_ghoulybox_single_warlock` first. The script's unnamed section holds on objTag_Actor_Warlock existing in the scene
  (condition opcode 16 at the end of the section); until the box is broken the driver walks straight to the shut exit
  and the run holds for 36000 frames.
- With the box, Cooper fights the warlock (class 0x3A) and other ghoulies (classes 0x1B, 0x36) until about frame
  31736 ("exit door revealed"), with no faints, and the script moves on to chapter3c_scene8_1playcam at frame 32348.
- Checked in the captures: Cooper among the washing machines and baskets (15000) and by the mangle with broken boxes
  and 9 energy, near the exit (31800).

### 2026-10-28: chapter 3c scene 6: the fire imps' fireball scale

- `--scene3c6` (chapter3c_scene6_1playcam) crashed (exit 127) around frame 7000 while Cooper fought the fire imps
  (class 0x19, objTag_Actor_ImpFire). Under cdb: an access violation in `prepare_segment_pool`, from a weapon's
  `walk_runtime_body_contacts`, with `g_segment_count` (DAT_00511A18) overwritten by a collect that stored more than
  the 27 slots (DAT_00511A1C..DAT_00511AF4).
- The weapon was the imp's fireball (factory type 0xA7, class 0x9D). Its contact radius was NaN, so every probe
  counted as a hit. The NaN came from its scale: the release (`tick_runtime_throw_state` ->
  `teleport_runtime_body_secondary` -> `detach_runtime_body_secondary` -> thrown projectile state 7 ->
  `restore_runtime_thrown_projectile_root`, 0x000CC1E0) restores a root that `record_runtime_weapon_root`
  (0x000CC010) took from a zero-scale matrix.
- The cause: `command_runtime_body_create_secondary` (0x0001A2D0) scales the clone's objparams +0x20 by
  `*(float *)(*(int *)(body + 8) + 0x38)`, the host actor's +0x38. We read it from the host's raw `bytes`, which stay
  0 there; the value lives in the typed `Actor::radius_xz`. It now reads `host.actor->radius_xz`, and the fireball
  gets the imp's scale (it had been 0 x 1.0).
- With the crash gone the scene still did not pass while the driver fought: free play only spawns the kitchen fire
  imps when Cooper enters region 1 (condition opcode 8, then node type 0x3C), so the mode takes the energy soup and
  heads for the exit. It passes at frame 4812.
- Checked in the captures: a hit spark by an imp at the kitchen table (1500) and Cooper among the stoves with 20
  energy (2400).

### 2026-10-28: chapter 3c scene 5: the cold store

- `--scene3c5` (chapter3c_scene5_1playcam) passes with no new translation. "event - kill all/limited attacks" runs
  from frame 1542 to 5625 (class 0x3C ghoulies, 15 to 9 energy), and the script moves on to
  chapter3c_scene6_1playcam at frame 6513.
- Checked in the captures: the fight among the zombies and skeletons, with the event HUD's RIP count (6), a green heart
  with 44 (the attacks left, it seems) and a yellow heart with 26 (2400).

### 2026-10-28: chapter 3c scene 4: the groundskeeper's key

- `--scene3c4` (chapter3c_scene4_1playcam) passes with no new translation. "event - ghouly find the key" runs from
  frame 367; Cooper fights class 0x10 ghoulies (20 to 17 energy), and the script moves on to
  chapter3c_scene5_1playcam at frame 7184.
- Checked in the captures: Cooper beside the red-bearded groundskeeper in the yard (1200).

### 2026-10-28: chapter 3c scene 3: the groundskeeper

- `--scene3c3` (chapter3c_scene3_2playcam) reaches no untranslated code but does not pass.
- With the 1000 fight range, Cooper fought his way across the yard and fainted twice (a worm exploding beside him,
  then class 0x1F ghoulies). The mode now fights within 50. That way he reaches region 1 (the groundskeeper's spot, at
  112.6, 69.7, radius 50), and that condition is met.
- Still failing: PlayerApproachesDoor (condition 6) for door 24 (0x18) within 50, at (-106, -41). Cooper comes no
  closer than 136 (frame 11100). Twice he is back at the start (-117, 634) without a faint, at frames 12089 and
  30000. The scene's title is "groundkeepers warning", and capture 12000 shows him on the floor, blurred, as if
  caught. Getting past the groundskeeper is not something the driver does.
- Checked in the captures: Cooper with a weapon facing a gassing ghouly (4500), and Cooper caught (12000).

### 2026-10-28: chapter 3c scene 2: the farmyard

- `--scene3c2` (chapter3c_scene2_1playcam, the farmyard) passes with no new translation. Free play from frame 329;
  the script moves on to chapter3c_scene3_2playcam at frame 10748. Cooper fights class 0xD ghoulies and keeps all 5
  of his energy.
- Checked in the captures: Cooper crossing the farmyard (3000) and by the fenced shed (9000).

### 2026-10-28: chapter 3c scene 1: the mummies

- `--scene3c1` (chapter3c_scene1_1playcam, the foundry) reaches no untranslated code but does not pass.
- The script runs in this order:
  1. Free play, then region 1, which shows the hint chapter3c_scene1_ribstakeextinguisher.
  2. An untitled section with three waits: a 60 s timer (node 0x658), PlayerApproachesDoor 0x34 within 75 (node
     0x678), and a SpecificGhouliesAlive count of 3 mummies (node 0x698).
  3. "exit door closes", then the event "number (mummy)", set up by node 0x95C (type 0x25) and node 0xAB8 (type 0x5F,
     6..6).
  4. "exit door opens", and the final free play.
- The challenge never started: the script cases for node types 0x25 and 0x5F (in 0x00126140) were stubs that built
  their tags and counts and dropped them. Now translated:
  - Type 0x25: step type 8 (0x00125E80, delay 1.5), +0x18 0, the named tags at +0x1A, their count in the byte at
    +0x4C, queued through 0x000D05D0.
  - Type 0x5F: step type 0xE (0x00126000), +0x18 the count 0x0001BB60 draws from node+8 to node+0xC, queued.
  - The icon count (0x000D0F20) gains types 8 (the tag at +0x18 once set) and 0xE (0x000D01A0: the target less
    0x0012AE40 plus the new 0x0012AE90, `script_sum_paired_statistics`).
  - The runners from the 0x003039CC table: `0x000CFFE0`, "next one different" (a new kill of the current tag fails
    the step, a kill of another tag becomes the current one), and `0x000D0160`, the number step.
  - The script switch's other stubs, types 0x22, 0x23, 0x26, 0x27 and 0x87, are still stubs; none is reached here.
- Checked in the captures: the challenge HUD shows the RIP stone with 6 and the red X (4500, 6000), with the furnace
  open and burning behind the mummies (6000).
- Still failing: the count stays at 6. Cooper's punches knock the six class 0x22 mummies down but none dies. The
  cursed mummy (class 0x23) then takes 10 energy about every 600 frames until he faints (frames 6305..8259), and the
  scene restarts. How the mummies are meant to die (the furnace, or the extinguisher the hint names) is not settled.

### 2026-10-28: chapter 3b scene 7: the barn

- `--scene3b7` (chapter3b_scene7_1playcam, the barn) first stopped on the retail resource error screen at frame 680,
  with the last missing resource aid_anim_ghoulies_actor_skeleton_noegganims_turnStep.
  - Under cdb the screen (0x00032700) came from the struggle state (`update_runtime_struggle_animation`, 0x000A3070)
    starting its in-band record.
  - The scene's own bundle (aid_script/ghoulies_chapter3b_scene7_1playcam.bnl) lists the animation as
    ..._noegganims_turnstep. The skeleton's table names it with a capital S.
  - The host bundle lookup `find_entry` compared names exactly. An AID is its hash, and `aid_hash` folds case, so
    `find_entry` now falls back to an ASCII case-insensitive match. The run then reaches no untranslated code.
- Still failing: the script waits on PlayerEntersRegion (condition 8) for region 10, at (942, 73), radius 35.
  Cooper, carrying an extinguisher, is held against a low pen wall in the barn at about (150, 175) for the rest of
  the run. The class 0x19 ghoulies and the way east are beyond it. What opens the way is not settled.
- Checked in the captures: Cooper with the extinguisher at the pen wall (3000).

### 2026-10-28: chapter 3b scene 6: the foundry key

- `--scene3b6` (chapter3b_scene6_1playcam, the foundry) passes with no new translation. The script moves on to
  chapter3b_scene7_1playcam at frame 5947.
- "event - find the key/don't kill" arms the bg-break key at (-136.9, 24.4, -60.5), up behind a barrel. The nearest
  open cell is 19 from it, so the driver, which punches within 16, stood there for the rest of the run. Change to the
  driver: once Cooper stands on the key's approach cell (within 6), he punches at a key within 24. The key comes out
  at frame 1457.
- Then "event - one type/time limit" against the class 0x12 haunted doors. Cooper goes from 25 to 13 energy.
- Checked in the captures: the key floating out with its sparkles (1500), and Cooper fighting a haunted door at 0:38
  (3600).

### 2026-10-28: chapter 3b scene 5: the sleeping ogres

- Type 0x72, entityStateGrabAndThrow (getter `0x000947A0`, vtable 0x00505900), in
  `entities/body_grab_throw_state.cpp`. It has the sizes, construct, destroy, tick, step and message slots, the hand
  test `0x00095000`, and the throw `0x000951B0`.
  - Construction: the mind's target becomes the victim, held through victim body +0x718. The marker is the kind 0x1A
    one whose +8 word is the actor's +0x1FC key, otherwise the farthest.
  - The grab: between 0.15 and 0.25 of the animation, the hand shape (parameters +0x10) is swept against the victim's
    bound. Message 0x3C, then 0x3D on the reply. The victim is held at the hand's model binding (parameters +0x12)
    and turned toward the marker.
  - The throw: past 0.55 of the animation. It follows the vampire's throw (`0x0008BF90`) but with damage keys
    0x34 / 0x15, parameters 0x68 and +0x27C 3. It first places the victim level with the actor and clear of
    contacts (0x001227E0, 0x000FC3F0, 0x00122480), then sends the actor message 0x57.
- The body services gain `markers` (DAT_0051108C). Type 0x72 joins the tick, step, message child, collision reply,
  update and draw lists. The collision reply raises for id 0x7C, the one id 0x00094FA0 reads past.
- `--scene3b5` (chapter3b_scene5_2playcam, the ogres' lane) passes. The script moves on to chapter3b_scene6_1playcam at
  frame 4933.
  - The three class 0x1E ogres sleep, and each throw costs 5 energy. With the driver fighting, Cooper woke them and
    fainted after every second or third throw. So the mode fights nothing (range 0) and takes an
    `aid_ghoulybox_powerups_supersoups_energyboost5` soup first (kind 3).
  - He is still thrown three times on the way (frames 1859, 2546 and 3245) and reaches the exit with 5.
- Checked in the captures: a sleeping ogre beside Cooper (2100, in a fighting run), an ogre over Cooper with another
  asleep behind (1800), and Cooper running on along the lane (2400).

### 2026-10-28: chapter 3b scene 4: survive the workshop

- `--scene3b4` (chapter3b_scene4_2playcam, the workshop) passes with no new translation. The script moves on to
  chapter3b_scene5_2playcam at frame 15585.
- The event, "event - survive/no weapons", lasts 90 seconds: node 0x448, type 0x1E, 0x42B40000. Its TimeOut
  conditions are waves, not the end: skeletons at 10 s (node 0x5D4), and a cursed mummy box at 30 s and at 60 s
  (nodes 0x684 and 0x7AC).
- With 15 energy and the class 0x34 skeletons, Cooper fainted before the 90 seconds ran out. Changes to the mode:
  - It passes `aid_ghoulybox_powerups_supersoups_energyboost5` as its powerup box (kind 3). The soup adds 5 energy.
  - It fights only while the title is the event's, so after the event Cooper walks to the exit
    (-194.7, 299.5) instead of chasing the skeletons that are left.
  - The driver now fetches the powerup box again after a faint, since the restart brings the boxes back.
- Run: Cooper faints once in the first event (frame 7206). He survives the second, which ends at frame 15093, and
  leaves.
- Checked in the captures: the event clock at 0:57 with the skeletons round Cooper by the furnace, energy 21 (11400),
  and Cooper leaving among the skeletons (15300).

### 2026-10-28: chapter 3b scene 3: the warlock

- `--scene3b3` (chapter3b_scene3_1playcam, Dunfiddlin Cottage) reaches no untranslated code but does not pass.
- The scene script's first free play waits on GhoulyExistsInScene (condition 16, link 0x3CC) for tag 0x3A, the
  warlock. He is in the `aid_ghoulybox_single_warlock` bg-break box at (159.7, 36.3, -297.7), so the mode passes
  that box as its powerup box with kind 0: Cooper breaks it first, then fights what comes out.
- Checked in the captures:
  - The box breaks at frame 1047.
  - Cooper fights the warlock (class 0x3A) in the hallway (1200).
  - The warlock's intro card plays (1500), and the title becomes "free play (find warlock)" at frame 1409.
- Still failing: no condition is pending after that, and the script holds there. The warlock reappears downstairs
  (about y 1, for example at 280, 119) while Cooper is upstairs (y 23). The driver skips ghoulies more than 15 away in
  height (the rule kept for scene 2a2's plate-climbing zombie), so it walks Cooper to the shut exit (192.9, -340.8)
  instead. What the script's next node (0x618, type 0x78, objTag_Actor_ImpFlying) waits on is not settled.
- In the first run, before the box was passed, Cooper lost one energy point about every 300 frames while he waited
  by the exit. The cause is not settled, and the drain did not show once the box was broken.

### 2026-10-28: chapter 3b scene 2: the worm

- Type 0x2B, entityBodyWorm (getter `0x00076090`, body `0x000760A0`, vtable 0x00505308), in
  `entities/player_factory_owner.cpp`. It has the zombie's slots except +0x20, +0x2C and +0x4C, which are in
  `entities/body_action_ids.cpp`:
  - `0x00076100`: 0x10C while bit block byte 2 bit 3 (bit 0x13) is set, otherwise the parent's.
  - `0x00076130`: 0x165 while bit 0x13 is set, otherwise the parent's.
  - `0x00076160`: the death key, 0xAB while bit 0x13 is set, otherwise 0xA9.

  Type 0x2B joins 0x2D in every body list in `scene_body_messages.cpp` and `scene_actor_services.cpp`.
- Type 0x57, entityStrategyWorm (getter `0x000C4580`, vtable 0x00506080), in `entities/worm_strategy.cpp`. It has
  the sizes, construct, destroy, message, update, enter and leave slots, the effect mode `0x000C4C70` and the timer
  `0x000C4D60`. While holding, it wanders ("far") until it claims a target queue, then attacks:
  - It loads "attackprepare" while bit 0x13 is clear, then "attack".
  - Once bit 0x13 is set, the timer at +0x138 (5 seconds) runs down, and at zero the worm rests (0x400, request key
    0x67).
  - Its animation event kind 0x3A naming flag 0x13 makes it emerge: scale 1.25, effect mode 1 (the +0x38 object),
    tag bit 8 and actor message 0x4A.
  - Being hit gives effect mode 2 (the +0xB8 object, latches raised).
- New shared helpers:
  - `0x0002B970`: the target queue join that marks the link active (strategy +0x30), in
    `strategy_target_queue.cpp`.
  - `0x001394D0`: the hit sender becomes the mind's target, in `strategy_target_selection.cpp`.
  - `0x0001D830`: body +0x6E and the first live attachment's +0x16 tag, in `body_attachment_requests.cpp`.
- Scene bindings for the worm strategy:
  - `event_at`: message 7's animation event record, by its bound address.
  - `flag_names`: 0x00338FB0.
  - `select_request`: 0x0001D210 on the mind graph's attachment requests, through
    `SceneMindMessages::select_attachment_request`.
- Type 0x68, entityStateExplode (getter `0x0008E760`, vtable 0x005057E0), in `entities/body_explode_state.cpp`.
  1. Once its animation slot 0 is done, the actor is hidden and taken out of collision, with callout 0x17.
  2. For 0.3 seconds a blast that shrinks from 25 to 8 sends each actor in reach message 0x21. An unanswered 0x21
     gets a 0x0001F8C0 contact.
  3. Once its sounds end, the body dies (0x00134AE0).

  The 0x21 notice (`RuntimeBlastNotice`) is passed on as words to players and props. Nothing on the native path
  answers 0x21 yet. The 0x0001F8C0 velocity is DAT_0050DD80 (zero) and its word is 0. The stack block the dump
  passes as its fifth argument is never read.
- `--scene3b2` (chapter3b_scene2_1playcam, the garden) reaches no untranslated code, but does not pass. Its end
  conditions are three PlayerEntersRegion conditions (condition 8: regions 3, 1 and 2, words 0x3B8, 0x414 and 0x4E0).
  Region 2's volume sits on the worm. The mode's fight range is 50, so Cooper walks the regions and fights only what
  is near. With 1000 he chased every ghouly in the garden and entered none.
- Checked in the captures of the final run:
  - Cooper enters region 3. At frame 7200 in an earlier run, and from frame 7560 in the final one, the worm comes up
    out of the ground (red, with its effect particles) and hits him.
  - The driver now logs a body that enters type 0x68, and captures every 10 frames for a second after. The worm
    enters it at frame 7991. At 8000 it stands beside Cooper. At 8040 there is an orange blast with sparks, and Cooper
    is thrown back, losing 7 energy (16 to 9) at frame 8030.
  - At 8400 the worm is gone. The driver goes on hitting it until frame 8640, while the state waits for the worm's
    sounds to end before it dies (0x00134AE0). After that, nothing targets it.
- Still failing: Cooper never reaches region 1 (-259.7, 139.8). From frame 8760 to the end he is held at the garden
  gate in front of the cottage (about -257, 75; capture 20100), although the gate leaf looks open. Class 0x36
  ghoulies take him from 15 to 1 in one hit (frames 2391 and 5459), and he faints once.
- What holds him at the gate, found with temporary prints that were then removed:
  - His movement probe (0x001226C0, kind 4, mask 0xC0000) hits the garden background's static physical mesh (shape
    type 1, 0x00053AA0), not a jointed part. The triangle's +0xC flags are 0x4 and its category (+0x10) is 0.
  - The mesh walk collides when the probe's first word, 4, shares a bit with the flags. The second word, 0xC0000, is
    the excluded set. So bit 4 blocks the player.
  - Region 1's box (half extents 45 and 55, yaw 0) starts at z 84.8, behind that wall at z 79.
- The scene script (68 nodes) does nothing to category 0:
  - At the start, node 0x2CC runs type 0x67 on category 255 with a nonzero word (0x0012C930 ORs bit 4). That shuts
    60 event triangles to the player.
  - Node 0x4E0, reached on region 2, clears it again (0x0012C950).
  - Node 0x414, reached on region 1, plays aid_cutscene_ghoulies_chapter3b_scene2_gate and sets category 0xFB.
  - Part removal (0x00044EF0) only clears categories numbered row + 1.

  So the gate triangle stays a wall, and in the retail game the way to region 1 is probably not through this gate.
  The test's navigation grid walk still routes Cooper through its cells, and the driver has no other route. This is
  not checked against xemu. In another run Cooper was held inside region 3 (about -157, -37) from frame 10800 instead.

### 2026-10-28: chapter 3b scene 1

- `--scene3b1` (chapter3b_scene1_1playcam) passes with no new translation: free play from frame 358, and the script
  moves on to chapter3b_scene2_1playcam at frame 7924. Seen in the captures: Cooper fighting class 0x1B ghoulies
  among the greenhouse plants (1200, 4800) and walking to the exit door (7800). Cooper's energy stays at 10.

### 2026-10-28: chapter 3a scene 12: the vampire's bite

- **entityStateBloodSuck** (factory type 0x61, getter `0x0008B200`, vtable 0x00505700; the factory table's row 0x60
  carries the name) in `entities/body_blood_suck_state.cpp`:
  - sizes `0x0008B1E0` (0x180 / 0x124), init `0x0008B260`, destroy `0x0008B530`, tick `0x0008B6C0`, +0x14
    `0x0008B9C0` and message `0x0008BB90`. +0x18 is the empty `0x001628B0` and +0x1C `0x0002B620`. The tick's and
    the phase change's switches were re-exported with their jump-table seeds.
  - Phase 1 reaches for the mind's target. Once the parameters' grab shape (mask 0x20002) meets the target's model
    bound (`0x0008BD80`), the target is sent message 0x3C. If it accepts, it is sent 0x3C again, gets the grab
    callout, and is pulled in over the grab animation. The body's latches, the target's holds, live holds and bit
    0x3A go up.
  - Phases 2 (bite) and 3 (struggle) hide the target's render bins and hold it in front of the vampire (+0x14). It
    loses one energy a second (`0x0008BE60`, unless it is down or may not be hurt), and its pad's shaking
    (`0x00109810` over 0x00302728) moves it between the two. A full struggle (30) is phase 4.
  - Phase 4 throws it off: `0x0008BF90` deals hit 0x47 (else the actor's 0x28), sets its death motion and sends
    0x6E. Phase 5 releases the rider and ends in body slot +0x38 (+0x20 when nothing was grabbed).
  - New helpers: `0x00021190` (the body is down or may not be hurt) and `0x0003B1B0` (slot 0's animation position).
- Type 0x71 (getter `0x00095550`, vtable 0x00505920) in `entities/body_grabbed_hold_state.cpp`: the state message
  0x3C puts the grabbed body in. It drops what it holds and raises its latches, holds, +0x30, +0x309..+0x30C and bit
  block byte 4 bit 1 until it is destroyed. Its sizes are actorMove's `0x00095530`. The factory table's row 0x70
  names it entityStateGrabbed, the name `body_grabbed_state.h` gives type 0x70 (row 0x6F there is entityStateGetUp).
  The existing file is not renamed.
- `--scene3a12` (chapter3a_scene12_1playcam, the greenhouse) reaches no untranslated code but does not pass. See the
  test table. Seen in the captures: the vampire standing in his coffin holding Cooper up off the floor (19500).
- Not settled: one earlier run went black from frame 1800 to the end (HUD included). Two later runs did not repeat
  it.

### 2026-10-28: chapter 3a scene 11: the vampire's hits

- `0x00020690` (`receive_runtime_body_knock_back`, in `entities/body_hit_messages.cpp`): the knock-back hit the
  vampire's 0x406 first stage (`0x000C1920`) sends his own body (actor +0x95C). It is a reaction hit (1, 0x32, 0x65)
  at his position. It faces along his horizontal movement (actor +0x3C), pushes along the body's horizontal +0x1D8
  heading, and names the first background (DAT_00511B20 - 0x934) as its source. Either direction falls back to
  straight up when it has no horizontal part. It goes to `0x00020DC0` with no reply. The scene binds it as
  `NativePlayerSceneBindings::knock_back`, and `attached_actor_at` now gives a background (type 4) a source view.
- `0x00022FB0` (`receive_runtime_body_front_hit`): the vampire body's moving contact from in front.
  - The amount is the sender holder's (+0xD8C) class value 0x41 (`0x00133BD0`) when the vampire is player-masked.
    Otherwise it is his own value 0x22 (`0x00028DA0`), raised to the body's +0x94 for a live player sender.
  - Below +0x94, while the bit block's byte 8 bit 0 is clear, the hit is the lighter (0xD, 0x42, 0x67), and he is
    first sent actor message 0x1A (it shuts him in). Otherwise it is (0xC, 0x42, 0x68).
  - The packet is otherwise the moving contact's (`0x00022E40`), and the reply (`0x00020F80`) always follows.
- `--scene3a11` (chapter3a_scene11_1playcam, the vegetable garden) reaches no untranslated code but does not pass.
  See the test table. Seen in the captures: the vampire springing out of his open coffin at Cooper, with bats
  (3000).

### 2026-10-28: chapter 3a scene 10: the vampire

- **entityBodyVampire** (factory type 0x2C, getter `0x00075EC0`, body `0x00075ED0`, vtable 0x005052A8; the factory
  table's row 0x2B carries the name). It has the zombie body's slots except for:
  - +0xC `0x00075FC0` (`dispatch_runtime_vampire_body_message`): a moving contact (0x19) from an attacker whose
    +0xDAC is 0xC lands only from -20..75 degrees off the vampire's yaw while the bit block's byte 1 bit 0 is clear.
    Such a contact then goes on to `0x00022FB0` (translated for scene 11). Any other contact from that
    attacker is answered {1, 0, 1}. Every other message goes to the parent's `0x00074080`.
  - +0x20 `0x00075F30`, +0x30 `0x00075F90` and +0x34 `0x00075F60`: 0x29, 0x2A and 0x25 while the bit block's byte 8
    bit 0 (the shut coffin) is set, else the parent's. +0x2C stays the base `0x00026A90`.
- **entityStrategyVampire** (factory type 0x54, getter `0x000C2000`, vtable 0x0050602C) in
  `entities/vampire_strategy.cpp`:
  - sizes `0x000C1FE0` (0x5C / 0x18), init `0x000C2050` (record 0x003034D0), update `0x000C2070`, message
    `0x000C2140`, enter `0x000C2350` and leave `0x000C2420`. All four switches were re-exported with their jump-table
    seeds, and their tail jumps are separate functions: `0x000C1B70`, `0x000C1C80`, `0x000C1DC0` and `0x000C1940`.
  - He waits in the coffin (0x401, "randomwander"). Once the record's 1 s delay has run, he springs at a target in
    reach (`0x000C17C0`): the attack roll opens the coffin (0x3EF, "special", then "coffinclose" after 5 s or once
    the target is held by another). Otherwise the knockdown roll picks 0x3EE ("knockdown", then "coffinopencheer" or
    "coffinclose"), and the rest go to 0x3ED ("knockback", then "knockbackfollowup" half the time after a hit, then
    "coffincloseknockback").
  - A hit from behind while he waits (`0x000C1640`, the record's 0.4 dot) is 0x406. Its first stage calls
    `0x00020690`, the knock-back hit on the body (translated for scene 11). It then walks 100 off and
    looks round ("coffinlook").
  - Message 0x1A shuts him in (0x416, "waitingforfollowup"): the body's byte 8 bit 0 is set and body +0x288 is
    "opencoffin".
- `--scene3a10` (chapter3a_scene10_2playcam, the village) reaches no untranslated code but does not pass. See the
  test table.
- No vampire was picked out on screen in scene 10. The captures show the village path and the club-wielding
  class 0x24 ghoulies that knock Cooper down (5100, 12600). Scene 11's captures show him.

### 2026-10-27: chapter 3a scene 5: the slam smash attack

- **entityStateSlamSmashAttack** (factory type 0x82, getter `0x000A03D0`, vtable 0x00505B20; the factory table's row
  0x81 carries the name) in `entities/body_slam_smash_state.cpp`. It is a leap that lands as a slam.
- Its slots and helpers:
  - sizes `0x000A03B0` (0xD8 / 0xA0), init `0x000A0430`, destroy `0x000A05A0` and update `0x000A0630` (table
    0x000A0994);
  - the phase change `0x000A09B0` (table 0x000A0B40) and the landing point `0x000A0B50`, both exported with
    `range-function-force`;
  - the message, +0x14 and +0x18 slots are the empty `0x001628B0`, and +0x1C is `0x0002B620`.
- The phases:
  1. take-off, until slot 0's animation ends;
  2. crouch for 0.5 s, with the landing point picked (the mind's target, else 40 ahead) on its cell bottom;
  3. a 0.7 s leap rising 200 over the point (callout 0x3F where it left);
  4. a 0.5 s drop at scale 2 with the contact shapes live;
  5. landed: the parameters' "aid_" effect, callout 0x4C, and the body's death effect.
- Wired in `SceneBodyMessages::slam_smash_context`. The scene's type lists for the empty +0xC, +0x14, +0x18 and the
  +0x1C next key now include 0x82, and its 0x4A to its own body goes through `dispatch` (body type 0x27 has no scene
  object view).
- Not settled: which actor in scene 3a5 uses the state was not picked out on screen.

### 2026-10-27: chapter 3a scene 4: the warlock

- **entityStrategyWarlock** (factory type 0x56, getter `0x000C3CC0`, vtable 0x00506064; the factory table's row 0x55
  carries the name) in `entities/warlock_strategy.cpp`. It has seven slots: `0x000C3CA0`, `0x000C3D10`, `0x000C3E90`
  (the entrance camera's end), `0x000C3EF0` (its jump table at 0x000C4020 / 0x000C4044), `0x000C3EA0`, `0x000C40B0`
  and `0x000C40F0`. The project had none of its privates as functions, so they were exported with
  `range-function-force`:
  - `0x000C2DB0` and `0x000C2ED0` (the hold, table 0x000C3140);
  - `0x000C3160` (the mode change, table 0x000C3358);
  - `0x000C3370` (the move to a random point of a random mask 0x20 record);
  - show / hide / solid: `0x000C33E0`, `0x000C3410`, `0x000C3440`, `0x000C3490`;
  - `0x000C34E0` (the fade) and `0x000C3570` (the squash and stretch);
  - attacks: `0x000C3AC0`, `0x000C3B20`, `0x000C3BE0`;
  - `0x000C4130` (whether it may react);
  - the "warlock entrance" camera: `0x000C4170`, `0x000C42F0`, `0x000C4440`.
- Its record at 0x003034F8 holds the attacks allowed by energy (1 to 4), the modes' times (hold 3.5, vanish 0.5, gone
  2), the stretch (share 0.6, squash 2 x 0.5, stretch 5 high, rise 25), the entrance (1.5 s) and the 250 scare radius
  (the inlined `0x0013B660`).
- Hold modes (strategy +0x3C):
  1. shown and waiting;
  2. vanishing: it fades out, squashes, then stretches and rises, and its effect object is made;
  3. gone: moved away;
  4. reappearing;
  5. waiting to attack (0x3EE).
- A hit while it holds (message 0x6E) restarts the hold with the short times, so it vanishes at once.
- New beside it: `0x0002B410` (inlined, the actor's height) and `0x00124A10` (`random_occupancy_record` in
  `navigation/occupancy.cpp`).
- Driver: scene 3a4 breaks the warlock's box first (`powerup_box`, kind 0).
- Not settled: the entrance camera runs only with story byte 3 bit 0x20, which was clear in these runs, so it was not
  seen. After a faint the scene restarts without its warlock.

### 2026-10-27: chapter 3a scenes 1 and 2: the haunted television and the homing projectile

- Scene 1's book and playcam needed nothing new.
- The haunted TV's target check (0x00141530) read the target's mind (actor +0x958) through the actor-sized
  `actor_bytes_at`, past the mind's object; the strategy views gained `mind_bytes_at` (0x14 bytes, through +0x10).
- **entityWeaponProjectileHome** (factory type 0xA8, getter `0x000CC790`, vtable 0x00506398; the factory table's row
  0xA7 carries the name) in `entities/homing_projectile.cpp`: its slots `0x000CC770` .. `0x000CD090` (four jump
  tables decoded) and privates `0x000CD120` .. `0x000CD3A0`. It flies in weapon state 10, so the weapon's follow
  update is translated too (`entities/weapon_motion.cpp`): `0x00149750`, the steering `0x00149D20`, `0x00149C50`,
  `0x00149C30`, `0x00103330` and `0x00105B90`. The root record `0x000CC010` now takes its outputs (the thrown
  projectile passes +0xF24 / +0xF30, the homing one +0xE70 / +0xE7C). The weapon services gained `target_at` (a
  mind target record by address) and `player_cells` (a player's +0xCE8 cells).
- The body projectile command (`0x0001A3C0`) looked its new object up in a map nothing filled; the scene now holds
  the projectile from its creation, it is found by address, and its native `spawn_owner` follows the +0xD8C word.
- Message 0x2A (a direct hit, `0x000205B0`) now binds its sender as the source view.
- Found with AddressSanitizer (`build-asan`, `/fsanitize=address`): the follow entry `0x001495C0` allocated retail's
  0x34 bytes for a host path search of 72 (64-bit pointers), and scene teardown read a player's position after its
  native view was freed (`released_binding`); the position is now kept by the `released` hook.

### 2026-10-27: chapter 2c scenes 10 to 13: the hallway, kitchen, wine cellar and Amber

- Scenes 10 and 11 (and 11's and 13's first books) needed nothing new.
- **entityStrategyGhoulieGirl** (factory type 0x3D, getter `0x000AC160`, vtable 0x00505DA8; the factory table's row
  0x3C carries the name) in `entities/ghoulie_girl_strategy.cpp`: its six slots (`0x000AC140` .. `0x000AC460`, the
  message's jump table at 0x000AC35C / 0x000AC378) and the privates `0x000ABC60`, `0x000ABD00`, `0x000ABD60`,
  `0x000ABE10`, `0x000ABE80`, `0x000ABF60`, `0x000ABFA0`, `0x000AC020` and `0x000AC090`, none of which the project
  had as functions (exported with `range-function-force`). Its record at 0x00302B68 is 0.25, 4, 3, 1.
- Driver: a `powerup_kind` of 0 breaks a box that holds ghoulies, then fights; a box more than 16 from open floor
  (scene 2c12's racked barrels) is punched standing from within 24.
- Not reliable: scene 2c10 (see its row).

### 2026-10-27: chapter 2c scenes 8 and 9: the paintings, the cloakroom and scene restarts

- Scene 8 (the haunted paintings) needed nothing new.
- Script type 0x28 builds step type 0xC (`0x00125F80`, XMM0 1.5) and queues it (`0x000D05D0`); it was a silent
  no-op. Step type 12 (`0x000D0140`, from the step table at 0x003039CC, two words per type) fails the event once the
  running score is nonzero while the steps do not wait: the cloakroom's event is to score nothing.
- DAT_00545978 (broken parts) and DAT_00545980 (running score) each had two host copies: the script VM cleared one,
  the scene's services counted into the other. Both are now `g_545978` / `g_545980` in `scripts/script_vm.h`.
- A restarted scene (after a faint) broke twice in the host-only `SceneActorServices::State::reset_scene_data`, which
  runs after the loading scene's script has started:
  - It cleared DAT_0051248C, the script's title mode. The restarted cloakroom's FREE PLAY (mode 0, id 1) then
    matched the cleared state, never activated, left DAT_00512514 clear, and its section end let the script run
    out: the scene moved on at once.
  - It zeroed the pause counts DAT_0050E764 / DAT_0050E768 after the loading scene's first pause (`0x00117400`), so
    FREE PLAY's two releases left them at 0xFFFFFFFF and every actor paused for good.
  Neither is cleared by the retail functions that reset stands for. `0x001287F0` clears the title mode (already
  translated), and `0x00116E00` clears the pause counts before the scene loads: that part, with its DAT_00545978 and
  DAT_00545980 clears, is now in `release_scene_music_at_start`.
- Driver: scene 2c9 runs calm in FREE PLAY, so Cooper scores nothing and walks to the exit.
- Not checked: the restart fixes reach every scene that restarts after a faint; only scenes 2c8 and 2c9 ran after
  them (2c8 before the restart fixes).

### 2026-10-27: chapter 2c scene 7: generic ghoulies and the cursed mummy

- **entityStrategyGeneric** (factory type 0x3C, getter `0x000ABB80`, vtable 0x00505D8C) in
  `entities/generic_strategy.cpp`: constructor, message (`0x000ABC50` / `0x000AB420`), update, enter, leave and the
  privates `0x000AB900` (the once-a-second reselection), `0x000ABA40` and `0x000ABA80` (the attack goal by the held
  weapon). The sizes are the haunted chair's `0x000ABB60`. A held gun's choice (`0x000ABB10`) raises.
- **entityStrategyMummyCursed** (factory type 0x4D, getter `0x000BDA20`, vtable 0x00505F68) in
  `entities/mummy_cursed_strategy.cpp`: all seven slots and the privates `0x000BC6D0` to `0x000BE070`, plus
  `0x0013B840`. It curses a watched player (0x3EF, "deathcurse", callout 0x42, the object named at +0xBC) or grabs a
  target it claims as a rider (0x3EE, eight phases at +0x144 through the slab whack). A landed curse shows the
  baddie meter: `0x0012E5E0` / `0x0012E6C0` are in `frontend/scoreboard_update.cpp` over new DAT_00545A28..A60 words.
  The scene binds its patched spawn, `0x00112CC0`, `0x0013B1F0`, `0x0013B660` and the meter. Callout tokens go to the
  strategy's blocks by pointing `command_attachments` at them around each dispatch; Jessie Clyde's and the arc
  state's dispatches pass the block address but leave the span on body +0x2D8, so their tokens likely land there.
- **entityStateDroneAppear** (factory type 0x65, getter `0x0008D350`, vtable 0x00505780) in
  `entities/body_drone_appear_state.cpp`: a ghoulie appearing or vanishing over half a second, its alpha
  (`0x0008D5F0`) and scale (`0x0008D690`, translated from the instructions) easing with the time left.
- Script: type 0x90 retires the tagged DAT_00511B38 actors through the inlined `0x00112CC0`
  (`SceneActorServices::retire_tagged_actors`); condition nodes 0x65, 0x8C and 0xA3 record opcodes 12, 20 and 26.
- The strategy goals' body-slot read attaches the body methods on first use, as the attachment's does.
- Driver: 36000 frames (was 24000).
- Not settled: the baddie meter's draw (`0x0012E710` / `0x0005C1A0`) still raises once the meter shows; the runs so
  far never landed a curse long enough to reach it. Cooper does not beat the mummy before its whacks knock him out.

### 2026-10-27: chapter 2c scene 6: Jessie Clyde

- **entityBodyJessieClyde** (factory type 0x29, getter `0x000759F0`, vtable 0x005051E8; the type names sit one row
  early in DAT_0030D41C) over the human body (0x27): the zombie body's slots but for +0x20 and +0x2C, `0x00075A70` /
  `0x00075AA0` in `entities/body_action_ids.cpp`, which pick actions 2 and 3 while the body's bit block byte 1 bit 7 is
  set, else the parent's. The scene's body checks and the child message route accept the type.
- **entityStrategyJessieClyde** (factory type 0x4A, getter `0x000B9440`, vtable 0x00505F14) in
  `entities/jessie_clyde_strategy.cpp`: sizes, constructor (`0x000B9490`: calm, the effect name, callout 0x11's tokens
  at +0xF4, the model's node 0x10 when the record sets bit 16), destructor, update, message, enter and leave, and
  their private callees `0x000B8080` to `0x000B9B50`. Calm, she walks about until struck, provoked or seeing her target
  within 70 inside a quarter turn of her facing (`0x000B8270`), then transforms (`0x000B83A0`: "transform", her effect
  and every latch up, then "noflower" at body +0x288 and the body bit that changes her actions). Angry, a 2 s cue,
  then the shout (whose animation event scares the room through `0x0013B270`, bound as `scare_at`), the knockdown or
  the special. `0x000416C0` (a record's point within a radius of the actor) is in `entities/actor_transform.cpp`.
  The switch bodies are behind jump tables the temporary export does not follow; they were exported with
  `range-function-force:` seeds read from the tables (separated by `+`, since cmd splits on commas).
- Stale pointer fixed: the bg-break key held its key's `FactoryObject*` from `keep_object`, which points into the
  scene's object vector, and later objects (Jessie's effect) moved it, so the key's show read freed storage (an
  `unordered_map` key error). The key's update now finds it again from its +0x9F8 address. Other `keep` callers use
  the pointer at once; a pointer-stable object list would cover them all, but it reaches a dozen contexts.
- Driver: while an event has armed a bg-break key (+0x9BC) and none is out (+0x9C0), Cooper walks beside it and
  punches, as for the powerup box; FREE PLAY is fought.
- Not settled: after a faint (with FREE PLAY calm, the imps knocked Cooper out), the restarted scene skips "FIND THE
  KEY" and plays "EXIT DOOR OPENS" without opening the door, so it cannot be left; some state of the found key
  outlives the restart. The passing run has no faint.

### 2026-10-27: chapter 2c scene 5: the walled garden's water

- Background part kind 7 is a water surface: `0x00050940` builds a flat 28 x 28 sheet (`0x00050220`, every border
  point pinned, each point starting with DAT_0050DD80's force) that rustles with the water footstep. `0x001153A0` gives
  it the level hook `0x00114580` (each point pulled back toward the first point's height, 0.2 of the squared offset),
  the ripple hook `0x001145F0` (an actor inside its bounds whose body spans the middle height pushes the inner points
  out of a 4.4 + 0.4 sin(5 DAT_004312CC) ring 3.4 above it), damping 0.01, layer 1, no shadow and specular shading.
- `0x0004BA90` (the specular shading) is translated, and `0x0004C000` now calls it instead of `0x0004B800` for
  +0x1EE. `0x0005D710` draws the water: `0x0005D470`'s strips with the texture rows and columns waved 0.02 by the sin
  and cos of 20 times their share plus DAT_005462B0, which advances 0.01 a draw.
- Use-after-free fixed: the script's type 0x60 (`0x00112CC0`) retires the brambles one of the doors' clips holds,
  and `0x001232D0` frees them while the clip plays on, so the clip's release (`0x0004E9E0`, at teardown or a
  restart) read a freed actor (an `unordered_map` key error or an access violation, depending on the heap). Retail's
  row keeps the freed block; the host now clears any live cutscene row naming an actor as it is destroyed.
- Driver: a region's nearest record is the goal (region 1 here is five spheres, four behind the garden wall);
  `spare_watched` holds Cooper back while a PlayerHitGhouly condition is live; every frame on which Cooper's energy
  (body +0x94) changes is logged, and the occupancy records are listed at frame 0.
- Not settled: whether the five-heart shovel hit is retail's (it comes from the statsheet's per-class damage for the
  pirate under the 100% special attack attributes); the scene does not pass.
- Checked in the captures: the garden with the skeletons digging, the pirate and the flying imp by the dungweed
  (frame 2400). The water surface was not picked out on screen.

### 2026-10-26: chapter 2c scene 4_2: the SSS door

- **entityAvatarMiscSSSDoor** (factory type 0x20, getter `0x000711A0`, vtable 0x00504EC8) in `entities/sss_door.cpp`:
  sizes `0x00071180` (0xA54 / 0x450), init `0x000711E0`, update `0x00071380`, message `0x000715B0`, trigger
  `0x00071640` (the fourth reader of the invisibility row) and the challenge start `0x00071750`. Its other slots are
  the actor base's. New door helpers `0x0005C0A0` (the nearest door), `0x0005C130` (close it, state 7) and `0x0005C150`
  (at rest), and `0x0003B340` (`release_runtime_actor_animation_slot`).
- Door state 7 (`0x0005B540`): `0x0005A9B0` snaps the clip's first track actor to its joint 0 pose and bursts it into
  the shape's break model (DAT_004249A4 + shape * 0x94), then the clips are released. `0x0013B1F0` is now the shared
  `burst_runtime_actor_into_bits` (the transform state uses it too).
- Powerup kind 0xC (the Mini Cooper): `0x001349E0` (`spawn_runtime_placed_actor`) makes
  `aid_objparams_ghoulies_actor_minicooper` at the player, sets its body bit 7 and lists it in DAT_00512320 with its
  time. Kind 5 (event complete) now plays sound 0, as the dump has it. The Mini Cooper was picked up in one run
  (where it raised before this) but not in the passing run, so its spawn is not checked on screen.
- Checked in the captures: Cooper at the SSS door's doorway with the fright gauge up (frame 9000) and recovering after
  the scare (9300).

### 2026-10-26: chapter 2c scene 2: the cloakroom's invisibility

- DAT_00512218 is the powerup table's Invisibility row (kind 9) +0 active, not a constant: the ghouly spawn trigger
  (`0x0013E980`), the scare figure trigger (`0x0006FA40`) and the SSS dropper now read it (`runtime_invisibility_active`,
  bound by the scene). `0x00071640` reads it too and is not translated.
- The player body's powerup start (`0x00075C60`, kind 9) hides Cooper and suspends his target record (actor +0x8F8,
  `0x0012B1A0`), so the coats' and the picture's volume goals (`0x00138A30`, `0x00129640` and `0x00129920`'s +0x18 test) no
  longer see him. The body message graph never bound that record (`live_attachment_` stayed null), so the suspend
  never happened; it is now the actor's first live record on every dispatch. The same record feeds the hit bounds
  (`0x00022A40`), so hits now read Cooper's real bound: 2b9 still passes, after three faints (frame 19019) where it had
  one.
- Driver: `powerup_box` / `powerup_kind` send Cooper to a bg-break setup's ghoulybox and onto the powerup it drops
  (state 1; state 2 is collected). The object dumps print powerup kinds, container contents and setup boxes.
- Checked in the captures: the invisibility icon with Cooper unseen in the cloakroom corridor (frame 1500) and by the
  exit door (1800).

### 2026-10-26: chapter 2c scene 1: the haunted paintings

- **entityStrategyHauntedPicture** (factory type 0x43, getter `0x000B0BB0`, vtable 0x00505E50) in
  `entities/haunted_picture_strategy.cpp`: sizes `0x000B0B90` (0x50 / 0x18), the shared constructor `0x000CFFD0`,
  message `0x000B0EA0`, update `0x000B0C00`, enter `0x000B1150`, leave `0x000B12D0`, wake `0x000B1420`; it wanders
  (`0x000B0720`, "getrandomdest") and circles its target on the table at 0x00302E80 (`0x000B0680`, `0x000B0890`,
  `0x000B09C0`, `0x000B0AC0`, with the new `request_runtime_strategy_sway` for `0x0013A000`), attacking between
  0.45 and 0.55 s waits (`0x000B1320`, `0x000B1350`).
- **entityStateYoYoSpinAttack** (factory type 0x90, getter `0x000A6A00`, vtable 0x00505CE0) in
  `entities/body_yoyo_spin_state.cpp`: sizes `0x000A69E0` (0x1DC / 0x1A4), init `0x000A6A60`, destroy `0x000A6C40`,
  tick `0x000A6CC0`, message `0x000A7030`, with `0x000A70C0`, `0x000A7240`, `0x000A72E0` and the sound request
  `0x00041D40` (`request_actor_sound_at`). `call_out_runtime_hit_point` (`0x0013AD60`) now takes the callout
  directly, so the scoop-whack and yo-yo states share it.
- Checked in the captures: the Haunted Painting introduction (frame 900) and Cooper leaving by the exit (1200).
  Not checked: the paintings' circling and attacks, or the yo-yo throw (free play is calm, so Cooper does not fight).

### 2026-10-26: chapter 2b scene 10: Mr Ribs

- Script condition node type 0x75 (`0x001257A6` in `0x00125450`) builds opcode 0x12,
  scriptConditionId_ActorHasNoEnergy (the tag of the name at node+8 and the float at node+0x48); its update
  `0x00133870` (`script_tick_actor_energy_condition`) holds while an actor of that tag has energy, else counts the
  float down. The evaluation `0x00132DB0` was already translated.
- Checked in the captures: Cooper fighting Mr Ribs in the kitchen, Ribs flashing red with 13 energy left
  (frame 3000).

### 2026-10-26: chapter 2b scene 9: the pantry's haunted coats and doors

- **entityStrategyHauntedCoat** (factory type 0x41, getter `0x000AF050`, vtable 0x00505E18; row 0x40 carries the
  name) in `entities/haunted_coat_strategy.cpp`: sizes `0x000AF030` (0x54 / 0x18), init `0x000AF0A0` (on the wall
  when 3 above its floor cell: action 0xF3 and a body +0x258 hold, else 0xF2), message `0x000AF340`, update
  `0x000AF180`, enter `0x000AF5A0`, leave `0x000AF6E0`. Asleep (0x413, "sleep" / "sleep_wall") it wakes (0x417,
  `0x000AF870`: event 0xB4, the wall hold released with body message 0x4A) at 60 or on a hostile target; closing in
  (0x40C) it sways and bobs (`0x000AEBF0`): mind request 0x11 + 0x27 when the target's cell is closed
  (`0x000AEEC0`), else 0x10 to a heading-search point (`0x000AEF10`, `0x0013AA50`), and attacks (0x3ED, "attack",
  `0x000AF830`) once the target is near, the body settled and the target within 60 degrees ahead (`0x000AF750`,
  new `runtime_actor_faces_point` for `0x00137F50`).
- Weapon scene start `0x00144FD0` (+0xC18 set) finds its background part (`0x0013B980`, the scene's background
  list now on the weapon services) and messages itself {2}, or clears +0xC18.
- The furnace-hit scorch `0x000965C0` is translated in `body_hit_state.cpp` (the knockdown sweep's probes and
  0x0013FC50 binding).
- Powerup kind 5 completes the event sequence: `0x000D0870` (`complete_script_event_sequence`).
- **entityStateSpawnGhouly** (factory type 0x85, getter `0x000A1A10`, vtable 0x00505B80) in
  `entities/body_spawn_ghouly_state.cpp`: init `0x000A1A70`, destroy `0x000A1B60`, tick `0x000A1BE0` with the
  placement `0x000A1FD0`; the haunted doors' imp spawns.
- Driver: free play is calm (as in 2b7); the doors still stand after their event and otherwise wear Cooper down
  until he faints and the room restarts. The "takes no hits" rule now counts its 20 s per ghouly, so turning between Mr Ribs and a door no longer resets it.
- Passes at frame 9537 after one faint and restart. Checked in the captures: the coats on the pantry wall
  (frame 900), the Haunted Coat introduction (1200), a coat attacking Cooper (1500), Cooper fighting the haunted
  doors under the event's door icon (4200, 8700). Not checked: that a door's imp spawn (state 0x85) actually made
  an imp; the logs show one imp in each object dump.
- `--scene2b8` no longer passes, and fails the same way at the committed `aa06925` (checked by building it alone):
  the haunted chairs wear Cooper down, "Don't Get Hit" fails and he faints at frame ~1320. On the restart the
  script's first node (type 0x51, "Remove Ribs after restart!") runs before the native scene's actors exist and
  raises. The 2b8 pass recorded below came from a build before that commit's last edits.

### 2026-10-26: chapter 2b scene 8: the giant hand

- **entityStrategyGiantHand** (factory type 0x3E, getter `0x000AC8E0`, vtable 0x00505DC4; the factory table's row 0x3D
  carries the name) in `entities/giant_hand_strategy.cpp`: sizes `0x000AC8C0` (0x164 / 0x118), construct `0x000AC930`,
  message `0x000ACAF0`, update `0x000ACA30`, enter `0x000ACBB0`, leave `0x000ACBF0`. Waiting (0x401, `0x000AC4C0`) the
  hand is hidden (`0x001364B0`) and out of the world's collision (`0x00136540`); while its target can be grabbed
  (`0x000AC590`) it tells the players {0x40, hand, distance} (`0x00122D10`) how far the target is outside its room
  (`0x000AC610`, `0x00124760`, `0x00124B90`) and grabs (0x3EF) once the target is in it (`0x000AC690`). The grab
  (`0x000AC740`, "grab") shows the hand and puts its collision back (`0x000ACCA0`, `0x001365C0`) and announces 0x6C;
  on body bit 0x2D it starts its scene challenge (`0x000AC820`, 0x000873A0 with scare mode 2). Its end announces 0x6D,
  and 0x3F0 goes back to waiting when no challenge started (`0x000AC870`).
- **Super Scary mode 2/3**: the head's model binding 2 (`0x000408D0`) is kept at +0x288 (its address in the head
  model's source) and the player is bound to it (`0x0003B500`, now `bind_runtime_actor_marker_now`, which the jump-on
  ride also uses). The release's floor height (`0x000874F0`) reads the floor cell's +2 through `0x00110BF0` /
  `0x00110EF0`; with no cell it keeps the floor index's bits, as retail does.
- Broadcasts 0x6C and 0x6D are one word.
- Driver: a ghouly fought up close for 20 s without its energy falling is left (Mr Ribs, tag 0x31, carrying the egg
  by the exit). Not rerun on the earlier scenes.
- Checked in the captures: the hand rising out of the floor with Cooper in its grip (frame 2400) and Cooper beside
  Mr Ribs at the doorway (frame 3300). Not checked: the button prompts of the challenge on screen.

### 2026-10-26: chapter 2b scene 7: the furnace

- **entityAvatarMiscFurnace** (factory type 0x17, getter `0x0006E1C0`, vtable 0x00504CE8) in `entities/furnace_actor.cpp`:
  sizes `0x0006E1A0` (0x93C / 0x334), destroy `0x0006E210` (both contact pools, +0x934 and +0x938), message
  `0x0006E2A0` (0x7C drops the actor from both pools), update `0x0006E260`. `0x0006E4D0` tells actors in a sphere of
  50 x +0x38 (players aside) {0x16, reply, sender, contacts, point} and gives each that answers callout 0x16 between
  them (`0x0006E2F0`); `0x0006E6F0` tells actors in 55 x +0x38 {0x1B, 0, sender, 1, contacts, point}. Draw is the
  actor's (`0x0006EF20`). Notice 0x16 shares 0x17's layout in the contact conversions.
- **entityStateHitFurnace** (factory type 0x75, getter `0x00096A50`, vtable 0x00505980) in
  `entities/body_furnace_hit_state.cpp`: construct `0x00096AB0`, destroy `0x00096E60`, tick `0x00096FC0`. The body
  slides to the furnace (body +0x26C) over one second, turns away from it, then dies (`0x00134AE0`). Body message
  0x16 (`0x000233A0`) now has its views. Naming note: by the factory table (a type's name is on the row before it),
  0x73 is entityStateGroundAttack, 0x74 entityStateHit and 0x75 entityStateHitFurnace; the older code calls 0x73
  "hit" and 0x74 "hit furnace" (`body_hit_state`, `body_state_actions`), so the new state's symbols say
  `furnace_hit` to stay apart. Those older names were not changed.
- **Scene marker kind 0x19** (`0x000FFA30`) is bound: when it runs it attaches to the first `DAT_00511B20` node's
  actor (the background) and that actor's +0x8F4 list.
- **Background hit 8** (`0x00043E50`): a non-player attacker's body +0x270 / +0x274 are read through the attacker's
  body view (`body_of`), not through actor +0x95C in scene memory, which the host never fills. Before, a skeleton
  knocked into a wall read the background model's bytes as an actor (0xFFFFFFFF) and failed with "scene address range
  exceeds its native instance". `--scene2a8` still fails with that message by another path.
- Type 0x75 and 0x8C join the empty +0x14 / +0x18 / message / collision-reply lists and the `0x0002B620` +0x1C list.
- FREE PLAY's GhoulyExistsInScene (tag 0x23, objTag_Actor_MummyCursed) is not what ends it: its link (node 0x5C) only
  sends {0x36} to a cursed mummy once one exists. The section ends at the exit once its gate is up, and the workshop
  spawn keeps sending mummies meanwhile, so `--scene2b7` leaves ghoulies alone in "FREE PLAY" (as `--scene2a5` does).
- Checked in the captures: the kiln rooms, Cooper fighting the mummy among the pots, and Cooper walking out of the exit
  door (frame 12000). Not seen: a
  skeleton being drawn into the furnace.

### 2026-10-26: chapter 2b scene 6: Mr Ribs and powerup kind 0x13

- **entityStrategyRibs** (factory type 0x50, getter `0x000BF760`, vtable 0x00505FBC) in
  `entities/ribs_strategy.cpp`: construct `0x000BF7B0` (model node pair 0 set to 1 on the +0x204 and +0x310 models),
  update `0x000BF7F0`, message `0x000BF8B0` and enter `0x000BF910`. State 0x401 loads "far" and moves to 0x40C once a
  selected target is within 65; 0x40C picks the body's vtable +0x20 action when body bit 0x14 is up and queues mind
  request 0xD (20, 25, 60, 65, then 0.4 / 0.6 / 0.8 / 1.0 as health falls past 3/4, 1/2 and 1/4). Sizes are the base
  strategy's `0x0002B6A0`, the leave is the base `0x000BFA50`.
- **Powerup kind 0x13** in `0x00123E70` (`entities/powerup_awards.cpp`): walks DAT_00511B48 (`g_world.alt`, the
  players' links[2]) and, for each actor the dump does not skip (`0x00028360` masks 3 / 0x24, a paused strategy, a
  blocked player group or decay mode, a goal on the ring), stores the amount at +0x978, drops a mind target
  `0x00029C80` rejects (message {0x64, 0}, mind +0x10 cleared) and sends {1}. Its sound is row 0.
- Driver: a pending PlayerHitGhouly (opcode 14) names the class to chase when no event step does, and a ghouly
  walked to for a minute without getting within 16 is left (the penned vampire chickens, tag 0x39).
- Checked in the captures: Cooper fighting Mr Ribs at the barn door (frame 1500) and walking out of the exit door
  (frame 18600).
- Scene 2b5's open item was a death, not a script loop (see the 2b5 checkpoint).

### 2026-10-25: chapter 2b scenes 4 and 5: chickens, vampire chickens and the blunderbuss

- `--scene2b4` (new) runs without a native stop; the driver loses its timed event (see the table). `--scene2b4b` (new)
  plays the book into scene 5.
- Scene 5's scene start needed three classes, now translated:
  - **entityStrategyChicken** (factory type 0x3A, getter `0x000A9FE0`, vtable 0x00505D38, `entities/chicken_strategy.cpp`):
    construct `0x000AA030`, update `0x000AA050`, message `0x000AA0F0`, enter `0x000AA1D0`; the skeleton's sizes and the
    base leave `0x000BFA50`. A hit from itself or from a class 7 actor within parameter +0 (DAT_00302B5C) arms it; with
    body bit 0x29 up it enters 0x3EF, "transform". Otherwise it wanders ("far").
  - **entityStrategyVampireChicken** (factory type 0x55, getter `0x000C28E0`, vtable 0x00506048,
    `entities/vampire_chicken_strategy.cpp`): construct `0x000C2930`, update `0x000C2950` (with `0x000C2530`,
    `0x000C2580`, `0x000C25E0`, `0x000C2740`, `0x000C2820`), message `0x000C2A40` (`0x000C2D90` releases the held body),
    enter `0x000C2C70`, leave `0x000C2D10`; the director's sizes. It emerges ("emerge"), wanders ("randomwander") and,
    queued on a standing target, grabs it (0x3ED, "knockbackprepare", claiming the target body's +0x50 through
    `0x0001B0D0`) or knocks it down (0x3EE, "knockdown").
  - **entityWeaponSingleShot** (factory type 0xA5, getter `0x000C8E00`, vtable 0x00506210,
    `entities/single_shot_weapon.cpp`): the blunderbuss over the gun. Sizes `0x000C8DE0`, construct `0x000C8E70`,
    message `0x000C8EC0` (0x9A answers 1), fire `0x000C8EF0` (+0x4C): the projectile template at +0xEFC is made at the
    muzzle (swept back from the holder, `0x00122480`), owned by the holder (+0xD8C), sent 0xA5 toward the aim point,
    callout 0x2B shown there, 0.15 of the charge spent and message 0x3E sent to the holder.
  - The gun base null-tests its class slots +0x54, +0x58, +0x5C and +0x60 (`0x000CAA80`, `0x000CAD90`, `0x000CAF10`,
    `0x000CB0B0`); the single shot leaves +0x54, +0x58 and +0x60 unset, so those calls are skipped, and its +0x50 and +0x5C
    are the empty 0x001628B0. Type 0xA5 joins the weapon class lists in the scene services and factory owners.
- Script condition node 0x8B (`0x00125661`, through `0x00125420`) builds condition opcode 0x13 (`0x00132E00`): the
  link, the dword at +8, the tag of the name at +0xC and the float at +0x4C.
- **entityStateTransform** (factory type 0x8C, getter `0x000A43A0`, vtable 0x00505C60,
  `entities/body_transform_state.cpp`), the body state a chicken transforms in: sizes `0x000A4380`, construct
  `0x000A4400`, destroy `0x000A4690`, update `0x000A46D0`, message `0x000A49D0`, with `0x000A4B00` (the morph actor,
  made at the body's pose with its 32 nodes hidden), `0x000A4BB0` (the final actor, with the body's scale and +0x1FC
  marker key through `0x0011F2E0`), `0x000A4CA0` (message {0x65, 0x3FD}) and `0x0013B1F0` (bits through `0x00068770`).
  The morph's nodes are shown as the body's animation plays; two 1.25 s holds follow; a hit instead brings the body
  back and finishes it (`0x00134AE0`).
- **Condition opcode 0x13's update** `0x00133900` (the tally, then +0x14 counting down) now runs in the VM tick.
- **entityAvatarMiscChickenEgg** (factory type 0x16, getter `0x0006DB50`, vtable 0x00504C70): a plain actor under
  its own type (the actor's sizes, the shared constructor `0x000CFFD0`, and message / update / draw slots that jump to
  the actor's); it joins the type 2 lists in the scene services.
- Checked in the captures: the Baron's page, the survive timer (0:57 .. 0:12), Cooper firing the blunderbuss (its
  green cloud) and the vampire chickens. Not seen: a chicken mid-transform.
- After the survive event's completion ("event complete" ditty, door open) the driver fights ghoulies by the exit at
  low health and Cooper dies (frame ~14300: the death state's FAILPITCHBEND stream, then the page turn). The scene
  restarts natively from its first page, which is why the egg page and the Baron's section run again; there is no
  script loop. The second attempt runs out of frames in the survive event.
- `--scene2a8` fails with "scene address range exceeds its native instance" around frame 2280; it fails the same way
  at 6ecff3a, before this session's work.

### 2026-10-25: scene 2b3's flying imps

- `--scene2b3` (new) stopped at its scene start: factory type 0x48 (getter `0x000B6B70`) was not translated. The type
  table names it entityStrategyImpFlying (row 0x47). It is now translated in `entities/imp_flying_strategy.cpp`, over
  the spider views the other imps use; the parameters are DAT_00303124.
  - Vtable 0x00505EDC: sizes `0x000B6B50` (0xF8 / 0x98), construct `0x000B6BC0`, message `0x000B6CB0`, update
    `0x000B6BF0`, enter `0x000B6F80` (its default is `0x0002BAF0`), leave `0x000B7050`.
  - The update, enter and leave tail-jump to `0x000B5B30` (0x401), `0x000B5D70` (0x3ED), `0x000B5F10` (0x409),
    `0x000B5D50`, `0x000B5E90` and `0x000B5ED0`; Ghidra had no functions there, so they were exported as ranges and
    read from their instructions.
  - 0x401: attacks a queued target (`0x000B5CF0`, then 0x3EF or 0x3ED by `0x000196F0`), evades within 50, and
    otherwise hovers to one of four clear points 30 out (`0x000B6630`) at a height from `0x000B6A30`, kept within 5
    of its height over the floor cell.
  - 0x3ED: "wingwhackprepare", then "evade" (`0x000B5E20`).
  - 0x3EF: "arrowdive" and its effect (`0x0003DF40`), a hold, then a lunge to its model's marker 0 over parameter
    +0x24 seconds and "arrowdiveunstuck" (`0x000B63A0`, `0x000B6060`).
  - 0x409 (a hit that lands on the imp) sets it down until body bit 0x31, then it rises to its hover height
    (`0x000B6950`); 0x417 does the same with a 5 s limit (`0x000B6480`, `0x000B6560`).
  - Two retail reads use the first argument's stack slot as a float where no floor is found (`0x000B6480` without
    body +0x19E, `0x000B6630` with no cell under the imp); the translation passes the strategy's address bits.
- Checked in the captures: the "Flying Imp" intro card with the imp flapping toward the camera, and Cooper punching
  imps that die (GIMP_DIE).
- Not passing: the driver dies to the imps and the Medusa (class 0x1F, which grabs Cooper by the exit) and the scene
  restarts. The HUD heart's number jumps between values (41, 12, 87, 27, ...) within a second early in the scene; not
  yet compared with retail.

### 2026-10-25: chapter 2b scenes 1 and 2, and DAT_004E1C34

- `--scene2b1` (the freezer) needed nothing new. `--scene2b2` (the garbage yard) held at "Event: Survive": its
  waves wait on condition 4 (`0x001327A0`), which, once the kill count is reached, latches whether a scene cutscene
  is playing (DAT_004E1C34 >= 0) and then waits for the stack to empty. The condition read a host word that nothing
  wrote (0), so it always saw a cutscene and never fired. It now reads the scene cutscene stack's top.
- The scene services kept a second DAT_004E1C34 (`prompt_lock`, -1 for the run) for the scoreboard, the player
  strategy, the weapons, the area prompts and the Super Scary shock, so they never saw a cutscene playing. The
  cutscene runtime's top is now bound to that word, so they read the stack's real top. The services' own reset of
  it is gone; the teardown's `0x0004ECE0` empties the stack.
- Checked in the captures: the spiders and the HUD's spider counter (30, 14, 6 through the event).

### 2026-10-25: scenes 2a16 and 2a17, the fade-out effect and the attach check's bind node

- `--scene2a16` and `--scene2a17` (new). Scene 16 needed nothing new. Scene 17 now moves on to chapter 2b's first
  scene.
- **entityFxCustomFadeOut** (factory type 0x33, getter `0x00076BE0`, vtable 0x005053F8). The type table's names sit
  one entry early, so type 0x33's name is at entry 0x32. Ghidra has no functions at the getter, construct or
  update, so they were exported as temporary ranges. Translated:
  - the construct `0x00076C30`;
  - the update `0x00076CB0`: the actor's alpha (+0x19B, the top byte of +0x198) falls as the level rises to 1. At 1,
    the actor's bind tree is activated if +0x24 is set (`0x00112CC0`), then the effect releases itself
    (`0x0003DE30`).
  - Its sizes are `0x00076BC0` (the same code as the death state's) and its message is the fade-to-grey's
    `0x00076DF0`.
  - Seen in one run of five: after the cook's "here you are", a one-second fade (1 s, flag 1) ran 255 to 0 and the
    effect released itself. The captures don't show which actor it was: its world position reads NaN, and the
    camera turns away from the dropped cleaver during the fade.
- **`0x00027D40` reads the target's bind node natively.** The attach check tests the target's +0x918 word (the
  parent's bind node: none, a root node, or a node of no actor). It took a span that nothing filled, so a bound
  target raised. Scene 17's cook (a cinema mind) reached it as "spawn attachment actor +0x8F8 link view". It now
  reads `Actor::link`, as the pirate's chest check and `0x00146150` do, and the span plumbing is gone from the player
  and cinema minds.

### 2026-10-25: the chair's ghoulybox request, the screen fade and the scene teardown

- **Scene 2a15 raised at the chair's message.** A ghoulybox request (`0x0013E8D0`, the 0x108-byte A8 message) reaches a
  weapon through its DAT_00512B74 controller, and that is the weapon's vtable +0xC. The controller now dispatches by
  factory type through the same per-class messages the scene uses (`0x000CF0D0` for the chair). `--scene2a15` (new)
  passes.
- **Screen fade (`0x00060D50`).** The frame (`0x00033B50`) calls it between the scoreboard and the camera effects.
  Two parts:
  - the full-view tint at DAT_00504450 (`RuntimeScreenTint`): it swings between its colours over its time and is
    shown for one frame when its time is below zero;
  - the back-buffer blur at DAT_0050447C (`RuntimeCameraPulse`, which now has the retail layout: total and
    remaining were swapped and +0x48C / +0x498 / +0x4A4 were missing). Its radius and alpha run between their
    values while 0x0012B2A0 ticks the time, and a held pulse keeps the end values.
- Translated with it:
  - `0x00060BC0` / `0x00060C20` (the two clears) and `0x00060C80` (the scene start's reset, first in `0x0011CBD0`);
  - the draw `0x000615E0`: the tint quad, and the blur (the back buffer copied into book page 0, DAT_005047DC, and
    drawn back through boot shader row 12 with four offsets from `0x00061160`);
  - the retained border lines (`0x00061250`).
- The recording device has a CopyRects boundary, and the back-buffer copy takes a texture object or its frame
  record.
- Checked in `--player-death`'s new captures (`cache/player_death_blur_*.bmp`, every 15 frames after the death): the
  death state's pulse (4.6 s, radius 0 to 10, alpha 1 to 0) doubles and smears the view more as Cooper falls.
- Nothing native starts the tint yet.
- **Scene teardown `0x00117330`** now runs every step in retail order:
  - the profile times (`0x000F2BE0`);
  - the challenge frontend's release (`0x00049FF0`). Boot sets the frontend state DAT_004E17C8 to 0, so every
    teardown sets it to 1 through `0x0004A4D0` (states 2, 4, 5, 6 and leaving 6 raise). The host's reset of the
    frontend state between scenes is gone;
  - DAT_004E1744's play time;
  - the ghoulybox controller pool;
  - `0x001289A0`: the title back to "There is no script for this scene!" through `0x00128AB0`, the title pool, the
    cutscene skip input (`0x00127B50`), the pause name and the fired pool;
  - the condition pool;
  - the live script events (`0x00116790`). Their nodes now come from the engine heap, which their release always
    freed them to.
- DAT_0050E760 (the script AID `0x00117000` acquires) has no host registry reference to release, since the host
  parses the script from the AID store.
- Runs: `--player-death`, `--scene2a15` and `--scene2a1b` (two teardowns into played scenes) pass.
- The teardown's `0x001163D0` was listed as unfilled, but DAT_005462F4 / DAT_005462F8 is the actor snapshot list that
  the avatar motion blur appends to (`0x00116750`). It now releases each snapshot's texture and clears the decal gate
  (DAT_005462EC) and the count.
- Boot `0x0011C3F0` (its middle was exported as the fragment `0x0011C480`):
  - Now run: the named sound rows' ids become 0xFFFF (`0x00129F30`), and DAT_0050E7A0 goes up by one at the end,
    which the first scene's state 1 takes back.
  - Already covered: its input push and the transition handlers' inits run in boot and the services' start.
  - Nothing to do: its zeroed globals are already zero; `0x001233C0`, `0x0011B630` and `0x00123B30` clear lists that
    are empty at boot; `0x00057D40` and `0x00119160` fill host tables nothing native reads.
  - No reader in the dumps: the sums at DAT_0042A424 / DAT_0042A42C.
  - `--scene2a5` and `--scene3` pass after these.

### 2026-10-25: ghoulie introductions face the camera

- The intro's shots (`0x000F4220`) are placed from the direction to the ghoulie's mind target, and the intro goal
  ("ghouly_intro": Celebrate, then SetStateIfFlag) starts the cheer state 0x62 (`0x0008C240`), which turns the
  body toward its mind's target (actor +0x958 -> +0xC). The host never stored the mind at actor +0x958
  (`0x00028E50` does), so every reader of that word saw no mind: the cheer state never turned the ghoulie, and
  the shots framed whichever way it already faced (the scene 1 imp faced away from Cooper, so shot 0 saw its
  back). The player constructor now stores the created mind there. Checked in `--player-opening`'s new captures
  (`cache/player_intro_*.bmp`, every 20 frames of the intro): the imp turns, then faces the camera in shot 0 and
  three-quarter on in shots 1 and 2, as the shots are built.
- Other readers of actor +0x958 now see the mind too: the anim state's target, the cinema mind's escape
  (`cinema_mind_movement.cpp`, `mind_escape_position.cpp`) and the haunted TV's target check (the cinema and the
  haunted TV checked in play by the user), and script type 0x6B.
- Script type 0x6B (`0x00126A10`) raised for any actor with a mind; retail only goes on for a mind with a target
  (+0x10), through `0x00029C80` (EDX 1), and on a zero answer tells the actor {0x64, 0} and drops the target. That
  is now run, through three new DAT_00511B48 node views (the mind, the 0x00029C80 check and the actor message).
  Only three scripts have the node (a scan of every aid_script bundle): chapter2a_scene1_3playcam (the fire
  imps), chapter2a_scene13_2playcam (skeletons) and chapter2c_scene5_1playcam (skeletons, no test mode yet).
  `--scene2a1b` passes through scene1_3playcam; `--scene2a13` reaches no untranslated path and holds in the
  excluded fight ("EVENT - NUMBER").

### 2026-10-25: stuck in the snooker table; the camera above the level

- **Stuck in the snooker table (scene 5).** The room's collision has a box where the table stands (x ±30,
  z ±20, 11.25 high, triangle flags 0x4). The swing's approach puts Cooper on the pick-up point, whose radius-4
  sphere overlaps that box. The swing's end sends body message 0x4A, whose contact separation (`0x000744F0`)
  should push him out, but `0x0003E050` had its two masks swapped: it tested shapes and triangles against the
  caller's mask (0xC0000) and excluded the kind (4). The dump ANDs the kind with actor +0x1F0 for the shape and
  triangle mask (EBX into `0x00055960`) and passes the caller's mask on as the excluded flags. With that fixed
  the separation finds the box and Cooper ends just outside it (x ±34.0). Checked in play by the user: no longer
  stuck. Every body separation (`0x000744F0`, `0x000755A0`) now finds the room geometry it missed before.
- That separation reached two states' collision replies that had no dispatch: the spin-dizzy state 0x86
  (`0x000A24B0`) and the endure state 0x84 (`0x000A1850`) now take them.
- Scene 2a9 then reached message 0x53 to a walk-alert body: its answer `0x00023890` (no energy: +8 is 0;
  otherwise action 0x117, or the body's own key, through `0x00024B20`, and +8 is 1) is translated, and so is the
  state action 0x117 starts, factory type 0x89 (`body_stunned_state`: sizes `0x000A3180`, constructor `0x000A3200`,
  destructor `0x000A3300`, tick `0x000A3380`, message `0x000A3460`). The parent body message (`0x000253B0`) now
  rebinds the actions' message children after the second action's message, since that message can start a new
  first action.
- **The camera above the level (scene 2a12 and others).** The camera body stays where the rig puts it (about
  35 up in the hold), but the view adds the actor camera effects (`0x00049260` over DAT_004E1734), and nothing
  ticked them: retail's frame (`0x00033B50`) calls `0x00048F90` after the scoreboard, which swings, fades and
  ends each effect (`0x000E9210` unlinks and frees it). Without it every hit's effect stayed at its start and
  they piled up, lifting the view by up to about 55 during the hunchback fights. `0x00048F90` is translated and
  called there; the 2a12 view now stays at the rig's height (captures looked at: inside the hold, framing Cooper).
  Its neighbour `0x00060D50` (the screen flash and capture) is still not called.
- Regression (2026-10-25, after these): `--dialogue-resources`, `--pause`, scenes 3, 4, 7, 9, 12 and 2a2, 2a5,
  2a8, 2a10, 2a11 and 2a12 pass. Scenes 5 and 2a9 fail in the driver, with no untranslated path reached: scene 5
  loses the skeleton event and the retry doesn't restart it; scene 2a9 opens its exit (Free Play by frame 2423)
  but the driver keeps fighting the class 0x17 ghoulies instead of leaving. Both changed with the collision fix.

### 2026-10-25: the wind and rain bed, camera shakes and pad rumble

- Boot now acquires aid_misc_ghoulies_audio_generalsettings (0x44 bytes) into DAT_004DF358 as `0x00033030` does
  after the audio boot, and the ambience reads it through `audio_general_settings` fields. The hardcoded copy of
  its delays is gone; +0x3C / +0x40 (11.1 .. 16.1 s) are the thunder's next-flash delay.
- Boot's `0x0011C480` part `0x00118470` (the bed's initial state) was never called, so the bed's wait stayed 0 and
  it never armed. It now runs after the light defaults. Its random draws shift every later DAT_0054DD7C path.
- The wind and rain bed `0x00117A50` is translated with `0x001188C0` (the nearest ambient emitter),
  `0x001175E0` / `0x001176C0` / `0x001177A0` (the wind, thunder and rain curves over the wait), `0x00117880` (a
  bed voice started at the point, moved, or stopped), `0x00117900` (thunder), `0x00117930` (a flash) and
  `0x0012B2A0`, and the flash light curve over the eight keys at 0x0030F6C8. DAT_0050E924 is the last of the 21
  emitter slots, as in retail. A reset (`0x00117980`) and the extra wind's redraw (`0x00118290`) now stop a live
  voice through `0x00039690` instead of only dropping the word. DAT_0050E7A0 is `g_50e7a0` (the copy in
  ambient.cpp is gone) and DAT_0050EA98 is `g_flag_ea98` (scene.cpp's `g_50ea98` copy is gone; the teardown
  clears the one the readers use). Seen in the logs (not listened to): scene 5 arms the bed, starts
  GENVIRONMENT_WINDEXTRA and flashes thunder ten times; scene 7 and 9 arm it.
- Camera shakes: boot's pool and enables (`0x000FADE0`), the scene load's fresh pool with the unlock bit 4
  vibration switch (`0x000FAEE0`) and the frame's shake tick (`0x000FAF70` with `0x000FB220` .. `0x000FB650`) are
  translated. The shake rows are typed (`ActorCameraShakeAxis`), the four port records at DAT_00507D84 are
  `g_pad_shakes`, and the DAT_00507D40 rumble commands are shared from clock.cpp
  (`runtime_pad_rumble_commands`), where 0x000FAD80 already submitted them but nothing set their +0x0C enable.
  Seen in the log: scene 5's skeleton fight drives the motors over the 5.0 threshold on 2878 frames. Not felt: no
  pad was attached to the test runs.
- The teardown runs the dialogue's release (`0x00057DC0`: the speaker resource, icon, input device and widget),
  and the ambience steps the game's shared random word (DAT_0054DD7C) instead of its own copy.
- Teardown `0x00117330` now also releases the event steps (`0x000D0490`), clears DAT_0050EA9C bit 0x800000 and
  the score captions (`0x0012DF90`).
- The haunted door's hit callout (`0x000AFAC0`'s inlined `0x0003C040`) resolved only players; it now goes
  through the scene's actor callouts like the hunchback's (scene 12 reached it with a non-player).
- Scene 5's driver retakes the cue when the skeleton event is lost (Cooper faints, or stage 2 times out) instead
  of walking to a locked exit; the shifted random draws made it lose.
- Regression (2026-10-25): `--dialogue-resources`, `--pause`, scenes 3, 4, 7, 9, 12 and 2a2, 2a5, 2a8, 2a9,
  2a10, 2a11 pass. Scenes 5 and 2a12 fail in the driver's fights, with no untranslated path reached: scene 5 loses
  the skeleton event on its time limit and, retrying, Cooper walks into scenery on the way to the cue; scene
  2a12's Cooper swings at hunchbacks he cannot reach until the run ends. The shifted random draws (the bed's
  boot init) changed both fights. In those runs, the bed armed in scenes 5, 7, 9, 2a5 and 2a12, and the shakes
  drove the motors past the threshold in every scene with a fight.

Still open:

- The fight driver in scenes 5 and 2a12 (and 2a13 / 2a14, excluded before).

- `0x00117330`'s `0x000F2BE0` (the profile times), `0x00049FF0`, `0x001289A0`, `0x00116790` and its DAT_0050E760
  release; `0x001163D0`; the rest of `0x0011C480` (including its DAT_0050E7A0 increment).

### 2026-10-25: challenge actions and the scene teardown

- Challenge setup actions 0x7A .. 0x86 (`0x001251E0`) occur only in the challenge mode's 20 scripts
  (aid_script_ghoulies_challenges_*; a scan of all 157 script bundles found none in a story scene). Their callees
  (`0x0004A100`, `0x0004A240`, `0x0004ADD0` and the DAT_004E17C8 challenge states behind them) are a subsystem nothing
  native reaches yet, so each type now raises where reached instead of building a record nothing read. The same scan
  found script type 0x52 nowhere and 0x89 / 0x9A only in chapter 5.
- The teardown (`0x0011C5A0`) is split on the host: its globals run in `scene_teardown` as the state enters 4, and
  the actors, controls, cutscenes, doors and passes in `SceneFactoryRuntime::end_scene` as the next scene loads. Its
  notes now say where each step runs. Newly run: the contact slots' release (`0x001222A0`), the blob shadows'
  reset (`0x001199F0`; they were never cleared between scenes), the ambience reset (`0x00118560`), the camera
  effects' release (`0x00048F10`), the camera shakes' release and the pads' rumble stop (`0x000FAE40`, `0x00155930`),
  the story bits type 0x9E changed put back on a restart (`0x001272B0`) and the deferred models flushed
  (`0x000F97F0`). The present now collects released models every frame (`0x000F9780`), which nothing called, so
  they had piled up for the run.
- `0x0011CAC0`'s light reset wrote dead copies of DAT_00509B50 .. DAT_00509B58 and DAT_00509B70 / DAT_00509B74; it
  now writes lighting's (`g_scene_light_direction`, `g_light_paused`, `g_light_running`).
- The ambience reset arms the extra-wind timer, which reached the untranslated voice in `0x00118380`; it and the steps
  (`0x00118120`) are translated with their gate `0x00118800` (scene state 1 or 2, no overlay, the environment bank,
  id 2 through `0x000384F0`, loaded). Heard in the log (not listened to): scene 3 now plays the random ambience
  (XACT_SOUNDBANK_GRANDOMAMBIENCE_LAUGH, _TABLEMOVE, _WOLF). The wind and rain bed (`0x00117A50`'s armed body) still
  raises when reached.

Still open (the camera shakes, `0x00057DC0` and the shared random word were done in the next round):

- Teardown callees still not run: `0x00117330`'s calls other than its DAT_0050E560 clear, `0x001163D0` (a render
  target list nothing native fills), `0x00049EF0` (the challenge's state) and `0x0012A9B0` (the post models, which
  nothing native loads).

### 2026-10-25: the hunchback's two states; scene 4's exit walk

- Factory type 0x84 (getter `0x000A1450`, vtable 0x00505B60, parent 9; `body_endure_state.cpp`) is translated:
  sizes, constructor, destructor, tick, message and its phase entry `0x000A1430` .. `0x000A1930`. The body plays an
  entry animation, then a held one with a sound while it counts hits (0x15, 0x19, 0x1D) to a random number; then it
  makes an object from its template with a sound on slot 6, and after a hold sets body bit 0x2B. The retail name is
  not tied to the type.
- Factory type 0x86 (getter `0x000A21F0`, vtable 0x00505BA0, parent the dizzy state 0x64; `body_spin_dizzy_state.cpp`)
  is translated: `0x000A21D0` .. `0x000A24B0`. On top of the dizzy state it pushes the body round a circle whose
  angle and radius scale with the dizzy time left, so the spin slows as the stars run out.
- In `--scene2a12` the hunchback (class 0x16) enters 0x84 from frame 899 and 0x86 from frame 1500; the scene passes.
  Not confirmed by eye: the 0x86 spin and its stars (`scene2a12_1500.bmp` shows the hunchback but no clear stars).
- `--scene4`: scene 4_2's imps now take action 8's mind setting (0 at mind +0x6C) and crowd Cooper at the debris by
  the side doorway, where the straight walk from the key to the exit caught. The driver now routes the exit walk
  over the navigation grid (`nav_waypoint`) and looks for the key again if the scene restarts; the scene passes.
- Regressions on this build: the unit checks, `--pause`, scenes 3, 4, 5, 7, 9 and 12 and 2a2, 2a5, 2a8, 2a9, 2a10,
  2a11 and 2a12 pass. 2a13 still fails on the pirate fight (title "EVENT - NUMBER"), and 2a14 now holds at "free
  play" rather than in its event.

### 2026-10-25: the pause screen and the silent script actions

- The pause screen is native (`scene_pause.cpp`): Start in a running scene (`0x0011E7F0`, now on the live input stack
  DAT_0050E0E8 / DAT_0050E0EC, which always holds the boot "Scene Master Pad") asks for scene state 6; entering it
  (`0x0011CE10`) runs `0x0011D490` phase 2: the pause input entry, the "Paused" and "Chapter n, Scene m: name" text
  (`0x0005F5C0` with a panel, now drawn: `0x00060160` and `0x00061360`), the prompt widget (`0x00141DA0`, rows
  `0x0030F948`, chosen by `0x0011DD90`), the background volume (`0x00118CB0` / `0x00118AE0`), a ghoul laugh
  (`0x0013BA70`), the bonus book icon and the book on the scene's clip (`0x001162B0`, from the directory row
  `0x001162D0` now stores). `0x0011CFB0` ticks it (the book's hold at 0.123 and close past 0.815, the widget and the
  Start test `0x0011DEA0` / `0x0011DEE0`), `0x0011DB50` draws it and `0x0011CE40` / `0x0011CD90` release it. The
  prompt rows' buttons are bound: Return (`0x0011DEE0`), Quit (`0x0011DFA0`, the confirmation), No (`0x0011E1A0`),
  Stats (`0x0011E110`), Map and its return (`0x0011E140`, `0x0011E3E0`), Event Rules (`0x0011E150`) and Restart
  Challenge (`0x0011E410`). `--pause` pauses scene 3 twice: Start / Start, then X / B (No) / B (Return).
- Shown (looked at): the open pause (`pause1_open.bmp`: the book page, "Chapter 1, Scene 3: Billiard Room",
  "PAUSED", X Quit and B Return to Game) and the quit confirmation (`pause2_confirm_quit.bmp`). The scene runs on
  after each (`pause2_resumed.bmp`).
- Script world actions (`0x00128050`) that dropped their callees now run them: 0xA (the scene's name from
  aid_loctext_ghoulies_script_scenenames, shown over the title pool DAT_00512484 and kept for the pause through
  `0x0011DCF0`; `0x001288C0` releases the pool once empty), 0xB (`0x00123AD0`), 8 and 9 (the mind class rows
  `0x0010F690` and animation classes `0x0010F660`, with 9's {0x4E} message through `0x00122D40`), 0x6D (the scale
  exclusions), 0x4B (`0x000FD780`, the ambient tint, light and fog), 0x4C (`0x0003A1F0`), 0x79 (`0x00114050`), 0x8A
  (lighting's DAT_00509B70 / DAT_00509B74 rather than a dead copy), 0x8F (`0x001047E0` and `0x000FE160`) and 0xA1
  (`0x000FEA90`). 0x52, 0x89 and 0x9A raise where reached (`0x0012AA50`, `0x000F27C0`, `0x000F2740`).
- The tables actions 8, 9 and 0x6D fill (DAT_0050E148, DAT_0050E16C, DAT_0050E1B0) were bound into the player
  factory as empty copies; they are now the scene services' own, read live.
- Shown (looked at): scene 3's title card, "Billiard Room" centred over the room (`scene3_300.bmp`).
- The script pump's pad refresh (`0x00109470` on the scene record's +0x230) runs natively on the boot "Scene Master
  Pad"; the host copy of `0x00109470` over a stand-in span (`script_refresh_object_record`, `g_span_50e0e8`,
  `g_refresh_kind`) is deleted.
- The scene audio level (`0x00038110`) reaches the XACT category volumes (`0x0015C1C0`), and the teardown resets the
  reverb preset (`0x0003A1F0`, 0xD).
- The 0x7C notice goes to every weapon, as retail's DAT_00511AA4 walk does; the scene 9 freeze that led to holding it
  back no longer happens (scenes 3, 5, 7, 9, 12, 2a2, 2a9 and 2a12 pass with it).
- The background volume (DAT_0050EA00 ..) had three copies; the camera's clip extension (`0x0013C490`) now reads the
  one `0x00118AE0` writes, and the teardown clears that one.

- The title display steps the shared random (DAT_0054DD7C) for its pool and records, as retail does, so every scene
  with a shown name now draws a different random sequence. That sent three scenes down paths not taken before. Scene 7
  reached a knock-down sphere's own contact message (0x17, the sphere as sender, type 0x19), which the body's source
  view now resolves (`attached_actor_at` also covers type 0x19 and actors released since); scene 7 passes again.
- World actions run while the script loads, before the scene's actors start, so they use the boot scene services
  rather than beginning a scene early; the three tag tables clear in the teardown (`0x0011C5A0`), as retail clears
  them, not when the next scene begins.

Still open:

- (Closed in the hunchback checkpoint above: scene 2a12's type 0x84, and scene 4's exit walk.)
- `--scene2a11` failed once with "invalid unordered_map key" and passed on three reruns.
- Quit's Yes (`0x0011E1C0`) raises at the save of the game (`0x000F2CB0`); Restart's Yes at `0x000FAE40`; Map at
  `0x001410F0`'s room argument; Event Rules at `0x000D0780` / `0x0004A270`.
- The 0x1251E0 challenge setup actions (0x7A .. 0x86) still drop `0x0004A100`, `0x0004A240` and `0x0004ADD0`; the
  scene teardown (`0x0011C5A0`) still names about twenty untranslated callees.

### 2026-10-25: known issues pass

Calls that computed values and dropped them, or walked host lists that nothing fills, now run the dump's code:

- Script actions: types 0x4D (a template clone at a found actor, which is then activated), 0x4E and 0x73 (the
  DAT_00545C48 pairs and DAT_00545CCC groups the scene markers' randomizer reads; `start_markers` no longer raises
  for them), 0x6E (`0x001249B0`, occupancy mask bits by value), 0x8E (`0x001189D0`, the ambient phase and its sound),
  0x9D (a story bit, DAT_00506C84 and its bonus counter), 0x5D / 0x71 (messages by tag and by id over the native
  latch list), 0x70 (`0x0010F6C0`, the strategy class ranges, now one retail table that `0x0010F760` also reads),
  0x78, 0x97 and 0x08 (the DAT_00511B48 actors), 0x6B (raises at `0x00029C80`, whose views are not bound) and 0x90
  (raises when a tagged actor exists: `0x00112D10` is not bound).
- Script conditions 12, 18, 23, 24 and 26 read the native lists; `0x00122C70`'s broadcast raises with no scene bound
  instead of walking the empty host list (the unit checks bind an empty one).
- The scene tick (`0x0011E7F0`) now runs `0x001128A0` (the attacker queues re-sorted each second), `0x001162F0` (actor
  snapshots aged), `0x001185E0` (the ambience and the base light colour; its bed, step and extra-wind bodies raise
  when reached, as `0x00118470` never starts them), `0x000FDDA0` (light records flicker and swing) and the Start
  check (`0x001083F0` on the selected device writes DAT_00510F60; with no controller selected, as in a scene the
  tests boot straight into, there is no row to test). `0x0011B7F0`, `0x0012AA70`, `0x0013FD60`,
  `0x0013FFD0` and `0x00049DE0` raise when their gates open; none opens natively yet.
- Body message 0x17 (a knock-down sphere's contact) is bound; a strategy's message to a player's body, mind or
  strategy goes through the child dispatch.
- A weapon's holder release (`0x001462B0`) clears the +0xD8C word as well as its native view. The stale word was
  scene 2a13's crash in `0x00020F80`.

Still open:

- (Closed in the pause checkpoint above: the pause menu, and the 0x7C notice to every weapon.)
- With the queues re-sorted, scene 2a13's pirates keep their nearest three attacking: the driver dies three times in
  each run and `--scene2a13` fails, as `--scene2a14` does. Faster or slower attack taps did not help. `--scene2a11`
  passes again once the driver side-steps after 4 s pinned in one spot.

### 2026-10-24: chapter 2a scene 14 runs natively but is not won

- `0x0001AE00` (body animation command 0x19, the attachment list) walks the actor's +0x8C callout records into body
  +0x2D8, as the dump does. The binding passed body +0x2D8 itself as the record list, and the zombies' animation
  events walked body bytes as records until a "record" had no +0xC8 link.
- Driver: the first kill-all step's class is still chased after that step is done, because scene 2a14's done skeleton
  step stays in the list. Choosing by step +8 (0 until the step's delay starts) fixed that here but broke scene 2a13,
  so it was not kept.
- The event's fight is not won. Its NumberGhouliesAlive and dance-floor conditions run, the skeletons collapse and
  stand up, and the attacker queue (`0x00112A70`, depth 3 and 3 per class from the script) matches the dump. Cooper
  still dies in each attempt, and the event restarts after the death ditty (`FAILPITCHBEND`). Tried and dropped: the
  guitar (holding a weapon lets attackers with +0x96C skip the queue), stepping away from crowds, and striking
  downed skeletons.

### 2026-10-24: chapter 2a scene 13 plays to its end

- The pirate leader's chest: the secondary item `0x0001A2D0` creates asks the held container its contents' name with
  actor message 0x99. The name buffer is bound to a retail address through new body services (`bind_block` /
  `unbind_block`) for the message and released afterwards.
- `0x0009C150` (the knockdown's aim) now turns toward the body's +0x26C source through `0x00041900` when the push has
  no direction, instead of raising. Retail never clears +0x26C, and a knockdown from message 0x19 does not set it, so
  a source released since its hit (a dead pirate, a smashed prop) is read from its freed memory. The scene keeps a
  copy of each released actor's bytes and its last world position by address (addresses are never reused); the
  knockdown's position lookup, the script tallies' tag read and the body services' byte reads use it for such a source.
- A released player's held weapon (the pirates' swords) is kept by the scene until teardown (the `keep_object`
  binding, a deque so release walks never see it move), as retail keeps the object alive after its holder.
- `0x001232D0`'s 0x7C notice (an actor going away) also goes to the weapons players make, which retail activates
  onto DAT_00511AA4 but the host keeps off it. A held sword clears its +0xD8C holder (`0x001462B0`) when its pirate dies.
- Powerup kind 7 (`0x00123E70`) sets +0x930 on each marked actor whose +0xCDC is 1 or 5.
- Driver: once several kill-all steps (type 2, tag at +0x18) were queued together, the first one's class is chased
  like a one-type step's (scene 2a12's single kill-all step keeps the nearest-ghouly fight). Below half his health,
  with no ghouly within 16, Cooper walks to the nearest floor soup (FX emitter type 0xD, powerup kinds 3, 7, 8, 0xA,
  0xB, 0xD). Without this, the pirate waves wear him down bare-handed and the scene restarts. The scene 7 driver's
  frame cap went from 18000 to 24000 for the slower runs.
- Shown (looked at): Cooper fighting beside the leader and his chest (`scene2a13_3600.bmp`) and the cleared dining
  room on the way out (`scene2a13_13800.bmp`). Not checked: whether the chest's contents appear when the leader
  drops it.
- Not reliable yet. In 6 runs after the last change, 4 passed. One timed out after Cooper died twice (the bare-handed
  fight is luck), and one crashed in `0x00020F80` (`dispatch_runtime_body_damage_reply`): a moving-contact hit still
  named a holder (+0xD8C) whose view was gone, so `second_source` was null. That weapon's path past the 0x7C notice is
  not yet found.

### 2026-10-24: chapter 2a scene 12 plays to its end

- The hunchback's strategy (factory type 0x45, entityStrategyHunchback, `hunchback_strategy.cpp`, vtable 0x00505E88,
  parameters at 0x00428770) is translated: sizes and constructor `0x000B3A70` / `0x000B3AE0`, the update `0x000B3B40`
  and its states (special attack 0x3EF `0x000B3530`, evade 0x3FE `0x000B2BF0`, idle and taunts 0x401 `0x000B2DD0`,
  dizzy 0x409 `0x000B3220`, block 0x40C `0x000B33B0`), the message `0x000B3BF0`, the enter and leave slots
  `0x000B3DD0` / `0x000B3E70` and their helpers (`0x000B2B70` .. `0x000B3F10`). The special attack arms the 0xFD
  attack row and the hands' contact shapes (tag 3) and holds the latches; `0x000B39D0` undoes it.
- `0x000BABF0` (the mind's target within reach) moved from the Medusa into `strategy_target_selection` for both, with
  `0x000B5C50` beside it. Message 0x41's position callout goes through a new scene binding over 0x0003BE20.
- Shown (looked at): the hunchback's introduction page, his block (`scene2a12_block_2100.bmp`), his special attack
  (`scene2a12_special_3000.bmp`) and his dizzy stars.

### 2026-10-24: chapter 2a scene 11 plays to its end

- The walk state (factory type 0x8D, entityStateWalk, `body_walk_state.cpp`, vtable 0x00505C80) is translated: sizes,
  constructor, destructor, tick and message `0x000A4CD0` / `0x000A4D50` / `0x000A5060` / `0x000A5100` / `0x000A5270`,
  the end `0x000A5340`, the orbit `0x000A5390` and the placement `0x000A54D0`. The body turns along an arc for a
  set time, with up to three objects orbiting its head as the dizzy state's stars do.
- Driver: the groundskeeper (tag 0xB) is not a ghouly, and the grid walk refuses a step between cells more than 8
  apart in floor height (the store's balcony, stair and the floor beneath share one grid, so the walk ran into the
  stair's railing).
- Shown (looked at): the Super Scary shock overcome (`scene2a11_shock_2100.bmp`) and a Medusa beaten down in a puff
  of smoke (`scene2a11_medusa_2700.bmp`). Not confirmed: which body enters the walk state, and its orbiting objects.

### 2026-10-24: chapter 2a scene 10 plays to its end

- No new translation: the scene's zombies, haunted TV and event were already native.
- Driver: a kill-count step (type 3) sends Cooper after its class only (fighting the haunted TV first let the time
  limit run out, and the Grim Reaper's life drain restarted the scene); the exit is lined up through the door Cooper
  entered by when it lies within 25 of the exit volume (he caught on the jamb).
- Shown (looked at): the event's counter and clock (9 zombies left at 0:44, `scene2a10_event_1500.bmp`) and the
  haunted TV in the second "Free Play" (`scene2a10_tv_4500.bmp`).

### 2026-10-24: chapter 2a scene 9 plays to its end

- The imp's jump onto a head (factory type 0x79, entityStateJumpOnOff, `body_jump_on_state.cpp`, vtable 0x00505A00) is
  translated: sizes, constructor, destructor, tick and message `0x0009A0F0` / `0x0009A4C0` / `0x0009A710` /
  `0x0009AB00` / `0x0009AE60` and their helpers `0x0009A170` .. `0x0009B300`; the ride record (state +0x80)
  `0x000994E0` .. `0x0009A060`: it binds the imp under the target's head marker (bind mode 3, `0x0003B500`), steers
  its host (mind +0x3C), hits it (message 0x2A) and is shaken off by the host's left stick (`0x001087D0` into
  `0x00109810`). Shared helpers: the random timer `0x001050A0`, the leap `0x00103650` / `0x001036D0`, the rider claim
  `0x000265A0` / `0x00138140` / `0x00138190`, the landing search `0x00138220` and the "intros_imphat" notice
  `0x0013FE60`.
- A player's body resolves its settings' +0x1A8 / +0x1AC animation names (message 0x44's `0x00020920`) through the
  XBE strings, as they are not action-table rows.
- Driver: a goal on another floor is walked to through the reachable cell nearest its floor, then onto the next
  floor's grid; Cameron (tag 6) is not a ghouly; a rider on Cooper's head (body +0x718) is shaken off.
- Shown (looked at): the imp's leap at Cooper (`scene2a9_leap_2220.bmp`), riding his head (`scene2a9_ride_2240.bmp`)
  and, shaken off, beside him (`scene2a9_off_2300.bmp`).
- Not settled: two early runs of this mode diverged during "EXIT DOOR OPENS" (once the script reloaded the scene,
  once Cooper vanished and the screen went black); later runs did not repeat it.

### 2026-10-24: chapter 2a scene 8 plays to its end

- The pirate's strategy (factory type 0x4E, `pirate_strategy.cpp`, vtable 0x00505F84) is translated: sizes and
  constructor `0x000BE0C0` / `0x000BE130`, the tick `0x000BE160` (closing in 0x3EC / 0x40D, attacks 0x3ED / 0x3EE /
  0x3EF, drinking 0x3FA, walking 0x401, fetching a chest 0x405), the message `0x000BE520`, the enter and leave slots
  `0x000BE820` / `0x000BEA60` and their helpers `0x000BEB50` .. `0x000BED10`. A hit while he drinks with the body's
  bit 0x1A up makes him angry (the parameters' +0x18 effect through `0x0003DF40`, all six body latches) and he throws
  chests. Shared helpers: `0x0002BA60` (`0x0002B9F0` held while the game is paused) and `0x0001B6E0` (body +0x4C let go
  and told 0x97).
- A weapon a player attaches inside its own constructor (`0x0001B2B0`) names that player in message 0x91; the scene's
  object lookup now finds a player still being constructed.
- Shown (looked at): the pirate with his sword fighting Cooper (`scene2a8_3900.bmp`); his footstep, lunge, cheer and
  water-death cues fire (logged, not listened to). Not seen in this run: his drinking, his anger or a chest throw,
  since Cooper beats him first.

### 2026-10-24: chapter 2a scene 7 plays to its end

- The Medusa's strategy (factory type 0x4B, `medusa_strategy.cpp`, vtable 0x00505F30, parameters at 0x004287F0) is
  translated: sizes and constructor `0x000BB1B0` / `0x000BB220`, the tick `0x000BB340` and its states (gaze 0x3EF,
  its end 0x3F0, walking 0x401, 0x403, dizzy 0x409), the message `0x000BB400`, the enter and leave slots `0x000BB720` /
  `0x000BB7D0` and their helpers `0x000B9C00` .. `0x000BB870`. Her gaze is two light cones from markers 7 and 6; a
  ray along it that reaches a watching player starts the Super Scary challenge (`0x000873A0`) and sends message 0x15
  to the target and 0x41 to herself. Shared helpers: `0x0001D9D0` (body flinch reset), `0x00106140` (the angle
  between two horizontal vectors) and `0x00152C40` (`retail_acos`). `0x001414F0` moved from the ninja imp to
  `strategy_target_selection` for both.
- The light cones (DAT_0050EF20, `light_cones.cpp`): add, clear, the axis rotation `0x001047E0`, the cone builder
  `0x00119F40`, the sort `0x00119BE0` and the draw `0x0011AF90`, which projects each cone's spot onto the background
  grid with the shadow projector's alternate shaders (`0x000644F0` / `0x00064820`) and pushes each cone's rim as a
  strip. The D3D11 presenter now supports pixel shader texture mode 5 (clip plane), as xemu implements it.
- Event step type 1's key-candidate drop (`0x000D0200`) retires each dropped candidate through the inlined
  `0x00112CC0`; the Super Scary challenge's render toggles (`0x0003CBB0` / `0x0003CAE0`) now reach the actors.
- The player strategy's leave (`0x000BF410`) rebuilds the scene's DAT_00511B38 pause list first: at scene end it
  walked a list built before some of its players were destroyed.
- Shown (looked at): the gaze's spot, green on the floor and crates near Cooper (`scene2a7_2090.bmp` ..
  `scene2a7_2120.bmp`), then Cooper's shock (`scene2a7_2400.bmp`) and the Medusa's close-up (`scene2a7_2130.bmp`).
  The beams show in the captures (seen by the user). Not confirmed: the spot shows as hard-edged green patches the
  shape of the grid cells rather than a soft spot; that needs an xemu comparison.

### 2026-10-24: chapter 2a scene 6 (the boathouse), in progress

- The water pistol (factory type 0xA6, `water_pistol_weapon.cpp`, over the gun): its sizes, constructor and
  destructor `0x000C91F0` / `0x000C9280` / `0x000C9330` and the gun slots +0x4C..+0x60 (`0x000C9410`,
  `0x000C9540`, `0x000C95F0` with the droplet step `0x000C96E0`, `0x000CA1F0`, `0x000C98D0`, `0x000C9B20` with
  `0x000C9C30`, `0x000C9D40`, `0x000C9FA0`, `0x000CA480`, `0x000CA500`, `0x000CA830`, `0x000CA3D0`). Its droplets
  draw as one strip through `0x000659F0` (`draw_world_line_strip`, beside `0x000654E0`).
- The ghoulies' death state (factory type 0x63, entityStateDie, `body_death_state.cpp`): `0x0008C600`,
  `0x0008C750`, `0x0008C7E0`, finishing with `0x00134AE0`. The factory table's names sit one row before their
  type, so the port's type 0x62 (`body_die_state`, getter `0x0008C1E0`) is row 0x61's entityStateCheer.
- Script type 0x50 (a one-type event) builds step type 0xD, which `0x000CFDA0` runs.
- The player mind's 0x31 notice (`0x0007DD30`) skips an empty mind +0x14, as retail does.
- Played by hand (2026-10-24), the scene completes and moves on to the next one. Not done: the driver does not get
  Cooper through the event (see the `--scene2a6` row). The droplet strip has not
  been checked in a capture.

### 2026-10-24: chapter 2a scene 5 (the conservatory) plays to its end

- The ninja imp's strategy (factory type 0x49, `imp_ninja_strategy.cpp`, vtable 0x00505EF8) is translated: the tick
  `0x000B7B40` and its states (psych-up 0x3ED, kick 0x3EE, inner force 0x3EF, idle 0x401, meditation 0x412), the
  message `0x000B7C50`, the enter and leave slots `0x000B7E60` / `0x000B7F20` and their helpers `0x000B7140` ..
  `0x000B7FE0`. The factory table's names are one row off (each type's name sits in the row before it): type 0x49 is
  `entityStrategyImpNinja` and type 0x18 `entityAvatarMiscGhoulyKey`. Several Ghidra decompiles of this range
  (forced, unanalysed) disagree with the instructions; the translation follows the instructions.
- The player strategy bindings gain the scene's damage tables (for the inner force's health refill, `0x00028DA0`)
  and the parameters at 0x00303198; the strategies' object messages (vtable +0xC by address) now reach scene
  objects instead of raising.
- Script type 0x51 walks the native DAT_00511B38 list (message 1), which wakes the imp parked in the room.
- Event step type 0xF (the key carrier): its start `0x000D02E0` and run `0x000CFAE0`. The step's camera view lives
  beside the sequence (`ScriptEventSequence::key_camera`) since step rows move.
- The ghouly key (type 0x18, `ghouly_key_actor.cpp`): sizes, constructor and update `0x0006E930` / `0x0006E990` /
  `0x0006EA00`; its message is the actor's.
- The scene 7 driver's grid search may end on a closed goal cell (a door's point lies on the door's own cell).
- Scene 3 now plays the skeletons' ghoul intro when Free Play's node 0x51 wakes them, which moves the fight; its
  trophy walk now reaches the waypoint at (-104, -30) within 4 rather than 10, which had sent it into the cabinet's
  corner.
- Full regression (2026-10-24, after these changes): every mode passes except `--actor-loading` (failing before).
- Shown (looked at): Cooper among the ninja imps with the key event's icon (`scene2a5_2100.bmp`,
  `scene2a5_3300.bmp`) and the key in the door (`scene2a5_6600.bmp`). The big imp (the inner force) was not caught
  in a capture.

### 2026-10-24: chapter 2a scene 4 (the ballroom) plays to its end

The balcony's railing gap carries an invisible face (flags 0x144004, category 0xFE in the event byte +0x10) that
blocks the player (mode 2's mask 0xC0000). Script type 0x67 opens and shuts such categories: with a live 0x7D it now
calls `0x0012C8A0` with the category and `0x0012C950` (open) or `0x0012C930` plus message 0x4A (shut), clearing or
setting bit 4 of every matching collision event. `0x0012C8A0` walks the native DAT_00511AF4 probe list and each actor's
model shape list; it had walked retail words the port does not keep, so the background part removal `0x00044EF0` that
shares it never reached a shape either.

The section after the gate needed more of the script and the body:

- Handler 6 (`0x00125450`) registers types 0x66 (PlayerDeflectionContact, opcode 13) and 0x68 (PlayerHitGhouly,
  opcode 14) as conditions and skips their blocks; before, they fell through and ran their blocks at once, which ended
  the script. Its other untranslated types (0x65, 0x75, 0x8B, 0x8C, 0xA3) now raise.
- The condition updates `0x00133570` (opcode 13) and `0x001335E0` (opcode 14) count their timers down.
- A body's recovery (`0x00134060`) takes the front goal's action through `0x00135790` / `0x00135540`.
- The body relay (`0x000744A0`) reads +0x744 and +0x314 as it reaches each, so an action the first tick ends is not
  ticked again.

The scene 7 driver learns its stick offset only from moves near the recent top speed (a wall slide no longer turns it),
forgets its avoided cells when they leave no walk, and takes a `calm_title` in which it does not fight. Its grid
search (`nav_waypoint`) no longer steps diagonally past a closed cell: with the forgetting, scene 12's walk to its exit
cut the doorway's corner from (270, 306) and stood against the jamb for good.

- Full regression (2026-10-24): every mode passes except `--actor-loading` (the ambient point check, failing before
  this work). `--scene9`, which failed before, now passes. `--scene2a2-reaper` (fails by design) and
  `--scene6-save-reload` (run by `--scene6-save`) were not run on their own.

- Shown (looked at): Cooper in the railing gap (`scene2a4_2400.bmp`), the gate-shut cutscene from above
  (`scene2a4_2700.bmp`), the skeleton and zombie fight with its event icons (`scene2a4_6000.bmp`) and Cooper leaving
  through the curtains (`scene2a4_12300.bmp`).

### 2026-10-23: background flags (cloth kinds 1, 2 and 8)

The full regression after the ballroom's curtains found kind 2 parts in scenes 3, 4, 5, 6 and 8, which then stopped at
load. `0x00050880` now builds kinds 1, 2 and 8 as 20 x 16 cloths, shaded from the room's centre, and their hook
`0x00114910` pushes the points out of the player's capsule (within 60) and the nearest other actor's (`0x001025E0`,
`0x00102860`, over `0x000426F0`'s bound) and then out of the cloth's collision planes. `--scene3`, `--scene4`,
`--scene5`, `--scene6`, `--scene6-save`, `--scene6-save-reload` and `--scene8` pass again. Scene 8's captures show the
room as before (curtains by the windows); which drapes are the cloth was not picked out.

### 2026-10-23: the ballroom's curtains (background cloth)

The ballroom's background part table holds one kind 5 part: a pair of curtain halves. The scene load now makes the
cloth pools and a 256 x 1 grey ramp (`0x00114180`), builds the parts (`0x001153A0`, `background_cloths.cpp`) and
releases them with the scene (`0x001143A0`). Each half is a 17 x 16 particle grid on the existing point-graph solver
(`background_cloth.cpp`: `0x0004B5F0`, `0x0004FB30`, `0x00050CC0`, the step `0x0004C000`, the shading `0x0004B800`
and the sleep and wake hooks `0x001151F0` / `0x001152C0`). The scene tick steps them (`0x00115BB0`) and opens or
closes the pairs (`0x001137E0`, `background_morphs.cpp`), which is what scene 2a4's control waits on. The view pass draws
them (`0x00115A80`, `background_cloth_draw.cpp`): each in-view cloth's silhouette is captured on the page target and
projected onto the light grid's cells (`0x0005DA50`, `0x000644F0`, `0x00064820` with its frustum `0x00064030`), then
the cloths are drawn as pushed triangle strips (`0x0005D250`, `0x0005D470`).

- Shown (looked at): the curtains drawing apart in the intro (`scene2a4_600.bmp`), the dancers on the floor
  (`scene2a4_900.bmp`), the mummy DJ and the Super Scary dropper falling on Cooper (`scene2a4_1500.bmp`). The cloth
  shadow on the floor has not been picked out in a capture.

### 2026-10-23: chapter 2a scene 4 (the ballroom) starts

`--scene2a3` (new) passes; `--scene2a4` (new) boots the ballroom, whose start needed three classes:

- `entityStateDroneDance` (type 0x67, `body_drone_dance_state.cpp`, `0x0008DFF0`..`0x0008E500`): a body idles on its
  dance for one to two seconds, glides to a random point of its room (actor +0xCA0, `0x00110F50` with its box cells
  `0x0010FBE0` and settle `0x00111B40` in `spawn_candidates.cpp`) while turning to a random yaw, and reacts to 0x2D.
- `entityAvatarMiscSSSDropper` (type 0x21, `sss_dropper.cpp`, `0x00071800`..`0x00072200`): a Super Scary figure that
  drops onto a free player within reach, leaving after-images, lands into the Super Scary challenge (type 0x98), and
  leaps off again on 0x8E 7 or 8. Its draw goes through the glow capture.
- `entitySceneControlChapter2Scene4` (type 0x95, `chapter2_scene4_control.cpp`, `0x00082980`..`0x00082BC0`): plays
  the intro cutscene, then sets the background morphs playing and waits for the first to stop before letting the
  script go on.

Shown (looked at): the intro cutscene's view up the stairs into the ballroom (`scene2a4_300.bmp`). The run then stops
where the control reads the morph pool (see Background morphs above). The drone dancers and the dropper are not seen
in a capture yet.

### 2026-10-23: the Grim Reaper

`--scene2a2-reaper` (new) lets scene 2a2's time limit run out. The step's failure (result 3) summons the Grim Reaper
(`0x000CF7B0`): `aid_objparams_ghoulies_actor_grimreaper` at a free spot within 15 of the kind 0x12 marker farthest
from Cooper (`0x000CF950`, `0x0011F0B0`), told to appear (0x4D) and given script type 0x54's speeds. His strategy
(`grim_reaper_strategy.cpp`, type 0x3F, `0x000AD390`..`0x000AE135`) chases ever faster, points and laughs, swings
when near (his hands' contact shapes 3 and 0xD on for the swing's second half) and plays the reaper stream; a hit
slows him, a landed one knocks him down. When Cooper dies to him he sucks the life out (0x3F2), and Cooper fades to
grey (`entityFxCustomFadeToGrey`, type 0x30, and its glow draw hooks `0x000770F0` / `0x000771F0` with the one-texture
composite `0x00062580`) before the retry.

- On the way, the spider's spit leaves a web (`entityAvatarMiscSpiderWeb`, type 0x1B, `spider_web.cpp`); the flight
  message 0x93 now carries the velocity's address at word 0x16, which the landing (0x7A) passes to the web.
- Shown (looked at): the countdown at 0:01, "The Reaper" caption, his swing at Cooper by the door
  (`scene2a2r_4500.bmp` in the run), Cooper grey under the scythe and lying grey, then the retry's book page with Cooper
  on the floor (`scene2a2r_4800.bmp`). Heard in the log: the stream, scrape, laughs, guitar, SUCKLIFE and the fail
  ditty. Not listened to.

### 2026-10-23: chapter 2a scene 2 (the dishwashing room) plays to its end

`--scene2a2` (new) boots `aid_script_ghoulies_chapter2a_scene2_1playcam`: Free Play, "Exit Door Closes", the
time-limit page, then "Event - 'Kill All/Time Limit'" with one coward zombie. Cooper beats it with 9 seconds left on
the reaper's countdown (`scene2a2_3600.bmp`), Free Play takes him out and the script moves on to
`aid_script_ghoulies_chapter2a_scene3_1playcam` at frame 5976.

- The scene's effect emitter (factory type 0x97, a kind 0xA marker) ticks its effect once as it starts
  (`0x000EF2D0`); the marker pass now lends it the frame's effect scratch, as the actor passes do.
- Script types 0x1A, 0x1E and 0x1F queued nothing. They now build their steps as `0x00125D80`, `0x00125DC0` and
  `0x00126040` do (time limit, type 4; survive, type 5; type 0xF) and queue them (`0x000D05D0`). The time-limit
  and survive ticks (`0x000CFE50`, `0x000CFEE0`) count +0x18 down; the event icons show their seconds.
- The scoreboard draws its time rows (kinds 4 and 5) as [H:]MM:SS (`0x0012F110`, `scoreboard_draw.cpp`).
- Shown (looked at): the time-limit page (`scene2a2_page1.bmp`), the reaper's countdown 0:24 / 0:14 / 0:09 beside the
  zombie icon (`scene2a2_2700.bmp`, `_3300`, `_3600`). Not shown: the reaper itself (the time never ran out). Not
  listened to.

### 2026-10-23: the extinguisher's book and the kitchen's Kill All play to their end

`--scene2a1b` (new) plays `aid_script_ghoulies_chapter2a_scene1_2book` ("Fire extinguihser weapon Intro") to its end
and boots `aid_script_ghoulies_chapter2a_scene1_3playcam`. Cooper holds the extinguisher and the fire imps (fly
state, type 0x6D) swarm him; with the right stick held he aims and sprays (entityStateShoot, type 0x81), and an imp
in the spray (entityStateHitStream, type 0x76) loses 20 of its 10 energy a second until it dies. "Event - Kill All"
is won at frame 2052, "Exit door opens/player loses fire extinguisher" runs, Free Play takes Cooper to the exit and
the script moves on to `aid_script_ghoulies_chapter2a_scene2_1playcam` at frame 4115.

- Later translations: entityStateHitStream (type 0x76, `body_hit_stream_state.cpp`: `0x000971E0`..`0x00097B00`) and
  entityStateLookForWeapon (type 0x7B, `body_look_for_weapon_state.cpp`: `0x0009D8A0`..`0x0009DD75`, `0x001345B0`),
  which a body runs in Free Play with no weapon: a question mark over its head, then a light bulb once its mind has a
  weapon to go for.

- New translations: the extinguisher's spray (`extinguisher_weapon.cpp`: `0x000C7C60`..`0x000C8D30`: emission, the
  particle blocks, their billboards and the wall bounce and five-sphere hit query), the gun's 0xA2 / 0xA3 and hit
  report (`0x000CAB40`, `0x000CB0B0`), the spray's contact report `0x00141C00` (message 0x18 / 0x3F), the model
  segment probe `0x001228F0` with the actor's vtable +0x28 `0x0003E2E0`; the fly state (type 0x6D,
  `0x00090D10`..`0x000916C0`, beside the walk alert); the map view scene control (type 0x9E, `map_view_control.cpp`:
  `0x00083E80`..`0x00083FE0`) and the book map's menu (`0x00046F40`, `0x000473D0`, `0x000475B0`, `0x00047770`);
  entityStateShoot (type 0x81, `body_shoot_state.cpp`: `0x0009F6C0`..`0x000A0320`, `0x00140A60`); the mind's
  engaged actor `0x00028C50`.
- Host fixes: the scene's disposal walk finds controls the dialogue made (`0x000584A0`'s map view); the kill tallies
  `script_activate_title` (`0x00128AB0`) clears are the block the conditions read (DAT_00512B78), and it tests the
  event state (DAT_005064D8) rather than an unset copy; scene start clears the event sequence and DAT_0052C274
  (`0x00116E00`) and the script reset clears DAT_00512378 / DAT_00512414 / DAT_00512418 (`0x001287F0`).
- Shown (looked at): the fire imps glowing around Cooper and the cook (`cache/scene2a1_3_6600.bmp`), Cooper holding
  the extinguisher with its charge gauge (`scene2a1_3_900.bmp`), the cook's potion page (`scene2a1_3_5400.bmp`), the
  shut "?" exit door after a retry (`scene2a1_3_13200.bmp`, from an earlier run), the spray meeting an imp
  (`scene2a1_3_1500.bmp`), a dying imp's green dissolve over its silhouette (`scene2a1_3_1200.bmp`) and Free Play's
  walk out (`scene2a1_3_3300.bmp`). Not shown: the look-for-weapon question mark. Thin white streaks at a dying
  imp's base are not identified. Not listened to.
- The glow composite (`0x00062750`) drew an opaque square over the dying imp's screen rectangle, and over an imp in
  the spray. Its alpha does not depend on the captured page, so only the stamp's stencil keeps it to the
  silhouette, and the stamp wrote none. Three causes, all fixed:
  - `D3DDevice_SetVertexShaderConstantNotInline` (D3D `0x001735D0`) was an empty host stand-in. It now copies its
    dwords from the shader record's constant pointer (`PTR_DAT_00427200`, `0x004271E8 + n * 0x18`, DAT_004273C8)
    into the device constants. Without them the stamp's `oPos = v0 + c0` took a stale matrix row as c0, and its w
    went negative.
  - The DOT_ZW depth was the inverse ratio. As texm3x2depth, stage 2's dot product is z and stage 3's is w.
  - A stage whose texture mode is NONE now reads (0, 0, 0, 1), as xemu does. It read 0, so the stamp's alpha
    (its t0) failed the alpha test (GREATER 5) and its stencil ops never ran.

### 2026-10-23: chapter 2a scene 1 (the kitchen stairwell) plays to its end

`--scene2a1` (new) boots `aid_script_ghoulies_chapter2a_scene1_1playcam`. The chapter title moves on after A, Free
Play starts at frame 400 and the fire imps (strategy type 0x47) hover and breathe fire while Cooper climbs the
stairwell; the script moves on to `aid_script_ghoulies_chapter2a_scene1_2book` ("Fire extinguihser weapon Intro") at
frame 9132.

- New translations: the imp's flamethrower strategy (type 0x47, `imp_flamethrower_strategy.cpp`: `0x000AAE40`,
  `0x000B1350`, `0x000B4FE0`..`0x000B58F0`, leaving through the spider's `0x000B5AF0`); the hover state (type 0x77,
  `body_hover_state.cpp`: `0x00097B00`..`0x00097E60`); the fly alert (type 0x6E, `body_walk_target_state.cpp`:
  `0x000917B0`..`0x000923B0`); the flamethrower state (type 0x6A, `body_flamethrower_state.cpp`: `0x0008F6D0`..
  `0x000902A0`: the aim, the sway, the flame effect on locator kind 5 and the five-sphere sweep that sends 0x15 to the
  touched actors and 0x41 to the imp); the gun (type 0xA4, `gun_weapon.cpp`) and the fire extinguisher's
  construction, update and draw (type 0xA3, `extinguisher_weapon.cpp`); the third stick pair `0x00108AB0`; body
  message 0x19's contact views.
- Host fixes: the scene services find a constructing player's views by actor rather than by object.
- Shown (looked at): Cooper at the stairwell wall (`cache/scene2a1_2100.bmp`), climbing (`scene2a1_8700.bmp`) and at
  the kitchen with the cook in view (`scene2a1_9000.bmp`). Not shown: no capture has an imp or its flame on screen;
  the flamethrower is heard (XACT_SOUNDBANK_GIMPFIRE_FLAMETHROWER) but its effect has not been looked at.

### 2026-10-23: scene 12 (the wine cellar's haunted door) plays to its end

`--scene12` (new) boots `aid_script_ghoulies_chapter1_scene12_1playcam`. Cooper walks in and fights the spiders in Free
Play; near the exit the "Haunted Door Activation" section registers door 87 from the background (script node 0x56,
message 0x67 to DAT_00511B20), plays `hdooractivate` and queues the haunted door's intro caption (node 0x5E). The
haunted door (strategy type 0x42) wakes as Cooper comes within 60, knocks him flying with its scoop-and-whack and is
beaten in "Event 'One Type' (Haunted Door)"; Cooper's "I sure showed him the door!" page follows, then Free Play and
the exit, and the script moves on to `aid_script_ghoulies_chapter2a_scene1_1playcam` at frame 5837.

- New translations: the haunted door's strategy (type 0x42, `haunted_door_strategy.cpp`: `0x000AF920`..`0x000B05B0`),
  the scoop-and-whack state (type 0x7E, `body_scoop_whack_state.cpp`: `0x0009EB20`..`0x0009EF30`, `0x0013AD60`) and
  the flip state (type 0x6B, `body_flip_state.cpp`: `0x00090410`..`0x000908D0`); shared strategy helpers
  `0x00041560`, `0x0002B9F0` and `0x00139550`; condition update opcode 11 (`0x00133330`); condition node types 0x92
  and 0x93 (opcodes 21 and 22); script node 0x56's background message (`0x00122F70`) and node 0x5E's ghoul intro
  caption (`0x000F4930`).
- Host fixes: none beyond the bindings above. The driver now walks to a far ghouly with the goal walk's held-cell
  avoidance (it had wedged Cooper between the crates before the door).
- Shown (looked at): the door coming off its frame (`cache/scene12_2400.bmp`), Cooper at the door
  (`scene12_2700.bmp`), the haunted door loose in the cellar with Cooper on two hearts (`scene12_4500.bmp`) and the
  one-liner page (`scene12_page1.bmp`). The flip itself was not captured.

### 2026-10-22: scene 11 (the ugly girl's Super Scary shock and the kickabout) plays to its end

`--scene11` (new) boots `aid_script_ghoulies_chapter1_scene11_2book`; its book's five pages pass to
`aid_script_ghoulies_chapter1_scene11_3viewcam` (the archives). The Super Scary shock (type 0x22) waiting at its marker
starts the "uglygirlturnaround" ditty; message 0x88 finds Cooper within its reach, so it spawns the ugly girl from its
template and starts the Super Scary challenge (type 0x98). The driver presses the shown buttons, Crivens' "successfully
overcome it!" page follows, Free Play and "Ugly Partner Kickabout" play with the ugly girl at Cooper's side, the
skeletons come in and are beaten, and Cooper leaves through the exit; the script moves on to
`aid_script_ghoulies_chapter1_scene12_1playcam` at frame 3041. Left unanswered, the challenge fails into Crivens'
page and the scene restarts.

- New translations: the Super Scary shock (type 0x22, `super_scary_shock.cpp`: `0x00072250`..`0x00072980`) and the
  ugly girl's strategy (type 0x53, `ugly_girl_strategy.cpp`: `0x000C1300`..`0x000C14E0`), which starts its named
  attachment (`0x0003D1B0`) in state 0x3EC and removes it (`0x0003CFD0`) on leaving. The archives' carried
  container (type 0xA2, `container_weapon.cpp`: `0x000C75B0`..`0x000C7AF0`) is built over the weapon.
- Host fixes: the surface-mode scales (DAT_00217EC0) are bound as the retail table's 18 floats, not 8; mode 12
  (0.15) lay outside the old view once the skeletons walked.
- Shown (looked at): the ugly girl's shock close-up (`cache/scene11_3_300.bmp`), Crivens' page
  (`scene11_3_page1.bmp`), the kickabout with the ugly girl beside Cooper (`scene11_3_1500.bmp`) and Cooper on his
  way to the exit (`scene11_3_2700.bmp`). The skeletons' fight was not captured.

### 2026-10-22: scene 10 (the relics store's Super Scary mummy) plays to its end

`--scene10` (new) boots `aid_script_ghoulies_chapter1_scene10_1playcam` and plays it to its end. The scene's control
(type 0x94) plays the walk cutscene, the relic candle it carries burning (the candle's flame, shared with the weapon);
the cutscene's pulses bring the "sssmummy" ghoulybox to one of six markers, and once the mummy stands the control
puts the candle out, spawns Cooper holding the candle and starts the mummy's Super Scary challenge (type 0x98).
Crivens' "Gadzooks! ... SUPER SCARY SHOCK" page follows, Free Play takes Cooper to door 9, "Exit Door Closes" and
"Event: Kill All" play, and once the ghouly is knocked down the script moves on through "Ugly Partner
Transformation" to `aid_script_ghoulies_chapter1_scene11_2book` at frame 6078.

- New translations: the relics-store control (type 0x94, `relics_store_control.cpp`: `0x00080FF0`..`0x00081650`,
  its cutscene pulse `0x00081630` registered as a cutscene event callback); script condition opcode 4's update
  (`0x00133230`). The candle flame functions (`0x000C6C80`..`0x000C7540`) now take a flame block and its actor
  (`RuntimeCandleFlame`), so the weapon (+0xE5C) and the control (+0x40) share them.
- Host fixes: a player spawned holding a weapon (0x00134920 with the candle) attaches its body methods and message
  slot on demand while it is constructed. Players read message 0xB's retail words through the decoder the base
  actor uses. `object_of` no longer matches a moved-from object for an unresolved address, and a weapon still
  being created is matched by its bytes only after the scene's kept objects. The released-object notice (0x14)
  names the object by its retail address, so a body clears its hit timer (+0x760, 0x00027B20); the timer requests
  read +0x760 / +0x7F0 as retail does. The Super Scary challenge's held-actor view and drop (0x0001B810) are bound.
- Shown (looked at): the walk cutscene with the lit relic candle (`cache/scene10_0.bmp`), the mummy's Super Scary
  attack (`scene10_2400.bmp`), Crivens' page (`scene10_page1.bmp`), Kill All with the candle and the fuel meter
  (`scene10_4200.bmp`) and the open exit (`scene10_6000.bmp`).

### 2026-10-22: scene 9 (the basement cellar's mummy) plays to its end

`--scene9` (new) boots `aid_script_ghoulies_chapter1_scene9_1playcam` and plays it to its end. The cellar renders with
its light grid and point lights; "Free Play" holds on PlayerApproachesDoor (door 8) and PlayerEntersRegion. Cooper
walks to region 1 (frame ~7200), "Mummy Enters Room" plays the mummy's walk-in and Crivens' "Blimey, a mummy!" page, and
"Event 'One Type' (Mummies)" starts. Ordinary hits do not harm the mummy; after the knockdown count (condition opcode 5)
Crivens' second page gives the special candle, Cooper burns the mummy with it, "Player Leaves Through Door 8" opens the
exit and the script moves on to `aid_script_ghoulies_chapter1_scene10_1playcam` at frame ~9600.

- New translations: the mummy strategy (type 0x4C, `mummy_strategy.cpp`: `0x000BB8E0`..`0x000BC580`); the candle
  weapon (type 0xA1, `candle_weapon.cpp`: `0x000C64D0`..`0x000C7540`, its flame, scene and point lights, fuel and
  burning hits); the burn outline (type 0x31, `fx_custom_electric.cpp`: `0x000772E0`..`0x00077D30`); the scene's point
  lights and light grid (`scene_point_lights.cpp`, `scene_light_grid.cpp`, `scene_point_light_pass.cpp`), the
  animated light (type 0x13) and the scene fx control (type 0x97); script type 0x31 (`0x00123AD0` and the
  `0x0013A720` weapon give) and condition opcode 5's update (`0x001332B0`); the scoreboard meter's level, textures,
  shown update, buzzer and draw (`0x0012E770`, `0x0012EC70`, `0x0012E900`, `0x0012ED30`, `0x0012ED90`,
  `0x0012EA50`); `0x00140A00`, `0x001408C0`, `0x001408E0`, `0x00134620` and the truncating remainder `0x00152BE0`.
- Host fixes: the body attachment lists named the empty host copy of DAT_00506930 (`timed_attachments`) instead of
  the live effect table `g_actor_effect_nodes`, so the first knockdown crashed. A weapon created through
  `create_runtime_player_attachment` (and `0x0001A6D0`, the player's starting weapon) was looked up in a map nothing
  filled; the creation context now asks the scene for the new object's attachment view (`view_of`). DAT_00430D14 is
  the 0x8C-byte weapon name it is in retail, not one byte.
- Driver: `play_scene7` takes `doors_first`; the walk avoids cells where Cooper is held (`nav_waypoint`'s `avoid`).
- Shown (looked at): the mummy at its door with its HUD portrait (`cache/scene9_7500.bmp`), both Crivens pages
  (`scene9_page1.bmp`, `scene9_page2.bmp`), the lit candle with its glow, the burn sparks and the fuel meter
  (`scene9_9000.bmp`) and the meter after the burn by the exit (`scene9_9300.bmp`).

### 2026-10-22: scene 8 (the gaming room's haunted chairs) plays to its end

`--scene8` (new) boots `aid_script_ghoulies_chapter1_scene8_1playcam` and plays it to its end with the scene 7
driver. Cooper is spawned at the door marker (type 0x2D, `0x00127FF0`), "Exit Door Closes" fires on
PlayerApproachesDoor (condition 6, `0x00132840`), the chairs-flip-off cutscene and Crivens' "Crikey, haunted chairs!"
page play, and "Kill All" runs its event: the haunted chairs (type 0x40, `entityStrategyHauntedChair`) wake, swing,
slide at Cooper and mimic him, and break apart when beaten. The event completes once every chair is broken (about
frame 4250, EVENTCOMPLETE), the exit door opens, Cooper lines up with it and leaves, and the script moves on to
`aid_script_ghoulies_chapter1_scene9_1playcam` at frame 4644.

- New translations: the scene control (type 0x93, `haunted_chairs_control.cpp`), the ghouly swap setup (type 0x26,
  `ghouly_swap_setup.cpp`), the haunted chair strategy (type 0x40, `haunted_chair_strategy.cpp`), the scrabble state
  (type 0x7F, `body_scrabble_state.cpp`) and the slide state (type 0x83, `body_slide_state.cpp`: `0x000A0C10`..
  `0x000A1370`); the model copy by AID (`0x000F8A10`, from the actor model copy `0x00040CA0`); the type-4 effect
  particle model (`0x000F0160`) and its matrix build (`0x000F8EA0`); script condition nodes 0x42, 0x5A, 0x5B and
  0x72, their updates (`0x001333A0`, `0x001334B0`) and evaluators (opcode 6, opcode 17); and event step type 2 (script
  type 0x1C: `0x00125D00`, its runner `0x000CFDA0` and its icon arm in `0x000D0F20`).
- "Kill All" used to hold for ever: script type 0x1C computed its tag and queued nothing, so the event's step pool
  stayed empty, the event never ended and its end notice (0x83, the section's credit) never came. Its step then
  completed after the first kill, because the class count `0x00129240` walked the unfilled host `g_alt_links`; it now
  walks the native `DAT_00511B48` list (`g_alt_node_body`, `g_alt_node_class`).
- The chairs showed twice during the chairs-flip-off cutscene (the static chairs stayed while the cutscene's chairs
  left the table): script type 0x74 skipped `0x00115F40`, the {1} broadcast to the scene controls, so the chairs
  control (type 0x93) never armed and its 0x7E hide (`0x00082780`) never ran. Once armed, the hide's "bound" test
  (shared with `0x00082810`) read only the +0x918 link; retail also needs that parent link's own +0 word, so every
  chair on a table counted as bound and lost its cutscene row, and the chairs stopped animating. Looked at: frames
  620..680 of the flip, the chairs tipping off the tables and standing on the floor with none left behind.
- Host fixes: the death drop and death effects now hand their objects to the scene and message them through it
  (`RuntimeBodyDeathDrops::keep` / `message`; the old `created` map was never filled). The prop contact views are
  dropped when their object is released, since a later object at the same actor address crashed through the
  stale bytes after a retry.
- Driver: `play_scene7` takes a fight range and a title. Scene 8 fights any live ghouly in the room only during
  "Event 'Kill All'" (the room's other chairs sleep until hit), and in a "Player Leaves Through Door N" section
  Cooper lines up 8 units in front of the door's actor (`0x0005BEC0`) on the exit volume's z before walking in,
  because the open door panel blocks a straight walk to the volume.
- Shown (looked at): Cooper fighting the chairs in the gaming room (`cache/scene8_1800.bmp`), Crivens' page
  (`scene8_page1.bmp`), the exit door's cutscene (`scene8_2100.bmp`) and the "Energy Boost!" pickup with chair debris
  (`scene8_2400.bmp`). A Cooper knocked out by the chairs (`scene8_4200.bmp`, an earlier run) restarts the scene at
  Free Play.

### 2026-10-21: scene 7_3 plays to its end: the TV's shock, its ghouly generator and the exit

`--scene7` now plays `aid_script_ghoulies_chapter1_scene7_3playcam` to its end. Cooper loses to the TV three times
(each loss restarts the scene at Free Play); on the fourth try the script moves on to
`aid_script_ghoulies_chapter1_scene8_1playcam` at frame 7786.

- **fxCustom and entityFxCustomElectric** (types 0xC and 0x2E, `entities/fx_custom_electric`):
  - fxCustom (getter `0x000EC650`, vtable `0x005068F4`): sizes `0x00095530`, construct `0x000EC6A0`, destroy
    `0x000EC6E0`, update `0x000EC710`. It joins its actor's +0x1D8 frame list; the frame walks run its update and draw.
  - The electric effect (getter `0x00076590`, vtable `0x005053E0`): sizes `0x00076570`, construct `0x000765E0` (the
    sparks `0x000769D0` and the light `0x00076B00`), destroy `0x00076690`, update `0x00076720`, draw `0x00076850`.
  - Light records: `0x000FE080` (with `0x00051BC0`), `0x000FE0F0` and `0x00056C80`.
- **entityFxCustomSolidFadeIn** (type 0x32, getter `0x00077DB0`, vtable `0x00505440`, same module): sizes
  `0x00077D90`, construct `0x00077E00`, destroy `0x00077E90`, message `0x000780C0`, update `0x00077ED0`, restart
  `0x00078110`. It fades the actor's tint (+0x198 and the +0x19C levels) in, holds and fades it out, then releases
  itself. It is both the generator's release effect and the body's hold-timer object (`0x00027BE0` sends it 3; the
  timer's end, `0x0013B240`, sends it 5).
- **entityStateGenerateGhouly** (type 0x6F, getter `0x00093B60`, vtable `0x005058C0`, `entities/generate_ghouly_state`):
  - Slots: sizes `0x00093B40` (0x4A0 / 0x318), construct `0x00093BC0`, destroy `0x00093DF0`, message `0x00094150`,
    tick `0x00093EC0`, draw `0x00094130`.
  - Phase 1 plays the intro and its bolts (`0x000925C0`, `0x00092840`, `0x00092D50`, `0x00092F80`). When the
    animation ends, the beam actor and its light stand at the spawn point (`0x00093700`, `0x00093800`, `0x00093A60`,
    `0x00093AD0`).
  - Phase 2 (`0x000941A0`) lets out the ghouly the TV's box names (`0x00093190`, `0x000933C0`) and grows it over half a
    second (`0x00093540`). Phase 3 fades the beam (`0x00093780`, `0x000936A0`) and drains the ghouly's goals
    (`0x00093660`).
  - The bolts draw as view-facing quads (`0x00092D10`, `0x00092FF0`, and the new `0x00065460` / `0x000654E0` in
    `graphics/world_sprite_passes`). `0x0003EE30` relinks an actor into the live list.
- **entityStateShocked** (type 0x80, getter `0x0009F420`, vtable `0x00505AE0`, `entities/body_shocked_state`):
  - Slots: sizes `0x0009F400`, construct `0x0009F480`, destroy `0x0009F550`, tick `0x0009F5F0`.
  - Cooper drops what he holds and freezes with every latch held while the shock effect plays, then recovers
    (`0x00133F50`). `0x0001F4F0` (the latch hold timer) is new in `player_goal_cleanup`.
- **Door state 6** (`0x0005B540`): releases the row's two clips and moves the last row into its place (`0x000F71F0`).
- Fixes found on the way:
  - The actor frame list's objects are now keyed by their bytes and found in the scene when used. A
    `FactoryObject*` taken at construction pointed at the factory's stack copy.
  - Render lights carry their +0x40 spot words (`RuntimeRenderLight::spot`) in place of a side map that nothing
    filled. The TV's light was the first point light to reach the upload.
  - The debris and trail pools (`0x000EC2D0`, `0x000EBB80`) are now built at each scene's start, as `0x0011CBD0`
    does; `0x000ED450` frees them at each scene's end. Before, they were built once at boot, so the second scene had
    none.
  - The body timers' objects (`0x00027B60`, `0x00027BE0`) are kept by the scene and messaged through it. Their native
    views came from a map nothing filled.
  - `0x0003DE30` is one scene function, used by the TV, the shock state and the fade-in.
  - `0x0013AF80` takes its EDI tint (the generator's beam fades with {1, 100, 1}).
- Shown (looked at): Cooper knocked back by the TV's sparks with a spider in mid-air (`cache/scene7_3_900.bmp`),
  Cooper shocked in a blue glow beside the TV (`scene7_3_2400.bmp`, `scene7_3_6900.bmp`), and Cooper kicking a spider
  while the TV crackles in the corner (`scene7_3_7500.bmp`).
- Not shown: no capture caught the generator's bolts, beam or light, so they are not confirmed on screen. The
  shocked tint is not compared with retail.

### 2026-10-21: scene 7_3 starts: the haunted TV and the knock-down sphere

`--scene7`'s third leg, `aid_script_ghoulies_chapter1_scene7_3playcam`, now starts natively. Cooper walks in, the
haunted TV plays its ghoul intro (looked at: `cache/scene7_3_600.bmp`, the "Haunted Television" card over the TV
with static on its screen), and he fights it. Around frame 600 its attack makes the electric effect object
(type 0x2E), which is not translated (see the frontier).

- **entityStrategyHauntedTV** (type 0x44, getter `0x000B1EC0`, vtable `0x00505E6C`,
  `entities/haunted_tv_strategy`):
  - Slots: sizes `0x000B1EA0` (0x11C / 0x498), construct `0x000B1F10` (`0x000B1AD0` the screen, `0x000B1490` the
    timeline), destroy `0x000B1F40` (`0x000B1B50`, `0x000B1530`), message `0x000B23F0`, update `0x000B1F60`, enter
    `0x000B2650`, leave `0x000B2900`.
  - The screen is the model's shader_blinn22 colour0 parameter. `0x000B1BA0` gives it one of six parameter textures
    per mode (2 and 3 loop the STATIC sound in actor sound slot 6; 1 clicks, 5 turns off). Mode 4 (`0x000B1D30`)
    shows "aid_texture_ghoulies_actor_hauntedtv_<tag>" for the ghouly in the actor's box (+0xBA0), dispatching a
    spawn when the box name is not an AID.
  - The attack timeline (+0x108) is a list of {on, seconds} rows. `0x000B1710` / `0x000B1780` start and stop it,
    `0x000B1590` steps it behind the gate `0x000B1A90`, and `0x000B17B0` / `0x000B1940` switch the attack on and off:
    latch bytes 0..2 and 5, tag-0 contact shapes, the body's 0x65 attack row (`0x0001C560`) at +0x84, the live
    attachments' +0x15 holds, and the parameters' effect object (the inlined `0x0003DF40`, released by `0x0003DE30`).
  - States 0x3EE (near the target), 0x3EF (let a ghouly out), 0x3FE (`0x000B2A60`: mind request 0xD), 0x409 (knocked
    down), 0x40F, 0x413 (asleep), 0x416 (wait) and 0x417 (wake). Their inlined state changes are `0x0002B8D0`. Other
    helpers: `0x000B29D0` (target range), `0x00141530`, `0x00042450`, `0x000415A0`, `0x0013A400`.
- **Shared shader parameters** (`graphics/shader_parameters`): the book's `0x000F8540`, `0x000F85D0`, `0x000F86F0`
  and `0x0015B2B0` moved here (the struct is now `RuntimeShaderTextureParameter`). `0x000F8640` and `0x00053640`
  (stepping an animated texture's frame) are new.
- **entityAvatarMiscKnockDownSphere** (type 0x19, getter `0x0006EB40`, vtable `0x00504D60`,
  `entities/contact_prop_actor`): sizes `0x0006EB20`, construct `0x0006EB80`, message `0x0006EBD0`, update `0x0006EBA0`
  and `0x0006EC10`, destroy the contact prop's `0x0006EEA0`. Once message 1 switches it on, every frame it tells each
  actor its sphere touches {0x17, reply, sender, contacts, point}; both contact views now carry that notice.
- Bindings: the scene's `create_effect` (`0x0003DF40`) is now real, and a player's ghoulybox spawn controller
  (record +0x24) sends its 0xA7 / 0xA9 / 0xAA requests to the actor's vtable +0xC. It had no message function, so the
  TV's first spawn dispatch crashed.

### 2026-10-21: scene 7 (the cinema) and its book play to their ends

`--scene7` (new) boots `aid_script_ghoulies_chapter1_scene7_1playcam`, the scene `--scene6-1` moves on to:

- The walk-in and "Free Play" start natively. Free Play waits for region 1 (PlayerEntersRegion), the projection booth
  up the stairs. There the film-reel prisoner's page plays ("What's she doing here?", looked at:
  `cache/scene7_page1.bmp`).
- Its section then holds at its end (DAT_00512518) with the exit volume open. Cooper takes the booth's far stairs down
  into the cinema, where skeletons and imps fight him; once none is within 50 units he walks into the exit. The
  script moves on to `aid_script_ghoulies_chapter1_scene7_2book` ("Partner Down the Trapdoor") at frame 2311.
- The book plays natively: three pages over its comic panels (Amber stuck on her chair, Cooper's "Okay, don't panic!",
  Crivens' "Oh no! She must have fallen into the Archives!", looked at: `cache/scene7_2_page1..3.bmp`). The script
  moves on to `aid_script_ghoulies_chapter1_scene7_3playcam` at frame 2081, whose native start stops at type 0x44.
- New class: **entitySceneControlMovieVolume** (type 0x96, getter `0x000840D0`, vtable `0x00505650`,
  `scene/scene_controls.cpp`): sizes `0x000840B0` (0x58 / 0x1C) and the constructor `0x00055A90`. That constructor
  returns 0, so the factory destroys the scene's movie volume as soon as it is made, and its message (`0x00084120`)
  and tick (`0x00084220`) never run.
- Shown (looked at): Cooper at the top of the walk-in corridor, in the booth beside the projector, and among the
  cinema's skeletons with hit sparks and a falling skeleton (`cache/scene7_600.bmp`, `scene7_2100.bmp`,
  `scene7_1800.bmp`).
- Test steering: `nav_waypoint` (tests) steps Cooper along the navigation grid. Scene 3's trophy walk now steers each
  leg by it: since the 2026-10-21 spawn-trigger fix the skeletons can end their fight below the room's floor, where
  the old straight legs pinned Cooper against a wall (`--scene6-save` failed there).
- Fixed test: `--actor-loading`'s matrix-setup check gives its model the gate word `0x000400D0` has read since the
  2026-10-20 bone debris commit (model CPU +0x10 -> +0x10).

### 2026-10-21: scene 6 plays to its end: the spiders' arc attack and the corridor exit

`--scene6-1` now plays the Lower Corridors to its end. The generator's spiders crouch, rise, spin and leap at Cooper,
and he fights them off down the corridor. The section has held at its end since about frame 1200 (DAT_00512518), so he
walks out through the doorway in the corridor's south wall. The script moves on to
`aid_script_ghoulies_chapter1_scene7_1playcam` at frame 2831.

- **entityStateArcAttack** (type 0x5F, getter `0x000889B0`, vtable `0x005056C0`, `entities/body_arc_attack_state`):
  - Slots: sizes `0x00088990` (0x268 / 0x214), construct `0x00088A10`, destroy `0x00088ED0`, tick `0x00089060`,
    message `0x00089530`. +0x14 and +0x18 are the empty `0x001628B0` and +0x1C is `0x0002B620`.
  - Phases: take-off (until the take-off animation passes the parameters' time), rise (`0x00089AB0`: body +0x18
    lifted by a random height, or the target record's +0x30 by the height scale), hold (`0x00089BA0`, spinning about
    x, `0x00089C00`), leap (`0x00089760`, a `0x00140D80` arc at the mind's target record or 40 ahead, with the
    contact shapes, `0x0001F800`, and the six latches held) and retreat (`0x000898C0`, message 0x41: back toward the
    start by half the way, between 3 and 6 reaches).
  - The leap ends in `0x00089300` on a contact bit: a landing (`0x0008A1A0`, the end key or body slot +0x34 after
    the class's 0x11 value), or `0x00089DB0`. That one either hurts the target (body +0x26C..+0x280, +0x220..+0x22C,
    `0x00021120`, the body's +0x4C key) or reflects the velocity off the contact's normal. Message 0x62's contact
    record (`0x0008A030`, head-on below -0.7) and message 0x6E (`0x00140940`) reach the same paths. The face turn is
    the tail `0x00089D20` (exported with `range-function-force`).
  - Body slot +0x4C (`0x00027500`) is bound for types 0x27, 0x2A and 0x2D.
- **The debris draw** (`0x000EC470`): each live record (+0x68 == -2) is a textured quad (`0x0005CF90`, frame 0) at its
  position and euler. Its size is +0x20 x +0x18 and its colour is four `0x001673D8` bytes from +0x24..+0x30. The
  texture is its type row's +0x80 (`RuntimeEffectDebrisStorage::textures`).
- **The base strategy messages' actor view** (`message_actor`): the actor's +0x1FC / +0x1FE words, its +0x8F8 target
  record (the first live attachment's mind target) and, for a player, the actor its body +0x274 names. Message 0x8F
  (`0x00138EF0`) reads it when a zombie strategy hears the scene's script.
- **Mind target body views**: a target actor whose +0x95C word is not in the scene arena now gets the empty view a zero
  word gets. A wielded weapon's (type 0xAA) holds 3; retail would fault if it read it as a body.
- Shown (`cache/scene6_1_2400.bmp`, looked at): Cooper knocked back in the corridor by a spider, three spiders on
  the floor, a smashed vase's debris and a dropped heart pickup.
- Verified (with the scene 7 work): Release build; ctest 7/7; the Debug build compiles (not run). Passing modes:
  - default, `--player-death` with and without `--retail-step`, `--player-fight`, `--player-opening` and
    `--player-controls`;
  - `--scene3`, `--scene4`, `--scene5`, `--scene6`, `--scene6-save` and `--scene6-save-reload` (run back to back:
    every other mode empties `cache/test_saves`), `--scene6-1`, and `--scene7` to the end of scene 7_1;
  - `--gameselect-new-game` and `--gameselect-replay-scene`;
  - the six transition suites (`--actor-loading` after its test fix), `--title`, `--title-a`, `--attract`, `--goop`,
    `--ui` and `--audio-preparation`.
  `gbtg --headless --frames 900` exits 0 with and without `--scene chapter1_scene1_2playcam`. `git diff --check` is
  clean.
- Not matching: `player_opening_check` and `player_walk_check` differ from `cache/player_ref`. The tutorial page's
  lines sit a few pixels apart in word spacing; the walk check differs in 32 pixels. These checks last matched on
  2026-10-18, and the dialogue text code changed on 2026-10-19 (`63e6ad8`); this work does not touch it. Not
  bisected.

### 2026-10-21: scene 6 (the Lower Corridors) runs into its Ghouly Generator Introduction

- `--scene6-1` (new) boots `aid_script_ghoulies_chapter1_scene6_1playcam`, the scene `--scene5` moves on to (scene
  order 11). The scene starts natively; then:
  - The walk-in and Crivens' guide page play.
  - In "Free Play", Cooper walks to the telephone scare figure. It fades in and scares him (headless frame 900,
    looked at: Cooper's scared pose, the HUD heart at 15 with the scare head).
  - "Scare Introduction" plays the shock1 cutscene, then Crivens' "Sir has suffered a bit of a Scare!" page.
  - In "Ghouly Generator Introduction", the setup Cooper walks under spawns its ghoulybox. Its spiders hunt and
    flank him and back off as he chases them.
  - It stops at the spiders' arc attack (type 0x5F); see the frontier.
- New classes, from their dumps:
  - **entityBodyZombie** (type 0x2D, getter `0x000761A0`): the 0x27 parent's sizes and message slot. Its next-action
    slots `0x00076210` / `0x00076250` return 0x116 / 0x16D while the body's model-node state is shown (+0x2D0 mode 1,
    +0x2D2 set).
  - **entityStrategyZombie** (type 0x58, getter `0x000C57A0`, `entities/zombie_strategy`): the approach, swipe,
    kick, special attack and weapon fetch, from `0x000C4E70` to `0x000C5AD0`.
  - **entityStrategySpider** (type 0x52, getter `0x000C0B60`, `entities/spider_strategy`): the flank 30 units
    beside the target, the bite, the sticky web spit and the roam (`0x000C0620`..`0x000C1290`, leave `0x000B5AF0`).
  - **entityWeaponProjectile** (type 0xA7, getter `0x000CB570`, `entities/thrown_projectile`): the thrown weapon
    that spins, shatters or breaks, and spawns the actor its parameters name where it lands (`0x000CB550`..
    `0x000CC910`, the flight set-up `0x00148E60`, the ballistic launch `0x00140D80`).
  - **entityAvatarMiscScareFigure** (type 0x1D, getter `0x0006EFB0`, `entities/scare_figure`): its states and timers,
    the player-facing turn, the glow-pass fade draw and the trigger (`0x0006EF90`..`0x0006FC70`, `0x0013D510`,
    `0x000423D0`).
- Shared fixes:
  - `0x0013E980` (spawn triggers) walked `g_world.slot` as a fake node struct that read past each 16-byte node. It
    also tested `DAT_004E1C34` (the prompt lock, -1) where retail tests the never-written `DAT_00512218`, so it never
    saw Cooper. It now walks the native DAT_00511B50 view (`g_spawn_trigger_players`). Ghouly setups, triggers and
    weapons now see a player enter their radius, as in retail. `--scene4` still passes.
  - `0x00122CE0` (message to every DAT_00511B38 actor) used the same fake-node pattern. It now goes through
    `g_marked_actor_message`.
  - Marker kinds 7 (named sounds) and 0x16 (scare figures) are bound. The names live in DAT_0051254C's rows, whose
    count `0x00129F70` now resets at the scene teardown.
  - Node 0x6C (`0x00125748`) registers condition opcode 16 (GhoulyExistsInScene). Its test `0x00132C70` and its
    update `0x001336A0` walk the native DAT_00511B38 (`runtime_loading_actors`).
  - The stand-alert and walk-alert ticks' turn settle (`0x00134720`) is bound, with its `0x001068E0` lean. The state
    destroy's body message slot and the boost's +0x44 / +0x48 slots are bound too.
- Not checked: the scare figure's fade and the spiders' moves against xemu, and the thrown projectile, which nobody
  throws yet. The full regression set (the shared fixes change other scenes' paths) has not been run; `--scene4`,
  the animation unit tests and `gbtg --headless --frames 900` (with and without the opening scene) pass.

### 2026-10-20: finishing reacting imps, scene music, and scene 5's entry

- **Finishing a reacting imp.** Cooper's attack pick `0x0001CDA0` asks `0x0001D130` whether its target needs the
  special (finishing) attack. The port passed Cooper's own body to `0x001413C0` / `0x00134650` and tested the
  target's actor +0xCE0/+0xCDC. The asm reads the target record's +0x10 actor's body (`MOV ECX,EBP` at
  0x00141420) and ESI = the requesting body's actor (`local_24`, body +8). The old oracle used one actor for both,
  so it could not catch the swap. Now the target body rides in `RuntimeMindChainRequestSelection::target_body`.
  - Effect: the imp's statsheet stagger count (row `objTag_Actor_Imp`, key 0x10) is 1, so one hit empties its
    meter (body +0x16C, `0x0001FF60`). While it reacts (IsBeingHit, body +0x23C = 5) Cooper's next attack is the
    kind-3 knockdown finisher (sub 0x36, 10 damage), which takes it to 0. The meter refills (0.99) when the
    reaction ends without one (`0x00134060` → `0x001346B0`), so only a hit inside the reaction finishes, at any
    health. Before, kind-1 hits at 1 health were
    floored by `0x000215C0` (no death rows 0x68..0x6A) until the dizzy state, and one imp took 17 such hits.
  - Measured with temporary probes (removed): every opening imp now dies on the hit after its first one. The
    skeletons take the same path. Not compared with xemu.
- **Scene music.** `0x0011CAC0` (scene start) calls `0x00116E00`, whose first part releases the stream the last
  scene's script left in DAT_0050E4E0. That release is made now (`release_scene_music_at_start`); the rest of
  `0x00116E00` is still not translated. Measured: at scene 3's start after `--scene6`,
  `GSTREAMSROOMS_TROPHYROOM` is released; before, it played on. The same holds for the Grand Hallway's streams.
  `0x00117330`'s clear of DAT_0050E560 (the ambient tune made) is made too, so later scenes make their own tune.
- **Scene 5's entry ("Explain player energy").**
  - Script types 0x76 / 0x77 (`0x00127BC0`) were empty. They send the player message 0x42 with 1 / 0 (mind
    +0x7C), which holds Cooper's input around the Baron's page. In `--scene5` he now moves 1.5 units between
    frames 120 and 240, against 56 before.
  - The player mind setup (`0x0007EF10`) read DAT_00430D0C where the dump reads DAT_00430D10, which script type
    0x7 sets. Scene 5's type 0x7 gives Cooper 25 energy; he entered with 50.
  - DAT_00433BE8 and DAT_00433BF0 were two host words each: `0x001307D0` set one copy, and the heart display read
    the other. They are one now (`health_style`, `health_second_voice`).
  - With those, the heart display's style 1 runs: the rest of `0x001301A0`'s style-1 branch, `0x00130900` and
    `0x00130950`. The heart spins random two-digit values (DAT_00433C60) under `GPROPS_RANDOMIZER`, then the
    Baron's `GBARON_*_2` cue by the new energy. Shown in headless captures (looked at): the heart counting 45, 58,
    22 with the Baron's head beside it, then his page. Not listened to.
  - The vine door (door 4, clip `doors_round_1_highleft_opennoentry`) closed around Cooper in the doorway. Its
    node passes +0x14 = 1, so `0x0005B540` leaves the close clip paused and `0x0005AA90` resumes it once
    `0x001412F0` reports the actors settled. That check walked the host range links (`g_range_links`, never
    filled; `span_980` never set), so every actor looked settled and the clip ran at once. It now walks the native
    DAT_00511B38 players (`g_loading_actors`): the front kind of each +0x980 goal ring, +0x984 and +0x98C. Cooper's
    walk-in (body action 0x15C) holds the door until he stands inside; the vines then close beside him (headless
    frames 180 to 250, looked at). The ghoulybox wait in script type 0x2A reads the same check.
- **The snooker table.** The swing's attach (`0x00098E00`) and release (`0x00099280`) match their dumps: the table
  goes back to where it was taken. Getting stuck in it after a swing was not reproduced.

### 2026-10-20: scene 5 plays to its end on its success path: the snooker table swing

- `--scene5` now wins "Required Number" (EVENTCOMPLETE, no FAILPITCHBEND) and moves on to scene 6.
  - In stage 2 Cooper walks to the uncovered table's nearest pick-up sphere and takes it with A.
  - He swings it once, killing the skeletons in its path, and it breaks.
  - The broken table keeps its volume. The run checks Cooper never gets closer than 20 to its centre (he stays at
    about 25.5), then fights on by hand.
- Translated or fixed:
  - Factory type 0x78 (getter `0x00097FB0`) is the huge-weapon swing, in `entities/body_swing_state`:
    - Lifecycle: sizes `0x00097F90`, construct `0x00098010`, destroy `0x00098370`, tick `0x00098460`, message
      `0x00098620`.
    - Phases: phase start `0x00098680`, approach `0x00098780`, attach `0x00098E00`, side `0x00098980`, swing
      `0x00098B30`, spin `0x00098C10`, release `0x00099280`.
    - Effects: start `0x00098F40`, update `0x000991B0`, release `0x00099120`. Bob cutscene `0x000993A0`.
    - Helpers: `0x00026720` and `0x001057F0`.
  - The scene cutscene stop `0x0004EEA0`. `0x00134180` now takes a narrow `RuntimeBodyNextAction`.
  - The bob (type 0xA0):
    - Contact test `0x000C61D0`, and message 0x95 `0x000C6250`.
    - State 7: its entry tally `0x0012ADC0`, then its end `0x000C62C0`, which plays the break animation and then
      sets the next state to 0x10 (this was missing, so the end ran every frame).
    - State 14's start `0x000C63A0`.
  - The kill statistics (DAT_00512B78) that the script conditions and the "Required Number" step read are now the
    same table that `0x0012ACF0` tallies into (`g_script_tallies.second`, seven kinds).
  - `0x0007DEE0` read body +0x84's state byte through an empty view when +0x84 was zero. It crashed mid-fight.
- Not checked: retail's own swing for comparison. The break is checked from captures only.

### 2026-10-20: scene 5 runs natively into its stage 2 skeleton waves

- `--scene5` (new) drives the snooker room:
  - Free Play holds on its three conditions. Cooper picks up a cue, and "Skeletons from the Portraits" plays:
    the skeletons leap out of the portraits.
  - The "Required Number" event runs, and Cooper beats the skeletons.
  - Its NumberGhouliesAlive condition starts "snooker table activates": the cloth cutscene plays.
  - "stage 2" then sends skeleton waves, and Cooper fights them. The section never ends: see Not checked.
- Translated or fixed on the way:
  - Free Play's condition nodes 0x3F, 0x41, 0x43 and 0x44 (opcodes 2, 4, 6 and 7) are registered in
    `0x00125450` (from its jump table at 0x0012593C / 0x001258C8). Free Play ran straight through to scene 6
    without them: 0x43 (PlayerApproachesDoor) was an ordinary node.
  - Factory type 0x66 (getter `0x0008D930`) is the body's arc leap from where it stands to the front of its goal
    ring, between an optional take-off and landing animation. It's `entities/body_arc_state`: sizes `0x0008D910`,
    construct `0x0008D990`, destroy `0x0008DBF0`, message `0x0008DD30`, tick `0x0008DC70` / `0x0008DD70`.
  - A player's body graph (its body vtable slots) is now built before a strategy's first message. A
    just-spawned skeleton's goal reached `body_slot_key` before anything had built it.
  - Script node 0x59 sends message 1 to the DAT_00511B24 objects with its tag (`0x00122FA0`, new). That wakes the
    snooker table (the bob weapon), which plays its cloth cutscene.
  - Script node 0x2B queues its aid spawn record (`0x000F4E50`). It was a no-op, so stage 2 sent no skeletons.
- Not checked:
  - How stage 2 ends. Its timeout condition fires and runs its 0x33, then the section's 0x0F holds in mode 0
    until `g_gate_251c` rises. Nothing native queues 0xAF or 0xB3 here.
  - The "Required Number" event's steps (nodes 0x1D and 0x55) aren't queued, so its scoreboard and success path
    haven't run.

### 2026-10-20: scene 4 plays to its end: the key event, and attachments that start their effects

- `--scene4` drives scene 4_2. Cooper walks to the key's part (part 20 of the gaming room's background) and swings
  until it breaks. The key's show runs, and the event ends. "Free Play" follows, Cooper walks into the exit
  volume, and the script moves on to chapter1_scene5_1playcam.
- The event's key step was never queued:
  - Script node 0x1B now builds step type 1 (`0x00125CC0`, XMM0 0) and queues it (`0x000D05D0`). The other
    step builders in `0x00126140` are still the old value-only cases.
  - Step type 1's start is `0x000D0200`: it keeps one of the class 0x47 key candidates at random. Scene 4_2 has
    one, so nothing is dropped. Dropping a second candidate raises (unbound).
  - Step type 1's run is `0x000CFA90`: it waits for event 0xAE and the step delay, then drops the event. The
    step tables at 0x003039C8 hold (start, run) pairs by type.
  - The 0x81 start broadcast now registers the whole step table (count × 0x88) as a heap block. Its readers
    resolve heap addresses through `memory_at`: heap_address now records each block's bytes.
- Attachments never started their effects:
  - `0x0003D1B0` links new attachment nodes into the pending list (actor +0x1B4, list +8), and `0x0003CD00`
    starts them and moves them to the active list (+0x1AC).
  - The translation linked them straight into the active list, so persistent attachments kept slot -1. The
    update then wrote through `DAT_00506930[-1]`.
  - `ActorAttachmentList` now carries a view of its pending list; `construct_view` binds it, and creation links
    there. The update raises on an empty slot instead of writing through it.
  - The key's sparkle (`aid_fxemitter_ghoulies_misc_bgbreakkey`) is the first visible one. Scenes 3 and 6 show no
    change: all 90 captures are byte-identical.
- Shown (`cache/scene4_2_key_*.bmp`; looked at): the "breakkeycam" view, and the key popping out of the broken
  cabinet with its sparkle. It bounces among the debris, and the HUD's key icon shows.
- Not checked:
  - The bg-break setups' own break, their ghoulybox loads and their spawns. No setup's part broke in the driven
    run: only the key's part did.
  - The scene 4_2 imps' fight, beyond surviving the walk.

### 2026-10-20: scene 4 starts natively: the door, the false fireplace's imps and the hidden key

- Scene 4's book part (chapter1_scene4_1book) already played natively. `--scene4` (new) shows the comic cutscene
  and its three dialogue pages (`cache/scene4_*.bmp`; looked at). The script then moves on to
  chapter1_scene4_2playcam.
- Scene 4_2's start stopped at three gaps, now translated:
  - `0x0005B540` sets the state of door 7, which has no row. Retail adds the row through `0x0005B2D0`, with shape
    12, scale DAT_0042A840 (1, 1, 1), and the origin and default euler (DAT_0050DD80, DAT_0050DD74). It then finds
    the row with `0x000663B0` / `0x0005B870`. The old error named `0x0005B870`, which is only that search's
    comparison. The door dimension table (DAT_00424994) now spans 13 rows: shape 12's row sits just before the
    shape names at 0x00425118.
  - Factory type 0x24, entityAvatarGhoulySetupBgBreak (getter `0x0006A4F0`), is a ghoulybox held behind a
    background part. It's in `entities/ghouly_bg_break_setup`.
    - Construct `0x0006A530`, with `0x0006A350`, `0x0006A120` and `0x0006AF20`. Destroy `0x0006A600`. Update
      `0x0006ADC0`, with `0x0006A3F0` and the spawn-range check. Messages `0x0006A7B0`.
    - Message 0x7D finds the nearest background part (`0x0013B980` → `0x00046310`) and can hold it
      (`0x00046440` / `0x00045E50`).
    - With the player in range, the update breaks the part itself (message 0x13 → `0x00045D40`). The part's
      removal broadcast 0xAB then loads the ghoulybox (`0x0006A470` / `0x0006B0A0`, or the shared-group path
      `0x0006A170`, `0x0006A2C0`, `0x0006A310`) and scares nearby actors (`0x0013B660`).
    - Spawns 0xA7 / 0xA9 / 0xAA place the imps through `0x0006A760`, `0x0006A630` and `0x0006AE10`.
  - Factory type 0x1E, entityAvatarMiscBgBreakKey (getter `0x0006CC70`), is the hidden key. It's in
    `entities/bg_break_key`.
    - Construct `0x0006CCB0`, destroy `0x0006CD10`, messages `0x0006CD40`, update `0x0006CFD0`.
    - Message 0x7D links it to its part, and an event start (0x81) with a kind 1 step arms it.
    - When the armed part breaks: every actor pauses, the key (aid_model_ghoulies_misc_eventkey) jumps out of the
      class 0x47 actor (`0x00129EA0`, new), bounces three times under the "breakkeycam" view and spins. After 2 s
      the event 0xAE is queued and the actors resume.
  - Shared helpers:
    - `0x0013B660` now takes any actor (`scare_near_runtime_actor`); the weapon calls it through an adapter.
    - The background message context is built once (`with_background_context`). The setup's part calls reuse it.
- Shown (`cache/scene4_2_*.bmp`; looked at):
  - The "Find The Key" challenge starts, and Crivens' pages show.
  - Imps spawn and fight Cooper, the health HUD counts down, and Crivens' hint page ("searching for the key by
    breaking things") shows.
  - Undriven, Cooper loses and the scene restarts every ~800 frames ("Exit Door Closes", then "Free Play").
- Not checked:
  - Any breaking. No part has broken, so the bg-break setup's own break, its ghoulybox load and its spawns haven't
    run. The imps in the captures may come from the scene's ghoulybox node (type 0x2A) instead.
  - The key's whole path (0xAB, the bounces, the camera, event 0xAE) hasn't run either.
  - Message 0x81's step records are read through `memory_at` and haven't been seen yet.

### 2026-10-20: the challenge save, and named fields for scenes 6 and 3

- The save write after a challenge award is native. `0x000F24C0` (powerup row 1: the trophy heads' award, and the
  rare book's) now saves the current game when one is named (DAT_00506C90). The steps:
  - `0x000F3230` copies the challenge bits (DAT_00506C54) into a temporary block of kind 100.
  - `0x0010E5B0` copies that block into the save record's challenge block (+0x1AC).
  - `0x0010DC20` reads the record's +0x184 word; when it is set, `0x0010E2C0` clears it.
  - `0x0010DA80` queues the write. A failed write calls `0x000F32F0`, which shows the app error
    "gamesystem_corruptgamemessage" through `0x00032990`. That screen is not translated, so the callback raises, as
    game select's save errors do.
- The award context carries the scene's save context (the movie file rows, the profile and the held scene order).
- `--scene6-save` (new) plays scene 6 from a real save:
  - A child `--gameselect-new-game` run names and saves a game.
  - Its progress.xsv's script becomes `aid_script_ghoulies_chapter1_scene2_1viewcam` and the file is signed again.
  - Start, A on the slot and A on Play start that scene from game select.
  - The scene 6 driver then plays the trophy heads through to scene 3. The scene 3 driver then plays to the
    door, and Cooper collects the rare book on the way.
  - It requires that the file's challenge byte (+0x270: the record, then the 8-byte progress block) goes from 0 to
    6, and that the file's signature still verifies. The 6 is challenge 1 (the trophy heads, `0x00082440`) plus
    challenge 2 (the book: its parameters' +0x3C0, awarded by `0x0010A6D0`).
  - It then runs `--scene6-save-reload` (new) on that save. The save's script is set to
    `aid_script_ghoulies_chapter1_scene3_1playcam` and signed again, and the challenge bytes are left as written.
    Game select loads it, and `0x000F3020` must bring both bits back into DAT_00506C54. Scene 3 must then reach Free
    Play with no rare book: `0x00109FC0` doesn't start a powerup kind 1 emitter whose challenge bit is set.
- The rare book's emitters used to read a zeroed 0x20-byte `challenge_bits` copy in SceneActorServices that nothing
  filled, so a won book would come back after any load. `0x00109FC0` and the dialogue measurement's challenge
  view now read the profile block (`*(DAT_00506C54+8)`), as the trophy heads already did. A consequence: in one
  session, once the book is collected, scene 3 won't spawn it again.
- The trophy heads' win (challenge 1) doesn't hide the book. The book has its own challenge, 2. Plain `--scene6`
  still shows the book in scene 3: the log reports 1 rare book at Free Play, with the profile challenge byte at 2.
- The retail field rule: the objects the scene 6 and scene 3 work added now read their own fields through named
  descriptors. They are the trophy heads, the hold, walk-target, pick-up, lift and throw states, the skeleton
  strategy, the bob, the large projectile, the ghouly cutscene setup and the bone debris. The shared body-state,
  strategy and factory-parameter headers are named too. See `docs/RETAIL_LAYOUTS.md` for the table, the evidence and
  what stays literal (the shared body, mind, actor and weapon-base offsets).
- Checked: `--scene6` and `--scene3` pass, and all 90 of their captures are byte-identical to the captures from
  before the change, both after the naming and with the save write in place.
- Checked (the reload): `--scene6-save` passes with its reload. `--scene3` still finds 1 rare book at Free Play and
  collects it. The reload's undriven scene 3 captures differ from `--scene6`'s only in the skeletons' idle poses.
  The camera there never frames the book's spot by the sofa, so the book's absence is shown by the emitter count
  (1, then 0), not by a picture.
- Not checked:
  - The save write's failure path (the app error screen).
  - A reload at the save's own script. The reload test points the script at scene 3. A real save made after these
    awards still names an earlier scene (`aid_script_ghoulies_chapter1_scene0_1book` from the new game, then the
    test's scene 6), because the script record is written by scene code that isn't translated yet.

### 2026-10-20: scene 3 to its end: the throw, the rare book and the door

`--scene3` driven now plays the snooker room to its end. Cooper throws the trophy 90 frames after lifting it (right
stick, the mind's attack gesture). He then walks onto the scene's rare book, which is collected on touch at frame 5962.
Once Free Play's section holds at its end (type 0xF, DAT_00512518), he walks into the exit volume. The script moves on
to `aid_script_ghoulies_chapter1_scene4_1book` ("Partner and the False Fireplace") at frame 6384. The test now requires
the book and the exit when driven.

- The throw, type 0x8B (getter `0x000A37F0`, vtable `0x00505C40`, `entities/body_throw_state.cpp`):
  - Slots: sizes `0x000A37D0`, ctor `0x000A3850`, dtor `0x000A3910`, tick `0x000A3990`, message `0x000A3D20`.
  - Aim: turn `0x000A3D50`, the right stick `0x000A3E80`, spread `0x000A3FE0`, leading the target `0x000A4100` and
    `0x000413F0`.
  - The hit state's turn is now the shared `0x0001D5B0`.
- The trophy (0xA9) in flight, `projectile_weapon.cpp`:
  - State 2: launch `0x000CE020`, the arc `0x000CDAD0` / `0x000CE380` / `0x000CE290`, spin `0x000CE510`, bounce
    `0x000CE1F0` / `0x000CE150`, `0x001058E0`, and the wide list leave `0x0003E900`.
  - State 7: `0x000CE7E0`.
  - State 4, the shatter: `0x000CEBD0`, `0x000CEC70` (a reach growing over 0.6 s that hits each actor once through
    `0x00141A30`) and its effect callouts `0x000CEB10`.
  - The contact test is `0x000CE5D0`, and the thrower's hand empties through `0x0001B680`.
  - Prop and broadcast payloads gained 0x19 (the projectile hit), 0x8F (breakage) and 0x86 (powerup award).
- The rare book is the FX anim emitter (type 0xD, key 0x6C, at -111.5, 4.7, -82) holding powerup kind 1.
  - Retail collects it by touch in `0x0010A590` / `0x0010A6D0`. It never was collected here, for two reasons: the
    emitters' DAT_00511B40 player list was never filled, and the award (`0x00123E70`) was an unbound placeholder.
  - The scene now refreshes that list each frame from the case list (`refresh_emitter_players`) and calls the
    translated award. The intro-notice readiness test (`0x0013FF70`) reads the player's body bits through
    `case_body_bits`, which was unbound.
  - The collection burst's afterimage (type 0x10) with its glow set now draws through the glow capture (`0x00062220`)
    and the new stamp `0x00062490` (`graphics/glow_pass.cpp`).
- Shown (`cache/s3w_captures/`; looked at):
  - `scene3_throw_20` / `_40` / `_80`: Cooper's throw from the cabinet, with smoke and debris by the cabinet as the
    trophy leaves. The trophy's flight itself is off-camera; the log has it arcing to x 62 with three bounces and
    stopping there when it shatters.
  - `scene3_book` / `scene3_book_burst`: the book gone from by the sofa, and a blue sparkle burst where it lay.
  - `scene3_exit_360` / `_420`: Cooper walking through the open arched door.
  - Heard in the log, not listened to: the BONUSBOOK ditty cue.
- Not shown, heard or checked:
  - The trophy's shatter on camera.
  - Any "first bonus book" notice page: the run sees no second page before the exit.
  - The next scene past its first frame.
  - Nothing was compared with xemu.
- Verified: Release build in `cache/build_s3` with `cache/s3w_regress.sh`. All nine modes exit 0: default, both
  `--player-death`, `--player-fight`, `--player-opening`, `--player-controls`, `--scene6`, `--scene3` and headless.

### 2026-10-20: scene 3, Cooper lifts the trophy

With both skeletons down, the `--scene3` driver takes Cooper to the trophy (`objTag_Weapon_Trophy`, tag 224, in the
glass cabinet at -118, 7.2, -42). He goes round the snooker table by waypoints (up to z 60, across to x -115, down to
the cabinet), then presses A about it. Cooper lifts the trophy at frame 5646, and its event 0xAD (frame 5658) ends
Free Play's last condition. The test now requires both when driven.

- Type 0x7D, the lift (getter `0x0009E450`, vtable `0x00505A80`), beside 0x7C in `entities/body_pickup_state.cpp`:
  - Slots: sizes `0x0009E430`, ctor `0x0009E4B0` (latches `0x0001FC40`), dtor `0x0009E560` (`0x0001FD20`, the
    0x7C-style release, or `0x00146280`'s drop {0x97, 0}), tick `0x0009E6B0` and message `0x0009E9F0`.
  - The tick faces the weapon's pick-up point (`0x0009EA70`). When the animation crosses state +0x18, it asks the
    weapon with {0xA0, reply, actor, hold offset, lift time}; a weapon that agrees lifts itself, and its {0x5C}
    attaches it (`0x0001B190`). A null tick input word reads address zero, as in 0x73.
  - The 0x7C context gains the weapon message (replies come back), the latches, the actor's bytes and address zero.
- The trophy (0xA9): state 0xF's update `0x000CE810` (`lift_runtime_large_projectile`) eases it on an arc into the
  holder's hands and sends {0x5C}. Message 0x9D's `0x000CE660` stores its root pose (`+0xE8C` / `+0xE98`).
  State 3 now sets the holder's body +0x4C / +0x14 through a weapon service that keeps the native secondary and
  receiver views with the words (`hold_secondary`); before, it read a body address that does not resolve.
- HUD: the total-damage counter `0x0012DAF0` is translated (icon, digits through `0x0012F000`, slide by
  `0x00131F00`). DAT_00512278 stays the never-written zero, as elsewhere.
- Shown (`cache/s3w_captures/`; looked at): `scene3_trophy_pickup.bmp`, Cooper with the golden trophy over his
  head by the cabinet, and frames 5700 and 6000 holding it, with the counter icon and "1" at the bottom right.
- Not shown, heard or checked: the throw and its flight; the walk out through the door; how the lift's timing and
  arc compare with retail; nothing was compared with xemu.
- Verified: Release build in `cache/build_s3` with `cache/s3w_regress.sh`. All nine modes exit 0 (default, both
  `--player-death`, `--player-fight`, `--player-opening`, `--player-controls`, `--scene6`, `--scene3`, headless),
  before the final test assertion was added; `--scene3` was re-run with it.

### 2026-10-20: scene 3, the skeletons fall apart into bones

Driven at the skeletons, `--scene3` now plays the whole fight: the first skeleton dies at about frame 1725, the second
later, and each one bursts into its bones, which bounce on the floor and settle. The scene then holds in Free Play
with both skeletons gone and the trophy still on its board, until the run ends at frame 5400 (`PASS`).

- Type 0x12, the bone debris (getter `0x00067E60`, vtable `0x00504A18`, parent 2), in `entities/bone_debris_actor.cpp`.
  `0x00134C10` creates it when a knockdown hit kills the skeleton.
  - Slots: sizes `0x00067E40` (0xAAC / 0x55C), ctor `0x00067EB0`, dtor `0x00068010`, message `0x00068700`, update
    `0x00068060` and draw `0x000686A0` (fade draw `0x00068610`); +0x18 keeps the base `0x0003C830`.
  - The ctor copies the dead actor through `0x0013AE30`. Then `0x00068850` sets up the joint rows
    (`0x000400D0`) and an effect node of its own at +0x934 (`0x000ED1C0`, the explodeall stream `0x000ED990`, debris
    mode `0x000ED6E0`). `0x00068970` gives each group of the model's part header a particle (or one per joint),
    with an optional push away from a source point and an optional carried effect (`0x00053170`).
  - The update ticks that node (`0x000EF2D0`) and copies each particle's pose into the joints' +0x288 rows. The
    first floor hit (DAT_00506928 > 0.75) plays the 0x16 callout. Once every particle rests, the pieces stay for
    the attributes' +0x194 time (3 s here), then `0x0013AF80` leaves a two-second after-image and `0x00112CC0`
    takes the copy away.
  - Message 0x7C for the dead actor moves its playing sounds to the copy: `0x00041D90` with `0x0012A440`
    (`move_actor_sound`, new in `sound_runtime.cpp`).
  - Not translated (they raise): the "aid_" sound branch at parameter +0x558 (`0x00140560`, `0x000F8540`, and
    `0x000F8640` / `0x000F85D0` / `0x00035C00` later), and `0x0012A260` for parameter +0x4C4. The death
    effect leaves both zero.
- Scene wiring: the factory class and native instances in `scene_actor_factory_owner.cpp`. In
  `scene_actor_services.cpp`: the update and message dispatch, the draw, the visibility (with the class's +0x288
  rows) and the +0x38 slot. The new `tick_owned_effect` runs `0x000EF2D0` with the scene's particle simulation.
  `update_first_actors` now sets the frame's effect scratch, since a new actor's first update can tick its effect.
- Fixes on the way:
  - `0x000400D0` (`initialize_actor_model_matrices`) required a type-16 directory block. Retail tests the part
    header word (model CPU +0x10 -> +0x10). The skeleton has the header and an `ndMtxArray` (76 rows) but no such
    block, so the copy got no rows.
  - `0x000ECA20` (`probe_actor_effect_world`) passed its probe points and kind/mask the wrong way round. Retail's
    EBX (the probes' kept point) is the particle's next position and the first stack word its current one; EDX
    (0xC) is the kind and ECX (0) the mask. Before, no effect particle ever hit the room, so the bones fell
    through the floor.
  - The after-image ctor (`0x000690F0`) raised whenever parameter +0x350 was set. Retail calls `0x000F8540` only
    when that record's +8 reads "aid_".
- Shown (`cache/s3w_captures/`; looked at):
  - Frame 1758: the first skeleton bursting apart: skull, ribs, arm bones and a hand in the air.
  - Frames 1878 and 1998: its bones lying on the floor while Cooper fights the second skeleton.
  - Frame 2178: the bones gone.
- Heard (log only, not listened to): the fight's hits, punches and skeleton calls; Crivens' satisfied line; Cooper's
  "yeah".
- Not shown, heard or checked:
  - The bone-landing callout (0x16) has not appeared in the log since the probe fix.
  - The after-image fade of the bones was not seen in a capture (the gap between captures is too wide).
  - The trophy pickup, the conditions firing and the scene's end.
  - Whether the bones' bounce, rest time and fade match retail; nothing was compared with xemu.
- Verified: Release build in `cache/build_s3` with `cache/s3w_regress.sh`. All pass: default, `--player-death` with
  and without `--retail-step`, `--player-fight` (the chair, whose matrix setup goes through the corrected
  `0x000400D0`), `--player-opening`, `--player-controls`, `--scene6`, `--scene3` and the 900-frame headless run. Not
  run: the Debug build, ctest and the other suites.

### 2026-10-19: scene 3, the skeletons walk and fight

Driven at the skeletons, `--scene3` now plays the fight: the woken skeletons walk to Cooper, attack, knock him about
and take hits, until one dies at about frame 1700. The run then stops at factory type 0x12 (the bone debris; see
Frontier).

- Type 0x8F, the walk toward the mind's target (getter `0x000A6220`, vtable `0x00505CC0`), in
  `entities/body_walk_target_state.cpp`:
  - Slots: sizes `0x00091A90`, ctor `0x000A6380` (rows `0x00091820`), dtor `0x000A64D0`, tick `0x000A6520` and
    +0x1C `0x00091F70`. The message is 0x8E's `0x000A5A40`; +0x14 and +0x18 are the empty `0x001628B0`.
  - Tick helpers: `0x00019840` (the stick, through actor +0x960), `0x000A67C0` (aim; distance `0x000A6950`),
    `0x000A6280` (row from band `0x00091A00` and side `0x000A61B0`/`0x000A6140`), `0x000A62D0` (retime; rate
    `0x000918C0`) and `0x000A6640` (arrival).
  - The angle helpers `0x001009A0`, `0x00100A80`/`0x00100B80`, `0x00092120` and `0x000921E0` are there too.
    `0x001013A0` (`step_runtime_angle_toward`) moved out of `body_hit_state.cpp`'s anonymous namespace so both use it.
  - `RuntimeBodyStateContext` gains the mind view and an actor-position lookup (`0x000412A0` of the mind's target).
- Type 0x8A, a timed hold before the next goal action (getter `0x000A34C0`, vtable `0x00505C20`), in
  `entities/body_hold_state.cpp`: sizes `0x000A34A0`, ctor `0x000A3520`, dtor `0x000A35E0` and tick `0x000A3670`
  (`0x00134720`, `0x001346B0`, the 0x75 notice through `0x00122C70`, then `0x00134060`). The message and +0x14/+0x18
  slots are empty; +0x1C is `0x0002B620`.
- Type 0x59's message slot is the empty `0x001628B0` (its getter `0x0007FCE0` stores it at `0x005054C4`).
- Shown (`cache/s3w_captures/`; looked at):
  - Frame 1278: the left skeleton in mid-stride, stepping off the dais toward Cooper.
  - Frames 1338 to 1698: both skeletons at Cooper, with hit flashes, the skeletons' health hearts and Cooper's
    health HUD (50, then 47). A stunned skeleton has the pink dizzy bats over its head.
- Heard (log only, not listened to): skeleton footsteps on stone and wood, the build-up call, the dizzy bats and the
  heartbeat.
- Not shown, heard or checked:
  - A skeleton's death and its bones (type 0x12).
  - The trophy pickup and throw, the conditions firing and the scene's end.
  - Whether the skeletons' walk speed, turning and timing match retail; nothing was compared with xemu.
- Verified: Release build in `cache/build_s3`. Of the regression modes (`cache/s3w_regress.sh`), default and
  `--player-death` with and without `--retail-step` pass. The run was stopped during `--player-fight`, so
  `--player-fight`, `--player-opening`, `--player-controls`, `--scene6` and the 900-frame headless run were not run
  on this build. `--scene3` stops at its new frontier.

### 2026-10-19: scene 3, the snooker room's walk-in

Scene 3 (`aid_script_ghoulies_chapter1_scene3_1playcam`, "Player Enters Room") now starts and plays its walk-in and
"Free Play" natively. Its transition is row 1, which shows no chapter title. Booted straight in (`--scene3`), the
transition ends at frame 292. Cooper's walk-in cutscene then plays, followed by the door cutscene and Crivens' two pages
("Ah, the door ahead is open, Sir…", then the camera-trigger hint). Cooper spawns at the player marker at the walk-in's
end (frame 451). "Free Play" starts at frame 979 and holds on its conditions, with the player camera "cameron" bound.
Driven at the skeletons, Cooper wakes them (frame 1240: strategy 0x401, then 0x3F9) and the run stops at factory
type 0x8F.

- Scene start. The markers need four new classes:
  - Type 0x51, `entityStrategySkeleton` (`entities/skeleton_strategy.cpp`):
    - Slots: sizes `0x000A9FC0`; the shared constructor `0x000CFFD0`; message `0x000BFE10`; update
      `0x000BFAC0`; enter `0x000BFF80`; leave `0x000C0130`.
    - Helpers: `0x000C05C0`, `0x000C0170`, `0x000C01A0`, `0x000C02D0`, `0x000C03D0`, `0x000C0430`,
      `0x000C0500` and `0x000C0590`.
    - Its hit message uses the shared reactions `0x00139390`, `0x00139740` and `0x001397B0`, with `0x001372A0`,
      `0x001372F0`, `0x001408A0` and `0x001363E0` (`strategy_target_selection.cpp`). `0x00136AC0` (a range request)
      sits beside `0x00136A70`.
  - Type 0xA9, `entityWeaponProjectileLarge` (`entities/projectile_weapon.cpp`), the trophy:
    - Slots: sizes `0x000CD620`, constructor `0x000CD6B0`, destructor `0x000CDAA0`, message `0x000CDBE0`, enter
      `0x000CD700`, leave `0x000CD8A0` and state update `0x000CDA10`.
    - Helpers: `0x000CDF40`, `0x000CEEC0` and `0x001454B0`.
    - Not translated (they raise when reached): its flight, pickup and throw (`0x000CE020`, `0x000CE1F0`,
      `0x000CDAD0`, `0x000CEBD0`, `0x000CEC70`, `0x000CE7E0`, `0x000CE810`, `0x000CE660`, `0x0001B680`,
      `0x0003E900`) and its contact test `0x000CE5D0` (`0x00141A30`).
  - Type 0xA0, `entityWeaponBob` (`entities/bob_weapon.cpp`):
    - Slots: sizes `0x000C5B90`, constructor `0x000C5C20`, destructor `0x000C5D30`, message `0x000C5EB0`, enter
      `0x000C6100`, leave `0x000C61B0` and state update `0x000C5D50`. Its cutscene branch (story bit 8) goes through
      the scene's cutscene runtime.
    - Not translated: `0x000C6250`, `0x000C62C0`, `0x000C63A0`, `0x0012ADC0` and the contact test `0x000C61D0`.
  - Type 0x25, `entityAvatarGhoulySetupCutscene` (`entities/ghouly_cutscene_setup.cpp`), the walk-in ghoulyboxes:
    - Slots: sizes `0x0006B0F0`, constructor `0x0006B150`, destructor `0x0006B350`, message `0x0006B4C0` and update
      `0x0006B8B0`.
    - Helpers: `0x0006B390`, `0x0006B9C0`, and the cutscene event callback `0x0006BA60` (event 0x13, registered in
      the cutscene runtime's `event_callbacks`).
    - `0x00048EB0` (`camera_rebind_view`) is new in `entity_camera.cpp`.
  - The two new weapons keep the weapon base's update, draw, visibility and +0x48 slots, like the chair.
- Fixes on the way:
  - `set_runtime_player_goal_volume` (`0x00028500`) stored actor +0x98C as the address of a host occupancy record,
    which had none. Records in the `DAT_00512344` pool now map to addresses inside the pool's block.
  - Bob's message 0x23 tail is `0x0003EF40` (`clear_actor_model_anchors`).
  - The 0x8E-style contact tests of the new weapons raise when they run, not when they are fetched.
- The walk-in:
  - Script type 0x53 was a host shortcut that set DAT_00512470 and held the script forever. It is now
    translated from `0x001274B0` (`play_script_walk_in_cutscene`). The cutscene starts at the first kind 0xC marker,
    turned by pi (`0x00100DF0`, `add_wrapped_angle`); `0x0004EF20`/`0x0004EF80` place it.
  - The relic candle's scene control (`aid_objparams_ghoulies_scenecontrol_candle`) is created when the cutscene has a
    `reliccandle` actor. This walk-in has none, so the control's class is not needed yet.
  - At the end the player marker runs again (`0x00127FF0`, `SceneActorServices::restart_marker_player`). That
    handler spawned no one, because DAT_005462FC stayed 1. Retail's scene start (`0x0011CBD0`) clears it last; the
    port's `start_scene` now does too. `0x0012A1E0` and `0x00049BA0`, before it, are still not translated.
  - The marker handler's `0x00123B00` binding is `script_end_gate_set`.
- Free Play:
  - `0x00125450` lacked type 0x6A. It now registers condition 15 (GhoulyTriggered) with the tag, node+0x48 and the
    float at node+0x4C. Before, the section ran straight to its end and the scene ended two frames in.
  - Condition 15's update `0x00133770` is translated (`script_tick_ghouly_signal_condition`).
  - Move action type 0x59 (getter `0x0007FCE0`) is installed: constructor `0x0007FD30`, destructor `0x0009F150`
    (the body's speed limit), +0x10 `0x0007FD70` (as 0x5B) and +0x14 `0x0007FDD0` (`0x0002AE90` with 1, 1).
- Tests: `--scene3` (new). `--scene6` now continues into scene 3 and plays its walk-in and Free Play with the
  sticks centred. `pass_chapter_title` accepts a transition without a title.
- Shown (`cache/scene3_*.bmp`, `scene3_page1.bmp`; looked at):
  - The walk-in in the snooker room: sofas, the snooker table under its lamp, the skeleton portraits.
  - The door cutscene's two skeletons (yawning, hands on hips) by the empty trophy board.
  - Crivens' pages.
  - Cooper from behind in the player camera, walking to the skeletons by the far wall.
- Heard (log only, not listened to): Cooper's breath and carpet, stone and wood footsteps; the skeletons' yawn,
  attack, question, hands-on-hips, fold-arms and look-at-nails calls; the door creak, vine, dirt mound and Crivens'
  door; Crivens' dialogue; the page A ditty (SUPERSCARYSTUF_RIGHTBUTTON); the walkabout stream; skeleton footsteps
  once they wake.
- Not shown, heard or checked:
  - The trophy on its board (not seen in the captures; whether it should be is not checked).
  - Any HUD; no fight has touched health yet.
  - The skeletons walking, attacking or throwing; Cooper hitting them.
  - The trophy pickup and the conditions firing; the scene's end.
  - Anything in xemu.
  - The class-6 marked actor with strategy state 0x40C, which appears with Cooper, is not identified.
- Verified: Release build. The `cache/regress.sh` modes pass: default, `--player-death` with and without `--retail-step`,
  `--player-fight`, `--player-opening` and `--player-controls`. The 900-frame headless run exits 0. `--scene6` passes
  through scene 3's Free Play. `--scene3` fails at its frontier. Not run: the Debug build, ctest and the other
  suites.

### 2026-10-19: scene 6, the trophy heads and their Super Scary Shock

Scene 6 (`aid_script_ghoulies_chapter1_scene2_1viewcam`, "Trophy Heads Super Scary Shock") now plays to its end
natively. The transition's chapter title moves on after A, the leaving page turns, the heads' cutscene plays, the
heads attack, and each attack starts a button challenge. When the cutscene ends, the script moves on to
`aid_script_ghoulies_chapter1_scene3_1playcam` ("Player Enters Room").

- Factory names: the name for type t is at 0x0030D418 + t * 0xC, one word before the getter, not at +8 of the
  0x0030D41C rows. So type 0x92 is entitySceneControlChapter1Scene2 (the task note's "Chapter1Scene8" was the next
  row's name), and type 0x98 is entitySceneControlSuperScary.
- Type 0x92, the trophy heads (`scene/trophy_heads_control.cpp`): sizes `0x000816A0`, constructor `0x00081710`,
  destructor `0x00081AD0`, message `0x00081B00`, tick `0x00081890`, and state machine `0x00081CE0`. Its draw slot
  is the empty `0x001628B0`.
  - Head choice: `0x000821A0`, `0x000820D0`, `0x00082440` (the head's kind-0xD event in the +0x104 events resource)
    and `0x00082170`.
  - Attack: `0x00082370` with `0x000873A0`, which builds the 0x1D8-byte type-0x98 parameters with the
    "superscary" aidlist.
  - Clip helpers: `0x0004F010`, `0x0004E7D0` (a clip actor's slot-1 animation), `0x0003B310` and `0x0003D670` (the
    actor's bound size). `0x0003D670` writes `Actor::model_extent` (+0x1D0), a new field.
  - State 2 spawns Cooper with `0x00134920`. State 4, which runs near the cutscene's end, awards powerup row 1
    (`0x00123E70`).
- Type 0x98, the Super Scary challenge (`scene/super_scary_control.cpp`):
  - Slots: sizes `0x000842D0`, constructor `0x00084340`, destructor `0x000846E0`, message `0x00084930`, tick
    `0x00084960`, and draw `0x00085380`.
  - States: `0x000856E0`, with cases decoded from the raw bytes because Ghidra drops the EAX events argument and
    the per-mode animation rows.
  - Helpers: `0x00086BC0`, `0x000874F0`, `0x00087170`/`0x00087080` (the button row and its shuffle),
    `0x00086C90` (message {0x8E, step} to the scene controls), `0x000854E0` (the clip at an actor),
    `0x00086860` (slot-0 animation), `0x00084E50` and `0x00086CE0` (the "superscaryshock" camera),
    `0x00086E80` (Cooper faces the head), `0x00086FD0` (the ditty streams), `0x00086990` (the shock strings),
    `0x00086A40`/`0x00086B30` (right and wrong buttons through `0x001083F0`), `0x00086B70`, `0x00087720`,
    `0x00085190`/`0x00085280` (button positions and their panel), `0x0006DAD0` (the bat's "doh"), and
    `0x00116140` (`dispose_runtime_scene_control`).
- 0x92 and 0x98 talk through the scene controls. `0x0004EB20`/`0x0004F050` walk DAT_0050E438 with 0x7E/0x7F, and
  that walk now reaches the native control list (it walked the never-set `g_50e438` before).
  `NativeSceneControlOwner::message` dispatches by type and carries the instance pointer that the second word
  holds in retail. The messages:
  - {0x8E, 0}: load the head's roar animation and events.
  - {0x8E, 7}: the challenge is won; Cooper is retired and the cutscene resumes.
  - {0x8E, 8}: the challenge is lost; the same, and health drops by 10.
  - 0x7F: the 0x92 control unpauses.
- Award: `0x00123E70` is translated in full in `entities/powerup_awards.cpp`. Its reached case 1 calls
  `0x000F24C0`, which sets challenge bit 1, queues "challenge_firstbonusbook" for the first win and shows the
  bonus counter. The stream row is picked by the count. The cases that need views the scene does not bind raise:
  5 (`0x000D0870`), 6, 7, 0xC (`0x001349E0`) and 0x13. `0x001318A0` (the bonus board's count) is translated in
  `0x0012D690`. `audio/stream_restart.cpp` (`0x00038880`) was in the tree but not in the build; it is now built.
- Other fixes on the way:
  - `0x00048860` releases the page texture (`0x00035C00`, `0x00036210`, `0x00035C60`).
  - Every native actor view gets the `0x0003B3C0` animation-event handler (message 7 through its vtable +0xC). The
    head's roar events had no handler.
  - The challenge's button bat (type 0x15) joins the actor updates through `bind_button_bat`, which is shared with
    game select.
  - Broadcast 0x8E has a two-word layout.
- Profile blocks: the two controls read and write the boot-owned `g_runtime_profile` blocks: the story bits
  (DAT_00506C6C) and the challenge bits (DAT_00506C54). The script actions use the same blocks. SceneActorServices
  still keeps its own `story_bits` and `story_byte` copies, which the older scene code uses. (Its `challenge_bits`
  copy was removed on 2026-10-20: the FX emitters and the dialogue measurement now read the profile block too.)
- `--scene6` (new):
  - Boot and transition: boots scene 6, presses A on the chapter title, and requires the transition to end in the
    scene. The run then plays the scene.
  - Challenges: it leaves the first challenge unanswered, so it fails (state 10), and presses the shown buttons in
    the later ones, so they are won (state 8). It presses A on Crivens' page.
  - It requires a failure, a success, the page, and the heads' cutscene resuming.
  - One run: four attacks (moose first). One failure took health from 50 to 40; three wins followed; then the
    award, state 3 (event 0xAF), and the script moved on to scene 3.
- Shown (`cache/scene6_c*_s*_*.bmp`, `scene6_page1.bmp`):
  - The head's close attack and Cooper's two recoils.
  - The four A buttons in their panel, with the bat on the current one.
  - Cooper slumped after the failure.
  - Crivens' "Gadzooks! Sir has suffered a SUPER SCARY SHOCK and lost some energy as a result!" page with the A
    prompt.
  - Cooper with his hand on his chest after a win, and the heads' cutscene between attacks.
- Heard (log only, not listened to): GTROPHYHEADS_MOOSE/MOOSE2, the shock strings, GBOY_FRIGHT, GBOY_SHIVER, the
  BUTTONPRESS ditty, eight heartbeats (the last four fast), BUTTONNODIE, GBOY_SHOCKSIGH/SHOCKEXHALE and Crivens'
  CRIVENSFRIGHT.
- Not checked:
  - Whether Cooper's pose 160 frames into a won challenge (arms loose, close to his rest pose) matches retail.
  - Whether the first attack's all-A row matches retail (count 1 gives four A buttons).
  - The "challenge_firstbonusbook" notice: it is queued, but the scene ends before it shows.
  - The bonus board count on screen.
  - The modes 2 and 3 of type 0x98 (the head-bound camera, `0x0003B500`, the floor height and the render
    toggles). They raise if reached; scene 6 uses mode 0.
  - Anything in xemu.

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
- Corrected 2026-10-19: retail swaps every second vblank (PresentationInterval 2), so its logic runs at 30 Hz
  with a 1/30 s step (`0x000FC030` counts the fields between swaps). The loop now ticks that way and presents
  the last frame again between ticks; headless and screenshot runs step 1/30 s. See `docs/NATIVE_CUTSCENES.md`.
  The opening scene's checkpoints above were taken at 1/60 s steps.
- An imp was seen staying behind the wave's door in a 240 Hz play. `--player-fight` driven at four 1/240 s steps a
  frame did not show it: all ten imps came into the hall after "Kill All", as at 60 Hz. The cause is not known.
  In both runs the imps hold still behind the door from about frame 420 (the door shuts near frame 400) until
  "Kill All". With the 60 Hz loop the user no longer sees it in play.
- Verified (with the after-image fixes): Release and Debug build; ctest 7/7; every suite passes except
  `--player-fight`, which ends at the expected type 59 in both; headless 900 (both scenes) exits 0.

### 2026-10-19: the butler and the scene's end

- Butler strategy (factory type 0x3B, `entities/butler_strategy.cpp`): sizes `0x000AB020`, constructor `0x000AB090`,
  message `0x000AB170` with the hit reaction `0x000AB2D0`, update `0x000AB0C0` with its look-around `0x000AB260`,
  enter `0x000AB1A0` ("far" for 0x401, "scared" for 0x411) and leave `0x000AB220`. `0x00129210` (live actors on
  DAT_00511B48) is translated beside the "Kill All" gate. The update's tail `0x0013D400` sends message 0x63 to the
  class-6 actor; the body message service now views its cylinder (message +4) for `0x000253B0`.
- entityStateStruggle (factory type 0x88, `entities/body_struggle_state.cpp`): sizes `0x000A2B10`, constructor
  `0x000A2B90`, destructor `0x000A2D60`, tick `0x000A2DB0` with `0x000A2F90` (turn toward the mind's target),
  `0x000A3070` (the in-band / out-of-band animations) and `0x000A2EB0` (the next key); `0x000197B0` (the heading to
  body +0x3C) is in `body_orientation.cpp`. The butler struggles while scared.
- Script actions: 0x60 (`0x00129ED0` and `0x00112CC0` on the native actors), 0x9E (story bits `0x000F29A0`,
  `0x000F29D0`, `0x000F29F0`), 0x9F (the DAT_00511B38 walk with `0x000F96F0` on both models) and 0x46 with a live
  0x7D (the player drops its weapon, `0x0013A7C0`, and `0x0012ECC0` resets the meter): the butler takes the chair.
- The scene's end: `0x00140E30` (exit, notice and camera-tune volumes) replaces the host stub that read
  `g_actor_mark` and never ran; it now runs at the head of `0x00117100` as the dump has it, with `0x00124970` and
  `0x0013D610`. Its event 0xB0 is handled in `0x00116950` (the door actor's {0xA6} pose, or the door marker's
  through `0x0011F260` / `0x0011F2A0`, into `0x00117580`'s next-scene entry pose) and sets DAT_0050E788, so
  `0x00117140` requests the next scene. In this scene the exit is region 1's box at (-208.6, 49.6) behind the far
  door.
- Transition: `0x00048260` binds an "aid_" page texture (`0x00035E90`, `0x000F86F0`); `0x0012B640` clip 2 shows the
  chapter title (`0x0004B550`, `0x0004B5A0`, the "chapterheadings" loctext, and its text pool).
- Fixed: the dialogue draw (`0x00058C30`) wraps onto line 5 of a five-line page and asks `0x0005A630` for that
  line's position before its loop test ends the page; retail reads the word after the five widths (DAT_004EE22C,
  past the DAT_004EE208 page) and drops the result. The native widths array now has that sixth word; it read past
  the array before (a Debug assert on the butler's "discard weapons" page).
- `--player-fight`: the play-on walks Cooper into any region a live PlayerEntersRegion (condition 8) names, and into
  the exit volume while a section holds at its end (type 0xF, DAT_00512518). The walk learns the camera's offset
  from Cooper's movement. The mode passes once the next scene's script starts.
- Shown: the butler's pages ("Crivens is my name…", "Let me remove that chair for now…", "Let me also give you
  this map…", "…discard weapons by pressing the B Button…", "Good luck to you, Sir!"), then Cooper without the chair
  walking into the far doorway past the butler. Heard: the butler's dialogue cues (log); not listened to.
- Verified (with the death work below): Release and Debug build; ctest 7/7 in both; the six transition suites,
  `--title`, `--title-a`, `--attract`, `--goop`, `--ui`, `--audio-preparation`, `--player-opening` and
  `--player-controls` pass (Release); `--player-fight` and `--player-death` pass in Release at both steps, and both
  pass in Debug; headless 900 (both scenes) exits 0. The driver lines Cooper up with the exit box
  before going in: at 1/30 the straight walk clipped the door frame. Nothing here was compared with xemu.

### 2026-10-19: the player's death

- Factory type 0x69 (`entities/body_player_death_state.cpp`): sizes `0x0008EEB0`, constructor `0x0008EF30`,
  destructor `0x0008F1A0`, tick `0x0008F290` (its jump-table cases decoded from the raw bytes), phases `0x0008F4D0`,
  the restart `0x0008F650` (the native `0x00134AE0`), the fail stream `0x0008F6A0` with `0x00038A10` (hold the
  lower-priority banks), and `0x0002B630` (a state timer) and `0x000F2B30` (an unlock bit). The tick's case 1 tail
  calls `0x0012C5D0`, the native `begin_scene_transition`; `0x0008F4D0` sets DAT_005493DC first, so `0x0012C2D0`
  takes row 2. The state's context is the knockdown context plus the latches, the mind and its events, the camera
  pulse, the streams, the scene dialogue and the transition.
- `--player-death`: Cooper's health goes from 50 to 0 in about 5960 frames (1/60 s) with no input; the death plays,
  the transition starts at row 2 and the scene restarts at "Free Play (Fighting Intro)".
- Shown: `player_death_5880` (health 1), `_6000` (0, standing), `_6120` (slumping), `_6240` (lying on the stairs
  by the book). Heard: XACT_SOUNDBANK_GSTREAMSDITTIES_FAILPITCHBEND starts (log); not listened to.
- Not reached: the brew offer (state 4), which needs the unlock bit 3 and enough brews.

### 2026-10-19: glyph textures

- Text glyphs were stitched from the wrong texels: in the forming frames, part of a glyph's right side showed at
  its left edge, one block row lower. The cause was the font GPU data.
  - Fonts load from loose files (`OriginalFiles/font`). The resource owner kept the whole GPU payload, but the
    texel data starts at the first GPU chunk's offset (16 bytes in).
  - So every 32x32 DXT frame (8 per glyph) was decoded one 16-byte block early.
  - Bundle textures already take the first chunk. The loose font branch in `dialogue_resource_owner.cpp` now takes
    it too, as the legacy host font loader did.
- Texture address modes: only model push-buffer draws carried D3D_g_DeferredTextureState (DAT_00181970, stage stride
  0x20 words: ADDRESSU, ADDRESSV). Dialogue quads and fixed-function draws were presented with WRAP.
  - The dialogue sprite setup (`0x0005CDC0`) sets CLAMP on stage 0.
  - The recording device now copies the deferred address modes at each draw, as it does for fog (D3D's lazy
    update). Every recorded draw carries them: indexed, inline, quad and fixed.
- Game select panels (`0x00061360`) draw their rounded caps as one line per 640x480 row, with width 1
  (DAT_00425168). The presenter drew a width-1 line as a D3D11 line, one output pixel thick, so above 640x480 the
  rows left gaps (stripes across the green frames at 1920x1080). Every line of positive width is now a quad
  measured in retail screen units. Checked: the 1080p game select frames are solid, and 640x480 is unchanged.
- Shown: the chapter title's glyph frames decode whole (dumped with a probe, since removed), and the stray sliver
  between "Chapter" and "One" is gone. Not compared with xemu.

### 2026-10-19: the chapter title at the scene's end

- Leaving the opening scene runs the book transition with clip 2. When the page turn finishes, the book handler's
  tick (`0x0012B370`) raised on its chapter title. It now follows the dump:
  - `0x0012B810` adds two sticky, centred text records (`0x0005F5C0`): the chapter number at 10% of the screen
    height (scale 1.2) and the name at 60% (scale 0.9).
  - It creates the "Chapter splash screen" input (`0x00107D60`, `0x00109340`) and the "transitionMenu_basic"
    prompts (`0x00141DA0` with menutext, anchor (width / 2, DAT_0050974C - 16)). It attaches their bat
    (`0x00142860`) and sets the widget's sound to DAT_004312E8 (XACT_SOUNDBANK_GGRIMREAPER_DEATHBELLFRONTEND).
  - Each tick refreshes the input (`0x00109470`) and updates the prompts (`0x00142460`). When they accept, the
    tick pops and releases the input, sets the widget to mode 3 (`0x00142C20`), ends the text records
    (`0x0005F970`) and moves to phase 2.
  - Phases 2 and 3 keep updating the prompts with no device. The draw slot (`0x0012B560`) draws them
    (`0x00142720`, immediate). The end slot (`0x0012B5A0`) destroys them (`0x00142070`) and releases the input by
    its kind (`0x00107DD0`, `0x00107960`, `0x00107B60`).
- The transition services now carry the scene's widget, input and draw contexts, the frame (DAT_004DEA58) and
  DAT_0050974C.
- Booting straight into a scene (`gbtg --scene 6`) takes the same row (3, clip 2, from the target scene's directory
  row) and shows the same title. Its text and prompts stayed invisible there. Retail's text-record draw reads the
  frame globals (DAT_005096C0, DAT_004312C4, DAT_004312DC), but the native dialogue context copies them only in
  `tick_scene_dialogue`, which does not run before the scene starts. So the step stayed 0. `script_pump`
  (`0x0011E4C0`) now refreshes the copies each frame (`sync_scene_dialogue_clock`).
- Shown: `player_chapter_title.bmp`, "Chapter One" and "The Rescue" over the sketch page, with the A prompt and its
  bat. `--player-fight` now requires the title and that A moves the transition to phase 2.
- Next blocker: once the next scene (chapter1_scene2_1viewcam, "Trophy Heads Super Scary Shock") has loaded, its
  scene control, factory type 0x92 entitySceneControlChapter1Scene8 (getter `0x000816C0`), is not translated.
  It is the trophy-heads controller: sizes `0x000816A0` (0x188 / 0x108), constructor `0x00081710`, tick
  `0x00081890`, destructor `0x00081AD0` and message `0x00081B00`, with its state machine `0x00081CE0`. Until it
  exists, the transition's leaving page turn (phase 3) cannot run.

### 2026-10-19: actor collision and the challenge-complete ditty

- Cooper walked through the imps, the butler and the chair. `0x000756C0` reads the count of its selected shapes
  from body +0x898. Native keeps that count in `RuntimeBodyShapeStorage::selected_count` (which `0x00075270`
  fills), but the loop read the raw body word, which is never written. So Cooper had no push shapes and was never
  pushed. The loop now reads the native count.
- With the push live, other actors' shape probes (`0x0003C770`, DAT_00511B04) reach a flying imp's knockdown state
  as message 0xE. The knockdown contact pool (state +0xCC) is now native: record `0x0009BB60`, forget `0x0009BEA0`
  (message 0x7C), notify `0x0009BD40` (message 0x45 to each contact; a contact that replies halves the flyer's
  horizontal velocity, DAT_0032B0F4 = 0.5), and free in `0x0009CA80`. The knockdown constructor's `0x00027BE0` is
  wired too. The body message 0x45 now binds its sender's views (word 3), as 0xF does.
- On the probe list during the fight are classes 4 (Cooper), 23 (the imps), 5 (the butler) and 137 (the chair).
  These classes come from the tag table: Cooper is 4. Cooper's closest approach to an imp's centre went from 1.38
  to 2.68.
- The `--player-fight` driver can no longer reach the chair's centre: the chair's shape stops Cooper at about 7.9.
  It now closes to 10, then circles the chair between A presses until a grab point (`0x00147430`) is in reach.
- `0x000D0B40` played EVENTBEGIN for a finished challenge. The dump passes 0x00303C60 (GSTREAMSDITTIES_EVENTCOMPLETE,
  id 10) when the last step finishes and 0x00303AE0 (GSTREAMSDITTIES_EVENTCOMPLETEHALF, id 0xB) otherwise. The run
  now plays EVENTCOMPLETE as the Kill All door opens (log); it was not listened to.
- Not checked: how solid retail actors feel (only the logs and the closest-approach numbers were compared), and the
  push between two imps.

### 2026-10-19: the restart after a death, and dialogue kerning

- The restart froze on its first frame: the restarted scene's native start threw inside 0x7D, player mind 0x34,
  strategy 0x4F message 0x4D, which builds factory type 0x70. `script_queue_scene_start` swallowed the error and
  left `actor_start` at -1, so the script's 0x6F player wait (node 0x254) never passed.
- Type 0x70, entityStateGrabbed (`entities/body_grabbed_state.cpp`, getter `0x000942F0`, vtable `0x005058E0`):
  sizes `0x000942D0`, constructor `0x00094350`, destructor `0x00094450` and tick `0x00094530`. Its +0xC and +0x14/+0x18
  slots are the empty `0x000D4179`, and +0x1C is `0x0002B620`. The constructor's parameters +0x10 call
  `0x00027BE0` (`hold_runtime_body_latches` in `body_timer_requests.cpp`). That function sends {3} to body +0x7F0, or
  creates that timer from the "aid_" name at +0x7F4. Once (+0x7E4), it raises all six latches through `0x0001F3F0`.
  Then it counts +0x7E5 and copies +0x7EC to +0x7E8.
- `--player-death` now plays on after the restart: the script leaves node 0x254 (to 0x31C) and the imps attack
  again. The test requires that the script leaves the node, and saves `player_death_restart.bmp`. Shown: the follow
  camera in the hall behind Cooper, with the imps around him. Cooper restarts at health 1, not at the 50 he entered
  with, so with no input he dies again within seconds. The source of that 1 was not traced, and it was not compared
  with xemu.
- The camera tune volumes (`0x00140E30`, mask 0x10) never match in this scene, so they do not cause the exit
  camera. In the `--player-fight` captures the camera moves in and turns toward the butler as Cooper goes out.
  That view was not compared with retail.
- Dialogue kerning: `0x0005F210` counts the two kerning tables (DAT_005043BC/C0, from PTR_DAT_00425160) into
  `text_tables`. However, the `RuntimeTextMetrics` kerning that `0x0005F250` reads was never given them, so no pair
  applied. With them wired in `SceneActorServices`, the idle page no longer shows the gaps in "po p", "hel p",
  "f rom" and "f or". This was checked in the butler's page captures.

### 2026-10-19: the opening at retail's 30 Hz step

- Step consumers checked against the dumps: every dump that caps the step at DAT_0032B150 (1/60) is
  translated with the cap (`0x0011E7F0` for `0x00115BB0`, which is untranslated, so the cap has no effect
  yet; `0x00044A90` the background graph push; `0x0012BCE0` the transition). Everything else multiplies
  DAT_005096C0 * DAT_004312C4 or counts down by it, including opcode 3's delay (`0x00133170`), so it is
  step-independent.
- Fixed: `clock_pad` (`0x000FC030`) left the 1/300 s accumulator DAT_005096C8 holding its whole ticks, so
  DAT_005096C4 counted up forever instead of giving each step's ticks. Only the inspector reads it.
- Not fixed, host copy: the present gate in `gfx_d3d11.cpp` / `gfx_null.cpp` (`0x00032AB0`) has its own
  `dt = 1/60` for DAT_005096C0. Its fade (DAT_004DE9AC) is never installed, so nothing runs it.
- Headless captures at 1/30 s (frame 90, 150, 450, 900) against `cache/player_ref/` at the same time:
  90 = ref 180 (1672 pixels differ), 450 = ref 900 (the tutorial page, 3883 pixels, the corner bats'
  pose), 150 = ref 300 differs as it has since fix 1 (Cooper in the scared pose). All exit 0.
- `--retail-step` runs: `--player-opening` and `--player-controls` pass. Cooper's run speed at 30 Hz is
  the same as at 60/144/240. In `--player-fight` the first fight passes "Kill All" (frame 1512, 50 s;
  2600 frames, 43 s at 1/60), the re-hit and HUD checks pass, and the chair's section moves on through
  event 0xAD: pickup 2.53 s, event 3.17 s, section ends 4.70 s (1/60: 2.52, 3.17, 4.68 s). The walk,
  chair and held-chair captures match the 1/60 ones. In the intro capture the camera is on an imp that
  faces it, where at 1/60 the imp faces away. This is likely the same random spread as the wave below.
- The driver had tick-counted waits (1 s before walking to the chair, the A cadence, the walk and intro
  captures). At 1/30 s these made the pickup late enough for the 5 s TimeOut to win. They now count time.
- Play-on at 1/30: the second wave's first imp spawns at (-216.1, 53.8) and stands there, in walk action
  0x15C and strategy 0x3FB, for 4 s until its mind's +0x8C timer runs out and re-plans. By then the
  door has shut. It stops against x = -195.244, behind the door, for the rest of the run. "Kill All"
  never passes, so the run never reaches type 59. At 1/60 every imp in the wave walks in at once.
  Spawn times (one every 0.3 s from 1.8 s) and walk speeds (27-37 units/s) are the same at both steps;
  only the spawn spots differ, presumably because the random sequence is consumed per tick. This is
  probably the imp seen behind the door in the 240 Hz play. Whether retail can strand an imp the
  same way, and what blocks it at its spawn spot, is not known. Not compared with xemu.
- The play-on used to return 0 when it gave up with the opening script still running. It now fails.
- Verified: Release build; ctest; `--player-opening`, `--player-controls` pass at both steps;
  `--player-fight` ended at type 59 at 1/60 and at the stranded imp at 1/30 (later that day both steps play the
  scene to its end; the 1/30 wave did not strand an imp in those runs); headless 900 (both scenes)
  exits 0. The test target did not build on `eddace6` (the light-construction check still used the removed
  `SceneLocalLightView`); it now builds the 0x98-byte light records the native code reads.

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
