#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>


#include "entity_types.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"

namespace gbtg
{
using namespace entity_detail;

// Xbox 0x000FCD70
void entity_copy_match_block(void* dest, const void* src)
{
    // Xbox 0x000FCD70
    // EAX is the destination and ECX is the source. Copies 0x54 bytes forward.
    // Equal pointers return without a write. 0x000FCCC0 appends records of this
    // size, and other callers plant the block at object +0x1cc and +0xc48.
    if (dest == src)
    {
        return;
    }
    std::memcpy(dest, src, 0x54);
}


static_assert(sizeof(EntityMatchBlock) == 0x28);

static_assert(offsetof(EntityMatchBlock, value) == 4);

static_assert(std::bit_cast<u32>(0.01f) == 0x3C23D70Au);
  // 0x0032B124
static_assert(std::bit_cast<u32>(-0.01f) == 0xBC23D70Au);
 // 0x0032B270

// Xbox 0x0010AE90
i32 entity_match_blocks_equal(const EntityMatchBlock& left, const EntityMatchBlock& right)
{
    // Xbox 0x0010AE90
    // ECX is left and EAX is right. cam_72cb0 calls this with the slot index
    // at 2, passing slot 0 + 0x28 and slot 2 + 0x28 of the 0x54-byte records
    // copied by 0x000FCD70. Both bytes at +0 must be 0. Each of the nine floats
    // at +4 must satisfy -0.01 <= left - right <= 0.01 (0x0032B270 / 0x0032B124).
    // The bounds are inclusive. Any other case returns 0.
    if (left.mark != 0 || right.mark != 0)
    {
        return 0;
    }
    for (int i = 0; i < 9; ++i)
    {
        const float diff = left.value[i] - right.value[i];
        if (diff < -0.01f || diff > 0.01f)
        {
            return 0;
        }
    }
    return 1;
}

namespace entity_detail
{


// DAT_0050dd80. Three BSS floats. Message 9 copies them onto the actor and the body.
Vec3 g_anchor{};


struct SegmentProbe;


WorldLists g_world{};

} // namespace entity_detail

// Xbox 0x00152BB0
float engine_absolute(float value)
{
    // Xbox 0x00152BB0
    // Absolute value. Non-negative inputs, including zero, are returned as
    // stored. A negative input is returned as 0 - value.
    if (0.f <= value)
    {
        return value;
    }
    return 0.f - value;
}


// Xbox 0x001534C0
float engine_reciprocal_sqrt(float value)
{
    // Xbox 0x001534C0
    // Reciprocal square root. 0x00153450 seeds from
    // ((0x7F000000 - bits) >> 1) + 0x1FC00000, multiplies by 1/sqrt(2) when
    // bit 0x08000000 is clear, and takes two Newton steps. Two further steps
    // refine that estimate. Every input, including zero and negatives, runs
    // the same steps.
    const u32 bits = std::bit_cast<u32>(value);
    float y = std::bit_cast<float>(((0x7F000000u - bits) >> 1) + 0x1FC00000u);
    if ((bits & 0x08000000u) == 0u)
    {
        y *= std::bit_cast<float>(u32{0x3F3504F3u});
    }
    const float half = value * 0.5f;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    return y;
}


// Xbox 0x001233C0
void entity_clear_scene_lists()
{
    // Xbox 0x001233C0
    // cam_11c480 calls this before the aid-list release. Every head from
    // DAT_00511AA0 through DAT_00511B58 is stored as zero. The nodes stay
    // allocated; later walks no longer find them.
    g_world = {};
}

} // namespace gbtg
