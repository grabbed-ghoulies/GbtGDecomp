# First playable scene

Target: `aid_script_ghoulies_chapter1_scene1_2playcam`.

This scene is not playable yet. Rendering its background and advancing its
camera do not verify the retail dialogue, actor, or gameplay behavior.

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
