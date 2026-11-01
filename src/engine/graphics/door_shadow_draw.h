#pragma once

#include "../frontend/dialogue_draw_context.h"
#include "../navigation/cell_registration.h"
#include "model_shader_states.h"

namespace gbtg
{
// 0x0005AC40's outputs, from the door shape's 0x94-byte row at 0x00424994: the depth range (+4 ± DAT_0032B214) and
// the opacity at either end (+8 and +0xC, ordered by the side the camera is on).
struct RuntimeDoorShadowBounds
{
    float depth_start, depth_end; // EBX, EDI.
    float opacity_start, opacity_end; // ESI, the stack pointer.
};
u32 select_runtime_door_shadow_bounds(std::span<const u8> row, const Vec3& camera,
    RuntimeDoorShadowBounds& bounds, const RuntimeRelocationImage& image); // Xbox 0x0005AC40
void draw_runtime_door_shadow(std::span<const u8> row, const float* view, const float* projection,
    const Vec3& camera, const RuntimeRelocationImage& image, DialogueD3DBoundaries& d3d); // Xbox 0x0005AE40
struct RuntimeDoorShadowDraw
{
    entity_detail::NavigationCellRegistration* pool; // DAT_004EE2B8.
    const RuntimeRelocationImage& image;
    DialogueD3DBoundaries& d3d;
    RuntimeModelShaderStates& states;
    std::span<u32> deferred; // DAT_00181968.
    std::function<void()> clear_model_view; // D3DDevice_SetModelView(0, 0, 0).
    std::function<void(u32)> vertex_shader; // 0x00065F60, EAX is the table index.
    std::function<void(u32)> pixel_shader; // D3DDevice_SetPixelShader, boot table handle.
    std::function<void(std::span<const u8>, const float*, const float*)> draw; // 0x0005AE40.
};
void draw_runtime_door_shadows(RuntimeDoorShadowDraw& context); // Xbox 0x0005B020
}
