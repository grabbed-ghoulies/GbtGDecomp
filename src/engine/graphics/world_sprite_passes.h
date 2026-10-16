#pragma once
#include "gbtg/types.h"
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
void draw_effect_debris(std::span<const u8> particles, RuntimeDisplaySetupContext& context); // Xbox 0x000EC470
// Full-view flash (DAT_00504478), back-buffer capture overlay (DAT_005044A4) and retained lines (DAT_005044B0).
struct RuntimeScreenEffects
{
    const u32& flash; // DAT_00504478, set by 0x00060BC0 / 0x00060C80 / 0x00060D50.
    const u32& capture; // DAT_005044A4, set by 0x00060C20 / 0x00060C80 / 0x00060D50.
    const i32& transition_guard; // DAT_0050484C.
    const u32& lines; // DAT_005044B0, set by 0x000619D0.
};
void draw_screen_effects(const RuntimeScreenEffects& effects,
    RuntimeDialogueOuterDrawContext& context); // Xbox 0x000615E0
}
