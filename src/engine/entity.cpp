#include "gbtg/entity.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <span>

namespace gbtg
{
namespace
{

std::vector<EntityType> g_types;

void stub_tick(Entity&)
{
}

void stub_shutdown(Entity&)
{
}

bool stub_message(Entity&, const Message&)
{
    return false;
}

void add_named(const char* name, EntityKind kind)
{
    EntityType t;
    t.name = name;
    t.kind = kind;
    t.tick = stub_tick;
    t.shutdown = stub_shutdown;
    t.handle_message = stub_message;
    g_types.push_back(t);
}

constexpr i32 kHeapSlotCount = 4;

struct HeapSlot
{
    u32 id = 0;             // record + 0x20
    void* handle = nullptr; // record + 0x24 (DAT_00507834 for slot 0)
};

// DAT_00507810 stride 0x30. g_heap_current is DAT_00507808.
HeapSlot g_heap_slots[kHeapSlotCount];
i32 g_heap_current = 0;

i32 heap_index_for(void* block)
{
    // FUN_000f7100. EDI is the block. Four handles at DAT_00507834, stride 0x30.
    // msize (0x0014DED0) failure forces the footer byte to 0xFF, which cannot
    // match a heap id. The allocator at 0x000F72D0 writes that id at msize-2.
    // No match returns 3.
    for (i32 i = 0; i < kHeapSlotCount; ++i)
    {
        if (g_heap_slots[i].handle == nullptr || block == nullptr)
        {
            continue;
        }
        const u32 tag = 0xFFu;
        if (tag == g_heap_slots[i].id)
        {
            return i;
        }
    }
    return 3;
}

} // namespace

Vec3 actor_world_position(const Actor& actor)
{
    // Xbox 0x000412A0: if attach+8 is live, multiply local by parent matrix.
    // Unattached path copies actor+0x08.
    if (actor.attach == nullptr)
    {
        return actor.position;
    }
    const Vec3 p = actor.attach->position;
    return Vec3{actor.position.x + p.x, actor.position.y + p.y, actor.position.z + p.z};
}

Vec3 actor_world_euler(const Actor& actor)
{
    // Xbox 0x00041820: unattached copies actor+0x14; attached adds parent euler.
    if (actor.attach == nullptr)
    {
        return actor.euler;
    }
    const Vec3 e = actor.attach->euler;
    return Vec3{actor.euler.x + e.x, actor.euler.y + e.y, actor.euler.z + e.z};
}

Vec3 fn_104040(const float m[12])
{
    // Xbox 0x00104040
    // Copies the 12 floats, then 0x00103AF0 (EAX = copy, arg = 1) normalizes
    // each column of the 3x3. A zero-length column is left as it is. Pitch is
    // asin(-row1.z), saturated to +/- pi/2 (0x0032B140 / 0x0032B13C). Yaw is
    // atan2(row0.z, row2.z), or atan2(row0.y, row2.y) when those two are 0.
    // Roll is atan2(row1.x, row1.y), or 0 when both are 0. 0x00152E90 is asin
    // and 0x001530E0 is atan2. The three angles are written through EBX.
    float a[12];
    std::memcpy(a, m, sizeof(a));
    for (int col = 0; col < 3; ++col)
    {
        const float x = a[col];
        const float y = a[col + 4];
        const float z = a[col + 8];
        const float len2 = x * x + y * y + z * z;
        if (len2 != 0.f)
        {
            const float inv = 1.f / std::sqrt(len2);
            a[col] = x * inv;
            a[col + 4] = y * inv;
            a[col + 8] = z * inv;
        }
    }

    constexpr float kHalfPi = std::bit_cast<float>(0x3FC90FDBu);
    constexpr float kNegHalfPi = std::bit_cast<float>(0xBFC90FDBu);
    const float pitch_arg = 0.f - a[6];
    float pitch = kHalfPi;
    if (pitch_arg < 1.f)
    {
        if (-1.f < pitch_arg)
        {
            pitch = std::asin(pitch_arg);
        }
        else
        {
            pitch = kNegHalfPi;
        }
    }

    float yaw = 0.f;
    if (a[2] == 0.f && a[10] == 0.f)
    {
        yaw = std::atan2(a[1], a[9]);
    }
    else
    {
        yaw = std::atan2(a[2], a[10]);
    }

    float roll = 0.f;
    if (a[4] != 0.f || a[5] != 0.f)
    {
        roll = std::atan2(a[4], a[5]);
    }
    return Vec3{pitch, yaw, roll};
}

void fn_41730(Actor& actor, const Vec3& point)
{
    // Xbox 0x00041730
    // EDI is the actor and ESI is the source point. A bind at +0x88 whose
    // parent (+8) is set, or whose actor+0x918 link has a non-zero head,
    // multiplies the point by the 3x4 at bind+0x3c and writes actor+0x08.
    // A dead bind is released through 0x000663E0 and the point is copied.
    // Both paths clear the matrix stamp at actor+0x2a8.
    if (actor.bind != nullptr)
    {
        const bool parent_live = actor.bind->parent != nullptr;
        const bool link_live = actor.link != nullptr && actor.link->head != nullptr;
        if (parent_live || link_live)
        {
            const float* m = actor.bind->m;
            actor.position.x = m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3];
            actor.position.y = m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7];
            actor.position.z = m[8] * point.x + m[9] * point.y + m[10] * point.z + m[11];
            actor.xform_valid = 0;
            return;
        }
        fn_663e0(actor.bind);
        actor.bind = nullptr;
    }
    actor.position = point;
    actor.xform_valid = 0;
}

void fn_663e0(void* block)
{
    // Xbox 0x000663E0
    // EAX is the block. The owning slot is selected, DAT_00507808 is switched
    // to that index for HeapFree(handle, 0, block), then the previous index
    // is restored. A null slot faults on Xbox; the process heap releases it.
    const i32 index = heap_index_for(block);
    const i32 saved = g_heap_current;
    const void* handle = g_heap_slots[index].handle;
    g_heap_current = index;
    if (handle != nullptr || block != nullptr)
    {
        std::free(block);
    }
    g_heap_current = saved;
}

void fn_fcd70(void* dest, const void* src)
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
static_assert(std::bit_cast<u32>(0.01f) == 0x3C23D70Au);  // 0x0032B124
static_assert(std::bit_cast<u32>(-0.01f) == 0xBC23D70Au); // 0x0032B270

i32 fn_10ae90(const EntityMatchBlock& left, const EntityMatchBlock& right)
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

void entity_body_camera_sizeof(u32 out_sizes[2])
{
    // Xbox 0x00073170
    out_sizes[0] = 0x914;
    out_sizes[1] = 0x7C8;
}

void entity_body_camera_tick(Entity& body)
{
    // Xbox 0x000731F0: clear a flag, insert into the camera list, copy actor pose.
    if (body.actor == nullptr)
    {
        return;
    }
    body.pose_position = actor_world_position(*body.actor);
    body.pose_euler = actor_world_euler(*body.actor);
    body.pose_quat = body.actor->quat;
    body.pose_fov = body.actor->fov;
    body.listed = true;
}

void entity_body_camera_shutdown(Entity& body)
{
    // Xbox 0x00073300
    body.listed = false;
}

bool entity_body_camera_message(Entity& body, const Message& msg)
{
    // Xbox 0x00073320: id 99 translates body+0x18; other ids fall through.
    if (msg.id != kMsgCameraTranslate)
    {
        return false;
    }
    body.position.x += msg.vec.x;
    body.position.y += msg.vec.y;
    body.position.z += msg.vec.z;
    return true;
}

namespace
{

// DAT_00545AEC. Profile 4 leaves the aim height alone; every other value adds 6.
i32 g_look_profile = 0;

constexpr float kLookEps = std::bit_cast<float>(0x3456BF95u);  // 0x0032B27C, 2e-7
constexpr float kDeg2Rad = std::bit_cast<float>(0x3C8EFA35u);  // 0x0032B0FC
constexpr float kPitchBiasDeg = std::bit_cast<float>(0xC0000000u); // DAT_0043401C, -2
constexpr float kAimLift = std::bit_cast<float>(0x40C00000u);  // DAT_00434020, 6
constexpr float kPi = std::bit_cast<float>(0x40490FDBu);       // 0x0032B148
constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);    // 0x0032B298

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

} // namespace

i32 fn_105e20(float x, float y, float z, float& yaw, float& pitch)
{
    // Xbox 0x00105E20
    // EAX is the direction. The first out is yaw = atan2(x, z). The second is
    // pitch = atan2(-y, horizontal length), where that length is sqrt(x^2+z^2)
    // and stays 0 when both axes are quiet. 0x00152BB0 is the absolute value
    // and 0x001530E0 is atan2. A component no larger than 2e-7 (0x0032B27C)
    // counts as zero. A dead XZ forces yaw to 0 and the result to 0. A dead Y
    // together with that zero length forces pitch to 0 and returns 0. A live
    // XZ returns 1 after both angles are written.
    const bool xz = std::fabs(x) > kLookEps || std::fabs(z) > kLookEps;
    float horiz = 0.f;
    i32 live = 1;
    if (xz)
    {
        horiz = std::sqrt(x * x + z * z);
        yaw = std::atan2(x, z);
    }
    else
    {
        yaw = 0.f;
        live = 0;
    }
    if (std::fabs(y) <= kLookEps && std::fabs(horiz) <= kLookEps)
    {
        pitch = 0.f;
        return 0;
    }
    pitch = std::atan2(-y, horiz);
    return live;
}

void fn_72ba0(EntityLookAt& cam)
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
    fn_105e20(cam.aim.x - world.x, cam.aim.y + lift - world.y, cam.aim.z - world.z, yaw, pitch);
    cam.actor->euler.x = wrap_half_turn(-(pitch + kPitchBiasDeg * kDeg2Rad));
    cam.actor->euler.y = wrap_half_turn(yaw + kPi);
    cam.actor->euler.z = wrap_half_turn(cam.roll);
    cam.actor->xform_valid = 0;
    if (cam.actor->euler_out != nullptr && cam.actor->euler_out->actor != nullptr)
    {
        cam.actor->euler_out->euler = actor_world_euler(*cam.actor->euler_out->actor);
    }
}

namespace
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

} // namespace

void fn_1d850(EntityChannelAnim& anim, float dt)
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

void fn_1ede0(EntityChannelAnim& anim, float dt)
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

void fn_1ea90(EntityChannelEase& cam, float dt)
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

namespace
{

// DAT_005077F8. Row of the triple table at 0x0042A460. 0 is 4:3, 1 is 16:9, 2 is 2.35.
i32 g_aspect_preset = 0;
// DAT_00509704. Zero scales the chosen aspect by 1.1 (0x0032B1A0). Any other byte leaves it.
u8 g_aspect_exact = 0;

// DAT_005077A8. 0x000F5900 points this at the display record and copies the viewport
// (DAT_005096E8 / DAT_005096EA) into the size fields. A null pointer faults on Xbox.
struct DisplayView
{
    i32 aspect_mode = 0; // +0x3c. 3 selects g_aspect_preset.
    i32 origin_x = 0;    // +0x48
    i32 origin_y = 0;    // +0x4c
    i32 width = 0;       // +0x50
    i32 height = 0;      // +0x54
};

DisplayView g_display{};
DisplayView* g_display_view = &g_display;

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

constexpr float kAspectPad = std::bit_cast<float>(0x3F8CCCCDu); // 0x0032B1A0, 1.1
constexpr float kFovScale = std::bit_cast<float>(0x42E52EE0u);  // 0x0032B360, 360/pi
constexpr float kHalfRad = std::bit_cast<float>(0x3C0EFA35u);   // 0x0032B358, pi/360
constexpr float kNegOne = std::bit_cast<float>(0xBF800000u);    // 0x0032B120

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

// 0x000F5760. ECX is the aspect slot, ESI and the stack pointer receive the span and the origin.
void fit_vertical(i32 slot, i32& span, i32& pos)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = g_display_view->height;
    span = trunc_i32(static_cast<float>(limit) * (ref / selected));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(g_display_view->origin_y));
}

// 0x000F56C0. Same slot. The span is the wide axis and the second out is its centered origin.
void fit_horizontal(i32 slot, i32& span, i32& pos)
{
    const float ref = aspect_triple(g_aspect_preset).aspect;
    const float selected = chosen_aspect(slot);
    const i32 limit = g_display_view->width;
    span = trunc_i32(static_cast<float>(limit) * (selected / ref));
    if (span > limit)
    {
        span = limit;
    }
    const i32 remain = static_cast<i32>(static_cast<u32>(limit) - static_cast<u32>(span));
    pos = trunc_i32(static_cast<float>(remain) * 0.5f + static_cast<float>(g_display_view->origin_x));
}

} // namespace

void fn_f57f0(float focal, i32 aspect_slot, i32& fit_w, i32& fit_x, i32& fit_h, i32& fit_y, float& fov_deg,
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
    fit_vertical(aspect_slot, fit_h, fit_y);
    fit_horizontal(aspect_slot, fit_w, fit_x);

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

void register_entity_types()
{
    if (!g_types.empty())
    {
        return;
    }
    g_types.reserve(64);

    // Xbox 0x00073190 entityBodyCamera_ctor: fill defaults at 0x1DA60, then overwrite slots.
    EntityType camera;
    camera.name = "entityBodyCamera";
    camera.kind = EntityKind::Body;
    camera.xbox_desc_va = 0x00504F80;
    camera.xbox_size = 0x914;
    camera.xbox_extra = 0x7C8;
    camera.tick = entity_body_camera_tick;
    camera.shutdown = entity_body_camera_shutdown;
    camera.handle_message = entity_body_camera_message;
    g_types.push_back(camera);

    const char* bodies[] = {"entityBodyPlayer", "entityBodyHuman", "entityBodyZombie", "entityBodyVampire",
                            "entityBodyWorm", "entityBodyJessieClyde"};
    for (const char* n : bodies)
    {
        add_named(n, EntityKind::Body);
    }
    const char* minds[] = {"entityMindPlayer", "entityMindHuman", "entityMindCinema"};
    for (const char* n : minds)
    {
        add_named(n, EntityKind::Mind);
    }
    const char* strategies[] = {
        "entityStrategyPlayer",        "entityStrategyDirector",     "entityStrategyGeneric",
        "entityStrategyFNPC",          "entityStrategyBaron",        "entityStrategyBaronPlane",
        "entityStrategyChicken",       "entityStrategyGhoulieGirl",  "entityStrategyGiantHand",
        "entityStrategyGrimReaper",    "entityStrategyHauntedChair", "entityStrategyHauntedCoat",
        "entityStrategyHauntedDoor",   "entityStrategyHauntedPicture","entityStrategyHauntedTV",
        "entityStrategyHunchback",     "entityStrategyImp",          "entityStrategyImpFire",
        "entityStrategyImpFlying",     "entityStrategyImpNinja",     "entityStrategyJessieClyde",
        "entityStrategyMedusa",        "entityStrategyMummy",        "entityStrategyMummyCursed",
        "entityStrategyPirate",        "entityStrategyRibs",         "entityStrategySkeleton",
        "entityStrategySpider",        "entityStrategyUglyGirl",     "entityStrategyVampire",
        "entityStrategyVampireChicken","entityStrategyWarlock",      "entityStrategyWorm",
        "entityStrategyZombie"};
    for (const char* n : strategies)
    {
        add_named(n, EntityKind::Strategy);
    }
}

const EntityType* find_entity_type(std::string_view name)
{
    for (const auto& t : g_types)
    {
        if (t.name == name)
        {
            return &t;
        }
    }
    return nullptr;
}

std::vector<const EntityType*> all_entity_types()
{
    std::vector<const EntityType*> out;
    out.reserve(g_types.size());
    for (const auto& t : g_types)
    {
        out.push_back(&t);
    }
    return out;
}

} // namespace gbtg
