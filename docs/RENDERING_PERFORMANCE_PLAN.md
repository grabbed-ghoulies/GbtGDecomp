# Native rendering performance plan

## Objective and constraints

Target steady 60 FPS (16.67 ms per frame) at 1920x1080 on the user's
machine. This is a proposed target, not a measured current capability.
Keep attract, menus, transitions and cutscenes at their existing performance.
Record the adapter, build configuration and display settings with every result.

Native game code continues recording retail draws through SceneD3DDevice.
Optimize the D3D11 presenter and resource boundaries. Preserve draw order,
shader behavior, render targets, viewport/depth conversion, blending, alpha
test and resource lifetime. Do not replace native behavior with the older host
scene renderer or add gameplay logic to game.cpp.

The current actor-update hold remains a reconstruction limitation. Rendering
benchmarks must label it; they cannot establish full gameplay performance.

## Evidence and open questions

scene_d3d_present.cpp currently interprets each indexed draw's vertex program
on the CPU. Its shaded-vertex map lasts for one draw, and it expands indexed
triangles into a new vertex vector. gfx_draw_native uploads that vector using
WRITE_DISCARD for every draw. The pixel stage uses a generic GPU register
combiner interpreter. State objects and decoded textures already have some
caching; measure cache misses before adding more caches.

The cutscene prop path uses gfx_draw_mesh, and frontend paths use mesh/sprite
helpers. They demonstrate reusable presentation resources, but some owners are
legacy host implementations (see LEGACY_HOST_CODE.md). Reuse presentation
techniques, not their gameplay logic. Relative CPU/GPU contributions and the
cause of the resolution-sensitive slowdown have not been profiled yet.

## 1. Measure before changing execution

Add an optional renderer profiling mode with bounded overhead:

- CPU timings for native draw recording, declaration decoding, vertex-program
  execution, topology expansion, texture preparation, D3D11 submission and
  presentation. Separate game update time from rendering time.
- Asynchronous D3D11 timestamp/disjoint queries for GPU time, without waiting
  for the current frame's results. Report presentation waits separately.
- Counters for draws, unique/input/output vertices, shader instructions,
  uploaded bytes, allocations, texture uploads and cache misses.
- Warm-up followed by median, p95 and p99 frame times. Report loading and
  shader compilation separately from steady rendering.

Benchmark Release at 640x480, 1280x720 and 1920x1080 using the same scene
frames: opening hall, tutorial, attract, a menu, a cutscene and transitions.
Capture reference images at fixed native times. Use controlled recorded-command
replay where useful to isolate rendering from unfinished actor updates; retain
the referenced resources and label replay results explicitly.

Deliverable: reproducible benchmark command, timing breakdown and a ranked
list of measured bottlenecks. Do not claim CPU or GPU dominance from wall-clock
capture duration alone.

## 2. Retain geometry and indices

Extend the presenter/backend interface to submit indexed geometry instead of
duplicating a vertex for every triangle index. Preserve native topology; if
conversion is required, cache the converted index buffer.

Cache parsed vertex declarations and shader instruction decoding. Reuse
scratch storage and replace the per-draw hash map with bounded indexed storage
where the measured vertex range makes that suitable.

Use persistent GPU buffers for proven immutable resources and a reusable
dynamic upload arena for changing data and inline arrays. Resource identity
must include lifetime/generation and content revision, not just an address.
Wire invalidation to actual creation, write/lock, replacement and destruction
boundaries. Keep resources with untracked writes dynamic until those writes
are covered. Do not cache transformed vertices across changing constants.

Deliverable: lower allocation/upload/expansion costs with image parity and
correct unload/reload behavior. Rebenchmark before advancing.

## 3. Execute native vertex programs on the GPU

Translate supported NV2A microcode into HLSL once per shader/declaration and
bind the original vertex/index streams. Upload per-draw constants through
constant buffers. Move the existing clip-position conversion into the GPU
path without changing homogeneous coordinates, depth or viewport semantics.

Retain the CPU interpreter as a reference and as an explicit fallback for
programs the GPU translator cannot yet support. Unsupported retail operations
must still raise; fallback is not permission to invent shader behavior.

Test register addressing, swizzles, masks, simultaneous instruction outputs,
arithmetic edge cases and all boot shader programs. Compare GPU outputs and
fixed-frame images against the interpreter with stated numeric tolerances.
Cache compiled shaders by content and translator version; compilation stays
outside the steady draw loop.

Deliverable: eliminate repeated CPU vertex interpretation for covered native
draws, with measured improvement and a reported fallback count.

## 4. Specialize pixel shaders if profiling warrants it

Generate HLSL for each actual D3DPIXELSHADERDEF's structural configuration
instead of interpreting combiner fields for every pixel. Keep runtime color
constants separate from shader structure so constant changes do not recompile
shaders. Cache by structural definition and translator version.

Preserve stage dependencies, texture modes, input mappings, bias/scale/clamp,
final combiner, alpha test and blending. Keep the generic shader as the
reference/fallback. Validate the boot definitions and live material variants.

Deliverable: lower measured GPU time, especially at 1080p, with image parity.
Skip or defer this phase if the measurements show it is not material.

## 5. Reduce remaining submission overhead

Use profiling to remove redundant state and constant-buffer bindings, reuse
dynamic buffers, and consolidate adjacent compatible submissions when their
ordering semantics permit it. Do not reorder translucent geometry, shadows,
clears or render-target operations. Avoid adding synchronous readbacks to
normal frames; capture synchronization is measured separately.

Prefer a faster correct path at the requested resolution. Resolution scaling
can be an explicit user option later, not a substitute for these changes.

## Acceptance for each phase

Build and test Release and Debug; measure performance in Release. Run existing
render, opening, scene-start, frontend, transition and cutscene checks that
exercise changed interfaces. Inspect fixed-frame screenshots, including
transparent edges, shadows, tutorial render targets and animation poses.
Exercise resource mutation and scene unload/reload where caches are changed.

Compare the same warm benchmark scenarios to baseline. Record median/p95/p99
CPU and GPU times, cache/fallback counts and uploads. Require no material
regression in attract, menus or cutscenes. Report remaining frame-budget
misses and visible mismatches plainly. Full gameplay acceptance follows only
when native actor updates are reconstructed and enabled.

## First implementation step

Implement phase 1 instrumentation and benchmark capture. Choose the first
optimization from those measurements; geometry/index retention and GPU vertex
execution are the leading candidates based on the current code.


## Implementation progress — 2026-10-14

Status: implemented presenter optimizations and instrumentation; full comparative
acceptance remains incomplete. Work was stopped for the incoming usage limit.
The actor-update hold remains unchanged. No retail assets were edited, staged or
committed. These timings do not establish full gameplay performance.

### Implemented

- Optional `--render-profile file.csv`, plus explicit `--width`, `--height` and
  `--vsync` overrides. Frame CSVs contain tick, presenter, declaration, CPU vertex,
  topology, native texture, submission, presentation, shader compilation,
  recording and existing host-rendering timings. Nested timings must not be added.
- Bounded asynchronous D3D11 timestamp/disjoint queries use DONOTFLUSH and skip
  samples when the ring is full. `file.csv.gpu.csv` distinguishes whole-frame GPU
  timestamp spans from sampled summed draw durations (one frame in 32). Frame
  spans include GPU idle gaps during CPU submission; they are not GPU busy time.
- Draw/vertex/instruction counts, native geometry/index/texture upload bytes,
  dynamic buffer creation, state/declaration/pixel cache misses, vertex fallback
  coverage, total backend draws and main-thread C++ new/new[] allocation counts.
  Allocation counts exclude malloc, native heap calls, driver and worker heaps.
  Upload bytes currently exclude constant buffers and some legacy host uploads.
- Startup game/device initialization and screenshot readback/capture costs are
  logged separately. Profiled headless runs use neutral input and fixed 1/60 time.
- Indexed native geometry retains unique vertices and triangle order. Persistent
  dynamic vertex/index arenas use DISCARD on wrap and NO_OVERWRITE otherwise.
  Raw resource bytes and constants are reread for each draw: untracked resource
  writes are deliberately kept dynamic rather than cached as immutable geometry.
- Declaration caches use declaration content; topology caches use primitive/count.
  Bounded generation-stamped u16 index scratch and reusable vectors replace the
  per-draw shaded-vertex hash map and duplicated triangle vertex uploads.
- GPU HLSL translation caches by program content and device lifetime. Supported
  MAC arithmetic, direct input/constant ARL, validated relative constant reads,
  swizzles, masks, paired outputs and MOV/RCP/RCC/RSQ ILU operations execute on GPU.
  Constant writes, temporary-derived ARL and EXP/LOG/LIT retain CPU interpretation.
  Operations outside the CPU interpreter's supported set still raise. Native
  pixel texture modes outside NONE/2D and more than eight combiners now raise.
- NV2A stream format decoding still runs on CPU; GPU input buffers contain decoded
  registers rather than binding original packed resource streams directly.
- Pixel shaders specialize structural words using the same generic HLSL equations;
  colour constants remain dynamic. The generic pixel shader remains available via
  `--render-reference` and as a compilation fallback. Compilation happens on cold
  first use and is measured; it is absent from the measured warm hall draw loop.
- Contiguous immutable state snapshots replace per-state tree allocations. Native
  batches avoid restoring the shared pipeline between adjacent native draws,
  restoring it at clears, target boundaries and batch end. Draw order is unchanged.
- `--cpu-vertex` retains indexed uploads/specialized pixels but selects the CPU
  interpreter. `--render-reference` selects expanded uploads, CPU vertices, generic
  pixels and per-draw pipeline restoration; recording snapshot changes remain.
- `--render-replay-after N` freezes updates after frame N and replays captured native
  commands. The Game retains their resource owners until replay ends. This excludes
  host-only drawing owners and must be labelled native-command replay.

### Reproduction

Run from the repository root with an installed Python and CMake:

```powershell
cmake --build build --config Release --target gbtg gbtg-transition-tests --parallel 8
python tools/render_benchmark.py --label final
python tools/render_benchmark.py --label reference --reference
python tools/render_benchmark.py --label cpu-vertex --cpu-vertex
# Resume completed matrix cases after an interrupted run:
python tools/render_benchmark.py --label final --resume
# Select a subset; baseline generic pixels need a longer timeout:
python tools/render_benchmark.py --label reference --reference --cases hall tutorial --timeout 900
build/Release/gbtg.exe --validate-render-boundary
build/Release/gbtg.exe --headless --scene chapter1_scene1_2playcam --frames 300 --validate-gpu-vertices
```

The runner executes six scenarios at all three requested resolutions serially,
records commands/metadata/results in `cache/render_perf/<label>_results.json`,
and writes raw CPU/GPU CSVs, logs and fixed-frame BMPs beside it. Hall captures
frame 300 (warm window 260–299), tutorial frame 720 (600–719), attract/menu/book
cutscene frame 300 (120–299), and the opening transition interval through frame
180 (all frames; explicitly nonsteady). Attract/menu/book owners are existing
frontend/cutscene owners, including legacy presentation; none was replaced.
GPU draw-time sample counts are small; do not infer stable p99 busy times from
one sample. Offscreen benchmarks have no swap-chain presentation wait. A separate
windowed presentation benchmark is still required.

### Measured optimization sequence

Machine: NVIDIA GeForce RTX 4070 Ti, D3D11 hardware, Release, 1920x1080,
offscreen, vsync 0, fixed 1/60 native time, actor updates held. The instrumentation-
only baseline executable is retained locally at
`cache/render_perf/baseline_gbtg.exe`. Compare the same hall frame/window and
40 CPU samples; nearest-rank median/p95/p99 below are milliseconds.

- Instrumentation-only baseline: 1264.25 / 1285.25 / 1297.73. CPU vertex execution
  median 22.812 ms; submission median 1211.870 ms. Whole-frame GPU timestamp median
  approximately 1253.7 ms, including submission gaps. Native geometry uploads
  9,028,080 bytes, 1,286 native draws, 20,668 unique / 112,851 expanded vertices.
- Indexed upload + declaration caching + structural pixel specialization:
  43.084 / 45.043 / 45.653. Submission median 1.130 ms; geometry/index uploads
  2,104,844 bytes. This frame's BMP is byte-identical to the baseline.
- GPU programs with one validated address load: 24.677 / 25.685 / 27.017;
  1,194 GPU / 92 CPU native draws.
- Multiple direct address loads / GPU skinning: 17.470 / 18.963 / 20.124;
  1,286 GPU / 0 CPU native indexed draws. Recording median 8.260 ms.
- Contiguous state snapshots + command capacity reuse + native pipeline batching:
  7.898 / 8.775 / 9.761. Recording median 0.876 ms; native geometry/index uploads
  5,742,412 bytes (decoded GPU inputs are larger than CPU shaded vertices).

Raw evidence uses `<phase>_hall_1080.csv`, `.csv.gpu.csv`, `.log` and `.bmp`
under `cache/render_perf/`, with phase names baseline, optimized, gpu_relative,
gpu_skin and state. These establish a large hall improvement and a short warm
window under 16.67 ms, not acceptance of every scenario or unrestricted gameplay.

### Validation completed

- Release and Debug builds, including transition/movie regression targets.
- Release CTest: 6/6 passed. Release title, title-a, goop, UI, player-opening,
  dialogue-resources, scene-models, scene-spawn, scene-start, book/map transition
  and movie checks all exited 0 (`cache/render_release_regressions.json`). The
  transition traversed scene0_1book -> front-garden map -> scene1_1book; scene
  teardown reported zero factory objects/addresses.
- `--validate-render-boundary` passed vertex/index mutation, texture serial update,
  release/address reuse, constants and device unload/reload. RCC/RSQ fixtures
  include zero, negative zero and very small finite input. Logs:
  `cache/render_boundary_validation.log`.
- Opt-in stream-output readbacks compared up to 16 actual vertices per GPU program
  against CPU clip positions, diffuse/specular and all four UVs. Hall passed using
  `5e-5 + 5e-5 * abs(reference)` tolerance; see `validated_hall.log`. This checks
  live paired operations and masks. It is excluded from performance runs.
- Hall reference and optimized captures were visually inspected. Indexed/pixel
  specialization is byte-identical. GPU execution changes 26 pixels out of
  2,073,600 at frame 300, maximum channel delta 72 and mean channel delta 0.0000686.
  These sparse edge differences are reported, not treated as exact pixel parity.
  Existing black patches and held poses remain visible.

### Remaining acceptance and next work

- Complete the reference and per-phase six-scenario/three-resolution matrix;
  the optimized matrix is recorded separately below as far as completed.
  Attract/menu/cutscene non-regression has not been established by matching
  performance baselines, even though their Release regressions pass.
- Inspect all matrix captures, especially tutorial targets, transparent edges,
  animation poses and transition frames; compare to same-frame reference images.
- Finish Debug CTest and runtime regression execution, and rerun validation after
  the final strict-arithmetic/unsupported-pixel checks. Verify replay independently.
- Add accounting for constant/legacy uploads and native/driver allocations if
  complete upload/allocation coverage is needed. Tick-minus-rendering is a
  residual, not a fully instrumented game-update-only measurement.
- Bind original packed streams on GPU where formats permit, prewarm new shaders
  outside first-use draws, and consider immutable-resource retention only after
  every resource mutation/lifetime boundary is tracked.
- Broaden numeric fixtures (NaNs, infinities, more boot variants and unsupported
  programs). CPU interpreter and generic pixel references remain available.
- Confirm windowed presentation/vsync waits and longer warm samples before a
  whole-machine steady 60 FPS acceptance claim. Do not change the actor-update hold.

### Final optimized matrix collected before stopping

All 18 runs exited 0. Results: `cache/render_perf/final_results.json`.
Values are nearest-rank median / p95 / p99 frame milliseconds, ordered 640x480,
1280x720, 1920x1080. These are optimized results, not non-regression proof.

- hall: 5.721 / 6.634 / 7.348; 5.617 / 6.273 / 6.745; 5.820 / 6.957 / 7.122.
- tutorial: 3.115 / 3.998 / 4.163; 3.056 / 3.987 / 4.324; 2.855 / 3.722 / 3.928.
- attract: 0.014 / 0.147 / 0.376; 0.015 / 0.117 / 0.577; 0.018 / 0.120 / 0.299.
- menu: 0.273 / 3.057 / 4.826; 0.274 / 5.047 / 7.442; 0.273 / 9.303 / 11.296.
- cutscene: 0.281 / 0.674 / 1.113; 0.476 / 1.075 / 1.611; 0.597 / 1.586 / 3.356.
- transition: 1.512 / 7.292 / 140.453; 1.487 / 7.116 / 137.886; 4.218 / 7.576 / 140.761.

Final frame-300 hall comparison: maximum channel delta 72, changed pixels 26, mean channel delta 6.864068930041153e-05.

## GBTG_OPTIMIZE (2026-10-20)

`GBTG_OPTIMIZE` is a CMake option, OFF by default. OFF builds the retail presenter path. ON
compiles the host presenter optimizations below. Native code, the retail call order and the
recorded `SceneD3DDevice` stream are the same in both builds. The optimized code is in each
`#if GBTG_OPTIMIZE` branch and the retail code stays in the matching `#else`, so neither path is
removed. `src/engine/graphics/render_optimize.h` defaults the macro to 0. `--render-profile`
logs `profile optimize=on|off`, and the frame CSV has an `optimize` column.

Build both into separate trees and compare them:

    cmake -B build-opt -DGBTG_OPTIMIZE=ON
    cmake --build build-opt --config Release --target gbtg
    python tools/render_benchmark.py --exe F:\...\build\Release\gbtg.exe --label retail --resolutions 1280x720
    python tools/render_benchmark.py --exe F:\...\build-opt\Release\gbtg.exe --label opt --resolutions 1280x720
    python tools/compare_screenshots.py cache/render_perf/retail cache/render_perf/opt
    python tools/compare_render_benchmarks.py retail opt

Pass `--exe` an absolute Windows path. Copy `build/Release/saves` and `gbtg.ini` beside the
optimized exe; otherwise the menu case starts from different save data.

| Step | Where | Change under GBTG_OPTIMIZE |
| --- | --- | --- |
| 1 | `scene_d3d_device` | Draws share one immutable state snapshot (`shared_ptr`) until a state value actually changes. `decode_push` reuses its scratch. |
| 2 | `gfx_draw_native` | Within a native batch, skip binds that did not change: viewport/scissor, SRVs, samplers, blend, depth, raster, layout, topology, VS/PS. Skip GpuCB, material and 3 KB vertex-constant Maps when the contents are bitwise equal. Look up the vertex program once. |
| 3 | `scene_d3d_present`, `gfx_d3d11` | One-entry memos keyed by content or shared snapshot replace map searches: declarations, address validation, `apply_states`, `pixel_shader_words`, specialized pixel shaders. |
| 4 | `scene_d3d_present` | Adjacent quads with the same snapshot, texture and address modes go out as one triangle list. Inline and fixed draws reuse scratch vectors. |
| 5 | `native_texture` | A refreshed image of unchanged size goes to its existing texture through `UpdateSubresource`, not a new Texture2D/SRV. |
| 6 | `gfx_readback` | Persistent staging texture. |

Not done: removing the synchronous render-target round trip (copy → Map → swizzle →
re-decode). That needs proof that native code never reads the target's memory in the same
frame. Also not done: R16 index uploads, and GPU decoding of packed streams.

Results: Release, 1280x720, the six benchmark cases, labels `optflag2_retail` / `optflag2_opt`.
Every screenshot matches with maximum channel delta 0. Median / p99 frame ms, retail →
optimized:

- hall: 4.484 / 5.318 → 3.533 / 3.973
- tutorial: 4.539 / 5.831 → 3.589 / 4.134
- attract: 1.799 / 4.058 → 1.540 / 3.608
- menu: 0.866 / 5.328 → 0.766 / 5.379
- cutscene: 0.970 / 1.500 → 0.769 / 1.175
- transition: 16.809 / 76.764 → 13.572 / 75.287

The transition p99 is unchanged; it is dominated by loading and render-target readback. Main-thread
allocations per hall frame fell from 34373 to 29505.
