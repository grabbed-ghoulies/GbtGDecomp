#pragma once
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
extern u8 g_light_order[0x19]; // DAT_0050D724.
// The scene's registered light records (DAT_00509B7C) are dropped.
void release_runtime_light_records(RuntimeShaderRegistrationState& shaders); // Xbox 0x000FDED0
// The scene teardown's light and fog reset: the light records, the shadow-volume counts, the fog record
// (DAT_00509AFC..DAT_00509B0C) back to off, 1..1000 and 0xFFC0C0C0, and the base light colour (DAT_00509B3C).
void reset_runtime_light_environment(RuntimeShaderRegistrationState& shaders,
    RuntimeSceneFog& fog); // Xbox 0x000FD710
}
