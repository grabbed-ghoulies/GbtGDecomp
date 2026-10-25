#pragma once
#include <functional>
#include <span>
#include "gbtg/entity.h"
#include "gbtg/model_render.h"
#include "gbtg/model_runtime.h"

namespace gbtg
{
// The scene light-volume records (DAT_00509C20, 25 records of 0x25C bytes) and their draw order.
extern i32 g_light_paused; // DAT_00509B70. Nonzero aims every shadow volume from the view.
extern i32 g_light_running; // DAT_00509B74. Nonzero eases actor +0x318 toward +0x324.
extern Vec3 g_light_offset; // DAT_0050D764..DAT_0050D76C, the default light offset over an actor.
extern i32 g_light_filled; // DAT_0050D71C.
extern i32 g_light_filled_gate; // DAT_0050D720.
void set_light_filled_gate(i32 limit); // Xbox 0x000FEA90, EAX the limit: clamped to 0 .. 0x19.
// The default light offset over an actor (DAT_0050D764): the direction times 30, or (0, 30, -30) for none.
void set_runtime_light_offset(const Vec3* direction); // Xbox 0x000FE160, EAX the direction
// Script world action 0x4B's record (ESI): the ambient tint from its colour bytes (DAT_00509B20 ..), the light's
// intensity (DAT_00509B4C) and direction in degrees (DAT_00509B50 .. DAT_00509B58), then the fog: its enable
// (DAT_00509AFC, configured at once through 0x000FD5C0 when set), start, end and colour.
void apply_runtime_scene_lighting(std::span<const u8> record, RuntimeSceneFog& fog,
    const std::function<void()>& configure_fog); // Xbox 0x000FD780, ESI the record
extern u8 g_light_order[0x19]; // DAT_0050D724.
// Each registered light record's animation for the frame: +0x66 bit 0x40 flickers its colour (+0x78) between +0x80
// and +0x84, bit 0x80 flickers its range (+0x70) between +0x8C and +0x90, both on frames the +0x88 / +0x94 masks
// pass; +0x67 bits 2 and 4 swing the colour and the range on sine periods +0x88 and +0x94. Changed records get
// +0x66 bit 1.
void animate_runtime_light_records(RuntimeShaderRegistrationState& lights, u32 frame, float time,
    u32& random); // Xbox 0x000FDDA0
// The scene's registered light records (DAT_00509B7C) are dropped.
void release_runtime_light_records(RuntimeShaderRegistrationState& shaders); // Xbox 0x000FDED0
// The scene teardown's light and fog reset: the light records, the shadow-volume counts, the fog record
// (DAT_00509AFC..DAT_00509B0C) back to off, 1..1000 and 0xFFC0C0C0, and the base light colour (DAT_00509B3C).
void reset_runtime_light_environment(RuntimeShaderRegistrationState& shaders,
    RuntimeSceneFog& fog); // Xbox 0x000FD710
}
