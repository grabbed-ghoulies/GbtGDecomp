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
