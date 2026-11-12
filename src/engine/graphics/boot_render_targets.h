#pragma once
#include "glow_pass.h"
#include "../frontend/gameselect_rows.h"

namespace gbtg
{
// D3DSURFACE_DESC as D3DSurface_GetDesc fills it.
struct RuntimeSurfaceDescription
{
    u32 format = 0; // +0
    u32 type = 0; // +4
    u32 usage = 0; // +8
    u32 size = 0; // +0xC
    u32 multisample = 0; // +0x10
    u32 width = 0; // +0x14
    u32 height = 0; // +0x18
};
// The D3D boundaries boot 0x000620C0 calls.
struct RuntimeBootRenderTargetD3D
{
    // D3DDevice_CreateTexture2 (0x00177840): width, height, depth, levels, usage, format, resource type.
    std::function<u32(u32, u32, u32, u32, u32, u32, u32)> create_texture;
    std::function<DialogueSurfaceHandle(u32 texture, u32 level)> surface_level; // D3DTexture_GetSurfaceLevel2
    std::function<DialogueSurfaceHandle(i32 index)> back_buffer; // D3DDevice_GetBackBuffer2
    std::function<DialogueSurfaceHandle()> depth_stencil; // D3DDevice_GetDepthStencilSurface2
    std::function<RuntimeSurfaceDescription(DialogueSurfaceHandle)> describe; // D3DSurface_GetDesc
    std::function<void(DialogueSurfaceHandle)> release; // D3DResource_Release
    // 0x00175C90, D3D's surface create: a surface header (0x18 bytes) and its contiguous memory, or null.
    std::function<DialogueSurfaceHandle(u32 width, u32 height, u32 usage, u32 format)> create_surface;
    std::function<u32(DialogueSurfaceHandle)> surface_data; // The surface's +4 word, its memory.
    // XGSetTextureHeader: width, height, levels, usage, format, pool, texture, data, pitch.
    std::function<void(u32, u32, u32, u32, u32, u32, u32, u32, u32)> texture_header;
    // D3DSurface_LockRect with no rectangle: the locked bits.
    std::function<u32(DialogueSurfaceHandle, u32 flags)> lock_surface;
    RuntimeScreenTextureBoundaries textures; // 0x000533A0's.
};
// The globals boot 0x000620C0 sets.
struct RuntimeBootRenderTargets
{
    u32& light_volume_texture; // DAT_00504844
    DialogueSurfaceHandle& light_volume_surface; // DAT_00504848
    DialogueRenderTargetState& targets; // DAT_005047D0, DAT_005047D4, DAT_00504800
    std::array<std::span<u8>, 2>& pages; // DAT_005047D8, DAT_005047E4
    RuntimeGlowTargets& glow; // DAT_005047DC, DAT_005047E0, DAT_005047E8, DAT_005047FC
    i32& transition_guard; // DAT_0050484C
};
// The light volume texture, book page 0 (a texture of the back buffer's size and format) with its surface, and a
// surface of the depth buffer's size and format whose memory is also a LIN_A8R8G8B8 texture (DAT_005047E8) and book
// page 1. `glow.depth_texture` is the DAT_005047E8 header's storage.
void create_runtime_boot_render_targets(RuntimeBootRenderTargets& targets,
    const RuntimeBootRenderTargetD3D& d3d); // Xbox 0x000620C0
}
