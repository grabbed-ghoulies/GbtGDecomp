# First playable scene

Target: `aid_script_ghoulies_chapter1_scene1_2playcam`.

This scene is not playable yet. Rendering its background and advancing its
camera do not verify the retail dialogue, actor, or gameplay behavior.

Latest live check (2026-10-10): the script holds at `+0x254`, type `0x6F`.
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
