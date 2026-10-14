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
3. **Run `0x0011E640` per frame** so spawned actors activate.
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
