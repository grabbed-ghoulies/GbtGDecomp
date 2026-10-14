# First playable scene

Target: `aid_script_ghoulies_chapter1_scene1_2playcam`.

Current milestone (2026-10-12): opening-scene activation is complete through
`--scene-spawn`, using the real marker/factory/service graph and translated
0x00123070. All 18 spawned actors activate exactly once; pending=0; a second
activation pass does nothing. Explicit teardown releases every factory object
and address mapping. The live Cooper/imp contact-message round trip now reaches
the player-mind B6 filter and verifies sender, receiver, class, and duplicate
suppression. This does not exercise spatial push collision or make the scene
playable.

The retail scene pass `0x0011E640` activates pending actors, sends `0x7D` to
the newly activated `+0x910` list, then calls each actor's `+0x10` slot and
refreshes its transform while draining that one-shot list. This is activation
work, not the recurring gameplay tick. `frame_tick` still has no scene-owned
actor update/draw dispatch. Although player update `0x0002C2B0` is translated,
its per-player `RuntimePlayerUpdate` context is not connected; its required
player-strategy update slot `0x000BEF40` is also not translated. The exported
body confirms that it has real state-dependent branches and cannot be skipped
or replaced by a stand-in. Historical checkpoints below retain earlier stops;
the final progress entries describe the current implementation and validation.

Marker translation checkpoint (2026-10-11): every non-null retail handler slot
is translated and present in the native binder (22 slots, including the shared
kind-6/kind-0x18 handler and the actual empty kind-0x12 handler). No handler is
marked untranslated. Marker resource loading, row transforms and dispatch were
already translated. Scene ownership and the actual factory classes remain
live-spawning dependencies. Validation details and limits are at the end.

This scene is not playable yet. Rendering its background and advancing its
camera do not verify the retail dialogue, actor, or gameplay behavior.

Historical live check (2026-10-10): the script holds at `+0x254`, type `0x6F`.
The legacy live handler still returns a constant 0: this is not yet an
execution of the translated player readiness query. Dialogue and cutscene are both inactive;
the only actor is the camera. Retail player templates and native spawn wrappers
are now available, but the player factory and scene ownership graph remain
incomplete. Dependency preflight progress must be distinguished from live
opening progress. See the final progress sections for the current dependency state.
The complete parent body constructor/destructor (0x00024C50/0x00024F70) are now
translated and checked through their real dependency chains. The complete player
constructor (0x00028830), attachment creation/replacement and base strategy entry
are now translated and checked. Complete player destruction (0x00028A30) and
goal-ring/navigation cleanup are also translated and checked. Player message dispatch
(0x00028F60/0x00028FC0), the render wrapper 0x00028050 and the player update 0x0002C2B0
(with its body collision and contact walk) are translated and checked. Scene ownership, type
registration and the actual body/mind/strategy class methods still prevent live player spawning.

## Reproduction and opening regression

```powershell
cmake --build build --config Release --target gbtg-transition-tests
.\build\Release\gbtg-transition-tests.exe OriginalFiles --player-opening
```

The check runs 300 frames with no controller buttons and verifies that the
script has opened its tutorial dialogue and has not started a cutscene. It
writes `cache/player_opening_check.bmp`. This only tests the opening gate;
passing it will not establish working gameplay.

On 2026-10-08 the check fails: dialogue inactive, imp cutscene started without
A, one actor and one entity (the camera). The existing story transition test
still passes (`scene0_1book` -> garden map -> `scene1_1book`, 461 map frames).

## Retail evidence and missing calls

The scene's node `+0x274`, type `0x14`, names
`chapter1_scene1_fight1_howtofight`. Handler `0x001274B0` checks the actor tag
at node `+8`, then resolves dialogue text from node `+0x48`. Its missing
`0x000582A0` call builds a `dialogs__` localisation key and calls `0x000582E0`.
That function enqueues text in an eight-slot ring and enters dialogue mode 1
through `0x000584D0` when no dialogue is active. A successful enqueue suspends
decay, sets `DAT_00512474`, and returns 0 to hold the script cursor. The next
walk waits until `DAT_004EE1EC` is zero before releasing the gate. The current
port instead returns 2 without enqueuing text.

Type `0x2A` in `0x00126A10` now calls the translated `0x0013E8D0`. This callee searches
the controller registry at `DAT_00512B74` for the node's controller key,
then sends message `0xA8` through vtable `+0xC`, with the ghoulybox and optional
attribute names. The typed request preserves offsets `+4`, `+0x84`, and
`+0x104`; the receiver writes the result at `+0x104`. Missing keys and null
controller slots return the cleared message result, as in the dump.
Controller removal (`0x0012ACA0`) removes the first matching pointer.

The receiving controller and its registration/lifecycle must be reconstructed before this
call can create Cooper, imps, or props. The dump also returns a value from the
message record; inventing a success value would mask this missing work.

The next implemented piece is initializer `0x0006BB70`, in
`src/engine/entities/ghouly_box_controller.cpp`. It appends a nonzero key from
parameters `+0x334`, grows the registry by 10 entries when full, clears only
the pointers at `+0x934` and `+0x93C`, and always advances the supplied retail
LCG state. A random value less than or equal to the chance at `+0x438`, combined
with the `aid_` prefix at `+0x338`, copies the initial box and attributes and
sets `+0xA48`/`+0xA4C`. The field at `+0xA50` is assigned unconditionally.
Selection failure leaves earlier names and selection fields untouched.

The loading regression now registers its test receiver through this initializer
and verifies chance equality, NaN rejection, zero keys, invalid prefixes,
random advancement, and preservation of the load pointer at `+0x938`.
This initializer is not yet installed into the live object factory: doing so
requires its real methods, rather than the test receiver.

Descriptor `0x0006BB30` identifies message receiver `0x0006BDF0` and tick
`0x0006C0A0`. Message `0xA8` checks the source at `+0x93C`, source eligibility
(`0x00146150`), and an empty pending slot at `+0x938`. It calls `0x000F39A0`,
links the allocated record, and writes 1 into the message result only when
allocation succeeds. `0x000F39A0` depends on resource lookup, `0x000F3920`,
and `0x000517B0`; those calls remain the next actor-creation boundary.

Allocator `0x000F3920` is now implemented in
`src/engine/entities/ghouly_spawn_record.cpp`. It allocates a native spawn
record and a pool with stride `0x288`, growth 2, and room for two rows. It
sets the fields written by the dump, including the empty attributes string,
index -1, and the exact `DAT_0032B1A4` timer bits `0x3A83126F`. It leaves the
controller pointer and completion field for their callers to initialize.
Native pointers expand the allocation sizes; row bytes keep their retail size.

`0x000F3C30` now detaches a record from its current list before appending it
to the requested list's tail. A null destination detaches without reattaching.
`0x000F3A00` unlinks a linked record, then frees its pool and record allocations.
The loading test uses these allocated records and verifies FIFO ordering,
holding on a later unfinished record, reparenting between lists, detachment,
and releasing a linked final record without leaving a stale loading gate.

The decoder `0x000517B0`, row-population functions `0x000F3AB0`,
`0x000F3B60`, and `0x000F3BD0`, and pool growth `0x000662A0` are translated.
The heap reallocation boundary follows `0x000F73A0`, including footer tagging
on successful reallocation. The decoder preserves signed integer percentage
conversion, shared LCG advancement, CVTTSS2SI truncation, raw timing bits,
untouched short-row fields, unknown-node skipping and the zero terminator.

```powershell
.\build\Release\gbtg-transition-tests.exe OriginalFiles --ghoulybox-decoder
```

This passes synthetic checks for all eight node types and repeated pool growth,
then decodes all three retail ghoulyboxes in the opening scene bundle: the imp
box has one definition and count 10; Crivens and the cook's chair each have one
definition and count 1. These are data checks, not live actor spawning.

Resource wrapper `0x000F39A0` now calls the existing reconstructed registry
acquisition `0x00035E90` and release `0x00035C00`. It checks the AID prefix,
reports error 2 through `0x00032700` when lookup fails, allocates the pending
record, supplies its default count of one, decodes and releases the resource.
The decoder regression registers the retail bundle through the reconstructed
bundle provider. It checks non-AID rejection, a missing AID (using the retail
error-suppression flag), repeated loads, decoded counts and zero remaining
registry references after each wrapper call. This is not installed in the
live scene controller yet.

The next dependency is the live controller message receiver `0x0006BDF0`.
Eligibility `0x00146150` is now translated in `actor_spawn_eligibility.cpp`.
It tests the link's first pointer before the retail 0x918 sentinel comparison,
then the low byte at +0x93C, the blocker at +0xAD0 and source at +0xAC4.
The sentinel alone is not safe to pass as a native pointer: the dump dereferences
it first. Tests cover null/empty/live links, low-byte flag semantics, blockers
and absent/restored source pointers.

Group lookup `0x00129F00` is now translated in `actor_lookup.cpp`, using the
existing DAT_00511AA4 list representation. It compares the caller key from
+0x1FC against each candidate's +0x1FE and returns the first match. Unlike
`0x00129ED0`, a zero key participates in the lookup. The actor-loading regression
checks these offsets, zero matching, duplicate ordering, misses and empty lists.

The receiver still requires nearest eligible actor query `0x00129D70`, pose and spawn
helpers, and the complete base-message callee `0x0003EF70`. That base handler has
untranslated lifecycle and animation callees. Its default no-op for message
0xA8 does not justify installing a partial base handler. No live receiver is
installed until those dependencies are reconstructed.

`0x001412F0` now implements the completion check: every pending record must
have bit 0 at `+0x2C` set; `DAT_00511B0C` must be empty; live actors must have
one of the retail ready states or a nonzero override at `+0x984`/`+0x98C`.
The script sends its request once, holds on subsequent walks, then clears its
gate when this predicate passes. An empty world passes this predicate in the
retail dump; it does not prove that actors were created.

```powershell
.\build\Release\gbtg-transition-tests.exe OriginalFiles --actor-loading
```

This test exercises the actual script handler with an explicit test controller,
including message/result propagation, controller removal, null duplicate
slots, asynchronous waiting, actor readiness, and reissuing after completion.
It passes. The full player opening test still fails because standalone
dialogue is not implemented; the game still has only its camera actor.

The scene contains the imp-entry cutscene node at `+0x980`. A cutscene whose
camera advances while no actor objects exist cannot establish gameplay.

## Decompile recovery

The current project is `Z:\GhouliesStuff\ghidra_projects\GbtG`, program
`default.xbe.original`. `ExportPlayerScene.java` exports specified existing
functions to `cache/ghidra_frontier/<VA>.txt` without renaming or creating
functions. Run Ghidra with `-readOnly -noanalysis`:

```text
-scriptPath tools\ghidra\ghidra_scripts -postScript ExportPlayerScene.java 001274B0 000582A0 000582E0 000584D0 0013E8D0
```

The dialogue's text-resolution, audio, widget, and camera dependencies remain
part of the reconstruction requirement. The existing cutscene-caption renderer
alone does not implement the standalone dialogue queue. Follow the exported
callees; do not replace those calls with an ad hoc tutorial overlay.

After the opening regression passes, extend acceptance to A advancing all
tutorial pages, retail cutscene completion, live actor creation, and actual
controller-driven player movement and fighting.

## Receiver dependencies added

The fallback `0x00129D70` is a nearest eligible actor query over DAT_00511B28,
not an actor constructor. It uses `0x00129C50`, a 625-unit squared vertical
threshold (DAT_0032B524 = 0x441C4000), nearest squared distance and optional
path validation `0x00129CD0`. Its filter is now translated, with its complete
leaf predicates:

- `0x00146080`: requester class key against signed-count stride-0x10 entries.
- `0x001460C0`: +0x93F state, optional +0xD8C owner, owner mode and full flag word.
- `0x00145470`: +0x9A0 blocker and model header +0x10 -> +0x10 unsigned flag byte.
- `0x00129C50`: exact optional requester and flag gates, including the previously
  translated `0x00146150` eligibility check.

The +0xAC4 field is now named `spawn_point_count`, based on `0x001476D0`;
the earlier opaque-pointer representation is removed. Its nonzero eligibility
comparison is preserved. +0x93F reads the high byte of the existing +0x93C word.

Base-handler animation dependencies `0x0003ACB0` (four-slot controller creation
and event bridge) and `0x00030B10` (named/borrowed animation mix dispatch) are
translated using the existing reconstructed animation engine. Base-handler
segment registration `0x0003E810` now appends the actor's probe to DAT_00511AF4
when +0x1F0 is nonzero, retaining sentinel and tail links.

The actor-loading regression covers these actor filter and list predicates.
The animation event regression covers controller reuse, event dispatch and
borrowed animation ownership without looking up the ignored name.

Still required before the parent fallback and receiver can be installed:
`0x00129CD0` and its spawn-point/navigation callees `0x001476D0` and `0x0012CC50`;
plus the remaining `0x0003EF70` lifecycle/animation helpers.
Their dumps are recovered. No success value is substituted for these callees.

Validation for this dependency batch: Release builds of `gbtg`,
`gbtg-transition-tests` and `gbtg-animation-event-tests` pass. The actor-loading
and retail ghoulybox regressions pass, and all five CTest tests pass, including
the expanded animation-event regression. The opening is still not playable;
the controller receiver and parent fallback have not been installed.

## Navigation neighbor checks

`src/engine/navigation/navigation.h` now exposes the existing navigation grids
with the complete 0x12-byte cell layout. The former collision-local view kept
only bottom/top heights. Those fields retain offsets +2/+0xE; the shared view
also retains X/Z, flags +7, region +0xC, connection bits +0x10 and flags +0x11.
Static assertions verify the size and every field used by this batch.

Translated in `navigation.cpp`:

- `0x00110E80`: world-to-cell coordinates with CVTTSS2SI, 32-bit origin
  subtraction, signed division and unchanged output arguments on failure.
- `0x0012CA40`: destination-to-source neighbor connection query, including
  the 0xFF shortcut, eight-bit direction order and grid bounds.
- `0x00036770`: isolation query with optional masked region matching, blocked
  neighbor flags and closed-region bits; the distance square root is evaluated
  and discarded as in the dump.

The actor-loading regression checks all eight directions, boundary rejection,
empty connection masks, the 0xFF shortcut, matching and different regions,
blocked neighbors, signed coordinate truncation, origins and missing grids.
Release builds and both actor-loading/retail ghoulybox regressions pass;
all five CTest checks also pass.

The cell eligibility, occupancy query and full path walker described below
are now translated. The spawn-point chain still needs `0x001476D0`,
`0x00056330` and `0x00056790`. Newly exported dumps identify those calls;
none has been replaced by an assumed-success result. The live opening remains
unconnected to the full receiver.

## Occupancy and path walker

Added `occupancy.h/.cpp` under navigation. The typed 0x38-byte records preserve
mask +4, type +8, group +0x1C, center +0x20, dimensions +0x2C/+0x30 and yaw
+0x34. The used end is pool +8; spare capacity is not searched. The query uses
the existing DAT_00512344/DAT_00512348 state instead of a second registry.

Translated:

- `0x00124640`: sphere containment and outside-distance output.
- `0x001244D0`: retail rotated-box containment, height gate and corner distance,
  using reconstructed absolute/sine/cosine routines. Assembly COMISS/JA behavior
  is retained for unordered comparisons rather than adopting Ghidra's ordered
  less-than-or-equal shorthand.
- `0x001248D0`: first matching used record, aggregate mask, exclusion, group
  wildcard, supported sphere/box dispatch and missing-pool behavior.
- `0x0012CB80`: marked-cell, region/tag and occupancy predicates, including the
  exact allowed-record exception.
- `0x00110BF0` / `0x00110EF0`: full floor selection and cell indexing.
- `0x0012CC50`: both path rasterization branches, connection checks, intermediate
  cell validation, early target return, failure-output rules and cross-floor flags.

Auditing `0x00110BF0` corrected the old collision helper: preferred floor -1
never takes the height-band early return; it chooses the last minimum strictly
below the point. The old count cap and extra cell-index guards were absent
from the retail dumps and have been removed from these reconstructed entries.
Collision queries now call the same verified functions as the path walker.

The actor-loading regression covers geometry boundaries, untouched containment
outputs, rotations, unordered comparisons, pool/type/group/mask/exclusion gates,
used-end limits, cell flags and occupancy exceptions, horizontal/reverse/vertical/
diagonal paths, failure outputs, preferred floors and cross-floor transfer flags.
Release builds and retail ghoulybox regressions pass; all five CTest tests pass.

## Live spawning reconstruction in progress

Completed and built on 2026-10-08:

- Spawn-point matrix/sphere transforms and nearest-point selection:
  `0x000315F0`, `0x00056790`, `0x00056330`, `0x001476D0`.
- Navigation-aware nearest spawn source: `0x00129CD0`, `0x00129D70`.
- Retail resource cloning and spawn factory overrides: `0x001347C0`,
  `0x001348F0`, with the scene type lookup `0x0011F2E0`.
  The unit-scale wrapper's register arguments populate derived override slots
  `+0x4BA`, `+0x53A`, `+0x5BA`; its stack arguments populate `+0x3BA`, `+0x33A`.
- Class-cap gate `0x0013E860`, queued messages `0x000F3E90`,
  `0x000F3FC0`, `0x000F4030`, row selection `0x000F40A0`, and timing/retry
  scheduler `0x000F3CF0`.
- Proximity entry/chance trigger `0x0013E980`, controller tick `0x0006C0A0`,
  and controller shutdown `0x0006BC70`.
- Shape registration/removal `0x0003C770`, `0x0003C700`; render-bin
  registration/removal `0x0003CAE0`, `0x0003CBB0`; segment removal
  `0x0003E850`; child-list registration `0x0003ED70`.
- Model-anchor allocation/update/registration `0x0011C0B0`, `0x0011BE40`,
  `0x0003EEB0`.
- Bind-tree traversal, model pose updates, live attachment release, and sound
  pose updates now have translated modules under `src/engine/entities/`.
- Camera effect creation and resource-backed shake records: `0x000493D0`,
  `0x000494B0`, `0x00049520`, `0x000FB120`, `0x000FB7C0`.
- Effect node allocation, particle free chains, recycling, texture/model changes,
  and all 27 effect stream nodes: `0x000ED120`, `0x000ED170`, `0x000ED1C0`,
  `0x000ED550`, `0x000ED730`, `0x000ED8C0`, `0x000ED970`, `0x000ED9F0`,
  `0x000EF790`, `0x000EF7F0`, `0x000EF870`.
- Effect resource dispatch `0x00053060`, `0x00053140`, `0x00053170`; duration
  and last-effect retention `0x0003A9A0`, `0x0013E750`.
- Attachment request and callout dispatch `0x0003BBF0` through `0x0003BEC0`,
  including the exact 85 retail callout names and `0x0003BE20` linked records.
- Spawn queue selectors `0x000F36C0`, `0x000F3840`, `0x000F5280`,
  `0x000F5610`, `0x000F50D0`; population/resource/timing gates `0x0013E790`,
  `0x0013E7E0`, `0x0013EB10`, `0x00051900`, `0x000F5200`.
- Trail parameter initialization and factory requests `0x000411A0`,
  `0x0013B030`, `0x0013B1B0`.
- Scare-event placement, model-height fallback, factory calls and world
  broadcasts: `0x0003DB90`, `0x0013B270`, `0x0013B3A0`, `0x00122CE0`.
- AID spawn queue allocation, decoding, selection, retries, and release:
  `0x000F3640`, `0x000F3690`, `0x000F37B0`, `0x000F5370`, `0x000F53D0`,
  `0x000F5110`, `0x000F5170`, `0x000F5530`, `0x000F5410`, `0x00051940`,
  `0x000F4E50`, `0x000F4F80`, `0x000F4EE0`.
  Scene cleanup now releases this complete native queue representation.
- Frame queue worker `0x0011B670`, including the pause/global-disable gates,
  player-position lookup, saved-next walks, and completed-record unlinking.

Release builds, actor-loading regressions, and all five CTest cases pass.
These checks exercise actual translated functions with synthetic state and
retail resources; they do not establish that the scene can spawn actors yet.
The controller receiver `0x0006BDF0`, inherited actor message handler
`0x0003EF70`, and animation event dispatcher `0x0003A380` are now translated.
The receiver regression uses these actual handlers to select distinct source
group and anchor IDs, return a source pose, reject an unavailable source, decode
and enqueue an A8 request, preserve a pending request, and forward source release
and animation events to the inherited handler.

The user authorized a real host audio backend. Added the audio core allocation,
source creation/mix/destruction, cue preparation/play/release, voice allocation,
callback, and update functions. The original failure branches and cue flags are
preserved, including the original cue-wrapper leak on failed preparation.
The XAudio2 PrepareEx boundary submits a buffer without starting the source voice;
PlayEx starts that prepared voice and propagates actual backend errors.
Compressed wave payloads are rejected until a decoder exists, rather than submitted
as PCM.

```powershell
.\build\Release\gbtg-transition-tests.exe OriginalFiles --audio-preparation
```

This hardware check passed with `XACT_SOUNDBANK_GPIRATE_ATTACK`: one queued buffer,
zero samples played before PlayEx, successful actual playback, then voice release.
Release builds, the expanded actor-loading regression, and all five CTest checks
pass. The hardware check is separate because it requires a working audio device.

The existing inherited actor initializer, factory registry, resource views, audio
notifications, and frame worker still need scene-owned runtime state and lifecycle
wiring. The translated controller receiver is not installed into a partial factory.
Live spawning and gameplay have not been established.

The type-1 background name request (0x0011C890) and deferred object name request
(0x0011C950) now preserve their retail queues instead of discarding script names.
Added the background parameter builder (0x0011C8B0), which uses the translated
0x000411A0 initializer and 0x00107520 factory. Both targets build, and the
actor-loading regression passes after this change.

The retail factory table selects type 4 through 0x00042BC0, with constructor
0x00042DA0 and destructor 0x000430C0. The constructor assembly is exported in
cache/player_instructions_00042DA0.txt. It walks model block 20 through 0x001198A0,
handles block 8 through 0x000FEFD0, and allocates background animation state.
These calls and the inherited actor destruction path are the next reconstruction
dependencies. The queued background is not yet consumed by a scene-owned factory.

Added background geometry dependencies 0x0003DBF0 / 0x0003DCA0 (first matching
model block 5, scaled endpoints), 0x00118AE0 (25-unit scene margin and eight
corners), and 0x001198A0 (block-20 region registration). The endpoint readers
preserve the assembly's scale multiplication order and stop at a null first
matching block. Regression checks cover reflected bounds and overflow from an
incorrect multiplication order.

Added physical-shape event traversal 0x00055A30, type-1 / type-2 visitors
0x00054110 / 0x00054DB0, and filtered record traversal 0x000555C0. Native event
list views must map actual relocated descriptors; no missing list is synthesized.
The background event collector 0x00043DD0 uses the existing translated heap and
pool-growth functions, preserving event source pointers and complete flag words.
Checks cover filtering, callback early termination, and growth beyond ten records.

The read-only Ghidra exporter can now temporarily define a labeled code range,
decompile it, and roll back the transaction. This produced the complete
0x00042DA0 constructor dump without saving changes to the Ghidra project.
Both Release targets, the actor-loading check, and all five CTest checks pass.
Background animation setup, effect registration, actor destruction, and complete
scene-owned factory/resource wiring remain before live spawning can be tested.

Added the next eight background-construction dependencies:

- 0x00155FE0 / 0x00156050: matrix count and first-match matrix binding, including
  rejected bindings returning 0xFFFFFFFF.
- 0x000400D0: actor matrix storage setup against the actual ndMtxArray node and
  relocated binding views. Empty matrix arrays preserve the per-record globals.
- 0x00045F70: merge matching kind-3 spheres from model block 4 into the background
  animation volume; preserve unsuccessful outputs and retained shape byte 3.
- 0x0010AA60: forward shape copy, preserving alignment branches and overlapping
  dword-copy behavior.
- 0x0012AFE0: allocate and link a live attachment into its actor and global lists,
  copy both shapes, and retain the original mode/tag/origin fields.
- 0x00045D70: initialize the animation volume's live attachment and model anchor,
  including the exact 0x3F2A7EFA height multiplier and radius at anchor +0x34.
- 0x00042C30: background model-node traversal, clearing push-buffer state and
  expanding bounds by 70 only below matrix arrays while preserving sibling scope.

Release targets, the expanded actor-loading check, and all five CTest checks pass.
The checks exercise allocation/release ownership, merged volumes, missing outputs,
matrix binding rejection, overlapping copies, and matrix ancestry during node walks.
The complete background constructor still depends on animation initialization
0x00044490, effect registration 0x00043B40, bounds/shape initialization 0x00043190,
resource lookup, and destruction. No incomplete constructor has been registered;
live spawning remains unverified.

The scene-script interpreter (0x00128A00) now advances when all handlers return
3, matching the retail dispatcher, and no longer imposes a 256-node host limit.
A regression executes 300 unclaimed nodes, stops at the actual type-0x10 gate,
then resumes the retained cursor to the terminator when that gate opens.

Added retail local-text dependencies 0x000FBF90, 0x000FF3C0, 0x00162910,
0x00162920, 0x00162950, 0x00162A20, 0x00162A30, 0x00162A80, and 0x00167F86.
These preserve key sanitation, collision-to-ID mapping, packed six-byte entry
lookup, the special servants-dormitory key, and empty/disabled failure results.
Release targets, the expanded actor-loading check, and all five CTest checks pass.

This is not completion of the opening script. Its type-0x14 dialogue action still
advances without calling the retail dialogue enqueue path. That path requires
resource acquisition/lookup 0x000FF410 / 0x000FF0A0, queue 0x000582A0 /
0x000582E0, screen activation 0x000584D0 and its UI/camera/audio dependencies.
The conditional actor predicate 0x00129420 also remains untranslated. No partial
dialogue implementation has been wired into the scene handler; the first
dialogue, cutscene, and gameplay sequence remains unverified.

Added 0x000FF410 text resource acquisition against the translated registry and
mandatory relocated local-text views. It allocates native lookup state at the
retail heap call, copies UTF-16 with 0x001660E9 / 0x0014A961, and releases both
lookup state and resource on success and lookup failure. The failure path keeps
DAT_0042A714 as retail does; the success path clears it. Tests cover both paths
and verify the registry reference count returns to zero.

Added 0x0014AA3D UTF-16 length, 0x00056F10 prefix token lookup, 0x00057260
next-page speaker selection, and 0x00058B50 button permission. The tests use the
actual 92 retail token strings at PTR_DAT_004245A8, represented as an immutable
native UTF-16 view; this does not implement 0x00056DF0's heap ownership.
Both Release targets, the actor-loading check, and all five CTest checks pass.

Still missing before enqueue/screen activation can be connected: 0x000FF0A0's
formatting wrapper, 0x000584D0's widget/camera/actor dependencies, and the text
layout/tag execution path 0x000583C0 / 0x00059880 / 0x00056EA0 / 0x00056F80.
Their dumps have been exported read-only. Queue functions 0x000582A0 and
0x000582E0 have not been installed with callbacks for missing callees.

Added the namespaced request 0x000FF0A0 using translated string formatting
0x001661F9 / 0x00168FED. The formatter keeps the original null-output count
mode and termination branch; its fixed 0x42 string-stream flags select the
inlined IOERR branch of 0x00168E46, without requiring a simulated file flush.
The request copies the resource name, sanitizes the key, formats group__key,
then calls the translated acquisition path. Tests verify sanitation, lookup,
character counts, null-output formatting, and balanced resource references.

Added closing-path resource cleanup 0x00057400 / 0x000577B0, preserving speaker
reset fields, the mode-dependent return value, and the retained icon when no
icon resource is owned. Added widget parameter copy 0x00142DF0: ten 0x48-byte
rows, copying only each row's icon, terminated text, and trailing option word.
Tests verify all ten rows, retained bytes, empty inputs, and cleanup ownership.
Both Release targets, the actor-loading check, and all five CTest checks pass.
The queue's real master-volume/reverb calls and complete screen activation
still need translation and scene-owned runtime wiring; opening dialogue remains
unverified.

Added dialogue audio dependencies 0x001673D8 (x87 integer conversion),
0x0015B8F0 (127-based attenuation), 0x0015C1C0 (category volume), 0x0015C1E0
(twelve I3DL2 listener fields), 0x0003A1F0 (owned reverb preset acquisition),
and 0x00058B90 (duck/restore categories with attract-mode bypass).
The host audio boundary now reads actual sound-entry category byte +0xA,
retains category volume for newly prepared voices, and updates matching live
voices. Reverb uses the real XAudio2 reverb effect and SDK I3DL2 conversion,
returning actual creation/setup HRESULTs. The current native listener applies
to the master mix immediately; per-source reverb sends and deferred listener
updates are not reconstructed and remain limitations of that host boundary.

Added text-pool ending 0x0005F7E0 against native views of the retail 0x1238-byte
records and font glyph textures. It preserves the first-character texture
lookup on every iteration, the assembly's zero-extended marker comparison,
random-state advances, lesser/greater/unordered age comparisons, and sticky
flag clearing. Tests cover record misses, multiple glyphs, timing clamps,
empty text, category restoration, preset lifetime, and failed boundary returns.

Both Release targets, the expanded actor-loading check, and all five CTest
checks pass. The separate real-device audio-preparation check also passes:
it creates/sets the reverb DSP, changes and reads back prepared-voice volume,
then starts and releases the retail cue. This establishes audio boundary
operation, not acceptance of the complete dialogue or opening scene. Queue
and screen activation remain unwired while their widget/camera/text-layout
callees are incomplete.

Added borrowed local-text lookup 0x000FF120, wide scanner wrapper 0x00166A72,
and text metrics 0x00060270 / 0x00051760 / 0x000607F0 / 0x0005F250 /
0x00060980 / 0x00060790 / 0x00060A40. Added text record creation 0x0005F5C0
and pool allocation/release 0x0005F390 / 0x0005F460, preserving record growth,
registry compaction, sticky lifetimes, transform-prefix behavior and panel bounds.
Added widget text layout 0x00142AF0, mode changes 0x00142C20 and prompt changes
0x00142180. These routines use owned icon resources, borrowed local text and
actual text records rather than caption substitutes.

Added widget position builder 0x001421C0, preserving single-column unrolled
operations, alternating two-column loops and first-position-only updates for
other modes. Added line width 0x00060710 from its assembly, including the
single-scaled space path (different from character advance), newline maximum
updates and unordered comparisons. Added widget width 0x001428F0 with real
resource acquire/release, sentinel row count and retail constants 32 and 20
verified at 0x0032B458 / 0x0032B234. The absence of a row sentinel leaves the
active count zero, as in the original.

Release gbtg and gbtg-transition-tests build successfully. The expanded
actor-loading suite passes, including pool lifetime/growth/compaction,
positions, line width, widget width/resource lifetime, and prompt text
activation/closing. All five CTest checks pass. Pool lifecycle checks now live
in check_actor_loading itself rather than the unrelated spawn-effect block.

Remaining: 0x00141E20 widget constructor reads the mode word without writing
it; 0x000F72D0 requests allocation with zero heap flags. Its initial state
must be accounted for without introducing an invented mode. Widget update,
button-bat actor factory, full tag processing, camera transitions and complete
0x000584D0 activation still have untranslated callees. The scene-owned
0x000582A0/0x000582E0 queue and script type 14 remain unwired. These passing
component checks do not establish dialogue/cutscene/gameplay acceptance or
live scene actor spawning.

Added dialogue input dependencies 0x001554C0 (stored device mask),
0x00107CE0 (port presence), 0x001082B0 (device presence), 0x00108300
(held buttons), 0x001083F0 (new button edge), 0x000335A0 (activation latch),
0x001085F0 (analogue axes), and 0x001429E0 (dialogue direction). Exported
assembly and jump-table words recover branches omitted by the decompiler.
Native device views preserve controller +8, special device +0x10 and shared
keyboard state, with current/previous buttons at +0x38/+0x74 in each block.
The aggregate axis routine overwrites its output once per child, including
an absent final child; this behavior is retained. Direction uses strict
+/-0.2 dead-zone bounds and left/down precedence for opposing held buttons.
These routines are component translations; scene-owned input views are not
yet installed in the complete dialogue widget update.

Added button-bat creation 0x0013B580 through the existing executable-resource
clone and object factory. The configuration byte's 0x40 bit selects normal
or gold retail resource names, position shifts are scale*2 and scale*21,
and depth -1 retains the resource value (NaN is an override). Parameters
are released after factory construction. The factory still requires actual
registered constructors; this does not install the missing button-bat actor
constructor or its update/draw/destroy handlers in the scene registry.

Release gbtg, gbtg-transition-tests and gbtg-object-factory-tests build.
The actor-loading input checks and all five CTest checks pass, including
normal/gold factory selection, pose/depth and cloned-source ownership.
Complete widget update/constructor, tag processing, camera transitions,
screen activation and dialogue queue wiring remain outstanding.

Added owned token initialization 0x00056DF0 from the retail ASCII table,
allocating the original 0x86-byte blocks including heap footer and preserving
signed-byte widening and table order. The prior immutable UTF-16 view remains
a separate borrowed representation. Added wide prefix comparison 0x000FBDD0
with unsigned 16-bit subtraction, count-zero and terminated-prefix behavior.

Added bounded CRT formatting 0x00166449 using the translated 0x00168FED output
engine and fixed string-stream error branch, and resource-name formatting
0x00035830 with its 128-byte limit and ASCII lowercase pass. Added speaker
selection 0x000576D0 with prior texture release, alternate mode/scene head
swaps, no-speaker sentinel, texture height and retained previous/next fields.
Added icon selection 0x00057970, preserving same-icon early return, owned
texture replacement, five retail name patterns and sentinel release. Its
native caller supplies the name buffer; invalid groups preserve those bytes
rather than receiving an invented resource name.

Added score classification 0x0004A400 with ascending/descending threshold
orders, inclusive boundaries and -1 sentinel. Added final/high trophy tags
0x00057330 / 0x000573A0 and pause-icon substitution flags 0x000572D0.
Tests cover owned token lookup, resource formatting, head swaps, texture
lifetime, all score tiers, trophy acquisition/repeat selection/release and
pause substitution flags. Release builds, actor-loading checks and all five
CTest checks pass.

Heap investigation: 0x0014E872 merges allocation flags with heap +0x18.
0x000F7150 creates the heap with flags zero; the engine record's +0x28 value
8 is distinct from those heap defaults. This investigation has not proved
an initialized widget mode word for 0x00141E20, and no mode was invented.
The numeric tag's 0x00166B9D still needs 0x0016AD9A; sound-tag 0x00057B00
still needs complete string-copy/concatenation translation. Full 0x00056F80
processing, widget lifecycle, camera transitions, 0x000584D0 activation and
scene-owned dialogue queue/type-14 integration remain outstanding. Component
checks do not establish opening dialogue, cutscene, gameplay or live spawning.

Added counted CRT string copy 0x001665E0 and append 0x001664A0, retaining
alignment-sensitive byte/word paths, word zero-byte detection, zero padding,
count-zero return, truncation and append termination. Actor-loading checks
compare padded buffer contents and returned pointers against the CRT across
four alignments, nine source lengths and twelve counts.

Added sound-tag handler 0x00057B00 with the actual 18-entry boy and nine-entry
girl source/target mapping tables exported read-only from the retail program.
It retains leading-space skipping, case-sensitive prefix substitution under
alternate mode/scene flags, closing-brace boundaries, cue prefix construction,
128-byte copy and ASCII uppercase. Empty text preserves the previous cue;
unknown dialogue cue names remain unchanged. Tests cover both swap directions,
ordinary names, unmatched names, capitalization and empty input. This routine
selects the cue name; playback remains owned by the existing audio paths.

Release gbtg and gbtg-transition-tests build, actor-loading checks pass, and
all five CTest checks pass. Numeric conversion 0x00166B9D / 0x000FB810 is still
blocked on complete 0x0016AD9A reconstruction: its 0x0016E7CA / 0x0016E744
callees must expose parser status, consumed length and conversion status.
The existing collapsed crt_parse_double value path does not expose these
outputs and was not substituted with invented return flags. Full tag
dispatch, widget lifecycle, camera/screen activation and scene queue wiring
remain outstanding.

Added the numeric conversion foundation in runtime/crt_mantissa.cpp:
0x0016F505 word addition/carry, 0x0016F526 96-bit addition, 0x0016F584
left shift, 0x0016E44C zero-tail query, 0x0016E47E increment/carry,
0x0016E4CB rounding, 0x0016E53D copy, 0x0016E558 zero query,
0x0016E571 right shift, 0x0016E5EC format conversion, 0x0016E744 double
conversion, and 0x0016F5DF decimal digit accumulation/normalization.
The double descriptor is verified at 0x00338308. Assembly restores omitted
return values and all-three-word zeroing where decompile output loses them.
Native unsigned operations preserve x86 masked shift counts and carries;
conversion returns the retail normal/overflow/underflow statuses.

Tests cover two-word carry propagation, cross-word left shift, whole-word
right shift, exact-half versus sticky-tail rounding, digit accumulation to
125, normal 1.0, infinity/overflow, the smallest double subnormal/underflow,
and negative zero. Release builds, expanded actor-loading checks and all
five CTest checks pass.

Numeric tags remain unwired: decimal scale multiplication 0x0016F8CF still
needs 0x0016F6AD, after which the 0x0016E7CA parser state machine and
0x0016AD9A / 0x00166B9D / 0x000FB810 wrappers can use these actual status
results. Full tag dispatch, widget/camera/screen lifecycle and scene dialogue
queue integration remain outstanding. No end-to-end opening acceptance is
claimed by these arithmetic checks.

Completed decimal scaling 0x0016F5B2 / 0x0016F6AD / 0x0016F8CF with
the exported retail power tables, then the parser 0x0016E7CA and wrappers
0x0016AD9A / 0x00166B9D. Parser results retain consumed length, invalid
input, overflow and underflow flags; incomplete exponents roll back to their
marker. Added numeric tag conversion 0x000FB810 and full tag dispatch
0x00056F80 / 0x00056EA0, using the translated speaker, icon, trophy and
sound handlers. Checks cover numeric scale/time, paragraph/alignment flags,
sound names, unknown and unterminated tags, and the 63-character numeric cap.

Added integer conversion 0x00167B2C / 0x00167B6A and text substitutions
0x0005A6C0 / 0x0005A730 / 0x0005A7A0. These retain borrowed scratch output,
32-bit wrapped count addition, signed decimal formatting and byte-to-wide
copy. Checks include INT32_MIN, unsigned hexadecimal -1, zero replacement
and both accumulated counts. Release gbtg and gbtg-transition-tests build;
actor-loading checks, all five CTest checks and git diff --check pass.

These supersede the numeric/tag blockers above. Layout 0x00059FE0 still
needs its other substitutions, including 0x00066180, 0x000FBB20,
0x000FB860 and the wide formatter 0x00166A03. Widget/camera/screen
lifecycle, dialogue queue/type-14 integration and live scene spawning
remain outstanding; no end-to-end opening acceptance is claimed.

Added bit population count 0x00066180 and count substitution 0x0005A810,
wide formatting wrappers 0x00166A03 / 0x00166FB1 / 0x0014BC66 /
0x0014BC7D, wide append 0x001660BF / 0x0014AA05, and score substitution
0x0005A920. The missing-score text at 0x00323C34 is the verified UTF-16
question mark. The formatter preserves both byte writes of the terminating
wide zero and the fixed string-stream error branch.

Added localized time formatters 0x000FBB20 / 0x000FB860 using actual
time resource acquisition, borrowed key lookup and resource release, the
existing 0x00152BE0 remainder and 0x001673D8 integer conversion, and the
exported 0x0032B150 multiplier. Added elapsed/final time substitutions
0x0005A880 / 0x0005A960. Day-branch seconds labels intentionally follow
the minute count, as the retail instructions do. Checks cover zero,
singular/plural, hours, days, negative values, omission of day splitting,
appended output, scratch replacement, resource lifetime and full UTF-16
append. Release builds, actor-loading checks and all five CTest checks pass.

The substitution callees needed by 0x00059FE0 are now available. That
routine still needs translation and integration; dialogue page layout,
widget/camera/screen lifecycle, scene-owned queue/type-14 execution and
live spawning remain outstanding. These component checks do not establish
the complete opening sequence.

Added segment measurement 0x00059FE0 with actual tag processing and recursive
count, score and localized-time substitutions, icon/seed widths, paragraph
initial scaling, apostrophe retention, single-scaled spaces and page/line
stop flags. Assembly restores the float return omitted by the decompile.
The closing brace remains the preceding kerning character, and unordered
scales propagate through the original paths. The native context supplies
the distinct retail initial/drawing scales and shared scratch buffers.

Added page layout 0x00059880: five line-width slots reset to -1, text
spacing set to 2, initial/following segment measurements, space lookahead
wrapping, explicit break/page flags, line limits, icon widths and every
substitution branch. Captured layout scale remains distinct from tag state.
Retail wrapping stores the prior width in the next line slot; the maximum
width is tracked separately. A retained break flag suppresses the final
line-count increment even after ordinary characters; a subsequent tag can
replace it. Default-scale initial and layout widths also remain distinct.

Tests cover paragraph/apostrophe behavior, kerning across tags, NaN scales
and wrap widths, empty pages, explicit and wrapped line limits, five-slot
sentinels, all score/time/pause-stat substitutions and owned resource release.
Release builds, actor-loading checks, all five CTest checks and diff checks
pass. Widget/camera/screen activation, scene-owned dialogue queue/type-14
execution and live spawning remain outstanding. Full opening acceptance is
still pending.

Added camera view initialization 0x000496C0 in camera/view_initialization.cpp,
with the native view record exposing pose, focal/clip/blend values, aspect
slot, flags and fitted projection outputs. The quaternion branch writes
identity; the Euler branch clears only XYZ and preserves the fourth word.
It copies through the name terminator, clears both links, and calls the
existing 0x000F57F0 display-fitting routine. Constants at 0x002038F0 /
0x002038F4 and 0x0032B30C were verified from the retail program.

Checks cover both pose branches, retained flag bits/fourth component, link
clearing, exact short-name copy, 32-character names and the component
fixture's zero-size display projection. Release builds, actor-loading
checks, all five CTest checks and diff checks pass. Initialization is not
yet installed into dialogue activation: 0x00048CF0 still lacks its full
retail pose/projection work, and widget lifecycle/scene dialogue queue
integration remain outstanding. The exported 0x000F5900 dump records the
display initialization chain for continuing that work.

Completed the missing pose/projection work in named camera binding
0x00048CF0. Mask 2 seeds a default position for an empty active slot or
copies the current display position; mask 1 converts the default/negated
display forward vector to Euler pitch/yaw and clears roll while preserving
the fourth word. The quaternion path copies all four display words.
The prior name is copied through its terminator, the selected view becomes
active, and 0x000F57F0 fills its pixel aspect/focal outputs. The display
view now exposes the actual pose fields as a native internal record.

Added insertion 0x00048C20 with root traversal, head linkage, previous-link
repair and automatic named binding only for an inactive slot. Existing
0x00048C70 removal now reaches the completed named-binding behavior.
Checks use a 640x480 display record and cover insertion order, case folding,
default/quaternion/Euler poses, untouched unseeded poses, saved-name fallback,
lookup miss and empty-list cleanup. Release builds, actor-loading checks,
all five CTest checks and diff checks pass. Display initialization and
scene/widget activation are still separate outstanding integration work;
the full opening dialogue/cutscene/gameplay sequence remains unverified.

Completed display initialization 0x000F5900, Euler-to-quaternion conversion
0x00104EB0 and display rotation/forward-vector setup 0x000F5AE0 in
camera/display_initialization.cpp. Four native display records preserve
retail flags, viewport fitting, initial pose, clip distances, identity
matrices and the final active-record selection. Checks cover quaternion
rotation, normalization and the zero-quaternion branch.

Added dialogue page preparation 0x000583C0 and reset 0x000589F0 in
frontend/dialogue_page_setup.cpp. Preparation processes leading tags,
whitespace and the tilde marker before the existing retail page layout.
Reset applies character/line limits, sound and style defaults, captures
alignment and conditionally scans the next speaker. Checks cover retained
text for whitespace-only input, leading head/button tags, both entry line
limits and next-page speaker lookup.

Release builds of gbtg and gbtg-transition-tests, the actor-loading checks,
all five CTest checks and git diff --check pass. These functions still need
the scene-owned activation and widget lifecycle dependencies before the
script dialogue handler can run the complete opening sequence. Live scene
spawning and dialogue/cutscene/gameplay acceptance remain outstanding.

Completed widget destruction 0x00142070 in
frontend/dialogue_widget_lifecycle.cpp. The native widget now exposes the
retail text-resource ownership flag and bat pointer. Destruction releases
only owned text resources, releases all ten icon slots, removes the text
pool from its registry, clears prompt state and runs the existing actor
child activation/message/registration chain before releasing widget heap
storage. Tests cover borrowed and owned text, first/last icon slots,
registry removal, child-before-parent registration and already registered
actors without repeated activation messages.

Added page voice request 0x00057D10 in frontend/dialogue_page_voice.cpp.
It allocates mode zero, writes the actual voice stop word at +4 and calls
0x00039A20 with the selected dialogue cue. Tests cover successful cue
ownership, missing-name behavior and exhausted voice slots. The function
retains the underlying allocator/preparation results; it does not substitute
success for unresolved cues.

Release gbtg and transition builds, actor-loading checks, all five CTest
checks and diff checks pass. Scene activation 0x000584D0 and the widget
constructor/update/bat-factory integration still remain. In particular,
0x00141E20 never writes widget +0 before 0x00142C20 reads it; setting mode
zero would add behavior absent from the dump. The opening scene is still
not accepted as a working dialogue/cutscene/gameplay sequence.

Added actor attachment refresh 0x0003CD00 in entities/actor_attachment_updates.cpp.
It drains the pending list, starts each real effect stream, releases rejected
or transient requests, appends retained effects to the active list and updates
position/euler fields only when their retail flags request following. Tests
exercise successful and rejected requests together, transient flag 8,
pending unlinking, retained ownership and enabled/disabled pose following.

Added base actor frame update 0x000420D0 in entities/actor_frame_update.cpp.
Native records expose the existing actor animation, attachment and anchor
lists plus eased position and frame clocks. The function invokes the existing
animation controller/pose routines, saves each virtual attachment's next
link before dispatch, refreshes effects/anchors, eases target coordinates,
advances model shader animations and applies the retail pause-decay and
clock branches. Checks cover callback mutation, freeze, blocked pause decay,
direct/eased coordinates and elapsed-time advancement after pause expiry.

Added button bat update 0x0006D5C0 in entities/button_bat_update.cpp, calling
the completed base update before setting its world position from display
position plus forward times five. Retail idle timeout selects hop and state
3; states 2/3/4 return to idle on animation completion. The constants and
animation names were checked through read-only Ghidra exports. Tests verify
camera-relative positioning, timer expiry and completion/reset behavior.

Release builds of gbtg and transition tests, actor-loading checks, all five
CTest checks and diff checks pass. These updates are available to the scene
factory but are not installed there yet. Widget construction/update, native
bat factory registration and 0x000584D0 activation remain outstanding; the
full opening scene and live spawning are still not accepted as working.

Completed widget construction 0x00141E20 and resource-owning wrapper
0x00141DA0 in frontend/dialogue_widget_construction.cpp. Construction maps
the actual untouched heap word into the native mode field; no initial mode
value is fabricated. It creates the text pool, copies layout/style/resource
fields, derives duration from glyph 0x41 (or the fallback texture), initializes
the sound name and invokes the existing prompt transition. The retail marker
comparison zero-extends a ushort before comparing with -1, so a 0xFFFF marker
with a non-null texture still supplies timing. Tests use explicit allocator
contents to verify both a normal transition and an already-3 mode guard;
these tests do not establish a deterministic retail heap initial value.

Completed widget update 0x00142460 in frontend/dialogue_widget_update.cpp.
It calls input activation, dispatches the mapped real button-bat update,
advances opening/closing progress, handles the retail button masks and
directional prompt path, requests button audio when enabled and invokes
mapped prompt code pointers. Closing waits for the text pool to empty before
transitioning. Tests cover freeze, progress clamping, input absence, button
edges versus held buttons, directions, callbacks, delayed closing and bat
frame dispatch.

Added bat attachment 0x00142860 in frontend/dialogue_widget_bat.cpp. It calls
the existing 0x0013B580 factory helper, resolves the native actor instance,
retains factory allocation ownership and unlinks the actor's +0x900 render
link. Creation uses the previous selected row before storing the new row
and scale. Tests exercise gold resource selection, the translated bat
initializer, prior-row coordinates, retained depth, render-list neighbor
repair and reuse without another factory allocation.

Release gbtg and transition builds, actor-loading checks, all five CTest
checks and diff checks pass. The factory test provides an explicit native
constructor fixture, not the complete retail scene registry or base actor
startup. These widget APIs still need scene-owned factory/resource/input
contexts, real prompt callback registration and 0x000584D0 activation/queue
wiring. The first playable scene's complete dialogue/cutscene/gameplay
sequence and live spawning remain unverified.

Cutscene sampling dependencies: 0x000514C0 now preserves the assembly's returned interpolation fraction, and 0x00051430 samples actor scale using each channel's duration and unsigned index clamp. Native relocated curve views preserve explicit retail key counts. Boundary tests cover negative times, single keys, terminal pairs and unequal scale channel durations. These functions are not yet connected to a complete native cutscene constructor; scene-owned contexts, prompt callbacks and activation/queue wiring remain outstanding.

Cutscene construction dependencies: native 0x0004D560 now allocates actor rows and applies the retail field writes without clearing untouched event/row storage. Native 0x0004DFA0 clones actor parameters, applies position/model/secondary fields, dispatches the existing object factory and releases the clone; failed clones stop dispatch. Native 0x000512D0 provides interpolated and discrete actor position sampling. Tests cover allocator-preserved bytes, camera index -1, clone ownership, conditional overrides, missing resources and independent position-channel durations. The complete 0x0004D680 loader remains pending: 0x0004DC70 actor initialization and 0x0004D7D0 camera setup require their real native contexts. Event preparation 0x0004C560 additionally reaches 0x0004C850; this dispatcher must be reconstructed before enabling scene activation. Scene-owned contexts and callback/queue wiring remain incomplete.

Native camera and actor track dependencies: 0x0004F770 camera position sampling, 0x00051590 quaternion interpolation (retains first-channel fraction), 0x0004F8C0 rotation sampling, 0x0004F990 strict aspect thresholds, 0x0004E0E0 instance transform composition and 0x0004D7D0 camera view update are implemented. 0x001535C0 and 0x00154500 use verified retail polynomial constants and Euler order table; existing quaternion interpolation/product are exposed without changing behavior. 0x0004DEC0 creates track actors with initial position/model substitution/secondary flags; 0x0004DC70 starts four-slot controllers, applies scale/pose, pauses slot zero and sends message 9, preserving failed-row writes and optional unlinking. 0x0004E950, 0x0004E980, 0x0004E780 and 0x0004E6D0 provide event actor lookup, state and pause operations. MinaPlayer pause is now an unsigned byte, preserving retail increment/decrement wraparound. Actor-loading tests exercise these native APIs with factory fixture bindings and a borrowed MINA resource; this does not establish live scene-owned registration or end-to-end acceptance. Full 0x0004D680 load/release, 0x0004C850 event dispatcher, audio pause/continuation helpers, 0x000584D0 activation and scene-owned callbacks/contexts remain outstanding.

Cutscene event wiring: 0x0004E640 now connects instance pause-reference counts to 0x0004E6D0 actor counters and 0x00039AA0 audio voice states, then resolves and invokes the retail indirect continuation callback at +160 with payload +164 when an unpause reaches count zero. 0x00039AA0 calls the existing 0x0015CDD0 cue pause function, whose native backend returns real XAudio2 Stop/Start results. 0x0004C420 connects all three cutscene voice handles to actual cue stopping and clears each handle; prepared and missing cues retain their distinct retirement states. Integration fixtures verify nested pauses, actor and audio propagation, continuation timing/arguments, ownership flag 0x10, missing cues/callbacks and repeated cleanup. These are native component connections, not end-to-end live scene activation. Remaining blockers are actual 0x0004C850 event dispatch, bank acquisition, end-event dispatch 0x0004D3E0 for 0x0004E070/0x0004D720, and scene-owned factory/widget/input registrations needed by 0x000584D0.

Dialogue completion audio: 0x0004C4F0 now releases the matching dialogue bank through 0x00038970 and replaces the requested stream through 0x0004D4C0. The checked no-sound name at DAT_00421960 is `XACT_SOUNDBANK_GMISC_NOSOUND`. Native bank/channel lookup, resume selection, channel retirement and stream request/release functions preserve the 20-bank, 16-channel and six-slot retail trackers. 0x0004D460 and 0x0004D490 provide conditional event stream/bank cleanup. Integration fixtures invoke the real completion implementation through the pause continuation map and verify final versus shared references, background resume, actual cue stop arguments, stream replacement, retained slot state/time, higher-priority and stream-bank selection, held/silent channel gates and the no-sound marker. This fixture does not install a live scene callback; full event dispatch and scene-owned contexts remain outstanding.

Dialogue bank acquisition: 0x00038910 and 0x0004D510 now acquire shared bank references and replace the event-owned bank through translated initialization/allocation/channel-start/priority-pause functions. Bank initialization checks `XACT_SILENCE` at DAT_002FD0C0, distinct from the completion callback's no-sound name. Channel acquisition uses the real 0x00039830 voice allocator and preserves its failure; bank channel assignment writes only the byte at +84. Tests verify acquire/release priority propagation, highest remaining bank resume, shared references, silent-bank handling, retained upper channel bytes, voice/channel/bank exhaustion and unchanged output arguments on allocation failure. Successful channel reuse uses a prepared-cue fixture; this is component validation, not retail scene startup acceptance. The remaining scene blockers are full 0x0004C850 event dispatch and end-event/load/release callers, 0x000584D0 activation, and scene-owned factory/resource/audio/widget/input registrations.

Startup event dependencies: 0x00039B30 positions a spatial voice relative to the real listener view through 0x0015C800; 0x00039510 requests a spatial voice, preserving zero-distance replacement with 5 and allocation failure. 0x0004C3B0/0x0004C4A0 implement cutscene voice start/end and slot retirement. 0x0004C6C0 selects start/end/intermediate/teardown phases with strict time boundaries, the retail one-entry policy tables (0xF and 0xE), and unchanged output on rejection. 0x0004C7B0 builds actor effect pose/binding options while preserving untouched fields. 0x001037F0 supplies the clamped range interpolation used by fog events. 0x00118840 advances the shared LCG and clamps camera impulse to [1/15, 1]. 0x000584A0 clones map-view control parameters, dispatches the actual object factory and frees the clone; its regression uses an explicit constructor fixture.

Release builds, the expanded actor-loading integration test, all five CTests and diff checks pass for this batch. The actual `--player-opening` check still fails: dialogue inactive and cutscene started early. The host scene path has not been replaced with a complete native startup path. Ghidra exports became unavailable during this batch because Z: was disconnected by a network outage, confirmed by the user. Cached decompiles remain available; missing downstream dumps must be recovered when the project returns. In particular, full 0x0004C850 dispatch still reaches movie/resource, dialogue enqueue/activation and seek dependencies; 0x000584D0 additionally needs complete cutscene load/release and input-stack lifecycle (0x001093C0 calls 0x001095B0/0x001078A0/0x00107EC0/0x00107C30). Existing host movie helpers contain untranslated calls and do not satisfy those reconstruction requirements. Scene-owned factory, resource, audio, widget, input and callback contexts remain required before acceptance of live scene startup.

Network recovery: the user confirmed the outage ended, and read-only exports from the same Ghidra project work again. The input-stack lifecycle dependency 0x001093C0 and all of its activation/reset callees are now translated in `frontend/input_activation`. It removes and shifts the selected retail stride-0x50 entry, activates the previous entry by its folded name and resets device kinds 1/2/5. Snapshot/copy functions retain their alignment branches and 0x3C state size. Controller/player mask conversions, raw controller selection and the invalid-port preceding-mask read are preserved. Integration tests cover previous-state snapshots, current controller mapping, stack reactivation/removal, final empty selection, missing-name results and unaligned copies. These context-based functions do not yet install the live dialogue input stack.

Additional dispatch leaves: 0x00038070 uses the reconstructed bounded CRT formatter and retail ASCII uppercase conversion. 0x001117A0 reads the navigation cell's surface override at +B, falling back to +A or zero for missing grids/cells. 0x0013DE80 applies the surface remap, formats the requested footstep cue and tests actual bank/cue lookup before formatting the default. Tests verify remapping, suffix order, available/missing cues and navigation override/fallback behavior. The expanded actor-loading integration test and Release builds pass. Full 0x0004C850 dispatch, 0x000584D0 dialogue activation and live scene-owned contexts/registrations remain outstanding; input-stack teardown and these audio/navigation leaf functions no longer block their parents.

Cutscene actor teardown: 0x001075C0 invokes the existing inherited factory destructor chain using the object's retail type word, frees its host heap storage and decrements the live object count. 0x0003EE70 reconnects the previous/next actor view pointers, including the native list sentinel, and clears both links. 0x0004E9E0 now connects these functions and 0x00112CC0 bind-tree reactivation to actor-row cleanup, then releases the row's event resource through 0x00035C00. Native context maps resolve actual factory objects, render registrations and resource pointers; no substitute destructor or activation callbacks are used. Tests cover owned and bound actors, resources without actors, repeated empty-row cleanup, sentinel unlinking and factory count/storage behavior. Release builds, actor-loading checks and all five CTests pass (`cache/cutscene_actor_release_build.log`). Read-only exports also confirm 0x00105AA0 and 0x00040DE0 already have native translations (`normalize_euler_angles` and `actor_refresh_transform`). Full event dispatch, dialogue activation and live registrations remain outstanding; this component teardown does not establish end-to-end scene startup.

Dialogue input destruction: 0x00108230 and its stack-argument counterpart 0x001081E0 now dispatch to the actual kind-specific release functions. 0x00107DD0 clears active+4 and frees simple devices; 0x00107960 clears active+4, recursively releases each nonzero child in four slots and frees the composite. 0x00107B60 either frees a detached device (slot -1) or clears its owner's +960 pointer and calls 0x00107A40 to free and clear the pool slot. Native address maps resolve the retail pointer words; heap calls use the existing real engine heap boundary. Unknown kinds retain the retail no-op. Integration fixtures exercise nested/composite devices, actor-owned slots, detached devices and unknown kinds. 0x0004E610 also copies both cutscene actor replacement names through their terminators, preserving untouched destination bytes; the dialogue bat/gold-bat names and empty replacements are verified. Release builds, actor-loading and all five CTests pass (`cache/dialogue_input_release_build.log`). These functions remove the input destruction and model-replacement leaves from the 0x000584D0 dependency list. Complete activation still requires full cutscene load/release and event dispatch, followed by live scene-owned contexts; startup remains unaccepted.

Movie error dependencies: 0x000E96A0 now searches the zero-code-terminated retail message table and preserves its title/message pointers; unknown codes use the verified DAT_00322D20 format (%d) through bounded variadic formatter 0x001663B9 with capacity 15. That wrapper uses the already translated identical stream/output/termination logic of 0x00166449. 0x001074A0 calls this lookup, invokes the real 0x00032700 resource error function and returns the original code if error handling returns. Tests cover matched rows, empty tables, signed fallback codes, untouched buffer tails and nonterminated truncation. Resource-error suppression is explicit in the returning test fixture; production error handling is not replaced with success. Release builds, actor-loading checks and all five CTests pass (cache/movie_error_dependencies_build.log). Movie open/load, full event dispatch, dialogue activation and live scene registrations remain incomplete. The export request for 0x00106FE0 did not produce a dump because Ghidra has no function defined at that address; further instruction-level recovery is needed for that callback.

Movie startup leaves: 0x00106EF0 now formats the PAL candidate through 0x00035830 and the real resource hash, probes file attributes, falls back to the standard path on any failed PAL probe, and validates the selected path again. Directory attributes produce 0x10B; missing attributes use the supplied XAPI last-error boundary. Final failures call translated 0x000E96A0 and 0x00032700; an explicitly suppressed error returns the path as in retail. 0x00106DD0 searches four event rows for matching ID/state 1 and either increments the node's unsigned play count (mode 2) and marks state 2, or marks state 3. It preserves missing-ID behavior and wraparound. Tests cover PAL success/revalidation, missing PAL fallback, override/directory failure, inactive matching IDs, count wraparound, other-mode retirement and missing IDs. Release builds, actor-loading integration checks, all five CTests and diff checks pass (cache/movie_startup_dependencies_build.log). IO callbacks in the fixture verify branches and call order; live scene-owned file/error contexts remain to be installed. 0x001071A0 movie allocation and 0x00107030 opening are still blocked by remaining library/file/asynchronous-read dependencies. Full cutscene dispatch and dialogue activation remain outstanding; this is not live scene startup acceptance.

Native startup checkpoint (2026-10-09): movie allocation/opening, completion, frame stepping and release now use translated file-worker operations and the existing FFmpeg Bink decoder. The native session reads a retail movie and decodes a 160x120 frame. Assembly identifies 0x0015D380 as allocator/free callback registration despite its misleading Ghidra D3D name. Its two stores are translated, and the decoder uses and releases the registered real heap allocation. Xbox file-cache sizing is retained as requested state; an equivalent host cache policy is not implemented. The retail file-worker handle is initially zero in BSS, so the translated synchronous branch is used until an actual worker is installed.

The full native cutscene event dispatcher, load/release and scene stack modules, dialogue activation/input lifecycle, page drawing and outer drawing are compiled. The outer drawing path includes prompt sprite queueing/flushing, actual indirect render-target callbacks, D3D target save/restore and borders. Camera setup now includes explicit/active camera selection, effects, view/projection construction and six-plane frustum generation. Both 0x001041B0 implementations preserve the assembly's floating-point addition order and alias handling. The native actor factory owner installs the known base/actor/button-bat lifecycle chain and shares actual class tags with its actor views.

Full 0x00129420 filtering and 0x001291A0 tag counting now preserve every mask branch and genuine predicate calls. `script_dialogue_actions` translates the complete 0x14 branch of 0x001274B0, including random suffixes, enqueue failure returning zero, one-shot flags and dialogue waiting. This native branch is not yet installed in the Game script caller. Its scene flags must alias the existing global, and live actor-list and timing views must refresh with the actual scene.

Release builds pass (`cache/scene_startup_camera_dialogue_build.log`, `cache/scene_startup_checks_build.log`). Actor-loading checks cover camera transforms/frustum planes, cancellation-sensitive matrix multiplication and both aliases, all actor filter masks, predicate counts, NaN comparisons, sprite records and target hooks. The real movie check verifies allocation and matching release through registered callbacks. All six CTests pass. These checks establish component behavior, not the complete first playable scene: the last live startup probe still skips the dialogue in the legacy type-0x14 caller (`cache/scene_live_startup_probe.log`). Live shared resource/audio/input contexts, actor/model rendering callbacks and their remaining lighting callees still require implementation and installation.

Actor rendering leaves: 0x0003DE00 now walks actual frame attachments, caches the next link before invoking each genuine vtable+14 draw callback, and passes both the actor and draw kind recovered from assembly. 0x00101930 compares packed colors; 0x00159210 retains the input float channels and uses the existing x87 truncation before storing their low bytes, preserving out-of-range wrapping and invalid conversion behavior. 0x0015B300/0x0015B320 copy the two environment matrices as raw words. Tests cover callback-driven list mutation, all four color bytes, unclamped/NaN float inputs, and bit-preserving/self matrix copies. Release game/transition/movie builds and both actor-loading and retail movie checks pass (`cache/scene_render_helpers_names_build.log`). Full actor drawing 0x0003C9E0 remains blocked by 0x000F9080 and its remaining lighting/environment callees, including 0x00158F20, 0x000FD850 and 0x000FCDF0. No live model-tree draw callback is installed yet.

Environment sampling and matrices: 0x000524A0 samples the retail X/Z RGB grid with x87 fractions, wrapped indices and the original clamp order; 0x001018A0 saturates packed color channels, 0x000FEE90 combines grids, and 0x000FD850 applies override, scale and the 32 point contributions. The existing 0x00101730 interpolation now uses the real 0x001673D8 conversion, preserving invalid and out-of-range results. 0x00153EB0 preserves opposite-direction quaternion branches and unordered comparisons; 0x000FCDF0 constructs both environment matrices from actual display and scene light directions. 0x0015ACA0 retains the world-space byte. Release builds and actor-loading checks pass (`cache/scene_environment_matrices_build.log`, `cache/scene_environment_matrices_checks.log`), including negative grid coordinates, saturated additions, last-slot point sampling, opposite directions and NaNs. The remaining 0x00158F20 shader upload and its 0x000FD2A0/0x000FD3F0 parents still prevent full actor draw installation. The live dialogue caller and scene-owned context graph remain outstanding.

Directional light state: 0x001564F0 uses the existing 0x00156DD0 state accessor, writes the actual +0x4A count and copies count*5 raw words into the +0xF8 directional records. Tests cover four records with NaN payload bits, zero count with no source access, and retaining records beyond a partial upload (`cache/scene_light_records_checks.log`). This completes a callee of 0x00158F20; the shader upload itself remains untranslated.

Shader light upload: 0x00170250 stores the four raw vector words, and 0x00158F20 now traverses the actual light list and uploads constants starting at 191/190, then descending from 189. It preserves directional negation, genuine D3DX transform/normalize boundaries, direct world-space position fourth lanes, spot +0x40 parameters, adjoining range words and exact type==1 selection of at most four directional records. Borrowed record storage retains the two unwritten flag bytes; the native scratch vector's fourth lane aliases the zero vector's first word. The tests exercise the no-matrix/no-world-space early exit, empty lists, ranged points, spots, local transforms, wrapping color conversion and five directional lights (`cache/scene_light_upload_checks.log`). Release game/transition/movie builds pass (`cache/scene_light_upload_build.log`).

0x000FD2A0 now connects fog, the display transform transposed for D3DX, light uploads, override-selected base color, sampled/clamped ambient color and both environment matrix snapshots. Native 0x000FD3F0 light construction and the model/actor draw parents remain outstanding. These APIs still require the actual scene-owned renderer, D3D boundary installation and shared global views; this does not establish live dialogue/cutscene/gameplay acceptance.

Scene light construction: 0x000FD3F0 now sets the scene color, clears the real light list, constructs the directional light with override-selected intensity and scans all forty local-light slots for at most two temporary point lights. It preserves type 0xE, half-radius range setup and packed RGBA conversion. Register 190 uses the actual mutable 0x0032E4E4 scene-color storage shared with 0x00159210, rather than its initial white value. Tests cover an empty list, holes, the last slot, the two-light limit, stale-list removal and this shared color alias. The native model draw wrapper 0x000F9080 and actor draw 0x0003C9E0 are now the next parents; live renderer callbacks and dialogue/context installation are still required.

Validation for this checkpoint: Release game, transition and movie builds pass (`cache/scene_light_construction_final_build.log`); actor-loading integration checks, shared-color assertions and the real retail movie allocation/completion/release check pass (`cache/scene_light_construction_final_checks.log`, `cache/scene_light_construction_movie_checks.log`). Diff checks pass. Full live scene startup has not yet been accepted.

Actor/model drawing parents: 0x000F9080 now selects the actual model header bit-1 lighting branch, derives the position from world translation, calls native scene light/environment setup, applies materials and draws the model instance. 0x0003C9E0 now preserves hidden/model/header gates, draws attachments before model filtering, refreshes shader matrices through 0x00040200, writes the model material overrides and invokes genuine pre/post callbacks around model drawing. The model override words are borrowed references to the native instance fields; the draw context uses the existing relocated CPU image and model tree. Tests join the entire chain through a fixture model-tree callback, verify callback-updated world translation, payload/order, visibility flags, lit/unlit behavior, white scene-color reset and retained disabled override fields. Release builds pass (`cache/scene_actor_draw_build.log`, `cache/scene_actor_draw_final_build.log`), actor-loading and real movie checks pass (`cache/scene_actor_draw_final_checks.log`, `cache/scene_actor_draw_movie_checks.log`). This callback fixture does not install the live model-tree renderer. Live actor render ownership, the native dialogue caller and the shared resource/audio/input context graph remain outstanding.

Native model-tree callback wiring: 0x00158A80, 0x00158C00, 0x001589F0, 0x00158900 and 0x00156E40/0x00156E70 now provide matrix preparation, render-state setup, node descriptor lookup and the actual sentinel stack traversal. 0x00158960 queues group siblings before children for depth-first traversal. The installers bind renderer.state.draw to the retail tree dispatcher and register the original group callback address. Integration checks run actor -> model -> installed tree dispatch, preserve the instance-storage argument recovered from assembly, exercise node-mask filtering and siblings, and verify group child/sibling order and stack unwinding. Release builds, actor-loading checks and retail movie checks pass (`cache/scene_model_tree_wiring_final_build.log`, `cache/scene_model_tree_wiring_final_checks.log`, `cache/scene_model_tree_wiring_movie_checks.log`).

This installs the native callback in the integration context, not the live Game renderer. The Game frame loop still draws parsed meshes directly; its native actor/context ownership must be connected after the remaining node handlers and real D3D bindings exist. The next concrete draw dependency is ndPushBuffer 0x001582A0 and its visibility test 0x00158240. The dialogue caller still uses the legacy type-0x14 branch. No full live scene acceptance is claimed.

Push-buffer handlers: 0x00158240 now preserves the signed visibility byte, -1 bypass, signed byte offset and x86 shift masking against the actual model flag origin. 0x001582A0 issues the original primitive/count/index arguments, temporarily disables culling for double-sided nodes, restores the retained cull mode and always queues native child/sibling traversal. Its installer registers the original 0x001582A0 callback address. Tests cover packed flag mismatch, negative offsets/shifts, bypass, indexed argument order and invisible-node sibling continuation. Release game/transition/movie builds and actor-loading/movie checks pass (`cache/scene_push_buffer_build.log`, `cache/scene_push_buffer_checks.log`, `cache/scene_push_buffer_movie_checks.log`). D3D calls in these tests are boundary fixtures. Actual live D3D resource/shader bindings, the remaining node handlers, scene-owned renderer contexts and native dialogue installation remain outstanding.

Additional node registrations: 0x00158430/0x00158480 retain vertex-stream descriptor addresses using the retail 0x18 stride, then traverse the group links. 0x00158090 is the verified tail jump to group traversal used by visibility/pixel-parameter registrations. 0x001586D0 selects the retained pixel shader, issues the raw 0x40354/0x203 depth command, retains that result before enabling depth, issues 0x40304/0 to disable blending and continues traversal. The installer binds all three original callback addresses. Release game/transition/movie builds, actor-loading checks and movie checks pass (`cache/scene_render_nodes_build.log`, `cache/scene_render_nodes_checks.log`, `cache/scene_render_nodes_movie_checks.log`). Checks cover zero/partial stream counts, untouched slots, exact D3D call order and sibling continuation. Vertex-shader handler 0x00158510 and its 0x001596D0 shader-selection dependency remain untranslated; real shader creation/resource bindings and live scene/dialogue ownership still prevent full Game installation.

Shader-selection dependencies: 0x00159390 returns the actual lighting flags, and 0x00158EB0 searches the 239 fragment buckets with the complete unsigned key, preserves collision traversal and leaves the output untouched on a miss. 0x0018850C classifies retail shader signatures, and 0x00186FD5 unpacks instruction bits while preserving output-clear order and alias effects. Release builds, actor-loading/movie checks and diff checks pass (`cache/scene_shader_code_build.log`, `cache/scene_shader_code_checks.log`, `cache/scene_shader_code_movie_checks.log`).

The cache-miss chain 0x001596D0 -> 0x00159530 -> XGSpliceVertexShaders 0x0018D2F4 remains blocked by optimizer 0x0018C563 and its native callees, now exported for reconstruction. Shader cache +0x904..+0x930 contains six handle/key pairs; +0x934 is their count, not a shader handle (the existing host cache's `handle` field names this count). The full-cache branch returns the oldest handle at +0x92C without creating another shader. No host shader-splice boundary or invented success result has been added. Vertex-shader node registration and live Game ownership remain incomplete.

Native shader optimizer helper checkpoint: the completed batch now includes scheduling tail cost 0x00185750; channel-mask decoding/construction and tree destruction/reset/lookup 0x0018E46F, 0x0018F2FC, 0x0018F258, 0x00190C93 and 0x001900DE; pair decoding/compatibility/dependency/merging/compaction 0x00187E9B, 0x0018735A, 0x001873C8, 0x0018731B, 0x0018A506, 0x0018A8DD, 0x00187406, 0x0018ACFB and 0x0018AD2D; and eleven operand/destination helpers in shader_merge_optimizer.cpp. Assembly resolves the move merger's retained EDX argument, which the decompiler misleadingly presents as a 64-bit return. The shader cache field at +0x934 is now named variant_count, with layout assertions preserving the native offsets.

Release game, transition, movie and relocation builds pass (cache/scene_shader_optimizer_build.log). Actor-loading integration checks, including rejected merges, single-pass compaction, operand comparisons, destination conflicts, mask channel order, callback-mutated tree traversal and NaN scheduling comparisons, pass (cache/scene_shader_optimizer_checks.log). Model relocation and the real retail movie allocation/completion/release checks also pass (cache/scene_shader_optimizer_relocation_checks.log, cache/scene_shader_optimizer_movie_checks.log).

The larger optimizer remains blocked by swizzle decode/encode 0x00187671/0x0018784D/0x00187A01 through 0x00188032, plus the validator/compiler dependencies of 0x0019723C and remaining scheduling passes. Consequently XGSpliceVertexShaders, shader variant creation/selection and vertex-shader node registration remain incomplete. Live Game dialogue and native actor/resource/context ownership remain outstanding; these passing helper and integration checks do not establish full live scene startup.

Shader swizzle and instruction merge checkpoint: 0x00187584 decodes operand channel masks using the four actual retail tables at 0x001A22D0/0x001A2300/0x001A2330/0x001A2348. 0x00187621/0x00187671 decode masked swizzles; 0x001877CE/0x0018780B/0x00187833 select the native defaults and replication fallback; 0x0018784D encodes the three operands sequentially; and 0x00187A01 merges compatible channels without touching output on a conflict. 0x00187F88 merges destination descriptors, and 0x00188032 merges instructions with original partial writes and error codes 0x7B/0x7C/0x7D/0x7E. Assembly resolves the decompiler's misleading extra-register returns and the combined scalar/vector destination restrictions.

0x00187057 move-to-scalar conversion now accepts caller-owned output and preserves its retained bits and exact alias reads. Tests demonstrate different results for zero-filled, all-ones and self-aliased output; rejected opcodes leave output unchanged. The retry wrapper 0x00188348 remains untranslated because its actual retained stack scratch state must be represented, rather than replaced with invented zero initialization. Consequently 0x0018A86A and its optimizer parents remain outstanding, alongside validation and scheduling dependencies.

Release game and transition builds pass (cache/scene_shader_swizzle_final_build.log). Actor-loading integration checks pass (cache/scene_shader_swizzle_checks.log), including all 256 active operand swizzles, all sixteen channel masks, inactive operand defaults, signed fallback bytes, conflict output retention, each merge error code and conversion alias behavior. Diff checks pass. Live scene startup is still incomplete: shader creation/selection, real D3D bindings, native dialogue caller and scene-owned actor/resource/context installation remain required.

Shader retry and merge-run checkpoint: 0x00188348 now attempts the direct instruction merger, then move-to-scalar conversion of the first and second inputs in retail order. Its conversion words are explicit borrowed storage for EBP-0xC..EBP-0x4; no initialization is invented. The initial call writes the caller error, while fallback errors write the native local argument slot and are discarded. Conversion scratch persists between fallback attempts and repeated calls. 0x0018A86A applies the original dependency gate, retries merging, then tries the first multiply-operand swap or, only if unavailable, the second swap. 0x0018ADFB compacts full compatible runs with the original signed count checks and in-place output order.

Release game/transition builds pass (cache/scene_shader_retry_build.log, cache/scene_shader_retry_final_build.log). Actor-loading integration checks pass (cache/scene_shader_retry_checks.log), covering successful conversion with retained initial error, failed retries retaining scratch, full-run compaction, rejection followed by another mergeable run, negative counts and register dependency rejection without output/scratch writes. Diff checks pass.

These functions require an actual owner of the retained retail stack region before live shader compilation can use them; supplied fixture storage does not establish the original initial stack contents. Broader merge dispatcher 0x0018ADAF still needs 0x00187B22 and 0x0018AB16, so 0x0018AE8A remains untranslated. Validation/scheduling passes, XGSpliceVertexShaders, real shader/D3D resource wiring, native dialogue caller and live scene ownership remain outstanding.

Full shader merge-pattern checkpoint: 0x00187B22 fuses vector multiply/add instructions after checking matching output masks/destinations, exactly one referencing operand, identity swizzle and shared vertex/constant fields. It preserves both operand orientations and forward copy/read-after-write behavior. 0x0018AB16 merges matching operations with all three operand comparisons, dependency and destination restrictions, swizzle merging, and the original partial-copy behavior on a swizzle failure. 0x0018ADAF now dispatches the four retail patterns in order; 0x0018AE8A compacts full compatible runs through that dispatcher with native signed count checks.

Release game and transition builds pass (cache/scene_shader_patterns_build.log). Actor-loading integration checks pass (cache/scene_shader_patterns_checks.log), including both multiply/add operand orientations, identity-swizzle rejection with retained output, complementary output masks, destination rejection before copying, swizzle rejection after copying, dispatcher fallthrough to fusion and compaction chaining fusion with a later scalar retry. Diff checks pass.

This completes the merge-pattern dependencies previously blocking 0x0018AE8A. Top-level optimizer 0x0018C563 still needs 0x0018C277/0x0018C3F6/0x0018C1B9/0x0018BF69/0x0018BB40 and validator 0x0019723C with their untranslated callees. Retained retry scratch still requires a real owning context. Shader creation/selection and live scene dialogue/actor/resource/D3D wiring remain incomplete.

Scheduling initialization and validator arena checkpoint: 0x00185807/0x0018586A initialize register and issue tables with exact raw offsets, thirteen rows, actual constants and retained gaps. 0x00189462 resets pending bytes, register slots and issued time; 0x0018B1D3 enables only low-byte mode one and forwards the actual initializer return retained in EAX. 0x001858E5 advances all fourteen issue rows and reasons, preserving the final-row aliases that overwrite its initial +0xD0 and +0x450 stores. 0x001857E5 selects the real vector latency table, 0x0018582C writes selected register channels and wraps cycle addition, and 0x00187718/0x00187760 decode channel unions with native bitfields and x86 shift masking.

Validator arena helpers 0x0018F0C2/0x0018F119 recursively clear parent links and call actual deleting callbacks; 0x001906A1/0x001906F1 reset arena storage after destruction. 0x0018F0EB advances using the second subtree, first-child descent and parent links. 0x0018F142 updates list endpoints, invokes deletion and decrements count, preserving native null/count wrapping behavior and leaving neighbour relinking to the deleting callback.

Release game and transition builds pass (cache/scene_shader_schedule_build.log). Actor-loading integration checks pass (cache/scene_shader_schedule_checks.log): all sixteen channel masks and latency entries, all initialized/advanced rows, untouched gaps, raw NaN payload transfers, wrapping arithmetic, both arena reset callbacks observing retained storage, callback-mutated child traversal, iterator branches, list endpoints and count wrapping. Diff checks pass.

Cost parent 0x0018BF95 still needs 0x0018B9C9/0x0018BA2D/0x0018BA95 through 0x0018B1EE and 0x0018BE49/0x0018BEA8/0x0018BF0B through 0x0018BAF8. Update parents 0x00189499/0x001895E1 and 0x0018C0DB remain untranslated; full issue updates 0x00185961 require 0x001856D5 and exact floating-point branches/constants. Their evidence is exported. Top-level optimization/validation, retained scratch ownership, real shader/D3D bindings and live dialogue/actor/resource/context installation remain incomplete.

Hazard-cost and issue-update checkpoint: 0x001856D5 preserves strict float-near bounds and unordered comparisons; 0x00185961 implements the full issue update branches, actual constants/status banks, channel selection and special register thirteen behavior. 0x00189499/0x001895E1 record register/issue writes with actual failure returns and write ordering, including pending-byte aliases with native scheduler metadata. Hazard chain 0x00185F25/0x00189723/0x001872C1/0x001897A3/0x0018B243/0x0018B1EE/0x0018BAF8 now implements channel cost selection, vector removal, producer bypass checks, constant-bank readiness and genuine three-/six-cycle comparisons.

The six operand wrappers 0x0018B9C9/0x0018BA2D/0x0018BA95/0x0018BE49/0x0018BEA8/0x0018BF0B connect those helpers. 0x0018BF95 estimates enabled hazards or scans disabled scheduling delays through the retail five-attempt limit; 0x0018C0DB issues instructions, preserving truncation, threshold comparisons, table advances, cost storage and failure results. 0x0018C277 estimates complete program cost including the native tail, and 0x0018C1B9 selects cheaper multiply operand order. Their scheduler frame bytes are explicit borrowed storage, retaining gaps rather than inventing initialization.

Release game and transition builds pass (cache/scene_shader_hazards_build.log, cache/scene_shader_hazards_final_build.log). Actor-loading integration checks pass (cache/scene_shader_hazards_checks.log), covering strict near bounds/NaNs, option-ten versus eleven special-register timing, unit latency banks, upper-mask rejection, truncated cycles, invalid destinations that still write, constant readiness banks, equal-cost reason replacement, unordered channel skipping, three-cycle bypass and six-cycle readiness boundaries. Complete program tests check enabled/disabled costs, untouched caller output on recording failure in both modes, retained multiply order on equal cost, and the native pending-byte/mode alias. Diff checks pass.

These 21 completed functions close the cost-estimation and multiply-order dependencies of top-level optimizer 0x0018C563. Remaining optimizer parents 0x0018C3F6, 0x0018BF69 and 0x0018BB40 still require their native rewrite/storage chains, and validator 0x0019723C remains incomplete. Retained scratch/frame ownership, XGSpliceVertexShaders, shader creation/selection, real D3D bindings and live dialogue/actor/resource/context installation remain outstanding. This checkpoint does not establish full live scene startup.

Rewrite-storage and validator cleanup checkpoint: 0x00186679 initializes raw rewrite storage with exact option banks and retained bytes at +0x201..+0x203. 0x001862A8/0x00186394 decode and apply vector/scalar destinations, including opcode thirteen, output sentinels and removal of empty operations. 0x0018689E moves three-word instruction records; 0x00186913 replaces pairs with native forward-copy alias behavior, compaction and retained trailing words. 0x001869EC scans signed instruction counts for register-one rewrites, and 0x0018C329 tests genuine instruction hazard cost. 0x00190CC1 clears expression/instruction arenas in order and repeatedly reloads the owned-object tree after actual deleting callbacks; callbacks retain responsibility for genuine tree/count mutation.

0x0018C1B9 now returns the last native issue/configuration result held in EAX, including recording failures, despite its decompiled void signature. Release game and transition builds pass (cache/scene_shader_rewrite_build.log). Actor-loading integration checks pass (cache/scene_shader_rewrite_checks.log), including instruction movement in both directions, replacement alias rereads, signed counts, raw storage gaps, destination round trips/removal, special opcode masks, cleanup ordering and root replacement.

These eight functions are dependency work, not live scene acceptance. 0x0018C3F6 still requires 0x0018C358 and 0x0018B47B; other optimizer parents require their remaining output predicates/context mutators, and validator 0x0019723C remains incomplete. Real scratch ownership, XGSpliceVertexShaders, shader creation/selection, D3D binding and live dialogue/actor/resource/context installation remain outstanding.

Write-conflict and identity-output checkpoint: 0x001866C9 clears exactly bytes +0..+0xDE, retaining +0xDF; 0x00186724 accumulates register/constant/output writes with combined scalar register-one selection, replacement of individual output masks and native partial writes on failure. 0x001866F8/0x00189C00 compare the three actual mask banks and address marker. 0x00186806 decodes selected swizzle channels with x86 byte shifts and checks direct/relative constant reads. Operand wrappers 0x00189C62/0x00189CB3/0x00189D06 preserve boolean AL returns despite void decompiler signatures. 0x00189D56/0x00189EAD select native opcode masks, retaining output bytes on no-op/error; 0x0018B384 combines actual vector/scalar/relative conflict results.

Identity predicates 0x00185F5F/0x00185FDF/0x0018607B/0x00186120 check opcode, source/destination, negation and only active swizzle channels. 0x001861BB removes identity writes sequentially; 0x00186201 tests redundant outputs without accepting empty masks merely because they are empty. Release game and transition builds pass (cache/scene_shader_conflicts_build.log, cache/scene_shader_conflicts_final_build.log). Actor-loading integration checks pass (cache/scene_shader_conflicts_checks.log), covering each bank boundary, padding retention, partial failure writes, every vector opcode, inactive-channel filtering, relative reads, and dual identity removal.

These 17 functions close the leaf chains needed by reorder searches 0x0018B403/0x0018B47B/0x0018C358, which remain to be translated before 0x0018C3F6. Other optimizer/validator parents, retained-frame ownership, shader/D3D binding and live scene installation remain incomplete; this is not live startup acceptance.

Scene startup continuation: reorder searches 0x0018B403/0x0018B47B/0x0018C358 and parent 0x0018C3F6 now use genuine write/read conflicts, pair merging and scheduler costs, retaining the retail retry pass and error returns. Rewrite context chain 0x001864C4/0x00186520/0x0018657E/0x001865DB/0x00186617/0x00189ADC/0x00189B3F/0x00189BA2/0x00189858/0x0018B299 is complete. Parent 0x0018BB40 uses real heap allocation, backward rewrite and reversal into the caller program only on success. Validator leaves 0x0018E57C/0x0018E917/0x0019018D/0x00190271 initialize channel order, copy masks and construct raw nodes with genuine retail pointer/vtable words.

Release game/transition builds pass (cache/scene_shader_reorder_build.log, cache/scene_shader_dead_writes_build.log, cache/scene_shader_dead_writes_final_build.log). Actor-loading integration checks pass (cache/scene_shader_reorder_checks.log, cache/scene_shader_dead_writes_checks.log), covering safe candidate search, read/write conflict rejection, actual reorder compaction, rewrite retention/suppression, opcode failure before append, backward overwritten-write removal, original-program retention on failure and validator constructor/copy semantics.

Live player-opening acceptance was rerun and still FAILS (cache/scene_live_startup_current.log): dialogue=0, cutscene before A=1, actors=1, entities=1. Game still executes the legacy type0x14 branch rather than execute_runtime_script_dialogue with real owned contexts. The existing full dialogue handler requires real scene-owned widget/input/audio/cutscene/actor resources before installation. Shader parent0x0018BF69 remains blocked by register-one rewrite0x0018BD12 through0x0018B6CC/0x0018B8D0/0x0018A38E. Validator0x0019723C still needs tree insertion/balancing and exception dependencies. Native context/actor/resource/D3D ownership and live startup remain incomplete; dependency checks do not establish acceptance.

Register mapping and validator insertion checkpoint: 0x00186D09 records native scalar/vector lifetime flags; 0x00186D67 tests fixed channel occupancy or flexible available channel counts. 0x00186F20 releases channels using the last nonzero reference lifetime and actual expiry index. 0x00186F9D remaps masks through lifetime channel mappings; 0x0018A38E updates operand and destination fields using genuine reference/value/lifetime records. Host pointers are explicit owners; raw 28/10/18-byte table records retain native offsets.

Validator balancing 0x001904C3/0x001904FB/0x00190575/0x001905C0/0x00190B27/0x001917D9 preserves signed height comparisons, wrapping arithmetic, independent parent-child comparisons, cached parent traversal, single/double rotations and root restoration. Insertion 0x00191FBB/0x001921BB uses actual heap results, unsigned key ordering, duplicate reuse, native null-node INT3, count increments even on allocation failure, and genuine balancing. Typed overloads of 0x0019018D/0x00190271 preserve constructor store order while expanding host pointers.

Release game and transition builds pass (cache/scene_shader_register_tree_build.log). Actor-loading integration checks pass (cache/scene_shader_register_tree_checks.log), including reorder recording failures, empty-program retention, mask remapping, fixed/flexible channel availability, last-reference channel release, actual instruction mapping, single/double rotations, height overflow, real heap insertion, duplicate reuse, unsigned keys and failed-allocation count wrapping. Diff checks pass.

Remaining register-one rewrite parents still require lifetime-list merging/allocation through 0x00186A7B/0x00186DE0/0x00186E96/0x0018A24E/0x0018A302 before 0x0018B6CC/0x0018B8D0/0x0018BD12/0x0018BF69 can be completed. Full validator 0x0019723C still requires registration and validation-pass/exception dependencies. Real scene-owned dialogue/input/audio/cutscene/actor/resource/D3D contexts remain uninstalled; latest live acceptance remains the failing cache/scene_live_startup_current.log, not these passing dependency checks.

Register allocator and approved exception checkpoint: register chain 0x00186A7B/0x00186DE0/0x00186E96/0x0018A24E/0x0018A302/0x0018B6CC/0x0018B8D0/0x0018BD12/0x0018BF69 is complete. It merges sorted lifetime lists, clears merged-away records, preserves earlier channel writes on conflict, uses exact DAT_001A2410 candidate bytes {0,2,3,4,5,6,7,8,9,10,11}, collects packed instruction/value/lifetime tables, allocates genuine heap storage and frees/nulls all three tables on every native exit. Assembly confirms 0x0018BD12 returns retained collection result one when later register allocation fails, skipping remapping rather than returning the later failure.

Validator wrappers 0x00192655/0x00192713/0x001928DE register initial/existing object and diagnostic nodes with native count/store order and supplied native identity mappings. The user explicitly approved real host exception dispatch this turn, extending the previously approved host boundaries for the actual retail RaiseException calls. 0x001679C7 copies the eight-word DAT_0020910C record and dispatches through real Windows RaiseException with full-width object/type-info pointers. 0x0018E935 supplies actual operand byte 0x21 and mapped throw-info. Raw retail throw-info remains explicitly distinguished from host compiler catch metadata; no typed-catch compatibility or fake C++ throw is claimed. Non-Windows exception dispatch is unavailable rather than silently succeeding.

Release game and transition builds pass (cache/scene_shader_allocator_exception_build.log, cache/scene_shader_allocator_exception_final_build.log, cache/scene_shader_allocator_merge_build.log). Actor-loading integration checks pass (cache/scene_shader_allocator_exception_checks.log), including sorted lifetime merging, exact candidate order, partial conflict writes, linked program remapping, thirteen full-write register exhaustion retaining result one and original program, heap-table cleanup, wrapper skipping, registration allocation order/duplicates/failure, and genuine structured exception delivery. Exception checks inspect live byte 0x21 before unwind and validate code E06D7363, non-continuable flag, count three, magic 19930520 and full-width pointers. Windows adds EXCEPTION_SOFTWARE_ORIGINATE 0x80, confirmed in the installed SDK; tests account for that actual host flag without changing dispatch behavior.

The register-one dependency of optimizer0x0018C563 is now closed. Validator constructors0x00192934/0x00192A54 still require genuine unwind cleanup (funclets0x00170A1F/0x00170886) and metadata mapping before full validator0x0019723C can be accepted. Its remaining validation-pass callees and live dialogue/input/audio/cutscene/actor/resource/D3D context ownership are still outstanding. Latest live acceptance remains failing cache/scene_live_startup_current.log; the completed dependency checks do not establish dialogue-to-gameplay startup.

### Validator cleanup, traversal and unregister integration (2026-10-09)

Integrated fifteen translated dependencies: 0x00190148, 0x001902BA,
0x0019038F, 0x0018EFC4, 0x0018E5BC, 0x0018E43B, 0x0018F281,
0x001901D6, 0x001909A6, 0x0018F0AC, 0x00190234, 0x0019158A,
0x00191F4A, 0x0019269C and 0x001928F6.

The key lookup uses the balanced-node layout (parent +0x08, children
+0x0c/+0x10); pass iteration retains its distinct parent +0x0c and children
+0x10/+0x14 layout. Cleanup tests exercise callbacks that change child links,
owner reset timing, real allocation release, and reserved-register lookahead.
Unregister tests cover missing keys, null erase, joined tree publication before
actual deletion, count decrement after deletion, and base-vtable installation.
Exception checks also verify delivery of modified caller-supplied record fields.

Release gbtg and gbtg-transition-tests build passed:
cache/scene_shader_erase_build.log. Actor-loading dependency checks passed:
cache/scene_shader_erase_checks.log. Earlier cleanup/traversal checks are in
cache/scene_shader_unwind_leaves_checks.log.

This closes direct unregister/cleanup dependencies for constructor unwind;
the constructors 0x00192934/0x00192A54 and full validator dispatch 0x0019723C
remain outstanding. The real scene startup probe remains failing as previously
recorded: these dependency tests do not establish live dialogue/cutscene/gameplay
acceptance, and no replacement success values or partial native owners were added.

### Validator constructors and operand helpers (2026-10-09)

Integrated constructors 0x00192934 and 0x00192A54 with actual Windows SEH
finally blocks and the retail unwind-state tables. The owner diagnostic registry
now shares one balanced-node tree with its cleanup view; three typed cleanup
overloads preserve parent clearing, fresh child reads, deleting callbacks, and
owner reset timing. Six unwind funclets are translated from their cached dumps.
No global /EHa setting or synthetic exception return was introduced.

Also integrated operand-tree constructor/insertion/registration 0x001901EB,
0x001920BB and 0x001926B4, plus channel packing/order helpers 0x0018E442,
0x0018E7D3 and 0x0018F3B5. The operand tree retains unsigned comparisons,
duplicate behavior, genuine allocation failure, and count increments even when
allocation fails. Channel helpers retain nonzero-byte mask tests, sequential
copies, and alias-safe missing-channel completion.

Release gbtg and gbtg-transition-tests build passed:
cache/scene_shader_constructors_build.log. The --actor-loading suite invokes
check_actor_loading -> check_scene_render -> check_shader_compiler_leaves ->
check_shader_rewrite_dependencies and the new constructor checks; passed log:
cache/scene_shader_constructors_checks.log. Tests cover normal kind-five mask
replacement, active-parent fallback, null allocation, actual invalid-operand SEH,
nested registration SEH with allocation release before owner unregister, and
allocation SEH before state three. The latter preserves the retail state-zero
behavior of retaining the base registration; the fixture explicitly releases it
later. Shared diagnostic roots are never copied into an independent cleanup tree.

Full validator 0x0019723C remains blocked on its real pass constructor/preparation
and virtual dispatch chains (0x00196739, 0x00197223, 0x001973F1/0x00197A94 and
serialization). Serializer 0x0018F8B7 still needs 0x0018E7F6 through
0x0018F5D7 and genuine retained scratch inputs. Live dialogue/cutscene/gameplay
acceptance remains outstanding; this checkpoint does not change the real scene
probe failure documented above.

### Validator serializer, accessors and channel helpers (2026-10-09)

Integrated the instruction serializer chain: 0x0018F8B7 packs one validator
instruction into three microcode words, and 0x001914E8 walks the pass
instruction list, appends those words after a count and calls the existing
reserved-register pass 0x0018EFC4. Its callees 0x0018E7F6 (order assignment,
retaining the destination vtable), 0x0018F5D7 (operand order replacement with
its by-value argument and returned copy), 0x0018E973 (parent of an owner's
operand unless it is the active owner DAT_001a4d34) and 0x0018E816
(destination index, 0xF when absent) are translated, with a typed operand
overload of 0x0018E7D3. `ShaderValidationInstruction` records the 0x4C-byte
object built by 0x00192DF1.

The serializer reads three frame words before writing them. Tracing every
mask shows exactly which retained bits reach the output: word 0 bits 0, 30
and 31, and word 1 bits 30 and 31. All other bits are written. The words are
an explicit caller-owned `ShaderValidationSerializerFrame`; retail leaves the
packed words in those slots, so consecutive calls from 0x001914E8 consume the
previous instruction's bits. No initial value is invented. Before packing,
each operand's missing channels are filled in place through 0x0018F3B5.

Also integrated seventeen accessor helpers in `shader_validation_accessors`:
mask/order constructors 0x0018E414/0x0018E595; order queries
0x0018E5C3/0x0018E67C/0x0018E6BE; instruction mask getters
0x0018E833/0x0018E84E/0x0018E869; per-channel unit tests 0x0018E884/0x0018E8A5
(the output flag selects whether the output mask belongs to the vector or
scalar unit); opcode classes 0x0018E8E4/0x0018E8C6 (signed 5..7 range)
/0x0018E8F8; owner links 0x0018E76F/0x0018F4F7 and the channel-owner search
0x0018E7AF; and signed pass-tree lookup 0x00190370, which pops an unused
second argument. Owner fields +0x30/+0x34 are now the typed `derived_owner`
and `channel_owner` links.

Eleven channel-order helpers are in `shader_validation_channel_ops`:
0x0018E6DF, 0x0018F394, 0x0018E5E2, 0x0018E69C, 0x0018E741 (by-value order,
RET 0x14), 0x0018E548, 0x0018E604, 0x0018E63F, 0x0018E661, 0x0018E700 and
0x0018F355.

Verification: each of these 34 functions was compared against the retail
machine code executed in a CPU emulator (`tools/oracle/`, Unicorn running
bytes exported from the user's XBE into `cache/oracle/`). Randomized object
graphs, retained frames, junk channel values and signed boundaries gave zero
mismatches across several thousand cases per function, and deliberately
mutated builds were detected. The fixed vectors in
`tests/shader_validation_serializer_tests.cpp` come from that emulator and run
in the `--actor-loading` chain through `check_shader_rewrite_dependencies`.
These modules and tests were compiled and run with g++ on Linux; the Windows
Release build of `gbtg-transition-tests` and its `--actor-loading` run also
pass with them (confirmed by the user on 2026-10-09).

`tools/ghidra/ghidra_scripts/CalleeTree.java` now inventories untranslated
callees read-only. From 0x0019723C (plus its vtables at 0x00203820 and
0x002037D0) 173 functions (about 31 KB) remain. The remaining leaves include
0x00190B12, 0x00190CA6, 0x001909B1, 0x0019149A, 0x0018F314, 0x0018F1E5,
0x001902D9, 0x001903E1, 0x0019042A, 0x00190473, 0x001929B7, 0x0019051C, 0x00190337,
0x0018F595, 0x0018F86B, 0x0018F4A9, 0x0018F436, 0x0018E988, 0x0018ED95,
0x0018E4EB, 0x0018E48C, 0x0018FF4B, 0x0018FFC9, 0x0018E9DC and 0x0018EBEA.
0x0018E94C orders instructions by +0x40 and then by object address, so it
needs the retail address map rather than host pointer order. Pass constructor
0x00196739, 0x00197223 and dispatch 0x001973F1 remain the large parents. The
live scene probe is unchanged; this checkpoint does not establish dialogue,
cutscene or gameplay startup.

### Validator containers, register tables and instruction predicates (2026-10-09)

Thirty-two further validator functions are translated, each compared against
the retail code in the emulator (`tools/oracle/run.sh`) with zero mismatches
and with deliberately mutated builds detected.

- `shader_validation_containers`: balanced-node initializers 0x001902D9,
  0x001903E1 and 0x0019042A; pass-node initializer 0x00190473 and height
  0x0019051C; predecessor iterator 0x00190337; pass lookup wrapper 0x00190B12;
  last tree value 0x00190CA6; list destructor 0x001909B1, node constructor
  0x0018F1E5 and append 0x0019010E; single-channel mask 0x0018F314; and the
  instruction base constructor 0x001929B7, which registers the object through
  the existing 0x001928DE path.
- `shader_validation_registers`: pass-tree height update, rotations, balancing,
  rebalance and subtree join (0x00190554, 0x0019060B, 0x00190656, 0x00190BDD,
  0x00191990, 0x001919BD); register-table lookups 0x00191E8F and 0x00191F00
  and the kind-one chain test 0x00192629; owner equivalence 0x0018F4A9.
- `shader_validation_channel_ops`: owner channel mask 0x0018F595, operand
  source mask 0x0018F86B, order composition 0x0018F436, and mask
  intersection/union 0x0018E4EB/0x0018E48C.
- `shader_validation_instruction_ops`: slot use 0x0018E988, per-channel reads
  0x0018E9DC (translated from the assembly; the decompiler output for this
  switch is not reliable), move negation 0x0018ED95 and scalar-to-vector move
  conversion 0x0018FF4B.

Layout findings: the pass node's +0x08 word (now `data`) holds the newest
`ShaderValidationOwner` of a register, and +0x18 is its height. The validator
pass object holds a register table of kinds 0..12 at +0x28 and a 0x100-keyed
table at +0x3C (`ShaderValidationRegisterTables`). The list object keeps its
first node at +0x04 and last at +0x08; 0x0019010E appends at the last node,
and 0x001909B1 deletes from it. 0x0018F142 leaves neighbour relinking to the
deleting destructor, so after 0x001909B1 the first link can still name a
deleted node, exactly as in retail. A failed 0x0019010E allocation still
clears the last link and increments the count.

Several retail paths dereference null when given inputs the callers never
produce: 0x00190337 on the first node of a tree, 0x00190BDD with an
inconsistent height, 0x00192629 with a null owner, and channel indices above
four in the order/mask helpers. These paths are kept without added guards and
noted at each definition.

Linux g++ builds and the in-repo checks pass; the Windows Release build and
`--actor-loading` run also pass with these modules (confirmed by the user). 141 functions (about
29 KB) remain under 0x0019723C. The next leaves are 0x0018F848, 0x0018EB4C,
0x0018EB96, 0x00192469, 0x0018FFC9, 0x0018EBEA, 0x0018EDEE and 0x0019149A.
0x001A1B0B/0x001A1B86/0x001A1BC2 belong to a separate 0x00203828 object and
0x001679A8 is the compiler's SEH prologue helper. The live scene probe is
unchanged.

### Validator dependency, swap and tree-instance checkpoint (2026-10-09)

Thirty-two further validator functions are translated and compared against the
retail code in the emulator with zero mismatches (`tools/oracle/run.sh`, now
including `trees_oracle.py`); deliberately mutated builds are detected.

- Instruction reads and dependencies (`shader_validation_instruction_ops`):
  operand reads 0x0018F848/0x001906BB/0x001907E9, slot read mask 0x0018EB4C,
  unset-channel reads 0x0018EB96, vector-to-scalar move conversion 0x0018FFC9,
  result propagation 0x0018FC2A/0x0018FD7D, dependency and pairing tests
  0x00190858/0x0018FED0/0x001908AE, destination channel matching and swapping
  0x0018EBEA/0x0018EDEE (both translated from the assembly), shared channel
  writers 0x0018F815 and the owner registry search 0x00190D14 (with a typed
  balanced-tree overload of 0x001900DE).
- Pass trees: erase 0x00192469, erase by key 0x001936B5 and signed insertion
  0x00191A82.
- Tree template instances (`shader_validation_tree_instances`): joins
  0x0019164F/0x00191714/0x00191806/0x001918CB/0x00191B1C, erases
  0x0019204A/0x0019214A/0x0019224A/0x0019234A/0x00192529, unregisters
  0x001926FB/0x001927B9 and inserts 0x001922BB/0x0019259A. A normalized
  instruction comparison shows each is byte-identical to an existing
  instance (0x0019158A, 0x00191F4A, 0x0019269C or 0x00191FBB, differing only
  in the node initializer), and the emulator runs every address separately.

Type corrections found on the way:

- `ShaderValidationOperand` +0x04 is the owning instruction pointer, not a
  numeric source. 0x00192DF1 passes the instruction and 0x00192934 raises
  the invalid-operand exception when it is null for kinds other than 3.
  Operand +0x0C (`kind`) is the operand slot 0..2, or 3 for an owner's operand.
- Owner channels (+0x20..+0x2C) hold 0, the retail 0xFFFFFFFF marker, or the
  instruction that writes the channel (0x0018FFC9 dereferences it and compares
  its scalar destination). They are now `ShaderValidationChannelWriter`
  (pointer-sized) with `shader_validation_unwritten_channel` for the marker;
  0x00192A54 takes its channel value in that type. Callers that pass retail
  0xFFFFFFFF must pass the marker constant.

The owner-registry search stores retail operand addresses in its nodes, so
0x00190D14 takes an explicit address-to-operand map rather than casting
32-bit values to host pointers.

Linux g++ builds and in-repo checks pass; the Windows Release build and
`--actor-loading` run, including the constructor changes, also pass (confirmed
by the user). The remaining validator work is
mostly larger parents (0x00192DF1 instruction construction, 0x00195FBE,
0x00194E5E, 0x001943CD and the pass dispatcher 0x001973F1), plus 0x0018E94C
(address-ordered comparison) and the 0x00203828 object helpers
0x001A1B0B/0x001A1B86/0x001A1BC2. The live scene probe is unchanged.

### Validator instruction sets and owner channel swaps (2026-10-09)

Twenty-two further functions are translated, emulator-verified with zero
mismatches (`sets_oracle.py`), and covered by in-repo checks:

- Owner chain predicates 0x0018E78B, 0x0018F56F and 0x0018F549.
- Tree destruction and deletion 0x0018F2AA, 0x0018F2D3 (instances of
  0x0018F281), 0x0018F1BC (pass layout), 0x00190322, 0x00190163,
  0x00190AA0 and 0x00190AC6 (deleting destructors of the 0x002037D0 and
  0x002037D8 tree objects, freeing through XMemFree for flag bit 0).
- Registration into the 0x002037EC/0x002037F4 trees and pass trees,
  0x00192772, 0x0019287F, 0x001924DA, plus find-or-register 0x00192830.
  0x001924DA is declared void, but 0x00192830 uses the node it leaves in EAX,
  so the translation returns it.
- The instruction set: comparator 0x0018E94C orders by the signed sequence
  at instruction +0x40, then by retail object address. Set nodes therefore
  store retail addresses, and the set functions (0x0018F174 lookup, 0x001923BB
  insertion, 0x001927D1 registration, 0x001929E7 copy, 0x00192A2B release,
  0x00190774 writer membership) take explicit address/instruction maps rather
  than ordering host pointers.
- Owner channel swapping 0x00190FFE, which climbs derived owners, propagates to
  channel owners (undoing on failure), swaps writer channels through 0x0018EDEE
  and rewrites registered operand orders; plus a typed balanced-node overload of
  iterator 0x0018F0EB.

Instruction +0x44/+0x48 are now `instruction_set_count` and `instruction_set`
(a 0x002037D0 tree object registered in the DAT_001a4d58 registry).

Remaining under 0x0019723C: 87 functions (about 23 KB). The SEH-framed functions are next
(instruction construction 0x00192DF1, pass-object destruction 0x001934A5,
0x001935E6, 0x00190E27 and 0x00192B84), along with the large pass bodies. They
need the same Windows SEH treatment as the existing constructors.

### SEH-framed construction and pass destruction (2026-10-09)

The first SEH-framed validator functions are translated using the existing
`__try`/`__finally` pattern. Their unwind actions come from the retail FuncInfo
tables, which a new read-only byte export (`DumpBytes.java`, code ranges
0x00160000+ and data 0x00320000+) makes readable:

- 0x00192DF1 `create_shader_validation_instruction` (FuncInfo 0x0032B968):
  allocates the 0x4C instruction through the 0x001929B7 base, clears its fields
  and constructs three slot operands parented to the active owner. Each of
  states 0..3 frees the allocation in EBP-0x10 (funclets
  0x001708D5/DF/E9/F3). The instruction allocation is dereferenced without a
  null test, as in retail.
- 0x001935E6 `create_shader_validation_constant_owner` (FuncInfo 0x0032B8CC):
  a kind-3 owner with a full mask and channel value 0, optionally parented to
  the pass owner at +0x50.
- 0x001934A5 `destroy_shader_validation_pass` (FuncInfo 0x0032BA34, five
  states): unregisters the three registered pass-tree sub-objects from
  DAT_001a4d48, clears the special, register and value trees, destroys the
  instruction list, then unregisters the pass from DAT_001a4d38. The unwind
  funclets 0x001709A2/97/8C/81/79 call 0x001914B9 (pass-tree destructor, now
  translated), 0x001909B1 and 0x001928F6 in the retail order.

`ShaderValidationRegisterTables` is replaced by `ShaderValidationPass`, which
follows the retail layout: base +0x00, instruction list +0x04, three 0x14-byte
registered pass trees at +0x14/+0x28/+0x3C (each with a registered sub-object
at +0x10), and the owner at +0x50.

Verification: `ctor_oracle.py` runs the retail functions (including the EH
prolog 0x001679A8 against an emulated FS:[0] list) and compares the normal path
with SEH-shimmed host builds. It reports zero mismatches with allocation
failures injected, and a reordered destructor is detected. The Windows
build with real SEH and `tests/shader_validation_lifecycle_tests.cpp` (in the
`--actor-loading` chain) also passes, as confirmed by the user. 0x00190E27 and 0x00192B84 (local tree
objects with unwind state) and the 0x00193C2A stub remain.

### Owner reader registration and writer negation (2026-10-09)

Two more SEH-framed functions are translated in `shader_validation_owner_sets`:

- 0x00190E27 `shader_validation_owner_readers_registered` (FuncInfo 0x0032BAA8;
  its single state destroys the by-value mask, funclet 0x00170A5F). For every
  registered reader of a masked channel, the operand's retail address must be in
  the given registry, be one of the instruction's operands, or be the operand of
  an operand parent. The check then recurses into the derived owner and the
  channel owner, clearing channels each one writes.
- 0x00192B84 `negate_shader_validation_owner_writers` (FuncInfo 0x0032B8F0;
  state 0 runs the local set destructor 0x001909CE). It builds a local
  instruction set of the owner's writers, then negates each writer's move source
  through 0x0018ED95. If one writer refuses, it walks back with 0x00190337 to undo
  the earlier negations. On success it toggles every registered reader's marker,
  then frees the local set nodes with 0x0018F2AA.

`ShaderValidationSetContext` carries the allocator, the set-node deleting
destructor and the retail address maps. The emulator now also maps the vtable
block at 0x00203000 (node vtables 0x002037E0..0x002037F8 are one-slot deleting
destructors), so retail set nodes are freed through the real destructor.
`ctor_oracle.py` compares both functions (600 cases with allocation failures)
with zero mismatches, and catches deliberately broken builds. The lifecycle
tests cover successful negation, rollback and rejected owner kinds; they pass
in the Windows build (confirmed by the user).

Remaining under 0x0019723C: 82 functions (about 22 KB), led by the pass
bodies 0x00195FBE, 0x00194E5E, 0x001943CD, 0x001956D1 and the dispatcher
0x001973F1.

### Small validator helpers: register retirement and operand equivalence (2026-10-09)

Five small functions are translated:

- 0x0019149A `construct_shader_validation_registered_pass_tree`: clears the
  root and count, stores the pass-tree vtable 0x00203800, and writes the
  sub-object vtable twice (base 0x00203798, then 0x002037FC). The +0x04 word is
  left untouched.
- 0x00190813 `shader_validation_operand_writers_in_set`: an operand without a
  parent passes. Otherwise its parent goes through 0x00190774 with the channels
  the operand sources (0x0018F86B).
- 0x00193C2A `shader_validation_instruction_accepts_order`: an EH-framed stub
  that always returns false (RET 0x18). Its only unwind state destroys the
  by-value order (FuncInfo 0x0032BB2C, funclet 0x00170A29).
- 0x00194311 `shader_validation_operands_equivalent`: parents must be
  equivalent (0x0018F4A9) and markers equal. Then the orders must be compatible
  (0x0018E741), or 0x00193C2A must accept on either instruction. Retail passes
  this operand's kind in both 0x00193C2A calls.
- 0x00193EE2 `retire_shader_validation_register` (`RET 8`, pass in ECX):
  - Kinds outside 0..12, or kinds without a register node, return true.
  - Otherwise the owner's readers must be registered (0x00190E27 with a full
    mask, no instruction, and both climbs). If they are not, the kind is kept.
  - If they are, the kind is erased (0x001936B5) and the owner's
    `channel_owner` chain is walked. The first owner whose readers are not
    registered becomes kind 1 with source = kind. It is re-inserted under that
    kind (0x00192830), and the function returns false.

Verification:

- `sets_oracle.py` now enters `writers` cases through an operand half the time.
- `ctor_oracle.py` adds `equiv`, `retire` and `regtree` cases. Retire cases
  report the erased keys from the retail XMemFree hook.
- Both report zero mismatches.
- These deliberately broken builds are each caught by the oracle and by the
  in-repo tests:
  - a skipped marker test
  - a derived/channel-owner swap
  - a wrong kind range
  - a wrong takeover kind
  - a swapped vtable
  - a null-parent result

New tests are in `tests/shader_validation_lifecycle_tests.cpp` (retirement,
takeover and equivalence) and `tests/shader_validation_serializer_tests.cpp`
(operand set test).

Inventory note: 0x001A1B0B, 0x001A1B86, 0x001A1BC2, 0x001A1C89 and 0x001A1A7F
are false positives. They come from scanning past the two-slot vtable
0x00203820 and are not validator functions. 0x001679A8 is the compiler's EH
prolog. With those excluded, 71 functions (about 20.9 KB) remain under
0x0019723C. The current leaves are 0x00193649, 0x00190F87, 0x00193566,
0x00191417, 0x00191D2D and 0x001911A8. The largest remaining are the pass bodies
0x001943CD, 0x00194E5E, 0x00195FBE and 0x001956D1, and the dispatcher
0x001973F1.

### Input-owner cache and missing-reader channel mask (2026-10-10)

Two more validator leaves are translated:

- 0x00193566 `get_shader_validation_input_owner` looks up the exact source key
  in the pass's +0x14 value tree. On a miss it allocates a kind-2 owner with
  a full mask, zero writers and the constructor's active-owner fallback,
  then caches the result. A failed owner allocation still inserts a null
  value; subsequent lookups return it without retrying allocation.
- 0x00190F87 `shader_validation_unregistered_reader_channels` checks each of
  four single-channel masks through 0x00190E27, with both owner climbs enabled
  and the caller's optional instruction. Each output byte is the negated
  result, and the output mask receives vtable 0x00203790.

Both retain Windows SEH cleanup. Read-only export from the Ghidra project at
`Z:\GhouliesStuff\ghidra_projects` confirms 0x00193566 shares FuncInfo
0x0032B8CC and allocation-release funclet 0x001709B7 with 0x001935E6;
0x00190F87 uses FuncInfo 0x0032B7F0, whose funclet 0x001707EF destroys its
local mask. The exported metadata stays in `cache/oracle/`.

The extended `ctor_oracle.py` suite passes 6,000 randomized cases across two
seeds with zero mismatches against retail machine code, using a Windows MSVC
harness with real SEH (normal paths, including heap allocation failures).
Changing the mask polarity produces 59 mismatches in a 600-case run; changing
the input owner's kind produces 41. The oracle now exits unsuccessfully for
mismatches or missing output rows, and selects `.exe` harnesses on Windows.
In-repo lifecycle checks cover cache reuse, cached null values, active-parent
selection, both reader climbs and local writer masking.

The full Windows Release build, all six CTest checks, `--actor-loading` and
`--ghoulybox-decoder` pass. `--player-opening` still fails with dialogue=0,
cutscene before A=1, actors=1 and entities=1. These helpers are dependency
work; they do not establish playable scene startup. The next previously
listed leaves are 0x00193649, 0x00191417, 0x00191D2D and 0x001911A8;
the large pass bodies and dispatcher remain outstanding.

### Owner relocation, reordering and derived-pass destruction (2026-10-10)

Five more functions are translated:

- 0x00191417 `move_shader_validation_masked_channels` relocates occupied
  masked channels into unmasked empty channels, searching destinations in
  ascending order. It passes the caller's climb/propagation bytes to
  0x00190FFE. Failure on a later channel retains earlier successful moves.
- 0x001911A8 `reorder_shader_validation_owner_channels` accepts owner kinds
  1 and 6, removes repeated order entries, fills missing channels, then swaps
  pairs until the inverse order is identity. It preserves the separate
  derived-owner path, local writer updates, reader-order rewrites and
  current-pair rollback. Earlier completed pairs are not undone on failure.
- 0x00191D2D `shader_validation_owner_dependencies_registered` checks the
  derived owner's readers, then membership of its real masked writers in
  the instruction set. Zero writers and 0xFFFFFFFF markers are exempt.
  Kind 5 additionally checks the pass owner's readers when that owner exists.
- 0x00193649 `destroy_shader_validation_program_pass` clears the derived
  object's instruction set at +0x88 before calling the full base destructor.
  The native `ShaderValidationProgramPass` exposes only these used fields.
- 0x00193EBE `delete_shader_validation_program_pass` invokes that destructor,
  then frees the object through XMemFree with flags 0x24870000 when the
  incoming flag's low bit is set, and returns the original pointer.

The SEH unwind actions are verified against the exported retail FuncInfo
tables: 0x0032B848/0x00170823 and 0x0032B86C/0x00170835 destroy the two
by-value masks; 0x0032B824 destroys the reordering temporaries and argument
through 0x00170811/09/01; 0x0032BA58 calls the base pass destructor through
0x001709CB. These use the existing Windows SEH backend.

The final extended `ctor_oracle.py` suite passes 12,000 randomized cases
across seeds 5 and 6 with zero mismatches using the native MSVC harness.
It compares writer slots, instruction masks, reader orders, destructor event
order, release flags and return values. This checks normal paths; it does
not establish exceptional-path equivalence. Five deliberately mutated builds
(masked-channel polarity, dependency kind, reorder kind, omitted derived-set
cleanup and wrong free bit) are all rejected. Fixed lifecycle regressions
cover partial relocation, swap rollback, marker handling, kind gates,
registered readers/writers and destruction before heap release.

The Windows Release build, all six CTest checks, `--actor-loading` and
`--ghoulybox-decoder` pass. The live `--player-opening` probe remains unchanged:
dialogue=0, cutscene before A=1, actors=1, entities=1. No live validator or
scene acceptance is inferred from these dependency checks.

Read-only `CalleeTree.java` inventory from the supplied Ghidra project, using
all existing source translations plus the two-slot vtables at 0x00203820 and
0x002037D0, now lists 64 untranslated functions. Its next direct-call leaves
are 0x00193DF4 (owner selection) and 0x00191DF3 (owner linking). It also
reports vtable target 0x00192818 as NOFUNC; that entry needs inspection rather
than an invented implementation. Large pass bodies and dispatcher remain.

### Owner selection, linking, installation and reader-chain queries (2026-10-10)

Six further functions are translated:

- 0x00193DF4 `select_shader_validation_owner`: kind 1 looks up the requested
  register, kind 2 selects/caches the input source, kind 3 allocates the
  separate constant source, and other kinds return null. The mask passed
  to 0x00191E8F is popped but unused, as in that getter's assembly.
- 0x00191DF3 `link_shader_validation_owners`: a null other owner returns
  before dereferencing the first owner. Otherwise both masked relocations
  run with climb/propagation enabled, before checking existing derived links
  and incompatible channel-owner tails. Query mode still performs relocation
  but does not install links; neither a later refusal nor query mode undoes
  completed relocations. Commit inherits an absent first tail, makes the
  second owner point back to the first, and installs the derived link.
- 0x00191EC3 `set_shader_validation_pass_owner`: passes two empty masks to
  the linker, ignores its result, stores the new pass owner and returns it.
  Null into an empty pass follows the linker's null-other shortcut.
- 0x00193E52 `replace_shader_validation_register_owner`: looks up the prior
  owner, attempts a link with empty masks, then erases/replaces the regular
  register entry regardless of the link result. Source 12 looks up the special
  0x100 entry but still replaces the regular source-12 entry. A failed root
  allocation leaves a null root with count one through 0x001924DA; the
  replacement function still returns the supplied owner.
- 0x00191BE1 / 0x0018F513: the mutually recursive reader query checks the
  current diagnostic registry, then follows empty channel-owner slots. A
  kind-4 owner with no channel owner supplies the terminal true result.

The selection wrapper retains FuncInfo 0x0032BB74, which destroys its mask
through 0x00170A4D. Linking retains FuncInfo 0x0032BC64: state 1 destroys
the first mask (+0x10) through 0x0017084F, then state 0 destroys the second
mask (+0x18) through 0x00170847. No exception or success stubs are added.

The extended native MSVC/retail-machine-code oracle passes 12,000 cases
across seeds 8 and 9 with zero mismatches, including allocation failure and
the null first-owner/null other-owner path. It compares selected owners,
writer slots, masks, reader orders, link pointers, register contents and
deletion events. Exceptional-path equivalence remains unverified. Six
deliberate mutations (wrong input-source argument, inverted query mode,
discarded pass assignment, omitted register erase, wrong terminal owner kind
and skipped direct reader registry) are all detected. Fixed lifecycle tests
cover query relocation, retained moves on rejected links, ignored link
results, special-table lookup and allocation-failure count semantics.

The Release build, all six CTest checks, `--actor-loading` and
`--ghoulybox-decoder` pass. `--player-opening` remains dialogue=0,
cutscene before A=1, actors=1 and entities=1; gameplay acceptance is unchanged.

The refreshed read-only Ghidra inventory now lists 58 untranslated functions.
Its next direct-call leaves are 0x00191CC0 (unwritten-channel dependency
query) and 0x00191C74 (unused instruction destination query). The large pass
bodies, dispatcher and NOFUNC vtable entry 0x00192818 remain outstanding.

### Unwritten dependencies and unused destinations (2026-10-10)

Translated 0x00191CC0 and 0x00191C74 in shader_validation_sets. The first
checks only empty owner channels: kind 4 also accepts a derived writer when
there is no writer along the channel-owner chain. The second rejects any
output register, then checks all four channels of each present vector and
scalar destination for readers. Assembly confirms the receiver is the
destination owner despite the decompiler's ambiguous call expression.

Both functions use existing translated callees without new host boundaries.
The expanded constructor oracle passes seed 10 with 6,000 cases and zero
mismatches. The Release build and all six CTest checks pass. The refreshed
read-only Ghidra inventory has 56 untranslated functions. This does not
change the preceding player-opening failure or establish gameplay acceptance.

### Operand reparenting and unused-channel cleanup (2026-10-10)

Translated the mutually recursive 0x00192C9E/0x00192B20 pair. Reparenting
erases the old diagnostic registration, reloads the old parent, and detaches
an unused kind-3 parent's own operand when it has a foreign parent. It then
assigns/registers the new parent and substitutes DAT_001a4d34 for null.
The owner wrapper creates a kind-3 operand only when absent. Allocation
failure still reaches the retail null dereference; no fallback is invented.
FuncInfo 0x0032B8CC state 0 uses 0x001709B7 to release an allocation whose
constructor fails. Exceptional-path equivalence remains unverified.

The diagnostic virtual erase target 0x0019275A is also translated. The saved
Ghidra project has no function there; exported retail instructions identify
the thunk's calls to 0x001902BA and the already translated 0x0019214A.
The local instruction dump is cache/ghidra_frontier/0019275A.txt. No Ghidra
function or symbol was created or renamed.

Translated 0x0019373D/0x0019379C in the instruction lifecycle module. Unread
operand channels become 4; an empty order reparents to the active owner.
An unread channel reports a change even when already 4. Instruction cleanup
visits all three operands before combining the result bytes.

The expanded constructor oracle compares parent pointers, diagnostic keys,
node deletion order/flags, lazy allocation, global registration count,
channel orders and cleanup results. Seeds 12 and 13 pass 16,000 cases with
zero mismatches. Fixed lifecycle regressions cover already-missing channels
and all three operands. Release and transition-test builds, all six CTest
checks, actor loading and retail ghoulybox decoding pass. Player opening
still fails with dialogue=0, cutscene before A=1, actors=1 and entities=1.

The refreshed inventory has 52 untranslated functions. The writer/destination
mutation cycle (0x00190D50, 0x001908FC, the 0x0019070B/2E/51 wrappers and
0x0018F608/6B7/766 setters) is the next useful group; it must be translated
together rather than replaced with callbacks or stubs.

### Writer replacement, destination mutation and duplicate-channel merging (2026-10-10)

Translated eleven functions in shader_validation_destinations.cpp/.h:
0x00190D50, 0x001908FC, 0x0018F608, 0x0018F6B7, 0x0018F766,
0x0019070B, 0x0019072E, 0x00190751, 0x00191C0B, 0x00190D77
and 0x00191C47. CMake and the constructor oracle's Linux SEH-shim build
include the new module.

Channel writer replacement removes the previous real writer's destination
channel before assigning the new slot; zero and the marker bypass removal.
Destination removal chooses the first matching output/vector/scalar owner,
clears the mask byte before the recursive writer update, then detaches an
empty destination. Replacement snapshots the old mask, clears its writers,
installs the new mask's writers, and assigns the destination last. Null
replacement copies an empty mask. The three wrappers retain their current
mask; the masked-writer loop treats every nonzero byte as selected.

The setters preserve FuncInfo 0x0032B7CC: states 1/2 destroy the local
snapshot before the argument mask, while state 0 destroys only the argument.
The masked-writer loop preserves FuncInfo 0x0032B848's argument cleanup.
These normal paths are CPU-verified; exceptional-path equivalence remains
unverified.

Duplicate-channel merging first checks the real shared-writer predicate.
Ancestor channel occupancy can reverse the surviving channel. Removal then
rewrites diagnostic operand orders along empty removed-channel slots and
stops at an occupied ancestor. The all-pairs wrapper visits all six pairs
and ignores individual refusal results.

The expanded retail oracle compares every owner writer, all three instruction
destinations, mask bytes/vtables, operand orders and defined return values.
Seed 15 passes 8,000 mixed cases; seed 16 passes 4,000 focused destination
cases. Both report zero mismatches. A generated invalid graph was excluded:
removing through a null destination with a nonzero selected mask dereferences
null in retail, but Unicorn's mapped FS page had hidden that fault. The host
translation retains the dereference and does not invent a null fallback.

Fixed regressions verify destination alias precedence, noncanonical selected
bytes, null replacement, and ancestor-driven merge reversal. Release and
transition-test builds, all six CTest checks, actor loading and retail
ghoulybox decoding pass. Player opening remains dialogue=0, cutscene before
A=1, actors=1 and entities=1. The refreshed read-only Ghidra inventory has
41 untranslated functions; 0x00193A40 and the larger 0x00192F0C/0x00193222
are now useful next translations.

### Reader migration onto the derived owner (2026-10-10)

Translated 0x00193A40 as move_shader_validation_unwritten_readers_to_derived
in shader_validation_constructors.cpp/.h. The first pass accumulates writer
masks along the channel-owner chain and rejects any diagnostic reader that
reads both the accumulated mask and its complement. It completes the current
registry traversal before observing rejection at the next owner. Without a
derived owner it initializes its local mask and returns.

When allowed, a second pass rebuilds the accumulated mask and reparents
readers that do not read it onto the original owner's derived owner. Every
reparent is followed by a fresh first-node lookup in the mutated registry;
continuing a saved successor would change retail behavior. Existing translated
reparenting and diagnostic registry functions perform all updates. FuncInfo
0x0032BACC state 0 uses 0x001709FA to destroy the accumulated mask on unwind;
exceptional-path equivalence remains unverified.

The new moveunwritten oracle fixture compares reader parent pointers, every
owner's sorted diagnostic keys/count, and deleting-node events/flags across
varying masks, opcodes, reader orders, owner kinds and chain lengths. Seed 17
passes 3,000 focused cases; seed 18 passes 6,000 mixed cases, both with zero
mismatches. Fixed regressions cover multiple moves with iterator restarts,
a mixed-channel reader preventing otherwise eligible moves, and no derived
destination. Release/transition builds, all six CTest checks, actor loading
and ghoulybox decoding pass. Player opening still reports dialogue=0,
cutscene before A=1, actors=1 and entities=1.

The refreshed read-only inventory now has 40 untranslated functions.
0x00195BE1 remains blocked by untranslated 0x00194D12; do not implement it
using a placeholder. The dependency-ready larger transformations
0x00192F0C and 0x00193222 remain next, followed by the instruction-set
construction cycle 0x0019390D/0x00193C44/0x001931E4.

### Splitting combined scalar instructions (2026-10-10)

Translated 0x00192F0C as split_shader_validation_scalar_instruction in
shader_validation_instruction_constructor.cpp/.h. It returns null without
allocation unless both opcodes are nonzero. The scalar opcode moves into a
new instruction with the original sequence, a null instruction set and count
0xFFFFFFFF. Its first two operands get fresh kind-0 owners; the scalar operand
copies the original parent, marker and channel order. Unless the vector
opcode is 3 or 4, the source scalar operand is reparented onto another fresh
kind-0 owner.

Every nonzero output flag transfers the output destination, including
noncanonical flag bytes. Scalar destination transfer follows output transfer,
so destination aliases observe any earlier mask mutation. Existing translated
destination setters update writer ownership before the source fields are
cleared. FuncInfo 0x0032B9BC cleanup states 0..6 preserve the retail allocation
slots and nested unwind edges 2->1 and 4->3. Exceptional cleanup and allocation
failure paths remain unverified by the CPU oracle.

The split fixture compares source/result fields, masks, operands, parent
addresses, destination writers, allocation calls and sorted object keys.
Seed 19 passes 3,000 focused cases; seed 20 passes 6,000 mixed cases, both
with zero mismatches. Fixed regressions cover the early gate, sequence/opcode
transfer, noncanonical flags, copied scalar order/marker and fresh parent
construction. A regression initially used opcode 2 for add; the retail
predicate accepts 3 and 4, and the test was corrected.

Release/transition builds, six CTest checks, actor loading and retail
ghoulybox decoding pass. Player opening still fails with dialogue=0,
cutscene before A=1, actors=1 and entities=1. The refreshed read-only
inventory contains 39 untranslated functions. The two small consumers
0x0019368D and 0x00193F8A are now ready, followed by 0x00193222 and the
instruction-set construction cycle.

### Registering scalar splits during program traversal (2026-10-10)

Translated 0x0019368D as split_and_register_shader_validation_scalar_instruction
and 0x00193F8A as split_shader_validation_program_scalar_instructions in
shader_validation_pass_lifecycle.cpp/.h. The first checks both opcodes,
splits the instruction and registers the returned instruction in the
program's +0x88 instruction tree. The second finds the first node, applies
that wrapper and advances the current node after insertion. It preserves
the live tree traversal rather than collecting a snapshot of instructions.
All callees are existing translated functions.

The splitregister and splitpass oracle fixtures compare registration keys
and counts as well as source/result state, destination writers and allocation
calls. Seeds 21 and 22 each pass 2,000 focused cases; seed 23 passes 6,000
mixed cases, all with zero mismatches. A fixed regression starts with three
instructions, exercises insertion into the existing tree, then checks that
a second traversal performs no further splits. Empty traversal also performs
no allocation. Exceptional paths remain unverified.

Release/transition builds, all six CTest checks, actor loading, retail
ghoulybox decoding and diff whitespace checks pass. Player opening remains
blocked by skipped type-0x14 dialogue calls: dialogue=0, cutscene before A=1,
actors=1 and entities=1. The read-only Ghidra inventory now has 37 untranslated
functions. Next is 0x00193222, followed by the instruction-set construction
cycle 0x0019390D/0x00193C44/0x001931E4.

### Remaining-function task: dependency and register group (2026-10-10)

The request to translate all remaining 37 functions is still incomplete.
Fourteen are now translated in shader_validation_dependencies.cpp/.h:
0x0019390D, 0x001931E4, 0x00193C44, 0x00193FE4, 0x00194002,
0x00194047, 0x001940AC, 0x00194D12, 0x00195BE1, 0x00195CA0,
0x00196814, 0x001969C0, 0x00196A14 and 0x00197075.

The mutually recursive dependency builder caches each instruction's set before
walking diagnostic readers and ancestor writers. It registers set objects in
DAT_001a4d58, optionally includes operand ancestor writers through DAT_001a4d30,
and refreshes the cached count even when the set already exists. Priority
ordering uses descending signed dependency counts, ascending signed sequence,
then the retail instruction address. List selection swaps values in place.

Parent compatibility checks other operand owner kinds, derived-owner overlap,
writer dependencies and the instruction's own destinations. Reader migration
restarts first-node lookup after each reparent. Kind-6 rotation reconnects both
directions of the owner chain before another migration. The new typed
0x00190CA6 overload preserves its count check and zero-key return semantics.

Register assignment recursively handles derived owners, moves unregistered
reader channels when required, and preserves query-only branches. At
0x00196966 retail writes the pass address into a by-value mask's channel bytes;
the translation preserves those exact bytes rather than substituting the
previously calculated mask. Register selection tries the owner's source,
11 down through 2, 0, then 1 only for instructions without both opcodes.
0x00197075 counts register availability and queries a destination's existing
kind when necessary.

Unwind metadata was inspected for the mask/order states in the dependency,
parent, migration and register functions. 0x00193C44 destroys only its used
mask in state 0; 0x00196814 state 1 destroys its unregistered mask before
unwinding the incoming mask. CPU exception equivalence remains unverified.

The dependencybuild oracle constructs four-instruction acyclic graphs and
compares set addresses/keys/counts, allocation calls, every pair's dependency
and priority predicates, list selection, parent acceptance, reader migration,
rotation/detachment, register assignment/selection and availability counts.
The migration/rotation portion uses empty channel writers; broader occupied
migration cases remain a verification limitation. Allocation failure cases
are excluded. Focused seed 29 passes 3,000 cases and mixed seed 30 passes
6,000 cases with zero mismatches. Earlier intermediate focused checks also
passed while the fixture was expanded.

Fixed regressions verify cache reuse, duplicate reader/writer dependencies,
priority by dependency count, a query that leaves registration unchanged, and
refusal to register an existing owner under another kind. Release/transition
builds, all six CTest checks, actor loading and retail ghoulybox decoding pass.
Player opening remains dialogue=0, cutscene before A=1, actors=1, entities=1.

The read-only inventory now has 23 untranslated functions:
0x00193222, 0x001943CD, 0x00194E5E, 0x001954F3, 0x001956D1,
0x00195A15, 0x00195AFC, 0x00195B89, 0x00195C1A, 0x00195D42,
0x00195E43, 0x00195FBE, 0x00196545, 0x00196739, 0x00196B22,
0x00196C74, 0x00196D71, 0x00196DD1, 0x00196F09, 0x00197223,
0x0019723C, 0x001973F1 and 0x00197A94. The next dependency-ready leaves
are 0x00193222 and 0x00194E5E; the other remaining functions depend on them.

### Remaining validator functions completed (2026-10-10)

All 37 functions from the requested inventory are now translated. This pass
completed the 23 addresses listed above, and translated ten additional virtual
destructor/removal callees exposed by their vtables: 0x00192818, 0x001928C6,
0x00192D13, 0x001936CD, 0x0019370F, 0x001937D8, 0x001937FC,
0x00193BBC, 0x00193FC0 and 0x00194088. The two removal thunks absent from
Ghidra's function table were decoded from the exported retail bytes.

The translated modules now cover owner merging and move rewriting, packed
instruction/pass construction, instruction and owner destruction, optimization
worklists, scheduling/register assignment, serialization dispatch and the outer
validator entry point. Each definition and declaration retains its Xbox address.
The new modules are separately compiled through CMake.

The optimizer preserves both writer redirection and reader reparenting, including
the duplicate writer checks, sequential destination changes, explicit diagnostic
node erasure, marker normalization and worklist restart points. Owner destruction
removes references from the actual expression registry objects; instruction
destruction removes the instruction from every registered dependency set.

The scheduler maintains writer/deferred instruction sets, ready/blocked lists and
a reader registry. It retains the vector/scalar conversion and merge passes,
register availability checks, pairing decisions, blocked-list ordering and NOP
insertion. The outer 0x0019723C entry point retains its second attempt with scalar
splitting disabled and conservative scheduling enabled; the second scheduling
result is ignored as in retail. Its C++ catch-all handler at 0x001973C2 clears the
result byte and resumes global cleanup. Allocation and container unwind states
were checked against FuncInfo 0x0032BE70 and the other exported unwind tables.

Typed registry overloads of 0x00190C93/0x00190CC1 support balanced nodes storing
retail addresses. Their object callback represents the genuine registered-object
deleting destructor. The context must dispatch to the translated operand,
instruction, owner and pass destructors. The move and serializer interfaces retain
their explicit caller-owned frame bytes; the implementation does not invent
initial values for retail stack data read before assignment.

The expanded dependencybuild CPU oracle compares construction, optimization,
scheduling, serialization, registry cleanup and allocation counts against retail.
It includes programs using input owners and register-dependent move chains, and
uses real translated virtual destruction/removal functions. The allocator fixture
now reserves 0x100 retail bytes per allocation so the 0x98-byte derived pass cannot
overlap its next allocation. The serializer fixture explicitly initializes the
retained frame words before its first call.

Focused checks passed with zero mismatches: seeds 31 (1,500 cases), 32/33/34
(3,000 each), 35 (2,000), 36 (300), 37 (1,500), 39 (1,500 combined scheduling
cases), 41 (1,500 complete validation cases) and 43 (300 cases including register
move chains). Mixed seed 42 also passed 1,500 cases.
The final expanded fixture passed seed 44 (1,500 focused cases) and seed 45
(1,500 mixed cases), both with zero mismatches. Allocation failures,
exception propagation and retry-path equivalence remain outside the CPU fixture;
these results establish only the normal paths exercised by those cases.

Release and transition builds, all six CTest checks, actor loading and retail
ghoulybox decoding pass. Player opening still fails with dialogue=0, cutscene
before A=1, actors=1 and entities=1: script type 0x14 still skips the dialogue
calls. Completing this validator translation does not establish gameplay
acceptance or wire its typed contexts into the live shader registration path.

The refreshed read-only Ghidra inventory reports no TODO/NOFUNC entries for the
23 completed functions and the checked virtual call targets. Ghidra symbols,
OriginalFiles and the Git index were not changed.

## Scene opening resource ownership (2026-10-10)

`SceneDialogueResourceOwner` now owns the retail localization storage and native
font/key views needed by the translated dialogue calls. It registers the real
bundle-provider finder/loader/unloader and reads the English loctext and fonts
from the disc dump. The native LSBL view retains the string table and IDs; key
names use 0x00162A80 to build the native lookup hash rather than treating the
disk key metadata as that hash. Testing only the tutorial did not catch this:
the challenge-award keys initially resolved to unrelated strings.

`runtime_pad_state_views()` exposes the exact existing current/previous samples
from `pad_tick`, including the four 0xA8-byte controller records and the merged
record. This lets translated input contexts read the live pad storage directly.

Run the resource/input preflight independently with:

```powershell
cmake --build build --config Release --target gbtg-transition-tests
.\build\Release\gbtg-transition-tests.exe OriginalFiles --dialogue-resources
```

The check compares all 457 dialogue keys with the independent disk parser,
calls 0x000FF0A0 and 0x000589F0 for the actual opening tutorial, and verifies
the A press/hold/release through a translated kind-3 input device reading live
pad samples. The tutorial resolves speaker 2, five lines, width 361.6 and buttons
enabled. `--player-opening` also runs this preflight before its live check.

This is resource and input ownership progress, not scene-opening acceptance.
The live script still executes the legacy type-0x14 branch and the opening
gate still fails. Remaining integration is a scene-owned activation context,
the native detached dialogue cutscene and its factory actor mappings, the
button-bat widget/update context, input completion callback and translated draw
context. Those contexts must be supplied to `execute_runtime_script_dialogue`
and ticked/rendered by the live scene; setting the two opening flags is not an
implementation of those calls. No OriginalFiles edits or commits were made.

## Live dialogue dispatch and frame hooks (2026-10-10)

The live type-0x14 branch now delegates to `execute_runtime_script_dialogue`
instead of skipping the enqueue call. The retail nonzero gate still returns 2
before dependency lookup. A missing activation graph raises an explicit error;
it no longer starts the fight cutscene by silently treating dialogue as done.

`bind_scene_dialogue` borrows the activation and outer draw contexts together.
It requires the actual live script latch and scene flag references, and requires
the renderer to share the same activation context. `frame_tick` invokes native
0x00057E60 after the script walk and 0x00058150 during presentation. Unbinding
calls 0x000583A0 before clearing the borrowed pointers. The owner must outlive
these calls and unbind before destroying its dependencies.

Rendering cannot be omitted: 0x00058C30 updates `page.next_text`, which
0x00058AE0 reads when advancing. The frame adapter forwards the current clock
frame/dt and visibility mask, and the actual simulation freeze/scale globals to the
activation graph.

The player-opening check now requires native widget, detached cutscene, input
and page ownership after its no-button hold. It then supplies fresh A edges
with releases and allows the retail fades to finish, requiring the script latch
and native mode to clear and the opening cutscene to start. The resource check
also exercises the live handler's gated and missing-owner paths.

**Still incomplete:** `Game::init` does not construct/install the native scene
owner. The first unbound, ungated type-0x14 call raises
`scene dialogue runtime is not installed: type 0x14 requires the owned 0x000584D0 context`.
The current playable opening has an earlier blocker: its fired script cursor
stops at type 0x6F, before reaching the tutorial. This requires the real player
actor returned by 0x001292D0 and the native mask/state comparisons in
0x00127BC0; returning 2 to skip that wait would invent progress. Neither the
binding guard nor a stalled script establishes opening acceptance. The
next work must construct the retail executable template pools, model relocation
and shader registration contexts, actor factory and cutscene resource views,
widget/input callbacks, audio contexts and D3D draw adapters, then bind them
before the first script walk. No fabricated actor, cutscene or callback results
were added.

The initial live check still showed an imp cutscene despite never reaching
0x14. `load_cutscene_reel` was selecting the first cutscene in the script's
reference list even when no type-0x13 action requested it. That fallback is
removed. Aidlist reels remain supported, and a chapter clip requires the actual
pending script request. The opening check prints each waiting fired cursor,
so an earlier player-spawn wait is distinguishable from dialogue activation.

Validation of these changes: Release `gbtg` and `gbtg-transition-tests` builds
pass, all six CTest checks pass, the dialogue resource/input preflight passes,
and `git diff --check` passes. The live opening correctly remains a failing
acceptance check: `dialogue=0`, `cutscene before A=0`, `actors=1`, `entities=1`,
with fired cursor `offset=0x254 type=0x6f`. The one actor is the body camera;
no real playable actor has yet unblocked the script. Start with the player spawn
queue ownership and type-0x6F query before treating dialogue activation as the
first live failure. OriginalFiles, Ghidra symbols and the Git index are unchanged.

## Retail player templates and spawn wrappers (2026-10-10)

`Game::init` now owns `SceneExecutableResources` before the first script pump.
It reads the untouched `.original` XBE when present, otherwise the current XBE,
and maps only file-backed section bytes. Read-only Ghidra exports of 0x00034C30
and 0x00034BC0 establish the two inactive objparams registrations: 497 records
at 0x003D7928, then 860 at 0x00402CA0, each stride 0x8C. Pool order and inactive
status are preserved. Names, CPU spans and clone byte counts come from these
records; all 1,357 headers agree with the recorded sizes. These pools are used
for native parameter cloning, not as an invented GPU or active bundle provider.

The following calls are translated against their cached dumps:

- 0x00123A30: search the four 0x84-byte scene override rows at 0x00430DA0.
- 0x00123A80: select boy/girl/cook, retaining the scene and runtime flag branches.
- 0x00134920: clone the selected player, write position/Euler/unit scale, clear
  the two identity words and copy the optional override before factory creation.
- 0x00028DF0 / 0x00028E50: release and replace body/mind children with the native
  parent token at parameter +0x1B0 / +0x9C respectively.
- 0x00028640: replace the optional actor-owned ghouly spawn controller, preserving
  its prefix check, load result, controller pointer and side-list linkage.
- 0x00107830: allocate the zeroed 0xB0-byte kind-1 inactive player input device.
- 0x001292D0 and the type-0x6F branch of 0x00127BC0: subtract 0x93C from the
  player link, then preserve the mask and +0x984 loading comparisons. Native
  address views explicitly map zero to nullptr; no missing actor is fabricated.

The resource preflight clones all 1,357 selected templates and tests the actual
scene override bytes, the opening boy/girl flag selection and the cook override.
Its parameter-capture factory is a test fixture only. It verifies the spawn
wrapper's fields and the body/mind parent tokens and releases; its callbacks
are never installed in a live Game. Actor-query checks cover missing player,
the link sentinel, zero/nonzero masks, negative loading values and the dump's
address-zero view. The player input check verifies kind 1, inactive status and
zeroed state before using its actual release call.

**Still not a live player constructor:** the opening boy template is factory
type 5, getter 0x00027F60, constructor 0x00028830. It also names
`aid_objparams_ghoulies_body_boy` (type 0x2A, getter 0x00075AF0),
`aid_objparams_ghoulies_mind_player` (type 0x34, getter 0x0007E100), and
`aid_objparams_ghoulies_strategy_player` (type 0x4F, getter 0x000BED40).
NativeActorFactoryOwner currently installs only types 1, 2 and 0x15. Its actor
constructor cannot stand in for these types. The parent constructor, strategy
setup, class-specific constructors and lifecycles still need translation and
real registrations, followed by scene spawn/update/query ownership. The new
player wait function is not yet substituted for the legacy live handler's
constant hold; it needs that same owned player address/query graph. Dialogue
activation/drawing ownership remains incomplete after this earlier spawn wait.
Read-only Ghidra exports are in cache; no retail bytes, symbols or Git index
entries were modified.

### Child lifecycle dependency progress (2026-10-10)

Six more complete leaf routines are translated in `player_children.cpp`:
0x00075AD0 / 0x0007E0E0 / 0x000AF030 supply body, mind and strategy sizes;
0x000BED90 decrements the active strategy count only for a nonzero +0x48 and
clears that field; 0x00029E00 releases one mind request; 0x00029E80 clears the
mind's +0x58 list. Single-request release invokes the actual mind vtable +0x1C
mapping before unlinking. Bulk release unlinks first, then calls single release.
Heap release uses the existing native heap boundary. The callback remains a
required virtual dispatch mapping, not an installed dummy implementation.

The preflight checks a three-node list with active and inactive requests,
both virtual-release orderings, empty-list cleanup, and repeated strategy
destruction. `--actor-loading` runs the new player wait mask checks. Both
preflights pass. Release builds pass and the six CTest checks pass.
The latest live opening check still fails at +0x254/type 0x6F with dialogue=0,
cutscene=0, actors=1 and entities=1. This is not scene-opening completion.

Read-only getter exports identify mind constructor 0x0007EF10 and destructor
0x0007F0D0 at vtable 0x00505498. The mind constructor includes chapter/health
setup, animation lookup and input-stack registration; those calls must be
resolved before installing factory type 0x34. Strategy vtable 0x00505FA0 uses
the retail constant-success constructor 0x000CFFD0 and destructor 0x000BED90,
but its parent type 8 and state-transition slots remain prerequisites.
Body vtable 0x00505248 also uses 0x000CFFD0; its parent type 0x27 must perform
the actual body setup. None of these classes is registered as a substitute
for the still-missing parent and player constructors.

`ExportPlayerScene.java` now rebuilds temporary ranges with branch-following
disassembly, handles existing functions, and removes conflicting function
analysis only inside a transaction that is always rolled back. This fixes
incomplete instruction exports caused by unanalysed code marked as data or
overlapping function bodies. The Ghidra project stays read-only.

### Mind teardown and constructor lookup dependencies (2026-10-10)

Translated 0x0007F0D0 in `player_children.cpp`. The native caller supplies the
actual actor view reached through mind +8. The destructor retains the runtime
flag 0x800 gate and actor +0x960 nonzero test, then calls the complete translated
0x001093C0 input-stack pop. It does not release the input device or mind requests:
neither operation appears in this destructor. The input regression checks both
hold gates and an unrelated flag before verifying real row movement, previous
device reactivation and reset through the destructor.

Added `scripts/script_table_lookup.h/.cpp` for 0x00027510, a prerequisite of
the mind constructor's body +0x73C lookup. It preserves signed count/index
arithmetic, unsigned 16-bit key comparisons, stride 0x108 and caller-output
clearing. A null table returns without touching the output; an empty nonnull
table clears it. Resource preflight checks hits including 0x8000/0xFFFF,
misses, null tables and zero/negative counts. The older script sample helper
is separate and has not been used as a replacement for this native call.

Read-only exports from `Z:\GhouliesStuff\ghidra_projects\GbtG` recovered
0x0013CCD0, 0x00027510, 0x0013D000, 0x00107720 and 0x00107520. Address
0x001307D0 is absent from the function table; a rolled-back temporary range
export recovered its instructions through RET at 0x0013082B. It sets
DAT_00433BE8, selects DAT_00433BEC using the configuration byte's 0x40 bit,
and stops the selected sound voice through 0x00039690 before clearing the
signed 16-bit voice index. That routine and the camera chain still need native
contexts before completing 0x0007EF10; no constructor was registered in this
batch. The camera chain includes pose/offset selection 0x0013D000 and a
required message 0xBA to the created camera, alongside the existing clone and
factory implementations. Missing factory/virtual mappings must still be
resolved rather than replaced with callback success values.

Release `gbtg` and `gbtg-transition-tests` builds pass. Dialogue resources,
actor loading and all six CTest cases pass. Live player opening still fails:
dialogue=0, cutscene before A=0, actors=1, entities=1, fired cursor
+0x254/type 0x6F. No OriginalFiles edits, Ghidra symbol changes, staging or
commits were made.

### Player mind construction and strategy setup batch (2026-10-10)

Six more routines are translated, with one batched build/CTest run after
implementation and focused checks were added:

- 0x001307D0 resets the intro sound globals, selects scale 1/2 from the
  configuration bit and stops the actual indexed voice through 0x00039690.
- 0x00145FF0 reads the signed 16-bit class count and stride-0x10 rows, returning
  unsigned class values. A negative count skips search but still returns the
  first class; zero returns zero. This native entry does not use the older
  vector convenience helper, which cannot represent a negative retail count.
- 0x0013D000 builds the player camera target with the actual actor world
  position, body scale factors, retail state/class offset table and population
  height gate. Output byte +3 remains untouched.
- 0x0013CCD0 clones the executable camera template at 0x00314E50, calculates
  its pose through translated sine/cosine and direction routines, creates it
  through the existing object factory and releases the clone. A nonnull player
  requires real camera message 0xBA dispatch, including when creation fails;
  the implementation does not add a success-only message guard.
- 0x0007EF10 implements mind health/chapter gates, animation key 0x2C lookup,
  camera creation and the exact state writes. The scene context retains the
  created camera object. The parent mind constructor must supply the native
  actor and body views before this child constructor can be registered.
- 0x00028EB0 releases/replaces strategy, clears mind requests, writes the parent
  token and invokes the real leaving/entering virtual mappings. The leaving
  callback precedes reading the saved old state; request cleanup sits between
  setting +0xE and entering. Neither virtual mapping is supplied as a live stub.

The first five entries are in `entities/player_mind_setup.h/.cpp`; strategy
replacement is beside the other wrappers in `player_children.h/.cpp`.
All address declarations/definitions retain their Xbox notes.

Resource preflight now exercises the retail camera template and offset table
with capture-factory callbacks confined to the test. It covers state/class
bounds, negative/zero class counts, null camera sources, rejected names,
population gating, preserved target bytes, camera payload and pose, voice
cleanup, seven health/chapter flag combinations and exact mind state writes.
Strategy checks modify +0xE during leaving, enqueue a real allocated mind
request, then verify detach/release and entering order. These fixtures are
not live player/camera registrations and do not establish factory acceptance.

Release `gbtg` and transition builds pass; dialogue resources, actor loading,
retail ghoulybox decoding and all six CTest cases pass. The live opening still
fails at +0x254/type 0x6F with dialogue=0, cutscene before A=0, actors=1 and
entities=1. Next work remains the parent mind/body/strategy constructors and
their complete virtual/lifecycle mappings, the player constructor 0x00028830
dependencies, and the real scene-owned factory/spawn/query graph. Camera
message dispatch must map to the actual translated camera receiver. No retail
files, Ghidra symbols or Git index entries were changed.

### Parent lifecycle and player attributes batch (2026-10-10)

Fifteen additional routines are translated before the next build/test batch:

- Parent mind sizes 0x00029D10, parameter/class settings 0x00029A60,
  constructor 0x00029F20 and destructor 0x00029FF0.
- Mind request initialization 0x00029EC0, event-list cleanup 0x0002A240,
  and two-allocation storage release 0x00036B60.
- Parent strategy sizes 0x0002B6A0, class-range sampling 0x0010F760,
  constructor 0x0002B740, embedded-list detach 0x00112A20,
  destructor 0x0002B7A0 and leaving-state counter update 0x000BFA50.
- Player attribute acquisition/random selection 0x00019240 and the low-AX
  tag-index result consumed from 0x00133A50.

The parent routines are in `entities/player_parent_lifecycle.h/.cpp`; attributes
and tag lookup are in `entities/player_attributes.h/.cpp`. Native resource,
storage and list views alias the actual fields named in their interfaces.
The mind constructor borrows +0x60 directly from the executable resource
record; a non-AID name preserves its previous view and an AID miss clears it.
Mind destruction clears requests before events and storage, then clears the
borrowed table without releasing it. The strategy sampler leaves its output
bytes and random state untouched on a miss, including signaling-NaN bytes.
Its constructor retains +0xE, while destruction rereads that word after the
leaving callback before setting state 1000 and dispatching entering.

Attribute acquisition keeps the registry reference, matching the dump's lack
of a release call. The six chance tests consume the shared LCG in retail order,
preserve chance equality and reject unordered comparisons. The timer at +0x974
retains its raw bits. A null attribute resource still passes address-zero +0x38
and +0x78 to name lookup; the caller must supply that real native address view.
No guard skips these name calls. The tag-index loop stops at the actual null-tag
sentinel, uses stride 0x87 and returns the low 16 bits used by 0x00028830.

`tests/player_parent_checks.cpp` runs through `--dialogue-resources` and the
opening preflight. It covers real retail mind/strategy parameter clones,
borrowed/missing/non-AID table lookup, signed counts, unsigned override values,
first duplicate matches, reverse random ranges, byte preservation, list anchor
cleanup, request/event/storage destruction order, callback-modified states,
counter wrap, tag sentinels, registry acquisition, retained references,
chance equality/NaNs and raw timers. Synthetic callback/resource fixtures stay
inside tests. The retail player mind's named table is absent from the current
executable-only pools; a live owner must supply its actual scene table provider.

Read-only exports establish parent type 7 at getter 0x00029D30 (vtable
0x004DD6F4), parent type 8 at getter 0x0002B6C0 (vtable 0x004DD74C), and body
type 0x27 at getter 0x000733F0 over type 6/getter 0x0001DA50. Both mind and
strategy parents derive from type 1. No new type has been installed prematurely.
The remaining body constructor chain includes 0x00024C50, 0x00073450,
0x000734F0 and 0x00075270 plus body destruction and virtual methods. Strategy
entering 0x0002BAF0 and the player's entering/leaving overrides still require
their full dependency chains. Player weapon setup 0x0013A720 depends on
attachment 0x0001B2B0 and detach 0x0013A7C0; their dumps are now recovered but
their untranslated callees still block completing player constructor 0x00028830.

Release `gbtg` and transition builds pass after correcting test fixture
expectations for absent tables and retained provider storage. Resource preflight,
actor loading, retail ghoulybox decoding and all six CTest cases pass. Opening
still fails at +0x254/type 0x6F: dialogue=0, cutscene before A=0, actors=1,
entities=1. Ghidra exports used rolled-back temporary ranges where necessary.
No OriginalFiles edits, symbol changes, staging or commits were made.

### Mind goals, body state and action batch (2026-10-10)

Twelve further routines are translated together before validation:

- Mind goal lookup 0x0002A770, numbered wildcard selection 0x0002A810,
  request allocation 0x00029D90 and resource decoding 0x0002A8E0 in
  `entities/player_mind_goals.h/.cpp`.
- Actor pause reference counts/message dispatch 0x00117400 and player
  strategy leaving 0x000BF410 in `entities/player_state.h/.cpp`.
- Body motion defaults 0x00025800, configured joint count 0x00073920,
  flagged shape collection 0x00075270 and special shape collection
  0x00026010, beside body action initialization 0x000278F0 and action
  creation/reset 0x00027B60 in the same module.

Goal lookup folds every byte above 0x20 with 0xDF, including punctuation.
Wildcard selection counts contiguous numbered names starting at 1, consumes
one shared LCG draw only after finding them, and ignores text after '?'. It
reuses the translated integer append routine 0x000FBEA0. Request allocation
uses the actual size table at 0x00217F18 and appends to the actual mind +0x54
anchor. Native pointer fields expand separately from the zeroed retail request
storage. Resource decoding uses the 49 actual goal names at 0x003397BC,
the exact size/skip test and payload offset +0x10, releases its registry
reference and returns success even when no recognized request is copied.
An allocation failure is not converted to a skipped request.

Pause counters wrap without clamping and broadcast only on their 0/1
transitions. The actor next pointer is saved before dispatch; the head is
reread for the second group. Leaving preserves the player's state-specific
counter behavior, body +0x30 increment and pause release. Virtual message
callbacks require actual scene mappings. Body action setup uses the existing
resource clone and object factory; an existing action receives message 3.
The kind at body +0x750 is a full zero-extended word, and existing +0x754
mode/value fields are preserved after message dispatch.

Both shape collectors test the terminal record before its zero stride.
Selected pointers grow four slots at a time through the existing heap
reallocation boundary, now also exposed from `entity_heap`. A zero selection
preserves the prior state allocation. Native special records retain +0x26
and +0x54 semantics with expanded host pointers. Model nodes and body storage
must be supplied by their real owning objects; these routines are not live
factory registrations.

Parent preflight now covers executable goal names and request sizes, exact
payload copies and registry release, empty goal nodes, case/punctuation
folding, wildcard gaps and random preservation, list cleanup, pause nesting
and callback mutation, leaving counter wrap, all 80 motion vectors and
untouched gaps, terminal shapes, pointer-array reallocation, retained empty
shape storage, raw action timer bits and repeated action reset.

Further read-only exports recover motion controller insertion/dispatch
0x0002F3C0, 0x0002F480 and 0x0002F6A0, and callback math 0x0002EEB0,
0x0002EF20 and 0x0002EF90. Joint motion setup 0x000734F0/0x00073A90 is still
blocked on the complete 0x0002F350/0x0002F060 chain, including container
insertion 0x00066310. Strategy entering 0x0002BAF0 still requires 0x00138870.
Body health 0x0001DB10 has an opening calculation omitted by its decompiler
output; translate its assembly rather than treating that output as complete.
The full body/player constructors and actual scene factory/spawn graph remain
required before the opening can pass.

Release game and transition builds pass. The new parent/dependency preflight
runs through `--dialogue-resources`; retail actor loading, ghoulybox decoding
and all six freshly rebuilt CTests pass. Preflight initially caught a fixture
gap assertion at +0x343, which is actually part of the explicitly cleared
+0x340 word; the corrected check uses +0x33F. The live opening remains at
script +0x254/type 0x6F, with dialogue=0, cutscene before A=0, actors=1 and
entities=1. No new live type registrations were installed. `git diff --check`
passes. Read-only Ghidra exports used rolled-back temporary functions; no
OriginalFiles edits, Ghidra symbol changes, staging or commits were made.

### Body controller, orientation and health batch (2026-10-10)

Twelve further routines are complete:

- Pool insertion 0x00066310, controller initialization 0x0002F3C0,
  sorted joint insertion 0x0002F480, callback registration 0x0002F350
  and two-callback dispatch 0x0002F6A0.
- Angle constraint 0x0002EF90, matrix angle extraction 0x0002F1D0,
  pitch adjustment 0x0002EEB0 and yaw adjustment 0x0002EF20.
- Body health initialization 0x0001DB10, health texture release
  0x0001DCA0 and body action/resource cleanup 0x00027580.

Controller/orientation code is in `entities/body_motion_controller.h/.cpp`;
health and cleanup code is in `entities/body_health.h/.cpp`. All reconstructed
definitions and declarations retain their Xbox addresses. The controller
reuses translated pool growth 0x000662A0, with native pointer expansion in
the function/data pools. It retains the extra joint sentinel, sorted insertion,
duplicate behavior and insertion order. Registration writes only the first
callback/data pair; dispatch rereads the second callback and joint after the
first call, preserving callback mutation. No live controller callbacks are
invented or registered.

Angle extraction preserves the dump's repeated addition/subtraction of 1
outside [-1,1], signed pitch endpoints and roll fallback when matrix +4/+0x14
are both ordered zero. Angle constraints retain the real remainder routine
and normalize positive pi to negative pi. Pitch/yaw adjustments use translated
angle extraction, sine/cosine, axis matrix creation and alias-safe affine
multiplication; their x87 arithmetic is kept through the final float store.

Health initialization translates the opening assembly omitted by Ghidra's
decompiler: actor +0xCF4 selects a stride-0x87 tag value at tag*stride-0x47,
byte endpoints map to the two float endpoints, and both ascending and
descending clamp branches remain. Zero tag/equal byte endpoints use the
maximum directly. Conversion goes through translated 0x001673D8 and keeps
the low 16 bits. Four real scoreboard texture names are acquired in retail
order; misses call the actual resource error routine. Their native views alias
body +0x98/+0x9C/+0xA0/+0xD8 and are released/cleared in order by cleanup.
Body action cleanup releases actual factory objects before clearing mode,
word and retained resource fields.

Dependency preflight covers controller initialization, sentinel growth,
sorted insertion, duplicate replacement, callback/joint mutation, preservation
of both callback pairs when moved, identity/wrapped/endpoint/fallback angles,
both angle constraint directions, positive pi, pitch/yaw postmultiplication,
translation retention, zero weight, health interpolation/clamps/equal byte
endpoints and cleanup field boundaries. Callback fixtures remain test-only.

Release game and transition builds, retail dependency preflight, actor loading,
ghoulybox decoding and all six freshly linked CTests pass. `git diff --check`
passes. Live opening remains at script +0x254/type 0x6F, dialogue=0, cutscene
before A=0, actors=1, entities=1. No premature type registrations were added.
No retail file changes, Ghidra symbol changes, staging or commits were made.

Next joint-motion work is acceleration 0x001010B0 and its distance helper
0x00100F30 (full read-only ranges now recovered), then the real callback
0x0002F060, record setup 0x00073A90 and 0x000734F0. Strategy entering still
requires 0x00138870; full body/player construction and scene-owned factories
remain necessary for opening acceptance.

### Joint-motion chain and live-blocker audit (2026-10-10)

The unchanged +0x254/type 0x6F marker has a concrete cause beyond missing
dependencies: `script_handle_actor_effects` in `scripts/script_actions.cpp`
still returns a literal 0 for that case. The separately translated
`wait_runtime_script_player`/0x001292D0 query in `script_actor_query.cpp` is
not wired into this Game caller. `NativeActorFactoryOwner::install` installs
only types 1, 2 and 0x15; the opening player template needs type 5 plus its
body/mind/strategy descendants. No complete scene-owned player address and
query graph exists in this caller. Moving the marker requires completing and
owning those real objects and binding the query, not returning 2 to skip it.
Prior repeated acceptance reports showed this same structural blocker; they
did not establish that the newly translated dependencies were exercised by
the live opening. The top-level status and source comments now say this
explicitly instead of describing the query itself as untranslated.

Six further routines close the full joint-motion setup dependency chain:

- 0x00100F30: signed sum of the two normalized angle legs through an anchor.
- 0x001010B0: angular acceleration/braking, maximum-speed branches, target
  crossing clamps and zero-distance velocity decay, translated from assembly
  where the decompiler misidentifies XMM return values.
- 0x0002F060: real joint callback, including unconditional matrix multiplication,
  record-word gate, pitch/yaw limit sentinels, constrained targets, smoothing,
  velocity resets and both matrix adjustments.
- 0x00073A90: accumulated limits, actual joint-index lookup, lazy controller
  allocation/aliasing and callback registration, exact 0x30-byte record writes.
- 0x000734F0: six record slots, initialization writes, actual joint count and
  heap allocation, ordered sum and per-slot parameter multipliers.
- 0x00073450: player body child constructor, raw parameter copies, complete
  joint setup, shape collection and ordered-zero counter increment.

The first three are in `body_motion_controller.h/.cpp`; setup and construction
are in `player_body_motion.h/.cpp`. All native controller/record fields must
alias their real actor/body owners. Record +2 remains untouched, missing joints
retain accumulated values, and the zero-count path preserves prior pointers.
Both limits are selected using the first limit's comparison, preserving the
dump's asymmetric condition. The final two slots copy maximum/acceleration
without multiplying by 1, preserving raw floating-point payloads.

Checks now exercise actual synthetic model-node-9 lookup, lazy controller
aliases, genuine callback registration, record limits and multipliers, missing
joint accumulation, no-joint pointer preservation, unordered body factors,
positive/negative acceleration, crossing clamps, zero-distance braking,
unconditional matrix multiplication, disabled limits and zero-weight resets.
Release game/transition builds, dependency preflight, retail actor loading,
ghoulybox decoding and six fresh CTests pass. The live handler/factory blocker
has not been bypassed or wired with fabricated objects. No retail file edits,
Ghidra symbol changes, staging or commits were made.

Next work should prioritize the remaining parent body constructor/destructor
chain (0x00024C50/0x00024F70), strategy entering/action chain and player
constructor 0x00028830, then real type registration and scene-owned spawn/query
binding. The body joint-motion chain is no longer an untranslated dependency.

### Parent body resource and collision batch (2026-10-10)

Nine further complete routines are in `entities/body_resources.h/.cpp`:
0x0001FA50 friction curve/tag settings; 0x000242E0 attribute table acquisition;
0x0001ED70 model channel initialization; 0x0001E9F0 ten-resource/hash cleanup;
0x00027740 action-resource reload; 0x0001B920 reference-cache release and last-row
compaction; 0x0013F150 collision state construction; 0x0013EE80 active collision
filter restoration; and 0x0013F240 collision state destruction. Native resource
and pointer fields must alias their eventual real body owners. Both executable
model-channel indices at 0x00217E44 are zero; the second probe is retained.
Friction reads its own tag column (stride 0x87 minus 0x37), not the health
column, and copies damping bits 0x3F7D70A4. Collision matching includes the
terminal zero-stride shape. Cached zero references wrap rather than gaining
an invented guard; last-reference removal copies the entire last native row.

New checks cover interpolation and clamp, exact damping/mode writes, null and
present model channels, retained/last/zero cache references, whole-row compaction,
terminal collision shapes and active-only filter restoration. One combined
Release build of game, transition checks and six CTest executables passed;
all six CTests plus retail dependency preflight, actor loading and ghoulybox
decoding passed. No opening rerun was needed: the live 0x6F constant-zero handler
and missing player factory ownership are unchanged, so this is dependency
progress, not evidence of advancing the live script marker.

Read-only dumps for the next constructor chain are now cached: 0x0001C220,
0x0001C1C0, 0x00066140, 0x00035D90, 0x00040980, 0x00027830, 0x00027890 and
0x0001AF10. Full 0x00024C50/0x00024F70 must still wait for every real callee;
do not install a partial body constructor or fabricated return values.

### Complete parent body animation chain (2026-10-10)

Eleven further routines now live in `entities/body_animation_setup.h/.cpp`:
0x00066140 rounded bit storage; 0x0001B9E0 retained group-cache lookup;
0x0001C400 sequence-length histogram insertion/counting and pool growth;
0x0001C490 packed native allocation size; 0x0001C4C0 group-header/pointer-table
layout; 0x0001C220 complete sequence grouping and shared cache insertion;
0x0001C1C0 maximum animation/group count; 0x0001CB60 animation resource and
bit allocation; 0x0001D360 body animation settings and actor-class override;
0x00051CA0 model record counting; and 0x00040980 node-two model lookup.
This completes the animation setup callee chain of the parent constructor.

The shared cache entry's retail +8 field is a resource identity pointer,
now represented as a native pointer rather than an integer. Resource +0x10
sequence pointers require owner-provided native views keyed by the actual
CPU resource identity, including newly acquired resources. The raw +0x10
gate remains in the translation; no relocation fallback or substitute callee
is supplied. Group rows retain pointers to each original 16-bit sequence word,
group by first-seen length, and allocate the headers and pointer tables in
one heap block. Cache lookup retains the existing block and copies its count;
cache misses preserve the output count. Empty sequence paths preserve the
body count byte, matching the dump. Native pointer expansion affects allocation
sizes and header strides without replacing heap allocation with vector growth.

Checks exercise actual multi-length sequence grouping, individual word pointers,
two-body shared cache retention/last release, rounded and zero-bit allocations,
raw NaN parameter-bit preservation, matching class override, model record stride
and null model behavior. One combined Release build of game/transition and all
six CTest executables passed, as did six CTests, dependency preflight, actor
loading and ghoulybox decoding. No retail edits, symbol renames or commits.

The full parent lifecycle is still pending action-row selection/attachment
callees and resource-hash acquisition (0x0001E7C0, including the exact loaded
resource-size query 0x00035D90). Live type 5 ownership/query binding remains
absent and type 0x6F remains constant zero. Do not treat these dependency tests
as opening-script acceptance.

### Complete parent body lifecycle and action batch (2026-10-10)

Thirteen further routines complete the parent body constructor/destructor and
their remaining direct dependency chains:

- `body_resources.h/.cpp`: 0x00035D90 exact loaded-resource size query and
  0x0001E7C0 ten-resource acquisition/hash collection. The size query scans actual
  one-based resource slots and adds raw loaded +0xC/+0x14 words, plus the first
  dword of +0x10 storage when present. It does not substitute a CPU span length.
  The future scene loader must supply these genuine retained raw fields through
  `RuntimeBodyLoadedResourceSize`; the older host registry spans alone cannot
  establish their numeric values. Hash collection skips resource zero, allocates
  four rows initially, takes only the first dword on its first allocation path,
  and otherwise deduplicates eight-byte resource rows and grows by four.
- `body_action_selection.h/.cpp`: 0x0001B020 snapshots both real virtual message
  recipients and sends animation notice 0x28; 0x0001AF10 performs real variant
  lookup/fallback, starts the real actor animation and notifies; 0x00027830 and
  0x00027890 clone distinct optional template slots or executable resources,
  override body token +8 and construct through the real object factory;
  0x000276A0 releases both prior actions, selects row/key/animation and creates
  replacements; 0x00027650 performs the actual signed binary table lookup.
- `body_attachment_cleanup.h/.cpp`: 0x0003CFD0 searches the primary then secondary
  attachment lists, invalidates secondary timed records, unlinks and frees;
  0x00025090 clears the body's recorded tokens while rereading its count;
  0x0001B4C0 sends 0x9D, performs real actor bind detachment, clears the owner,
  sends 0x9C, adjusts class-three count and broadcasts 0x73 through the existing
  scene pulse implementation; 0x00024F70 is the complete parent destructor.
- `body_construction.h/.cpp`: 0x00024C50 is the complete parent constructor,
  including shared LCG scale variation/class exclusions, parameter bit copies,
  health, flags, world transforms, actions, animation, friction, attributes,
  model channels, collision/hash/special-shape allocation and final action key.

Native attachment and first animation-receiver fields alias the same owners
across action selection and detach contexts. Required relocated strings/tables
and resource-size views must map real scene objects; no missing function is
replaced by a callback. The destructor deliberately repeats group/resource/bit
cleanup after unload callbacks, matching the assembly. Collision destruction
receives body +0x2C8 in EAX; the decompiler's apparent actor stack argument
is not the state pointer. Group count bytes and other fields absent from the
cleanup dump remain untouched.

Checks now run the complete parent constructor and destructor with translated
callees and verify exact random scaling, excluded-class random preservation,
raw parameter bits, world position, damping/constants, conditional byte writes,
real allocations and cleanup. Additional checks cover hash growth/deduplication,
raw resource-size terms, both optional action-template clone paths, prior-action
factory destruction, snapshotted recipients, timed secondary removal and detach
message order. Registry-reference release is checked separately from provider
unload, which is deferred by the existing resource registry.

Release game, transition checks and six freshly linked CTest executables pass;
all six CTests, dependency preflight, retail actor loading and ghoulybox decoding
pass. No retail edits, symbol changes, staging or commits. Live 0x6F remains
constant zero; these lifecycle checks do not establish opening acceptance.
Next priority is full player 0x00028830 and strategy entry, then actual factory
ownership/query binding. Their next ranges 0x00107830, 0x0013A720, 0x00040200
and 0x00138870 have been exported read-only for continued work.

### Mind events and attachment helper batch (2026-10-10)

Thirteen more routines are translated from their cached dumps:

- `player_mind_events.h/.cpp`: event allocation 0x0002A190, refresh/rehome
  0x0002A280, target-event matching 0x0002A0C0 and target setter 0x0002A030;
  position request 0x00136A70, current goal flags 0x000283A0 and direction-angle
  query 0x00105F90. Descriptor flags determine event reuse; reuse restores the
  original duration rather than replacing it with the requested duration.
  Refresh unlinks and prepends at the actual list root. Clearing a nonzero
  target sends the real actor virtual message before overwriting the token.
  Position requests call the existing allocator and preserve raw coordinate
  bits. Empty goal rings require the genuine address-zero view. Angle tests
  follow the assembly's JA comparisons, including unordered NaN behavior.
- `player_attachment_helpers.h/.cpp`: class value lookup 0x00146190, host
  assignment/notification 0x001461C0, eligibility 0x00146100, bind allocation
  0x0003B9D0, model joint binding 0x0003B840 and host notification 0x0001B770.
  Lookup sign-extends row values, host assignment retains the actual actor
  token, new binds initialize both identity matrices, and joint lookup calls
  the existing translated model helper. Activation rereads host state after
  the real virtual message callback.

Native mind events retain LiveScript links so the existing base-mind cleanup
unlinks and frees these allocations. Their retail payload is separately retained
at the exact descriptor size; scene ownership must alias the embedded anchor
and provide actual executable descriptors and actor virtual dispatch.

One combined Release build covered the game and all CTest executables, followed
by the transition target with the new fixtures. All six CTests and the retail
dialogue-resource preflight pass (1357 template clones, 457 localization keys).
Checks cover event reuse/reordering, exact clock/reset writes, notification
order, request bits, zero-address ring reads, NaN comparisons, attachment
eligibility, real model lookup and retained bind transforms. No retail edits,
staging, commits or Ghidra project changes.

Full strategy entry remains pending 0x00138870's target-query chain:
0x00129640, 0x00129920 and 0x001299B0. Assembly distinguishes its category mask
stack argument from predicate flags in EAX; these must remain separate. Full
player construction still needs 0x0001B2B0's attachment chain. Existing input
creation 0x00107830 and matrix refresh 0x00040200 are already translated and
should be reused. Live 0x6F remains constant zero until the real player factory,
scene ownership and readiness binding are complete.

### Complete base strategy entry and target query (2026-10-10)

Five substantial routines close the full base-entry dependency chain rather than
leaving a partial strategy handler:

- `player_mind_targets.h/.cpp`: target eligibility 0x00129920, reachability
  0x001299B0 and nearest-target search 0x00129640. Eligibility calls the actual
  actor activity and body-bit queries. Reachability computes the radius-offset
  destination using retail angle/sine/cosine, rereads world position, calls the
  translated navigation query and preserves the occupancy identity/group tests.
  Target search keeps separate category-mask and predicate-flag arguments,
  masks category shifts to five bits, filters real actor classes/identity,
  preserves SSE distance order and COMISS tie/unordered behavior, and only writes
  the output distance when a target was found.
- `player_strategy_entry.h/.cpp`: player goal entry 0x00138870 and complete
  base state entry 0x0002BAF0. Goal entry retains actor status/body state gates,
  null versus empty-ring behavior and literal address-zero reads; it appends
  real position requests and clears their duration on the exact retail conditions.
  Kind ten uses mind +0x68 as category mask, mind +0x64 with bit 0x20 removed
  and bit 8 added as predicate flags, then calls the real target setter and goal
  loader with the actual 0x0031C7AC string and 0x0031B548 fallback. Base entry
  selects the executable goal names, preserves byte increments/fallthrough, and
  state 0x3FC directly clears the target and frees only the first kind-three event
  without sending the target-setter notification.

These contexts require actual scene actor/body/mind ownership, target registry,
occupancy records, ring and executable string views. They do not install fake
targets, substitute goal-loader results or connect the legacy live 0x6F handler.

Checks exercise independent masks, signed class identities, category shift wrap,
ties and first-NaN distances, unchanged miss output, all eligibility predicates,
occupancy identity/group exemptions, missing-grid rejection and a genuine grid
path whose success depends on radius adjustment. Full entry fixtures verify all
position-request kinds, actor/ring timing suppression, raw coordinate copies,
empty-ring address-zero reads, target-event creation, body state transitions,
byte wrap and single-event removal. Goal-selection tests use the actual executable
strings and translated resource loader, including both target-goal success and
fallback while retaining the preceding position request.

One combined Release build of game, transition checks and all six CTest targets
passes. All six CTests, retail dialogue-resource preflight (1357 template clones
and 457 localization keys), actor loading and ghoulybox decoding pass. No retail
edits, symbol changes, staging or commits. The next constructor dependency is
0x0001B2B0: integrate the attachment helpers with the real body action selection,
pose update and formatting/class-string chain, then complete player 0x00028830.
Live player factory ownership/readiness binding remains missing; opening gate
progress must still be reported separately from this dependency completion.

### Complete player constructor and attachment lifecycle batch (2026-10-10)

Seven further routines close the remaining constructor dependency chain:

- `runtime/crt_format.h/.cpp`: 0x001661A2 varargs formatting uses the existing
  translated 0x00168FED output engine and preserves the string-stream termination
  branches. No host printf replacement is used.
- `player_attachment.h/.cpp`: body variant formatting 0x00020B30, full attachment
  0x0001B2B0, release 0x0001B810, replacement 0x0013A7C0 and factory creation
  0x0013A720. Attachment retains the real accepted-class gate, body/receiver
  ownership and flag updates, executable weapon/class names, model-joint or
  parent binding, actual body virtual action-key lookup, action selection,
  animation notification, class-three count, pose update and host notification.
  Release snapshots both attachments, preserves the inline mode/host tests
  (which do not include the separate availability helper's +0x941 busy byte),
  force override, mind-action recipient path and secondary cleanup. Replacement
  uses actual bind-tree activation on its unavailable path and resets retained
  player blend storage/controller dirtiness. Creation clones the real executable
  resource, uses the actual factory, destroys on attach failure and broadcasts
  the actual 0x74 message on success.
- `player_construction.h/.cpp`: complete 0x00028830, including mode/tag and world
  transform copies, the exact 0x0032B0F4 half-scale random phase, shared attribute
  and spawn setup, body/mind/strategy child setters, actual base input allocation,
  conditional body animation channel initialization/random advance, optional
  attachment creation, player count/story-flag writes and final matrix refresh.
  Native input and factory attachment ownership are retained explicitly rather
  than truncating native pointers into retail words. The provisional +0x8C write
  uses the actual DAT_00338EE0 word before the retail random overwrite.

Calling-convention verification also corrected the earlier 0x0001B4C0 detach:
0x0003B560 consumes the attached actor in EBX, not the body's host actor.
`RuntimeBodyAttachedActor` now retains that genuine native owner, and the detach
clears both its native body owner and the raw +0x48 word. A regression fixture
uses distinct host/attached binds and verifies only the attached bind is freed.
Host assignment keeps the native +0xD8C owner visible before the virtual notice.

Checks cover rejected classes/occupied body, exact variant output and notice order,
real parent bind/pose update, protected versus forced release, distinct secondary
and absent/present mind recipients, actual blend cleanup, varargs string/integer
formatting, clone-prefix rejection, failed factory construction and failed attach
cleanup. Full constructor fixtures check transform/tag/input fields, the initial
LCG phase, six real attribute advances followed by the body channel advance,
optional weapon creation, story flag, counts and retained ownership cleanup.
Their factory callbacks are test fixtures supplying native child instances; they
are not installed in a Game and do not validate live player spawn. The parent
body constructor has its separate complete translated-chain checks.

One combined Release build of the game, transition checks and six CTest targets
passes. All six CTests, retail dialogue-resource preflight (1357 template clones,
457 localization keys), actor loading and ghoulybox decoding pass. No retail
edits, staging, commits or Ghidra project mutations. Live 0x6F remains unchanged.

Next lifecycle work is player destructor 0x00028A30, particularly goal-ring removal
0x00028240 and unregister 0x001108B0, followed by the real player virtual slots
0x00028F60/0x0002C2B0/0x00028050 and their dependencies. Getter 0x00027F60 installs
these as well as constructor/destructor into vtable 0x004DD6B8. Complete those
before installing type 5 and its body/mind/strategy descendants in the real
scene owner and binding the translated readiness query to the live script gate.

### Complete player destructor and goal cleanup batch (2026-10-10)

Eight routines complete another lifecycle dependency chain:

- `player_goal_cleanup.h/.cpp`: exact six-byte body latch mask 0x0001F7A0,
  increment 0x0001FC40 and decrement 0x0001FD20, goal classification 0x000283D0,
  script override restoration 0x00138030 and complete goal removal 0x00028240.
  Counters wrap as bytes, and leaving mask 0x3F notifies the actual body owner.
  Recipient traversal saves the next node before the virtual call. Goal removal
  preserves all eight excluded goal kinds, circular head advancement, null versus
  empty-ring/address-zero reads, actual heap release, the two-word 0x79 broadcast,
  and conditional restoration of the actual five-word 0x86 override messages.
  Override restoration snapshots player mode but rereads the second global
  enable/payload after the first callback, retaining both raw float words.
- `navigation/cell_registration.h/.cpp`: 0x001108B0 walks fixed 0xC-byte records,
  decrements either the low five bits or bits 5..8 of the actual selected grid
  cell, preserves unrelated bits and releases the retained heap header. Empty
  registration avoids the grid lookup; there are no invented saturation guards.
- `player_destruction.h/.cpp`: complete 0x00028A30 decrements the player count,
  clears +0x98B before draining goals, clears strategy/requests through the real
  setter, releases mind/input/body/attributes/spawn storage through translated
  dependencies, unlinks all four actual intrusive lists, unregisters navigation
  cells and clears the nonzero tag index. The input dispatch retains kinds 1..5
  and deliberately leaves the retail +0x960 word/native dangling view unchanged
  after release, as the dump does. This destructor is single-shot.

Fixtures cover independent latch bits, byte overflow/underflow, list mutation
during callbacks, every goal exclusion, circular goal removal, empty-ring literal
zero-address reads, final-goal overrides, mode/global changes during virtual
messages, navigation counter wrap and bit preservation. The constructor fixture
now tears down through the full destructor with multiple goals, actual factory
body/input release, all four list backlinks, count/tag/flag cleanup and the
unchanged input word. Its enabled overrides also verify destructor suppression
of restoration by clearing +0x98B before draining the goals.

The combined Release build of the game, transition checks and six CTest targets
passes. All six CTests, dialogue-resource preflight (1357 template clones and
457 localization keys), actor loading and ghoulybox decoding pass. Ghidra exports
remain read-only; there are no retail edits, staging or commits.

The remaining player lifecycle frontier is virtual dispatch 0x00028F60 with its
0x00028FC0 message handler, update 0x0002C2B0 and collision wrapper 0x00028050
with its dependencies. Type 5 registration and actual scene/body/mind/strategy
ownership still await these translations. Live +0x254/type 0x6F is unchanged;
this batch completes destructor dependencies rather than advancing that gate.

### Player message dispatch, activation and navigation registration (2026-10-10)

Nineteen routines complete the player virtual message slot (`player_messages.h/.cpp`, plus
`navigation/cell_registration`):

- 0x00028F60 runs the player handler, then forwards the message through vtable +0xC of the
  strategy, mind and body children (+0x954/+0x958/+0x95C), rereading each slot after the
  previous call and skipping absent children.
- 0x00028FC0 switches on the message id and always finishes in the base receiver 0x0003EF70:
  - 7: 0x00019150 fades animation slot `message.slot` when the event kind (+0xC) is 5. It writes
    (1 - position) * player duration and fade mode 3, and marks the controller dirty. Slot 0 is
    never touched.
  - 0xE: 0x000294F0 replies to the sender at message +4 with id 0xF, the six words at +8 and the
    zero-extended +0x20 word.
  - 0xF: 0x00029590 sends {0x2D, self, sender} to self, then reuses the same stack record with
    the owner and sender words swapped for the sender.
  - 0x23: activation 0x00029270, then the pulse broadcast {0x71, self}.
  - 0x2D: sets bit 1 of the first body-bit byte (body +0x15C, +8).
  - 0x6E/0x70: when the message names this actor, carries a nonzero +8 word and attribute word
    +0x28 (0x000196D0) is set, 0x00019660 resets the attribute timer with value 0.
  - 0x7C: when the message names this actor, the pulse broadcast {0x76, self}.
  - 0x7D: 0x00029490 registers navigation cells around +0xCC0 with radius body-owner +0x38 times
    body +0x888, unless that product equals zero (NaN still registers).
  - 0xA7/0xA9/0xAA: 0x00028700/0x00028770/0x000287C0 accept a spawn, variant or alternate
    variant only when +0xBA0 does not start with `aid_` and +0xB9E is zero. They copy the names
    to +0xBA0 (and +0xC20 for 0xA7), select mode 0/1/2 at +0xB9C and write result 1, otherwise -1.
  - Any other id: 0x00027FB0. Message 9 increments +0xCD8. The first pause moves the strategy to
    state 1000 through 0x0002B8D0 and drains every goal (0x00028210) when 0x00028360 reports an
    active mask. Message 10 decrements the count. At zero, if 0x00028D50 allows it, the strategy
    returns to its +0xC state. 0x00028D50 picks reference group 1 for mode 2 or a linked +0x93C,
    and otherwise returns DAT_0030F144[DAT_0051248C].
- 0x00029270 activation:
  - Inserts node +0x934 into DAT_00511B34. Mode 1 also inserts +0x944 into DAT_00511B44. Mode 2
    inserts +0x94C into DAT_00511B4C and appends +0x93C at the tail of DAT_00511B3C.
  - Stores world position and euler at +0xCC0/+0xCCC.
  - Records occupancy hits at +0xCA0 (mask 2, when +0x1FC is set) and +0xCA4 (mask 4, actor tag
    group) for modes 1 and 4.
  - Resets the attribute timer for excluded tags (DAT_0050E228/0x0050E22C).
  - Sets +0x98B when the first goal kind is nonzero; otherwise restores script overrides
    (0x00138030).
  - Stores the navigation floor at +0xCEC.
- 0x001104E0 navigation cell registration:
  - Allocates the 0x40-byte row header (stride 0xC, growth 4) on first use. Later calls first
    release earlier rows.
  - Bounds the four radius corners through 0x001109E0/0x00110970.
  - A cell within (half grid-0 cell + radius) of the centre increments its low five bits when
    `include_outer` is set, when it is inside the radius, or when it is the centre cell; otherwise
    it increments bits 5..8.
  - Rows grow by four through the heap reallocation boundary.
  - `NavigationCellRegistration` gains the retail capacity, stride and growth fields. The release
    0x001108B0 is unchanged.

Native ownership notes:

- The list nodes are the same native `ListHead` aliases used by the destructor. Node +0x934 must
  be the link of an `ActorWorldMessageNode`.
- The +0x93C word and `Actor::mask_flag` receive the predecessor's retail address through the
  activation's address map. The other raw link words are not mirrored.
- +0xCD8 updates both the raw word and `Actor::parked_gate`, which other readers use.
- The +0xBA0 copies keep the spawn controller's first-name-byte alias in sync.

Verification:

- `tools/oracle/navigation_oracle.py` runs retail 0x001104E0 (with its callees and hooked heap
  allocation/reallocation) against the host build. Each case has 1-3 successive registrations,
  random grids, origins, NaN and negative radii, and missing floors. It reports zero mismatches,
  and catches deliberately broken builds (wrong half-cell grid, growth size, NaN test).
- `tests/player_message_checks.cpp`, called from the dialogue-resource preflight, covers:
  - fade slots, timers and attribute reads;
  - contact replies;
  - all spawn acceptance and rejection paths;
  - strategy resume groups;
  - transitions, nested pauses and goal draining;
  - registration counters, growth and release;
  - mode 1 and 2 activation with list ordering, occupancy, excluded tags and goal-deferred
    overrides.
- The ghoulybox receiver check (`--actor-loading`) also sends 0xA9, 0x2D, 9 and 0xF through the
  full dispatch with the real base receiver. This verifies child forwarding order and the
  absent-child skip.

The Release build of the game and transition tests passes, as do all six CTests,
`--dialogue-resources`, `--actor-loading` and `--ghoulybox-decoder`. The build was run on the
user's PC through `cache/_run/build_and_test.bat`, which writes `cache/build_log.txt`.

The remaining player frontier:

- Update 0x0002C2B0 (about 30 untranslated callees, led by 0x0013F7B0, 0x00026100 and 0x0013EBE0).
- The render wrapper 0x00028050 with its overlay chain: 0x0001DF70, 0x0001E3E0, 0x0001E2A0,
  0x0001E770, 0x0002AC70, 0x0003DD50, and the billboard queue 0x0005C1A0.
- Type 5 registration and live scene ownership.

Live +0x254/type 0x6F is unchanged.

### Player render wrapper and body overlay (2026-10-10)

Eight routines complete the player draw slot 0x00028050 (`player_overlay.h/.cpp`, plus
`graphics/billboard_queue`):

- 0x00028050 returns 0 without drawing when +0xCE0 is set, the actor is mode 2 or linked at
  +0x93C, and bit 2 of DAT_0050EA9C is set. Otherwise it draws the actor (0x0003C9E0), queues the
  body overlay and calls body +0x744's vtable +0x18 when present. It returns the draw result.
- 0x0001DF70 runs only in pass 2:
  - A body counter (+0xE0) uses sprite +0xD8, white, the truncated +0xDC value, size 3 and rises
    of 3.
  - Otherwise both +0xA4 and +0xA8 must be set. It picks sprite +0x98, or (when 0x0001E770 allows
    the alternate overlay) +0xA0 or +0x9C by +0xA9. Colour comes from 0x0001E2A0 with alpha
    +0xA5, the value is health +0x94, size is +0xBC, and the rise is 5.
  - The anchor (0x0003DD50) is world position plus radius_y * size_z, raised by
    |(size_y - size_z) * radius_y|. Case class 3 adds 6, via state 3 or state 1 with
    0x00145FF0 on the body +0x48 actor.
  - The sprite frame is trunc(frames * actor +0x924) mod the sprite's +0x11 byte.
  - After queuing the sprite, the digit anchor moves by (digit rise - rise), then sideways along
    the normalized camera side vector (0x0002AC70) when the lateral offset is nonzero.
- 0x0001E2A0 picks one of four colours by health ratio (< 0.25, < 0.5, < 0.75).
- 0x0001E770 allows the alternate overlay when +0x98C is clear, +0x978 > 0 or +0x969 is set, and
  the attributes have no +0xB8 word.
- 0x0001E3E0 splits the value into at most four decimal digits. It measures each digit sprite
  (u16 +8 * 0.02), centres the run with 0.2 gaps along the camera side vector, and queues them
  highest digit first at size 1, frame 0xFF.
  - The translation mirrors the retail stack slots, which overlap. For four-digit values, retail
    reads the top digit's index from the first width's float bits, and advances by the stored
    sprite x. These reads are reproduced rather than corrected.
- 0x0005C1A0 appends to the 0x800-entry billboard queue (DAT_004EE330, count DAT_00504330):
  sprite, position, size, rotation, colour, frame. A full queue drops the sprite. The consumer
  of this queue is not translated yet.

Verification:

- `tools/oracle/overlay_oracle.py` runs retail 0x0001DF70 and 0x0001E3E0, with all their
  callees, against the host build. It uses values up to 999 and exact colour-threshold ratios,
  and reports zero mismatches over 400 cases. Deliberately broken builds were each caught:
  - case-class rise
  - alpha byte
  - side direction
  - frame modulus
  - digit gap
  - half-ratio threshold
  - alternate-overlay timer test
- `tests/player_message_checks.cpp` adds fixtures for:
  - digit centring and order
  - zero and a full queue
  - colour bands
  - alternate-overlay gating
  - a counter overlay's anchor, rise and sideways digit offset
- The gating and child call in 0x00028050 have no fixture, because the full actor draw context
  is not available in these tests.
- A complete `run.sh` pass now takes longer than one three-minute call, so each new oracle was
  run on its own.

Confirmed on Windows through `cache\_run\build_and_test.bat`: the Release build of the game and
transition tests (including `player_overlay.cpp`, `billboard_queue.cpp` and the new fixtures)
passes, as do all six CTests, `--dialogue-resources`, `--actor-loading` and `--ghoulybox-decoder`.

The remaining frontier is player update 0x0002C2B0 (about 30 untranslated callees, led by
0x0013F7B0, 0x00026100 and 0x0013EBE0), then type 5 registration and live scene ownership.

### Player update, body collision and contact walk (2026-10-10)

Twenty-one routines complete the player update slot 0x0002C2B0. A fresh `CalleeTree.java` export from
the update (saved as `cache/ghidra_frontier/callee_tree_0002C2B0.txt`) listed 22 untranslated
static callees plus the update itself and its contact callback 0x0001F8C0. The inline
0x000F7100/0x0014DED0/`HeapFree` and 0x0014E872 sequences are the same heap boundary as
`engine_heap_free`/`engine_heap_allocate` (0x000F72D0 disassembles to exactly that wrapper).

- `player_update.h/.cpp`:
  - 0x00019520 counts actor +0x974/+0x978 down while decay runs and sends 0x55/0x56 at zero; a NaN
    timer is left alone.
  - 0x0002B450 records the type-4 occupancy record at +0xCA8, latches +0xCB8 for a foreign record
    (group 0 or the actor tag), otherwise keeps the last free position at +0xCAC while body +0x30
    and +0x34 are zero, and sends 0x5D for a latched, unlinked mode-1 player with a strategy.
  - 0x0002C980 sends 0x33 outside DAT_0050EA04..0x0050EA18 (the top is max(+0x14 bound, y) + 1,
    with the retail NaN choice), or for a mode-1 player off the navigation grid whose first goal
    kind is zero (address zero for an empty ring).
  - 0x0002C2B0 itself: input reset/refresh by device kind and first goal kind; strategy and mind
    ticks only while +0xCD8 is not positive; body ticks, body-bit clears and the animation
    tick/apply around body +0x14; for a free body the filter (+0x1F0) and shape mode (+0x118) are
    cleared around the shape-probe re-registration and sphere pass, with the support query setting
    or clearing body bit 0x10; a carried body (+0x258) records its offset in the motion history,
    moves the actor to it and copies the owner back; then transform refresh, frame attachments,
    attachment effects, position easing, shader animations, body +0x18, the contact walk, the
    +0x744 action key (looked up and selected with the zero template token), body position/yaw/
    euler copies, timers, room tracking, floor tracking with 0x24, cell registration or release,
    the bounds check, pause decay and the actor clock.
- `body_collision.h/.cpp`: 0x00025A30 writes a delta into the body +0x340 ring (divided by
  rate x step when positive) and wraps at 0x50. 0x00025F00 builds the world matrix at body +0x18
  and counts pushes in body +0x716. 0x00026100 sweeps each sphere up to three times (kind 0
  through 0x001226C0 with the radius as the opaque word, kind 1 from the far surface point through
  0x00122480), copies the first record once, pushes the centre along the record normal by at least
  0.05, zeroes Y when body-bit byte +5 has bit 2 set, and clamps the lateral move to step x rate x 20 (DAT_003390C8),
  with x87 products kept in `long double`.
- `navigation/floor_tracking.h/.cpp`: 0x00110C90 releases the old floor's registration on a change
  and resets the mind path search (0x00036AF0) over the new grid. Ghidra's stack reading here was
  confirmed under Unicorn after a manual count disagreed.
- `entity_collision.cpp`: sweep 0x00122480 (vtable +0x1C), point probe 0x001229F0 (+0x2C) and
  multi-contact collection 0x00122B00 (+0x30, one slot pool per hit). `SegmentProbe` gains these
  slots after its existing fields, so the head sentinel aliasing is unchanged.
- `segment_records.h/.cpp`: 0x000FCC90 copies a slot's 0x54-byte records into a heap pool;
  0x000FC6C0 picks the nearest sphere surface point among records whose +0x28 byte is 1.
- `body_contacts.h/.cpp`: 0x0013F2C0 ages the override rows (0x0013ED20), and while the frame
  advances or +0x1C forces it, clears the player filter, suspends override actors (0x0013EE10),
  walks the active shapes (0x0013F7B0), restores (0x0013EE80), drops the removal list's actors
  from the overrides and restores the filter. 0x0013F7B0 refreshes each shape centre, builds the
  flag mask (0x220002/0x40/0x100), notifies every multi-contact hit at its nearest point, then the
  single point hit, stopping when the callback asks or clears the active list. 0x0013FB50 sends
  the 0x14-byte contact record and velocity (delta x 60) to the callback, then 0x0013EBE0 adds an
  override unless the actor is a listed player, has a +0x908 link or is already present.
  Removals in 0x0013EE10 and 0x0013F2C0 copy the last row over the removed one (0x000F71F0 is
  called with EAX/ECX/EDX; the decompile hides this).
- 0x0001F8C0 sends 0x15 to the hit actor and 0x41 (carrying the reply) to the player, and on a
  reply bumps the per-tag counters DAT_0053E528/DAT_00524E2C only for the first contact counted
  at body +0x2CC.

Representation changes:

- Override rows now hold a native `Actor*` and 0x0013EE80 restores `Actor::model_flags`, which is
  the +0x1F0 filter that segment-probe registration reads. The existing fixture was updated.
- `RuntimeBodyCollisionState` names +0x10 (active shape list, linked through the new record
  `next`), +0x14 (walking), +0x18 (removal pool of `Actor*`) and +0x1C (forced).
- Child and receiver virtual methods (body +0x10/+0x14/+0x18/+0x1C, strategy/mind +0x10, body
  +0x744 +0x1C, probe +0x1C/+0x20/+0x2C/+0x30, actor +0xC) are context callbacks; their actual
  class methods are not translated yet.
- Hit actors are reached through a `contact_actor` mapping from their `SegmentProbe`.

Verification:

- `tools/oracle/update_oracle.py` compares 0x00025A30, 0x00110C90, 0x00019520, 0x0002C980,
  0x0002B450 and 0x00025F00/0x00026100 with retail code: 600 mixed cases, zero mismatches.
  Mutants for the lateral rate and the Y clear were caught.
- `tools/oracle/contact_oracle.py` runs retail 0x0013F2C0 with hooked probes and either a hooked
  callback or the real 0x0001F8C0: 700 and 600 cases, zero mismatches. Mutants for the player
  check, mask flag, velocity scale, row age, report reply and forced walk were all caught.
- `tests/player_update_checks.cpp` (from the dialogue preflight) drives 0x0002C2B0 with recorded
  children: tick order, pause gating, input reset, body bits, filter restore, easing, body copies,
  the carried-body motion sample, a floor change with path-search reset and 0x24, cell
  registration and release, pause decay and the clock.
- Confirmed on Windows through `cache/_run/build_and_test.bat`: the Release game and transition
  builds pass, as do all six CTests, `--dialogue-resources`, `--actor-loading` and
  `--ghoulybox-decoder`.

The remaining frontier is type 5 registration with the actual body (0x27), mind and strategy class
methods behind the callbacks above, and live scene ownership. Live +0x254/type 0x6F is unchanged.

### Player body child destruction and rotation easing (2026-10-10)

Five further routines are translated before the batch build:

- `player_children.h/.cpp`: 0x00076180 reports the type 0x27 parent's actual
  instance size 0x924 and parameter size 0x820. The player body child remains
  0x950/0x820 through 0x00075AD0; these descriptors are distinct.
- `player_body_motion.h/.cpp`: 0x000734B0 releases the selected shape state
  (+0x89C) and shape pointer allocation (+0x8A0), clearing each pointer after
  release, then calls complete joint-storage release 0x00073760. That routine
  clears live slots from +0x910 backwards to +0x8AC and releases the last pointer
  encountered, the lowest live slice of the shared joint allocation. Shape
  counts, joint latches and motion controller ownership remain with their retail
  owners; the child destructor does not clear or release them.
- 0x00101280 eases an angle using a wrapped delta, gain, minimum/maximum speed
  and elapsed time. Assembly recovers the target in XMM0 and the return in
  XMM0, omitted by the decompile. The overshoot comparison uses the signed step:
  a positive overshoot preserves the current angle, while a negative step can
  pass the absolute delta. The resulting angle wraps into a positive turn.
  Unordered comparisons retain the retail arithmetic path.
- 0x000265D0 applies that easing on all three axes only when body +0x728 is
  nonzero. It reads the actor's actual world euler, uses frame step and actor
  rate, normalizes into the local actor euler and clears +0x2A8. Its final world
  euler read uses the actual actor at `[actor +0x95C] +8` and writes that body's
  +0x72C output, rather than assuming it is the original actor.

The existing constructor fixture now releases the joint allocation through
the real child destructor, including multiple live slices and unchanged counts
and latches. Additional checks cover descriptor sizes, gain and both speed
limits, signed overshoot, wrapping, NaN targets, disabled rotation, pause gating
and a distinct output owner. These are host fixtures; no new differential
Unicorn oracle was run for these routines.

Read-only Ghidra exports from `Z:\GhouliesStuff\ghidra_projects\GbtG` recovered
0x00074080 (body message), 0x000744A0 (body relay), 0x00073820 (joint target
application), 0x00073C30 (joint target selection), 0x00074670 (body collision),
and 0x00025AE0 (body apply, reached by the tail jump at 0x00074490). Additional
frontier dumps are available for 0x000743E0, 0x000744F0 and 0x00075120.
The relay still requires complete 0x00073820/0x00073C30; the body message,
collision and apply chains still require their genuine callees. No partial
method table or factory registration was installed. The earlier constructor
0x00073450/0x000734F0 chain was already translated before this batch.

Validation: Release `gbtg` and `gbtg-transition-tests` builds pass, as do
`--dialogue-resources` (including these fixtures), `--actor-loading`,
`--ghoulybox-decoder` and all six CTests. The opening was not rerun; the last
recorded live gate remains +0x254/type 0x6F. Type 5 registration, complete body,
mind and strategy methods, and live scene ownership remain outstanding.
No retail payload edits, Ghidra symbol changes, staging or commits were made.

### Player body joint targets and complete relay (2026-10-10)

Four more body routines are translated in `player_body_motion.h/.cpp`:

- 0x00073C30 selects a target for one of the six joint slots. The disable-byte
  table DAT_002FFE68 is `{2, 2, 1, 0, 3, 3}`. It computes pitch/yaw deltas from
  the target and genuine actor world euler, checks the yaw allowance (+0x8A8
  plus DAT_0032B160), and preserves the ordered assembly comparison's acceptance
  of an unordered allowance. Its four blend states retain acquisition/release
  countdowns, conversion on interrupted blends and exact clamping comparisons.
  Assembly supplies the return weight in XMM0, absent from Ghidra's C output.
  Unknown states return the reused first-argument stack word: the body address
  without target calculation, or the wrapped yaw delta after that calculation.
- 0x00073820 clears bit 0x40 in body-bit byte +4, then visits all six motion
  records only when +0x310 is zero. Null records and bone index zero are skipped.
  It calls actual joint transformation 0x0003AEB0 and target selection, sets
  the tracking bit, and updates record +0x20/+0x10 only when +0x2C/+0x1C are
  not -1, including unordered values. Record +0xC receives the returned blend.
- Complete 0x000744A0 forwards the original argument through action +0x744's
  vtable +0x10 and attachment +0x314's vtable +0x10, rereading the attachment
  after the first callback. It then calls actual rotation easing 0x000265D0
  and joint target application 0x00073820. Child virtuals remain explicit
  native method views until their concrete class implementations are installed.
- 0x000743E0 allows action selection only with no positive actor pause count,
  no nonzero first goal kind, zero body +0x30/+0x34/+0x258/+0x6A and none of
  DAT_002FFE80's eight blocker bits `{48, 44, 30, 49, 46, 42, 52, 54}`. Empty
  goal rings preserve the literal address-zero kind read; NaN timers fail zero.

The new `tools/oracle/body_joint_oracle.py` compares 0x00073C30 and
0x00101280 with their retail machine code and actual math callees, without
hooking those dependencies. The host harness links the built game's actual
engine objects and core library. Across 800 joint and 800 angle cases it
reports zero mismatches; return/row floats allow small rounding differences
and equivalent NaNs, while tracking results and all other body bytes compare
exactly. Cases include all six slots, all four states plus invalid states,
disabled targets, reversed limits/rates, pauses, NaNs and infinities.

This oracle exposed two issues in the existing direction helper 0x00105E20.
`camera/entity_camera.cpp` now calls actual `retail_square_root` and
`retail_atan2`, preserves the square-add order, and uses the assembly's ordered
JA gates for Y and horizontal magnitudes. A quiet NaN can produce pitch zero;
with a live horizontal magnitude it still reaches atan2. Retail infinite
length behavior also differs from `std::sqrt`. The fresh read-only dump is
`cache/ghidra_frontier/00105E20.txt`.

Host fixtures in `tests/player_parent_checks.cpp` cover target acquisition,
release, interrupted blends, disable-byte mappings, invalid-state stack reads,
NaN allowance, genuine model-pose joint transformation, pitch/yaw sentinels,
zero bones, the global override, callback changes to child slots, each action
blocker bit, empty goals and the direction-helper corrections.

Validation: Release game and transition builds, all six CTests,
`--dialogue-resources`, `--actor-loading`, `--ghoulybox-decoder`, and the
1,600-case body-joint oracle pass. Tests followed the implementation batch;
subsequent rebuilds fixed the dependency behavior exposed by the oracle.

The relay is now complete. Next body blockers are message 0x00074080,
collision/support 0x00074670, apply 0x00025AE0 (tail entry 0x00074490), the
inherited channel step 0x00072AC0 and actual child methods. The apply dump
identifies 0x0001EEC0, 0x0001EC20, 0x00025190, 0x00025C50, 0x0001FB40,
0x00027960, 0x0001DD00 and 0x00025130. Follow their genuine implementations
and missing callees rather than the legacy `Entity` no-ops. Type 5
registration and live scene/body/mind/strategy ownership remain incomplete.
The opening was not rerun and no live gate progress is claimed. No retail
edits, Ghidra symbol changes, staging or commits were made.

### Body frame-channel reconstruction in progress (2026-10-10)

The live-spawning continuation has added `entities/body_channels.h/.cpp` with
native per-body translations of 0x00030B40, 0x0001EA90, 0x0001D850,
0x0001EDE0, 0x0001EC20, 0x0001EEC0 and the complete channel-step wrapper
0x00072AC0. The native views preserve the actual animation-resource indirections,
model auxiliary targets, body rows/banks and key-table reads. They deliberately
retain the retail missing-bank-row fault and terminal-key loop behavior, rather
than using the additional bounds/terminal guards in legacy camera helpers.
0x00025190 now walks actual actor model anchors and clears body +0x308;
0x00025130 applies body +0x2D1/+0x2D2 to both actual model instances through
complete 0x000F96F0. Child virtual +0x14 remains an explicit method view pending
concrete child class ownership.

`player_body_motion.h/.cpp` also translates movement-message helper
0x00075120: exact body position offsets, ordered freeze/pause gates, the 50-unit
rate limit, processor FSQRT comparison and actual 0x001534C0 normalization.
Fresh read-only dumps for these routines and the four channel-key tables are
cached from the existing Ghidra project. This checkpoint is implementation in
progress; no new builds/tests have run, and no live spawning success is claimed.
The remaining apply dependencies include 0x00025C50, pulse 0x0001FB40,
timers 0x00027960 and wash 0x0001DD00 plus their genuine callees. Body message,
collision, mind/strategy methods and scene/factory ownership remain outstanding.

The same implementation batch now includes `body_timers.h/.cpp`, masked latch
release 0x0001F2F0 and `body_wash.h/.cpp`. Pulse 0x0001FB40 calls complete
latch expiry 0x0001F600; timers 0x00027960 send the actual `{5, 0}` messages,
release the six latch counters and preserve callback-sensitive ordering. Wash
0x0001DD00 follows the assembly's health/actor gates, four phases, opacity
conversion and true frame timer. Its genuine helpers 0x0001E270, 0x0001E770,
0x0001E300, 0x0001E380, 0x0001E6C0 and 0x00104DF0 are translated; the
oscillator uses actual retail sine/cosine and the existing float-to-integer entry.

`actor_shape_bounds.h/.cpp` translates 0x00042640/0x000426F0 shape selection
and bound construction, 0x0003DB30 model radius, 0x00055B30/0x00055AA0 cylinder
measurement, 0x00103BE0 direction transformation, 0x00066260/0x000662A0 native
shape-pool storage, 0x0012B160 bound-record copying and body bound update
0x00025C50. Existing actual shape transforms, sphere metrics, model height,
shape copying and retail math provide the remaining callees. The default
0x00025AE0 apply sequence and player tail entry 0x00074490 are now translated
in `body_apply.h/.cpp`, including model channels, anchor/bound updates, frame
latch countdown, pulse/timers, gated +0x70 decay, +0x34 decay, wash/model state
and the action child's genuine +0x14 method view.

These additions remain unvalidated at this checkpoint; implementation has been
batched before builds/tests. Concrete child methods, body message/collision,
mind/strategy methods, factory registration and live ownership still need work.

Validation of the body frame batch: Release `gbtg` and transition-test builds
pass. The new `tests/body_frame_checks.cpp` checks bound-channel capture, channel
zero merge/NaN behavior, both default blend writes, banks/key mode transition,
child callback count changes, full apply ordering with chained timer callbacks,
decay gates, model-state propagation and actual sphere/pool geometry.
`--dialogue-resources`, `--actor-loading`, `--ghoulybox-decoder` and all six
CTests pass. The new `tools/oracle/body_frame_oracle.py` executes 400 cases each
for movement, blends, pulse, timers and wash against original machine code with
actual math/latch callees: 2,000 cases, zero mismatches. The previous 1,600-case
joint/angle oracle also remains clean. Unchanged body bytes compare exactly;
changed float fields use the existing small rounding tolerance/equivalent NaNs.
Recipients are null in this differential oracle; the fixture tests actual
message delivery and callback ordering.

The oracle corrected two wash gates hidden by the decompile: phase zero's
+A7 flag skips the low-health/timer reason, and phase two checks recovered health
only when +B4 is exactly zero (the parity test distinguishes unordered values).
No live opening rerun or spawning success is claimed. Continuing with body
message 0x00074080 and collision 0x00074670 before concrete class installation.

Collision/message continuation: `body_message_helpers.h/.cpp` now translates
0x0001EFD0/0x0001F030 bit writes, 0x0001F750 latch gating,
0x0001D4C0/0x00024BA0 push reception, action notification/reset
0x00024B20/0x00020920, position event 0x0013AD60, action requests
0x00020980/0x000209E0 and collision action reply 0x00075960. The
0x000209E0 early branch preserves the incoming reply, as shown by assembly.

`body_shape_push.h/.cpp` translates cylinder merging/shifting and actual probe
walks 0x001202C0/0x00120D10/0x00120480, fallback point push 0x001203E0,
sphere conversion 0x00056720 and player selected-shape push 0x000756C0.
The fallback height is actor +0x34 times body +0x884; radius uses +0x38
and +0x888. Selected shapes retain actual shader-mask and kind gates.
The older movement probe 0x001226C0 and its pool preparation/growth now
follow the dump without invented missing-callback, allocation or bounds guards.
Pool growth uses the genuine heap reallocation boundary. Sphere contact
collection 0x001227E0 and record aggregation 0x00122310 are translated,
with a native view for the actual segment vtable +0x24 callback.

This second group has fixture coverage added but has not yet been built or run.
Live spawning remains incomplete: full body collision/message methods and
concrete scene-owned body/mind/strategy installation still require reconstruction.

Second-group validation: both Release targets build; dialogue-resource and
actor-loading fixtures pass, and all six CTests pass. The expanded body frame
oracle compares 2,800 cases with zero mismatches, including actual triangle
0x0010ACD0 and sphere 0x0010ABD0 separation routines. Player floor sweep
0x00075350 is translated with genuine segment sweep/copy and navigation calls;
its +0x874 surface, flag and vertical anchor writes follow the assembly.

Action-selector reconstruction is continuing in `body_action_ids.h/.cpp`:
0x00145FF0 classification and 0x001460C0 attachment range gating provide
actual callees for direct action IDs, paired IDs and table-gated IDs. This new
selector group is not yet validated or installed in the live factory.

Action-selector validation: direct IDs 0x00026B00/0x00026BB0/0x00027220/
0x00027320/0x000273D0/0x00027480, range ID 0x000272D0 and paired IDs
0x00027060/0x00027140 pass 3,600 retail machine-code comparisons using
`tools/oracle/body_action_oracle.py 1 400`. Cases include signed negative
class counts, matching/missing tags, invalid classes/states, actual host range
flags and preserved pair words. Both Release targets build and dialogue-resource
fixtures pass after these additions. Table-gated selectors, parameter selectors
and near selector 0x00026C60 are translated but not covered by that differential
fixture yet. No live factory installation or live spawning success is claimed.

Base-body message dependencies added in the next implementation batch:
0x0001AEB0 receiver clears, 0x00024340 damage-source clears, 0x00027B20 timer
recipient clears, 0x000275E0 action-child destruction, 0x000277E0 current-action
reselection, 0x0001B570 secondary detach and 0x0001B100 actor-reference cleanup.
Actor position/Euler propagation 0x0002B430/0x0002B560 uses genuine transforms
and the actual +0x95C body owner. Physical shape flags 0x00042500 now walk the
real model type-4 records instead of the legacy cached intro-flags value.
Contact removal 0x0013EDD0/0x0013F720 preserves immediate compaction versus
walk-time deferred removal and duplicate suppression; 0x0001F850/0x0001F890
use the genuine override insertion. Cylinder push 0x00120760 and vector scaling
0x00120250 are translated with actual retail math. This batch is awaiting build
and oracle results; full message dispatch and live installation remain incomplete.

The contact-removal/cylinder batch builds and dialogue-resource fixtures pass.
The frame oracle now checks 3,200 cases with zero mismatches, including actual
cylinder push 0x00120760 and its 0x00120250 scale callee, ordered/NaN gates,
untouched failure outputs and DAT_005111AC contact-point writes.

Animation-command reconstruction has started in `body_animation_commands.h/.cpp`.
Actual contact activation/deactivation 0x0013F420/0x0013F4D0 now retains the
intrusive sentinel/head semantics through native previous-link aliases. Commands
0x0001AAF0/0x0001AB50/0x0001ABC0/0x0001ABF0/0x0001A7E0/0x0001AE50 and
helpers 0x0001EF70/0x0001F3F0/0x00019A90 are translated. The direction-push
helper converts the command's signed integer speed; zero selects integer 40
before conversion, as confirmed by CVTSI2SS/FILD assembly. This command group
is awaiting validation. Remaining command effects and damage handling are still
untranslated and continue to block complete live body message installation.

2026-10-11 continuation: attachment contact wrappers 0x00144E90/0x00144EE0
and command 0x0001A290 now use the genuine contact-list functions and actor
message 0x92. Commands 0x0001A720/0x0001A810/0x0001A880 dispatch actual
attachment records with the owning actor context and zero final callback value;
the two named callouts use the real 85-entry name table supplied by the owner.
Detach command 0x0001A690 calls genuine primary/secondary detachment followed
by 0x00112CC0 bind activation. Facing push 0x00019A50 retains its flag gate,
signed speed and retail degree conversion. Cleanup 0x000250E0 clears the
command latch and removes the recorded attachment tokens with the actual
signed count loop. Expanded contact fixtures cover intrusive activation/removal
and sphere-contact collection/aggregation. This batch is awaiting validation.

Read-only damage exports identify 0x00021220/0x00021450 as dispatchers into
additional damage processors. Those processors, the remaining animation effects,
and full body/mind/strategy message/update methods still block live spawning.
No partial vtable is installed and live spawning is not complete.

The animation/contact batch builds both Release targets and passes the
expanded dialogue-resource fixtures. The frame oracle now passes 4,400 retail
machine-code comparisons with zero mismatches: the additional 1,200 cover
0x0001AE50 random timer selection (including NaNs), 0x0001AB50 counter wrapping
and 0x00019A90 signed direction push. These checks do not establish live spawning.

Further translations now include 0x0013B8B0 attachment-token capture and
0x0001AE00 command attachment begin/clear, using actual attachment request
callees and a native view of callback output at body+0x2D8. The existing
0x0003BC40 wrapper now preserves its genuine token return. Relative target
angle 0x00041A30 and target push 0x000199F0 use the actual world-position,
direction-angle and remainder callees; target stored position is supplied by
its native owner. Damage notices 0x00024A00/0x00024A80/0x00024AD0 use actual
pulse broadcast and script queuing. Damage eligibility 0x00028D50 and
attribute-based damage-kind adjustment 0x000211D0 are translated. These latest
angle/damage additions await validation; damage processors remain incomplete.

Damage support now also translates actual table interpolation 0x001339C0,
class lookup 0x00133BD0, actor lookup 0x00028DA0, player/live-player predicates
0x00028D00/0x00028D20 and amount selection 0x000238E0. Native actor state
retains the actual +0xCF4 class separately from the damage command's +0x64
class. Health subtraction 0x00021120 and damage-health application 0x00022310
preserve signed wrapping, player gates and the specific half-damage key cases.
Pulse count/pause table selectors 0x00023CB0/0x00023E20/0x00023F70 and pulse
reduction 0x0001FF60 are translated. Contact rejection 0x00022870 walks the
actual 0x54-byte records, retaining point coordinates when a record lacks a
point, and selects the nearest record's flag. Source state 0x00024370 preserves
its word counter increment/reset. This damage group awaits build and retail
comparison; the full hit processors and live factory installation remain open.

Damage table validation passes 2,400 retail machine-code comparisons with zero
mismatches using `tools/oracle/body_damage_oracle.py 1 400`. These cover genuine
interpolation, class/actor lookup and pulse/pause selectors, including equal byte
endpoints, descending ranges, NaNs, infinities and CVTTSS2SI indefinite results.

Additional motion/action dependencies are translated: 0x000239C0 push amount,
0x00023A50 four damage parameters, 0x000244E0 direction sector, 0x00041900
actor-to-actor angle, 0x00100FE0 angular step, 0x000246D0 facing update,
0x00022400 damage push and 0x00024830 motion state. Animation lookup/start
0x0001AED0/0x0001AFA0 use the actual animation records and mix callees.
Spatial notification 0x0012AFE0 creates the actual actor/global intrusive links
and copies both 0x20-byte shapes; 0x0001D780 creates the notification cylinder
and 0x000257C0 captures the pose. Its scratch view preserves the unwritten
shape header byte instead of inventing a value. This latest group is awaiting
validation; full dispatch and live spawning remain incomplete.

Further damage dependencies: 0x00105C10 keeps the genuine integer candidate
count and closest-angle index; 0x00020B50/0x00020C10 derive directional/joint
animation names. Lookup 0x00020C80 preserves the retail variant fallback order,
including its repeated final lookup, and 0x00024970 selects the damage action
and optional animation through actual callees. Death-effect probability
0x00024620 preserves random advancement only when attributes enable the effect.
Death notice 0x00024910 broadcasts/queues once and sets its gate after callbacks.
These latest translations await validation. Actual death-effect creation
0x00134C10/0x00134D20 remains untranslated, so 0x00024890 and processors that
call it remain blocked rather than being replaced by a host callback.

The frame oracle now passes 5,200 retail comparisons with zero mismatches,
including damage pulse reduction and health subtraction. Sound command helpers
0x00107780/0x000259C0/0x0001A020/0x0001A0A0 and commands
0x00019B40/0x00019F80/0x00019FD0 now use the actual cue lookup and sound queue.
Teleport 0x0001B5D0/0x0001A750 preserves secondary detach/message order and the
shared-attachment notice. Proximity broadcast 0x00122D80/0x0001ADB0 saves the
next intrusive actor before virtual callbacks and preserves strict radius checks.
Spatial notifications reuse the existing genuine 0x0012AFE0 implementation;
the duplicate allocation translation was removed. This combined batch awaits
validation. Live spawning remains incomplete and no partial vtable is installed.

Primary attachment command 0x0001A6D0 now clones the genuine resource, creates
through the real factory, releases the clone before attachment, and destroys
failed attachments. Damage processors 0x00021760/0x00021A90/0x00022010 now
translate their exact flag/action-table gates and state/message ordering using
actual push, action lookup/selection and health callees. The first combined
build found a missing math declaration include in the sound module; that include
is corrected. The processor/attachment additions await validation. Death-effect
creation and the remaining hit processors still block full dispatch/live spawning.

The combined sound/attachment/spatial/damage-animation batch builds both Release
targets and passes dialogue-resource checks (1,357 template clones and 457 keys).
Further translations include turning hit 0x00021B70, amount hit 0x00021C50,
death-action lookup 0x00024680, main reaction hit 0x000215C0 and interrupting
hit 0x000219C0. These retain actual health/pulse/push/facing/animation callees;
0x000215C0 checks for its action row after applying health, as in the dump.
Death-effect parameter setup 0x00068770 uses actual actor effect initialization
and world pose. These newest additions await validation. Full death-effect
creation, damage dispatch and concrete body/mind/strategy ownership remain open.

After the usage-limit interruption, read-only Ghidra exports resumed successfully.
Reference-name allocation/lookup/retain/release 0x00066430/0x000666F0/
0x00066530/0x000664A0/0x00035760 are now translated, including exact string
matching, reference-word wrapping, string/row growth and relocation of names.
Ghouly name collection 0x000F3C80 walks actual 0x288-byte definitions; template
type lookup 0x0013E820 uses genuine executable-resource lookup. Death-class
notices 0x00134EF0 preserve the repeated used-row count and actual script events.
Actor creation at a supplied/default pose 0x00112C50 uses the real clone/factory
callees. This group awaits validation. Navigation candidate selection
0x0010F7F0/0x00111420 remains in progress before actual drop/effect creation.

Navigation candidate collection 0x0010F7F0 and random selection 0x00111420
are translated with actual rectangle bounds, occupancy/path checks, candidate
pool growth and three random advances on successful selection. Drop-name take
0x000286B0 and death-drop creation 0x00135080 preserve name clearing and the
three creation/message branches. Effect creation 0x00134C10/0x00134D20 and
request 0x00024890 preserve live-player scene requests, lifetime gates and
attachment release ordering. Death-motion selection 0x00024760 and knockdown
processors 0x00022520/0x000218A0/0x000218E0 now use these actual callees.
Native parameter scratch retains unwritten values when the retail parameter
selector returns false. These additions await a combined validation batch;
full dispatch and live factory installation remain incomplete.

The navigation candidate oracle passes 400 retail machine-code cases with zero
mismatches. It bridges only heap allocation/reallocation and compares candidate
indices/order and written row words, including filters, growth, descending or
empty distance ranges and NaNs. New native fixtures cover reference-name
identity through both growth paths and strict/fallback navigation selection.
Surviving/timer/lethal hit paths 0x00021920/0x00021D10/0x00021EA0,
classification 0x000226C0, bit lookup 0x0001EF10 and hit timer creation
0x00027B60 are translated. This latest group awaits validation; the two damage
dispatchers and remaining animation effects are still in progress.

Damage entry 0x00020DC0 and dispatchers 0x00021220/0x00021450 now use the
translated classification, processors, timers and script notices. Direct reaction
0x000220F0 retains health/pulse changes before animation lookup. Secondary
attachment 0x0001B3E0, host notice 0x0001B7C0 and creation command 0x0001A2D0
are translated with real class lookup, bind, animation and factory callees.
The message-0x99 name output uses a typed native view of that actual virtual
message; unwritten name scratch is retained. A combined build/fixture pass is
next. Full animation dispatch and live factory ownership remain incomplete.

The combined damage/secondary-attachment batch builds both Release targets and
passes dialogue-resource checks, including the name-growth and navigation
fixtures. Breath command 0x0001AC50 now uses actual model binding, pose, effect
stream and animation duration 0x0003B1D0. Model-binding position 0x00040A60
is translated. Surface-effect name selection/formatting/creation
0x0001A130/0x0013DEF0/0x0001A220 and chance command 0x00019C60 preserve
actual navigation material lookup, provider fallback and effect slot consumption.
These newest effect additions await validation. Live spawning remains open.

Projectile command 0x0001A3C0 now uses actual binding/pose matrices, actor creation
0x00112C50 and direction addition 0x00106220, preserving host assignment and
virtual message order. Retail degree/radian constants were read from the image.
Animation dispatcher 0x0001A8F0 now calls all translated command handlers.
Created objects are retained by the native owner; no placeholder virtual table
is installed. This effects/dispatch group awaits build and differential checks.

Parent body message 0x000253B0 now translates child-forwarding order and every
active message branch using real animation dispatch, contact, latch, pose, action
and cylinder-push callees. Native message views carry relocated pointer arguments.
Secondary-attachment fixtures now check class rejection, secondary ownership,
notification order and real bind cleanup. These additions await the next combined
build; concrete factory/vtable installation is still blocked by player handlers.

Contact direction 0x000FCA60 now retains the retail four-record unroll, fixed
0x54-byte scan, header-stride average and actual reciprocal-square-root callee.
A new machine-code oracle covers zero/short/unrolled pools and nonfinite values.
Parent message native receiver/timer views follow retail pointer clears; nullable
contact views are dereferenced only after the original state/link gates.
The latest build found a missing fixture include, now corrected. This group
continues toward validation and the remaining player hit-message handlers.

The effects/parent-message batch builds both Release targets and passes focused
dialogue-resource checks, including secondary ownership fixtures. Contact
direction oracle 0x000FCA60 passes 400 retail cases with zero mismatches.
Scaled hit-position events 0x0013ADC0/0x0001F0B0, direct-hit message 0x000205B0,
health notice 0x00023740 and lethal action request 0x00134E50 are translated.
These hit-message additions await validation; other contact damage packets and
concrete player body/mind/strategy ownership remain outstanding.

Damage contact position 0x00021010 and reply/event paths
0x00020F80/0x0001F1D0/0x0001F240 now use actual first-contact geometry and
attachment dispatch. Health-contact 0x000233A0 and knockdown-contact 0x00023580
messages build the genuine stack damage packets and preserve handled replies.
The contact oracle is extended to the actual 0x00021010 code before the next
validation batch. Remaining player collision and hit-packet branches still block
full concrete body installation and live spawning.

The hit-message batch builds both Release targets and all six CTests pass.
The expanded contact direction/position oracle passes 800 retail cases with
zero mismatches. Projectile-contact 0x00022CF0, moving-contact 0x00022E40,
impact notice 0x0001F150 and impact packet 0x000231C0 are now translated.
This latest packet group awaits validation. Player collision, the remaining
source contact packets and full native ownership still prevent live spawning.

Resumed after usage reset. Source contacts 0x00020020/0x000240C0 and timer
contacts 0x000229F0/0x00022A40 now construct the original packets and call the
translated damage/reply paths, including timer reflection 0x00020A60.
Angle helpers 0x00041970/0x001060A0/0x0001D540 preserve the retail constants
and direction selection. Collision hit 0x000202E0 was checked against assembly:
the compared dot is the original signed dot, and the return/output is damage
result word 1, despite the decompiler's float signature. This batch awaits
validation. Concrete player collision and body/mind/strategy ownership remain
outstanding; live spawning is not complete.

Death completion 0x00134AE0 now calls the translated class notices, world
position, queued 0xB5 script event, attachment release, clone/factory/free,
death-drop and bind activation paths. Its controlled-player restart gate and
early return on failed effect cloning follow the dump. This joins the pending
source-contact batch for one combined build.

The source-contact/death batch builds both Release targets and passes all six
CTests plus focused dialogue-resource/player checks. Read-only Ghidra exports
resolved the missing sweep chain. Quadratic selection 0x00102DA0, point/edge
sweeps 0x00102C30/0x00102970, sphere sweeps 0x00159CC0/0x0015AB50 and triangle
intersection/sweep 0x00159E20/0x00102E90 are translated with image-verified
constants. Multi-contact separation 0x000FC3F0, player contact correction
0x000744F0 and wide-list insertion 0x0003E8C0 now call concrete dependencies.
This geometry batch awaits retail differential validation before player wiring.

The geometry/player-message batch builds both Release targets. Six sweep entries
pass 2,400 retail machine-code cases without callee bridges. The combined contact
oracle initially found one sphere-separation precision mismatch; assembly keeps
the Z subtraction in x87 across an FST float spill. Preserving that intermediate
fixes the mismatch, and all 1,200 direction/position/separation cases now pass.
The complete player body message dispatcher 0x00074080 calls concrete handlers.
World contact correction 0x000755A0 and main collision update 0x00074670 are
translated; this last update awaits validation. Reviewing its actual caller also
corrected 0x00075960's native payload: +0x18 is the upper-position pointer and
+0x1C is the contact pointer. No partial player vtable has been installed.

Main collision update 0x00074670 builds and matches 400 empty-world retail cases
through actual dependencies, checking the full body bytes, actor position/stamp
and flags. No game callee is bridged. This verifies only the empty-world path;
scene contact branches still need an oracle with real probe recipients.
All six CTests and focused dialogue/player-resource checks pass after the
collision batch. Inherited body methods and concrete factory ownership remain
the next live-spawning work.

Resumed after the next usage reset. Parent mind messages 0x0002A2E0 and
contact/actor-reference/target-end handlers 0x0002A3E0/0x0002A460/0x0002A5A0
are translated against their cached assembly. They use real event allocation,
refresh and release, and actual actor virtual notices; the explicit type-4
unlink/free path remains separate as in retail. This batch awaits validation.
Mind/strategy virtual methods and concrete factory ownership still block live
spawning; no partial vtable is installed.

Parent mind tick 0x0002A9F0 now calls translated request progression
0x0002AA60, event expiry 0x0002AB50 and alert propagation 0x0002A650;
alert thresholds 0x0002A740 retain the assembly's unordered comparison behavior.
Request initialization/update/release remain actual virtual slots. Parent body
relay 0x00025EC0 is translated with both actual child virtual calls. These
additions join the pending validation batch while player-specific mind helpers
are exported from the read-only Ghidra project.

Player mind feedback now includes 0x0007DAC0, 0x0007DAE0, 0x0007DB20,
0x0007DBE0, 0x0007DC40, 0x0007DC80, 0x0007DCC0, 0x0007DCE0,
0x0007DEE0, 0x0007E420, 0x0007E470, 0x0007E5C0 and 0x0007F100.
Actual input selection 0x00109620 is translated; feedback uses the concrete
attachment-event and camera-shake implementations. Constants were exported
from the executable image, including 900 distance-squared and the 0.9/1.5
timers. These changes are pending the combined validation batch. The complete
mind request selector, player dispatcher and concrete ownership remain open.

The parent-mind/feedback batch builds both Release targets and passes all six
CTests plus focused dialogue/player-resource checks. Alert threshold, distance
and nearest-position helpers match 1,200 retail cases exactly without game
callee bridges. An initial oracle setup error put actor position at +0x20;
correcting it to the actual +8 world-position field resolves the discrepancies.
Attachment request searches 0x0001C560/0x0001C5A0/0x0001CB20/0x0001C7C0,
classification 0x0001B170/0x0001B150/0x0001C800, special selection 0x0001C930
and request acceptance/selection 0x0001D2E0/0x0001D210 are now translated.
This attachment selection addition awaits validation; live spawning remains open.

Parent mind request selector 0x000295E0 now dispatches the concrete goal,
callout, body-bit, scale, attachment selection, named action, animation completion,
spawn query and idle-check paths. Assembly corrects request 0x21/0x22 flag names
to +0x50 and preserves mutable 0x3B actor replies. Dependencies 0x0003BFA0,
0x0003B290, 0x001416D0, 0x00129C20, 0x0001EF40/0x0001EFF0,
0x00026930, 0x00028810, 0x00024C20, 0x000191C0 and 0x00041B90 are
translated. This selector/attachment batch is pending validation. Player-specific
mind and strategy methods plus concrete factory ownership remain outstanding.

The selector/attachment batch builds both Release targets. Five retail lookup
entries have a new differential oracle prepared for the next harness build.
Mind navigation initializer 0x00136700 now uses the complete native navigation
search state and real 0x00036AF0; cleanup 0x00036B60 frees the same scratch field.
Mind alert selection 0x00137340, reference clearing 0x00140A40, special-event
classification 0x00123E50 and active query 0x00028CA0 are translated. The retail
low-health threshold is 0.25. This latest navigation/alert addition is building;
live spawning is still incomplete.

Navigation/alert additions build both Release targets. All five attachment lookup
entries match 2,000 retail machine-code cases with zero mismatches. Controlled
player registration 0x0007F870 and input binding 0x0002CAB0 are translated,
including the actual input release/create/push calls, linked-list player index
and alternate port-device path. This input binding awaits the next build.
Health-display dependencies of the player mind tick are now being reconstructed.

Health texture acquisition 0x0012FE10, decimal digit updates 0x0012F990,
health-display state 0x00130710 and mind health update 0x0007FC30 now use
the actual registry/error paths, widths, thresholds and preset timer bits.
Player mind tick 0x0007FB10 is translated with the real input resets, parent
request/event progression, alert propagation and health display. This batch
awaits build/validation. Player mind message/request handlers and strategy
methods remain outstanding before concrete factory ownership can be installed.

The input/health/player-tick batch built both Release targets successfully.
Boost request 0x00137A80 and turn request 0x00137C30 now call the real
attachment release, action selection, strategy resume and pulse broadcast paths.
Their player feedback wrappers 0x0007F8C0/0x0007F920 are translated. Story-bit
test/set 0x000F2A20, direction-to-point 0x000419D0, actor/body rotation
0x0002B560, target occupancy check 0x00141760 and saved-path clearance
0x0013A2A0 are translated. Attachment callout 0x0007DD30 uses the verified
0.5 probability constant. This new batch awaits validation; player dispatcher,
request/control chain, strategy methods and factory ownership remain open.
Player control leaves 0x0007D280 and 0x0007D300 now advance the hold timer
and release attachments through the concrete input edge, availability and body
action implementations. Strategy decay query 0x0002B7F0 is translated.
The remaining control dependencies have been exported read-only for the next
reconstruction pass. These additions join the same pending validation batch.
Secondary axes 0x001088D0 and both axis writers 0x00108FC0/0x001090A0
are translated after exporting and checking their jump tables. Camera-relative
axis rotation 0x00140AE0 and player input transformation 0x0007EE40 now use
the concrete math, read and write paths. Composite-device handling preserves
the retail four-child loop, disconnected gates and final-child output behavior.
The new build initially found an undefined signed-byte alias; the equivalent
sign-bit expressions fix it. This expanded batch awaits validation.
Input tracking 0x00109810 and complete boost timer progression 0x00137840
are now translated. The expanded batch builds both Release targets, passes
all six CTests and the focused retail dialogue/player-resource checks.
The complete input tracking method matches 2,000 retail machine-code cases
with zero mismatches, including special float values and all tracking states;
its square-root callee runs as retail code, without a game bridge. Live spawning
remains incomplete and no partial player factory has been installed.
Goal completion 0x00135700 now clears the active goal, releases actual body
latches, selects the real primary action and queues the original 0xB7 script
event with actor tag and result payload. Target controlled-player query
0x001414C0 is translated. Input gesture state 0x0007D5A0 and direction update
0x0007D460 are translated; a differential oracle for both is prepared.
This goal/gesture batch is building. Full strategy and player mind control
composition remain open before factory installation.

The goal/gesture batch builds. The six CTests and the three preflights pass,
and the prepared oracles report zero mismatches: input gesture/direction
0x0007D5A0/0x0007D460 over 3,000 cases and input tracking 0x00109810 over
2,000 (the body-joint harness was relinked against the current game objects).

### Remaining class-slot frontier and strategy goal states (2026-10-11)

A read-only callee tree from every slot of the four player classes gives the remaining work before
real factory installation. The slots come from getters 0x00027F60 (player), 0x00075AF0/0x000733F0/
0x0001DA60 (body), 0x0007E100/0x00029D30 (mind) and 0x000BED40/0x0002B6C0 (strategy). Unanalysed
slots were exported with temporary functions through `ExportPlayerScene.java range-function:`
(0x00027F40, 0x00075B60, 0x0007EBC0, 0x000BEF40), and their calls were followed by disassembly. The
trees are saved as `cache/ghidra_frontier/callee_tree_player_slots.txt` and `_2.txt`.
About 180 functions remain:

- Player mind control from 0x0007D130 (about 60), including attachment selection.
- Mind navigation and pathfinding from 0x00136620/0x00136B00/0x00136EB0 (about 30).
- Strategy parent message slot 0x0002BC10 and its goal helpers.
- Player slot methods: sizes 0x00027F40, body message 0x00075B60 and 0x00075B90, mind 0x0007F1E0/
  0x0007D360/0x0007EBC0.

The player strategy overrides 0x000BEE10/0x000BEF40/0x000BF2B0 are already translated in
`player_strategy.cpp`; they dispatch player messages, update the brew/idle states and enter player
strategy states. They were still listed as outstanding in the earlier slot frontier.

This batch completes the strategy parent update slot 0x0002C130 and its goal-state chain
(`player_strategy_goals.h/.cpp`), 22 routines:

- 0x0002C130 dispatches on the strategy state, using the byte-indexed jump table read from the image.
  State 1000 resets the input device. States 0x3E9/0x3F4 and 0x3F2/0x418 move to strategy +0x18 or
  +0x10 once the mind has no requests. 0x3F3 loads "celebrate", and 0x3FB runs the goal state.
  0x3FF drops a target whose occupancy differs, then reaches the free position either by searching
  (0x0013A0F0) or through the saved path (0x0013A2A0); on arrival it records the position and
  resumes (+0x18) or stops (1000). 0x415 counts down strategy +0x34 and moves to +0x12 at zero.
- 0x00138A30: with a goal volume (+0x98C) it alerts the class-6 latch listener with message 0x63
  (0x0013D400). It then finds a target inside the volume (0x00139E90) and, once the +0x994 delay
  expires (0x000285B0), finishes the goal for a controlled player, clears the volume (0x00028580),
  enters the next goal action (0x001391D0) and plays the actor's effect names and sound
  (0x00028150/0x0013E6B0). Otherwise a ready goal (0x00135420) completes a step (0x00139010), and
  goal kinds 6 and 0x23/0x25 load "drone_waitforscare" or "celebrate".
- 0x00139010 keeps the last goal pending (+0x984) when decay is off. Otherwise it removes the goal,
  activates the bind tree after a kind-0x15 goal, begins the next goal (0x00138F60), and finally
  leaves to 0x415 or +0x12 (decay on) or to 1000.
- 0x00138F60 finds the scripted occupancy record for +0x1FC (0x00124800, through the DAT_0051108C
  records) once, stores it with latch handling (0x00028500), and either selects the mode action
  (0x00135540) or enters the goal action.
- 0x001391D0 selects the goal-kind action (0x00135790). On success it moves to 0x3FB and loads
  "ghouly_intro" for kind 9; otherwise it drains the goals.
- Action selection by mode (0x00135540) and goal kind (0x00135790) uses keys read per case from the
  assembly, body virtual slots +0x24/+0x38/+0x3C/+0x50/+0x54/+0x58, the carry action 0x00135AB0
  (0xBE/0xBF/0xC0 by attachment state) and, for kind 0x12, controller release (0x0003AC90) and
  pose refreshes.

Assembly corrections to the decompile: goal kind 0x1A in 0x00135420 tests body bit 0xB, not 0x30;
0x0002B540 writes the world position to actor +0xCAC; and 0x0013A0F0 passes flags 4 and the
+0xCAC point to 0x00111420, then tests the path from the actor to the selected cell. The inline
strategy transitions re-read the old state after the leaving call, as retail does. `LatchNode` gains
the owner tag (owner +6) that 0x0013D400 matches.

Verification: `tools/oracle/strategy_goals_oracle.py` runs the retail routines with every outside
callee and virtual slot hooked to log its arguments and return scripted values. The host harness
links `player_strategy_goals.cpp` alone, with stubs that log the same way, and compares the call
logs plus the touched actor, strategy, mind, request and event words. Nine entry points (0x0002C130,
0x00135790, 0x00135540, 0x00124800, 0x00028150, 0x00138A30, 0x00139010, 0x00138F60, 0x00135AB0) pass
2,700 cases with zero mismatches, and every hooked callee is reached. Mutants for an action key,
the 0x1A bit, the request kind, the ring count, a goal name, the kind-7 marker and the 0x63
height scale were all caught. The full Windows build, six CTests and three preflights pass.

### Strategy parent message slot (2026-10-11)

Ten routines complete the strategy parent message slot 0x0002BC10 (vtable 0x004DD74C +0xC). This
finishes the strategy parent class; the player strategy overrides (0x000BEE10/0x000BEF40/
0x000BF2B0) are also translated in `player_strategy.cpp`.

- 0x0002BC10 dispatches through its byte-indexed jump table (ids 1..0x8F). The state arguments for
  0x0002B8D0, which the decompile hides in DI, were read from the assembly. Message by message:
  - 1: target alert (0x00138C80).
  - 0x24: a mode-1 player with no target clears its mind requests.
  - 0x26: decay off does nothing; a pending mask without +0x984 waits. Otherwise a mask drains the
    goals and moves to 0x415 or +0x12; with no mask, a mode-1 player with a target moves to 0x3E9
    and the rest to +0x18.
  - 0x27: with no mask and outside 0x3F3, moves to 1000.
  - 0x32: completes a goal step.
  - 0x4B: moves to strategy +0xC.
  - 0x4D: queues a goal (0x001387F0).
  - 0x5D: moves to 0x3FF.
  - 0x64: moves to +0x16 or +0x14 by message +4.
  - 0x65: moves to the state in message +4.
  - 0x6E: detaches strategy +0x20 when message +0x10 is set and message +4 is the actor, then falls
    through to 0x70.
  - 0x70: contact completion (0x001392D0).
  - 0x72: item delivery within 30 units (0x00138B40, 900 at DAT_0032B4EC).
  - 0x74 and 0x8F: group handoffs (0x00138E40/0x00138EF0), matching actor +0x1FC against the other
    actor's +0x1FE.
  - 0x7D: with decay off, sends 0x27 to the actor.

  Messages 0x5D/0x64/0x65 and 0x24 share an inline gate. It requires no strategy +0x1C hold, a free
  and enabled decay group (mode 2 or +0x93C selects group 1), and no pending goal kind.
- 0x00138C80 clears strategy +0x34. While the actor is alerted (+0x978 > 0 or +0x969), the target
  query sets flag 0x10, clears 4 and passes class words +0x97C/+0x97E (only values above 1). It
  retries with the plain flags, then finishes the goal and either enters the next action with
  effects or completes a step.
- 0x00028420 appends a goal row. The first append allocates a 12-row, 0x20-stride ring and takes
  latches only then. An empty ring restarts at row 0; otherwise the tail wraps with no full check.
  The row stores the kind, the two vectors and the +0x20 word, and clears +0x984.
- 0x00028C30 returns the mind target. 0x00042070 returns an actor's +0x8F8 target record when its
  +0x18 byte is clear.

Native views: messages are their retail words. Actor words resolve through `message_actor` to a
`RuntimeStrategyMessageActor` (bytes, native actor, +0x8F8 target, body +0x274 holder). Body +0x268
and strategy +0x20 are supplied natively. Host goal rings are one `engine_heap_allocate` block (header
plus 0x180 row bytes), matching how 0x00028240 frees them.

Verification: `strategy_goals_oracle.py` gains a message entry point, two other actors and their
targets, holders, the carried object, the ring header and tail row in the snapshot, and hooks for
0x00028360, 0x00028210, 0x00112A20, 0x00028D00, 0x000283D0, the heap and the actor message slot.
Every handler reaches its completion path. Ten entry points pass 3,000 mixed cases and 3,000
message cases with zero mismatches. Mutants were caught for the 0x64 state choice, the alert flag,
the class threshold, ring restart, the delivery radius, the stored contact actor, the mode-1 gate
and the decay group. The class-threshold mutant at first survived, because the target stub did not log the
class words; the stubs on both sides now log them. The full Windows build, six CTests and three
preflights pass.

### Mind path search result predicate (2026-10-11)

Navigation pathfinding helper 0x00036C60 is translated as
`navigation_path_search_has_result`. It returns 1 only when the existing 0x34-byte
`NavigationPathSearch` has state 3 at +0x28 and a nonzero result flag at +0x2C; otherwise it returns 0.
Its caller 0x00136EB0 uses this gate before consuming the result. The same frontier now translates
0x00036BC0 (`clear_navigation_path_search`) and 0x00036C80
(`begin_navigation_path_search`): the former clears the node rows and search-list fields, frees
scratch storage and clears state; the latter sets the endpoint indexes and enters state 1. The
cell-registration counter helpers 0x00110400/0x00110470 are also translated and are used by the
existing registration update/release paths. They apply the retail wrapping 5-bit inner or 5-bit
outer counter update to every registered cell. The larger navigation and pathfinding chain remains
open.

### Navigation route continuation (2026-10-11)

After reviewing the strategy/goal and search-state changes from the other agents,
work continues on the remaining mind navigation chain. Route append 0x000376B0,
endpoint updates 0x000376F0, route validation 0x00037720/0x00036E90 and route
extraction 0x00037470/0x00036E50 now use native pool pointers and the existing
real heap/growth methods. Extraction follows the signed node predecessor fields,
preserves cell heights and replaces only endpoint X/Z coordinates. Neighbor mask
0x00036560, adjacency 0x00036590 and open-neighbor query 0x00036600 are translated.
This batch awaits verification; the search update and mind navigation composition
remain open. Live spawning is still incomplete.

Route differential verification passed: `navigation_route_oracle.py 1 400`
reported **400 cases, zero mismatches**, comparing all route words for extraction,
heap growth, point append, endpoint replacement, and route validation. Only the
retail heap allocator/reallocator calls are bridged. The Release game build
passed; the transition test executable is still compiling in the same batch.

Translated `0x00037340` as `navigation_search_cells_connected`, preserving the
retail neighbor order, connection/bounds gates, discarded distance calculation,
and grid reload after each eligible neighbor. This new leaf awaits the next
verification batch. The A* update (`0x00036CA0` / `0x00036EB0`) remains the next
navigation dependency; live spawning remains incomplete.

The Release game and transition-test builds both completed successfully for the
route batch. Added the actual grid `+0xC` distance multiplier to `NavGrid` and
translated `0x00036A60` as `navigation_cell_distance_cost`; callers must supply
the retail multiplier when constructing a grid. No live resource owner currently
populates navigation grids. Discarded neighbor square roots now call the existing
translated `0x00152B30` implementation rather than the host standard library.
These later navigation leaves have not yet been rebuilt or differentially checked.

The extended route oracle (`navigation_route_oracle.py 2 1000`) passed **1,000
cases, zero mismatches**, now including `0x00037340` adjacency and `0x00036A60`
distance-cost float bits. The subsequent Release game rebuild passed.

Translated route insertion `0x00066310` and retargeting `0x00037980`, including
start/end connection scans, trimming, first-point insertion, final point repair,
and the ten-edit cutoff. Added differential coverage for these operations and
search endpoint/edit-counter state; this latest extension awaits verification.
The existing `truncate_runtime_float` in `audio/listener_settings.cpp` already
translates `0x001673D8`, so the A* path must reuse it rather than create another
conversion approximation.

Latest verification: the Release game rebuild passed, and the route oracle
(`navigation_route_oracle.py 3 2000`) reported **2,000 cases, zero mismatches**,
including route retarget results, endpoint indexes, edit counts, and every route
word. This is heap-only differential coverage of the translated navigation
routines, not live-spawn acceptance. No new CTest run was needed for this batch.
Next implementation work: preserve the search open-list linkage in native node
storage, then translate `0x00036EB0` and its outer `0x00036CA0` using the existing
retail square-root and float-to-qword conversion. The search node identity word
at `+0xA` must be preserved exactly, including the existing clear routine's
behavior; do not silently repair it when beginning a search.

### A* search continuation (2026-10-11)

Corrected `0x00036BC0`: the retail instructions clear `+0/+4`, `+8`, and
`+0xC..+0x13` but preserve the node identity at `+0xA`. The previous blanket
`memset` was incorrect. This supersedes the previous warning about preserving
that implementation's zeroed identity behavior.

Translated `0x00036EB0` and `0x00036CA0` into `navigation/path_search.cpp`, with
retail neighbor order, best-score selection, budget accumulation, region and
occupancy gates, reopened nodes, target flag clearing/restoration, and the actual
`0x00152B30` / `0x001673D8` math callees. Four-byte node link words use relocated
references (null=0, search sentinel=1, node=index+2) so the native 64-bit build
preserves the original 0x14-byte node rows without pointer truncation or extra
allocation. Added a differential oracle that executes the entire retail search
without callee bridges and compares search state, all node words (normalized
links), and restored grid bytes. This batch awaits build and oracle verification.

A* verification: Release game build passed, and
`navigation_search_oracle.py 1 1000` reported **1,000 cases, zero mismatches**.
This executes `0x00036C80`, `0x00036CA0`, and all A* callees in retail code without
bridges. It compares budget accumulation, terminal states, all five words of each
node (with normalized links), and all grid bytes after target restoration.

Translated route curve parameter `0x00037F50` and steering `0x00037D50`, preserving
control-point selection/padding, distance-to-parameter mapping, float weight
ordering, and the sampled-output write behavior. Added steering float-bit
comparisons to the existing route oracle; this extension awaits verification.

Route steering verification (`navigation_route_oracle.py 4 2000`) passed **2,000
cases, zero mismatches**. Added `0x000377B0` route length, retaining the four-edge
unroll and per-edge SSE addition order; the expanded oracle including length
(`navigation_route_oracle.py 5 2000`) also passed **2,000 cases, zero mismatches**.
Translated `0x00111660` nearest open cell, preserving the subtractive radial
neighbor scan and its all-eight-outside termination. This leaf awaits dedicated
verification.

Translated floor connection selection `0x00110D00`, exposing the actual
DAT_0050E248 connection lists, and player navigation request `0x00136EB0` using
real registration, A*, route, actor-clock and vector-scaling functions. Preserved
the floor conversion's observed output-Z-from-height instruction. These latest
callers await build and differential verification; their globals are not yet
populated by a live scene owner. Live spawning remains incomplete.

Latest checks: Release game and transition-test builds passed; all six CTests
passed. `navigation_search_oracle.py 2 3000` passed **3,000 cases, zero mismatches**.
The first floor-query oracle run found 1,848 mismatches, all in successful
floor-conversion output Z. The earlier instruction reading was incorrect:
`POP EDI` shifts ESP before the final load, so it loads the selected Z rather
than the selected height. Corrected the translation to copy the selected XYZ.
This supersedes the preceding output-Z-from-height notes. Nearest-open-cell
results matched throughout that run; corrected floor conversion awaits rerun.

Corrected floor-query verification: `navigation_floor_oracle.py 1 2000` now
reports **2,000 cases, zero mismatches** for nearest-cell selection and floor
conversion. Release game and transition-test rebuilds passed after the correction;
dialogue-resource preflight passed (1,357 template clones and 457 retail keys).
`0x00136EB0` builds with translated callees, but its full combined player-context
behavior has not yet received a retail differential check. Do not count it as
live-spawn acceptance.

Next navigation frontier: `0x00136B00` needs attachment registration release
`0x00137100`; its input-axis write `0x00108FC0` is already translated in
`frontend/dialogue_input.cpp`. Fallback `0x001373F0` needs `0x00137460`, whose
real dependencies are `0x00137500` and `0x00137610`, plus `0x00137790` proximity.
Those then unlock `0x00136620`. The player control/class-slot and live scene
navigation/resource owners remain outstanding. No new stubs or live factory
bindings have been installed, and no files have been staged or committed.

### Player navigation control continuation (2026-10-11)

Translated `0x00137100` attachment registration suspension, `0x00137500` motion
alignment, `0x00137610` input alignment, `0x00137460` motion fallback,
`0x00137790` proximity, `0x001373F0` fallback, `0x00136B00` navigation drive, and
`0x00136620` request dispatch. The native contexts expose the actual attachment,
body actor, bit block, and input device rather than substituting callbacks.
The drive instruction stream reloads speed (argument six) for its distance
interpolation; this differs from the decompiler's parameter reconstruction.
The request wrapper passes message +0x24 plus 2.5 as radius, +0x28 as approach,
and +0x2C as speed, as established by its pushes and stack overwrite.
This complete implementation batch awaits build and retail differential checks.

The batch builds. `navigation_motion_oracle.py 1 3000` passed **3,000 cases,
zero mismatches**, including NaN/infinity cases and full retail input/math callees.
Added a combined `0x00136EB0` / `0x00136B00` oracle with real A*, routing, steering,
and input writes; only retail heap allocation/reallocation is bridged. Its first
1,000-case run found 46 drive-axis magnitude mismatches. Corrected the stack
interpretation: `0x00136C8F` loads approach before the temporary square-root
argument is removed; `0x00136CB7` then loads speed. The interpolation therefore
uses approach as the denominator and speed for the clamp. This supersedes the
preceding speed-interpolation note. Corrected combined verification is pending.

Corrected combined navigation verification (`navigation_drive_oracle.py 1 3000`)
passed **3,000 cases, zero mismatches** for actual player path and drive calls,
including blocked cells, absent searches, body gates, search budgets, steering,
and input-axis writes. Only heap allocation/reallocation is bridged. Full
attachment and cross-floor contexts remain outside this oracle's current cases.
Added the exact player storage-size slot `0x00027F40` (0xCF8, 0x63C). Continuing
with player-mind request preparation `0x0007EBC0` / `0x0007EDF0`; requested a
read-only jump-table instruction export from the existing Ghidra project.

The Ghidra export succeeded read-only after its first automatic approval review
timed out. Translated `0x0007EDF0` range randomization and player-mind class slot
`0x0007EBC0` request preparation. Jump-table instructions establish different
input ranges for kinds 0x10 (+0x1C), 0x11 (+0x10), and 0x2E (+0x1C), which the
decompiler does not retain. Kept the actual RNG updates, mind-field reset, and
reference-position angle callee. These additions await the next verification
batch; no class owner has been installed yet.

Latest verification: player-mind request oracle (`player_request_oracle.py 1 3000`)
passed **3,000 cases, zero mismatches**, with complete message/mind/RNG comparisons
and no bridged callees. Release game and transition-test builds, all six CTests,
and dialogue-resource preflight passed for the navigation/request batch.

Exported `0x00075C60` and `0x00075DD0` read-only for the player-body message slot.
Remaining callees include `0x0012B1A0`, `0x0013B7E0`, `0x0013B810`, and
`0x0003EF40`; bit updates, attachment event reset, and model-anchor registration
already have translations. Added player-body action slot `0x00075B90` using the
existing script table lookup and parent action ID selection, with an actual
vtable +0x24 callback for the goal-kind-7 branch. This latest slot awaits build
and differential coverage. Live spawning remains incomplete: the message/control
class slots and live ownership/resource wiring still need completion.

The Release game rebuild also passed with `0x00075B90` included. Its differential
coverage remains pending; do not treat that build as behavioral acceptance.

### 2026-10-11: player body control dispatch and remaining mind leaves

Translated player-body vtable message dispatch `0x00075B60` and its actual
`0x86`/`0x87` handlers `0x00075C60`/`0x00075DD0`. These preserve actor identity
checks, health save/restore and wrapping additions, visibility reference counts,
resource callout IDs `0x38`/`0x39`, flag 7, and animation rate changes. Added their
missing wrappers `0x0013B7E0`/`0x0013B810` (player attachment event tracking),
`0x0012B1A0` (live attachment suspension), and `0x0003EF40` (model-anchor release).
All call real translated dependencies; no missing game call was replaced by a
constant or callback. Context views remain to be installed in the live owner.

Release game build passed. `player_body_control_oracle.py 1 3000` matched all
3,000 cases using retail `0x00075B60` and real callees without bridges. Coverage
includes scalar state, identity rejection, empty resource/anchor/attachment
lists, and suspension counts; populated resource and model-anchor cases remain
outside this fixture. The native harness initially lacked flag storage and was
corrected before the successful comparison. This is not live spawn acceptance.

Added the next mind leaves `0x0007E3A0` (six retail idle-action keys selected with
the actual negative random scale, request lookup and acceptance) and `0x0003B1F0`
(animation position crossing, including backward playback, wrap and unordered
comparison behavior). These latest additions await build/differential checks.
`0x0007D130` still needs its remaining input, attachment and target-control
callees; `0x0007D360` and `0x0007F1E0` remain incomplete. Live spawning still
requires complete mind slots followed by native factory/scene ownership wiring.

The next batch also translated `0x0007E160` (input/inactivity timing and idle
request triggering), attachment eligibility `0x00027DA0`/`0x00027E50`/`0x00027D40`,
and pending attachment control `0x0007D210`. Eligibility calls the real actor
position, model-radius and spawn-point routines; pending control uses the actual
body vtable +0x5C result as the action-selection key. No incomplete control
callee was represented by a fake implementation. Native owner contexts must
supply the actual pending actor, actor-link view, and vtable binding.

The Release game and transition-test builds passed after moving a dependency
include out of the attachment helper header to avoid an include cycle. All six
CTests passed. Additional retail differential results: animation crossing
`animation_crossing_oracle.py 1 3000`, idle request selection
`player_idle_oracle.py 1 3000`, and inactivity
`player_inactivity_oracle.py 1 3000`: 3,000 cases each, zero mismatches. The
inactivity comparison first found 39 differences at the input threshold;
DAT_0032B22C is `0x3C23D70B`, not the usual rounded literal `0.01f`. Corrected
that exact retail bit pattern before the passing run. Inactivity coverage uses
keyboard axes with absent animation controller/action table; populated action
selection remains outside that fixture. Attachment eligibility and pending
control currently have build verification only.

Current next frontier: `0x0007D890` needs `0x0007E4B0` and `0x0007DF50` plus their
real call trees; `0x00137140` needs the gaze/anchor selection chain and body look
helpers; `0x0007F980` needs the path-probe chain. These block `0x0007D130`, then
`0x0007D360`; player-mind message slot `0x0007F1E0` and live factory/scene ownership
wiring still remain. The latest live opening evidence is unchanged: camera only,
script opcode `0x6F` blocked. Do not count these build/oracle results as a live
Cooper spawn.

### 2026-10-11: complete gaze control chain

Translated `0x00137140` and its remaining gaze callees: `0x000251D0` gaze origin,
`0x00025250` body look angle, `0x00105D90` input-axis angle, `0x00100E40` relative
angle, `0x0011C130` anchor navigation-path query, `0x0011BED0` nearest anchor,
`0x0011BFF0` random owner anchor, and `0x0002A130` mind anchor/duration setup.
These call the existing real joint lookup/transform, actor ancestry, navigation
path, and math translations. The caller arguments `1,30,100` mean check-path,
maximum vertical difference, and search radius respectively. Native contexts
retain actual anchor identities and target actor pointers; live ownership wiring
is still outstanding. This batch awaits its first build and retail comparison.

The nine-function gaze batch built successfully and `player_gaze_oracle.py 1
3000` matched all 3,000 cases with no callee bridges. It exercises populated
anchor lists, ancestry filtering, random owner selection, cached and explicit
gaze, and secondary-axis steering, including unordered values. Models and
navigation grids are absent in this fixture; marker/pose and populated path
coverage remain outstanding.

Added target range helpers `0x00134240`/`0x00134350`, target acceptance
`0x0007E960`, embedded shape-target refresh `0x0007EA20`, and redirection
`0x0007E9E0`. Shape refresh copies the actual live attachment's two shape records
and follows the retail float/extended-precision steering order, state and flag
gates. Target acceptance preserves the unconditional second range query and
the kind-3 mind-event timer gate, including the original address-zero read when
no event exists. These latest additions await build and differential checks.

The five target range/shape functions now pass the Release build and
`player_target_oracle.py 1 3000`: 3,000 cases, zero mismatches, with no callee
bridges. The fixture supplies live contact shapes and exercises refresh,
redirection and acceptance; attached model-radius coverage remains outstanding.

Continued target selection with `0x00128F70` (nearest eligible actor inside an
angular window) and `0x0001D3D0` (held-model radius plus body margin). The angular
query calls the actual eligibility, direction-angle, path and math functions;
its fallback radius uses the retail bits `0x47C35000`. These two additions await
build and retail comparison. Live spawning remains incomplete.

Added `0x00041490` actor distance and `0x0007E600` near-actor selection. The latter
preserves the instruction-level two-empty-results behavior: it writes the
retained cone-distance stack word rather than silently skipping the output or
inventing a default. That word is explicit native caller storage; ownership
wiring must supply it. These additions also await build/differential checks.

Translated `0x0007E6B0` focus maintenance using those real searches, body reach
and actor-distance functions. It preserves locked-target restoration, the
60/70-unit gap thresholds, the 200-unit fallback query and gaze-timer reset.
Native context maps retain actual target bodies and original actor identities;
the factory/scene bindings remain outstanding. Awaiting build and comparison.

The five-function actor-focus batch passed the Release game and transition-test
builds. `player_focus_oracle.py 1 3000` matched all 3,000 cases with no callee
bridges: angular queries, empty near searches including retained-stack output,
and focus retention/fallback/timer behavior. Navigation grids and held models
are absent in this fixture.

Continued the held-item shape-target selection branch: `0x00129B20` scoring,
`0x00129770` angular mind-target search, `0x00137DE0` search windows,
`0x00137D00` category priority, `0x0007D770` search mode, `0x0007D7C0` retained
held target, and `0x0007D6B0` policy selection. They call the actual target
eligibility/reachability, range and math functions; window constants were read
from retail bytes. Native contexts expose the retained search-angle stack word
and mutable angular-limit global. This seven-function batch awaits build and
differential checks; live ownership and the remaining mind handlers still block
Cooper spawning.

The first shape-search comparison found three retained-angle differences after
failed held-target checks. Earlier retail callees spill the original actor
address into the same incoming search-local stack slot. The fixture now
observes that incoming word (without bridging any callee) and supplies it to
the native retained-storage context. Live owner integration must reproduce
that storage binding; this comparison does not validate native stack ownership.

The seven-function held-item selection batch passed both Release builds and
`player_search_oracle.py 1 3000`: zero mismatches after supplying the observed
incoming retained-stack word. Coverage uses one populated target and all real
callees; held models/navigation grids and native stack ownership remain outside
this fixture. All six CTests passed after this batch.

Continued with target proposal `0x0007E880` and enclosing target control
`0x0007E4B0` in `player_mind_target_control`. These call the real policy search,
shape refresh/redirection, mind-event allocation/removal, focus/acceptance and
feedback functions. Contexts map original target tokens, searchable and
embedded shape records, and their actual actor owners. These latest two
translations await build and retail comparison. The remaining input-direction
branch `0x0007DF50`, navigation avoidance `0x0007F980`, mind-message handling
and live factory ownership still block the milestone.

The enclosing `0x0007E880`/`0x0007E4B0` proposal/control module now passes both
Release builds; it has build verification only. Continued the action-request
frontier with `0x000414F0` squared actor distance, `0x0001CC10` target quadrant,
`0x00026830` turn direction, `0x0001D180` body readiness, `0x00134650` body target
state, `0x001413C0` target action requirement and `0x0001D130` special requirement.
All use real math/position/bit callees and retail constants. This latest batch
awaits build and differential checks. `0x0007DDA0` and request selection still
need their remaining translated callees before `0x0007DF50` can be completed.

Also translated `0x0001CD00` target-height/shape preparation and
`0x0007DDA0` action direction using the newly completed real callees. Direction
preserves current-target repetition rules and the 625 squared-distance limit
for a different focus actor. These await the same pending batch validation.

The nine-function action-query batch now passes both Release builds and
`player_action_query_oracle.py 1 3000`: 3,000 cases, zero mismatches with all
real callees. It covers readiness/state/requirement gates, direction and turn
classification, height (without shape output), focus-based action direction and
squared distance. Held attachments and actor binding trees are absent in this
fixture. All six CTests passed; the dialogue-resource check also passed with
1,357 retail template clones and 457 localization keys. `git diff --check`
passed. No new live opening acceptance run was performed.

Current reconstruction frontier: complete action-request row filtering and
selection (`0x0001BA20`, `0x0001BBB0`, `0x0001CDA0`, `0x0001CF70`,
`0x0001CFE0`, `0x0001D060`, `0x0001D0E0` and their remaining real callees),
then `0x00136770` and `0x0007DF50`. `0x0007DDA0` is now translated and
differentially checked. Target proposal/control `0x0007E880`/`0x0007E4B0`
currently have build verification only. Navigation avoidance `0x0007F980`,
player-mind message handling and complete factory/scene owner installation
remain. Latest live evidence still shows camera only and opcode `0x6F` blocked;
these results are not a live Cooper spawn.

### 2026-10-11: action-request selection continuation

Resumed from commit `bb16479`, which contains the preceding target-control and
action-query work. Added real row filtering `0x0001BA20`, descending chain-row
lookup `0x0001BE20`, model target flag `0x00024600`, point-shape eligibility
`0x001415D0` and target surface distance `0x0013A5B0` in `body_request_selection`.
They preserve signed row heights, unordered range comparisons, target/body
repeat gates, and the original target flags. These additions await a batched
build and retail comparison. Random eligible-row selection additionally needs
`0x00066180`, whose dump is not currently cached; no replacement was invented.

Recovered `0x00066180` from the read-only Ghidra project and translated it with
random eligible-bit selection `0x000661C0`, calling the existing real
`0x001673D8` conversion. The bit count includes every stored byte, including
padding; if an ordinal lies outside the logical bit count, the original output
word remains unchanged and the function still returns success. Continued with
second, pair, category and special row selectors `0x0001BBB0`, `0x0001C9A0`,
`0x0001C5E0` and `0x0001C860`. The second selector's instruction-level distance
test is surface distance > attachment radius * 3, correcting the misleading
decompile. These six additions join the pending implementation batch.

Added the four body request-selection wrappers `0x0001CF70`, `0x0001CFE0`,
`0x0001D060` and `0x0001D0E0`. They use actual direction/height/shape preparation
and the completed row selectors, preserving the table-pointer gates and byte
direction/height masks. The chain-bearing `0x0001CDA0` still needs the sequence
callees. No build or test has been run for this continuation batch yet.

Completed first-table target lookup `0x0001C140`, address-to-chain lookup
`0x0001C510` (missing instructions recovered read-only), chain length
`0x000243C0`, sequence resume `0x0001C0E0`, sequence candidate selection
`0x0001BE60`, chain request selection `0x0001CDA0`, the five mind request
handlers `0x00136830`/`0x001368B0`/`0x00136920`/`0x00136990`/`0x00136A00`,
and dispatcher `0x00136770`. Sequence candidate removal uses the actual
last-record copy, preserving candidate ordering; it does not shift the pool.
Retail data addresses remain explicit and resolve to native storage views.
These additions await the combined build and retail comparison; live scene
installation and live Cooper spawning remain incomplete.

The combined Release game/transition build passed, all six CTests passed, and
retail tutorial resource checks passed (1,357 template clones and 457 keys).
Added a retail differential oracle for `0x00066180`, `0x000661C0` and
`0x000243C0`: 3,000 cases passed, including padding bits, retained output,
NaNs, infinities and signed zero. Populated row/sequence and mind-action paths
still require differential coverage.

Continued with `0x0007DF50` and main action-control handler `0x0007D890`,
using real secondary input, target control, event allocation/removal and mind
request dispatch. Target-to-native-owner bindings must be supplied by live
scene ownership; these functions do not install themselves into the factory.
They await the next combined build. Navigation avoidance `0x0007F980`,
player message handling, remaining mind update/control handlers and complete
factory/scene installation remain. No new live opening run has been performed.

Main action control `0x0007D890` and target-action execution `0x0007DF50`
now build. The extended request oracle adds real row filtering `0x0001BA20`;
6,000 cases passed with zero mismatches (2,000 each for row filtering,
bit selection/count and chain length). The earlier 3,000 primitive cases also
passed. Populated sequence selection and full mind dispatch remain unverified.

Added navigation heading probe `0x0013AC80`, alternating heading search
`0x0013AA50`, randomized/wrapped heading `0x00100C70` and player avoidance
`0x0007F980` in navigation/player_avoidance. They call the existing real
navigation spawn selection, retail trigonometry, direction conversion and
input-axis writer. Retained caller-stack occupancy/direction values remain
explicit native context inputs; their live ownership is not yet validated.
These four translations await build and differential checks.

Navigation avoidance and its three helpers build. Completed outer player
control update `0x0007D130` in entities/player_control_update, preserving
input transform, action control, pending attachment, gaze, inactivity, hold,
boost/avoidance and input attachment release order. It requires the actual
boost preset and shared native owner contexts. Build and runtime installation
are still pending for this last addition.

Latest checkpoint: all 34 reconstructed definitions in this continuation
(29 request/action functions, four navigation functions and the outer control
update) build in Release for both game and transition-test targets. All six
CTests pass. Heading wrap, direct heading probe and alternating heading search
passed 3,000 retail differential cases with empty grids. Across the request
and heading oracles, 12,000 cases passed in this continuation. This coverage
is limited: populated sequence selection, populated avoidance, complete mind
action dispatch, retained caller-stack ownership and live owner installation
are not validated. Tutorial resource checks passed earlier in the same batch.

Next work: recover the complete switch instruction range for player message
handler `0x0007F1E0` (the cached listing contains only the switch entry/default),
translate its real callees, and connect the completed control chain through
factory-owned contexts and the player mind vtable. Revisit populated-row and
sequence differential fixtures before live acceptance. No retail file or Ghidra
project was modified, and no commit was created. Live spawning is still
incomplete; the last opening evidence remains camera-only/opcode `0x6F`.

### 2026-10-11: player mind message slot

Resumed from commit `74e8ce8`. The cached `0x0007F1E0` listing stops after the switch entry, so the
full body came from disassembling the retail image. That includes the byte-indexed jump table at
`0x0007F7E0`/`0x0007F768` (ids 0x23..0xB2). The decompile is complete, but it hides register
arguments; these were read from the assembly:

- 0x41 passes message +0x28 as present and +0x14 as source to `0x0007DEE0`; 0x49/0x5A pass
  +0x2C and +0x18.
- 0x37 passes message +4 as the turn point.
- 0x6E passes message +0x14 as the payload and +0x10 as the target to `0x0007DBE0`.
- 0x2D queues the 4-word script event {0xB6, actor, message +8, word +6 of that actor}.
- 0x7D: `0x0003B1D0` leaves the animation duration in XMM0, and `0x00061960` reads that value.
  The five pushed words (10.0, 0, 0, 1.0, 0) belong to `0x00061960`.

New definitions:

- `dispatch_runtime_player_mind_message` (`0x0007F1E0`) in `entities/player_mind_messages`.
  Every case ends in the parent mind handler `0x0002A2E0`, as in retail.
- `runtime_resource_name_is_aid` (`0x000357B0`): tests the 4-byte "aid_" tag.
- `clear_runtime_goal_message` (`0x00049DB0`): the 9-word goal message 0x4D.
- `start_runtime_camera_pulse` (`0x00061960`) in `camera/camera_pulse`. It writes the block at
  `DAT_0050447C`, skipping the write while bit 15 of `DAT_0050EA9C` is set.

Message words that hold retail addresses go through context resolvers:

- the 0x50 camera-shake name;
- the 0x2D actor;
- the mind +0x88 source object.

These handlers translate the message words; they do not create the actors or objects behind
them. Health warnings (0x90) use body +0x94: thresholds 5/10 normally, and 2/5 once the
scene-end flag is set. A self contact (0x6E) adds 5.0 to mind +0x50 and clamps it at 15.0.

`player_mind_messages_oracle.py 1 3000` and `4 3000` passed 6,000 call-sequence cases with
zero mismatches. Every id and branch was exercised: activation, aid shake, clears, the 15.0
clamp, both health tables, the camera pulse and its flag gate, and the script event. Ten
non-equivalent mutants were killed. These covered:

- the health thresholds;
- the alert limit;
- the track word offsets;
- the goal word;
- the bit value;
- the pulse arguments, flag bit and active value;
- the aid tag.

Alert `>` versus `>=` is an equivalent mutant: clamping 15.0 to 15.0 changes nothing.
`cache\_run\build_and_test.bat` passed both Release builds, all six CTests and the
dialogue, actor-loading and decoder checks.

The control chain (`0x0007D130`) and the message slot are now translated. They are not yet
installed in the player mind vtable or connected to factory-owned contexts. The player strategy
batch (`0x000BEE10`/`0x000BEF40`/`0x000BF2B0` and helpers) still lacks its host harness for
`player_strategy_oracle.py`. Live spawning is still incomplete. No retail file or Ghidra project
was modified, and no commit was made.

### 2026-10-11: player strategy batch verified

The player strategy overrides and helpers translated earlier now have retail differential checks.
Checking the call sites against the assembly found one translation error. `0x000BF600` resets the
idle timer when body bit `bytes[6] & 1` is clear (`test; je reset`), or when a threat is found. The
translation had the bit test inverted; it is fixed, and the oracle rejects the old condition with
116 mismatches.

Two other corrections:

- `0x0012D090`: when error reporting is suppressed and a texture lookup fails, retail stores the
  null texture and reads its width at address 8. `RuntimeBrewMeter` now takes an `address_zero`
  view for that read, following the convention used elsewhere.
- `0x000BF4D0`: a record chain that refers back to an earlier key loops forever in retail too.
  The fixture uses forward-only chains.

Verification:

- `player_strategy_oracle.py` (seeds 1, 3 and 5; 9,000 cases) compares the batch call by call.
  It covers message, enter and update entries, every update state, the brew prompt, scripted
  goals, knockout, idle, move, the intro unlock, the brew meter and the real axis writers
  `0x00108FC0`/`0x001090A0`/`0x00109180`. All cases pass. Mutants killed:
  - the idle and stick thresholds;
  - the brew-count step;
  - the follow-up flag;
  - the difficulty gates;
  - the intro flag bit;
  - the health doubling;
  - the knockout flag value;
  - the story bit;
  - the half-width scale;
  - the negative-count clamp.

  The brew count clamp `> 5` versus `>= 5` is an equivalent mutant.
- `script_nearest_oracle.py` (seeds 1 and 3; 6,000 cases) checks the nearest-actor query
  `0x00128DF0` and its path gate `0x001295E0`. The filter `0x00129420` and the Newton-refined
  square root `0x00152B30` run for real on both sides; the harness includes `anim_math.inl` for
  the host refinement. All cases pass, and eight mutants were killed (distance order, ordering,
  radius gate, tag gate, path flag, path class, best update, path direction). Populated filter
  flags beyond `0x4000` are not covered.

`cache\_run\build_and_test.bat` passed again after these changes. Nothing was committed.

### 2026-10-11: player mind vtable installation

The retail getters fill both mind vtables at run time, because the tables live in BSS:

| Slot | Player mind 0x00505498 (getter 0x0007E100, type 0x34) | Parent mind 0x004DD6F4 (getter 0x00029D30, type 7) |
| --- | --- | --- |
| +0 sizes | 0x0007E0E0 | 0x00029D10 |
| +4 construct | 0x0007EF10 | 0x00029F20 |
| +8 destroy | 0x0007F0D0 | 0x00029FF0 |
| +0xC message | 0x0007F1E0 | 0x0002A2E0 |
| +0x10 tick | 0x0007FB10 | 0x0002A9F0 |
| +0x14 request update | 0x0007D360 | 0x000295E0 |
| +0x18 request init | 0x0007EBC0 | 0x00029EC0 |
| +0x1C request release | 0x001628B0 (empty) | 0x001628B0 |

The retail type table at `DAT_0030D41C` confirms the chain: type 0x34 has parent 7, and type 7 has
parent 1.

The last untranslated slot was the request update `0x0007D360`, now in
`entities/player_mind_request_update`:

- request 7 and request 8 select the body action from body vtable +0x38. Request 8 does so only
  when the body flag named at request +0x10 (`0x0001EF70`) is set, and returns 1 either way.
- 0x16 runs the control update `0x0007D130` and returns 0.
- 0x2E returns the navigation request `0x00136620` result.
- 0x31 selects from body vtable +0x3C.
- Everything else goes to the parent update `0x000295E0`.

`mind_request_update_oracle.py 1 3000` passes with zero mismatches, and five mutants were killed.

`NativePlayerMindOwner` (`entities/player_mind_owner`) installs types 7 and 0x34, following the
pattern of `NativeActorFactoryOwner`:

- Its lifecycle adapters call the translated constructors and destructors. The parent destructor
  releases the instance view after `0x00029FF0`.
- It provides dispatchers for player mind slots +0xC (`0x0007F1E0`), +0x10 (`0x0007FB10`), +0x14
  (`0x0007D360`), +0x18 (`0x0007EBC0`) and +0x1C (empty).
- Each mind's contexts (`NativePlayerMindSlots`) come from a binding that the player owner
  supplies when type 7 construction begins. Callers that already take child-slot functions bind
  them to these dispatchers: `RuntimePlayerUpdate::mind_tick`, the player child message,
  `RuntimeMindTick::update_request`/`initialize_request` and
  `RuntimePlayerMindRequests::release_active`.

The new `check_player_mind_owner` (run by `--dialogue-resources`) covers:

- the installed parents, vtable addresses and the 0x198 instance size;
- parent-mind construction through the owner, checking the bytes written by `0x00029F20`;
- view release on destruction;
- rejection of a second owner.

`cache\_run\build_and_test.bat` passes.

This does not yet spawn a player. The player owner still has to build `NativePlayerMindSlots` from
the player's native objects and bind the dispatchers. That needs factory installation for the
player (type 5), body (types 0x27/0x2A) and strategy (types 8/0x4F) classes. Full player-mind
construction through `0x0007EF10` (camera creation and the intro sound) has not been run through
the owner.

### 2026-10-11: player factory owner and the other three player classes

These vtables are also filled at run time by their getters. Retail type table `DAT_0030D41C` gives the
parent chains:

- player 5 → actor 2 → 1;
- body 0x2A → 0x27 → 6 → 1;
- strategy 0x4F → 8 → 1;
- mind 0x34 → 7 → 1.

| Type | Vtable (getter) | Sizes / construct / destroy | Other slots |
| --- | --- | --- | --- |
| 5 player | 0x004DD6B8 (0x00027F60, over actor fill 0x0003C080) | 0x27F40 / 0x28830 / 0x28A30 | +0xC message 0x28F60, +0x10 update 0x2C2B0, +0x14 draw 0x28050 |
| 6 body | 0x004DD658 (0x0001DA50 → 0x0001DA60) | 0x1DA30 / 0x24C50 / 0x24F70 | 24 base body slots |
| 0x27 body | 0x00504FE0 (0x000733F0) | 0x76180 / 0x73450 / 0x734B0 | +0xC 0x74080, +0x10 0x744A0, +0x18 0x74490, +0x1C 0x74670 |
| 0x2A body | 0x00505248 (0x00075AF0) | 0x75AD0 / 0xCFFD0 / empty | +0xC 0x75B60, +0x20 0x75B90, +0x2C 0x75C30 |
| 8 strategy | 0x004DD74C (0x0002B6C0) | 0x2B6A0 / 0x2B740 / 0x2B7A0 | +0xC 0x2BC10, +0x10 0x2C130, +0x14 0x2BAF0, +0x18 0xBFA50 |
| 0x4F strategy | 0x00505FA0 (0x000BED40) | 0xAF030 / 0xCFFD0 / 0xBED90 | +0xC 0xBEE10, +0x10 0xBEF40, +0x14 0xBF2B0, +0x18 0xBF410 |

The 0x2A table is a fresh 0x0001DA60 fill plus its own overrides; it is not built on the 0x27 table.

Four small slots were untranslated and are now added:

- `0x0001DA30`: base body sizes 0x894/0x7C8.
- `0x000CFFD0`: constant-success constructor.
- `0x00027500`: body +0x4C, which returns 0xA9.
- `0x00075C30`: player body +0x2C. It returns 0xD8 when body flag byte 5 bit 0x10 is set; otherwise
  it tail-calls `0x00026A90`.

`NativePlayerFactoryOwner` (`entities/player_factory_owner`) installs types 5, 6, 0x27, 0x2A, 8 and
0x4F, and owns the mind owner for types 7/0x34. It sits on top of the actor owner's types 1 and 2.

**Lifecycle adapters.** These call the translated constructors and destructors. Each child finds its
player through the actor address that the player constructor writes into the child's cloned
parameters: body +0x1B0, mind +0x9C, strategy +8.

**Per-player instance.** `NativePlayerInstance` holds one player's native state and contexts in
dependency order:

- **Player.** The constructor and destructor contexts, `self`, the attachment creation context, the
  child FactoryObjects, input and links.
- **Body.** Native mirrors of fields +0x10/+0x14/+0x48/+0x4C/+0x5C/+0x60/+0x64/+0x15C/+0x268/
  +0x2C8/+0x314/+0x73C/+0x740/+0x744/+0x760, plus actor +0x54/+0x5C/+0x60 and joint records.
  These feed the parent body cleanup and construction, action selection and motion contexts. Actor
  +0x1B0/+0x1B8 alias the next pointers of the actor's attachment sentinel (+0x1AC) and of its
  pending node (+0x1B4), matching the two list walks in `0x0003CFD0`.
- **Mind.** The request anchor +0x54 (its next is +0x58), the event anchor +0x24 (its next is
  +0x28), storage, script table, camera source and setup.
- **Strategy.** The link, setup, entry, goals, base messages, callout and the full
  `RuntimePlayerStrategyFlow`.

**Strategy dispatch during construction and destruction.** Retail calls the strategy's virtual
enter/leave slots here: `0x00028EB0` on creation, and `0x0002B7A0` re-enters state 1000 on
destruction. Because the object's vtable stays 0x00505FA0, these resolve to the player overrides
`0x000BF410` and `0x000BF2B0`.

**Value views.** Body, input, mind, strategy and the goal table are value views that the adapters
refresh as each child appears. The decoded goal table and the model nodes/channels come from scene
bindings.

The new `check_player_factory_owner` (run by `--dialogue-resources`) covers:

- the installed chains and vtable addresses;
- a player built through `create_factory_object`, with real translated constructors for the actor,
  player, body chain (6/0x27/0x2A) and mind chain (7/0x34). It checks the body/mind sizes, address
  binding, body health written by the mind and the camera notice;
- a standalone strategy chain whose destructor enters state 1000 through the player flow;
- a second player whose constructor creates and enters its strategy;
- full release of both players and their children.

`cache\_run\build_and_test.bat` passes.

**Still to do (method phase).** The remaining virtual-slot contexts must be built and attached to
the instance:

- player message/update/draw (`self`'s actor message);
- body slots +0xC..+0x5C (`body_action_key`/`body_slot_key`);
- the player-mind message/tick/request-update contexts (`NativePlayerMindSlots` pointers);
- strategy message/update dispatch.

Until then, those slots raise an explicit "not attached" error rather than running a substitute. The
live scene does not yet create a factory registry or these owners: no live code calls
`0x00134920` (player spawn). The last opening evidence remains camera-only/opcode `0x6F`.

### 2026-10-11: live wiring, stage 1 (scene markers), in progress

**How retail spawns the player.** Scene startup `0x0011CBD0` loads `aid_marker_<scene>` through
`0x0011CB80`/`0x000FFE20` into `DAT_0051108C`. `0x00140020` then works on a copy of those records:

1. it swaps id pairs (`0x001401B0`);
2. it shuffles id groups (`0x001402B0`/`0x00140450`);
3. it dispatches every row through the kind table `DAT_0042A720`, in the 27-kind order at
   `DAT_00312098` (`0x0011EEC0`).

Kind 2 (`0x000FFEC0`) is the player start; it calls `0x00134920`. The live host never loads
`DAT_0051108C` (`g_51108c` is only freed), and none of this ran before.

**What the opening scene contains.** `aid_marker_ghoulies_chapter1_scene1_2playcam` holds kinds:

- 0x13 ×2 and 2;
- 4 ×5, the imps (`aid_objparams_ghoulies_actor_imp`, type 5 with body 0x27, mind 0x35 and strategy
  0x46);
- 0xB ×3 and 0x10 ×5;
- 3 ×2 (rope-barrier props) and 6 (the powerup book, type 0xD);
- 8 ×2 and 0x14.

Kind 4 dispatches before kind 2, so the imps are created before the player.

**Added (builds; no retail comparison yet):**

- `scene/scene_markers`: `0x000FFE20`, `0x0011CB80`, `0x001401B0`, `0x00140450`, `0x001402B0`,
  `0x0011EEC0`, `0x00140020`, `0x0011F1C0`, `0x00127FF0`. Dispatch raises an error for a marker
  kind whose retail handler is not translated, rather than skipping it.
- `scene/scene_marker_handlers`:
  - every handler kind in the opening scene: 2 (`0x000FFEC0`, `0x00100110`, `0x000FFFD0`), 3
    (`0x000FF680`), 4 (`0x000FF4F0`, `0x001349E0`, `0x00069F30`), 6 (`0x001001A0`), 8/0x14
    (`0x00100210`/`0x001002B0` and the occupancy insert `0x001246A0`), 0xB (`0x000FFAD0`,
    `0x0006A010`), 0x10 (`0x000FFDC0`, `0x0006E110`) and 0x13 (`0x000FF880`, the camera start pose
    `DAT_00545AD0`);
  - created objects are retained by the scene.
- `tools/oracle/scene_markers_oracle.py`: retail side written. The host harness
  (`scene_markers_harness.cpp`) is not written and the oracle has not been run.

**Remaining for live spawning in this scene:**

1. Write and run the harness for `scene_markers_oracle.py`.
2. Add the scene runtime owned by `Game`. It needs:
   - the factory registry, the actor/player owners and the address map;
   - an animation resource registry backed by the scene bundle;
   - for every factory slot that has no class installed, a getter that raises "type not
     translated".
3. Call the marker load and start from scene start, before the 0x7D message, and bind opcode 0x6F
   to `wait_runtime_script_player`.
4. Translate the classes these markers create. Untranslated callee-tree sizes:

   | Class | Untranslated functions |
   | --- | --- |
   | Shared cinema/AI mind 0x35 | ~133 |
   | Imp strategy 0x46 | ~60 |
   | Powerup book 0xD | ~160 |
   | Trigger 0x23 | not yet counted |
   | Area 0x1F | not yet counted |

5. Add the player method contexts: message 0x28F60, update 0x2C2B0, draw 0x28050, the body slots,
   the mind methods and the strategy message/update.
6. Drive the per-frame update and drawing of factory actors from `frame_tick`.

Nothing is committed.

### 2026-10-11: continuation after scene marker and factory owner commits

Resumed from `89319d4` with the checkout clean except for the existing PDB.
The intervening agents completed player mind messages/slots, strategy flow,
native lifecycle ownership and opening marker handlers. Added owner adapters
for the player strategy message/update overrides (`0x000BEE10`/`0x000BEF40`),
refreshing shared native value views before dispatch. The constructor fixture
now exercises message dispatch through the owning strategy adapter.

Added `scene_marker_records_harness.cpp` and `scene_marker_records_oracle.py`
for swaps, group shuffling, dispatch ordering, next-id selection, restart and
copied scene startup. This separate oracle bridges heap and actual indirect
handler callbacks, recording the row inputs. It does not replace marker handler
bodies or validate their factory/resource behavior. The larger pre-existing
`scene_markers_oracle.py` still needs its full handler harness. No build/test
has run yet for this continuation.

Marker-record oracle passed 600 initial cases and a further 3,000 cases across
all six entry paths. Added a native handler-table binder for DAT_0042A720,
using the translated opening handlers. Verified the retail table directly:
kind 0x18 shares kind 6 (`0x001001A0`), kind 0x12 calls the actual empty
`0x001628B0`, and genuine null entries remain null. Eleven other non-null
handler slots remain explicitly untranslated and fail if reached. Bindings
must outlive the handler callbacks. This prepares dispatch ownership but does
not install the live Game runtime or translate the missing actor classes.

Validation checkpoint: Release game and transition-test builds pass. The
`--dialogue-resources` suite passes, including the new strategy-owner message
fixture; all six CTests pass. `git diff --check` reports no whitespace errors.
The marker-record checks total 3,600 passing cases. Coverage is record-level
only: full marker handler/factory behavior and live spawning remain unverified.
No live opening run, retail edits, Ghidra edits, staging or commit was performed.
Next: attach remaining body/player/mind instance method contexts and construct
the Game-owned scene factory/resource/address graph. The opening marker order
still requires real imp construction before the player; do not skip those rows.

### 2026-10-11: modified template and named marker continuation

Reviewed the intervening owner/marker work. Reconstructed six more functions
from read-only Ghidra exports: named parameter builder `0x0006D410`, kind
0xE handler `0x000FF830`, companion creation `0x00100770`, template name
and chain-name overrides `0x00146C80`/`0x00146CC0`, and kind 5 handler
`0x001007F0`. The handler preserves scaled template size, optional companion
creation before the main object, percentage conversions and kind-7 chain-name
selection. Installed both handlers in the native retail table; nine non-null
handler slots remain untranslated. Factory classes reached by these handlers
still require real implementations; this does not complete live spawning.

Added a differential parameter harness comparing the full output buffer,
including untouched bytes, for the three parameter builders. Implementation
was batched before any builds or tests. Validation pending at this checkpoint.

Validation: Release game and transition-test targets build successfully. The
new parameter oracle passes 3,000 randomized cases (seed 11) with zero
mismatches, exercising `0x0006D410`, `0x00146C80` and `0x00146CC0` against
retail code. All six CTests pass and `git diff --check` passes. The complete
kind-5/kind-0xE handler factory paths have not received differential coverage;
no live opening run was performed. The Game-owned scene graph, native body/
mind/player method bindings and preceding imp classes remain outstanding.
No retail/project changes, staging or commits were made.

### 2026-10-11: all remaining marker slots translated

Translated the nine remaining handlers (7, A, D, F, 11, 15, 16, 17, 19)
and their four parameter builders, name registration, kind counting and nearest
marker selection. The native handler table now covers every non-null retail
slot. Additional bindings carry the real name pool/count, executable template
name tables, player attachment lists and attachment origin. They also expose
retained stack positions in nearest lookup/dropper creation explicitly, so
missing chains do not receive invented zero positions. The kind-D handler
preserves its two writes back to the source record. Kind-19 uses the translated
navigation floor lookup and live attachment allocator. No factory class was
stubbed. Implementation completed before validation; handler differential
harness in progress. Game scene ownership and created classes remain separate
live-spawning work.

Marker translation milestone complete: all 22 non-null retail table entries
are bound, with the six genuine null entries preserved. Added 16 reconstructed
functions in this batch (nine handlers, four parameter builders, name storage,
marker counting and nearest selection), following the preceding six-function
batch. The binder fixture checks every table slot and dispatches the differential
cases through the binder itself.

Validation checkpoint: 3,000 handler cases passed with empty navigation floors,
then 3,000 cases passed with two populated floor bands after correcting kind-19's
floor argument to the attachment flag byte (tag is zero). Handler coverage is
kinds 5, 6, 7, A, D, E, F, 11, 15, 16, 17 and 19. It checks factory parameter
writes, conditional/order behavior, source-row mutation, name storage, player
presence, retained stack values, attachment metadata/shape copies and origin.
Factory construction/cloning are fixture capture boundaries, not reconstructed
class acceptance. Stack parameter padding that retail does not write is excluded
from the handler comparison; the independent parameter oracle compares full
seeded buffers, including untouched bytes.

The expanded parameter oracle passed 3,500 cases over seven helpers, including
all four new parameter builders and the multi-name fallback branch. Release
game/transition-test builds pass, all six CTests pass, the dialogue-resource
suite passes, and `git diff --check` passes. The larger legacy
`scene_markers_oracle.py` still lacks its original combined harness; the separate
record, parameter and handler oracles document their actual coverage instead.
No live opening acceptance is claimed. Additional native bindings must outlive
the handler table and supply real scene storage and retained inputs. No retail
payload/project changes, staging or commits were made.

### 2026-10-11: scene ownership and factory continuation

Added `SceneFactoryRuntime`, retained by `Game` and replaced on scene load.
It keeps executable template ownership, the actual bundle/demand resource
provider, all 0xAB retail type-parent/getter/name rows, and checked native
address registration/resolution. Every uninstalled class has a distinct
throwing getter. Registry creation does not fabricate constructors or skip
opening imps. Marker blocks have heap-compatible ownership, but marker start
is not connected yet. Actor/player lifecycle owners and their complete method
contexts still need attachment to this scene owner.

Removed the unused FactoryObject reference from NativePlayerInstance: keeping
it while the marker object vector grows could leave a dangling reference.
Actual instance bytes and native owner mappings remain stable.

Reconstructed area type 0x1F sizes/constructor/message and pose selection,
including the candidate-cell history ring, randomized candidate/annulus
selection and active-dialogue query. Its real navigation, actor transform,
direction-angle and base-message callees are reused. The class has an install
adapter but is not installed into the live registry until its actor contexts
are owned. This batch has not been built or tested yet.

Read-only Ghidra exports establish the other missing class getters: imp mind
0x35 at 0x0007C520, imp strategy 0x46 at 0x000B4480, trigger 0x23 at
0x00069620 and book 0xD at 0x00109DD0. No class is claimed complete solely
because its getter or lifecycle addresses are known.

Checkpoint: trigger 0x23 now has translated sizes, constructor, 512-byte
attribute copying, spawn-slot selection and both pose-query helpers. The
constructor retains the retail probability comparison and untouched fields;
pose selection reuses the actual navigation candidate selector and forwards
A6 queries to the anchor actor. Its full message/update/destruction binding
is still pending, so this class is not installed. Read-only exports of its
remaining methods and the imp strategy methods are cached for continuation.

The scene owner now retains factory objects, registers their instance address
ranges, and releases them through 0x001075C0 in reverse ownership order before
resource storage dies. This is native ownership plumbing; it does not replace
the retail scene teardown sequence. No build/tests have run in this batch yet.

Validation checkpoint: Release game and transition-test targets build with the
new ownership/area/trigger modules explicitly included in CMake. A new retail
oracle passed 3,000 seeded full-buffer comparisons of area initialization,
trigger initialization and spawn-slot selection (seed 23). Controller registry
insertion is excluded from this oracle because it requires native pointer
mapping; no retail callees are captured or replaced in these comparisons.

Trigger destruction/update and the complete 0x00069960 message dispatch are now
translated, including A7 spawn actors/goals/effects, A8 deferred ghoulybox loads,
A9 template creation preserving template scale, AA scaled creation, and 7C/7D
reference updates. Added the actual 0x00141840 template-pose helper and an
actor-only view of 0x00028150 so imp effects do not need a fabricated player
strategy context. These last changes still need compilation and method-level
oracle checks. Trigger installation still requires scene-owned native actor
and message contexts; the exported helper methods do not imply live spawning.

Native class ownership checkpoint: added NativeSceneActorClassOwner over the
base actor owner. It installs area 0x1F and trigger 0x23, retains stable native
contexts by instance bytes, routes derived messages/trigger updates, and
releases trigger registration/load state before base actor destruction. Its
controller adapter accepts actual A8 requests from 0x0013E8D0. All services are
explicit required bindings; missing actor/resource/address services are not
substituted. The live SceneFactoryRuntime has not attached this owner yet.

Area and trigger method bodies are translated; remaining acceptance work is
method-level retail comparison plus installation with the real scene services.
Imp mind 0x35, strategy 0x46 and book 0xD remain unfinished, so scene ownership
and factory spawning are not complete. The opening marker order remains intact.

Ownership checks passed against the retail hierarchy: real type-1 construction
and teardown for 200 objects across vector growth, exact ownership counts,
interior address round trips, overlapping-range rejection and stale-address
rejection after release. SceneFactoryRuntime installs the self-contained base
object class; actor-derived registrations still wait for their required services.

Added actor_word_messages as the explicit pointer-width conversion into the
translated base handler for animation events/mixes, vectors and references.
Its animation-options offsets were checked against the retail 0x00030B10 and
0x0003EF70 dumps. Area/trigger ownership uses this adapter with required scene
memory/reference/animation-record mappings. No raw Xbox pointer words are cast
as native ActorReferenceMessage pointers.

Imp mind 0x35 now has translated sizes, constructor/destructor and the actual
0x0007CC00 resource-loading/random-delay chain. This does not install its getter:
message/tick/request/init methods remain unfinished. The constructor oracle is
being extended to the no-resource path; actual resource acquisition is not
claimed verified by that path. All six CTests and retail dialogue-resource
checks passed after the earlier ownership/trigger batch.

Final checkpoint for this batch: Release game/transition-test builds pass.
The extended constructor/slot oracle passed 4,000 full seeded-buffer cases
(seed 29), including the imp mind no-resource constructor path. The scene
ownership harness passed; all six CTests and the retail dialogue-resource suite
pass. git diff --check passes. Area/trigger message/update paths, imp resource
loading and native message conversion still need their own method-level oracle
coverage; this batch's constructor oracle does not certify those paths.

Resume with the real scene model/animation/D3D resource services and ownership
of ActorBaseContext/actor effects, then attach NativeActorFactoryOwner,
NativeSceneActorClassOwner and NativePlayerFactoryOwner. Complete imp mind
0x35 message/tick/request/init, imp strategy 0x46 and book 0xD before dispatching
the opening markers. Their cached getter/lifecycle/helper dumps identify the
remaining chains. The milestone and live spawning remain incomplete; no
opening imps are skipped and no missing callee returns a fabricated success.
No retail payload/Ghidra project edits, staging or commits were performed.

Continued checkpoint: translated imp mind request initialization 0x0007C580.
It retains every kind branch, target-null rejection, range source/destination
and RNG update, including the degree-to-radian turn jitter and the distinct
0x2E/0x2F ranges. Existing angle/random helpers are reused. Indirect switch
blocks were exported separately to verify offsets missing from the first
function instruction listing. Request update 0x0007C0E0, message 0x0007C8B0
and tick 0x0007CE30 are now cached and remain unfinished; initialization alone
does not register type 0x35. A method oracle extension is prepared for target
present/absent branches and all initialization kinds.

Shared-behavior audit (2026-10-11): the retail type table names type 0x35
entityMindCinema, getter 0x0007C520. Across the 1,357 executable templates,
35 distinct mind templates use this type and are referenced by 45 actor
templates, including player AI, imps, skeletons, zombies, haunted objects and
other ghoulies. The earlier "imp mind" label was too narrow. Renamed the new
lifecycle/request modules and native context to cinema_mind/RuntimeCinemaMind;
these remain a single shared implementation with the original Xbox addresses.
The type table has no other getter alias or direct child of type 0x35.
Type 0x46 is entityStrategyImpFire (getter 0x000B4480): its single strategy
template is referenced by imp, impchallenge and improger. Keep that strategy
separate from shared cinema-mind behavior. No actor-name-based behavior is
introduced. Empty Ghidra references in unanalysed code are not used as evidence
of exclusivity. Shared message/tick/request-update methods remain unfinished.

Sharing refactor validation: Release gbtg and the class harness build pass.
The expanded five-mode retail oracle ran 5,000 cases (seed 31): 112 exact-byte
mismatches remain in request-init angle outputs. Constructor/slot cases retain
previous coverage. Added the actual CRT data page used by retail angle math;
no angle return is stubbed. Do not claim request initialization accepted yet:
investigate actor/angle fixture and shared direction helper before registration.

Angle mismatch resolution (2026-10-11): all 112 mismatches were caused by
oracle memory loading, not the shared cinema-mind implementation. The XBE
reader reads one section at a time. Reading 0x00200000 through 0x00210000 in
one call padded the next section with zeros, removing atan coefficients at
0x00203854 and related constants. Load 0x00200000..0x00203000 and
0x00203000..0x00210000 separately. Genuine retail 0x001530E0 angle math now
runs with its actual data. The same 5,000 seeded-buffer cases (seed 31) pass
with zero mismatches, including request initialization 0x0007C580, target-null
branches, all initialization kinds, RNG and exact output bytes. Added useful
float diagnostics for future failures; no tolerances, fabricated returns or
production angle changes were needed. This supersedes the preceding unresolved
angle checkpoint. Message/tick/request-update and live registration remain
unfinished; this result accepts only the exercised methods and input coverage.

Shared cinema-mind continuation: translated message handler 0x0007C8B0 and
its 0x0007C4C0 class-permission helper; added the actual shared base-mind
0x00029C80 eligibility policy, calling existing 0x00129920/0x001299B0.
Boost/turn 0x00137A80/0x00137C30 are confirmed shared by player and cinema
minds, using different embedded state offsets (+0x158 versus +0x94).
Moved their implementation/context into mind_action_requests and kept player
notifications in player_mind_requests. No per-actor duplicate was introduced.
Translated cinema sound timer 0x0007CCD0, calling genuine 0x00041D00, and
shared body-facing helpers 0x00134510/0x001345F0 plus request 0x0007B800.
All remaining switch blocks and full tick end were exported read-only; no
factory registration is claimed until request-update/tick and bindings finish.
Expanded focused oracle coverage for permission rows and sound-timer gates;
batch compilation and checks follow these implementation changes.

Focused verification: 7,000 cases (seed 41) passed with zero mismatches across
constructors, slots, request initialization, class-permission rows and sound
timers. Sound coverage runs the genuine empty-name 0x00041D00 branch, including
expiry RNG/reset and NaN/freeze/body-state gates; it does not certify nonempty
sound-queue delivery. Release game and transition-test builds pass.
Translated full cinema tick 0x0007CE30 through RET 0x0007D121 and avoidance
0x0007CAB0 using actual navigation-heading search, request allocation/cleanup,
input-axis writes, parent mind processing, target policy, gaze and boost timer.
Request-list access uses its native +0x58 pointer view. These full tick bodies
still need method-level retail coverage and live service bindings. Additional
oracle modes now cover 0x00134510/0x0007B800 and shared 0x00029C80 with nonzero
target categories. Request-update 0x0007C0E0 is still incomplete; the movement
chains require 0x00135B60/0x00136030/0x00136D60 and further helpers rather than
stubs. No getter registration or live-spawn acceptance is claimed.

Movement-chain checkpoint: the ten-mode oracle passed 10,000 cases (seed 43)
with zero mismatches. Added shared vertical-axis writer 0x00109260 and vertical
navigation drive 0x00136D60, with the real horizontal drive, nav-cell height and
body-state/input branches. Translated escape-position selector 0x00135B60:
actual kind-0x1B marker enumeration and farthest/behind selection, followed by
real navigation candidate/heading searches. No missing helper return is
invented. Oracle extensions cover direct/aggregate vertical input writes and
escape-marker selection; navigation fallback and full vertical drive still
need their own acceptance coverage. Request-update remains unregistered.

Expanded acceptance checkpoint: 12,000 cases (seed 47), zero mismatches,
including genuine retail escape-marker choice and vertical input writes.
All six CTests pass. Added real angle interpolation 0x00100E90 and escape
heading randomization 0x00136030 for the next movement-request chain; these
new angle methods still await focused verification. Escape request wrapper
0x0007B110 now translates its success/output and failure-return-4 branches.
Continue with cinema movement requests and temporary navigation cost handling,
then request dispatcher, class ownership and live bindings. Live spawning and
the scene ownership/factory milestone remain incomplete.

Movement implementation checkpoint: translated escape drives 0x0007B170 and
0x0007B2F0, position drives 0x0007B5A0/0x00136680/0x0007BC20, random/heading
selection 0x0007B8A0/0x0007B940, vertical/horizontal target chasing
0x0007B620/0x0007BC80, and target action 0x0007B090. Shared helpers now include
target movability 0x00141580, relative point angle 0x00041AE0, horizontal point
offset 0x001060F0, and collect/suspend/restore navigation costs
0x00110110/0x0007B490/0x0007B510. Calls use existing translated navigation,
attachment, input and chain-selection implementations. Horizontal chase keeps
the retail isolated-cell shape selection and conditional forward offset.
This batch awaits compilation and focused retail acceptance; the shared
request dispatcher and live class bindings are still incomplete.

Additional request bodies: 0x0007B840 now calls existing shared spawn eligibility
0x00146150 and attachment eligibility 0x00027D40, then the required actual body
vtable +0x5C callback and action selection 0x00027650. Timed motion 0x0007BA20
uses the real body speed 0x000259C0, navigation drive, input reads/writes and
new shared planar scaling 0x00105DD0. The speed query uses the body actor's
damage class. Caller-owned mode scales and action key are required; no missing
callee result is supplied. Release game and transition-test builds pass, and
all six CTests pass after this batch. A 1,400-case seed-53 retail comparison
passed with zero mismatches across fourteen modes, including angle interpolation
and horizontal offsets. Expanded planar-scaling acceptance is running next.
Remaining immediate work: spawn-point movement 0x0007BF60, exact request
dispatcher 0x0007C0E0, method-level movement/tick/message coverage, and native
cinema owner bindings before strategy/book and full live scene integration.

Spawn-position request 0x0007BF60 and its shared neighbor predicate
0x000368C0 are now translated. The request preserves the target's +0xD00
navigation floor and the owner's current position for the final neighbor
query; native target/address and link views must come from the actual owner.
The expanded 15,000-case seed-59 run found one horizontal-point-offset
mismatch (case 12058, 0x001060F0, x coordinate); focused diagnosis is in
progress. Do not treat the larger run or the new movement bodies as accepted
until that mismatch and remaining method coverage are resolved.

Mismatch resolved: shared sine 0x00153320 recomputed its outer polynomial's
square in a different floating-point order from the actual SSE instructions.
The translation now retains the square once, and the inner x87 branch also
retains its square before multiplying the coefficient. The exact failing
offset case now matches retail. The unchanged 15,000-case seed-59 suite then
passed with zero mismatches across fifteen modes, including planar scaling;
no tolerance change or invented callee was used. Release game/transition
builds and all six CTests pass, and git diff --check passes. Full movement,
tick and message methods still await their own retail coverage.
Exported all request dispatcher switch blocks read-only. Notable bindings for
the next pass: case 8 tests flag name then chance at request +0x40; case 0x19
approaches the actor's +0xCC0 checkpoint at radius 5/approach 30/speed 1;
case 0x25 faces mind +0x70, and case 0x26 uses inline request position +0x10.
The dispatcher remains unregistered, and live spawning remains incomplete.

Shared cinema dispatcher 0x0007C0E0 is now translated against the exported
switch blocks, including flag/chance gates, sound-before-movement ordering,
checkpoint approach, facing thresholds and genuine parent fallback. Review
also corrected 0x0007B840: body vtable +0x5C returns the action key consumed
by 0x00027650; it is not a caller-supplied key. Null target handling remains
inside the retail gates. Added required NativeCinemaMindSlots and explicit
install_cinema to the existing base mind owner, sharing type-7 lifecycle and
request/event/storage ownership with player minds. Native getter view type
0x35 uses vtable 0x00505478 and routes lifecycle/message/tick/request methods
to the translated cinema implementations. This install path is available
but not activated in the live scene: actual per-actor contexts must be built
before constructors, and method-level acceptance is still pending. Resource
views alias the lifecycle resource so acquisition is visible to requests.

Dispatcher/owner build checkpoint: Release game and transition-test targets
pass. The expanded sixteen-mode oracle passed 1,600 cases (seed 61) with
zero mismatches. New coverage calls the actual retail 0x0007C0E0 switch for
0x25/0x26/0x27 and compares all body bytes plus return values, including body
state gates, null targets, NaN thresholds, inline/mind/target positions and
relative heading wrapping. A 16,000-case seed-67 run is underway. This covers
the facing portion of the dispatcher, not all movement, sound or action paths.

Expanded dispatcher acceptance: 16,000 cases (seed 67) passed with zero
mismatches across all sixteen modes. Native base mind slots now own the
cinema slot view via unique_ptr, so it remains stable until base destruction
releases the instance. The native constructor adapter rejects an unbound
cinema context explicitly rather than dereferencing null or returning an
invented success. Final Release builds pass. The existing player owner and
its type-7 lifecycle continue to compile with the new optional owned view.
Next dependency is the actual per-actor context builder and scene service
graph; install_cinema is not called by the live scene yet. Type-0x46 strategy
and book classes also remain required by the opening imp templates. Keep
full dispatcher/tick/message coverage separate from the verified facing,
primitive, constructor and selection modes above.

### 2026-10-11: imp fire strategy (type 0x46)

Translated entityStrategyImpFire, the strategy every opening imp template uses
(getter 0x000B4480, vtable 0x00505EA4), with its untranslated callees. The
first call tree came from the decompiler and missed three tail-called helpers
(0x001395C0, 0x001398A0, 0x001399A0) because Ghidra inlined them into the
0x000B46B0 switch. The final tree was rebuilt from the machine code with a
capstone walk that follows jump tables and tail jumps. Message and switch case
blocks were disassembled from the XBE where the read-only exports stopped at the
indirect jump.

New modules:

- `strategy_target_queue`: the 20-slot attack queue at DAT_0050E284
  (0x0002CC30, 0x00112B40, 0x00112970, 0x00112A70, 0x0002B910). It shares
  `g_actor_attachment_slots` with 0x00112910, so slot owners are the native
  `ActorLiveAttachment` records. `RuntimeMindTarget` gains a trailing `record`
  pointer to name that identity.
- `strategy_target_selection`: shared base-strategy target selection, checks,
  reactions, tutorial-notice queueing and roam requests (0x00029AC0, 0x00029BA0,
  0x00139AA0, 0x00139C70, 0x00141450, 0x00141470, 0x000196B0, 0x000196F0,
  0x00129280, 0x00138410, 0x001409C0, 0x000F4E10, 0x000F4870, 0x0013A430,
  0x0013A4F0, 0x00138700, 0x00138740, 0x00138790, 0x001384F0, 0x001395C0,
  0x001398A0, 0x001399A0, 0x001383A0, 0x00139FA0, 0x00138310, 0x0013A0A0).
- `imp_fire_strategy`: sizes, constructor, destructor, message, update, enter
  and leave slots, and every state handler (0x000B0B90 through 0x000B4F50 and
  0x000C2D90).
- Angle helpers 0x00100B80/0x00100D00 in `mind_navigation_angles`, and the
  model record lookup 0x000409D0 beside 0x00040980.

Behaviour notes taken from the instructions rather than the decompile:

- The decompile hides the predicate flags that 0x00129640 receives in EAX. Each
  caller's adjustment of `mind +0x64` (`&~4|0x10` while hostile, `&~0x20|8` in
  0x00029BA0) is kept.
- 0x000B4160's direction-angle output reuses its caller's argument slot. That
  slot holds the parameter block address (0x004287BC), and a zero direction
  leaves that bit pattern as the angle.
- 0x000B40E0 tests the unrounded x87 remainder, not the rounded float.
- 0x00112B40 still swaps when the distances are unordered (COMISS/JNC).
- Strategy message return values are discarded by the actor dispatcher
  (0x00028F60), so the imp message slot returns void like the player strategy.

`NativePlayerFactoryOwner` installs type 0x46 over base type 8 and records each
player's strategy type. Strategy vtable +0x14/+0x18 now dispatch to the imp
enter/leave methods (0x000B4940/0x000B4A60) for imps, and `imp_fire_message` and
`imp_fire_update` adapt slots +0xC/+0x10. The scene bindings now include the
queue table and limits, the active-actor list, the tutorial-notice queue, the
0x004287BC parameter block, effect creation and messaging, and body and model
views. The imp methods read the strategy through the bound strategy bytes, so a
standalone strategy child works before the actor's +0x954 object is set.

Validation: `imp_strategy_oracle.py` runs the retail code under Unicorn against
the native modules. Seeds 3, 7 and 11 (2,000, 6,000 and 3,000 cases) have zero
mismatches, and ten mutants across the five modules are each detected. The
factory-owner fixture constructs a type-0x46 strategy through types 1, 8 and
0x46, dispatches its message slot and releases it. Release game and
transition-test builds pass. All six CTests and the dialogue-resource,
actor-loading and decoder suites pass.

Still outstanding for live spawning: book type 0xD, the Game-owned scene
service graph that supplies these bindings, and calling marker start and the
per-frame actor updates from the live scene. No retail payload or Ghidra
project was modified, and nothing was staged or committed.

### 2026-10-12: FX anim emitter (type 0xD)

The opening scene's kind-6 template creates type 0xD, `entityAvatarFxAnimEmitter`
(getter 0x00109DD0, vtable 0x0050E108, parent 2). Earlier notes called it the
"book". It is the floating powerup pickup: it flies to a landing point, bounces
up to three times, pulses its sprite, and is collected when a player's cylinder
or carried actor's shapes touch it. Translated in `fx_anim_emitter`:

- sizes 0x00109DB0, constructor 0x00109FC0, destructor 0x0010A120;
- message 0x0010A180;
- launch 0x00109D10, flight and bounce 0x001099C0, attachment release 0x00109980;
- update 0x0010A2E0, player scan 0x0010A590, collection 0x0010A6D0;
- visibility 0x00109E30 and draw 0x00109EE0;
- burst parameters 0x00069590.

Shared helpers:

- `actor_runtime_queries`: 0x0003D860 view radius, 0x00040550 matrix reach,
  0x00041CB0 sound check, 0x00042560/0x000425D0 shape walks and 0x001205F0
  cylinder separation.
- `view_frustum`: 0x00159AD0, which classifies a sphere against the display
  view's six planes.
- `intro_notices`: 0x0013FF70 moved out of the player strategy, and 0x0013FF00
  added. The gate now takes a narrow `RuntimeIntroNotices` view. DAT_00511B3C's
  next pointer is DAT_00511B40, the same player list the emitter scans.

Instruction-level details: message 7 only forwards animation events 0x1C, 0x1E,
0x20, 0x21 and 0x2F within 100 units of the camera. The base draw reads its kind
from the third argument, which the emitter also tests for its sprite pass. The
collection test reuses the player-cylinder height slot for the shape diameter.

`NativeSceneActorClassOwner` installs type 0xD over type 2 with slot adapters
for the constructor, destructor, message, update, draw and visibility.
`NativeSceneEmitterBindings` carries the frame context, timed attachments,
clock, display view, flag blocks, player list, resume gate, story byte, intro
notices, emitter list and the award binding. The native actor base does not yet
build model matrix records, so the emitter's matrix view starts with zero
records and the base's cached reach words.

Not translated: the powerup award 0x00123E70, reached only when a player
collects an emitter. It is a required binding, `NativeSceneEmitterBindings::award`.
Its tree is about 45 functions: the DAT_00512140 powerup table, scoreboard HUD
messages (0x001316F0, 0x00132450, 0x00132540), the 0x000D0870 challenge camera
path through 0x0005B540, and the first-bonus-book challenge path 0x000F24C0. That
path writes a signed save through 0x0010EB60, 0x000E9A00 and 0x000EB180. Most file
wrappers already exist in the movie module.

Validation: `fx_emitter_oracle.py` matches retail on seeds 7, 11, 17 and 19
(3,600, 3,600, 3,800 and 1,900 cases) with zero mismatches, and twelve mutants
are each detected. Release builds, all six CTests, and the dialogue-resource,
actor-loading and decoder suites pass. Nothing was staged or committed, and no
retail payload or Ghidra project was changed.

### 2026-10-12: powerup award tree, first piece

The award 0x00123E70 is still a required binding of the FX emitter. Its tree
splits into three parts; the first is now translated and verified.

**Translated (`frontend/scoreboard_messages`, `entities/powerup_awards`,
`audio/stream_restart`):**

- scoreboard messages: 0x001316F0 add, 0x001319C0 remove, 0x00131BD0 icon
  lookup through `aid_aidlist_ghoulies_scoreboard_icons`, 0x00132450 powerup
  icon, 0x00132540 award caption, 0x0012D7E0 bonus counters;
- the award table (DAT_00512140, 20 rows of 0x18): 0x00124320 cancel;
- 0x0004A370, which adds time to the kind 0x10/0x11 entries of DAT_004E17D8;
- 0x000D0650, the challenge record lookup in DAT_005064E0;
- 0x00038880, which restarts a named audio stream unless it is starting or
  younger than 0.24 seconds.

`request_namespaced_localtext` (0x000FF0A0) now returns the 0x000FF410 result,
which 0x00132540 tests. The scoreboard (DAT_00545ABC and *DAT_005459F8) is a
narrow native struct with the fields these calls touch; its constructor
0x00130BB0 is not translated.

Instruction-level details: in 0x00123E70, case 0xE checks the 0xF row
(DAT_005122A8) and cancels kind 0xF, and case 0xF checks the 0xE row and
cancels kind 0xE. Cases 2, 3, 5, 6, 8 to 0xD, 0xF, 0x12 and 0x13 pick sound
row 0; cases 4, 0xE, 0x10 and 0x11 pick row 1; case 7 picks row 4; case 1
picks row 2 or 3 from the bonus count. An unmatched kind uses row -1
(0x003121D0, the preceding objparams name). The rows are XACT names at
0x00312250, stride 0x80.

Validation: `powerup_hud_oracle.py` has zero mismatches on seeds 3, 7, 11 and
17 (10,000 cases), and eleven of thirteen mutants are detected. See the oracle
README for the two survivors.

**Still untranslated below 0x00123E70:**

- Kind 5 (EventComplete), 0x000D0870: the challenge door cutscenes.
  0x000D0490 frees the challenge table; 0x0005B510 and 0x0005B540 run the door
  rows of DAT_004EE2B8 (stride 0x154, cutscene instances at +0x140/+0x144)
  through 0x0005A9B0, 0x0005B2D0, 0x0005B890, 0x0005B960, 0x0005BDB0,
  0x0005BF30, 0x000663B0, 0x001117E0, 0x00036A00, 0x001410F0 and 0x0013B1F0.
- Kind 1 (Book), 0x000F24C0: sets the challenge bit, queues the
  first-bonus-book or first-challenge intro, and, when a save is active
  (DAT_00506C90), copies the 13 challenge bytes into the save slot (0x000F3230,
  0x0010E5B0) and writes it. The write is 0x0010DC20, 0x0010E2C0, 0x0010DA80,
  0x0010F2D0 and 0x0010EB60, through the file jobs 0x000E9A00, 0x000E9A10,
  0x000E9AF0, 0x000E9C20 and 0x000EB180, with the XAPI file and signature
  routines at 0x0014A91C to 0x0014CF9B. The save-slot manager DAT_005462E4 has
  no native owner yet; `saves/save_names.cpp` holds only the name scan.
- Kind 0xC (MiniCooper) reuses the translated 0x001349E0, but needs the marker
  object context. Kind 0x13 walks DAT_00511B48 with per-actor views.

Neither remaining part is on the path to a live spawn: the award runs only when
a player collects a pickup. Live spawning is blocked first by the scene
resource layer. The actor startup reads model asset views, but no live
"model" hook is registered: `register_runtime_model_resource_hook` needs the
D3D relocation boundaries and a prepared relocation image per bundle entry.
Nothing was staged or committed.

### 2026-10-12: live model resources

Actor construction reads model asset views (actor +0x204 through 0x0003E940),
and no live code produced them. The scene's resource provider
(`SceneDialogueResourceOwner`, owned by `SceneFactoryRuntime`) now registers
the model hook 0x00052F00 and relocates real retail models.

- `scene/scene_resource_memory` is the address-space adapter the relocation
  code needs. Retail stores absolute 32-bit addresses inside resource CPU
  blocks, GPU segments and model heap blocks. The scene keeps native copies
  and gives each a distinct address from a token arena at 0x40000000. It also
  provides the heap boundaries (allocate, physical allocate, free), the
  blendshape boundaries and the host D3D boundaries. The host D3D keeps a
  record per header address instead of Xbox D3D headers; the transpose
  boundary transposes the 4x4 matrix in place, since model data depends on it.
- 0x00034730, the relocation pass a bundle runs when it opens, is translated
  (`relocate_animation_bundle_pool`). The native provider prepares records one
  at a time, so each model record runs the pass when first found. Preparing a
  model copies its CPU block and each GPU chunk (offsets into the bundle's GPU
  heap) into the resource memory.
- The node-type registry (*DAT_004370A0) is a runtime allocation in retail;
  the provider allocates its descriptor table in the resource memory and uses
  that address.

Validation: `gbtg-transition-tests GAME_DIRECTORY --scene-models` acquires all
16 models in the opening scene's bundle (imp, shadow imp, butler, brambles,
the central hallway background, doors, the powerup book, the rope barrier and
the unbreakable chair). Each relocates, post-loads, publishes its asset view,
and builds and releases a model instance. Release builds, all six CTests, the
dialogue, actor-loading and decoder suites, and a 120-frame headless run of the
opening scene pass.

**Executable resources and animation tables.** Animation tables are not
bundle resources: they are records of the XBE's own animtable pool. Boot
(0x00034C30, then 0x00034BC0) registers seven executable pools in order:
animtable (0x0033AC88, 0x56 records, active), attackdata (0x0033E100, 0x1D,
active), hitreaction (0x00343800, 0x27, active), objparams (0x003D7928,
inactive), scripttable (0x003E8A10, 0x23, inactive), objparams (0x00402CA0,
inactive) and statetable (0x004202F0, 0x29, active). `SceneExecutableResources`
now reads all seven, where it read only the two objparams pools. The scene
registry now registers the executable provider (0x00034E90/0x00034EC0,
category 4) ahead of the bundle provider, as boot does.

Each animtable record is a 0x14-byte header: +0 record count, +4 records,
+8 alias count, +0xC aliases. Each 0x118-byte record holds a name pointer at
+0, the animation and events names inline at +4 and +0x84, four floats from
+0x104 and four bytes from +0x114; each alias is a pair of name pointers.
`SceneExecutableResources` decodes all of them into
`AnimationTableResourceViews` (a pointer-width adapter over the XBE bytes).
The `--scene-models` check resolves all 86 tables (2,004 animations) through
the registry, and the translated binary searches 0x00032180 and 0x00032210
find every record and alias.

Animations need no relocation in the native model: the translated Mina player
reads the raw bundle bytes with offsets, so `aid_anim_` resources already work
through the bundle provider. The `anim` relocate hook 0x0002DD90 must not be
applied to the bytes those readers use.

**Next: the `ActorModel` view.** The actor code reads `ActorModel.cpu` as the
serialized model, with offsets (0x000408D0, 0x0003D730, 0x00040550 and
others), while the relocated copy behind the asset view holds absolute
addresses. So each model needs both: the asset view over the relocated copy,
and an `ActorModel` over the original bundle bytes plus the instance and pose
pointers. Some readers (for example the render registration's +0x10 read) take
fields that retail fills only at post-load, so each consumer needs checking
against its dump before the view is wired in. The remaining hook types
(callout, camera, cutscene, font, loctext, texture relocate 0x00053520) are
also still unregistered, and the player model comes from a bundle the provider
has not been exercised with yet.

### 2026-10-12: live marker start, first stage

`SceneActorServices` (`scene/scene_actor_services`, owned by `SceneFactoryRuntime::start_actors`) is
the scene's actor service graph. It holds native storage for the retail globals the actor, player,
mind, strategy and scene-class constructors read, installs the class owners over the scene factory,
loads the markers and runs 0x00140020. Values read from initialised XBE data name their address;
zeroed storage is BSS whose scene-start value is zero. Services whose contexts are not built raise
when reached. `gbtg-transition-tests GAME_DIRECTORY --scene-spawn` runs it on the opening scene.
Marker start is not yet called from the live scene start.

What it needed, in the order the opening's markers reached it:

- **Scene record.** 0x0011CB80 and 0x00123A30 read the full script aid (`aid_script_...`), not the
  short scene name.
- **Object addresses.** The scene's type 1 now binds each instance's address when 0x00107700 runs,
  so derived constructors can name themselves. `address_of` covers scene objects, file-backed XBE
  memory (mapped back to its XBE address) and the scene resource memory. Plain bundle records are now
  copied into the resource memory, as models already were, so stored resource pointers have addresses.
- **Executable record views** (`SceneExecutableResources`): statetables (rows of 0x108 at +4, the body
  action table), attackdata sequences (+0x10, +0xC words), the row names statetables point at from
  +0x104, and scripttables (rows of 0x84 at +4, the mind goal tables).
- **Statsheet.** DAT_00545AC4 is `aid_misc_ghoulies_statsheet_actors` (boot, 0x00033030); 0x00133B70
  counts its rows up to `objTag_Null`.
- **`ActorModel` view.** The model hook's post-load also publishes the actor +0x204 view: the original
  bundle bytes, with the runtime header that 0x00052B70 allocates appended and its block pointers
  written as offsets (directory +0x10 points at it). Body constructors get the directory blocks and
  the instance channel word (+0x1CC) from it.
- **Loaded-row sizes.** 0x00035D90 reads loaded rows +8..+0x14. For bundle records these are the CPU
  pointer, the file chunk size, and the resource chunk (relocated to its metadata by 0x00034590 only
  when its size is nonzero). The scene provider fills a parallel table as it loads. Executable records
  carry their recorded size. Loose localisation and demand files come from other retail providers and
  their rows are not mapped.
- **Missing resources.** 0x00032700 halts on the disc-error screen; the scene's error boundary now
  raises with the last name no provider found.
- **Player bundle.** 0x00124420 (from 0x0011CAC0) adds the bundle of the template 0x00123A80 selects,
  unless 0x00124490 finds the scene in the six no-player scripts at 0x00430FB0. Scene loading now loads
  `bundles/aid_objparams/ghoulies_actor_player_boy.bnl` for the opening.
- **Cinema minds.** The player owner now creates the type 0x35 lifecycle with the mind; its message,
  tick and request contexts are attachable later, as the player mind's already are.
- **Body action classes.** Bodies construct their statetable's initial row (key at body parameters
  +0x188). Both the imp and the boy start in `stand`: move 0x5B `entityMoveXYZnoG` and state 0x87
  `entityStateStandAlert`. Translated: weapon base 0xA (sizes 0x0007FCC0, constructor 0x0002B600),
  0x5B (0x000CFFD0), actorMove base 9 (sizes 0x00095530), and 0x87 (sizes 0x000A2510, constructor
  0x000A2590, destructor 0x000A2790, with body shape-mode helper 0x0001FEB0). The state classes take
  a native view of their body, found from state +8.

**Result:** all five opening imps construct (actor, player class, body, cinema mind, imp fire
strategy and their stand actions), and the scene releases them cleanly. The boy's actor and bodies
construct. Construction now stops at the player's camera actor: the player mind creates
`aid_objparams_ghoulies_actor_cameron` (camera body type 0x28, cinema mind type 0x36, director
strategy type 0x39). Its mind lifecycle is trivial (0x0007AFC0, 0x000CFFD0) and its strategy is
small (0x000AAE40, 0x000AAEB0). Its body constructor 0x000731F0 links a view record at body +0x894
into the camera list DAT_004E1680, the same list the legacy live camera entity (`entity_camera.cpp`)
uses, so the two have to be reconciled first.

The rest of the action family matters once actors change state: 51 `actorMove` state classes and
five weapon move classes, of which only 0x5B and 0x87 are translated.

Validation: Release builds, all six CTests, the dialogue, actor-loading, decoder and scene-models
suites, and 120-frame headless runs of the boot and opening scene pass. `--scene-spawn` fails at the
camera actor as described. Nothing was staged or committed.

### 2026-10-12: live marker start, second stage (camera actor and activation)

**Marker start now completes.** `--scene-spawn` creates every opening marker object, including the
player's camera actor `cameron`, which the player mind creates (camera notice 0xBA):

- camera body type 0x28 (`camera_actor_classes`): constructor 0x000731F0 links its view record (body
  +0x894, native `EntityCamNode`) into camera slot 0 and binds `cameron` (string 0x00323E30) through the
  translated 0x00048C20/0x00048CF0; destructor 0x00073300 unlinks it. DAT_00510F70 is
  `g_camera_actor`.
- camera mind type 0x36: sizes 0x0007AFC0, constructor 0x000CFFD0, message 0x0007B040 (navigation
  setup 0x00136700 on 0x7D, then the parent mind handler 0x0002A2E0).
- director strategy type 0x39: sizes 0x000AAE40, constructor 0x000AAEB0, enter 0x000AA3A0 (mind
  requests 3/4/0x12 for states 0x3F6/0x3F7/0x401, else 0x0002BAF0), leave 0x000AA450, message
  0x000AAED0 (0x7D places the actor at the marker camera start through 0x0013CC70, now beside the kind
  0x13 handler; 0xBA/0xBB change state).
- move class 0x5C (vtable 0x00505500), the camera's initial `move` row.

The player owner now records each strategy's real factory type and routes enter/leave to the player,
imp fire or director methods.

**Activation.** Scene startup's 0x00123070 (`scene_actor_activation`) is translated: it sends {0x23,
0x7B, previous actor} to each pending actor (DAT_00511B08), moves the actor's +0x900 node to the active
list DAT_00511AA4, records it on DAT_00511B5C, and sends 0x7B notices to the active list and
DAT_0050E438. The services attach the message graphs this needs:

- player class message 0x00028F60 for every type 5 actor, with its activation (0x00029270), pause and
  navigation contexts, and child dispatch by factory type;
- `scene_body_messages`: the parent body handler 0x000253B0 and the imp (0x00074080), player (0x00075B60)
  and camera (0x00073320) body slots, with the damage, death, timer, sound, effect and projectile
  contexts. Statetable action children of types 0x5B, 0x5C and 0x87 have the empty 0x001628B0 message.
  Ids whose handlers read message-pointer views (7, 0xF, 0x15-0x19, 0x1B, 0x1D, 0x2A, 0x45, 0x60,
  0x63) raise until those views exist. The push-key table and the per-call damage animation rows are
  not located yet (an empty key view raises on read);
- `scene_mind_messages`: cinema minds (0x0007C8B0) and player minds (0x0007F1E0) over the parent-mind
  requests, with a body snapshot for the read-only `RuntimeScriptMindQuery`;
- the `callout` resource hook (0x0004F610, post-load 0x0004F650, unload 0x0004F6C0), which links each
  callout resource's terminator (+0xC8) to the next resource the callout walk follows;
- attackdata request tables (body +0x5C) decoded from the executable records.

Supporting fixes: retail tables for flag (0x00338FB0), state action (0x003390E4), callout (0x00421C9C)
and goal flag (0x003397AC) names; the case list is `g_world.case_head` (DAT_00511B3C), so the player
link DAT_00511B40 is its next node; world list heads and player link nodes have retail addresses;
engine-heap input devices get token addresses and join `input_addresses`; a scene actor's native
`Actor` maps to its object address. `EntityBonePose` can view a model instance's `ModelPoseBuffer`, and
the services refresh each actor's shape poses (instance +0x164/+0x168) before dispatch. The player
owner defers releasing a player's native state to the type 1 destructor, since retail keeps the type 5
vtable through the whole destructor chain and the base actor destructor still messages the player.

**Current stop.** The first imp's activation reaches the body's 0x23, whose contact separation sweeps
the world segment list. Every activated actor's segment probe is in that list, and the actor
collision slots that 0x0003C080 installs (+0x1C 0x0003DF70, +0x20 0x0003E110, +0x24 0x0003E050,
+0x2C 0x0003E200, +0x30 0x0003E3C0, +0x34 0x0003C0F0) are untranslated. They raise for now. Their tree
is 48 functions: culls (0x0003E4A0, 0x0003E590, 0x0003E680, 0x0015A270), shape-list dispatchers
(0x00055670, 0x000557E0, 0x00055960, 0x000568E0, 0x00056A60, 0x00056BE0) and the per-shape routines
in the tables at DAT_00423810 (physical kinds 1 and 2) and DAT_00423858 (model-node shapes). The
callers (for example 0x00122480) pass each slot (actor, candidate, kept from EBX, first parameter,
second parameter, record); the slot ANDs the first parameter with the actor's filter (+0x1F0). The
shape-list dispatchers take the candidate in EBX and the kept point in EDI, and call each shape routine
as (shape, xform +0x2AC, inverse +0x2DC, pose, block +0xC, candidate, kept, mask, second parameter,
record).

Validation: Release builds, all six CTests, the dialogue, actor-loading, decoder and scene-models
suites, and 120-frame headless runs of the boot and opening scene pass. `--scene-spawn` stops at the
first untranslated collision slot. Nothing was staged or committed.

### 2026-10-12: collision dependency continuation

Reviewed the latest marker/camera/activation changes before continuing. Added
shared collision sphere overlap 0x0015A270, actor segment/swept-sphere/sphere
culls 0x0003E4A0/0x0003E590/0x0003E680, and physical shape-list gate
0x000559F0 in actor_collision_culling. Culls call the existing genuine actor
view radius 0x0003D860 and world-position query, retaining SSE midpoint/distance
order, x87 square-root/radius addition, and unordered sphere comparison.
No actor collision slot is registered yet: shape dispatch and per-shape
methods still block activation. This batch requires compilation and focused
retail coverage; live spawning completion is not claimed.

Collision checkpoint validation: Release game and transition-test builds pass,
all six CTests pass, and git diff --check passes. The eighteen-mode retail
oracle passed 1,800 cases (seed 71), zero mismatches. Added modes compare
0x0015A270 and all three culls using genuine null-model 0x0003D860 and world
position calls, including NaN/zero inputs; a separate mode checks shape-list
0x000559F0 with negative/zero counts, physical kinds, tags and stride walking.
Model-backed cull coverage and the shape-list/per-shape collision methods
remain outstanding. No substitute collision return or slot bypass was added.

Larger collision batch in progress: translated all six list dispatchers
0x00055670/0x000557E0/0x00055960 and 0x000568E0/0x00056A60/0x00056BE0,
preserving physical dword strides, model byte strides, signed bone-mask
indices, accumulated hit results and contact epsilon offsets. Added actual
model sphere routines 0x00055CB0/0x00055F80/0x00056580, their contact writer
0x000563F0 and bound transforms 0x0010AAE0/0x0010AF90. Model kind-1 table
entries really call empty 0x00055A90, which is translated as such; kind-2
entries are genuinely null. Physical kind-1/2 routines remain mandatory
callbacks for their exact retail addresses, with missing bindings throwing.
No physical routine is faked, and actor slots are not registered yet.
This thirteen-function batch awaits compilation and focused retail coverage.

Thirteen-function checkpoint: both Release targets and all six CTests pass;
git diff --check passes. A 19,000-case seed-79 run passed with zero mismatches,
including direct sphere segment/sweep/contact calls and full seeded 0x54-byte
contact records. A 2,000-case seed-83 twenty-mode run also passed after fixing
the dispatcher fixture to load actual DAT_00423858 function pointers. It
exercises all three model dispatch slots with multiple rows, signed bone-mask
indices, kinds 0/1/2/3, null entries, actual empty kind-1 calls, sphere hits,
and contact epsilon adjustment. Initial matrices were identity; the next
focused fixture adds arbitrary affine matrices. Bone poses and contact-pool
growth still need coverage. Physical callbacks remain unbound and actor slots
still reject activation; this is dependency progress, not live acceptance.

Affine checkpoint: 2,000 cases (seed 89) passed with zero mismatches after
correcting matrix summation order against assembly. The 0x0010AAE0 sphere
bound and the 0x00055CB0/0x00055F80 output point sum different term orders
from contact writer 0x000563F0; those orders now remain distinct. Coverage
includes arbitrary affine world/inverse matrices and unchanged seeded contact
bytes, with genuine retail callees and actual dispatcher tables. Both Release
targets, all six CTests and diff check pass after the fixes. Physical kind-1/2
mesh routines and actor slot installation remain the next activation blockers;
skeletal poses and contact-pool growth are not certified by this fixture.

### Physical mesh integration continuation

Added physical kind-1 segment 0x00053720 and swept-sphere 0x00053AA0 paths,
their mesh cell bounds 0x00054340, triangle segment query 0x00103400,
filtered triangle selectors 0x000551A0/0x000553D0, and triangle contact writer
0x00054170. Native bindings for these exact two physical table entries now
call their genuine implementations. Cell records retain their +0x2C layout,
u16 dimension strides, selected triangle address, early zero-fraction exit,
and cumulative movement shortening. No activation slot is enabled while the
kind-2/contact/push dependencies are missing. Initial 2,100-case seed-97
primitive/contact comparison passed with zero mismatches; expanded direct
mesh segment/sweep coverage is running. Both Release targets build.

Expanded mesh checkpoint: 2,200 cases (seed 101) passed with zero mismatches
across twenty-two modes. New direct mode runs retail 0x00053720 and
0x00053AA0 through real cell-bound, filtered triangle, sweep and contact
writer callees on a single-cell mesh, comparing returned hits, kept points
and seeded contact bytes. No per-shape callback capture or callee stub is
used. The kind-1 segment and sweep table bindings are available, but actor
slot installation is still held: 0x00053F20/0x000554D0 sphere collection,
kind-2 pose-linked routines, their contact writer 0x00054DE0, and actor push
0x0003C0F0 remain untranslated. Exports for these dependencies are now cached
read-only, including the required 0x0015A440/0x0015A2E0 geometry. Live scene
activation has not been advanced past the existing collision stop yet.

Physical mesh batch validation: 4,400 cases (seed 103), zero mismatches;
both Release targets and all six CTests pass. Added pose matrix helper
0x000544B0 using actual 0x00031530 model-pose lookup, 0x00104400 inversion,
and 0x001041B0 matrix multiplication, with the retail null-pose copy branch.
This eighth translated function compiles but has not yet received focused
pose-backed acceptance. The mesh methods are available through exact-address
native callbacks, not installed actor slots. Remaining slot dependencies
above are still real blockers, and resource/cell address adapters must use
the scene's actual model storage when integrating them.

Sphere-collection/pose-linked batch: added sphere/triangle test 0x0015A440
and projected point/triangle test 0x0015A2E0, then triangle collection
0x000554D0, kind-1 sphere collection 0x00053F20, posed contact writer
0x00054DE0 and all kind-2 methods 0x00054520/0x00054850/0x00054C40.
All six physical table entries now have native bindings to translated methods.
Pose-backed bindings require the actual ModelPoseBuffer; an unbound native
pose raises instead of substituting a null pose. The initial 2,300-case
seed-107 geometry comparison passed, including finite and degenerate inputs.
Collection/pool growth and pose-linked methods await their focused coverage.
Actor collision slot installation and actor push remain outstanding.

Actor query batch: translated 0x0003DF70/0x0003E110/0x0003E050 and
0x0003E200/0x0003E3C0 over the genuine physical/model dispatchers, including
class-mask filtering, culls, alternate/primary pose selection and actual
0x000FCD70 contact copy for physical sweeps. These method bodies compile;
the factory's live slot adapters still need the actual model CPU address
space and model +0x16C mask words, not invented mask defaults.
Expanded 6,900-case seed-127 comparison passed with zero mismatches across
twenty-three modes. The direct mesh mode now exercises all six physical
table entries (kind-2 uses the actual null-pose copy path), and contact-writer
coverage includes both null and arbitrary affine bone matrices. Pool-backed
collection/growth, non-null ModelPoseBuffer lookup, actor-query methods and
NaN sphere/triangle behavior still require their own coverage. No live
activation advance or fully wired collision class is claimed yet.

Live collision integration checkpoint: installed all five translated actor query
slots with owning actor bytes, original model CPU resource tokens, actual
physical table methods, model shader words and native poses. Contact collection
bridges the scene SegmentPool to the native translated pool, retaining the
complete seeded capacity and propagating growth/cursor state. The first live
activation advance now passes the former collision-slot stop, reaching an
empty player-mind object resolver on nested message 0x11. Corrected the owner
adapter to resolve message +4 only where retail 0x0002A5A0 actually dereferences
it (the matching 0x8D target branch), and attached the scene resolver. Added
actor/child/message diagnostics to activation errors. Actor push remains
outstanding; activation is not yet accepted.

Activation ownership/input checkpoint: registered the controlled-player input
device with the existing scene input address map. Replaced the empty stack
input-pool header with the actual four-entry, stride-0x50 heap allocation
from retail 0x00033030 (0x00033274), retaining grow=4 and publishing the
rebound pool/selection to dialogue input after messages. Live startup now
activates Cooper, five imps and cameron, then reaches a separately owned
type-2 attachment actor. Scene object lookup now includes camera/callout
ownership and player-owned factory children; previously the pending-list
adapter dereferenced null for these actors. The next contact notice requires
the base actor +0x934 node identity for comparisons, now exposed in the
factory view instead of rejecting every non-player actor. Temporary traces
are being used to locate the remaining live stop and will be removed.

Live activation pass now drains all 18 pending actors and the empty second
pass leaves the active list unchanged. Acceptance was strengthened to compare
the exact spawned-address set, reject duplicates/foreign actors and require
zero pending nodes. All six CTests pass. Teardown exposed two native lifetime
issues: scene class message views must survive their derived destructors until
the actor-base destructor sends render-bin message 0x12, and separately owned
camera/callout roots need explicit factory release while services remain alive.
Camera-root release also required the scene address map and active factory
owner to remain alive: the service owner is now reset explicitly in the scene
destructor before those dependencies are cleared or destroyed. The actor-query class
mask now reads the native Actor.model_flags alias of +0x1F0 instead of stale
serialized instance bytes. Full live exit/teardown is being rechecked.

Activation completion checkpoint (2026-10-12): the strengthened --scene-spawn
run activates all 18 spawned actors, leaves pending=0, verifies the exact
address set with no duplicates, and verifies an empty second activation pass.
The run now exits 0 after releasing both marker roots and separately owned
camera/callout roots. Native scene-class message views survive through the
actor-base destructor; scene services are explicitly destroyed before the
active scene pointer/address map are cleared. Temporary traces are removed.
An explicit zero-factory/zero-address teardown assertion and final regression
batch are now being checked. Actor push 0x0003C0F0 is still a later gameplay
update dependency; it is not reached by this activation milestone.

Final activation validation: Release gbtg and gbtg-transition-tests builds pass;
--scene-spawn exits 0 with activated=18, pending=0, factory=52 before release,
and factory=0/addresses=0 after explicit release. The check compares the full
spawned/active address sets and checks the empty repeat pass. The world active
registry uses the shared g_world.latch alias of DAT_00511AA4, with a matching
list-count assertion and an empty-list teardown assertion. All six CTests,
actor-loading, dialogue resources (including player parent/message/update and
factory owner checks), and the 461-frame book/map scene transition pass. The
seed-149, 6,900-case scene-factory/collision oracle reports zero mismatches.
The 120-frame headless boot run exits 0 (crash check only). No retail payload
was edited, Ghidra project mutated, or files staged/committed. Pool-growth and
non-null-pose collision comparisons remain explicit coverage gaps; the live
activation result does not certify every later gameplay collision branch.

Shared actor-push batch started (2026-10-12): added actor_shape_push with
nine translated definitions, covering 0x0003C0F0 and its missing helpers
0x0003DD50/0x000418B0/0x0003C470/0x0003C3A0/0x0003C5D0 and
0x001206A0/0x00120950/0x00120BA0. These are base-actor methods shared by
all actor classes. Existing genuine sphere/cylinder transforms, cylinder
push and swept-sphere solver are reused. D3DX normalization and vtable
message dispatch are explicit required context bindings. Added focused
retail comparison fixtures for bound metrics, offset-cylinder culling,
planar actor delta and attachment projection. Native slot installation
and contact-message bridging remain pending; no push acceptance claimed.

Push integration checkpoint: the 2,500-case seed-163 comparison passes with
zero mismatches, including the complete 0x0003C0F0 null-other path through
real sphere/cylinder transforms, multiple masked nodes and per-cylinder
contact writes. Added the four D3DX normalization routines from their actual
dumps (including the unrounded Z reciprocal and approximate-unit shortcut).
Installed the shared actor +0x34 slot over real model words and the global
contact-point index. Contact 0x0E/0x0F now converts sender pointers at the
scene boundary while preserving raw words for child handlers; the 0x0F body
handler receives actual source/body/bound views. Directed normalization,
attached push and the complete message round trip still await validation.

Directed push validation: seed 173, 5,200 cases, zero mismatches across
26 modes. Covers the genuine retail D3DX normalization entry/callees,
zero/tiny/unit vectors, scalar tolerance edges, vector division, directed
sphere sweep and attached-cylinder push with both query-pointer branches.
The oracle now loads the D3DX XBE section separately and requires every
retail call to return before its instruction budget (no silent truncation).
Contact-message integration reached the real player-mind 0xB6 event filter.
Translated its missing 0x00116890 linked lookup and the deduplication arm
of 0x00116950. Added a live Cooper/imp 0x0E -> 0x0F -> 0x2D round-trip
check that validates sender/receiver/class and deduplication.

### 2026-10-12: live contact-message round trip

The Release `gbtg-transition-tests` target builds, and
`gbtg-transition-tests.exe OriginalFiles --scene-spawn` passes. The check
selects Cooper through the scene's actual player link (actor +0x93C), finds a
live class-23 imp, sends the nine-word 0x0E contact message through the scene
actor service, and verifies the resulting B6 event's Cooper sender, imp
receiver, and class. Repeating the contact leaves exactly one B6 event. The
existing activation-set and teardown assertions still pass.

This validates the live message round trip and script-event deduplication, not
the spatial collision query or a full frame of player input, actor updates, and
rendering. `frame_tick` still ticks only `Game::entities` and draws `Game::models`
plus cutscene/UI paths. `SceneActorServices` exposes marker startup, activation,
and messages, but no frame update/draw entry points; player update 0x0002C2B0
also requires a per-player `RuntimePlayerUpdate` context wired to its strategy,
mind, body, collision, navigation, and actor-frame services. Factory actor
dispatch and model rendering in the frame loop remain the next implementation
work; this checkpoint does not claim a playable scene.

### 2026-10-12: scene-start broadcasts and the camera director

Retail scene start 0x0011CBD0 follows the marker pass (0x00140020) and
activation (0x00123070) with 0x00116810 (script 0x7D), then 0x001231F0 (actor
message 0x88) and 0x001231A0 (actor message 0x7D) over the activated +0x910
list. Both broadcasts are translated in `scene_actor_activation.cpp` and are
reachable through `SceneActorServices::send_scene_start_notice` and
`send_scene_start`. The new `--scene-start` test mode runs them after the
`--scene-spawn` checks; `--scene-spawn` itself is unchanged and passes.

0x88 runs the base actor pose update 0x0003AE00, which read the actor's
animation view with null pose buffers (an access violation for Cooper). The
factory's `ModelAnimationState` was only filled with the two pose pointers at
construction. `refresh_actor_model_animation_view` now rebuilds it from the
actor's current model instance before every scene message: poses +0x164/+0x168,
skeleton joints and count, auxiliary targets +0x1CC, node flags +0x294 and the
CPU type-1 resource record. Node flags the view changes are stored back
afterwards (only changed bytes, and only to the same model instance).
`RuntimeModelResourceBlock` now keeps the stored +4 word, because type-1
records are not relocated and hold the node mask 0x000320F0 reads.
All 18 actors now accept 0x88.

0x7D reaches cameron's director strategy (type 0x39), whose 0x7D arm calls the
actor's own vtable +0x10, the player update 0x0002C2B0. That update calls the
director's +0x10 tick 0x000AA250, now translated in `camera_director.cpp`
with its batch: 0x00028CC0, 0x000AA480, 0x000AA4D0, 0x000AA520, the target
comparator 0x000AA560, target collection 0x000AA590, input heading 0x000AADE0
and 0x0013DD80, scene-start check 0x0013D1E0 and body hold release
0x00020000. The CRT qsort 0x00166780 and its short sort 0x00166710 are
translated in `runtime/crt_sort.cpp`; the comparator never returns 0, so the
exact CRT algorithm decides tie order. The analog level readers 0x00108DE0
and 0x00108ED0 are in `dialogue_input.cpp`. The director tunables at
0x00428708..0x00428730 (and 0x0043400C) are .data values; retail defaults
skip both candidate walks (0x0042872C = 1) and the watcher checks
(0x00428708 = 1).

Current stop (`--scene-start`): cameron's 0x7D requires the per-player
`RuntimePlayerUpdate` context for 0x0002C2B0. For cameron the slots are the
director tick 0x000AA250 (now translated), camera mind tick 0x0002A9F0 and
camera body +0x10..+0x1C (0x00025EC0, 0x00072AC0, 0x00072B10, 0x00072CB0).
The last two are translated over the legacy `Entity` camera body, not the
factory body view, and need adapting before the update can be wired.

### 2026-10-12: camera mind rig and camera body slots

cameron's player update 0x0002C2B0 ticks its camera mind (type 0x36) through
the parent mind tick 0x0002A9F0, whose request methods are camera mind
vtable +0x14 0x00078170 and +0x18 0x0007AEF0. The director tick queues a
kind-1 request each frame; kind 1 is the camera rig 0x000781F0. Translated
from the assembly (several of these return through XMM0 or take EBX/EDI
outputs that the decompiles drop):

- `camera/camera_mind_rig.cpp`: 0x000781F0, rig placement 0x00078840 and its
  target framing 0x000783A0, stick turn 0x000784B0, heading ease 0x00078630,
  target merge 0x0013D640, obstruction tracking 0x00079720 and settling
  0x00079450, heading choice 0x00079320 with the eye sweep 0x000790B0 /
  0x00078C70 and heading goal 0x000791A0, input turn 0x0013DC40 / 0x0013DD00,
  floor height 0x0013D110, point offset 0x001062A0, heading clamp
  0x00101550 and aim offset 0x0013CE80. The tunables at 0x004274B8..0x0042753F
  and 0x00434008..0x00434033 are .data, mirrored in
  `RuntimeCameraRigTunables` by retail address.
- `camera/camera_mind_requests.cpp`: 0x00078170 (kind 1 to the rig; kinds 3,
  4 and 0x12 raise as untranslated; other kinds go to 0x000295E0) and
  0x0007AEF0 (kind 0x29 random span, body action key 0x19 / 0x1B).
- `navigation/cell_reach.cpp`: 0x0010FE10 (cells near a point; the retail
  heap list is internal to 0x0012CEF0) and 0x0012CEF0.
- `camera/camera_body_slots.cpp`: camera body +0x18 0x00072B10 with aim
  0x00072BA0, +0x1C 0x00072CB0, and Euler wrap 0x00105AA0, over the factory
  body bytes. The legacy `Entity` versions call an empty 0x00025AE0 and wrap
  with `std::fmod`, so they are not used.

The DAT_0050EA00 world-bound words (`g_amb_volume_enabled`,
`g_amb_color_lo/hi`) are now exported from `scene/ambient.cpp` through
`scene/ambient.h` for the player bounds check 0x0002C980.

0x000783A0 divides by the render size DAT_0054DD80/84 that display
initialization 0x000F5900 sets; that initialization is not installed in the
live game, so the rig raises if it is reached with a zero size.

Still to wire before `--scene-start` passes cameron's 0x7D: the per-player
`RuntimePlayerUpdate` (strategy, mind and body slot dispatch by child type,
body collision view, actor frame, navigation, room and bounds), the camera
body's +0x14 channel step 0x00072AC0 context, and the 0x00025AE0 body apply
context.

### 2026-10-12: live player update for cameron

`SceneActorServices::State::update_player` now builds the full
`RuntimePlayerUpdate` for 0x0002C2B0 from the player's native state and is
the director's actor update (0x000AAED0's 0x7D arm). Child slots dispatch by
factory type:

- strategy +0x10: 0x4F player (0x000BEF40), 0x46 imp fire (0x000B45D0),
  0x39 director (0x000AA250, wired over the scene's mark list DAT_00511B38,
  camera targets 0x0013D000 and the player input devices);
- mind +0x10: 0x36 camera mind through 0x0002A9F0 with the camera mind
  request methods 0x00078170 / 0x0007AEF0 (player and cinema minds raise);
- body +0x10..+0x1C: the camera body (type 0x28) through new
  `SceneBodyMessages::tick/animate/settle/supported`: 0x00025EC0, 0x00072AC0,
  0x00072B10 (with body apply 0x00025AE0) and 0x00072CB0;
- body action children: type 0x5C +0x10 0x00080340 (velocity ease 0x000803B0
  over the two-stage integrator 0x00103880 and its callbacks 0x00140700 /
  0x00140770) and +0x14 0x0002AD30, in `entities/body_move_action_slots.cpp`;
  the 0x5C class vtable has six slots, so +0x1C reads 0x000CFFD0 (returns 1).
  Types 0x5B (0x0007FD70, 0x000804E0) and 0x87 +0x10 (0x000A2930) raise.

Views that are not native yet raise when they would be read: body channel
rows (+0x150) and actor +0x50 bindings when +0x154 or +0xFC is set, actor
+0x8F8 bound and +0x8FC shapes, body +0x710 collision spheres, mind +0x1C
path search, the player contact callback 0x0001F8C0, and the camera rig's
render size. The director tunables, rig tunables, DAT_004312E0 and the
DAT_00338F64.. channel key rows are read from the image; DAT_005122D8 and the
render size are native storage at their BSS value 0.

`--scene-start` result: cameron's director 0x7D runs the whole player update.
The next stop is the camera mind's own 0x7D (0x0007B040 -> 0x00136700),
which reads the navigation node buffer DAT_0050E24C. That buffer, and every
`g_nav_grids` entry, is built by scene start's 0x00111820 from the background
actor's model; the background is factory type 4 created by 0x0011C8B0, whose
class (vtable 0x004E1644, constructor 0x00042DA0) is not installed natively.
The constructor's tree (lights, sectors, background nodes) plus the grid
builder is about 40 untranslated functions; their dumps are exported.

### 2026-10-12: navigation grid preparation dependency batch

Reviewed the intervening camera/director/player-update commits. The current
scene-start blocker remains the background factory and its navigation build.
Added `navigation/grid_preparation.cpp`: shared neighbor iterator 0x00036380,
height-bound accumulation 0x00111FC0, edge-cost initialization 0x001121B0 and
height-based connection initialization 0x00112560, from their cached assembly.
The iterator preserves direction advancement, connection/region/terrain gates,
surface-cost selection and SSE multiplication order. Surface costs are supplied
as a view of the retail DAT_002FCFB8 table, without invented defaults for reads.
These helpers are compiled but are not yet wired into 0x00111820. Added oracle
mode 27 for all four functions, including blocked cells, boundary rows, tagged
regions, negative/zero counts, zero/negative/NaN height thresholds and neighbor
iterator exhaustion. Validation pending; background construction is not complete.

Validation for this batch: Release `gbtg` and the scene factory oracle harness
build successfully. Oracle seed 179 passed 2,800 cases across 28 modes with zero
mismatches; 100 fixtures exercise the new grid preparation mode. Retail calls
must reach the return sentinel; grid walks receive a larger instruction budget
because they invoke the genuine square-root callee for multiple neighbors.
Fixed the pre-existing mode-26 harness declaration to use `gbtg::LiveScript`.
No live scene-start acceptance is claimed: background type 4, its constructor
0x00042DA0, model-grid decoding 0x00111B80 and remaining 0x00111820 preparation
passes still need translation and installation before DAT_0050E24C is populated.

### 2026-10-12: remaining navigation passes and model decoding

Added isolation marking 0x001122E0, pairwise connection removal 0x00112680,
and floor-connection collection 0x001127D0. Isolation retains the retail rule:
one eligible cardinal neighbor suffices; when the first eligible neighbor is
diagonal, a second eligible diagonal is required. Walks mutate cells in order.
Connection removal reads both original bytes before writing either endpoint.
Floor connections retain stride 0xC, growth 4, and untouched row +8 bytes.
Extracted the existing registration-pool grow into the shared translated
0x000662A0 helper, used by both registration and floor-connection collection.

Added `navigation/model_grid.cpp`: model point bounds 0x0004F1B0, cell-aligned
bounds 0x0004F460, grid allocation 0x000362F0, and model-cell decode 0x00111B80.
The decoder converts coordinates through CVTTSS2SI, preserves region/flag bit
assignments and the blocked-cell bitset, and invokes the real 0x00122480 ceiling
probe. Native ownership releases grid storage and temporary bitsets. This batch
is not yet built or oracle-validated, and scene-start wiring remains pending.

Added the complete navigation build sequence 0x00111820 over the first background
actor's offset-based model CPU view. It selects blocks 0xB / 0x17 and link blocks
0x19 / 0x1A, resets the two height bands, decodes available floors, executes all
preparation passes in retail order, releases temporary blocked-cell bitsets,
collects cross-floor links, and allocates/initializes DAT_0050E24C's stride-0x14
node records. `NavigationSceneStorage` owns the grids/node buffer and releases
its published globals and floor-link pools on destruction. This owner is not
yet installed into `SceneActorServices`; the live background root remains absent.

Validation: Release `gbtg` and the scene factory oracle harness build successfully.
Seed 181 passed 2,900 cases across 29 modes with zero mismatches for the helper
batch. Seed 191 passed 3,000 cases across 30 modes with zero mismatches, including
100 full-builder fixtures: absent background, one floor, two floors, and an
upper-floor block without the required lower-floor block. Compared complete cell
bytes, grid headers, height bands, aligned origins, floor-link rows, blocked
bitsets and node records. Model decoder/builder fixtures call the real 0x00122480
with an empty world segment list; background collision hits are not yet covered.
Only genuine heap API calls are intercepted by the new full-builder fixtures.
Oracle generation now asserts the requested fixture count and checks missing
harness result rows against the expected corpus size.

Next: translate/install background type 4's 0x00042DA0 constructor and remaining
light/sector/animation dependencies, bind the background root and scene-owned
navigation storage, then advance `--scene-start` beyond the camera mind's node
buffer guard. No live scene-start acceptance is claimed for this batch.

### 2026-10-12: background bounds and ambient-node construction

Added physical triangle bounds callback 0x00043C70 and background bounds setup
0x00043190. The latter invokes the existing model bounds, world-volume setup,
physical-shape presence check and real shape-event visitor; triangle bounds use
the retail unordered comparison behavior and remain separate from world bounds.
Added ambient-node setup 0x000432B0 for kind 8 / 9 / 0x1F model nodes, preserving
alternate-pose precedence, packed/unpacked bone matrices, actor matrix arithmetic
order, emitter append order and one RNG step for each kind-9 node. The ambient
view aliases existing count/vary/phase flags and adds their previously missing
point/color storage. Implementation landed; validation pending. The background
constructor remains unregistered while its animation/effect/sector callees are
incomplete.

Bounds/ambient validation: seed 199 passed 3,100 cases across 31 modes with zero
mismatches. Fixtures cover triangle bounds with NaNs, absent model/CPU/shape
lists, physical-event traversal, zero/nonzero shape flags, seeded globals,
all three ambient-node kinds, primary/alternate poses and both matrix formats.
The oracle exposed EDI = 0xFFFFFFFF in 0x00043190: this is a filtering mask,
not mask 0; zero-flag shapes/events are skipped. The implementation preserves it.

Added three shared animation dependencies rather than background-only versions:
0x00053000 finds the first type-0x11 matrix-array block, 0x00155FF0 computes a
matrix row offset with 32-bit wrap, and 0x0010BD40 averages model points in source
order with an extended reciprocal. Extended the oracle to include zero counts,
in-place averaging, first-match/null matrix blocks and wrapping matrix indices.
Validation of this extension is pending.

Final validation for the six-function background/shared dependency batch:
Release `gbtg` and scene factory oracle harness builds pass. Seed 211 passed
6,200 cases across 31 modes with zero mismatches, including 200 fixtures spread
across the six new functions. Shared point averaging covers in-place source/output
aliasing; matrix helpers cover null/first-match lookup and full 32-bit index wrap.
No CTests or live scene-start acceptance were run for this unwired constructor
batch. Remaining constructor blockers: sector setup 0x000FEFD0 and its callbacks,
effect registration 0x00043B40 -> 0x0005B2D0, animation initialization 0x00044490
(including 0x0010B1F0 and 0x00151FAD), and destructor 0x000430C0. Background
factory type 4 and the scene-owned navigation builder remain uninstalled.

### 2026-10-12: sector colors and shared graphics inverse

Recovered complete read-only dumps for 0x000FEFD0 and the five jump-table cases
of 0x000FEBE0. `ExportPlayerScene.java` now supports forced temporary ranges and
explicit disassembly seeds; its Ghidra transaction still rolls back every change.
Added `background_sectors.cpp`: settings lookup 0x000FEAC0, color-grid walk
0x00052660, brightness range 0x000FEB70, HSV remap 0x000FEBE0, and sector setup
0x000FEFD0. The passes keep column-major callback order, the first matching AID
setting, the default 0.3 / 0.3 / 1.2 settings, reversed brightness intervals,
upper-only value clamp, hue switch cases, and actual 0x001673D8 byte conversion.
Added the shared 4x4 inverse 0x00151FAD and finite-double test 0x0016744D, retaining
float spills, extended minors/cofactors, determinant order, the rounded reciprocal
filter, and untouched inverse output when that reciprocal is non-finite.
Implementation batch landed; build and differential validation pending.

Validation for this seven-function batch: Release `gbtg` and the scene factory
oracle harness build successfully. Seed 227 passed 1,400 focused mode-31 cases
with zero mismatches; seed 223 passed 3,200 cases across all 32 modes with zero
mismatches. Coverage includes callback positions/order, zero-sized color grids,
settings first-match/default behavior, NaN/reversed color intervals, gray/black
colors, wrapped byte conversion, inverse output aliasing, optional determinants,
duplicate/near-duplicate matrix rows, small/large diagonal matrices and finite
classification. No callee capture stubs were added for this batch.

Two oracle findings corrected: the color clamp requires the actual ordered
COMISS/JA predicate (unordered values clamp to 1), and Windows `long double`
loses determinant cancellation bits. `graphics/extended_float.h` now represents
a 64-bit significand using compensated arithmetic, rounds extended operations,
preserves signed zero and resolves float rounding ties. The inverse keeps the
retail float spills and uses this view for values retained in x87 registers.

Sector setup 0x000FEFD0 and inverse 0x00151FAD are now translated. Remaining
constructor dependencies are effect registration 0x00043B40 / 0x0005B2D0,
animation initialization 0x00044490 / 0x0010B1F0, and background destruction
0x000430C0 (including 0x00043D50, 0x000ED2E0 and 0x00044620). The native sector
context, background factory type 4, and scene-owned navigation storage still
need installation after the complete constructor/destructor chain is ready.
No live scene-start or CTest acceptance is claimed for this unwired batch.

### 2026-10-12: background teardown and shared point-graph setup

Added saved shape-event flag restoration 0x00043D50, animation allocation release
0x00044620, the shared model/resource/sound object release 0x000ED2E0, and the
background destructor 0x000430C0. Teardown retains retail order, clears only the
fields cleared by the dump, and uses the first background's animation count/rows
for 0x00044620. Freed row pointers are intentionally not rewritten before the
row buffer itself is released. The auxiliary release invokes the existing sound,
resource registry and deferred-model release implementations.

Added shared point-graph storage setup 0x0010B160, comparator 0x001021A0, edge sort
0x00102200 and preparation 0x001024A0. Storage writes only the descriptor fields
written by the dump; canonical address binding is supplied by the native owner.
Sorting uses the real reconstructed CRT quicksort and preserves the comparator's
-1 / 0 / 1 behavior, active-edge prefix and once-only preparation guard.
Implementation batch landed; validation pending. The complete point/edge builder
0x0010B1F0 and animation initialization 0x00044490 still remain.

Validation for the eight-function teardown/point-graph batch: Release `gbtg`
and the scene factory harness build successfully. Seed 239 passed 400 focused
teardown cases; seed 241 passed 1,000 focused point-graph cases; seed 239 passed
3,400 cases across all 34 modes, all with zero mismatches. Coverage includes
empty/nonempty saved-event pools, repeated event targets, every optional release,
release order, first-background animation metadata, four list-link combinations,
clear-state restoration, inactive auxiliary sound reset, storage descriptor writes,
nonstandard comparator results, real CRT edge sorting and the preparation guard.
Auxiliary non-null resource/deferred-model branches use existing translations but
are not covered by these new fixtures. No live scene or CTest acceptance claimed.

Continuing into the complete shared point-graph builder 0x0010B1F0; its remaining
callees are now available. This is shared graphics behavior, not an actor-specific
implementation. Background animation initialization 0x00044490 follows it.

Added the full shared point-graph builder 0x0010B1F0 and background animation
initialization 0x00044490. The builder retains four seed points, adaptive spatial
bins, inclusive sample bounds, x87 averages/distances, point-count edge sampling,
real CRT ordering, both valency passes and three seed-edge indices. The animation
initializer keeps its first-background/model/header/table gates, the CPU +0x2C
bypass, point means, matrix copies and both inverses. Canonical address resolution
is supplied by the native owner, without replacing engine callees. Extended
arithmetic now includes square root for the builder's x87 distance calculation.
Implementation landed; validation of these two functions is pending.

Validation of the builder/initializer pair: Release `gbtg` and the scene factory
harness build successfully. Seed 251 passed 140 focused builder cases; seed 257
passed 560 focused initializer cases; seed 263 passed 3,600 cases across all 36
modes, all with zero mismatches. Builder checks compare the complete allocation
bytes, including unused capacity and untouched point bytes, in addition to the
descriptor. Fixtures cover coincident/collinear points, small extents, bin-boundary
points, all point-count sampling strides and seed exclusion. Initializer fixtures
cover all early gates, zero animation rows, bypassed graph construction, zero
point means, negative/zero/positive matrix counts, multiple rows, nonsingular and
singular matrices, both inverses and full graph allocations. The bypass flag is
at animation-block +0x2C (the block referenced by CPU +0x10), not outer CPU +0x2C.
No engine callee capture stubs were introduced; fixture hooks are heap boundaries
and observers. `git diff --check` passes. No CTests/live scene acceptance run for
this unwired dependency batch.

Current remaining constructor dependency: **door/cutscene registration**, previously
labelled effect registration. The 0x00043B40 directory-kind-0xF rows feed 0x0005B2D0;
its 0x0005B890 callee generates `aid_cutscene_ghoulies_doors_*` and matching camera
AIDs, while 0x0005B960 loads and prepares paired cutscene instances. This chain
must use shared cutscene/resource behavior, not an invented generic effect API.
Remaining untranslated entry points include 0x00043B40, 0x0005B2D0, 0x0005B890 and
0x0005B960; inspect their existing shared callees before implementing the chain.
After that, install the typed background constructor/factory type 4, native
sector/animation/release contexts and scene-owned navigation storage. Activation
is not considered live-complete until the actual scene-start navigation blocker
is removed through that ownership path and exercised at runtime.

### 2026-10-12: background door/cutscene registration

Recovered the complete read-only 0x001410F0 room-suffix helper, including its two
aliases and bit-mask case comparison. Added name construction 0x0005B890, paired
clip loading/setup 0x0005B960, door registration 0x0005B2D0 and background directory
walk 0x00043B40. These use the existing resource registry, cutscene loading/release,
actor updates, sound cue pause and model node-state implementations. Clip setup
preserves release/load order, optional distinct camera clips, three transform
updates, signed-byte actor iteration, pause-reference/voice behavior and model
visibility flags. Native instance-address binding maps the original pointer slots.

Moved the shared pose-matrix builder 0x00103CB0 out of actor_transform.cpp into
`graphics/pose_matrix.cpp`, exposing it to door registration. Preserved its inverse
branches and m[0]'s original SSE multiplication order; zero-subtraction sign
and translation-store order follow the recovered assembly. Door bounds use the actual 20 / 15 / 40 constants;
room aliases are `all` -> `centralhallway` and `whitebackground` -> `(storybook desk)`.
Implementation batch landed; build and differential validation pending. The typed
background factory/native context wiring remains to follow this shared chain.

Validation for the door batch: Release `gbtg` and the scene factory harness build
successfully. Seed 269 passed 1,200 focused mode-36 cases; seed 271 passed 3,700
cases across all 37 modes, all with zero mismatches. New differential coverage
checks the room helper's pointer identity/null/suffix results, case/punctuation
masking, both room aliases, shape 12 vs named shapes, camera-name generation,
existing/missing resource records and both scaled/unscaled pose inverse paths.
The pose check caught an incorrect intermediate edit to m[0]; tracing the six
pushed trigonometric arguments confirmed the original `(sy * sx) * sz` order,
which is retained. No engine stubs were added for these comparisons.

The complete 0x0005B960 clip-loading/actor/voice/model branch matrix and the
0x0005B2D0 / 0x00043B40 integration are translated and compile, but are **not yet
covered by the new differential fixtures**. Those require an owned cutscene
fixture/context, including resources and actor bindings. Do not treat these
helper results as live spawning acceptance. The next work is that clip-context
validation plus the typed 0x00042DA0 constructor and scene-owned background
factory/navigation installation. Other constructor callees (region append,
volume setup, shape collection and model-node setup) already have translations;
reuse them and the shared cutscene services rather than adding substitute APIs.
`git diff --check` passes; no CTests or live scene-start acceptance run this batch.

### 2026-10-12: typed background constructor and factory audit

Added the previously missing AID-list lookup 0x00035A40 and typed constructor
0x00042DA0. The constructor now calls the reconstructed bounds, matrix, region,
sector, animation-volume, graph/animation, door, ambient, shape-event and model-node
routines in dump order, followed by the framebuffer-clear AID-list decision and
matrix refresh. The list lookup uses the stored actor model name at +0x208;
constructor event collection uses mask 0, distinct from the bounds pass's -1.
Native allocation/address bindings retain the original pointer-slot stores.
Implementation landed; build and validation pending.

Factory audit recovered getter 0x00042BC0: background type 4 inherits actor type 2
and sets vtable 0x004E1644 to sizes 0x00042BA0, constructor 0x00042DA0,
destructor 0x000430C0 and message handler 0x00043E50. Inspect/translate that message
slot before claiming a live installed class; delegating every background message
straight to the base handler would omit retail behavior.

Added factory storage sizes 0x00042BA0, model link-head lookup 0x00040BE0,
shared list append 0x0002CBF0 and selective door registration 0x00044300.
The message audit also led to part-settings lookup 0x00044C20, part-rumble
selection 0x00046470, volume navigation preparation 0x00045EC0 and shared
navigation region-bit writes 0x00036A00. The region helper keeps only bit 12's
assignment and matches the full requested tag against the cell's low 12 bits.
The rumble lookup retains the byte-indexed part name and its 0x22C settings stride.

The recovered 0x00043E50 activation branch (0x23) does additional render-bin,
model-list, shape-probe and bind-tree cleanup after the base handler. Its other
branches reach 0x000441A0 -> 0x000441F0, which still depends on untranslated
background-part destruction/dispatch functions 0x00044EA0, 0x00045CB0,
0x00045170 and 0x00044E70. This is a newly confirmed factory installation blocker;
do not replace those returns or silently route those cases to the base handler.
The constructor batch builds; the AID-list lookup passed seed 277, 800 focused
retail comparisons with zero mismatches. Remaining added helpers need validation.

Validation for this ten-function constructor/factory-dependency batch: Release
`gbtg` and scene factory harness builds pass. Seed 277 passed 800 focused AID-list
cases; seed 281 passed 1,200 focused mode-38 cases; seed 283 passed 3,900 cases
across all 39 modes, all with zero mismatches. Mode 38 covers factory sizes,
nullable model-link lookup, empty/nonempty tail-list insertion, signed grid counts,
full requested region tags, preservation of all other region bits, part-setting
first-match/masking and rumble pointer selection/default behavior. No engine callee
capture stubs added. `git diff --check` passes.

Coverage limits: the typed 0x00042DA0 constructor, selective 0x00044300 registration
and volume-navigation 0x00045EC0 call graph compile but are not covered end-to-end
by these helper comparisons. Door clip loading still needs its owned fixture/context.
Neither background type 4 nor live scene navigation is installed yet, and no live
scene-start/CTest acceptance is claimed.

Recovered complete remaining frontier dumps: 0x00044EA0 (active volume row lookup),
0x00045CB0 (part destruction eligibility), 0x00044E70 (part settings +0x220 lookup)
and 0x00045170 (full part destruction, ending at 0x00045A87). The three small lookups
are next; the last routine calls debris/model setup 0x000ED1C0 / 0x000ED990 /
0x000ED6E0 / 0x000EFE30, matrix transforms 0x00040640, part helpers 0x00044EF0 /
0x00044A90 / 0x000450A0 and attachment/script calls. Audit existing translations
before implementing each missing callee. 0x000441A0 -> 0x000441F0 -> 0x00045170
must be completed to translate the background's full message slot 0x00043E50;
then install the lifecycle with scene-owned cutscene, background and navigation
contexts. The exported Ghidra project remains unchanged.

### 2026-10-13: active parts and shared effect-particle setup

Added active volume lookup 0x00044EA0, destruction eligibility 0x00045CB0 and
settings +0x220 lookup 0x00044E70. Eligibility retains the first matching active
row, byte-0xFF metadata gate, ordered zero-time test and signed stage comparison.
Added shared debris reset 0x000ED6E0 and named interpreter wrapper 0x000ED990,
using the existing registry, deferred-model release and effect interpreter.

Added effect-particle initialization 0x000EF8C0, frame setup 0x000EFB20 and free-row
allocation 0x000EFE30. The allocator keeps the signed free-chain slot, original
+0x148 owner / +0x14C user words, active count wrap, partial initialization,
conditional frame pass and exact RNG advancement. Frame setup retains PAL rate
selection, unsigned count conversion, position/speed clamps and mode-bit priority.
These are shared graphics/effect routines, not background-specific substitutes.
Implementation batch landed; build and differential validation pending.

Added shared actor model-matrix sampling 0x00040640 after confirming all its
callees already exist: pose matrices, affine multiplication, array offsets,
axis scales, Euler extraction and actor world position. The sampler keeps its
cached-row fast path, alternate-pose precedence, global scratch selection,
column-major-to-affine conversion, source SSE coordinate order and optional
height clamp. This closes another direct 0x00045170 dependency; validation pending.

Validation: Release `gbtg` and the scene factory harness build successfully.
Seed 293 passed 1,600 focused effect/part cases; seed 297 passed 800 door/pose
cases; seed 301 passed 800 matrix-sampling cases; seed 307 passed 4,100 cases
across all 41 modes, with zero mismatches. Effect fixtures run real resource
acquisition/release and interpretation. Matrix fixtures cover cached rows,
unbound joints, local transforms, scratch state and height clamps; pose selection
and general rotated matrices remain to be exercised.

Validation exposed and corrected two older translations: 0x0005B890 queries the
clip name after generating both names (the final stack reload selects the first
output), and 0x00103A80 calls the existing retail square-root helper rather than
`std::sqrt`. The door fixture now initializes resource hashes and includes both
clip and camera records. The effect oracle corrects Unicorn's two-quiet-NaN
MULSS selection at 0x000EFB06, verified against the identical instruction on the
native CPU; no engine callee was replaced to obtain the passing comparisons.

Next frontier exports recovered 0x00044D20 (part sound/effect dispatch),
0x001117E0 (region-bit broadcast) and 0x0012C8A0 (physical shape-event broadcast).
The graph impulse helpers 0x0010CFE0 and 0x0010C8D0 are substantial routines and
still need complete translation. Background factory/live navigation remain
uninstalled; these helper results do not claim live scene acceptance.

Added 0x001117E0 region broadcast, 0x0012C8A0 physical shape-event broadcast,
0x00044D20 part sound/effect dispatch and 0x000450A0 stage advancement. The stage
helper keeps signed stage comparisons, both model node-state writes, the 2.5f
timer and the literal `unknown` fallback. Seed 311 passed 800 focused region and
stage cases, including nullable primary/secondary models and preserving unrelated
flags/shared state. Dispatch coverage currently reaches settings matching and
slot rotation with empty sound/effect names; queued sound and effect branches
still need end-to-end fixtures.

Added the real event-bit callbacks 0x0012C930 / 0x0012C950 and background volume
removal 0x00044EF0. Removal retains attachment/anchor release order, optional
volume-derived midpoint, region unblock, pulse 0xAB, scene event-bit clearing and
attachment callout 0xD through existing reconstructed callees. Build and the new
physical event fixtures are pending; volume removal is not yet covered end-to-end.

The expanded batch builds successfully. Seed 313 passed 1,000 focused physical
event cases; seed 317 passed 4,300 cases across all 43 modes, with zero mismatches.
The physical-event cases execute real 0x0012C8A0 -> 0x00055A30 -> 0x00054DB0 ->
0x000555C0 -> 0x0012C930 / 0x0012C950, covering nullable object/resource/shape
links, signed shape counts, marker gating, category filters and exact bit writes.
No CTests or live scene-start acceptance is claimed. The typed removal call graph
compiles but still needs its attachment/anchor/callout fixture.

Remaining immediate destruction frontier: 0x00044A90 requires graph collision
projection 0x0010CFE0 and graph transform recovery 0x0010C8D0. Their complete
exports end at 0x0010D3A6 / 0x0010CFDB. Additional untranslated graph callees are
0x00102500, 0x00102030, 0x0010C340 and 0x0010BEB0; sphere/triangle intersection
0x0015A440 and matrix inverse 0x00151FAD already exist. Translate those graph
helpers before 0x00044A90 and the full 0x00045170 dispatcher, then finish
0x000441F0 / 0x000441A0 / 0x00043E50 and the background class installation.

## Point graph solver and background graph push (2026-10-13)

Added `graphics/point_graph_solver` with the graph step 0x00102500, edge solvers
0x00101C80 / 0x00101DD0 / 0x00101F10, relaxation passes 0x00102030 / 0x00102280,
sphere record lookup 0x00052040, column store 0x00153A00, rotation between
vectors 0x0010BEB0, contact crossing 0x0010C100, edge projection 0x0010C340, the
collision step 0x0010CFE0 and transform recovery 0x0010C8D0. `graphics/d3dx_matrix`
adds the D3DX helpers 0x001850CA (multiply), 0x00184B7B (translation) and
0x0018462D (normal transform). `push_background_graph` 0x00044A90 in
`scene/background_animation` composes them over the block's grid.

`tools/oracle/point_graph_oracle.py` runs retail code in Unicorn over every XBE
section and compares the whole test image against `point_graph_harness` (built by
`cache/oracle/build_point_graph.cmd`). Seed 401 passed 500 cases and seed 409
passed 5,000 cases across modes 0-9. Seed 413 passed 2,200 cases of the collision
step (mode 10), whose random 2x2x2 grid sets the hit flag +0x1D6 in about 23% of
cases. 0x00044A90 itself is not yet covered by a fixture.

`--actor-loading` had started failing after 0x00103A80 moved to the retail
square root 0x00152B30. That change is correct: retail returns 0x3FFFFFF8 for
the root of 4, so the test now expects those bits. The `--scene-start` stop is
unchanged: the camera mind 0x7D handler needs nav buffer DAT_0050E24C, which
waits on the background class install.

## Background part destruction and message slot (2026-10-13)

Added `scene/background_part_destruction` with the breakable-part dispatcher
0x00045170. It has two branches:

- **Without graphs (+0x950 null):** creates the debris emitter
  `aid_fxemitter_ghoulies_background_bits_explode` on first use (0x000ED1C0 ->
  0x000ED990 -> 0x000ED6E0). It then allocates one particle per sample and
  writes the sampled pose plus six RNG draws at emitter +0x390 + n*0x150.
- **With graphs:** pushes each listed graph (0x00044A90) away from the nearest
  sphere record, or uses a random impulse when there is none, and clears graph
  +0x1D0 / sets +0x1D5. It then releases the attachment and anchor of the row
  indexed by the part (retail uses the part, not the new row).

Both branches then remove the volume (0x00044EF0) and advance the stage (0x000450A0).
Rows are the removal volumes; 0x00044EA0 / 0x00045CB0 / 0x000450A0 now take
`BackgroundAnimationVolume` rows (the factory oracle harness copies bytes in and
out; seed default passed 3,000 cases with 0 mismatches after the change).
0x000450A0 raises if a row of -1 reaches its stage write.

Added `scene/background_messages`: the hit loop 0x000441A0, single-hit break
0x000441F0 (part table caption through `backgroundparttable`, points from
0x00044E70) and message slot 0x00043E50 (cases 8/0x15/0x19/0x1D hit lists with
rumble reply 0x50, 0x23 activation cleanup, 0x66 volume navigation, 0x67 door
selection, 0x8B ignored, others to the base handler). `frontend/score_awards`
adds 0x0012DE90 (category multipliers DAT_003130B4, ten captions at DAT_00432724).

Background vtable 0x004E1644 (from the stores at 0x00042BCA..): sizes 0x00042BA0,
constructor 0x00042DA0, destructor 0x000430C0, message 0x00043E50, update
0x000464D0 (-> 0x00045A90 part/graph frame update, exported, untranslated) and
draw 0x00042C10. Next: install type 4 through a native class owner (constructor
plus message slot) so scene start's 0x0011C8B0 creates the background and
0x00111820 can build DAT_0050E24C. None of 0x00045170 / 0x00043E50 is oracle-covered yet.

## Background class install (2026-10-13)

Background type 4 is now installed by `scene/background_class_owner`
(`NativeBackgroundClassOwner`). It provides sizes 0x00042BA0, the constructor
0x00042DA0, the destructor 0x000430C0 and the message slot 0x00043E50 over the
type-2 actor base. `SceneActorServices::start_background()` runs the 0x0011CBD0
subset before the markers:

1. the door pool DAT_004EE2B8 (stride 0x154, growth 4);
2. the requested background (0x0011C8B0);
3. activation (0x00123070);
4. the navigation build (0x00111820) into `navigation_nodes` (DAT_0050E24C).

`--scene-start` calls it.

Supporting changes:

- **Loaders.** Translated the part-table loader 0x00044CA0 (DAT_00546294/98/9C)
  and the sector table loader 0x000FEF60 (DAT_0050D798/9C, rows of 0x8C). The
  legacy `light_bind_table` in lighting.cpp still leaves its own table null.
- **Model views.** `SceneDialogueResourceOwner` now publishes each actor model's
  shape event lists and collision vertices (shape list +0xC). The background
  views resolve model words below 0x10000000 as offsets in the actor model,
  scene objects from 0x10000000, and relocated resources from 0x40000000.
- **Effect node.** 0x000ED2E0 now releases the `ActorEffectNode` that 0x00045170
  creates (renamed `release_actor_effect_node`). The lifetime's +0x940 is that
  node, and the factory oracle's mode 33 builds a real node (3,000 cases, 0
  mismatches).
- **Doors.** `RuntimeDoorContext.cutscenes` is now a pointer and raises at use
  while unbound.

Current `--scene-start` stop: the opening hallway model has doors, so 0x00043B40
-> 0x0005B2D0 needs the scene cutscene runtime (`CutsceneRuntimeContext`) for
the door clip names. No scene installs it yet (`install_scene_cutscene_runtime`
has no caller).

### Cutscene runtime for door clips (2026-10-13, in progress)

The scene services now own a `CutsceneRuntimeContext` built from scene state: audio
banks, the model renderer, fog, the camera impulse and event asset, movie slots, the
replacement buffers DAT_004E1AC0/DAT_004E1A40, and the footstep kinds
PTR_DAT_00423620 (one entry, "BOY").

Supporting changes:

- `SceneDialogueResourceOwner` publishes `aid_cutscene_` track and camera views
  as its bundle provider loads them.
- `RuntimeDoorContext.models` is now a lookup function over the live actor
  views, since door track actors are created during the clip load.
- Cutscene instances map to heap token addresses.
- `object_of` also searches the cutscene track actors.

Current `--scene-start` stop: during 0x00043B40 -> 0x0005B960, a door clip's
track actor is null. Either `create_cutscene_track_actor` (0x0004DEC0 ->
`create_cutscene_actor`, cloning `aid_objparams_ghoulies_misc_actor`) returned
no object, or the clone or factory construction failed. Retail reads that
actor's +0x204 with no null check, so the clone or the type-2 construction is
the next thing to check.

### Cutscene track clone correction (2026-10-13)

Reviewed the newer graph, destruction, background-class and cutscene ownership
changes before continuing. The door track failure comes from the translated
0x0004DEC0 template literal: instruction 0x0004DF7A loads DAT_002FE6C0, which
contains `aid_objparams_ghoulies_misc_prop`. The translation used
`aid_objparams_ghoulies_misc_actor`, a name absent from both executable objparams
pools, so the real clone lookup returned null. Corrected the literal to the
retail prop template; no null fallback or substitute actor was added.
Build and scene-start verification are pending.

The corrected clone reaches the real template's type 0x1A, whose getter
0x0006EE20 was not installed. Added its contact-prop sizes, constructor,
destructor, message and frame callbacks (0x0006EE00 / 0x0006EE70 /
0x0006EEA0 / 0x0006EF30 / 0x0006EED0). The shared contact scan 0x0013EEB0
uses existing shape sampling, contact probes, copied hit records and body-contact
override insertion/restoration. Added shared expiration 0x0013EB60 and removal
notification 0x0013F110; they retain swap-with-last removal and NaN comparison
behavior. Installed the lifecycle through the actor owner and routed type 0x1A
messages and cutscene updates through its real callbacks. Validation pending.

The first integrated run now reaches the background constructor's AID-list query,
past door track creation. Its access violation exposed a native interface mistake:
0x00035A40 receives a pointer wrapper on the retail stack, and dereferences it to
the list's first name. The acquired resource already is that list; the helper
mistook its initial `aid_` bytes for an offset. Corrected the typed span interface
and its fixture, and synchronized the actor owner's stored model name (+0x208)
from the existing `RuntimeActorModelState::name`. No resource contents were changed.

Scene-start now builds the opening hallway background and navigation, creates
and activates 43 marker actors with zero pending actors, and delivers notice
0x88. The next stop was notice 0x7D: 0x00136700 allocated its real mind search
storage, but the scene address adapter did not recognize that owned heap block.
Added the native mapping for `NativePlayerInstance::mind_storage`; verification
of the remaining startup notices is pending.

Opening `--scene-start` now passes end-to-end through startup and teardown:
background/navigation built, 43 actors activated, zero pending actors, notices
0x88 / 0x7D delivered, contact round-trip/event deduplication passed, and final
factory/address counts both zero. Added the retail door cleanup 0x0005B1E0,
corrected cutscene actor release 0x0004E9E0 to test the link head at +0x918
(rather than the native embedded-link view's address), refreshed moved track
object ownership mappings, and released remaining scene-owned track objects.
This closes the previous live startup blocker; it does not accept untranslated
background per-frame update 0x00045A90 / 0x000464D0 or full gameplay.
Focused contact-helper and corrected AID-list comparisons are in progress.

Validation complete for this batch: Release `gbtg` and `gbtg-transition-tests`
builds pass. Seed 431 passed 1,200 focused contact expiration/removal cases;
seed 433 passed 800 corrected AID-list cases; seed 439 passed 4,400 cases across
all 44 factory-helper modes, with zero mismatches. Contact comparisons cover
null/empty pools, duplicate actor matches, restoration of saved filters,
swap-with-last bytes, unsigned masks and ordered/unordered age comparisons.
The opening `--scene-start` integration test passes with final factory=0,
addresses=0 and no live latch nodes. The new prop contact scan/frame callback
is compiled and connected but has no complete retail differential fixture yet;
startup exercises its lifecycle and messages, not its collision-update branch.

Next milestone: translate/validate background frame update 0x00045A90 and wrapper
0x000464D0, then install the actual background update slot. Keep the shared
effect/graph callees general. Opening startup acceptance does not imply that
the currently untranslated background tick or the full playable frame is done.

`--actor-loading` also passes. Updated its track-creation fixture to the actual
DAT_002FE6C0 prop name, and its release cases now distinguish a nonnull native
link view with a zero retail head from an actor with a real parent head. These
catch the two corrected cutscene translation errors rather than preserving the
old fixture assumptions. `git diff --check` passes. No CTests were run.

Background tick dependency batch: translated shared quaternion storage 0x00170250,
quaternion matrix 0x00184D48, matrix composition 0x00153BE0, Euler wrapping
0x001059B0, effect emission timing 0x000EF1D0 and graph matrix publication
0x00044430. Added focused retail fixtures for the math and emission helpers.
Validation pending. The complete effect tick 0x000EF2D0 still requires binding,
particle placement, simulation and notification callees; background update remains
unbound until those translations are available. No missing-callee substitute was added.

Release gbtg build passed for the dependency batch. Seed 443 passed 1,000
effect-emission cases, including sentinels, signed capacities and unordered timers.
Seed 449 passed 1,000 quaternion storage/rotation/composition and Euler-wrap
cases. Graph matrix publication is compiled but still lacks a differential fixture.
Added shared line and box placement 0x00105170 / 0x001051E0; the latter retains
the seven-way face selection and in-place alias behavior. Validation pending.

Validation complete: Release gbtg build passes. Seed 457 passed 1,200 shared
line/box placement cases, including aliased source/output and face selection.
Seed 461 passed 600 graph-publication cases, including negative/zero counts,
inactive descriptors, matrix offsets and dirty bytes. Seed 463 passed 4,800
cases across all 48 factory-helper modes with zero mismatches. git diff --check
passes. No CTests were run; startup behavior was not changed by these helpers.

Remaining background-tick effect frontier is now exported read-only from Ghidra:
0x000EEB60 binding preparation requires model attachment sampling 0x000EC8E0 /
0x000EC9D0 and size selection 0x000EEA00; particle placement 0x000EED50 requires
the remaining curved-volume samplers and 0x000EE5C0 velocity initialization.
Particle simulation 0x000F18E0 reaches bounded channels 0x00101980 / 0x00101A40 /
0x001062E0, oscillation 0x00106A80, approach channels 0x000F1F60, collision
0x000ECE50 and command dispatch 0x000F0FF0 / 0x000F0390. Notification 0x000EE560
and particle iteration 0x000EF0B0 are also outstanding. Continue those shared
callees before implementing 0x000EF2D0 and installing 0x00045A90 / 0x000464D0.
The 48-mode acceptance covers helpers, not complete background frames.

Particle simulation dependency batch: translated shared decreasing/increasing
bounded channels 0x00101980 / 0x00101A40, three-channel dispatch 0x001062E0,
and five-channel effect approach 0x000F1F60. Preserved strict boundary tests,
flag precedence, wrap loops, bounce flag changes and freeze/time scaling.
Added focused retail fixtures; build and validation pending.

Release gbtg build passed for the channel batch. Seeds 467 and 479 each passed
1,400 bounded-channel and target-approach cases with zero mismatches. Continued
with shared angle-range 0x00104FC0, rectangle 0x00105550 and ellipse 0x00105480
placement; preserved wrapped-angle intervals, ordered/unordered comparisons,
random draw counts, biased edge selection and aliased source/output ordering.
Added focused fixtures; this placement batch awaits build and comparisons.

Angle/rectangle/ellipse comparisons passed: seed 487, 1,000 angle-range cases;
seed 491, 1,200 rectangle/ellipse cases, including aliased extents. Continued
with ellipsoid 0x001053B0 and cylinder 0x001056B0 sampling using the existing
real direction helper 0x00106220. All six placement samplers reached by
0x000EED50 now have translations; the last two await focused validation.

Ellipsoid/cylinder validation passed: seed 499, 1,200 cases with zero mismatches,
including in-place extents, interior/surface draw counts and wrapped angle ranges.
The channel and volume-sampling batch now has nine new reconstructed functions.
Full 53-mode factory-helper comparison is running. The particle placement caller
0x000EED50 still needs jitter 0x000EEA70 and velocity initialization 0x000EE5C0;
it is not yet accepted merely because its volume samplers pass. Remaining
particle simulation still needs oscillation, collision and command dispatch.

Validation complete for this batch: Release gbtg builds pass; seed 503 passed
5,300 cases across all 53 factory-helper modes with zero mismatches. Focused
channel and placement checks passed another 6,200 cases. git diff --check passes.
No CTests were run. These helpers are general and are not attached to an
individual actor class. Background tick remains unbound pending full effect
binding, placement/velocity, oscillation, collision, commands and notifications.

Next concrete work: translate jitter 0x000EEA70 and velocity 0x000EE5C0 to close
placement 0x000EED50; export missing attachment validation 0x00040930 and size
sampling 0x000EC830 before effect binding 0x000EEB60. Existing model joint and
locator transforms cover 0x0003AEB0 / 0x0003B050 / 0x00051CE0 / 0x00051E40 /
0x00104040, so reuse those implementations rather than adding boundaries.

Particle placement batch: translated jitter 0x000EEA70, velocity initialization
0x000EE5C0 and full placement 0x000EED50. Retained mode-specific extra speed
draws, random/local/binding rotations, owner scale for volume sampling, effect
scale for velocity, inherited motion and copied orientation. The placement
interface carries explicit incoming scratch for an unsupported volume kind,
matching the retail local rather than assigning a fabricated zero. Added
focused retail fixtures; build and validation pending.

Jitter/velocity/placement validation passed: seeds 509 / 521 / 523 passed
1,200 / 1,400 / 1,800 focused cases with zero mismatches. Release build passes.
Continued attachment work: translated locator iteration 0x00051C60, actor
locator iteration 0x00040930, physical shape-block lookup 0x000424C0 and cyclic
effect shape selection 0x000EC750. Retained first matching table entry, cursor
advance and wrap order, shifted kind mask and excluded joint -1. Added focused
fixtures; the attachment selection batch awaits validation.

Attachment selection validation passed: seed 541, 2,000 cases, zero mismatches.
Coverage includes absent model/header, empty tables, first matching table entry,
null locator blocks, cursor advance/end behavior, cyclic shape scans, masked
kind shifts, excluded joint -1 and zero-mask early return. Full 57-mode
factory-helper comparison is running. The remaining binding wrappers are
0x000EC830 (shape center/radius), 0x000EC8E0 (locator position/euler),
0x000EC9D0 (joint position/euler), 0x000EEA00 (size mode) and 0x000EEB60
(binding preparation and inherited velocity). Their actual selection callees
now exist; connect native cached-pointer/address views while translating them.

Batch validation complete: Release gbtg build passes. Seed 547 passed 5,700
cases across all 57 factory-helper modes with zero mismatches, alongside 6,400
focused jitter/velocity/placement and attachment-selection cases. git diff
--check passes. No CTests were run. Placement is accepted for all defined volume
kinds 0..6 and velocity modes, including default-mode preservation; unsupported
volume kinds depend on incoming retail stack scratch and are not fixture-covered.
The full background frame remains incomplete until attachment wrappers and the
remaining effect simulation/command/notification routines are translated.

Effect binding batch: translated shape sampling 0x000EC830, locator sampling
0x000EC8E0, joint sampling 0x000EC9D0, size update 0x000EEA00 and binding
preparation 0x000EEB60. Reused existing shape/bone/world transforms and added
native cached shape/locator views with explicit canonical-address mirroring.
Preserved null/missing attachment fallbacks, retained unwritten radius/euler,
size modes, local +0x5C rotation for unbound effects and inherited motion.
Added full retail fixture setup for primary/alternate pose, joint translations,
locator iteration and shape sampling; build and validation pending.

Binding validation passed: seed 557, 1,800 cases across all five binding routines,
zero mismatches. Release build passes. Fixture covers null actors/models, both
pose buffers, successful shape/locator/joint paths, cached selection, failed
queries preserving radius/euler, size modes and inherited-motion branches.
Continued with shared oscillating channels 0x00106A80 and fallback-command
selection 0x000F0FF0. Preserved float spills around phase wrapping and previous
marker chaining; focused fixtures added, validation pending.

Focused simulation-helper checks passed: seed 563, 1,400 oscillation cases;
seed 569, 1,200 fallback-command selection cases, zero mismatches. The latter
checks empty streams, varying record strides and repeated fallback markers
including the retail previous-marker cache behavior. Full 60-mode comparisons
and transition-test rebuild are running. Opening scene startup will be checked
after the effect ownership storage change.

Batch validation complete: seed 571 passed 6,000 cases across all 60 helper
modes with zero mismatches, alongside 4,400 focused binding/oscillation/fallback
cases. Release gbtg and gbtg-transition-tests builds pass. Opening --scene-start
and --actor-loading both pass: 43 activated, zero pending, notices 0x88/0x7D
delivered and final factory=0 / addresses=0. git diff --check passes. No CTests.

Binding preparation and particle placement are now translated and validated.
The full effect tick remains unconnected: next implement collision response
0x000ECE50 with probe 0x000ECBD0 / 0x000ECA20, response 0x000ECC30, actor
probe 0x0013C0B0 and pitch 0x00106000; implement command execution 0x000F0390
and its helpers 0x00105140 / 0x000F0300 / 0x000F2430 / 0x000F23E0 /
0x000F22F0 / 0x000F1AF0 / 0x000F1D20; implement notification 0x000EE560
with actual sound request 0x00039580 and effect event 0x0013E610 callees.
New frontier dumps were exported read-only from the existing Ghidra project.
Then complete particle simulation 0x000F18E0, emission 0x000EF260, iteration
0x000EF0B0 and tick 0x000EF2D0 before installing background update 0x00045A90 /
0x000464D0. No untranslated callee was replaced with a stub.

Collision/command dependency batch: translated reflection 0x000ECC30, pitch
0x00106000, explicit random-state sampling 0x00105140, active-channel check
0x000F2430, command conditions 0x000F0300, color/alpha targets 0x000F22F0 /
0x000F23E0 and oscillation setup 0x000F1AF0. Preserved angular random draws,
strict/unordered comparisons, duration epsilon, x87 arithmetic spills and
reflection/spin response. Added focused retail fixtures; build/validation pending.
The collision probe 0x000ECA20 is still being audited against its actual world
query callees, and full collision/command dispatch remains untranslated.

Added real world/grid probe 0x000ECA20 and particle probe gate 0x000ECBD0 using
existing sweep/movement and navigation routines. Seed 599 passed 1,400 grid
hit/miss, empty-world sweep/movement and particle flag cases. Registered-world
geometry hits are still not fixture-covered. Reflection seed 577 and primitive
seed 587 each passed 1,600 cases. Target/setup checks exposed finite/infinity
division in shared ExtendedFloat: compensation produced NaN where retail yields
zero. Confirmed all observed target mismatches have infinite duration, and
corrected nonfinite division handling in the shared arithmetic helper. Rebuild
and affected/full comparisons pending.

Corrected target/setup comparisons now pass: seed 593, 1,600 cases including
infinite and unordered durations, zero mismatches. Corrected collision fixture
origins to verified DAT_0050E264 / DAT_0050E26C; seed 599 again passed 1,400
probe cases. Full 64-mode helper comparisons and point-graph regression are
running after the shared ExtendedFloat nonfinite-division fix.

Batch validation complete: Release gbtg build passes. Seed 601 passed 6,400
cases across all 64 factory-helper modes with zero mismatches; focused reflection,
primitive, target/setup and probe checks passed 6,200 cases. Seed 607 passed
2,200 point-graph/matrix regression cases across all 11 modes after the shared
ExtendedFloat fix. The point-graph oracle now supplies the build/Release DLL
search path, matching the factory oracle, so its linked harness starts reliably.
git diff --check passes. No CTests were run.

Ten additional routines are translated in this batch. World/grid collision
probe and reflection now exist, but registered-world geometry hits still lack
a dedicated differential fixture. Next complete camera-plane probe 0x0013C0B0
and collision caller 0x000ECE50; bounded-channel setup 0x000F1D20 and its
0x00101430 / 0x001063D0 / 0x00106590 callees; then full command execution
0x000F0390. Notifications 0x000EE560 and their real sound/effect event callees
also remain. Particle simulation, effect tick and background update stay
unconnected until all required callees are translated.

Camera/bounded-channel batch: translated wrapped-angle equality 0x00101430,
channel limits 0x001063D0, channel rates 0x00106590, particle bounded setup
0x000F1D20, camera-plane clipping 0x0013C0B0 and particle collision/movement
0x000ECE50. Reused actual probe, reflection, angle and frame-clock helpers.
Preserved camera projection order, optional outputs, retained row padding, PAL
rate caps, callback-before-response timing and unordered drag clamping from
assembly. Differential fixtures are being added; build and validation pending.

Focused checks pass for this batch: seed 613, 1,400 camera-plane cases;
seed 617, 1,800 equality/limits/rates/bounded-setup cases; seed 619, 1,800
full particle movement cases. Zero mismatches. Movement checks include camera
and grid contacts, empty-world probes, supplied callback changing reflection
inputs, freeze/time scaling, stopped particles, spatial bounds, pitch/yaw
alignment, rotation wrapping, gravity and ordered/unordered drag behavior.
Release gbtg build passes. Full 67-mode comparison is pending; no CTests.

Batch validation complete: seed 631 passed 6,700 cases across all 67 helper
modes with zero mismatches. Strengthened movement fixtures with deliberate
contacts: seed 641 passed another 2,200 cases, exercising 468 camera hits,
426 grid hits, 448 supplied callbacks and 345 initially stopped particles.
Callbacks modify the reflection inputs, so the fixture verifies real call
ordering as well as invocation. Release gbtg build and git diff --check pass.
No CTests were run. Registered-world geometry hits remain outside the new
movement fixture, which covers camera/grid hits and actual empty-world probes.

Camera clipping, bounded-channel setup and particle movement/collision caller
are now translated and validated. Next translate full particle command dispatch
0x000F0390: all its direct callees now have translations, including existing
0x000191C0 random-range and 0x001673D8 integer conversion. Then translate
notification 0x000EE560 with sound request 0x00039580 and effect event
0x0013E610, followed by 0x000F18E0 / 0x000EF260 / 0x000EF0B0 / 0x000EF2D0.
Effect simulation and the background update slot remain unconnected until
those complete callers are available. No missing callee was stubbed.

Particle command dispatcher 0x000F0390 is translated into a narrow shared
effect_commands module. Preserved fixed per-opcode strides, terminating and
waiting commands, cursor rewind, detach count wrapping, cached random durations,
color integer conversion, signed random angular ranges and nested channel setup.
All direct callees use existing translations. Added opcode/chain differential
fixtures; build and validation pending.

### Scene start passes through the background (2026-10-13)

The door clip's null track actor was a translation error in 0x0004DEC0. Retail
clones `aid_objparams_ghoulies_misc_prop` (DAT_002FE6C0), but the translation
named a nonexistent `..._misc_actor`, so the clone failed. With that fixed,
`--scene-start` passes end to end:

1. the background is created and activated;
2. navigation is built into DAT_0050E24C;
3. markers run and 43 actors activate;
4. 0x88 and the 0x7D sweep are delivered, including the camera mind's 0x7D
   (0x00136700) that previously stopped on the missing nav buffer;
5. teardown leaves factory=0, addresses=0.

All suites, ctest 6/6 and both headless runs pass.

Next, from the 0x0011CBD0 order: the remaining scene-start steps (0x001153A0,
0x00123560, 0x00119990, 0x0011F450 and the post-marker calls), the frame pass
0x0011E640/0x00123270 with the background update slot 0x000464D0 -> 0x00045A90,
wiring `start_background` into the live game's scene start, then the 0x6F
player wait and the tutorial dialogue.

Particle dispatcher validation: seed 643 passed 1,800 opcode/chain cases with
zero mismatches after correcting shared ExtendedFloat division to preserve a
signed-zero numerator. The six initial mismatches were only the size-rate sign
bit; retail stores negative zero. Point-graph regression seed 647 passed 2,200
cases across all 11 modes. Full 68-mode regression is running.

Translated notification 0x000EE560 and its actual sound/effect callees
0x00039580 / 0x0013E610. Positioned sound uses the real allocation, source
position and cue preparation functions. Effect events use the real stream
loader and preserve last-effect clearing/flagging. Notification retains unsigned
frame wraparound, the five-frame cooldown, hit/miss sound selection and aid_
prefix check. These callers are not yet wired into particle simulation.

The remaining collision callback is 0x000F18C0 -> 0x000F10F0. Its complete dump
is now available; 0x000F1040 and 0x000EC580 still need translation before the
callback and 0x000F18E0 simulation caller can be completed. Footstep naming
0x0013DE80 and camera effect 0x000FB120 already exist. No missing callee has
been replaced with a fabricated callback result.

Full helper regression seed 647 passed 6,800 cases across all 68 modes with
zero mismatches; the notification batch also builds successfully in Release.
Translated shared debris oldest-slot selection 0x000EBD80 (unsigned timestamps,
last slot wins ties), free-chain allocation 0x000EBDB0, phase setup 0x000EBE50,
collision debris spawn 0x000EC580, and collision pose sampling 0x000F1040.
Preserved slot eviction, initialization offsets, random draw ordering, SSE versus
x87 division, color/alpha setup and yaw/pitch output destinations. Added focused
pose/phase fixtures; validation pending. The collision interpreter itself and
full simulation/background update remain unfinished.

Pose sampling seed 653 passed 1,200 cases; debris phase setup seed 659 passed
1,400 cases, both with zero mismatches. Release build passes. Debris allocation,
eviction and spawn currently have source/assembly audit but no dedicated pool
fixture. Added complete 0x000F10F0 collision-command interpreter and actual
0x000F18C0 callback: all ten opcodes, per-opcode RNG/gate order, spawn/sound/
debris/camera side effects, counter wrapping and callback result. Build and
interpreter differential checks are pending; not yet wired into full simulation.

Collision interpreter/callback focused comparison seed 661 passed 2,200 cases
with zero mismatches. Covers all opcode strides, range/chance rejection, RNG
order, collision masks, reflection/scale mutation, unconditional eligible masks,
and signed counter wrap. Resource-producing branches are source/assembly audited
but not exercised by this fixture. Added full 0x000F18E0 particle simulation
using the actual collision callback, preserving expiry output behavior, separate
caller/global time steps, channel/size updates, fallback selection and command
dispatch. Added stopped-particle simulation fixtures; build/checks pending.

This batch's final validation: Release gbtg build passes. Particle simulation
seed 673 passed 1,800 stopped-particle cases (expiry leaves outputs untouched,
frame/size/color/oscillation/bounded updates, separate caller/global step,
fallback selection and dispatch). Seed 677 passed 7,200 cases across all 72
then-existing modes with zero mismatches. Added independent pool fixtures:
seed 683 passed 1,400 oldest-slot cases, including unsigned timestamp extremes
and ties; seed 691 passed 1,400 allocation cases, including negative free heads,
free-chain updates and exact initialized/retained bytes. Zero mismatches.
Point graph's 2,200-case regression also passed after the signed-zero fix.
No CTests were run in this batch. git diff --check passes.

Next frontier: 0x000EF260 spawn-and-simulate, 0x000EF0B0 active-particle pass,
then 0x000EF2D0 effect tick and the background update binding. Preserve the
active-pass assembly's local scratch behavior: 0x000F18E0 leaves the previous-
collision output untouched on a hit; 0x000EF0B0 compares that scratch's raw
word to 1, then makes a random hit/miss sound choice. Do not invent an initial
scratch value or replace the raw comparison with float equality. Placement's
unsupported-volume scratch likewise remains explicit. The full simulation's
collision-hit integration, resource-producing collision branches and combined
debris spawn/notification callers still need dedicated fixtures; lower movement,
callback, pose, phase, allocation and eviction helpers have independent checks.
Full gameplay/background ticking remains unconnected.

Translated 0x000EF260 spawn-and-simulate, 0x000EF0B0 active-particle pass and
0x000EF2D0 complete effect tick into effect_tick. All direct callees use real
translations. Preserved signed capacity, 16-bit loop index, wrapping count/free
chain, raw previous-collision scratch comparison, half-probability notification,
camera direction alignment, first-tick gate, per-emission substeps, stationary/
moving interpolation, restore/copy order and effect lifetime return condition.
Incoming scratch is explicit; no fabricated callback result. Added spawn,
active-particle and effect-tick differential fixtures; validation pending.

Focused checks pass for this batch: seed 727 passed 1,800 effect-tick cases
with live allocation/recycling as well as exhausted pools, camera alignment,
first-tick suppression, emission sentinels, stationary/moving interpolation,
per-emission substeps and lifetime returns. Seed 733 passed 1,800 active-particle
cases, including signed capacity, inactive slots, expiry and recycling. Seed 739
passed 1,800 spawn-and-simulate cases. Zero mismatches; Release gbtg build passes.
The initial tick fixture allowed zero interval with expired emission timer,
which loops forever in retail too; constrained fixtures to valid timer states
without changing implementation. Full 77-mode regression is running.

Remaining integration coverage: collision-hit notification with incoming local
scratch and real sound/effect resources, bound actor sampling through the full
tick, and full background/frame integration. Helpers for those branches already
have separate translations/checks. Next translate complete 0x00045A90 with its
first-background graph and recorded-animation paths, then 0x000464D0 wrapper.
Shader animation 0x00165A00 already has a translation; do not stub it. Effect
passes are complete shared translations but background update remains unbound.

Batch validation complete: seed 743 passed 7,700 cases across all 77 helper
modes with zero mismatches. Release gbtg build and git diff --check pass. No
CTests were run. Complete effect spawn/active/tick callers are now available;
background update and live gameplay frame binding are still outstanding.

Translated full background animation update 0x00045A90, including its 0x000446A0
tail path, and shader wrapper 0x000464D0. Uses the first background's graph/recorded
animation state, actual graph collision/recovery and matrix publication, packed
quaternion/position records, expiry flags, and the auxiliary effect tick/pose path.
Native background owner now has the +0x10 adapter registered in the existing
actor_updates map. The scene adapter supports graph/recorded paths; auxiliary
particle backgrounds explicitly remain blocked on the scene debris pool/effect
scratch context, rather than supplying an empty pool or fabricated result.
Differential fixtures and build validation are pending. The main gameplay frame
pass has not yet been connected to call the registered actor updates.

Recorded background differential check seed 751 passes 1,800 cases after an
assembly correction: the recorded path's multiplication input is descriptor
+0x30, not the physics rest matrix at +0x70. Covers packed frames, grouped rows,
age/time scaling, clip expiry, matrix output and dirty flags. Seed 761 passes
7,800 cases across all 78 helper modes with zero mismatches.

Added full background graph-update fixture to the point-graph oracle. It exposed
an existing 0x0010C8D0 recovery translation error: a failed 0x00151FAD inverse
must retain graph +0x170 bytes. The native helper was copying its unwritten local
instead. Seed the local from the actual destination before inversion. With that
fix, seed 757 passes 1,800 complete graph-update cases and seed 769 passes 2,400
cases across all 12 point-graph modes; zero mismatches. Includes collision solve,
transform recovery, inactive rows, frame clamp, age/motion settling and publish.
Release gbtg build passes; scene-start regression build/run is pending.

Identified real auxiliary debris initialization 0x000EC2D0 (50 slots, 0x16A8 heap,
aid_misc_ghoulies_fx_cheap_splat plus ten cached resources), teardown 0x000EC3A0,
and global effect/debris/trail tick 0x000ED480. Dumps exported read-only. These
are not yet translated/bound; the scene's auxiliary update remains explicit
until its real pool and incoming scratch state are available. Full gameplay
frame dispatch 0x0011E640 / 0x00123270 remains the subsequent frontier.

Scene-start regression passes after the owner/update registration changes:
background/navigation built, 43 activated actors, pending=0, notices 0x88/0x7D,
contact round-trip/event deduplication, teardown factory=0 and addresses=0.
Release gbtg and transition-test builds pass; git diff --check passes. No CTests
were run. Scene-start does not exercise the gameplay frame dispatcher, and does
not establish acceptance of the still-unbound auxiliary-particle context.

This batch completes the background update translations and native graph/
recorded update adapter. Outstanding: real 0x000EC2D0 pool/resource initialization
and auxiliary effect scratch/context binding, then full gameplay frame dispatch.
The initializer/teardown/global effect tick exports are retained as the next
frontier; no missing callee or pool was replaced with a stub.

Translated debris initialization 0x000EC2D0 and release 0x000EC3A0. The scene
owns the actual 50-slot heap pool and cheap-splat resource plus ten cached texture
views; initialization and release preserve free-chain and resource word writes.
Corrected existing heap selector 0x000F7100 to read the allocation's actual footer
tag after the HeapSize boundary instead of always using 0xFF. Auxiliary background
updates now construct collision, debris, sound, effect stream, command-resolution
and simulation contexts from actual scene state. Bind DAT_005077A8 by reference
to the real display global, and mirror the native script pointer into effect +0x134.
Incoming effect-pass scratch remains caller supplied through the native +0x10
adapter; it is not initialized to an invented value.

Added literal actor update queue drain 0x00123270: unlink before vtable +0x10,
then actual 0x00040DE0 transform refresh, re-reading the current-head word after
each callback. The complete 0x0011E640 frame caller still requires its other
untranslated passes; do not wire a shortened frame as acceptance. Build and
validation of this batch are pending.

Opening scene-start now passes with real debris resource/pool initialization,
caller-supplied scratch and an actual call through the registered background
+0x10 slot. Added native actor-queue verification: unlink-before-callback,
callback-added actor processed in order, and transform refresh after callback.
Pool release runs under native ownership; factory/address teardown remains zero.

Continued into frame prerequisites: translated control activation 0x00115FF0,
one-word 0x7D notification 0x00116090, update queue drain 0x001160F0 and gameplay
time advancement 0x0012B1F0. Preserve active-head insertion versus update-tail
append, callback-added links, current-head re-read, unsigned frame wrap and
0x027001E0 gameplay-time gate. Added control-order checks and time differential
fixtures. Full 0x0011E640 still requires the remaining sound, global effect,
scare/deletion and camera passes; no shortened frame loop is connected.

Batch checks complete: Release gbtg and transition-test builds pass; scene-start
passes including the actual background update slot and actor/control queue tests.
Actor-loading passes. Time advancement seed 773 passed 1,800 differential cases;
seed 779 passed 7,900 cases across all 79 helper modes with zero mismatches.
No CTests; git diff --check passes. Auxiliary binding now exists with real pool,
resources, display and command addresses, and caller-supplied scratch. The opening
fixture exercises the graph background; a dedicated auxiliary-background fixture
is still needed. Continue with the complete sound/global-effect frame passes,
then scare/deletion/camera closure before connecting 0x0011E640.

Translated actor sound frame pass 0x0012A150 with active-voice update 0x0012A510
and pending distance sort 0x0012A6E0. Uses actual voice allocation/preparation,
source positioning/cone APIs, cue stop and pending dispatch. Preserve callback
changes to the current voice index, saved-next traversal, owner notices 0x8A/
0x8B/0x8C/0x89, voice-state gates and COMISS unordered sorting. Generalized
0x0002CC30 link exchange for strategy and sound links, preserving its branches.
Added callback/index/source update checks, real exhausted allocation and distance
sort differential fixtures. These shared functions are not yet called by the
complete gameplay frame; the latter still needs its remaining passes.

Sound checks pass: seed 787 passed 1,800 pending-sort differential cases with
zero mismatches, including unordered NaN comparisons and pointer rewiring. Native
frame checks verify ended/live owner notices, callback-changed voice indices,
cue stop state/count, scaled source position, cone (not velocity), nearest-first
pending order and real exhausted voice allocation. Scene-start still passes with
43 actors, direct background update and clean teardown; actor-loading passes.
Release builds and git diff --check pass. Full 80-mode regression is running.

Remaining full-frame closure is explicit: 0x00115F70 queued object disposal,
0x00123BA0 scare timers, 0x000ED480 global effects/debris/trails, 0x001232D0 actor
deactivation/release and 0x00120EF0 camera/display work. The sound frame pass
0x0012A150 is now translated, but its scene message binding and the native +0x10
bindings for remaining actor classes also need completion before 0x0011E640 can
run every real slot. New exports include 0x000EC0B0 debris tick, 0x000EB9F0 trail
tick, 0x000EB6C0 trail phase setup, and shared scalar approach 0x00101B90 /
0x00101C00. These are the next implementation frontier, not fabricated callees.

Final batch regression: seed 797 passed 8,000 cases across all 80 helper modes
with zero mismatches. Release gbtg/transition builds, scene-start (including
background update plus actor/control/sound frame checks), actor-loading and
whitespace checks pass. No CTests were run. No commits or OriginalFiles edits.
The auxiliary background context is now bound with explicit incoming scratch;
the whole gameplay frame remains unconnected until its remaining real passes
and class update slots are complete.

Continued frame closure: translated shared scalar approach 0x00101B90,
rate-based approach 0x00101C00 and scalar bounded limits 0x00101B00.
Added debris tick 0x000EC0B0, trail phase setup 0x000EB6C0 and trail tick
0x000EB9F0, preserving unordered expiry gates, shared random color sample,
frame channel limits, lifetime handling and free-chain writes. Their native
interfaces omit incidental EAX values; the gameplay frame does not test those
values. Global effect pass and native trail pool ownership remain pending.
Added four differential fixture modes; validation of this batch is pending.

Seed 811 passed 1,200 cases each for the four new modes (4,800 total),
with zero mismatches; Release gbtg and the differential harness build pass.
Translated complete global effect pass 0x000ED480 using actual emitter tick
0x000EF2D0 and release 0x000ED2E0, then all 50 active debris and trail slots.
No placeholder callee or tested return is introduced. Native trail initialization
and scene-wide context binding remain to be completed before calling it live.
Global wrapper build and opening-scene regression are pending.

Batch validation complete: Release gbtg and transition-test targets build;
scene-start and actor-loading pass, with 43 activated actors, actual background
update and clean factory/address teardown. Seed 823 passed 8,400 cases across
all 84 modes with zero mismatches, including actual camera/grid hits and
collision callbacks. git diff --check passes; no CTests or OriginalFiles edits.
The new global wrapper is compiled and source-audited, but does not yet have
a dedicated whole-pass fixture or live scene binding; its new debris/trail
callees have differential coverage. Full gameplay ticking is still incomplete.

Exported the next closure batch read-only: 0x00123E00 scare-list removal,
0x000F4970 camera actor removal, 0x000ED5B0 emitter actor detachment,
0x00132620 scare timer notification and 0x00120FE0 / 0x001211E0 camera passes.
0x00124320 powerup cancellation and 0x001075C0 factory release already exist;
reuse their real implementations. Camera removal 0x000F4970 itself still needs
0x000F4C10 / 0x000F4CC0 / 0x000F4B90 closure. Remaining work is native global
trail/context ownership, queued disposal, scare/deactivation/camera passes,
sound scene messages and remaining actor update slots before 0x0011E640 wiring.

Continued the next closure batch: added 0x00132620 scoreboard warning updates,
0x00123E00 scared-actor removal, full 0x00123BA0 powerup/scare timer pass,
0x00115F70 queued factory disposal and 0x000ED5B0 emitter actor detachment.
Preserve saved-next disposal traversal, swap-from-last scare removal,
callback-visible nine-word 0x4D/0x1B messages and dt*scale decay without the
freeze gate. COMISS unordered remaining times stay active, as in assembly.
Native scene bindings and validation of this batch are pending. Camera actor
removal has a larger tutorial-introduction dependency chain; do not stub it.

Added native sound frame context binding: actual scene queues, audio voices and
current display listener, with scoped pointer-width sound-reference adaptation.
The actor reference adapter now resolves actual scene actors; player 0x7C/0x8B
messages use native reference payloads before forwarding original words to the
three child classes. Registered the existing player +0x10 update implementation.
Added differential modes for scare removal/ticking, scoreboard warning lookup
and emitter detachment, plus native saved-next destructor traversal verification.
Full-frame wiring still waits for real tutorial-introduction cleanup dependencies,
remaining camera/render passes and native global trail/effect ownership.

Scare/scoreboard seed 839 passed 1,800 differential cases with zero mismatches.
Emitter detachment initially exposed a fixture variable overwriting its unrelated
slot input; renamed that fixture variable (implementation bytes matched already).
Release builds, scene-start and actor-loading pass for the preceding batch.

Closed native trail ownership: translated 0x000EBB10 texture binding,
0x000EBB80 50-slot cheap-dust pool initialization and 0x000EBBF0 release.
Scene ownership now loads and releases the actual four texture resources and
pool alongside debris. Bound 0x000ED480 to real scene collision, audio, effect
streams, frame state, shader/resource release and caller-supplied effect scratch.
The opening-scene fixture now calls the global pass as well as background +0x10;
this latest native ownership/binding batch is pending validation.

### Live scene start and the 0x6F gate (2026-10-13)

**Scene start in the live game.** `script_queue_scene_start` (the host 0x0011CB60)
now runs `SceneActorServices::start_scene`, the translated part of 0x0011CBD0
in retail order:

1. background, its activation and navigation;
2. markers;
3. activation;
4. the script's 0x7D event (0x00116810);
5. the 0x88 notice;
6. the actor 0x7D sweep.

`SceneFactoryRuntime::actor_start` records the outcome. If a service is not yet
bound, the run logs `native scene start stopped: ...` and the scene keeps the
legacy path. The live opening run now logs `native scene start complete`, and
both headless runs exit 0.

**The 0x6F wait.** `SceneActorServices::wait_script_player` evaluates 0x00127BC0
over the scene's player link and actor graph. After scene start it returns 2
(continue); `--scene-start` prints this.

Binding it live moves the opening script past node +0x254 to the next type 0x14
dialogue node. That node raises because no scene dialogue runtime is installed
(`bind_scene_dialogue` has no caller). So the live 0x6F case keeps its hold, and
its comment now says why.

**Next.** Build the scene-owned dialogue runtime from the services:

- `RuntimeDialogueActivationContext`: the widget allocation and bat factory,
  page layout and settings, measurement and tags, input activation and release,
  listener, voices, the new cutscene runtime and widget update.
- `RuntimeDialogueOuterDrawContext`: render targets, D3D boundaries, the sprite
  queue and display setup.

Bind both at native scene start, then switch the live 0x6F case to
`wait_script_player`.

**Still missing from 0x0011CBD0:** 0x00060C80, 0x00114180, 0x001153A0,
0x00123560, 0x00119990, 0x0011F450, 0x0011B7A0/0x0011B8F0/0x0011B7F0,
0x00112D70, 0x00125B80, 0x00127250, 0x00125070, 0x0011C980, 0x000F4440,
0x001160C0, 0x00123240, 0x0012A1E0, 0x00132040 and 0x00049BA0. Their dumps are
now exported.

### Scene dialogue runtime installed (2026-10-13)

The services now build the dialogue runtime and bind it (`bind_scene_dialogue`)
when they start, and unbind it in their destructor.

**Boot globals translated or applied** (the parts of 0x00033030 the dialogue reads):

- HUD safe-area layout 0x000FC190 (`frontend/hud_layout`), over the D3D display
  mode 640x480.
- Display views 0x000F5900.
- Port masks DAT_0050E0F4.. = {0, 1, 2, 4, 8} (0x00033248).
- Text init 0x00060320 and its table counter 0x0005F210
  (`frontend/dialogue_text_system`): the null-char texture DAT_004E1C58, the
  language fonts DAT_00504444, the draw defaults, and the text-pool registry
  DAT_005043CC.

**Dialogue contexts.** The activation, measurement, tag and trophy, widget, page,
draw, display-setup and outer-draw contexts are built from scene state. The
initialized data (DAT_00424900.., DAT_003150D0 icon names, DAT_00421A1C/24
audio categories) is read from the image.

**D3D boundary.** `graphics/scene_d3d_device` is a host D3D device for the
dialogue boundary sets. It keeps transforms, vertex constants, textures and
states, and decodes push-buffer quad batches into recorded draws.

**Supporting changes:**

- `SceneDialogueResourceOwner` serves loose `aid_font_` files.
- `publish_font` gives the metrics the fonts that 0x00060320 acquires.
- `DialogueWidgetUpdateContext.bats` and `CutsceneRuntimeContext.draws` are now
  lookup functions over live actor views.
- The renderer gets 32 frames (DAT_004392B0) mapped to dialogue frame views.

**Live result.** With the 0x6F query bound, the live opening:

1. passes 0x6F;
2. reaches type 0x14 with `chapter1_scene1_fight1_howtofight`;
3. enqueues the localized text;
4. activates mode 1: menutext widget, button bat, the dialog camera cutscene
   load, page reset with the button prompt;
5. holds the script at 0;
6. runs `update_runtime_dialogue`: input device, widget and bat update, and
   the cutscene tick.

The draw then reaches `draw_runtime_cutscene` for the dialogue cutscene's prop
actor (type 0x1A, whose vtable +0x14 is 0x0006EF20 -> 0x0003C9E0 -> 0x000F9080).
That needs the translated scene model renderer context (`SceneModelDrawContext`:
lights, environment and model-tree dispatch with their D3D boundaries).
`actor_draw` raises with the class, so the live 0x6F case keeps its hold and
the live runs stay green.

**Next:** bind the scene model renderer, either through the host D3D device or a
renderer translation. Then the live 0x6F gate can open and the tutorial
dialogue draws.

### Handoff audit and renderer boot binding (2026-10-13)

Reviewed commits 27d535d and bee56d9 and the shared checkout. The live 0x6F
case is already bound to wait_script_player, and scene-owned dialogue and model
render contexts now exist in code. The prior final note saying actor_draw is
wholly unbound is stale: type 0x1A has its actual 0x0003C9E0 draw chain bound.

Confirmed earlier batch validation on the combined checkout: native trail pool
ownership and global effect call compile; scene-start/actor-loading pass, with
43 activated actors, actual sound/global-effect/background calls and clean
teardown. Scare/scoreboard and emitter detachment seed 839 each pass 1,800
retail differential cases. Full gameplay 0x0011E640 is still incomplete.

Found and fixed two renderer context omissions before further live validation:
call the existing 0x00155A10 boot routine over the actual relocation descriptor
allocation (17 types) and real shader-fragment table; resolve executable node
vtable words alongside relocated resources. Previously descriptors remained
zero and those vtable addresses were absent from the render address view.
Removed temporary TRACE output from the actor/model draw wrappers. Added a
native check that all 17 relocated node descriptors name the correct type and
resolve a real executable draw slot. Latest renderer changes await validation.

Model-node frontier is explicit: installed callbacks currently cover groups,
visibility, vertex buffers, pixel shaders and push buffers. Skeleton, matrix
array, vertex shader, shader-parameter, skin, blendshape, transform/2D and
background push-buffer draw slots still need literal translations/bindings.
Read-only exports now include 0x00155E60, 0x00157FC0, 0x00157D50 and
0x001580E0; the shader cache selection 0x001596D0 calls 0x00159530 and must
not substitute a shader result for that missing callee.

Renderer boot was also absent from the CMake engine source list; added it so
the real initialization routine links into both game and transition targets.
The resource registry check compares its 17 names against the existing native
relocation types and resolves each draw slot from the actual executable.

Latest Release game/transition builds and scene-start pass, including all 17
registry checks. The live player-opening fixture now reaches the model draw
path and fails on an unbound native lookup; it does not pass tutorial acceptance.
Added a host virtual-slot lookup diagnostic naming the actual missing model draw
callback and node, preserving the translated traversal and providing no default
callback. The diagnostic build/run will identify the next concrete closure.

The diagnostic identifies the actual opening tutorial frontier as skeleton draw
0x001569D0 at relocated node 0x40FE1F84. Translated that slot: resolve skeleton
instance +4 pose, publish render state +0x60/+0x68/+0x6C, then queue the actual
group traversal. Model pose binding now mirrors its real pointer-width store in
the instance storage, keyed by the retail copy offset. Bound the slot to the
scene model renderer. An untouched zero pointer word resolves to null; an
unmapped nonzero pointer raises instead of inventing a pose. This next closure
batch still needs validation and the subsequent reached node slots.

Added mode 86 differential coverage for skeleton draw 0x001569D0: varied
instance copy offsets, pose pointer words, bind matrices and joint counts, with
retail state +0x60/+0x68/+0x6C compared byte-for-byte. The full native model
header rebuild is running; do not run the old harness against the new mode.

Matrix-array lookahead: 0x00155F80 calls real matrix calculation 0x00155E60;
its scalar/matrix callees already exist. Output scratch DAT_00437080 is supplied
through 0x00155D20 (also mirrors DAT_00437090 and count DAT_00437094). Bind
that actual owner rather than choosing an arbitrary matrix buffer. The skeleton
fixture and native opening draw will be checked after the shared-header rebuild.

Scratch owner identified: 0x000F8740 calls 0x00155D40 for the real scratch size,
allocates it through aligned heap 0x000F7330 with alignment 0x20 and supplies
it to 0x00155D20. It also initializes the material pool and shader hooks, so
its remaining callees must be audited before binding the complete boot owner.

Skeleton validation complete: seed 853 passed 1,800 mode-86 differential cases
with zero mismatches. The fixture originally captured an indirect render-state
pointer; 0x00156DD0 actually returns the address of DAT_004370A0 directly.
Corrected the fixture capture; the translated skeleton implementation was
unchanged. Release game/transition builds, scene-start (all registry checks,
43 activated actors, sound/effects/background calls and clean teardown) and
actor-loading pass. Live player-opening now gets through skeleton draw and
stops at the next real slot: matrix array 0x00155F80, node 0x40FE2120.
Tutorial rendering therefore remains incomplete; do not mark it accepted.
All 87-mode differential regression is running; no CTests or OriginalFiles edits.

Final checkpoint: seed 857 passed 8,700 cases across all 87 modes with zero
mismatches, including actual camera/grid hits and collision callbacks. Release
builds, scene-start, actor-loading and git diff --check pass. Skeleton draw is
translated, bound and differentially verified. Tutorial acceptance still fails
at matrix-array slot 0x00155F80; this is the next implementation frontier,
not a missing skeleton pose or an empty node registry. No CTests, commits or
OriginalFiles edits in this continuation.

### Matrix-array and rigid-skin draw batch (2026-10-13)

Translated 0x00155E60 matrix calculation and 0x00155F80 draw dispatch, plus
scratch size/binding 0x00155D40 / 0x00155D20. Preserve flag-bit precedence,
dynamic-matrix flag offsets, skeletal/static matrix choice, SSE 0x00151E64
versus D3DX multiply, in-place second multiplication, count reloads and output
publication even for nonpositive counts. Bound the real 0x1F400-byte scratch
allocation with 0x20 heap alignment, following the allocation/binding sequence
inside 0x000F8740; this does not claim the entire boot owner is translated.

Added aligned heap 0x000F7330 with the temporary heap alignment field, footer
writes and alignment restoration. Native heap size/free boundaries retain the
matching aligned allocation metadata. Added rigid-skin draw 0x00157B90: actual
projection upload at c0x74 and transposed selected matrices at c0x78 + 3n,
then genuine model group traversal. Both draw slots are bound to scene state.
Validation and differential fixtures for this implementation batch are pending.

Added two differential modes for the matrix calculation/dispatch and rigid-skin
uploads, including count-zero/negative calculation, all row flag combinations,
selected skeleton/static/dynamic matrices and exact uploaded registers/words.
Added native aligned-heap checks over interleaved normal/aligned allocations,
full scratch-sized payloads and matching release. Changes are batched before
validation; shader node closures remain separate pending work.

Matrix/rigid-skin checks: seed 877 passed 1,800 cases each in modes 87 and 88
(3,600 total), zero mismatches. Release builds and scene-start pass, including
real aligned scratch allocation and interleaved aligned/normal release checks.
Live tutorial now passes matrix-array dispatch and reaches shader slot
0x00157CE0 (node 0x40FE2168). Its 0x0015B5E0 setup has four real render-state
helpers plus a post-shader callback; those must be translated/bound rather than
leaving the callback absent. Rigid-skin uploads are differentially verified but
remain later in this live traversal. No CTests in this batch.

### Shader-state closure (2026-10-13)

Translated shared state helpers 0x0015B380 / 0x0015B3F0 / 0x0015B4B0 /
0x0015B580, shader binding 0x0015B5E0, callback setter 0x0015B660, texture-stage
query 0x0015B670 and draw dispatch 0x00157CE0 in a narrow graphics module.
Preserve each hardware call before its shadow write, additive's call through
alpha setup, shader kinds outside 0..3, and pixel shader before post-callback.
These functions are not yet installed in the native scene: actual boot installs
0x000F9B40, which calls 0x000F84A0 and texture-combiner setup 0x000F7420.
Read-only exports now identify that entire chain; do not omit the callback to
claim live acceptance. The combiner's four render-state dispatch chains remain
next work. Validation of the newly translated shader functions is pending.

Assembly audit: 0x00156DE0 uses frame stride 0xC4, not 0x310 as an older native
header comment states. Logical frame indexing remains correct; +0x84 is the
projection matrix used by the verified rigid-skin upload.

Closed the callback chain after the shader-state checkpoint: 0x000F7420 now
retains its four texture-combiner state dispatches, actual command/dirty-mask
lookup tables, signed comparisons, specialized D3D calls and shadow/dirty store
order. Translated alpha-test helper 0x0005E0C0, model overrides 0x000F84A0 and
post-shader hook 0x000F9B40. Native pointer-width views use the existing model
+0x1C0/+0x1C4/+0x1C8 words, not fabricated flags. Shader draw 0x00157CE0 is now
installed with the actual callback setter. Shared depth/blend values alias the
same D3D shadow storage as dialogue; enlarged that storage to cover the actual
combiner index range. This supersedes the preceding unbound checkpoint.
Implementation batch complete; build and oracle validation are still pending.

Shader validation: seed 883 passed 1,800 mode-89 cases, zero mismatches. The
fixture checks full render-state shadows, dirty bits, selected texture stage,
GPU call order/arguments and the shadow checksum seen at each GPU boundary.
It covers native model override bytes, callback present/absent, shader kinds
outside 0..3 and all simple/deferred/specialized combiner dispatch ranges.
The fixture initially mapped an already mapped lookup-table page; corrected
that fixture mapping before execution, with no implementation change.
Release game/transition builds, scene-start and actor-loading pass. Live tutorial
now passes shader slot 0x00157CE0 and reaches vertex shader 0x00158510 (node
0x40FE22A0); player-opening remains incomplete. Its 0x001596D0 cache selector
requires genuine 0x00159530 creation and 0x0018D2F4 XG shader splice. Exporting
that closure next. The all-90-mode regression is running; no CTests.

Final shader checkpoint: seed 887 passed 9,000 cases across all 90 modes with
zero mismatches (19 camera hits, 25 grid hits, 19 collision callbacks and 10
stops). Release builds, scene-start, actor-loading and git diff --check pass.
The live tutorial frontier is now vertex-shader slot 0x00158510, not shader
state 0x00157CE0. No CTests, commits or OriginalFiles edits in this batch.
Read-only exports for the next splice closure are available at 0x0018D2F4,
0x00186FD5 (instruction unpack), 0x0018850C (shader type) and 0x0018C563
(optimization driver). The optimizer has additional non-host callees; export
and translate those before claiming a complete generic splice implementation.
Assembly also shows 0x001596D0 snapshots variant count once before its search;
its decompile suggests reloads, so retain the assembly's actual comparison.

### Transform/billboard and shader-splice leaves (2026-10-13)

Read-only shader optimizer exports reveal nine additional optimizer/validator
callees below 0x0018C563, with further dependencies. Generic XG shader splice
is still incomplete; no optimizer substitute or invented vertex shader handle.
Translated its independent leaves: instruction unpack 0x00186FD5 and shader
header type 0x0018850C. Also closing independent renderer slots while this
larger dependency chain is audited.

Translated frame push/pop 0x00156E00 / 0x00156E30, joint-offset matrix
0x00156A30, frame constant uploads 0x00156540, transform application 0x00157F10,
transform/billboard restores 0x00157ED0 / 0x00157D30, and draw dispatches
0x00157FC0 / 0x00157D50. Frame push copies all four actual C4-frame components:
combined matrix, camera pointer, world matrix and projection. Keep skeletal
versus dynamic/static matrix choice, D3DX multiply order, exact SSE translation
sum order, projection/world constant register tests, and sibling/restore/child
stack order. Native scene binds both draw and restore slots. Validation pending;
vertex-shader 0x00158510 remains the live tutorial frontier.

Build audit correction: 0x00186FD5 / 0x0018850C already exist in
vertex_shader_code.cpp, which the initial address search missed. The linker
caught duplicate instruction unpack definitions; removed this batch's duplicate
module and reused the established functions. Mode 91 now verifies those existing
leaves, including aliased output buffers. The transform/billboard module remains
new. This supersedes the preceding claim that the splice leaves were newly
translated. No generic splice/optimizer implementation has been substituted.

Wider source audit found the optimizer's nine direct non-host callees already
translated in shader_pair_optimizer, shader_merge_pass, shader_register_rewrite,
shader_rewrite_context, shader_reorder, shader_schedule_cost_inputs and
shader_validation_program. Do not redo these modules. The remaining driver
0x0018C563 must assemble their actual stack/typed contexts; splice 0x0018D2F4
then cache creation/selection can call that completed driver. Earlier notes
about needing to translate those nine direct dependencies are superseded by
this source audit. Generic splice and the driver themselves remain missing.

Transform/leaf validation: seed 907 passed 1,800 mode-90 cases and 1,800
mode-91 cases, zero mismatches. Mode 90 compares all three C4 frames, frame
index, sibling/restore/child stack entries and exact GPU constant registers,
counts and words; it covers skeletal/static/dynamic transforms, billboard
translation, standalone/restored uploads, register thresholds and in-place
joint matrix updates. Mode 91 verifies the existing unpack/type leaves over
random words, known/unknown headers and overlapping buffers. Release builds,
scene-start and actor-loading pass. Live player-opening still reaches the
pending vertex-shader slot 0x00158510. All 92-mode regression is running.

Final transform checkpoint: seed 911 passed 9,200 cases across all 92 modes,
zero mismatches (22 camera hits, 23 grid hits, 26 collision callbacks, 14
stops). Release game/transition builds, scene-start, actor-loading and
 git diff --check pass. No CTests, commits or OriginalFiles edits. Player-opening
remains unaccepted at vertex-shader slot 0x00158510; newly bound transforms
are differentially verified but occur later in that live traversal.

Next driver integration notes: existing ctor_harness.cpp constructs and tests
the complete typed ShaderValidationProgramContext around 0x0019723C; reuse
that ownership pattern rather than faking validation. 0x0018C1B9 and
0x0018C277 allocate separate local schedule frames in their assembly; their
native APIs take explicit state pointers, so supply separate retained scratch
views for those calls. 0x0018C563's other frame views are EBP-0x22C for dead
writes, EBP-0x24 for register rewrite and EBP-0xAF4 for reordering. Keep the
merge retry frame explicit, following the already translated merge APIs.

### Optimizer driver and vertex splice integration (2026-10-13)

Translated 0x0018C563 using the existing optimizer/validator passes and explicit
borrowed scratch contexts. Preserve low-byte option normalization, alternate
program allocation/validation, signed HRESULT exits, pass order, count-reduction
retry, ignored cost-call return values, ordered x87 cost comparison and cleanup.
Unordered cost comparisons retain the current program, as the assembly's parity
branch requires. Translated generic splice 0x0018D2F4, including capacity-query
return, fragment end-bit clearing, instruction unpack, actual optimization call,
backward instruction repacking and optional final instruction count. Optimized
calls require the genuine typed validation owner and retained scratch context;
no fabricated optimization results. The live cache creator passes optimize=0,
so that path naturally performs the actual concatenation without invoking the
optimizer. Driver/splice validation and vertex cache integration are pending.

Implemented cache creation/selection 0x00159530 / 0x001596D0 and vertex-shader
draw 0x00158510. Retain six-variant saturation behavior, backward pair shifts,
flag/sign-dependent fragment choice, real fragment lookup, ignored splice/D3D
return values and count reload after D3D creation. The creation boundary writes
the actual cache handle slot by reference. Native D3D records own copied shader
declarations/programs and vertex stream bindings. Bound the scene's existing
resource-owner shader cache, lighting fragment table and matrix register slots.
Vertex constants preserve conditional projection/world/camera/aux uploads and
the unconditional +0x198 upload. That upload uses existing real fog words at
+0x198/+0x19C; the remaining native fields retain their BSS state. Lookup's
retained fragment word is written by the real registered fragment search before
its live use. Build and differential validation are pending for this batch.

Vertex validation checkpoint: seed 929 passed 1,800 cases each in modes 92,
93 and 94 (5,400 total), zero mismatches. Splice coverage includes missing
size/count outputs, exact/short capacity and zero/count-zero fragments. Cache
coverage includes hits, six-variant saturation, fragment choices, signed flags,
full created declaration/program bytes and failed D3D creation retaining the
real output slot. Vertex coverage compares complete GPU call order, streams,
shader handle and every constant word/register/threshold plus retained matrix
register fields. The splice fixture initially reused its case-count variable;
renamed that fixture variable before comparison, with no implementation change.
Release builds pass. Live player-opening now passes vertex shader 0x00158510
and reaches shader-parameter slot 0x00157090 (node 0x40FE255E). Generic splice's
optimized branch/driver still needs its own new integrated validation with the
existing typed validator fixture; the tested/live creator calls optimize=0.
Added a native D3D ownership check that destroys/overwrites temporary creation
buffers and verifies both shader records and independent stream rebindings.

Final vertex checkpoint: seed 937 passed 9,500 cases across all 95 modes,
zero mismatches (20 camera hits, 24 grid hits, 19 collision callbacks and 15
stops). Release game/transition builds, scene-start (including temporary shader
buffer lifetime and stream rebinding checks), actor-loading and git diff --check
pass. No CTests, commits or OriginalFiles edits. Live tutorial frontier is now
shader parameter 0x00157090, node 0x40FE255E. Vertex cache/splice/draw's live
nonoptimized path is bound and differentially verified; integrated optimized
splice/driver validation remains outstanding, so do not mark that path accepted.

Next frontier exports include 0x00154840 (three-word copy), 0x00153AC0
(SSE column lengths plus existing 0x00152B30 square root), 0x001537C0 (row
scaling with alias test) and 0x0015B720 (texture-selection setter), alongside
existing dumps for the 0x0015B6xx adapters, 0x00157030 and 0x0015AD20. The
material bind has texture resolver 0x0015ABB0 and registered callbacks
DAT_004D9754 / DAT_004D9750 in addition to actual D3D calls. Audit/bind their
boot owners before installing shader-parameter draw. The gfx_d3d11 material
helper is a pre-existing host renderer adapter; verify its branches/pointer
views before attempting to share it with the reconstructed scene path.

### Live opening runs: 0x6F player wait enabled (2026-10-13)

The live opening (`gbtg --headless --frames 900 --scene chapter1_scene1_2playcam`)
now runs all 900 frames with the script 0x6F player wait live and exits 0. The
tutorial dialogue draws on 809 frames ("native scene start complete: Free Play
(Fighting Intro)"), and the script advances to its TimeOut condition. The 0x6F
action returns the native wait once `actor_start == 1`. Before a native start it
returns 0, because there is no player to wait on.

New translations behind the dialogue cutscene draw:

- **Shader-parameter node 0x00157090** (`model_shader_parameter_draw`):
  - It covers the material bind 0x0015AD20 with its texture-record walk
    0x0015ABB0, the texture key 0x00157030 and the name hash 0x00158BB0.
  - It uses the material global setters 0x0015B6B0, 0x0015B6C0, 0x0015B6E0,
    0x0015B690 and 0x0015B720.
  - The math leaves 0x00154840, 0x00153AC0 and 0x001537C0 went into
    `d3dx_matrix`.
  - 0x000F8740 binds the callbacks: DAT_004D9754 is 0x000F9BC0, which passes
    the texture to the existing 0x00053550 in ESI. DAT_004D9750 is 0x000F9B90,
    and its stage colours come from 0x000F7FA0 (both in `model_shader_states`).
  - Nothing writes DAT_004D9764, DAT_004D9648 or DAT_0054D950, so those branches
    keep their BSS values.
  - Render state +0x46 (`texture_key`) and +0x54 (`material_vector`) are now
    modelled, and 0x00155A10 zeroes them.
- **Blend shape node 0x00157A50** (`model_blendshape_draw`):
  - It takes the GPU path 0x00157880, whose instance arrives in EAX. That path
    picks the eight strongest weights into render state +0x1B0 and redirects
    target attributes 0x1B..0x22 to each target's first type-9 record in the
    +0x78 stream table.
  - The host stream table now spans the full native +0x78..+0x183 range (67
    words); the live node has 18 attributes.
  - The CPU vertex path 0x00157650 (node byte +0x26) is not reached live, and it
    raises.
- **Button bat draw 0x0006D6C0** (type 0x15, vtable 0x00504C34 +0x14), in
  `button_bat_draw`. It calls the screen-model draw 0x000F9250
  (`screen_model_draw`):
  - The position arrives in EAX, the model in ECX, and the pass in EBX into
    0x000F9080.
  - The display transform is set by 0x000F6EB0 (`set_display_transform`).
  - The D3D clear goes through the outer boundary.

Scene fixes:

- **Sprite flush:** 0x0011C240 flushes the queued dialogue sprites (0x00122230)
  right after the dialogue draw (0x00058150). The scene caller now does the
  same, so the widget icon queue (64 records) no longer overflows.
- **Audio core:** the scene's audio core had no host boundaries. Dialogue page
  voices reach `create_audio_core_source`.
  - `native_audio_core_memory` is the host heap.
  - `native_audio_core_source_boundaries` uses XAudio2 submix voices for XACT
    sources. Status is 1 while a routed cue voice still has queued buffers.
  - The existing cue and sound boundaries are now bound.
  - Cue names for sound-bank handles raise when reached. Nothing populates
    `g_audio_bank_names`, so `resolve_audio_bank_cue` fails before preparation.
- **Cutscene release:** `CutsceneActorReleaseContext` / `CutsceneEventContext`
  `objects` is now a lookup over the live cutscene track actors. The door path
  had been the only place that filled the old map, so the dialogue cutscene's
  release missed its actors.
- **Teardown order:** the services destructor now closes the dialogue before it
  frees the cutscene actors.

Validation:

- Release build: 6 transition suites, ctest 6/6.
- Default and opening headless runs both exit 0.
- `git diff --check` is clean.

Still outstanding:

- Oracle coverage for the new leaves.
- Bank name registration for dialogue voice cues.
- The remainder of 0x0011C240 (world/HUD draw), which the host frame still
  skips.

### Native views presented: camera and dialogue quads (2026-10-13)

The earlier "runs green" note only meant that the native path no longer crashes.
Nothing native reached the screen, because `SceneD3DDevice` recorded its draws
and discarded them. With the native scene started, the D3D11 frame now does this:

- **Per-view present (0x00033A20).** The frame runs
  `present_runtime_display_views`: for each active camera slot it calls 0x489E0
  and 0xF5E50, writes the 0x13C490 clip limits into display +0x2C/+0x30, then
  calls 0xF5FD0, 0xF6170 and the view draw. `SceneActorServices::present_views`
  supplies that view draw. It hands the slot's display (position, rotation, and
  field of view converted from degrees) to `gfx_set_camera`. It then runs the
  host world draw, the scene dialogue (0x58150 and the 0x122230 sprite flush),
  and the dialogue quads the native draw recorded.
- **Quad presentation.** Constants c0x60..c0x63 map each quad to clip space, and
  the quad is submitted as a 640x480 HUD sprite with constant c0x64 as its colour.
- **Texture recovery.** A bound texture is the frame record at
  header + 28 + frame * 0x14 (0x53550). That holds both for texture resources and
  for the per-glyph headers inside fonts. The presenter recovers the header from
  the record and decodes the frame with `decode_xbox_texture`.
- **GPU spans.** Non-model bundle rows now record their first GPU chunk, as
  `image_from_entry` and `transition_sprite` already read it. Before this, the
  row held the entry's whole GPU region, so textures decoded 0x80 bytes early.

During the tutorial dialogue, slot 0 is the dialogue's own camera node,
"dialog". `camera_bind_named_view` selects it when the page has no render
target. Its page cutscene, `dialogs_dialogpage1`, has single-key curves: a fixed
studio camera at (3359.6, 0, 2454.5), identity rotation and focal 35.

A frame-300 screenshot shows the butler head, the tutorial text and the A button
over black. Still to do: the page's backdrop and dialog-bat models (recorded
indexed draws) are not presented yet, and the rest of 0x11C240's world/HUD draw
is not translated.

### Opening scene tick, camera view and actor view pass (2026-10-14)

Two reported problems: after the tutorial the game left for the next scene
instead of starting the fight, and before the tutorial the hall did not show
Cooper and the imps.

**Leaving after the tutorial (fixed).** Condition nodes (`0x00125450`) end
with `0x000532B0(cursor, 0x3C)`: after appending the condition they move the
cursor to the block's closing `0x3C`, so the main walk skips the condition
body. The translation omitted that seek. The main cursor walked straight from
the `0x3D` timeout into the tutorial node, then through `0x11`/`0x3C`, which
emptied the fired list, and `0x00117140` requested the next scene AID.
`script_handle_condition_node` now takes the cursor and seeks. After the
tutorial the main cursor waits at the `0xF` gate of "(next bit)" (+0x618)
with the 10-second timeout and NumberGhouliesAlive conditions armed.
`--player-opening` now checks that hold for 300 frames after the tutorial.

**Scene tick.** `0x0011E4C0` runs the scene tick `0x0011E7F0` only in run
state 1 or 2, which comes from the mode `0x0011EB30` stores at scene record
+0x100. Every retail caller (boot `0x00033030`, the front-end slots, the
scene end `0x00117140`) passes 1, but the host `script_request` dropped it,
so the opening sat in state 0 and `frame_tick` ticked the script and the
dialogue itself. `script_request` now calls `request_scene_aid(name, 1)`;
`frame_tick` no longer calls `script_vm_tick` or `tick_scene_dialogue`; the
dialogue tick `0x00057E60` runs at its two retail call sites (`0x0011CFB0`
load phases 2..9 and `0x0011E7F0`). The script's timers now run only once
the slime-off transition ends (state 5 → 1), so the tutorial opens about 100
frames later than before; `--player-opening` holds for 720 frames.

**Play camera.** Cameron's camera body embeds its view node at body +0x894,
so `0x000731F0` and `0x00072B10` writing body +0x89C/+0x8A8 set the node's
position and Euler rotation. The native node is a separate record, so slot 0
stayed at the origin and the host world draw rendered the hall from inside
its geometry. Both functions now also store the node fields
(`RuntimeCameraBodySlots::view`, `RuntimeCameraBodyContext::view`). Frames
before the tutorial show the hall from the retail play camera.

**Actor updates (translated, held).** `0x00123020` (the bind tree under
`DAT_00511B10`, skipping actors whose +0x908 node is linked) and `0x00122FE0`
(the +0x934 list) are in `scene_actor_update.cpp`, bound as
`SceneActorServices::update_scene_actors` with a persistent scene-tick effect
scratch. Binding them live reached the frontier on the first frame, so the
call in `scene_tick_pause_counter` is held with a comment; the list is in
`docs/NATIVE_GAMEPLAY_FRAME.md`. Bindings added on the way and kept:

- mind +0x1C is the player's `mind_storage` (allocated by `0x00136700`);
- mind types 0x34/0x35 tick through their `SceneMindMessages` graph; the
  cinema tick `0x0007CE30` now has its avoidance, gaze, control and axis
  contexts, and the cinema slot's target view (mind +0x10);
- model anchors (`DAT_00510654`) have heap addresses for mind +0x30.

**Actor view pass.** Each native view now runs the bin sort `0x00121730`,
visibility `0x00121880` (base slot `0x0003C830`; FX emitter `0x00109E30`) and
the pass-2 draw `0x00121D40` (player `0x00028050`, base `0x0003C9E0`, FX
emitter `0x00109EE0`). Bin records keep their distance in
`ActorRenderRegistration::bin_distances`; `0x0002CC30` gained a list-head
form. The background's +0x14 (`0x00042C10`) is untranslated, and the host
world draw stands in for it, so the hall is drawn once per view in retail
order. The stand-alert state 0x87 (getter `0x000A2530`, found by scanning the
XBE for its slot immediates) has the empty `0x001628B0` at +0x18.

Cooper and the five imps are in view bin 0, in front of the camera and pass
the frustum test, but nothing shows: their models draw in passes 0 and 1,
inside the untranslated `0x00121900`. That function, its world helpers and
the held actor updates are the next milestone
(`docs/NATIVE_GAMEPLAY_FRAME.md`).

Validation: Release and Debug build; ctest 6/6 in both; all transition suites
in Release (default, `--title`, `--title-a`, `--goop`, `--ui`,
`--player-opening`, `--scene-start`, `--scene-spawn`, `--dialogue-resources`,
`--actor-loading`, `--ghoulybox-decoder`, `--scene-models`) and Debug
`--player-opening`; both 900-frame headless runs exit 0; `git diff --check`
is clean. Captures `cache/opening_260.png`, `cache/opening_300.png` (hall
from the play camera, no characters) and `cache/opening_600.png` (native
tutorial prompt) were inspected. No temporary traces remain; nothing was
staged or committed.

### View passes and boot vertex shaders (2026-10-14 continuation)

Continued the uncommitted view-pass work inherited from the previous session.
The live native view calls 0x00121900 between visibility and pass 2. Its
non-mirror path, shader-parameter handler swaps and gated world helpers remain
in the working tree. Actor updates are still held.

Removed the temporary door-shadow early return and the temporary environment
printf. The door pool now calls translated 0x0005B020, including its D3D state
writes, table index 0x13, transform retrieval and row eligibility checks.
New door_shadow_draw also translates 0x0005AC40 and 0x0005AE40 from their full
assembly: register-passed bounds, ordered/unordered comparison behavior,
transposed matrix constants, depth/alpha interpolation, the __ftol call and
packed-colour inline-array vertices. These geometry entries are not yet
reached live because pixel-shader creation is missing.

The host XGAssembleShader vertex boundary emits NV2A programs for all 24 retail
boot-table sources. It supports the table's arithmetic, matrix macro,
screenspace pragma and specific independent MUL/RCC pair; unsupported syntax
raises. Native 0x00065EE0 creates the table handles and 0x00065F60 binds them
and uploads its optional twelve constants from c0. Boot runs after the
executable/relocated address resolver is installed. This is shader-boundary
support, not a replacement for scene behavior.

SceneD3DDevice now records inline-array draws under created vertex shaders
with raw packed vertex words and draw-time state. The presenter reads their
actual declaration and runs the same NV2A interpreter and pixel definition as
indexed draws. The zero-handle dialogue quad path remains available.

Current measured live stop, with the temporary bypass removed:
`gbtg --headless --frames 300 --game-dir OriginalFiles --scene chapter1_scene1_2playcam`
exits 1 at `boot pixel shader creation 0x00063FB0 / XGAssembleShader`.
All boot vertex rows assemble and are created before that stop. A complete
opening screenshot cannot be obtained yet, so neither characters nor door
shadows are visually accepted. The old green headless results above do not
apply to this working tree's opening view pass.

The full pixel boot loop dump is cache/ghidra_frontier/00063FB0.txt. Its first
row calls factory 0x00063F40; that factory was exported with Ghidra read-only.
The remaining sources include paired alpha/RGB instructions, xdd/xmma,
texture kill and texture matrix instructions, not only mov/mul. The next
required work is their XDK assembler boundary, translation of 0x00063FB0 and
0x00063F40, and registration of owned pixel-shader definitions with the D3D
resource store. Then replace the explicit missing pixel binding in the door
context, run the opening, inspect captures and continue at its next live stop.

Added checks in scene_render_tests for homogeneous viewport conversion,
screenspace programs, vertex colour, unsupported opcode rejection and raw
inline-array colour/state ownership. Release default transition and
--scene-start checks pass; Release CTest passes 6/6. This does not establish
oracle equivalence for assembled shaders or acceptance of the complete scene.
No retail files were edited and nothing was staged or committed.

Final verification for this continuation: Release and Debug game/transition
builds pass; default transition runs pass in both configurations; CTest is
6/6 in both configurations; --scene-start passes in Release. Additional
synthetic checks cover door-volume boundary suppression with untouched
outputs, fade direction from either door plane, and unordered camera input.
The live opening still exits 1 at the explicitly missing pixel boot boundary;
both attempted frame-100 and frame-300 captures stop before producing a
complete screenshot. No visual or audible acceptance is claimed.

### Pixel boot restored; characters and tutorial visible (2026-10-14)

The missing pixel boot path from the previous checkpoint is now installed.
Native 0x00063FB0 walks all twenty rows, invokes factory 0x00063F40 for the
first row, assembles the other nineteen xps.1.1 sources and stores the handles.
The factory preserves its exact six nonzero D3DPIXELSHADERDEF words. As in
the assembly, both functions use CreatePixelShader's output handle without
testing its HRESULT; an assembly failure stores zero.

The XGAssembleShader host pixel boundary compiles the boot table's mov, mul,
add, sub, mad, dp3, lrp, cnd, xdd and xmma arithmetic, paired RGB/alpha writes,
def constants, x4 output scaling, texture loads, coordinate pass-through,
texture kill and the texm3x2pad/texm3x2tex dependent-read modes into a 60-word
NV2A definition. The format follows the register/channel and output fields
used by the existing presenter. Unsupported syntax raises. Static def values
are excluded from dynamic constant mappings. Program acceptance here means
these sources assemble; this is not an oracle check of every shader's output
or acceptance of the presenter's currently unused texture modes.

D3D creation copies each definition into an owned SceneResourceMemory block,
registers its handle with SceneD3DResources and releases those blocks/handles
when scene services are destroyed. The door context now binds the real table
pixel handle, with no temporary return or missing-boot throw.

Observed captures (inspected):
- cache/pixel_boot_300.png: Cooper and the imps are drawn in the central hall
  through native passes 0/1. The play camera looks up the stairs behind Cooper.
- cache/pixel_boot_900.png: tutorial head, text, textured backdrop, four corner
  models and the A-button bat are visible. The previously unpresented backdrop
  and dialog-bat model draws now reach the presenter.

Release live runs with and without chapter1_scene1_2playcam both complete
900 frames and exit 0. The opening reports native scene start complete;
acceptance does not rely on the legacy native-start-error fallback.
The inherited actor-update hold remains, so these captures establish drawing,
not controller movement or a playable fight. Dialogue voice playback was not
listened to during these headless runs.

Validation: Release and Debug game/transition builds; default transition check
in both configurations; Release title, title-a, goop, ui, player-opening,
scene-start, scene-spawn, dialogue-resources, actor-loading, ghoulybox-decoder
and scene-models suites; CTest 6/6 in both configurations. New checks cover
pixel RGB/alpha source selection, texture multiplication, paired channels,
static/dynamic constants, dependent texture encodings, unsupported syntax,
the native factory words and all twenty boot-loop rows including assembly
failure and ignored creation HRESULT. git diff --check is clean. No retail
files were edited and nothing was staged or committed.

Debug --player-opening also passes through fresh A edges and the post-tutorial
(next bit) hold. Inspected captures and test results above refer to the native
opening start completing, not a host fallback after a swallowed startup error.

### Native actor body checkpoint — 2026-10-14

The next native update work is recorded in NATIVE_GAMEPLAY_FRAME.md under
"Actor body frame checkpoint". Player/cinema mind contexts and body frame
slots are connected; gravity and stand-alert ticks are translated. A live
run exposed and fixed the missing actor +0x95C body pose binding. Camera and
imp body ticks now return, then the imp stops explicitly at the unbound body
channel rows (+0x150) and animation channel bindings (actor +0x50).

Actor updates remain held so the working rendered opening is preserved.
Movement and combat are not accepted. Release/Debug builds, CTest 6/6 in
each, dialogue-resource/body checks in both and Release scene-start pass.
The 900-frame screenshot cache/actor_checkpoint_900.bmp was inspected and
still shows the native tutorial, backdrop, head, corner models and A bat.

### Native body channels and collision — 2026-10-14

The previous body-channel frontier is resolved, and native gravity motion,
player-body collision and stand-alert next-key paths run. Both Release and
Debug live actor-frame experiments now stop at the uninstalled type 0x8E
entityStateWalkAlert (getter 0x000A55F0). Its lifecycle and update dumps are
exported for the next translation. The final scene keeps actor updates held
until that action is covered. See NATIVE_GAMEPLAY_FRAME.md's "Body channels,
motion and collision checkpoint" for branch coverage and validation limits.

Release/Debug builds, body/dialogue-resource checks and CTest 6/6 in both
pass. New motion checks cover root/velocity branches, freeze, phase writes
and backward limiting. Movement and combat remain unaccepted.

Release opening checks also pass bat animation, fresh A input and the next
script gate. Scene startup/teardown checks pass in both builds. The final
720-frame `cache/player_opening_check.bmp` readback was inspected and retains
the tutorial text, backdrop, head, corner models and A-button bat.

## Walk-alert actions and floor collision (2026-10-14)

Type 0x8E (entityStateWalkAlert) is translated and installed. Animation
events now reach scene players as native message 7. The actor collision slots
take the caller's shape mask and excluded-flag mode in retail order, so bodies
stand on the hall floor. Mina start samples its cycle root into player +0xF0.
The imps' live update is clean. Cooper stops at player mind request slot
`0x0007D360`, so the actor-update hold stays. Details and the next step are in
`docs/NATIVE_GAMEPLAY_FRAME.md` ("Walk-alert, animation events and floor
collision checkpoint").

## Actor updates live (2026-10-14)

`scene_tick_pause_counter` now runs the actor tree (`0x00123020`) and the
`+0x934` list (`0x00122FE0`) every frame. The mind target registry
(`DAT_00545988`) is a view over the live-attachment list. Cooper's player
mind request slot `0x0007D360` has its full control context. Base, area,
emitter and trigger actors register their update slots. See
`docs/NATIVE_GAMEPLAY_FRAME.md` ("Actor updates live").
