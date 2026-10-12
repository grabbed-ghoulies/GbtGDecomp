# Retail code oracle

These checks run a reconstructed function and the retail machine code for the
same address on the same inputs, then compare every output word. The retail
code runs in [Unicorn](https://www.unicorn-engine.org/) from bytes exported
from the user's own `default.xbe`; those bytes stay in `cache/oracle/` and are
never committed.

This complements the in-repo regression tests. The fixed vectors in
`tests/shader_validation_serializer_tests.cpp` were produced this way.

## Setup

1. Export the code range read-only (see `docs/GHIDRA.md`):

   ```text
   analyzeHeadless <ghidra_projects> GbtG -process default.xbe.original -readOnly -noanalysis
       -scriptPath tools/ghidra/ghidra_scripts -postScript DumpBytes.java cache/oracle 00160000-00185000 00185000-001A2000 00200000-00203000 00203000-00210000 00011000-00160000 00210000-00340000 00339000-0033A000
   ```

2. `pip install unicorn`, then run `tools/oracle/run.sh [seed] [cases]`.

The harnesses compile only the reconstructed modules they test, so they build
with g++ on Linux. Each oracle reports `mismatches 0` on success.

## Coverage

| Script | Retail functions |
|--------|------------------|
| `serializer_oracle.py` | `0x0018F8B7`, `0x001914E8` and their callees, including the retained frame words |
| `accessor_oracle.py` | `0x0018E414`, `0x0018E595`, `0x0018E5C3`, `0x0018E67C`, `0x0018E6BE`, `0x0018E833`, `0x0018E84E`, `0x0018E869`, `0x0018E884`, `0x0018E8A5`, `0x0018E8E4`, `0x0018E8C6`, `0x0018E8F8`, `0x0018E76F`, `0x0018F4F7`, `0x0018E7AF`, `0x00190370` |
| `channel_ops_oracle.py` | `0x0018E5E2`, `0x0018E604`, `0x0018E63F`, `0x0018E661`, `0x0018E69C`, `0x0018E6DF`, `0x0018E700`, `0x0018E741`, `0x0018E548`, `0x0018F355`, `0x0018F394` |
| `containers_oracle.py` | `0x001902D9`, `0x001903E1`, `0x0019042A`, `0x00190473`, `0x0019051C`, `0x00190337`, `0x00190B12`, `0x00190CA6`, `0x001909B1`, `0x0018F314`, `0x0018F1E5`, `0x001929B7` (XMemAlloc and the deleting destructor are hooked) |
| `registers_oracle.py` | `0x00190554`, `0x0019060B`, `0x00190656`, `0x00191E8F`, `0x00191F00`, `0x0018F4A9`, `0x0018F595`, `0x0018F86B`, `0x0018F436`, `0x0019010E`, `0x00190BDD`, `0x00192629`, `0x0018E4EB`, `0x0018E48C`, `0x0018E988`, `0x0018ED95`, `0x0018FF4B`, `0x0018E9DC`, `0x00191990`, `0x001919BD`, `0x0018F848`, `0x0018EB4C`, `0x0018EB96`, `0x00192469`, `0x0018FFC9`, `0x001906BB`, `0x0018FC2A`, `0x0018FD7D`, `0x001936B5` (via `0x00192469`), `0x0018EBEA`, `0x0018EDEE`, `0x001907E9`, `0x0018F815`, `0x00190D14`, `0x00190858`, `0x0018FED0`, `0x001908AE` |
| `trees_oracle.py` | Tree template instances `0x0019164F`, `0x00191714`, `0x00191806`, `0x001918CB`, `0x00191B1C` (join), `0x0019204A`, `0x0019214A`, `0x0019224A`, `0x0019234A`, `0x00192529` (erase), `0x001926FB`, `0x001927B9` (unregister), `0x001922BB`, `0x0019259A` (insert) and pass insertion `0x00191A82` |
| `sets_oracle.py` | `0x0018E78B`, `0x0018F56F`, `0x0018F549`, `0x0018F2AA`, `0x0018F2D3`, `0x0018F1BC`, `0x00190322`, `0x00190163`, `0x00190AA0`, `0x00190AC6`, `0x00192772`, `0x0019287F`, `0x001924DA`, `0x0018E94C`, `0x0018F174`, `0x001923BB`, `0x001927D1`, `0x00192830`, `0x001929E7`, `0x00192A2B`, `0x00190774`, `0x00190813`, `0x00190FFE` |
| `navigation_oracle.py` | `0x001104E0` with `0x001109E0`, `0x00110970` and `0x00110EF0` (heap allocation and reallocation hooked; needs `mem_00011000.bin` and `mem_00210000.bin`) |
| `overlay_oracle.py` | `0x0001DF70` with `0x0001E2A0`, `0x0001E770`, `0x0003DD50`, `0x0002AC70`, `0x0001E3E0` and `0x0005C1A0` (needs `mem_00011000.bin`, `mem_00160000.bin`, `mem_00200000.bin`/`mem_00203000.bin` and `mem_00210000.bin`) |
| `update_oracle.py` | Player update helpers `0x00025A30`, `0x00110C90` (with `0x00036AF0`, `0x00110BF0`, `0x00110EF0`, `0x001108B0`), `0x00019520`, `0x0002C980`, `0x0002B450` (with `0x001248D0`) and body sphere pushes `0x00025F00`/`0x00026100` (with `0x00056330`, `0x00040FB0`, `0x0001FE00`, `0x001226C0`, `0x00122480`, `0x00122C10`, `0x000662A0`); heap, `HeapFree`, receivers and probe vtables hooked; needs `mem_00011000.bin`, `mem_00200000.bin`/`mem_00203000.bin`, `mem_00210000.bin` and `mem_00339000.bin` |
| `contact_oracle.py` | Body contact walk `0x0013F2C0` with `0x0013ED20`, `0x0013EE10`, `0x0013EE80`, `0x0013F7B0`, `0x00122B00`, `0x001229F0`, `0x000FCC90`, `0x000FC6C0`, `0x0013FB50`, `0x0013EBE0`, and the player callback `0x0001F8C0`; same memory files as `update_oracle.py` |
| `strategy_goals_oracle.py` | Strategy parent message slot `0x0002BC10` (with `0x00138C80`, `0x00138B40`, `0x001387F0`, `0x00028420`, `0x001392D0`, `0x00138E40`, `0x00138EF0`, `0x00028C30`, `0x00042070`) and the strategy goal batch `0x0002C130`, `0x00138A30`, `0x00139010`, `0x00138F60`, `0x001391D0`, `0x00135790`, `0x00135540`, `0x00135AB0`, `0x00135B40`, `0x00135420`, `0x00139E90`, `0x0013A0F0`, `0x0013D400`, `0x0002A080`, `0x000285B0`, `0x00028500`, `0x00028580`, `0x00124800`, `0x0002B540`, `0x00028150`, `0x0013E6B0`, `0x0003AC90` as a call-sequence comparison: outside callees and virtual slots are logging stubs on both sides; needs `mem_00011000.bin` and `mem_00210000.bin`. Build `tools/oracle/strategy_goals_harness.cpp` with `src/engine/entities/player_strategy_goals.cpp` only |
| `player_mind_messages_oracle.py` | Player mind message slot `0x0007F1E0` with `0x000357B0`, `0x00049DB0` and `0x00061960` as a call-sequence comparison: the 29 outside callees and the actor message slot are logging stubs on both sides; needs `mem_00011000.bin` and `mem_00210000.bin`. Build with `cache/oracle/build_mind_messages.cmd` (`tools/oracle/player_mind_messages_harness.cpp` with `player_mind_messages.cpp` and `camera_pulse.cpp` only) |
| `player_strategy_oracle.py` | Player strategy overrides `0x000BEE10`, `0x000BEF40`, `0x000BF2B0` with `0x000BEDB0`, `0x000BF4D0`, `0x0013A490`, `0x000BF600`, `0x000BF6A0`, `0x000D0690`, `0x0013FF30`/`0x0013FF70`, the brew meter `0x0012D090`/`0x0012D4A0`/`0x0012D4E0`/`0x0012D510` and the real axis writers `0x00108FC0`/`0x001090A0`/`0x00109180` as a call-sequence comparison; other callees, including `0x00128DF0`, are logging stubs. Build with `cache/oracle/build_player_strategy.cmd` (links `player_strategy.cpp`, `brew_meter.cpp` and `dialogue_input.cpp` with `/FORCE:MULTIPLE /Ob0`, so the harness stubs replace the input reads) |
| `script_nearest_oracle.py` | Nearest script actor query `0x00128DF0` and path gate `0x001295E0` with the real filter `0x00129420` (flags 0 and `0x4000`) and the real square root `0x00152B30`; positions, the path walk and the predicate are logging stubs. Needs `mem_00011000.bin`, `mem_00203000.bin` and `mem_00210000.bin`. Build with `cache/oracle/build_script_nearest.cmd` |
| `mind_request_update_oracle.py` | Player mind request update `0x0007D360` (mind vtable +0x14) as a call-sequence comparison; `0x0001EF70`, `0x00027650`, body vtable +0x38/+0x3C, `0x0007D130`, `0x00136620` and `0x000295E0` are logging stubs. Build with `cache/oracle/build_mind_request_update.cmd` |
| `ctor_oracle.py` | `0x00192DF1`, `0x001935E6`, `0x001934A5`, `0x00190E27`, `0x00192B84`, `0x00194311` (with `0x00193C2A`), `0x00193EE2`, `0x0019149A`, `0x00193566`, `0x00190F87`, `0x00191417`, `0x001911A8`, `0x00191D2D`, `0x00193649`, and `0x00193EBE` (normal paths; Linux uses SEH-shimmed copies built by `run.sh`, Windows can compile the original modules with MSVC into `cache/oracle/ctor_harness.exe`; needs `cache/oracle/mem_00160000.bin`) |

The update and contact harnesses were built on Windows with MSVC from a Developer
Command Prompt (they are not yet in `run.sh`):

```text
cl /std:c++20 /O1 /EHsc /Gy /Iinclude /Isrc /Isrc/engine tools/oracle/update_harness.cpp src/engine/entities/body_collision.cpp src/engine/entities/entity_collision.cpp src/engine/entities/player_update.cpp src/engine/navigation/floor_tracking.cpp src/engine/navigation/cell_registration.cpp src/engine/navigation/navigation.cpp src/engine/navigation/occupancy.cpp src/engine/entities/actor_transform.cpp src/engine/entities/shape_transform.cpp src/engine/entities/entity.cpp src/gbtg/animation/anim.cpp src/engine/camera/entity_camera.cpp /Fe:cache/oracle/update_harness.exe /link /OPT:REF
cl /std:c++20 /O1 /EHsc /Gy /Iinclude /Isrc /Isrc/engine tools/oracle/contact_harness.cpp src/engine/entities/body_contacts.cpp src/engine/entities/body_resources.cpp src/engine/entities/segment_records.cpp src/engine/entities/entity_collision.cpp src/engine/navigation/navigation.cpp src/engine/navigation/occupancy.cpp src/engine/entities/actor_transform.cpp src/engine/entities/shape_transform.cpp src/engine/entities/entity.cpp src/gbtg/animation/anim.cpp src/engine/camera/entity_camera.cpp /Fe:cache/oracle/contact_harness.exe /link /OPT:REF
```

Then run `python update_oracle.py [seed] [cases] [kind]` and `python contact_oracle.py [seed] [cases]`
from `tools/oracle`.

An oracle is evidence for a translation, not a replacement for reading the
dump: inputs must reach every branch, and functions that call heap, D3D or
other host boundaries need those calls hooked rather than skipped.

`body_joint_oracle.py` compares joint target selection `0x00073C30` and angle
easing `0x00101280`, including the genuine direction, world-euler and retail
math callees. It uses the existing code/data exports listed in its emulator
setup, and requires Python with Unicorn. Build the Release `gbtg` target first,
then build the host harness with `tools/oracle/build_body_joint_harness.ps1`.
The script reuses the game's current Release engine objects (excluding
`main.obj`) and reads its linker dependencies from the generated project.
It currently supports the Visual Studio x64 generator.

```powershell
& tools/oracle/build_body_joint_harness.ps1
python tools/oracle/body_joint_oracle.py 1 800
```

The second argument counts cases per routine, so this runs 1,600 comparisons.
Tracking and untouched bytes compare exactly; output floats use a small
rounding tolerance and treat NaNs as equivalent. Unlike the older scripts,
this oracle returns a failing exit code on any mismatch.

The constructor suite also covers owner selection `0x00193DF4`, owner linking
`0x00191DF3`, pass-owner installation `0x00191EC3`, register-owner replacement
`0x00193E52`, and the mutually recursive reader queries `0x00191BE1`/`0x0018F513`.
These cases include ignored link failures, query-mode mutation, null-owner
shortcuts, allocation failure, and the source-12 special-table lookup.

It also covers unwritten dependencies `0x00191CC0` and unused instruction
destinations `0x00191C74`, including both owner kinds, owner-chain writers,
output registers, absent destinations and destination reader graphs.

Operand reparenting `0x00192C9E`/`0x00192B20`, diagnostic unregister thunk
`0x0019275A`, and unused-channel cleanup `0x0019373D`/`0x0019379C` are covered
by chained kind-3 parents, retained readers, active/null/same-parent updates,
lazy operand allocation, varied opcodes/masks and already-missing orders.
The comparison includes parent pointers, sorted diagnostic keys, deletion
events, object count, channel orders and changed results. Lazy allocation
failure's null dereference and exceptional cleanup are not oracle-verified.

The destination graph cases cover `0x00190D50`, `0x001908FC`,
`0x0018F608`/`0x0018F6B7`/`0x0018F766`, their current-mask wrappers
`0x0019070B`/`0x0019072E`/`0x00190751`, masked writer assignment
`0x00191C0B`, and duplicate merging `0x00190D77`/`0x00191C47`.
They compare writers, destinations, mask bytes/vtables and operand orders,
including competing writers and destination aliases. For a focused run,
use `ctor_oracle.py 16 4000 destinations`. Invalid null-destination removals
with selected mask bytes are excluded because the emulator's mapped FS page
would hide the retail null dereference. Exceptional paths remain unverified.

Reader migration `0x00193A40` has a dedicated `moveunwritten` fixture that
compares reader parents, sorted diagnostic keys/counts and deleting-node
events after the two-pass mask traversal. Reproduce its focused check with
`ctor_oracle.py 17 3000 moveunwritten`; it also participates in mixed runs.
Owners have real diagnostic registries, and moves use the translated
reparenting functions. Exceptional cleanup is not CPU-verified.

Scalar splitting `0x00192F0C` has a `split` fixture covering both opcode
gates, output flag bytes, scalar order/marker copying, parent replacement,
destination aliases and writer transfers. It compares both instructions,
all destination channels, allocation calls and object registry keys.
Reproduce the focused check with `ctor_oracle.py 19 3000 split`; mixed seed
20 passes 6,000 cases. Allocation failures and exceptional cleanup are
outside this fixture.

The scalar-split callers `0x0019368D` and `0x00193F8A` use
`splitregister` and `splitpass` fixtures. These additionally compare the
program instruction tree's sorted keys and count. Focused seeds 21 and 22
pass 2,000 cases each; mixed seed 23 passes 6,000 cases. The CPU fixtures
start with one instruction; fixed lifecycle regressions also exercise a
tree containing several instructions and repeated/empty traversal.

The `dependencybuild` fixture covers the dependency construction cycle
`0x0019390D`/`0x001931E4`/`0x00193C44`, lookup and priority/list
consumers, parent acceptance `0x001940AC`, migration `0x00194D12`,
detachment/rotation `0x00195BE1`/`0x00195CA0`, register assignment
`0x00196814`/`0x001969C0`/`0x00196A14` and availability
`0x00197075`. Four-instruction acyclic graphs compare cache addresses and
keys, allocation calls, predicate results, list values and owner/register
updates. Migration/rotation uses empty writers. Allocation failure and
exceptional paths remain outside the fixture. Focused
`ctor_oracle.py 29 3000 dependencybuild` and mixed seed 30 (6,000 cases)
pass with zero mismatches.

`dependencybuild` now also exercises owner merging, move rewriting, readiness,
instruction merging/destruction, packed instruction and pass construction,
optimization worklists, scheduling/register assignment and the complete
`0x0019723C` validation/serialization/cleanup path. Its packed programs include
input owners and register-dependent move chains. It compares the surviving
instruction count, scheduled instruction order/opcodes/operand parents, packed
words, registry counts and allocation calls with retail.

The allocator reserves `0x100` retail bytes per allocation (the derived pass needs
`0x98`); host allocations use 512-byte blocks. The first serializer call receives
three explicitly initialized retained frame words. Set/pass/list node callbacks
model the heap boundary, while instruction/operand/owner destruction and registry
removal execute their translated bodies.

Reproduce the complete normal-path comparison with
`ctor_oracle.py 41 1500 dependencybuild`; register-move coverage was added with
`ctor_oracle.py 43 300 dependencybuild`. These checks pass with zero mismatches.
The expanded fixture also passes seed 44 (1,500 focused cases) and seed 45
(1,500 mixed cases), with zero mismatches.
Allocation failures, C++ exception propagation and the validator's conservative
retry path are not compared by these fixtures. Linux shims also suppress the
outer exception handler; they are used solely for normal-path comparisons.

The body frame oracle uses the actual reconstructed body methods and retail
machine code. Build `tools/oracle/build_body_joint_harness.ps1`, then run
`body_frame_oracle.py 1 400` with Unicorn on PYTHONPATH. Movement, channel
blends, latch/pulse timers and four-phase wash each receive 400 cases. The
triangle/sphere/cylinder separation modes additionally compare the return byte,
point and contact writes. Random command timers, side counters and signed
direction push, damage pulse reduction and health subtraction bring the batch
to 5,200 cases. Float comparisons permit the harness's small rounding tolerance;
other body bytes compare exactly. Null scene recipients isolate scalar methods;
this check does not establish live spawning or concrete scene ownership.

`body_action_oracle.py 1 400` uses the same body-joint harness and compares
3,600 direct/range/paired action selector cases against original machine code.
It includes signed row counts, tag lookup/fallback, invalid attachment classes,
host range gates and retained pair output words. It does not cover table-gated
selectors or live factory ownership.

`body_damage_oracle.py 1 400` compares 2,400 interpolation, class lookup,
actor lookup and damage pulse/pause selector cases with original machine code.
Equal endpoints, descending ranges, NaNs, infinities and integer conversion
indefinite results are included. It does not establish live scene ownership.

`spawn_candidates_oracle.py 1 400` compares 400 navigation candidate cases
with retail 0x0010F7F0, using actual rectangle/cell routines and bridging only
heap allocation/reallocation. Candidate order, filters, row writes and growth
are checked; path/occupancy flags and live actor spawning are not covered.

`contact_direction_oracle.py 1 400` compares retail 0x000FCA60, 0x00021010 and 0x000FC3F0
without callee bridges. It checks zero, short and unrolled pools, header-stride
averaging, first-contact sphere projection, null contacts and nonfinite inputs.
It also covers combined triangle/sphere separation. All 1,200 cases pass;
this does not establish live spawning.

`contact_sweep_oracle.py 1 400` compares quadratic selection, point/edge sweeps,
sphere sweeps, triangle intersection and triangle sweeps with genuine math callees.
All 2,400 cases pass, including degenerate geometry and nonfinite inputs.

`player_collision_oracle.py 1 400` checks the main player collision update
against retail with empty world lists and a real contact pool. All 400 cases
pass, comparing complete body bytes, flags and actor position/stamp; no game
callee is bridged. Scene contact branches and live ownership are not covered.

`player_mind_helpers_oracle.py 1 400` checks alert thresholds, minimum-distance
recording and nearest-position retention, including nonfinite inputs. All 1,200
cases match retail exactly without game callee bridges. This does not verify
the complete mind dispatcher or factory ownership.

`attachment_lookup_oracle.py 1 400` checks five attachment request table searches
against actual retail machine code, including unsigned keys, absent/matching keys,
empty tables and the category-4 primary attachment gate. It does not verify
request acceptance or live attachment ownership.
All 2,000 cases pass with zero mismatches and no game callee bridges.

`player_input_tracking_oracle.py` compares complete 0x00109810 state updates
against retail machine code, including the actual square-root routine. The
`inputtracking` harness protocol returns all six tracking words. The current
2,000-case run has zero mismatches.

`player_input_gesture_oracle.py` compares 0x0007D5A0 and 0x0007D460, including
real square-root and atan2 callees. Its `inputgesture` protocol returns all eight
tracking words. These checks exercise timer boundaries, state transitions and
special float values; they do not establish live player factory acceptance.

`imp_strategy_oracle.py [seed] [count] [entry]` checks the imp fire strategy
(type 0x46) and the shared strategy helpers it calls: the DAT_0050E284 target
queue (0x0002B910, 0x00112970, 0x00112A70, 0x00112B40, 0x0002CC30), target
selection (0x00029AC0, 0x00029BA0, 0x00139AA0, 0x00139C70), target checks and
reactions (0x00138410 through 0x001399A0, 0x0013A430, 0x0013A4F0, 0x000F4870,
0x000F4E10), roaming requests (0x00138310, 0x00139FA0, 0x0013A0A0), every class
method from 0x000B0B90 to 0x000B4F50 plus 0x000C2D90, the angle helpers
0x00100B80/0x00100D00 and model lookup 0x000409D0. Other callees are logging
stubs on both sides. Build the harness with `cache/oracle/build_imp_strategy.cmd`.
Seeds 3, 7 and 11 (2,000, 6,000 and 3,000 cases) have zero mismatches, and ten
mutants across the five modules are each detected. Live imp spawning is not
covered: the scene still has to supply the bindings.

`fx_emitter_oracle.py [seed] [count] [entry]` checks the FX anim emitter (type
0xD): sizes, constructor, destructor, message, launch, flight, update, player
scan, collection, visibility and draw (0x00109DB0 through 0x0010A6D0), the burst
parameter builder 0x00069590, the actor queries 0x0003D860, 0x00040550,
0x00041CB0, 0x00042560, 0x000425D0 and 0x001205F0, the frustum test 0x00159AD0,
and the intro notice pair 0x0013FF00/0x0013FF70. The real retail sin, cos and
square root are linked; other callees are logging stubs on both sides. Build the
harness with `cache/oracle/build_fx_emitter.cmd`. Seeds 7, 11, 17 and 19 (3,600,
3,600, 3,800 and 1,900 cases) have zero mismatches. Each of twelve mutants is
detected, including exact-boundary cylinder distances and NaN frustum planes.
The powerup award (0x00123E70) is a stub here because its tree is not translated.
