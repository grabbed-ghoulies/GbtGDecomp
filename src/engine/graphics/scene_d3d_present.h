#pragma once
#include "scene_d3d_device.h"
#include "../scene/scene_resource_memory.h"
#include "gbtg/texture.h"
#include <functional>

namespace gbtg
{
// Host present of the draws SceneD3DDevice recorded. This is the D3D boundary: each indexed draw's
// bound NV2A vertex program runs over its streams, the projected vertices keep the draw's pixel shader
// definition, textures and render states, and quads and clears go out in the same order.
struct SceneD3DPresentContext
{
    const SceneResourceMemory& memory;
    const SceneD3DResources& resources;
    const std::map<u32, SceneD3DVertexShader>& vertex_shaders;
    std::function<const RgbaImage&(const u8*)> texture; // Decoded image of a bound texture record.
    // D3DDevice_SetRenderTarget: a texture surface, or SceneD3DDevice::kBackBuffer to return to the screen.
    // The depth surface comes too: a texture surface there takes the target's depth-stencil when it ends.
    std::function<void(std::uintptr_t color, std::uintptr_t depth)> render_target;
    // D3DDevice_CopyRects: the back buffer drawn so far into a texture object's level 0.
    std::function<void(u32 texture)> copy_back_buffer = nullptr;
    // GBTG_OPTIMIZE step 12: called after the last command, to write back render targets that ended on the GPU.
    std::function<void()> finish = nullptr;
};
void present_scene_d3d_commands(std::span<const SceneD3DCommand> commands, const SceneD3DPresentContext& context);
// GBTG_OPTIMIZE step 17: a context callback calls this when it writes scene memory (a render target's texels, a
// locked texture surface), so the presenter compares retained geometry bytes again. A no-op in retail builds.
void scene_d3d_memory_written();
}
