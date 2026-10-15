#pragma once
#include "display_clip_limits.h"
#include "display_view.h"
#include "../frontend/dialogue_draw_context.h"

namespace gbtg
{
struct RuntimeDisplaySetupContext
{
    RuntimeDialogueDrawContext& draw;
    DisplayClipBounds& scene_bounds;
    std::array<float, 16>& device_projection; // DAT_005077AC.
    std::array<float, 16>& device_view; // DAT_00506E98.
    std::array<float, 16>& device_world; // DAT_00506E58.
    std::array<float, 16>& projection_transpose; // DAT_00506FD8.
    const std::array<float, 12>& default_world; // DAT_0042A7C0.
    u32& effects_disabled; // DAT_004E173C.
    u32& current_display_address; // Native address represented by DAT_005077A8.
};
void apply_display_y_offset(const Vec3& angles, float amount, Vec3& position); // Xbox 0x00049560
void apply_display_z_offset(const Vec3& angles, float amount, Vec3& position); // Xbox 0x000495B0
void apply_display_x_offset(const Vec3& angles, float amount, Vec3& position); // Xbox 0x00049600
void apply_display_offset(u32 kind, const Vec3& angles, float amount,
    Vec3& position); // Xbox 0x00049650
void apply_display_camera_effects(Vec3& position, const Vec3& angles,
    RuntimeDisplaySetupContext& context); // Xbox 0x00049260
bool select_display_camera(EntityCamNode* camera, i32 slot,
    RuntimeDisplaySetupContext& context); // Xbox 0x000489E0
void bind_display_device_view(const float* world,
    RuntimeDisplaySetupContext& context); // Xbox 0x000F6430
void rebuild_display_view(RuntimeDisplaySetupContext& context); // Xbox 0x000F5E50
void rebuild_display_frustum(); // Xbox 0x000F6630
void set_display_render_frustum(const float* frustum,
    RuntimeModelRenderContext& renderer); // Xbox 0x00156350
void rebuild_display_projection(RuntimeDisplaySetupContext& context); // Xbox 0x000F5FD0
void bind_display_projection(RuntimeDisplaySetupContext& context); // Xbox 0x000F6170
bool setup_explicit_camera_display(EntityCamNode* camera, i32 slot,
    RuntimeDisplaySetupContext& context); // Xbox 0x00048BC0
void setup_active_camera_display(i32 slot,
    RuntimeDisplaySetupContext& context); // Xbox 0x00048B60
void set_display_transform(const float* matrix, RuntimeDisplaySetupContext& context); // Xbox 0x000F6EB0
// Display +0x48..+0x54 (origin and size), then the fitted view (0x000F57F0) from them.
void set_display_viewport(i32 x, i32 y, i32 width, i32 height); // Xbox 0x000F5CC0
// Display +0x108 (inverse transform), the frustum (0x000F6630) and the renderer's frustum pointer.
void set_display_inverse_transform(const float* matrix, RuntimeDisplaySetupContext& context); // Xbox 0x000F6E60
// The projection transposed into DAT_00506F98 and bound (SetTransform 1, the fitted viewport, 0x00156400),
// without 0x000F6170's view rebuild.
void bind_display_projection_only(std::array<float, 16>& transposed,
    RuntimeDisplaySetupContext& context); // Xbox 0x000F61F0
// Each active camera slot's display, then its view draw (0x0011C240).
void present_runtime_display_views(const std::function<void(i32)>& draw_view,
    RuntimeDisplaySetupContext& context); // Xbox 0x00033A20
}
