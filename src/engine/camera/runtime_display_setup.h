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
// Each active camera slot's display, then its view draw (0x0011C240).
void present_runtime_display_views(const std::function<void(i32)>& draw_view,
    RuntimeDisplaySetupContext& context); // Xbox 0x00033A20
}
