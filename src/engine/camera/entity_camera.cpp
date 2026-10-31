#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"
#include "display_view.h"
#include "../entities/player_mind_events.h"

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


#include "../entities/entity_types.h"
#include "../entities/actor_transform.h"
#include "../entities/entity.h"
#include "entity_camera.h"
#include "../entities/entity_collision.h"

namespace gbtg
{
using namespace entity_detail;


void entity_body_camera_sizeof(u32 out_sizes[2])
{
    // Xbox 0x00073170
    out_sizes[0] = 0x914;
    out_sizes[1] = 0x7C8;
}


namespace entity_detail
{


// DAT_004e1680, stride 8. Slot 0 is the sentinel. The list head read by
// 0x00048CF0 is anchor.next, at DAT_004e1684 + slot * 8.
EntityCamNode g_cam_anchor[4]{};


// DAT_004e16a0. The record currently bound in that slot.
EntityCamNode* g_cam_active[4]{};


// DAT_004e16b0, stride 0x21. Name copied from the bound record.
char g_cam_saved_name[4][0x21]{};


unsigned char cam_fold(unsigned char ch)
{
    if (ch > 0x20)
    {
        return static_cast<unsigned char>(ch & 0xDFu);
    }
    return ch;
}


// Case-fold used by 0x00048CF0: bytes above 0x20 lose bit 0x20.
bool cam_name_match(const char* node_name, const char* want)
{
    for (;;)
    {
        const unsigned char folded_node = cam_fold(static_cast<unsigned char>(*node_name));
        const unsigned char folded_want = cam_fold(static_cast<unsigned char>(*want));
        if (folded_node != folded_want)
        {
            return false;
        }
        if (folded_node == 0)
        {
            return true;
        }
        ++node_name;
        ++want;
    }
}


void copy_cam_name(char* dest, const char* src)
{
    char unit;
    do
    {
        unit = *src;
        *dest = unit;
        ++src;
        ++dest;
    }
    while (unit != 0);
}

} // namespace entity_detail
 // namespace

// Xbox 0x00048CF0
i32 camera_bind_named_view(i32 slot, const char* name)
{
    // Xbox 0x00048CF0
    // ESI walks the slot list from DAT_004e1684 through +4. EBX is the name.
    // A match stores that record in DAT_004e16a0 and returns 1. A miss clears
    // the slot and returns 0. Mask 2 seeds the pose from the current display;
    // mask 1 selects Euler direction conversion instead of quaternion copying.
    EntityCamNode* found = g_cam_anchor[slot].next;
    while (found != nullptr && !cam_name_match(found->name, name))
    {
        found = found->next;
    }
    if (found == nullptr)
    {
        g_cam_active[slot] = nullptr;
        return 0;
    }
    if ((found->flags & 2) != 0)
    {
        if (g_cam_active[slot] == nullptr)
        {
            found->position.y = 15.f; // DAT_0032B214
            found->position.x = 0.f;
            found->position.z = 30.f; // DAT_0032B108
        }
        else
        {
            found->position = g_display_view->position;
        }
        if ((found->flags & 1) == 0)
        {
            found->rotation = g_display_view->rotation;
        }
        else
        {
            Vec3 direction;
            if (g_cam_active[slot] == nullptr)
            {
                direction = {0.f, 0.f, 1.f};
            }
            else
            {
                direction.x = 0.f - g_display_view->forward.x;
                direction.y = 0.f - g_display_view->forward.y;
                direction.z = 0.f - g_display_view->forward.z;
            }
            direction_to_yaw_pitch(direction.x, direction.y, direction.z,
                found->rotation.y, found->rotation.x);
            found->rotation.z = 0.f;
        }
    }
    if (g_cam_active[slot] != nullptr)
    {
        copy_cam_name(g_cam_saved_name[slot], g_cam_active[slot]->name);
    }
    g_cam_active[slot] = found;
    i32 fit_width;
    i32 fit_x;
    i32 fit_height;
    i32 fit_y;
    float fov;
    camera_fit_display_view(found->focal, found->aspect_slot, fit_width, fit_x,
        fit_height, fit_y, fov, found->pixel_aspect, found->focal_pixels);
    return 1;
}


// Xbox 0x00048C70
void camera_unlink_view(EntityCamNode* node)
{
    // Xbox 0x00048C70
    // EAX is the camera record at body+0x894. Unlink it from the view list.
    // If it is the bound record in one of the four DAT_004e16a0 slots, try
    // camera_bind_named_view with the saved name at DAT_004e16b0. When that misses and
    // the slot list at DAT_004e1684 is still live, bind the first record by
    // its own name. An empty list clears the slot.
    if (node->prev != nullptr)
    {
        node->prev->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = node->prev;
    }
    node->prev = nullptr;
    node->next = nullptr;

    for (i32 slot = 0; slot < 4; ++slot)
    {
        if (g_cam_active[slot] != node)
        {
            continue;
        }
        if (camera_bind_named_view(slot, g_cam_saved_name[slot]) == 0)
        {
            if (g_cam_anchor[slot].next != nullptr)
            {
                camera_bind_named_view(slot, g_cam_anchor[slot].next->name);
                return;
            }
            g_cam_active[slot] = nullptr;
        }
        return;
    }
}

// Xbox 0x00048EB0
void camera_rebind_view(EntityCamNode* node)
{
    using namespace entity_detail;
    for (i32 slot = 0; slot < 4; ++slot)
    {
        if (g_cam_active[slot] != node)
        {
            continue;
        }
        if (camera_bind_named_view(slot, g_cam_saved_name[slot]) == 0)
        {
            if (g_cam_anchor[slot].next != nullptr)
            {
                camera_bind_named_view(slot, g_cam_anchor[slot].next->name);
                return;
            }
            g_cam_active[slot] = nullptr;
        }
        return;
    }
}

namespace entity_detail
{


// DAT_00545AEC. Profile 4 leaves the aim height alone; every other value adds 6.
i32 g_look_profile = 0;
  // 0x0032B27C, 2e-7
constexpr float kDeg2Rad = std::bit_cast<float>(0x3C8EFA35u);
  // 0x0032B0FC
constexpr float kPitchBiasDeg = std::bit_cast<float>(0xC0000000u);
 // DAT_0043401C, -2
constexpr float kAimLift = std::bit_cast<float>(0x40C00000u);
  // DAT_00434020, 6
constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
       // 0x0032B148
constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
    // 0x0032B298

// 0x00105AA0. fmod into one turn, then fold into [-pi, pi).
float wrap_half_turn(float angle)
{
    float wrapped = std::fmod(angle, kTwoPi);
    if (wrapped < -kPi)
    {
        wrapped += kTwoPi;
    }
    if (wrapped >= kPi)
    {
        wrapped -= kTwoPi;
    }
    return wrapped;
}

} // namespace entity_detail


// Xbox 0x00105E20
i32 direction_to_yaw_pitch(float x, float y, float z, float& yaw, float& pitch)
{
    // Xbox 0x00105E20
    // EAX is the direction. The first out is yaw = atan2(x, z). The second is
    // pitch = atan2(-y, horizontal length), where that length is sqrt(x^2+z^2)
    // and stays 0 when both axes are quiet. 0x00152BB0 is the absolute value
    // and 0x001530E0 is atan2. Each magnitude gate uses ordered JA against
    // 2e-7 (0x0032B27C), so NaN magnitudes are quiet. A dead XZ forces yaw and
    // the result to 0. Quiet Y and horizontal length force pitch to 0 and
    // return 0. The length and angles use the actual retail math routines.
    const bool xz = engine_absolute(x) > kLookEps || engine_absolute(z) > kLookEps;
    float horiz = 0.f;
    i32 live = 1;
    if (xz)
    {
        horiz = float(retail_square_root(z * z + x * x));
        yaw = float(retail_atan2(x, z));
    }
    else
    {
        yaw = 0.f;
        live = 0;
    }
    if (!(engine_absolute(y) > kLookEps || engine_absolute(horiz) > kLookEps))
    {
        pitch = 0.f;
        return 0;
    }
    pitch = float(retail_atan2(0.f - y, horiz));
    return live;
}


// Xbox 0x00072BA0
void camera_aim_actor(EntityLookAt& cam)
{
    // Xbox 0x00072BA0
    // ESI is the actor at camera+8. 0x000412A0 always writes its world position.
    // A zero byte at +0x308 returns after that. Otherwise the aim point at +0x2fc
    // is subtracted from that position. DAT_00545AEC == 4 adds nothing to aim Y;
    // any other profile adds 6 (DAT_00434020). 0x00105E20 turns the delta into
    // yaw and pitch. Actor euler at +0x14 becomes
    // (-(pitch - 2 degrees), yaw + pi, camera+0x8f4), each wrapped by 0x00105AA0.
    // Actor+0x2a8 is cleared. World euler of the actor at [actor+0x95c]+8 is
    // stored at that object +0x72c. A null actor or a null +0x95c faults on Xbox.
    if (cam.actor == nullptr)
    {
        return;
    }
    const Vec3 world = actor_world_position(*cam.actor);
    if (cam.enabled == 0)
    {
        return;
    }
    float lift = kAimLift;
    if (g_look_profile == 4)
    {
        lift = 0.f;
    }
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(cam.aim.x - world.x, cam.aim.y + lift - world.y, cam.aim.z - world.z, yaw, pitch);
    cam.actor->euler.x = wrap_half_turn(-(pitch + kPitchBiasDeg * kDeg2Rad));
    cam.actor->euler.y = wrap_half_turn(yaw + kPi);
    cam.actor->euler.z = wrap_half_turn(cam.roll);
    cam.actor->xform_valid = 0;
    if (cam.actor->euler_out != nullptr && cam.actor->euler_out->actor != nullptr)
    {
        cam.actor->euler_out->euler = actor_world_euler(*cam.actor->euler_out->actor);
    }
}


namespace entity_detail
{


// DAT_004312dc / DAT_004312c4. Freeze above 0 zeros the step (BanjoCam).
float g_sim_freeze = 0.f;

float g_sim_scale = 1.f;


struct ChannelKey
{
    float duration;
    float value;
    u32 channel; // 2 marks the row past the last live key
};


// Rows at 0x00338F64, 0x00338F88, 0x00338EE0, 0x00338EA4. Stride is three dwords.
constexpr ChannelKey kKeys0[] = {
    {0.f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {0.f, 0.f, 2},
};

constexpr ChannelKey kKeys1[] = {
    {0.f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {2.f, 0.f, 2},
};

constexpr ChannelKey kKeys2[] = {
    {2.f, 0.1f, 0},  {0.034f, 0.1f, 1}, {5.f, 0.06f, 0}, {0.034f, 0.06f, 1},
    {0.2f, 0.09f, 0}, {0.034f, 0.09f, 1}, {1.f, 0.1f, 0}, {0.034f, 0.1f, 1},
    {5.f, 0.1f, 0},  {0.034f, 0.1f, 1}, {2.f, 0.f, 2},
};

constexpr ChannelKey kKeys3[] = {
    {0.3f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {0.3f, 0.1f, 0},
    {0.034f, 0.1f, 1},
    {2.f, 0.f, 2},
};


static_assert(std::bit_cast<u32>(0.034f) == 0x3D0B4396u);

static_assert(std::bit_cast<u32>(0.06f) == 0x3D75C28Fu);

static_assert(std::bit_cast<u32>(0.09f) == 0x3DB851ECu);

static_assert(std::bit_cast<u32>(0.1f) == 0x3DCCCCCDu);

static_assert(std::bit_cast<u32>(0.2f) == 0x3E4CCCCDu);

static_assert(std::bit_cast<u32>(0.3f) == 0x3E99999Au);


std::span<const ChannelKey> keys_for(u8 mode)
{
    switch (mode)
    {
    case 0:
        return kKeys0;
    case 1:
        return kKeys1;
    case 2:
        return kKeys2;
    case 3:
        return kKeys3;
    default:
        return {};
    }
}

} // namespace entity_detail
 // namespace

// Xbox 0x0001D850
void camera_tick_channel_keys(EntityChannelAnim& anim, float dt)
{
    // Xbox 0x0001D850
    // ESI is the camera. Called from the type slot at 0x00072AC0 when +0xfc is set.
    // Phase 1 returns. Otherwise the step is host+0x92c (0 when host+0x930 > 0)
    // times DAT_005096C0 * DAT_004312C4, or 0 when DAT_004312DC > 0.
    // Each expiry writes the live key's value into +0xe4 + channel*0xc, sets the
    // mark byte at +8, and loads the following key's duration into +0x8c.
    // Channel 2 on that following key ends the row: mode 0 latches phase 1,
    // mode 1 restarts on the mode-2 row, and any other mode restarts at index 0.
    if (anim.phase == 1)
    {
        return;
    }
    float span = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        span = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(anim.host_gate > 0.f))
    {
        rate = anim.host_rate;
    }
    float remain = rate * span;
    while (remain > 0.f)
    {
        anim.timer -= remain;
        if (anim.timer > 0.f)
        {
            return;
        }
        remain = std::fabs(anim.timer);
        const std::span<const ChannelKey> keys = keys_for(anim.mode);
        if (keys.empty() || static_cast<size_t>(anim.index) + 1u >= keys.size())
        {
            return;
        }
        const ChannelKey& cur = keys[anim.index];
        const ChannelKey& next = keys[anim.index + 1u];
        if (cur.channel < 2)
        {
            anim.keys[cur.channel].mark = 1;
            anim.keys[cur.channel].value = cur.value;
        }
        anim.timer = next.duration;
        if (next.channel == 2)
        {
            if (anim.mode == 0)
            {
                anim.phase = 1;
                // The mode-0 terminal duration is 0. A positive overshoot would
                // subtract from that 0 and select this same key again.
                if (!(anim.timer > 0.f))
                {
                    return;
                }
            }
            else
            {
                anim.index = 0;
                if (anim.mode == 1)
                {
                    anim.mode = 2;
                    anim.timer = kKeys2[0].duration;
                    anim.phase = 2;
                }
            }
        }
        else
        {
            anim.index = static_cast<u8>(anim.index + 1u);
        }
    }
}


// Xbox 0x0001EDE0
void camera_tick_channel_blends(EntityChannelAnim& anim, float dt)
{
    // Xbox 0x0001EDE0
    // EAX is the camera. 0x00072AC0 jumps here after 0x0001D850 when +0xfc is set.
    // The step is host+0x92c (0 when host+0x930 > 0) times DAT_005096C0 * DAT_004312C4,
    // or 0 when DAT_004312DC > 0. Each slot at +0xe4 (stride 0x0c) reads the mark at +8
    // and moves the blend at +4 by step/value:
    //   1 rises to 1, then the mark becomes 2
    //   2 becomes 3 and holds the blend for that frame
    //   3 and 4 fall to 0, then the mark becomes 0
    //   5 rises to 1, then the mark becomes 0
    // Any other mark leaves the blend stored as it was.
    float scale = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        scale = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(anim.host_gate > 0.f))
    {
        rate = anim.host_rate;
    }
    const float step = rate * scale;
    for (EntityChannelKey& key : anim.keys)
    {
        float blend = key.blend;
        switch (key.mark)
        {
        case 1:
            blend = blend + step / key.value;
            if (blend > 1.f)
            {
                key.mark = 2;
                blend = 1.f;
            }
            key.blend = blend;
            break;
        case 2:
            key.mark = 3;
            break;
        case 3:
        case 4:
            blend = blend - step / key.value;
            if (blend < 0.f)
            {
                blend = 0.f;
                key.mark = 0;
            }
            key.blend = blend;
            break;
        case 5:
            blend = blend + step / key.value;
            if (blend > 1.f)
            {
                key.mark = 0;
                blend = 1.f;
            }
            key.blend = blend;
            break;
        default:
            break;
        }
    }
}


// Xbox 0x0001EA90
void camera_ease_channel_targets(EntityChannelEase& cam, float dt)
{
    // Xbox 0x0001EA90
    // EDX is the camera. 0x00072AC0 calls this when +0x154 > 0, then 0x0001D850
    // when +0xfc is set. Each row's target is cleared, then the bank at
    // +0x100 + index*8 (index from +0x158) copies its value onto the first row
    // with the same id. The value at +4 then moves toward that target by at
    // most host_rate * DAT_005096C0 * DAT_004312C4 * 0.75 (0x0032B2AC).
    // DAT_004312DC > 0 zeros the scale; host+0x930 > 0 zeros the rate. A zero
    // step leaves the value in place. A step past the gap snaps to the target.
    // The target is cleared again. A bank id with no row faults on Xbox.
    for (EntityChannelRow& row : cam.rows)
    {
        row.target = 0.f;
    }
    if (cam.bank >= 0 && cam.bank < 10)
    {
        for (const EntityChannelPair& pair : cam.banks[cam.bank])
        {
            for (EntityChannelRow& row : cam.rows)
            {
                if (row.id == pair.id)
                {
                    row.target = pair.value;
                    break;
                }
            }
        }
    }
    float scale = 0.f;
    if (!(g_sim_freeze > 0.f))
    {
        scale = dt * g_sim_scale;
    }
    float rate = 0.f;
    if (!(cam.host_gate > 0.f))
    {
        rate = cam.host_rate;
    }
    float step = rate * scale;
    if (!cam.rows.empty())
    {
        step *= 0.75f;
    }
    for (EntityChannelRow& row : cam.rows)
    {
        const float delta = row.target - row.value;
        float next = row.value;
        if (step != 0.f)
        {
            if (step > std::fabs(delta))
            {
                next = row.target;
            }
            else if (delta >= 0.f)
            {
                next = row.value + step;
            }
            else
            {
                next = row.value - step;
            }
        }
        row.value = next;
        row.target = 0.f;
    }
}


// Xbox 0x0001FE00
u32 camera_collision_mask(const EntityMaskHost& host)
{
    // EAX is the camera; camera +8's +0x980 is its goal ring. A null ring reads code 0; an empty ring (tail +6
    // equal to capacity +2) reads the u16 at address 0; otherwise the code is the row at head * stride. Code
    // 0x15 keeps 0xC4000. Mode 2, or a nonzero +0x93c, clears bit 0x4000. Modes 1, 4 and 5 clear bit 0x40000;
    // mode 3 clears bit 0x80000. Any other mode keeps 0xC4000.
    constexpr u32 kMask = 0xC4000u;
    u16 code = 0;
    if (const RuntimePlayerGoalRing* ring = host.ring; ring != nullptr)
    {
        const u8* front = ring->tail == ring->capacity ? entity_detail::g_address_zero.data()
            : ring->rows.data() + size_t(ring->head) * ring->stride;
        std::memcpy(&code, front, sizeof(code));
    }
    if (code != 0x15)
    {
        if (host.mode == 2 || host.flag != 0)
        {
            return kMask & ~0x4000u;
        }
        switch (host.mode)
        {
        case 1:
        case 4:
        case 5:
            return kMask & ~0x40000u;
        case 3:
            return kMask & ~0x80000u;
        default:
            break;
        }
    }
    return kMask;
}

namespace entity_detail
{


// DAT_005077F8. Row of the triple table at 0x0042A460. 0 is 4:3, 1 is 16:9, 2 is 2.35.
i32 g_aspect_preset = 0;

// DAT_00509704. Zero scales the chosen aspect by 1.1 (0x0032B1A0). Any other byte leaves it.
u8 g_aspect_exact = 0;
u8 g_video_widescreen = 0;


// DAT_005077A8. 0x000F5900 points this at the display record and copies the viewport
// (DAT_005096E8 / DAT_005096EA) into the size fields. A null pointer faults on Xbox.
DisplayView g_display_records[4]{};

DisplayView* g_display_view = &g_display_records[0];


// 0x0042A460 stride 0x0c: aspect, sensor height, default focal. Index 3 is the sentinel row.
struct AspectTriple
{
    float aspect;
    float sensor;
    float focal;
};


constexpr AspectTriple kAspectTable[] = {
    {std::bit_cast<float>(0x3FAAAAABu), std::bit_cast<float>(0x4178B780u), std::bit_cast<float>(0x416EE3E8u)},
    {std::bit_cast<float>(0x3FE38E39u), std::bit_cast<float>(0x41C00625u), std::bit_cast<float>(0x41B86FF5u)},
    {std::bit_cast<float>(0x40166666u), std::bit_cast<float>(0x41C00625u), std::bit_cast<float>(0x41B86FF5u)},
    {0.f, 0.f, 0.f},
};


constexpr float kAspectPad = std::bit_cast<float>(0x3F8CCCCDu);
 // 0x0032B1A0, 1.1
constexpr float kFovScale = std::bit_cast<float>(0x42E52EE0u);
  // 0x0032B360, 360/pi
constexpr float kHalfRad = std::bit_cast<float>(0x3C0EFA35u);
   // 0x0032B358, pi/360
constexpr float kNegOne = std::bit_cast<float>(0xBF800000u);
    // 0x0032B120

const AspectTriple& aspect_triple(i32 index)
{
    if (index < 0 || index >= 4)
    {
        return kAspectTable[3];
    }
    return kAspectTable[index];
}


float chosen_aspect(i32 slot)
{
    i32 index = slot;
    if (slot == 3)
    {
        index = g_aspect_preset;
    }
    float aspect = aspect_triple(index).aspect;
    if (g_aspect_exact == 0)
    {
        aspect *= kAspectPad;
    }
    return aspect;
}


// cvttss2si. Out of range and NaN become the indefinite integer.
i32 trunc_i32(float v)
{
    if (!(v >= -2147483648.f && v < 2147483648.f))
    {
        return static_cast<i32>(0x80000000u);
    }
    return static_cast<i32>(v);
}


// Xbox 0x000F5760
// ECX is the aspect slot, ESI and the stack pointer receive the span and the origin.
void fit_vertical(i32 slot, i32& span, i32& pos, const DisplayView& view)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = view.height;
    span = trunc_i32(static_cast<float>(limit) * (ref / selected));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(view.origin_y));
}


// Xbox 0x000F56C0
// Same slot. The span is the wide axis and the second out is its centered origin.
void fit_horizontal(i32 slot, i32& span, i32& pos, const DisplayView& view)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = view.width;
    span = trunc_i32(static_cast<float>(limit) * (selected / ref));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(view.origin_x));
}

} // namespace entity_detail
 // namespace

void fit_display_viewport(i32 width, i32 height, i32 aspect_slot,
    i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y)
{
    const DisplayView view{aspect_slot, 0, 0, width, height};
    fit_vertical(aspect_slot, fit_h, fit_y, view);
    fit_horizontal(aspect_slot, fit_w, fit_x, view);
}


// Xbox 0x000F57F0
void camera_fit_display_view(float focal, i32 aspect_slot, i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y, float& fov_deg,
              float& pixel_aspect, float& focal_px)
{
    // Xbox 0x000F57F0
    // EDI is the aspect slot. ECX receives the fitted height and EAX its centered y.
    // The stack receives the fitted width and its x, the vertical FOV in degrees,
    // the pixel aspect, and the focal length in pixels. 0x000F5760 runs first, then
    // 0x000F56C0. The sensor row is display +0x3c (3 means DAT_005077F8). A focal of
    // -1 uses the table focal for EDI the same way. FOV is
    // 2 * atan(sensor / (2 * focal)) degrees. The pixel focal is (height / 2) *
    // cot(half of that angle): sin is 0x00153320, cos is 0x00153230, atan is
    // 0x00152780. Pixel aspect divides width/height by the preset row's aspect.
    fit_vertical(aspect_slot, fit_h, fit_y, *g_display_view);
    fit_horizontal(aspect_slot, fit_w, fit_x, *g_display_view);

    i32 sensor_slot = g_display_view->aspect_mode;
    if (sensor_slot == 3)
    {
        sensor_slot = g_aspect_preset;
    }
    float used = focal;
    if (used == kNegOne)
    {
        i32 focal_slot = aspect_slot;
        if (focal_slot == 3)
        {
            focal_slot = g_aspect_preset;
        }
        used = aspect_triple(focal_slot).focal;
    }
    fov_deg = std::atan(aspect_triple(sensor_slot).sensor / (used * 2.f)) * kFovScale;
    const float half = fov_deg * kHalfRad;
    const float height = static_cast<float>(g_display_view->height);
    const float width = static_cast<float>(g_display_view->width);
    focal_px = (height * std::cos(half) / std::sin(half)) * 0.5f;
    pixel_aspect = (width / height) / aspect_triple(g_aspect_preset).aspect;
}
 // namespace

} // namespace gbtg
