# Vulkan backend

`gbtg` can present through Vulkan as well as D3D11. Both are host presenters of the same recorded
native output (`SceneD3DDevice` → `present_scene_d3d_commands` → `gfx_*`), so no game code or
retail translation differs between them.

## Building and choosing a backend

- Install the LunarG Vulkan SDK (1.3 or newer). CMake finds it through `VULKAN_SDK`.
- Configure with `-DGBTG_GFX="d3d11;vulkan"`. The first entry is the default backend; null is always built.
- Run with `--gfx vulkan` (or `Backend=vulkan` under `[Video]` in `gbtg.ini`).
- `dxcompiler.dll` from the SDK is copied beside `gbtg.exe`. At run time it is loaded from there, else from
  `%VULKAN_SDK%\Bin`. The Windows SDK's `dxcompiler.dll` has no SPIR-V code generation and is never used.
- `GBTG_VULKAN_VALIDATION=1` enables `VK_LAYER_KHRONOS_validation`; errors go to `gbtg.log` and the count is
  logged at shutdown.

## Layout

| File | Role |
|---|---|
| `src/engine/graphics/gfx_backend.h`, `gfx_dispatch.cpp` | `GfxBackend` table; the public `gfx_*` names forward to the selected backend |
| `src/engine/graphics/gfx_shader_sources.*` | The HLSL both backends compile, and native pixel-shader specialization |
| `src/engine/graphics/gfx_shader_permutations.*` | `cache/shaders.gbtg` format |
| `src/engine/graphics/gfx_present_logic.*` | Retail present logic (0x00033480, 0x00032AB0, 0x00033A90) shared by every backend |
| `src/engine/graphics/vulkan/vk_context.*` | Instance, device, VMA, buffers, images, barriers |
| `src/engine/graphics/vulkan/vk_swapchain.*` | Window swapchain |
| `src/engine/graphics/vulkan/vk_shader_compiler.*` | DXC → SPIR-V, register mapping, `cache/spirv/` |
| `src/engine/graphics/vulkan/vk_debug_ui.*` | ImGui renderer half (`imgui_impl_vulkan`) |
| `src/engine/graphics/vulkan/gfx_vulkan.cpp` | The `gfx_*` implementation |

## How it maps D3D11

- **Shaders.** The D3D11 HLSL is compiled by DXC (`-spirv -fvk-use-dx-layout -HV 2018`). Registers become
  set 0 bindings: `b0..b3` → 0..3, `t0..t3` → 10..13, `s0..s3` → 20..23, all as push descriptors.
  `stage_tex[4]` / `stage_samp[4]` are split into single bindings, because Vulkan cannot alias them with
  `g_tex` / `g_light` (`vulkan_hlsl`).
- **Clip space.** It is unchanged. The viewport is flipped (negative height), so D3D clip coordinates,
  clockwise front faces and the 0..1 depth range carry over.
- **Back buffer.** The game renders into its own BGRA8 image and D24S8 depth-stencil (D32FS8 where D24S8
  cannot be rendered). The swap blits that image onto the swapchain, stretched like DXGI does, and ImGui
  draws over it. `gfx_present_again` blits the held frame again.
- **Submission.** One command buffer is submitted, and waited on, at each swap and at each readback
  (`gfx_readback`, `gfx_end_native_target`, `gfx_read_native_target_depth`). Vertex, index, uniform and
  staging data share a ring that is reset after each wait. This is simple and correct, with no frames in flight.
- **State.** Blend, depth-stencil, cull and topology are baked into a pipeline cached by its state;
  viewport, scissor and stencil reference are dynamic. Clears use `vkCmdClearAttachments`.
- **SPIR-V cache.** Every compiled module is written to `cache/spirv/<hash>.spv`. A warm warm-up takes
  about 0.65 s for 159 permutations, against 17 s cold (D3D11: about 6.7 s).

## Verified

Checked in a Debug build on an RTX 4070 Ti:
- Headless `--screenshot` captures of eleven scenes (attract, game select, chapter 1 and 2a) match D3D11 to
  within ±1 per channel, apart from a few edge pixels. Scene 40 differs between two D3D11 runs as much as
  between the backends.
- No validation errors in those runs or in a windowed run.
- The windowed path with the ImGui overlay works, at the same frame time as D3D11.
- `--validate-render-boundary` passes.

## Not done

- **Legacy host mesh path.** `gfx_draw_mesh`, `gfx_set_camera`, `gfx_set_bones`, the dialogue textures and
  `gfx_draw_screen_mesh` throw on Vulkan. Nothing calls them; `button_bat_draw`, the last user of
  `gfx_draw_screen_mesh`, has no callers (`docs/LEGACY_HOST_CODE.md`).
- **`--validate-gpu-vertices`.** It needs D3D11's stream-out geometry shader. On Vulkan it throws, and the
  boundary check skips its GPU-versus-CPU vertex section.
- **GPU profiling.** No timestamp queries are recorded for `--render-profile` yet.
- **`GBTG_OPTIMIZE`.** It has no Vulkan-specific paths.
- **Inspector.** The inspector remains D3D11 only.
- **VSync=2.** It presents every vblank, because FIFO has no every-other-vblank mode.
