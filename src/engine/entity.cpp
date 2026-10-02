#include "gbtg/entity.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>
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
