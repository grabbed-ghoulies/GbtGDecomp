#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
namespace scene_detail
{
// DAT_005078D8
extern u32 g_pair_cache[256];

// DAT_004312dc
extern float g_sim_freeze;

// DAT_004312c4
extern float g_sim_scale;

// DAT_005096c0
extern float g_sim_dt;

// DAT_0050E764 and DAT_0050E768, the two counts 0x00117400 keeps.
extern std::array<u32, 2> g_pause_references;

// DAT_0050E768. Nonzero skips the +0x930 decay inside the base update.
extern i32& g_block_decay;

// Byte at [DAT_0051248C]+0x30F144. Zero skips that same decay.
extern u8 g_decay_arm;

} // namespace scene_detail
} // namespace gbtg
