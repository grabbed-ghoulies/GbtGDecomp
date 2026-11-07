#pragma once
#include "gbtg/types.h"
#include "../camera/camera_pulse.h"
#include <array>
#include <span>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
struct RuntimeDialogueOuterDrawContext;
// The bound projection, the sprite render states and depth-tested blending for camera-facing quads, with fog
// held off (DAT_00509B5C counts the hold).
void begin_world_sprite_draw(RuntimeDisplaySetupContext& context); // Xbox 0x0005CF20
// The sprite depth function (always or less-equal) and depth write by effect +0x140 mode 0..3.
void set_world_sprite_depth_mode(u32 mode, RuntimeDisplaySetupContext& context); // Xbox 0x0005DFF0, EAX mode
// A textured quad over the unit square in x/z, placed by position, the y/x/z euler and the x/z size through the
// DAT_0050DD70 world matrix, with the texture's frame (EAX) and a D3DCOLOR built from `color`.
void draw_world_sprite_quad(i32 frame, u8* texture, const Vec3* position, const Vec3* euler, const float* size,
    u32 color, RuntimeDisplaySetupContext& context); // Xbox 0x0005CF90, ECX texture, EDI position, EBX euler
// The debris records (DAT_005068EC, 50 of 0x74 bytes) drawn as textured quads; nothing without the pool.
// `textures` are the type rows' +0x80 textures (RuntimeEffectDebrisStorage::textures).
struct EffectDebrisRecord;
void draw_effect_debris(std::span<const EffectDebrisRecord> particles,
    std::span<const std::span<const u8>, 10> textures,
    RuntimeDisplaySetupContext& context); // Xbox 0x000EC470
// The bound projection and view, the sprite render states with depth writes and the line texture's frame (EDI).
void begin_world_line_draw(u8 frame, u8* texture, RuntimeDisplaySetupContext& context); // Xbox 0x00065460
// A quad `width` across from `from` to `to`, square to the view's forward, each end pulled `inset` toward the other.
void draw_world_line_quad(const Vec3& to, const Vec3& from, float width, float inset, u32 color,
    RuntimeDisplaySetupContext& context); // Xbox 0x000654E0, EAX to, EDI from
// The points (stride 12) as one strip of view-facing quads `width` wide, the texture bound (frame 0) and the depth
// mode set first.
void draw_world_line_strip(const Vec3* points, i32 count, u8* texture, u32 depth_mode, float width, u32 color,
    RuntimeDisplaySetupContext& context); // Xbox 0x000659F0, EAX points, EBX count, ECX texture
// Full-view tint (DAT_00504478), back-buffer blur overlay (DAT_005044A4) and retained lines (DAT_005044B0).
struct RuntimeScreenEffects
{
    RuntimeScreenTint& tint; // DAT_00504450, shown by 0x00060D50.
    RuntimeCameraPulse& pulse; // DAT_0050447C, captured by 0x00060D50.
    const i32& transition_guard; // DAT_0050484C.
    RuntimeScreenLines& lines; // DAT_005044A8 .. DAT_005044B0, retained by 0x000619D0.
    u8* capture_texture; // DAT_005047DC, book page 0's frame record.
    u32 capture_address; // Its native address, the D3D texture handle.
    u32 tint_vertex_shader; // DAT_00427224 (boot shader row 2).
    const u32* tint_shader_constants; // PTR_DAT_00427218's data, null when it is 0.
    u32 blur_vertex_shader; // DAT_00427314 (boot shader row 12).
    const u32* blur_shader_constants; // PTR_DAT_00427308's data, null when it is 0.
    u32 blur_pixel_shader; // DAT_00425E9C.
    float time; // DAT_004DEA60.
};
// `count` view offsets `radius` long, spread evenly around the circle from the angle `time` has reached in `period`.
void spread_runtime_blur_offsets(std::array<float, 4>* offsets, i32 count, float radius, float period,
    float time); // Xbox 0x00061160, EAX count
void draw_screen_effects(const RuntimeScreenEffects& effects,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x000615E0
}
