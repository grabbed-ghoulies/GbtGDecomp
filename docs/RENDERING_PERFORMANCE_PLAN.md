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

Relative CPU/GPU contributions and the
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
- Cold shader variants are captured to `cache/shaders.gbtg` and compiled after the
  device initializes on later runs by the `shader_warmup` plugin (`docs/PLUGINS.md`),
  which only plugin builds (`GBTG_RETAIL_GAME_ONLY=OFF`) have; retail builds compile
  each variant on first use. Capture merges with the existing file so representative
  gameplay builds one warm-up set. The manifest stores the normalized 60-word pixel
  definitions and complete translated vertex programs; bump its translator version
  (`src/plugins/shader_warmup/shader_manifest.cpp`) when shader generation or key
  normalization changes.
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
| 7 | `scene_d3d_present`, `gfx_d3d11`, `gfx_vulkan` | Retained native geometry. A GPU-program indexed draw keeps its decoded inputs and index list across frames. An entry is reused only when its declaration, stream bindings, index bytes and every stream byte it decoded compare equal (`memcmp` against a copy), so writes, unloads and address reuse decode again; no mutation tracking is assumed. Address validation runs once per reused draw against its current constants (input-relative checks already passed for those exact inputs). `GfxNativeDraw::retained_geometry` names the entry; both backends keep its upload in an immutable buffer instead of the per-draw ring. Idle entries age out after 600 presents; caps 192 MB (presenter) and 256 MB (each backend). GPU-program draws share one scratch `XboxVertexProgramState` (they read only its inputs) instead of zeroing it and copying the 3 KB constants per draw; CPU-interpreter draws still start from a zeroed state with their constants. |
| 2 (Vulkan) | `gfx_vulkan` | Within a command buffer, a native draw pushes only the descriptors that differ from the last push (none when all match), and sets viewport/scissor and the stencil reference only when they change. Push descriptors stay in effect until overwritten or until the set is disturbed, and both they and dynamic state are undefined when a command buffer begins, so tracking resets there and after the debug UI pass (another pipeline layout). One-entry memos sit in front of the pipeline map and the specialized pixel shader map (the latter keyed by the 60 raw definition words, so the definition check's outcome is unchanged; a fallback entry is never memoised). Validation layer: 0 errors and the same warnings as the retail path. |
| 3 (Vulkan) | `gfx_vulkan` | `native_programs` (a `std::map` keyed by whole programs) is looked up through a memo keyed by the address of the program words and verified with `memcmp` against a copy of them. |
| 10 (Vulkan) | `gfx_vulkan` | A GPU-program draw copies into its 3 KB constant ring slot only the registers its program can read: every operand of constant kind reads the instruction's index, and any relative read takes the whole file. The descriptor range is unchanged; the rest of the slot keeps earlier ring contents, which the shader never reads. |
| 3 (presenter) | `scene_d3d_present` | Address-validation checks are kept per program (keyed by the address of its words, verified against a copy) instead of in one entry, so draws alternating programs do not rebuild them. |
| 8 | `scene_resource_memory` | `resolve`, `tail` and `find_address` try the four blocks recent lookups found before searching the map (`find_address` scanned every block; a draw alternates between its stream and index blocks). Blocks never move, resize or overlap, and the cache answers only for addresses and pointers strictly inside its block, so results are those of the map; `free` clears it. The member exists in every build so the class layout does not depend on the option. |
| 11 | `gfx_render_thread`, `gfx_dispatch`, `gfx_vulkan` | The Vulkan backend runs on a render thread. `gfx_init` swaps the dispatch table for a wrapper (`[Video] RenderThread=0` or `--render-thread 0` keeps it synchronous; null is never wrapped; D3D11 is wrapped since step 20). The game thread keeps the tick, recording and translation, and queues each `gfx_*` call in batches of up to 128 commands, copying what it points at: vertices, indices and program inputs into the batch's arena (a retained-geometry id is copied once and aged after 600 frames), the 60-word pixel definition, and only the constant registers the program reads (the step 10 range, now `vertex_program_constants.h`). Vertex programs are interned by the address of their words and verified with `memcmp`, so the backend's step 3 memo still sees one address per program. Images reach the backend through proxies: a new serial queues the new pixels as a command, so draws queued earlier keep the pixels they were queued with, and `gfx_release_sprite` is queued in order. Calls that return results (`end_native_target`, `read_native_target_depth`, `readback`, `readback_ui`, the first program query for a new program, the loading screen, warmup) drain the queue and run on the game thread. One frame may present while the next is recorded; `present_again` waits for the previous present, and plugin overlays (the debug UI) wait for it before their next build. Window resizes reach the render thread through atomics; profile counters are per thread and the render thread's are added to the next frame. |
| 12 | `scene_actor_services_draw`, `scene_d3d_present`, `gfx_d3d11` | A created texture's render target (not a scene target) ends on the GPU (`gfx_end_native_target_into`): the target is copied into the texture native draws bind for the texture's image, at a new serial, and into a staging texture, with no wait. The texture's data is written once, from the last such target, by `finish_render_target`, which the presenter calls after its last command (`SceneD3DPresentContext::finish`). No native code runs between a target's end and that point, so native code sees the same data as before; the presenter's own draws in between sample the GPU copy. The image takes the read-back pixels instead of being decoded again. Before a target on a different texture opens, the pending one is finished, since the backend keeps one staging copy. Vulkan since step 23; null returns false and keeps the synchronous path. The D3D11 context is flushed after each such copy, so the GPU starts on the frame's commands instead of receiving them at the final Map. |
| 13 | `gfx_d3d11` | `gfx_native_vertex_program_supported` finds the program through a memo keyed by the address of its words and verified with `memcmp` against a copy (the Vulkan step 3 memo), before the `std::map` keyed by whole programs. |
| 14 | `scene_d3d_present` | A vertex stream's resource lookup (the `SceneD3DResources` map, then `SceneResourceMemory::tail`) is remembered in a 16-entry cache that each presented command list clears. The registry and the memory do not change while the presenter runs. |
| 15 | `gfx_d3d11` | `compile_shader` keeps the bytecode D3DCompile returns in `cache/dxbc/<FNV-1a of source, entry, profile, strictness and a cache version>.dxbc`, beside the Vulkan backend's `cache/spirv`, and reads it back on later runs. An entry must start with `DXBC` and hold its own length; anything else compiles again. Bump `kDxbcCacheVersion` when the D3DCompile flags change. |
| 16 | `scene_d3d_device` | A recorded draw's stream bindings are a fixed sorted array (`SceneD3DStreamBindings`, 16 entries: a declaration names its stream in four bits) with the part of `std::map`'s interface the device and its readers use. The map cost a heap node per stream per draw, freed when the frame's commands were cleared. `draw_indexed` builds the draw in the command list instead of copying a temporary there. |
| 17 | `scene_d3d_present`, `scene_actor_services_draw` | A retained geometry entry's bytes are compared once per epoch, not once per draw. An epoch is a span of the presenter in which nothing writes scene memory: no native code runs inside `present_scene_d3d_commands`, so the only writers are the context's callbacks. A new epoch starts with each command list, after each back-buffer copy, and whenever a callback reports a write through `scene_d3d_memory_written` (a target's texels or depth words, a locked texture surface). What names the bytes (declaration, shape, index and stream pointers, strides) is still compared for every draw. |
| 18 | `gfx_d3d11`, `scene_d3d_present` | Per-draw lookups. Backend: the blend, depth-stencil, rasterizer and sampler objects and the four texture views are taken from the previous draw of the batch when their keys (for a view: image and serial) are the same, and `gfx_release_sprite` forgets the images; the specialized pixel shader is the previous one when the 60 definition words are the same (the generic fallback is not remembered); the vertex constants are compared and uploaded only over the registers the program reads (the step 10 range), the rest of the buffer being undefined after `WRITE_DISCARD` and never read; a program's words are compared against the memo's copy once per native batch. Presenter: a program's words are compared once per command list for its address checks; the eight most recent declarations stay decoded; the raw combiner register overrides are read from the snapshot in one ordered walk instead of 57 searches. |
| 19 | `scene_d3d_present` | The command two ahead of the one being presented is prefetched whole, and the sorted states of the next draw's snapshot. A recorded command is 3.5 KB and cold by the time it is presented. |
| 20 | `gfx_render_thread`, `gfx_d3d11`, `inspector_main` | The D3D11 backend runs on step 11's render thread too (`--render-thread 0` keeps it synchronous). Its immediate context is used by one thread at a time: the render thread, or the game thread while the queue is drained. The window and its messages stay on the game thread; the game's swap chain is windowed (borderless for full screen) and never resized. A texture target's end into an image (step 12) is queued instead of drained: D3D11 accepts every end whose size matches the open target, so the wrapper answers for it (Vulkan and null decline, now without a drain), and the render thread gives the image's proxy the image's new serial before copying the target into it. The proxy keeps its old pixels at that serial, as the backend's texture is the copy; a proxy of another size first takes the image's pixels. Draws on the thread carry the wrapper's interned program words, so `gfx_draw_native` finds their program through the step 13 memo by address (`draw_program`) instead of the whole-program map. The inspector, which calls the backend directly and resizes its swap chain, sets `render_thread = false`. |
| 21 | `scene_d3d_device`, `scene_d3d_present`, `gfx_d3d11`, `gfx_render_thread` | Draws recorded while no vertex constant is written share one copy of the constants (`SceneD3DConstantSnapshot`), as states share a snapshot (step 1); every write to the device's constants forgets the copy. In the hall 82% of draws record the same constants as the draw before them, and a recorded draw shrinks from 3.5 KB to a pointer. Each copy carries a serial, never reused, that `GfxNativeDraw::vertex_constants_id` passes on: D3D11 skips comparing the constants when the id matches the uploaded ones, and the render thread copies a batch's constants once per id. |
| 23 | `gfx_vulkan`, `gfx_render_thread` | The Vulkan backend takes the D3D11 shortcuts. A stage's view and sampler come from the previous draw of the batch when the image and serial (or address modes) match, and `gfx_release_sprite` forgets the images; a program's words are compared once per native batch; a draw whose constants id matches the previous slot's binds that ring slot while the ring keeps it (`ring_epoch`, bumped at each submission and ring replacement). Step 12 on Vulkan: `gfx_end_native_target_into` copies the target into the texture native draws bind for the image and records a readback copy, with no submission; `gfx_read_ended_native_target` submits and reads it. The render thread queues Vulkan target ends as it does D3D11's. |
| 24 | `scene_actor_services_draw` | The light volume texture (`DAT_00504844`, 256x256) is not read back after a target ends on the GPU. The whole executable references its handle and surface in four instructions (a byte scan of the XBE for both addresses finds only these): `0x000620C0` creates them, `0x000FE5A0` sets the surface as the render target, and `0x000FE960` passes the texture to `0x00064820`, whose one use of it is `D3DDevice_SetTexture(0, ...)`. No native code reads its texels, so its memory keeps the texels it was created with. The inspector shows textures from those texels, so its build keeps the readback. This removes the presenter's last wait in the hall. |

Step 12 (2026-11-08). Temporary per-stage probes in the presenter (removed) found that 10 `render_target` calls a
frame took about 40% of the replayed heavy hall frame (`--render-replay-after 70`, 1080p): the hall draws into the
same 256x256 SZ_A8R8G8B8 created texture five times a frame and samples it after each, and each end waited for the
GPU to finish the frame so far, swizzled into the texture's data, then decoded and uploaded it again. Release,
D3D11, 1920x1080, `--vsync 0`, `--scene chapter1_scene1_2playcam`, medians before → after, frame / presenter ms:

- 4:3 settings: heavy frames 60–95 12.17 / 8.68 → 8.49 / 5.16; frames 120–199 13.29 / 8.74 → 9.59 / 5.21; replayed
  heavy frame 8.26 / 8.18 → 5.68 / 5.59.
- Widescreen settings (3,122 draws), two runs each: heavy frames 11.8–12.5 → 10.1–10.8; frames 120–199 16.5 →
  12.2–12.4; replayed heavy frame 9.8 → 8.1–8.3.

Checks: the six benchmark cases (labels `rt12_retail` / `rt12_opt`, 1280x720) match the retail build with maximum
channel delta 0, and hall frames 80 and 95, which draw and sample the target, are byte-identical to the retail
build. The Vulkan backend is unchanged by this step.

Steps 13 and 14 and the step 12 flush (2026-11-08). Per-stage probes (removed) on the replayed heavy hall frame,
2,382 indexed draws: program validation fell from 2,466K to about 1,000K cycles a frame (step 13), the stream
lookups from 2,487K to 1,770K (step 14), and the end-of-present readback wait from about 7,000K to 4,000K (the
flush). The readback still waits: a non-blocking Map at that point finds the copy unfinished in 999 frames of
1,000. Widescreen settings, 1920x1080, `--vsync 0`, two runs each, frame / presenter ms medians: retail presenter
(`GBTG_OPTIMIZE` off) heavy frames 60–95 25.6–25.8 / 20.2–20.7 and frames 120–199 25.2–25.9 / 18.7–19.5; steps 1–14
8.7–10.1 / 4.9–6.0 and 10.9–11.4 / 5.7–6.4 (steps 1–12 alone: 10.1–10.8 and 12.2–12.4 frame ms). The six benchmark
cases (labels `rt14_retail` / `rt14_opt`) match with maximum channel delta 0, and hall frames 80 and 95 are
byte-identical to the retail presenter.

Steps 15–19 (2026-11-09). A CPU sampling profile of a 200-frame hall run (the method under "Profiling round
after step 7"; `xperf -a stack -butterfly` gives callers and callees) put 64% of the whole run in D3DCompile:
`gfx_init`'s own shaders and each scene's first use of a program or pixel definition, 2.3 s in all and a worst
frame of 2.1 s. In the frame loop it showed `std::map` copies and frees of each draw's stream bindings (most of
the 43,000 allocations a heavy frame) and the retained geometry `memcmp`.
Temporary cycle probes (removed) then gave, per draw of the replayed heavy frame after steps 15–18: setup 620,
program lookup 180, retained geometry check 1,300, output assembly 590, backend 2,535 (of it the constants
compare, Map and copy 740, the retained upload lookup 400, the four views 345, DrawIndexed 200). Step 19 took the
total from 5,390 to 4,080, the backend to 1,740: its constant compares had been waiting on memory.

Release, D3D11, 1920x1080, `--vsync 0`, `--scene chapter1_scene1_2playcam`, 4:3 settings, medians of frames
60–95 / 120–199, best of three runs, with `--render-profile` on (its clocks cost about 0.3 ms of a heavy frame):

| Build | Frame ms | Presenter ms | Recording ms | Allocations |
| --- | --- | --- | --- | --- |
| Retail presenter (`GBTG_OPTIMIZE` off) | 19.98 / 22.88 | 15.54 / 16.27 | 1.76 / 1.91 | 48,771 / 69,043 |
| Steps 1–14 | 9.46 / 11.06 | 5.70 / 5.78 | 1.40 / 1.56 | 43,059 / 62,857 |
| + 15, 16 | 6.67 / 7.85 | 4.71 / 4.65 | 0.31 / 0.30 | 1,953 / 21,070 |
| + 17 | 5.33 / 6.79 | 3.63 / 3.81 | 0.25 / 0.27 | 1,953 / 21,070 |
| + 18 | 5.16 / 6.41 | 3.42 / 3.41 | 0.29 / 0.29 | 1,074 / 20,195 |
| + 19 | 4.37 / 5.70 | 2.72 / 2.75 | 0.24 / 0.25 | 1,074 / 20,195 |

With a warm `cache/dxbc`, the 300-frame run's compilation falls from 2,378 ms to 16 ms, its worst frame from
2,160 ms to about 125 ms (scene loading), and its wall time from 5.6 s to 1.5 s. The first run on a machine, or
after a shader source change, still compiles everything.

Checks: eight captures at 1280x720 (the six benchmark cases and hall frames 80 and 95, labels `p19_retail` /
`p19_opt`) match the retail presenter with maximum channel delta 0, as does hall frame 300 at 1920x1080;
`--validate-render-boundary` passes; hall frame 95 was looked at. The retail presenter here is a
`GBTG_OPTIMIZE=OFF` build run with `--gfx d3d11`.

Vulkan takes steps 16, 17 and 19 and the presenter half of 18 (15 and the backend half are D3D11's; Vulkan has
`cache/spirv`). Same runs with `--gfx vulkan`, frame / presenter ms: retail presenter 19.44 / 14.65 and
20.91 / 14.66; optimized 6.05 / 4.31 and 7.56 / 4.48 on the render thread, 7.11 / 5.12 and 8.18 / 4.99 with
`--render-thread 0`. Hall frame 300 is byte-identical between the Vulkan retail presenter and both optimized runs.

Step 20 (2026-11-09), same runs: D3D11 heavy frames 4.37 / 5.70 → 3.60 / 4.86 frame ms (presenter, which now
includes queueing, 2.72 → 2.09). With the target ends still draining, the thread gave only 4.16 → 3.89: the hall
ends its 256x256 target five times a frame. One drain remains per command list, at `finish_render_target`'s
readback. Vulkan, whose target ends no longer drain either: 6.05 / 7.56 → 4.99 / 6.30. Hall frame 300 is
byte-identical to the retail presenter on both. The eight 1280x720 captures (label `p20_opt`) match `p19_retail`
except the menu: with the thread on, this executable's game select screen reaches the same content one frame later
from frame 182 (draw and vertex counts shift by one frame), giving one stable other image. It is timing, not
drawing: the render thread changes game state only through readbacks, which drain first, and the screen's movie
thumbnails are paced by real time (`GetTickCount`, completed asynchronous reads). The virtual audio clock does not
remove it, `--render-thread 0` on the same executable matches, and the same code in a second build tree
(`GBTG_GFX=vulkan;d3d11`, `--gfx d3d11`) matches the retail image with the thread on. `--validate-render-boundary`
passes. A visible windowed run with every plugin (the debug UI, shader warm-up) ran 900 frames without error; the
debug UI's overlay was not opened. Shader warm-up of 425 vertex and 173 pixel permutations took 16.2 s on a cold
`cache/dxbc` and 2.5 s warm.

Steps 21–24 (2026-11-09), same hall runs (1920x1080, D3D11 and Vulkan on the render thread unless noted), frame /
presenter ms, heavy frames 60–95 and frames 120–199:

| Build | D3D11 | Vulkan |
| --- | --- | --- |
| Retail presenter | 19.98 / 15.54; 22.88 / 16.27 | 19.44 / 14.65; 20.91 / 14.66 |
| Steps 1–20 | 3.60 / 2.09; 4.86 / 2.12 | 4.99 / 3.53; 6.30 / 3.65 |
| + 21 | 3.38 / 1.89; 4.53 / 1.89 | |
| + 23 (Vulkan shortcuts; target ends still synchronous) | | 5.01 / 3.48; 6.17 / 3.51 |
| + 23 (step 12 on Vulkan) | | 3.56 / 2.13; 4.84 / 2.27 |
| + 24 | 3.04 / 1.58; 4.27 / 1.66 | 2.92 / 1.52; 4.08 / 1.59 |
| + 24, `--render-thread 0` | 3.54 / 2.05; 4.82 / 2.17 | 3.91 / 2.05; 5.19 / 2.16 |

Hall frame 300 is byte-identical to the retail presenter on both backends after each step. The eight 1280x720
captures match the retail presenter on D3D11 with the thread off (with it on, all but the timing-sensitive menu,
as in step 20) and on Vulkan with the thread off (labels `p24_off`, `vk24_p`). The `GBTG_OPTIMIZE=OFF` tree, the
inspector and the transition-test executable build; a headless inspector run of the hall exits cleanly. `--validate-render-boundary`
passes; Vulkan with `GBTG_VULKAN_VALIDATION=1` on the render thread reports 0 validation errors over 300 hall frames.

Tried and not kept (step 22): watching scene memory for writes (`VirtualAlloc` with `MEM_WRITE_WATCH` for the
scene's blocks, `GetWriteWatch` when each command list starts and after callbacks that report a write) so that
retained geometry whose pages were not written skips even step 17's once-per-list comparison. Large blocks were
page-aligned so per-frame writes to small blocks did not mark static vertex data. It skipped 59% of the
comparisons, but 5.7 MB a heavy frame was still compared (geometry the game rewrites every frame, unchanged while
actor updates are held), and a collection costs about 28 µs: heavy frames measured 3.37 ms against 3.38 without it.
It also relied on every write to scene memory going through the CPU. Reverted.

Not done, by the probes: offsetting into one constant buffer instead of a `WRITE_DISCARD` Map per draw (D3D11), and
uploading only the declared input registers (every GPU-program vertex is a full 256-byte `GfxNativeInputs`).

Tried and not kept: sharing equal retained-stream copies between entries so each region is compared once per
command list. The check compares 30 MB a frame, of which 14 MB is unique, but the probe time did not fall: reading the
unique bytes and the per-list region map cost what the duplicates did. Removing that check needs tracking of
writes to scene memory.

Not done (step 24 has since removed this wait for the hall's light volume texture): removing the remaining wait in step 12's end-of-present readback (about 1.2 ms of the
replayed heavy hall frame at 1080p, the GPU still drawing the frame). Moving it later, up to the next
native read of the texture, needs proof of when native code reads that memory. Also not done: R16 index uploads, GPU decoding of packed streams, and uploading only the
declared input registers (every GPU-program vertex is a full 16-register, 256-byte `GfxNativeInputs`).

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

### Step 7: retained geometry (2026-10-31)

Prompted by the Steam Deck port (`docs/LINUX.md`): on the Deck the hall's heavy frames (about 3,500 draws, frames
60–95 of `--scene chapter1_scene1_2playcam`) took 78 ms, with the presenter at 50 ms and 29 MB of program inputs
re-decoded and uploaded every frame. The same frames take about 17 ms on the desktop with either presenter.

Six benchmark cases, Release, 1280x720, labels `steamdeck_retail` / `steamdeck_opt` (D3D11, the opt build has
steps 1–7). Every screenshot matches with maximum channel delta 0. Median / p99 frame ms, retail → optimized:

- hall: 4.613 / 5.363 → 3.430 / 3.945
- tutorial: 4.629 / 6.460 → 3.737 / 4.415
- attract: 1.960 / 4.110 → 1.141 / 3.989
- menu: 0.205 / 35.118 → 0.221 / 5.635
- cutscene: 1.218 / 3.537 → 0.690 / 2.105
- transition: 20.306 / 89.612 → 10.377 / 77.331

Hall heavy frames 60–95 at 1280x800 (medians, retail → optimized): D3D11 17.6 → 9.0 ms (presenter 13.0 → 5.0 ms),
Vulkan 17.9 → 10.9 ms (presenter 13.1 → 6.5 ms); uploaded geometry 25.4 MB → 34 KB per frame. Frame 300 matches the
retail path on both presenters. `--validate-render-boundary` passes on the optimized build.

Reusing the Vulkan backend's per-draw uniform copies (camera, material, the 3 KB vertex constants) when unchanged was
tried and measured no gain: consecutive native draws almost always carry different constants. It was not kept.

### Profiling round after step 7 (2026-10-31)

The Steam Deck still took 52 ms on the heavy hall frames with step 7 (presenter 25.8 ms, of it Vulkan submission
12.1 ms; game tick and recording 20.4 ms). A CPU sampling profile on Windows (VS Standard Collector,
`VSDiagnostics.exe`, which needs no elevation; the `.diagsession` holds an ETL that `xperf -a profile -symbols`
reads with a RelWithDebInfo `GBTG_OPTIMIZE=ON` build) of the replayed heavy frame
(`--render-replay-after 70`) and of runs to frame 42 and 100 (subtracted, to isolate the heavy stretch) found:

- `present_indexed` and `vcruntime` copies: every GPU-program draw zeroed an `XboxVertexProgramState` and copied the
  3 KB constants into it, used only for decoding. Fixed under step 7.
- `std::map<std::vector<u32>, ...>` lookups of whole vertex programs in the Vulkan backend, twice per draw: 9% of
  samples. Fixed by the Vulkan step 3 memo.
- `SceneResourceMemory` lookups (`resolve`, `tail`, `find_address`'s linear scan): about 13% of the heavy stretch
  including the game tick. Fixed by step 8.

Per-draw stage timing of the replayed frame afterwards (temporary probes, removed): setup 0.57 µs, program check
0.08 µs (D3D11 0.55 µs), retained-geometry check 0.52 µs, output assembly 0.30 µs, backend 0.94 µs (D3D11 1.42 µs).

Measured against the retail build run back to back (the machine's absolute timings drifted by up to 25% between
runs), hall heavy frames 60–95 at 1280x800: Vulkan 0.61 → 0.53 of retail, D3D11 0.51 → 0.48; replayed heavy frame,
Vulkan 0.48 → 0.42. The six benchmark cases still match the retail path with maximum channel delta 0, and
`--validate-render-boundary` passes.

Tried and not kept, for no measurable change: reserving the next frame's command vector to the previous frame's size
in `SceneD3DDevice::take_commands`, and the Vulkan uniform reuse above.

Remaining, by the heavy-stretch profile: heap allocation and the kernel time that goes with it (about 47,000
main-thread allocations per heavy frame, roughly 13 per draw, from recording and game code), Vulkan per-draw
submission (descriptor pushes, pipeline lookup, the 3 KB constant ring copy) and the driver.

### Render thread, step 11 (2026-11-01)

Before the change, hall heavy frames (frames 60–95, `--scene chapter1_scene1_2playcam`, about 2,680 draws) were
counted for calls that must return a result: each frame ends five render targets, all between draws 1,579 and 1,611,
and reads nothing else back. Draws before that point overlap their submission with the presenter's translation; the
target readbacks drain the queue; the remaining 40% of the frame, the present and the GPU wait overlap the next tick.

Windows, Release `GBTG_OPTIMIZE=ON`, Vulkan, 1280x800, `--vsync 0`, three alternating rounds of `--render-thread 0`
and `1`: heavy frames median 11.5–12.1 → 10.3–11.0 ms, frames from 120 on 3.21–3.24 → 2.63–2.79 ms. Desktop
submission is under 2 ms of the heavy frame; on the Steam Deck it was about 12 ms before steps 8–10, so the overlap
there should be larger (not yet measured on a Deck).

Checks: the six benchmark cases on Vulkan match the retail build with maximum channel delta 0; the hall at frame 300
is identical with the thread on and off; the validation layer reports 0 errors over 300 hall frames with the thread
on; a windowed `--vsync 1` run exercises `present_again` and the debug UI wait. A ThreadSanitizer build (Linux,
lavapipe) over 300 hall frames reports no race involving the render thread; its reports are inside FAudio (game
thread against the SDL audio thread), unrelated to this step. It also found a game-thread use-after-free, since fixed:
`retire_target` copied the target's body query through a reference into the player body, which player destruction
frees before the actor view is released; the row now keeps the query from its last refresh, as it keeps the bits. `--validate-render-boundary` runs before `gfx_init`, so it does not exercise the wrapper.
