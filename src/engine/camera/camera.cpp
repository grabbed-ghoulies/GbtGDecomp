#include "../scene/scene_types.h"
#include "camera.h"

namespace gbtg
{
namespace scene_detail
{

// DAT_005078D8
u32 g_pair_cache[256] = {};

// DAT_004312dc > 0 freezes the step (BanjoCam). Otherwise the step is DAT_005096C0 * DAT_004312C4 (dt * scale).
float g_sim_freeze = 0.f; // DAT_004312dc
float g_sim_scale = 1.f; // DAT_004312c4
float g_sim_dt = 0.f; // DAT_005096c0

// DAT_0050E764 and DAT_0050E768, the two counts 0x00117400 keeps. The scene's actor services read the same words.
std::array<u32, 2> g_pause_references{};

// DAT_0050E768. Nonzero skips the +0x930 decay inside the base update.
i32& g_block_decay = reinterpret_cast<i32&>(g_pause_references[1]);

// Byte at [DAT_0051248C]+0x30F144. Zero skips that same decay.
u8 g_decay_arm = 0;

} // namespace scene_detail
} // namespace gbtg
