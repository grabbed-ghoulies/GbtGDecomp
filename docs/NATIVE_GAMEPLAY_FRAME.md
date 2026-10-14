# Milestone: native gameplay frame

Goal: the opening scene (`aid_script_ghoulies_chapter1_scene1_2playcam`) runs
its retail gameplay frame. Before the tutorial, Cooper and the imps stand in the
centre of the hall. After the tutorial, the script continues into its fight
stages ("(next bit)", the chair, the door, "Kill All") instead of leaving the
scene. Everything comes from translated retail functions, called in the retail
order, driven by retail state.

Read `AGENTS.md` first; every rule there applies. History is in
`docs/PLAYER_SCENE.md` (see "Opening scene tick, camera view and actor view
pass" at the end). The legacy host code that still shares the frame is listed
in `docs/LEGACY_HOST_CODE.md`.

## Retail frame order

`0x00033B50` (`frame_tick`) calls the scene state machine `0x0011E4C0`
(`script_pump`), then the scene pass `0x0011E640`, then present
(`0x00033480`, `0x00032AB0` → per view `0x0011C240`, `0x00033A90`).

- `0x0011E4C0` in run state 1 or 2 calls the **scene tick** `0x0011E7F0`
  (`scene_tick_pause_counter` in `scene/scene.cpp`). Every frame it runs the
  dialogue tick `0x00057E60`, the fired script `0x00117140` (twice), the
  control list `DAT_0050E438` (+0x10), **the actor tree `0x00123020` over
  `DAT_00511B18`**, **the `+0x934` list `0x00122FE0`**, `0x001128A0`,
  `0x0011B670`, then the script timers `0x00117100`, and more.
- `0x0011E640` activates newly created actors (`0x00123070`, `0x001231A0`,
  their one-shot first update `0x00123270`), effects, sounds and disposal.
  The port only runs these at scene start (`SceneActorServices::start_scene`).
  Ghoulybox spawns later in the script (the "Kill All" imps) need it per frame.
- `0x0011C240` per view: `0x0011F690`, `0x00121DC0`, `0x0012AB10`, then the
  actor bins: `0x00121730` (sort), `0x00121880` (visibility, vtable +0x18),
  **`0x00121900`** (the `+0x934` owners and every visible bin actor in passes
  0 and 1, plus world helpers), `0x00121D40` (pass 2), then effects, HUD,
  `0x00058150` (dialogue) and the sprite flushes.

## Current state (2026-10-14)

- The run state is correct: `script_request` stores mode 1 like every retail
  caller of `0x0011EB30`, so `0x0011E7F0` runs each frame. `frame_tick` no
  longer calls the script tick or the dialogue tick itself.
- `0x00123020` and `0x00122FE0` are translated (`scene/scene_actor_update.cpp`)
  and bound as `SceneActorServices::update_scene_actors`. **The live call is
  held** (comment in `scene_tick_pause_counter`), because the first frame
  raises at the frontier below.
- The play camera is correct before the tutorial: cameron's view node now
  mirrors body `+0x89C`/`+0x8A8` (the node is embedded at body `+0x894`).
- Each native view runs `0x00121730`, `0x00121880` and `0x00121D40`
  (`scene/scene_view_actors.cpp`), with the base visibility slot `0x0003C830`
  (`entities/actor_view_visibility.cpp`), the FX emitter slots
  `0x00109E30`/`0x00109EE0`, and the player draw `0x00028050`. The
  background's `+0x14` (`0x00042C10`) is the legacy host world draw.
- **Cooper and the imps are not drawn yet.** They are in the view bin, in
  front of the camera and pass the frustum test, but their models draw only in
  passes 0 and 1, which happen in the untranslated `0x00121900`. Pass 2
  (`0x00121D40`) draws only translucent models such as the door planes.

## Frontier

Measured with a temporary per-actor catch (removed) on the opening scene:

| Actor | Update slot | Stop |
|---|---|---|
| Imps (type 5, class 23, cinema mind 0x35) | player update `0x0002C2B0` → mind tick `0x0007CE30` (bound) | Cinema request update `0x0007C0E0`: `NativeCinemaMindSlots::request_update` is not attached. Its context needs `RuntimeCinemaMindMovement` (navigation control, escape position, attachment eligibility, mind target), `RuntimeCinemaMindSpawnMovement`, `RuntimeMindChainRequestSelection` (attachment target and chain selection) and the body vtable +0x38/+0x3C/+0x5C keys |
| Cooper (type 5, class 4, player mind 0x34) | `0x0002C2B0` → `0x0007FB10` | `NativePlayerMindSlots::tick` (`RuntimePlayerMindTick`) is not attached |
| Type 2 (class 62) | base `+0x10` `0x000420D0` (`update_actor_frame` exists) | Not bound in `State::update_actor` |
| Type 0xD (FX emitter) | `+0x10` `0x0010A2E0` (getter `0x00109DD0`) | Not bound |
| Type 0x1F (area) | base `+0x10` `0x000420D0` (getter `0x0006DBD0` overrides only +0 to +0xC) | Not bound |
| Type 0x23 (trigger) | `+0x10` `0x00069D20` (getter `0x00069620`) | Not bound |

Cameron (director `0x000AA250`, camera mind, camera body) and the background
(`0x000464D0`) update without error.

Other known gaps on this path:

- The mind target registry `DAT_00545988` (`State::targets`) is never
  populated, so any mind target (mind `+0x10`) lookup raises
  ("mind target records").
- Body actions 0x5B (`0x0007FD70`, `0x000804E0`) and 0x87 `+0x10`
  (`0x000A2930`) raise when reached.
- `0x00121900` callees: `0x000F96F0` (model node pair, translated inline in
  several places), `0x001572C0`/`0x001572B0` (shader-parameter handler swap),
  and the world helpers `0x00119AD0` (blob shadows), `0x000FE960` (light
  volumes), `0x00116460`, `0x001237E0`, `0x00115A80` and `0x0005B020` (door
  pool). Several gate on state the port does not model yet.

## Work

1. **Draw the characters.** Translate `0x00121900` for `param_3 == 0` and call
   it between `0x00121880` and `0x00121D40` in `State::draw_view_actors`.
   Translate its helpers or let them raise when their gates are open. Bind the
   background's passes 0/1 to the same host stand-in, drawn once per view.
   Check with `--screenshot` at about frame 260: Cooper and five imps in the
   centre of the hall.
2. **Lift the actor-update hold.** Bind the update slots in the table above,
   starting with the imps' `0x0007C0E0` context and Cooper's `0x0007FB10`
   context (both build from `NativePlayerInstance` and the scene services, as
   `SceneMindMessages` does for the cinema tick). Then call
   `update_scene_actors` from `scene_tick_pause_counter` and remove the hold
   comment.
3. **Run `0x0011E640` per frame** so spawned actors activate. Done
   (`SceneActorServices::run_scene_pass`; see `docs/NATIVE_AUDIO.md`).
4. **Play the fight.** With the A-driven `--player-opening` flow, continue the
   frame past "(next bit)" and check the script reaches "Freeplay (Temporary
   Weapon Explained)" when the imps are gone.

## Acceptance

- Headless screenshots of `chapter1_scene1_2playcam` before the tutorial show
  Cooper and the imps; frames during the tutorial still show the native prompt.
- `update_scene_actors` runs live with no hold.
- Release and Debug build; `ctest`; all transition suites, including
  `--player-opening` (which now also checks that the script holds at its
  "(next bit)" gate after the tutorial).
- `gbtg --headless --frames 900 --game-dir OriginalFiles` exits 0, with and
  without `--scene chapter1_scene1_2playcam`.
- `git diff --check` is clean; no temporary traces remain.

## Tips

- A temporary `try`/`catch` around the per-actor update in
  `update_scene_actors` that prints the factory type, class and error once per
  distinct message lists the whole first-level frontier in one run. Remove it
  afterwards.
- Vtables of these classes are BSS filled by getters; when Ghidra has no
  reference to a slot function, scan the XBE for the immediate (see the
  stand-alert getter `0x000A2530` found that way) and export the range with
  `range-function-force:`.

## Continuation checkpoint (2026-10-14)

The working tree now calls 0x00121900 live. Its inherited temporary door-shadow
bypass was removed. Native door dispatch 0x0005B020 reaches boot vertex row
0x13; all 24 vertex rows are assembled and created by translated 0x00065EE0,
and their binding is translated 0x00065F60. Generic inline-array recording and
presentation have been added. Door geometry 0x0005AC40/0x0005AE40 is translated
but not reached live yet.

The opening exits 1 at missing boot pixel shader creation
0x00063FB0 / XGAssembleShader. Do not describe the scene as drawing characters
or completing a clean opening run. Translate that boot loop and factory
0x00063F40 (dump now recovered), implement the pixel assembler boundary, then
resume the live view path. See the last PLAYER_SCENE.md checkpoint for tests
and shader-source coverage. The actor-update hold is still present.

## Pixel boot restored (2026-10-14)

The previous checkpoint's 0x00063FB0 stop is resolved. The twenty boot pixel
rows are created, including translated factory 0x00063F40, and door shadows
bind the actual boot handle. The opening completes 900 frames. Inspected
cache/pixel_boot_300.png shows Cooper and imps in the hall; inspected
cache/pixel_boot_900.png shows the native tutorial, including its backdrop and
corner models. The passes 0/1 drawing milestone is now visible. Actor updates
remain held; lifting that hold is still the next gameplay milestone. See the
last PLAYER_SCENE.md entry for shader coverage and validation limits.

## Actor body frame checkpoint (2026-10-14)

Wired player and cinema mind tick/request contexts, parent/player body relay,
animation and settle slots. Added native 0x0007FD70 / 0x00133E50 /
0x00140660 gravity ticking and 0x000A2930 / 0x000A2A90 stand-alert ticking.
Scene actor updates now use the scene's simulation pause, scale, delta and
0x004312E0 integration-step state. The shared mind alert uses DAT_00511B48.

A temporary live update run exposed a missing actor +0x95C body pose view.
That view is now attached, and 0x000265D0 writes its Euler result back to
body +0x72C. The camera and imp body ticks return. The next measured stop is
`scene body message service is not bound: body channel rows (+0x150) and
actor +0x50 channel bindings`, during the imp body animation frame.
The actor-update hold remains in the final tree; this is not playable movement
or combat. Later unbound request, collision and action-step slots also remain.

Release and Debug builds pass, CTest is 6/6 in each. Dialogue-resource checks
pass in both and exercise the new gravity checks; Release scene-start passes.
The inspected 900-frame capture `cache/actor_checkpoint_900.bmp` retains the
tutorial text, head, backdrop, corner models and A-button bat. No audio
acceptance or retail oracle equivalence is claimed for these new translations.

## Body channels, motion and collision checkpoint (2026-10-14)

The previous channel stop is resolved. SceneBodyMessages now aliases the
native resource-hash rows at body +0x150/+0x154 and builds animation binding
views from the controller's actual MINA slots and their +0x20 directories.
Resource banks use their native recorded row counts. No new gameplay parser
or channel sampling path was introduced.

Translated gravity-action step 0x000804E0 -> 0x0002AE90, its speed lookup
0x000259C0 and backward-motion limit 0x0002B300. This preserves the selected
root-motion branch, cyclic phase interpolation (shared DAT_00339900),
velocity branch and optional limiting/reverse gates. The native body collision
slot 0x00074670 now uses scene shapes, door rows and contact scratch. Added
the stand-alert next-key getter 0x0002B620.

Temporary Release and Debug live runs pass those stages and both exit 1 at
factory type 142 (0x8E), retail entityStateWalkAlert. Getter 0x000A55F0
uses vtable 0x00505CA0: sizes 0x000A55D0, constructor 0x000A5650,
destructor 0x000A5950, message 0x000A5A40, tick 0x000A59C0,
empty animate/settle, next-key 0x0002B620. Its read-only dumps, including
tick callees 0x000A5A60/0x000A5DC0/0x000A5EA0, are exported. Translate
that live action next. The final actor-update hold remains; no movement or
combat acceptance is claimed.

Release/Debug builds, body/dialogue-resource checks and CTest 6/6 in each
pass. New checks cover phase scaling, shared phase writes, simulation freeze,
selected-root planar displacement and backward-motion limiting. These are
regressions, not a differential oracle acceptance of 0x0002AE90.

Final Release opening checks pass tutorial hold, bat animation, fresh A edges
and the post-tutorial script gate. Scene-start/teardown passes in both builds.
The fresh 720-frame `cache/player_opening_check.bmp` readback was inspected:
tutorial text, backdrop, head, corner models and A-button bat are present.
The redundant 1920x1080 900-frame capture was stopped before completion;
it is not a successful-run acceptance result.

## Walk-alert, animation events and floor collision checkpoint (2026-10-14)

Measured with a temporary live call of `update_scene_actors` (removed; the hold
in `scene_tick_pause_counter` remains):

- **Type 0x8E (entityStateWalkAlert) is installed.** Lifecycle `0x000A55D0`,
  `0x000A5650`, `0x000A5950` (`entities/body_state_actions.cpp`); tick
  `0x000A59C0` with `0x000A5A60`, `0x000A5DC0`, `0x000A5FD0`, `0x000A5C70`,
  `0x000A5EA0`, `0x00133C90`, `0x00133D50`, `0x000258D0` and message
  `0x000A5A40` (`entities/body_walk_alert_tick.cpp`). Slots +0x14/+0x18 are the
  empty `0x001628B0`; +0x1C is `0x0002B620`. The parameters are executable
  objparams, so the state context resolves their pointers through the XBE
  image. Message 0x53 (`0x00023890`) and orientation `0x00134720` still raise.
- **Mina start root.** `0x0002D8E0` samples the position-1 root into player
  +0xF0 (`reference_root`), not +0xB8; `0x00133C90` reads its z at +0x108. The
  start translation and its regression test were corrected.
- **Animation events reach the actor.** Scene players now set
  `ActorAnimationState::message_handler` (`0x0003B3C0`): message 7 travels as
  native words {7, position, slot, event address, phase}. The event bytes are
  bound to a scene address for the call. `0x00028FC0` decodes it once for its
  fade (`0x00019150`) and the base handler (`0x0003EF70`), and body message 7
  views the same bytes. The player message context now reads actor +0x50 when
  the message runs.
- **Floor collision.** The segment walkers pass the caller's two words to actor
  vtable +0x1C/+0x18 in order. `0x0003DF70` and `0x0003E110` AND the first
  with actor +0x1F0 (shape mask) and pass the second as the excluded-flag mode.
  The host had them swapped, so the body floor sweep (`0x00075350`, words 8 and
  the `0x0001FE00` mask) never matched the hall floor. The imps used to fall
  until the bounds check killed them (message 0x33 → script filter 0x31). With
  the order fixed they stay at y = −2.
- **Player mind slots.** `input_at` (actor +0x960) and `target_position` (mind
  +0x10 object +0x20) are attached for type-0x34 minds.

With these changes the five imps update for 900 frames without error. The
first remaining stop is Cooper's player mind request slot `0x0007D360`
(`RuntimePlayerMindRequestUpdate`). Its control update `0x0007D130` needs
contexts that are not built for the player yet: action control
`0x0007D890` (target control `0x0007E4B0` with focus, proposal and feedback;
target-action bindings), pending attachment, inactivity, navigation avoidance,
the boost preset `DAT_002FFF24`, camera direction and invert `DAT_005122D8`.
Build those from `NativePlayerInstance` in `SceneMindMessages`, as the cinema
request context is built, then lift the hold.

Not yet checked: whether retail camera probes (`0x00026100` words 0xC and the
camera mask; camera rig words 4 and the camera mask) behave differently now
that the mask/mode order is corrected; they take the same corrected path.

## Actor updates live (2026-10-14)

`scene_tick_pause_counter` now calls `0x00123020`/`0x00122FE0`
(`update_scene_actors`) every frame. The hold is gone. With no input, the
opening runs 900 frames without an update error. To get there:

- **Mind target registry.** `DAT_00545988` is the `+8` link of each live
  attachment on `DAT_00545984` (linked by `0x0012AFE0`; read by `0x00129640`,
  `0x00129770` and the target search). `refresh_mind_targets()` runs at the
  start of each actor-update pass. It gives every record a retail-layout image
  (+0x10 actor address, then the record fields from +0x14) and a
  `RuntimeMindTarget` and `RuntimePlayerTargetView`. Rows are kept per record,
  and each record's address is its heap token. The "mind targets", "strategy
  target records" and "mind target records" lookups now use this table.
  Mind +0x10 holds one of these record addresses, so the mind services'
  `bytes_at`/`actor_at` and the update's floor hint (`+0x19`) resolve it
  there.
- **Player mind request slot `0x0007D360`.** SceneMindMessages builds the full
  `0x0007D130` control graph for type-0x34 minds:
  - focus, target search, shape target, proposal and target control
    (`0x0007E4B0`), action control (`0x0007D890`);
  - pending attachment, inactivity, player navigation avoidance, the boost
    preset (`DAT_002FFF24`), `DAT_004275B8` and `DAT_00433BD0`;
  - camera direction (`DAT_005077A8` +0x1C, copied before the mind runs) and
    `DAT_005122D8`.

  Player minds register their eligibility, actor address and body query for
  other minds' focus checks.
- **Other actor updates.** Type 2 and areas (0x1F) run the base
  `0x000420D0`. Emitters (0xD) run `0x0010A2E0` and triggers (0x23) run
  `0x00069D20`. Each is registered when the actor receives its first
  message, as the other classes are.
- **Imp fire strategy (0x46) views.** Strategy link +8 maps to its actor's
  object. `body_at`/`body_of` give player bodies (actor +0x95C); for other
  actors `body_of` returns the +0x95C word, which `0x000B4F50` only
  dereferences after `0x000B4EF0`. `model_of` is the actor +0x204 block
  directory.

Still raising when reached:

- body attach action (vtable +0x5C) and walk-alert message 0x53
  (`0x00023890`);
- orientation `0x00134720`;
- the actor +0x8F8 live-attachment view;
- the target-action bindings of `0x0007D890` (the map is empty, so a target
  event with input enabled throws);
- the attachment link map of `0x0007D210`.

The body query that target records expose for a player is that player's
snapshot from the last time its own mind ran, not live body fields.

Next: drive the fight. The `--player-opening` flow presses A through the
tutorial and holds at "(next bit)". Feed stick input past it and translate
what the attack, damage and "Kill All" path reaches.

## Player input after the tutorial (2026-10-14)

Cooper now walks under the left stick. There were two causes:

- **The pad boundary had no sticks.** `InputState` now carries the four XInput
  packets that `0x001555B0` polls: raw thumbsticks, trigger bytes and the Xbox
  button word, including Back, the thumb clicks, X/Y, Black (RB), White (LB)
  and the trigger bits. `pad_poll` copies them into the ports before
  `0x000FA4F0`/`0x000FA840` sample them.
  - The old left-stick-to-D-pad emulation is gone; menus get the D-pad and the
    real stick.
  - On keyboard, WASD drives the left stick and IJKL the right stick on port 0.
  - The host frontend booleans are still filled from the pad buttons.
- **The device refresh never ran.** `0x00109470` copies the selected
  controller into the player's input device only when the activation entry's
  +0x4C differs from `DAT_004DEA58`. The port bound that to a separate
  `g_4dea58` that nothing advanced. Both users now read `Clock::frame`, which
  `0x00033C80` advances, and the stale global is removed.

`--player-opening` now holds the left stick for 120 frames after the
"(next bit)" gate and requires Cooper to move. He moves about 164 units, up the
stairs; the frame is saved to `cache/player_walk_check.bmp`. The SDL build's
input path has no stick mapping yet.

## Combat frontier (2026-10-15)

After "(next bit)" the opening waits on a `scriptConditionId_NumberGhouliesAlive`
condition (the 0x3E record that links to +0x440). Cooper has to defeat the
imps. `gbtg-transition-tests OriginalFiles --player-fight` runs the opening,
then walks Cooper to the nearest imp with the right stick held toward it,
logging imps alive, distance and script title.

Done:

- **Target binding.** `0x0007D890` hands each target to `0x0007DF50` through
  `RuntimePlayerMindActionControl::binding`.
  `SceneMindMessages::target_action` builds the request contexts against that
  target from the attacking body:
  - bits +0x60;
  - tables +0x5C (`tables_address`);
  - chain rows +0x64 / +0x68;
  - attributes +0x268 as `model_state`;
  - the chain-length bounds DAT_00433D5C..64 and the statsheet;
  - the modes DAT_00512248 (powerup row 11 active) and DAT_00512200.
- **Chain rows.** The native `RuntimeBodyAnimationGroup` rows are mirrored into
  a retail-layout image (8-byte rows and 32-bit value words) with a stable
  host-heap address, so chain addresses stored in body +0x88 stay valid.
- **Address resolver.** The mind services' `resolve` maps addresses by arena:
  resources from 0x40000000, the host heap from 0x30000000, scene objects from
  0x10000000, and the XBE below.
- **Mind targets.** `refresh_mind_targets` keeps each live record's target
  object (it used to free targets that strategies still held).

Next. Cooper's attack request now creates factory type 0x73
`entityStateHit` (parent 9, vtable 0x00505940), which is not installed. It
needs:

- sizes `0x00095720`, constructor `0x000957A0`, destructor `0x000958A0`;
- message `0x00095C90` (with `0x000961C0` and `0x00028C80`);
- tick `0x00095940`, with `0x00095F30` / `0x00096260`, `0x00095EA0` /
  `0x0001D5B0`, `0x00096060`, `0x00096390`, `0x00134180` / `0x001346B0`,
  `0x0003CCB0` and `0x0003B260`;
- the body's next-action picks (vtable +0x20 / +0x2C) and the actor message
  0x29.

The tick takes the body's input record as its second argument (it reads the
stick through `0x001085F0`). Animation slot 0's player fields:

| Offset | Field |
|---|---|
| +0x14 | position |
| +0x1C | speed |
| +0x20 | duration |
| +0x2A | mode |
| +0x10C | previous position |

After that comes the imps' side: damage, death and removal through
`0x001232D0`.
