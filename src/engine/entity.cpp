#include "gbtg/entity.hpp"

#include <cstdlib>

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
