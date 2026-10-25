#pragma once

#include "../camera/runtime_display_setup.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"

#include <functional>
#include <memory>
#include <vector>

namespace gbtg
{
// One DAT_005462F4 record (0x34 bytes; +0 / +4 are the list links): +8 the eye, +0x14 the bound centre, +0x20
// the 64x64 texture, +0x24 the bound radius, +0x28..+0x2B colour bytes, +0x2C 1.0 and +0x30 1.
struct RuntimeActorSnapshot
{
    std::array<u8, 0x34> bytes{};
};
// The D3D calls 0x00116530 makes, in dump order. Handles are the recording boundary's.
struct RuntimeActorSnapshotD3D
{
    std::function<u32(u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)> create_texture;
    std::function<std::uintptr_t(u32 texture, u32 level)> surface_level; // D3DTexture_GetSurfaceLevel2.
    std::function<std::uintptr_t()> depth_stencil; // D3DDevice_GetDepthStencilSurface2.
    std::function<std::uintptr_t()> render_target; // D3DDevice_GetRenderTarget2.
    std::function<void(std::uintptr_t color, std::uintptr_t depth)> set_render_target;
    std::function<void(u32)> z_enable;
    std::function<void(u32)> stencil_enable;
    std::function<void(u32 method, u32 value)> set_state; // D3DDevice_SetRenderState_Simple.
    std::function<void(u32 count, const void* rectangles, u32 flags, u32 color, float depth, u32 stencil)> clear;
    std::function<void(std::uintptr_t)> release; // D3DResource_Release.
};
struct RuntimeActorSnapshots
{
    RuntimeDisplaySetupContext& display; // 0x000F6170; its draw context for 0x0005E110, 0x00156330, 0x00156400.
    RuntimeActorSnapshotD3D& d3d;
    u32& depth_mask; // DAT_00181C70.
    u32& depth_function; // DAT_00181C54.
    u32& cull_mode; // DAT_0032E3D0.
    // 0x000F9080 over actor +0x204 (ECX) at the given world matrix (EAX) and kind (EBX).
    std::function<void(Actor&, const AnimationMatrix&, u32 kind)> draw_model;
    std::vector<std::unique_ptr<RuntimeActorSnapshot>>& list; // DAT_005462F4, in link order.
    i32& count; // DAT_005462F0.
    // Host presentation: the snapshot's recorded draw is presented as soon as it is recorded, apart from the
    // frame's views.
    std::function<void()> begin_capture;
    std::function<void()> present_capture;
};

void set_runtime_cull_mode(u32 mode, u32& cull_mode); // Xbox 0x001565E0
// The actor's model drawn into a new 64x64 texture, framed on its bound sphere from 50 above and 10 behind.
std::unique_ptr<RuntimeActorSnapshot> snapshot_runtime_actor(Actor& actor,
    RuntimeActorSnapshots& context); // Xbox 0x00116530
void append_runtime_actor_snapshot(Actor& actor, RuntimeActorSnapshots& context); // Xbox 0x00116750, EAX actor
// The scene tick's snapshot ageing: past 16 kept snapshots every keep flag (+0x30) drops; an unkept snapshot's +0x2C
// counts down 0.01 a frame, and below 0 its texture is released and the snapshot freed.
void age_runtime_actor_snapshots(RuntimeActorSnapshots& context); // Xbox 0x001162F0
}
