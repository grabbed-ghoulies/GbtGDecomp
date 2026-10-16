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

## Current state (2026-10-16)

Verified against the source and live runs (`--player-opening`,
`--player-fight`, headless captures below).

Live and native:

- Scene start, markers, activation and teardown (`--scene-spawn`,
  `--scene-start`).
- The tutorial: the native dialogue holds the script until a fresh A edge, its
  button bat hops, and the script then waits at its "(next bit)" type-0xF gate.
- The play camera (cameron's director `0x000AA250`), the actor draw passes and
  the background `0x00042C10`. Cooper and the five imps are drawn.
- Actor updates run every frame (no hold). The imps walk and idle on the hall
  floor; Cooper walks under the left stick (state `0x8E` walk-alert).
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
- **"Kill All".** All five imps die and are removed by fight frame ~5100. The gate record (opcode 1, at most 0
  alive, 1.5 s) counts down in `0x001330E0` once the count holds, and `0x00132680` fires; the butler's
  "Well done, Sir!" page takes A, the script requests the ghoulybox through its trigger (message 0xA8) and moves
  to "Event Door Intro" (fight frame ~5660). The next section runs: Cooper stands in the main hall facing the
  stairs, stable for 3600 more frames.

Not live yet: the 64x64 actor snapshot records its draws but rasterizes nothing (its texture stays empty).
See the frontier.

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
| `--player-opening` | Tutorial hold, bat hop, A edges, the "(next bit)" gate, Cooper walking (`cache/player_walk_check.bmp`), the imp fight music |
| `--player-fight` | The opening flow, then Cooper fights the imps (see Acceptance) |

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

Measured with `--player-fight` (Release). The fight passes "Kill All" and the script moves to its "Event Door
Intro" section; the scene does not end there. The rest of the scene is more play: the bot would have to walk
Cooper on (the doors event) before the script reaches its scene request.

| Path | Stop | What it needs |
|---|---|---|
| Script (after "Event Door Intro") | no stop within 3600 frames of A edges | Drive Cooper through the next section (its doors event) and follow the script to its next-scene request through `0x00117140`; the native transition should then take over |
| Present (the snapshot) | renders nothing | `0x00116530` records 39 model draws into its 64x64 texture (the viewport follows SetRenderTarget), but the texture's colour and alpha stay zero. Not traced yet; the copy and its glow do not depend on it |
| Turn (not reached) | `0x001068E0` | `0x00134720` with body +0x19E set eases toward the body's +0x184 point first; it raises there |
| Script updates (not reached) | `0x00133170` ... `0x00133900` | `0x00117100` walks the whole update table; opcodes 0 and 1 are translated, 2, 6..9 and 20..26 are empty, the others raise when a record reaches them |
| Update (not reached) | `0x00123720` | The copy's point light, only when `DAT_005120F0` or `DAT_00512104` is set |
| Message (not reached) | `0x0009BB60` | The knockdown's contact pool (state +0xCC) for message 0xE while flying |
| Present (behind it) | not reached yet | The HUD draw leaves (`0x00131F00`, `0x0012DF30`, `0x000FF0A0`) once their gates open |

## Work

1. The scene's next section: extend `--player-fight` (or a new mode) to walk Cooper through the doors event and
   follow the script to the scene end and the next scene.
2. Why the actor snapshot rasterizes nothing.
3. The HUD draw bodies as their gates open.
4. Teardown after a failure: when a body state's destructor throws during scene release, the second release
   pass destroys the half-released player again and reads its freed input (seen only after an earlier failure).

## Acceptance

- `--player-fight` drives Cooper with the sticks, defeats every imp, requires
  the script to pass "Kill All" and saves readbacks along the way
  (`cache/player_fight_N.bmp`).
- Release and Debug build; `ctest`; the six transition suites (`--scene-start
  --scene-spawn --dialogue-resources --actor-loading --ghoulybox-decoder
  --scene-models`), `--player-opening`, `--player-fight`, `--title`,
  `--title-a`, `--attract`, `--goop`, `--ui`, `--audio-preparation`. Debug
  `--actor-loading` fails at `check_boot_pixel_shaders` on the unmodified tree.
- `gbtg --headless --frames 900` exits 0, with and without `--scene
  chapter1_scene1_2playcam`.
- Screenshots at the reference frames match or are better.
- `git diff --check` is clean; no temporary probes remain.

## Reference captures (2026-10-16, before the combat work)

`cache/player_ref/`:

| Capture | Screen |
|---|---|
| `opening_180.bmp` | The hall from behind Cooper; three imps on the floor, the staircase and portraits |
| `opening_300.bmp` | Same view a little later; the imps have moved |
| `opening_900.bmp` | The native tutorial page: Jeeves' head, "I suggest Sir uses the Left Thumbstick to move and HOLDS the Right Thumbstick…", the corner imps and the A-button bat |
| `player_opening_check.bmp` | `--player-opening` at frame 720: the tutorial page |
| `player_walk_check.bmp` | `--player-opening` after 120 frames of left stick: Cooper climbing the stairs (re-taken 2026-10-16 after commit 202d952 moved the walk end slightly) |

`--player-fight` before this work stopped at frame ~60 of the fight with
"scene factory type 115 is not translated/installed".

## Checkpoints

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
