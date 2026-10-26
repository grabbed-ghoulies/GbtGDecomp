#include "furnace_actor.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "player_mind_events.h"
#include "segment_records.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/model_runtime.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
struct Contact
{
    entity_detail::SegmentProbe* object;
    GhoulySpawnPool* records;
};

// The DAT_00511A1C slots 0x00122B00 filled, each with its own copy of the slot's records (0x000FCC90).
std::array<Contact, 16> keep_contacts(i32 count)
{
    std::array<Contact, 16> kept;
    for (i32 index = 0; index < count; ++index)
    {
        auto& slot = entity_detail::g_segment_slots[index]; // DAT_00511A1C.
        kept[index] = {slot.object, copy_segment_records(slot.pool)}; // 0x000FCC90
    }
    return kept;
}

void clear_pool(std::span<u8> fields, size_t offset, GhoulySpawnPool*& pool)
{
    if (pool != nullptr)
    {
        restore_runtime_body_collision_overrides(*pool); // 0x0013EE80, ECX the pool
        engine_heap_free(pool); // 0x000663E0, EAX the pool
        pool = nullptr;
        const u32 zero = 0;
        std::memcpy(fields.data() + offset, &zero, 4);
    }
}

// Node - 0x730, actor +0x204: a model whose first word (its header) is set.
bool has_model(const Actor& actor)
{
    return actor.model != nullptr && actor.model->cpu.data() != nullptr;
}
}

// Xbox 0x0006E1A0
void furnace_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0x93C;
    sizes.parameters = 0x334;
}

// Xbox 0x0006E210
void release_runtime_furnace(std::span<u8> fields, GhoulySpawnPool*& burns, GhoulySpawnPool*& warmths)
{
    clear_pool(fields, 0x934, burns);
    clear_pool(fields, 0x938, warmths);
}

// Xbox 0x0006E2A0
void handle_runtime_furnace_message(std::span<u8> fields, GhoulySpawnPool* burns, GhoulySpawnPool* warmths,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings)
{
    (void)fields;
    dispatch_base_actor_word_message(base, message, bindings); // 0x0003EF70
    if (message[0] != 0x7C)
    {
        return;
    }
    // EDX the message's actor: 0x0013EDD0 compares it with each row's actor and dereferences nothing else, so an
    // address with no scene actor matches no row.
    Actor* object = actor_at(message[1]);
    if (burns != nullptr && object != nullptr)
    {
        remove_runtime_body_contact_override(burns, *object); // 0x0013EDD0, ESI +0x934
    }
    if (warmths != nullptr && object != nullptr)
    {
        remove_runtime_body_contact_override(warmths, *object); // 0x0013EDD0, ESI +0x938
    }
}

// Xbox 0x0006E2F0
void burst_runtime_furnace_flame(const Vec3& point, Actor& object, Actor& furnace, RuntimeFurnaceContacts& contacts)
{
    const Vec3 at = actor_world_position(object); // 0x000412A0, ESI the object
    const float scale = furnace.radius_xz; // Furnace +0x38.
    float dx = at.x - point.x;
    float dy = at.y - point.y;
    float dz = at.z - point.z;
    const float length = (dx * dx + dz * dz) + dy * dy;
    if (length != 0.f)
    {
        // x87: the reciprocal root times the scale and DAT_0032B66C, then each component.
        const long double reach = static_cast<long double>(engine_reciprocal_sqrt(length)) * scale * 42.5f;
        dx = float(dx * reach);
        dy = float(dy * reach);
        dz = float(dz * reach);
    }
    const Vec3 toward{at.x - point.x, at.y - point.y, at.z - point.z};
    Vec3 euler{0.f, 0.f, 0.f};
    runtime_direction_angle(toward, euler.y); // 0x00105F90, EBX euler +4
    g_actor_creation_options.position = Vec3{point.x + dx, point.y + dy, point.z + dz}; // DAT_004E1C60.
    g_actor_creation_options.euler = euler;
    g_actor_creation_options.flags = 5; // DAT_004E1C5C.
    dispatch_actor_attachment_records(contacts.resource, 0x16, nullptr, &g_actor_creation_options, contacts.callouts,
        0); // 0x0003BE20 over furnace +0x8C
}

// Xbox 0x0006E4D0
void burn_runtime_furnace_contacts(Actor& furnace, const Vec3& point, GhoulySpawnPool*& burns,
    RuntimeFurnaceContacts& contacts)
{
    if (burns != nullptr)
    {
        age_runtime_body_contact_overrides(*burns, contacts.clock); // 0x0013ED20
    }
    for (RuntimeSegmentContactActor* player : contacts.marked())
    {
        // Node +0x3A8 and +8: a player (+0xCDC 2, or +0x93C set) stays out of the furnace's burns.
        if (player->actor.mask_mode == 2 || player->actor.mask_flag != 0)
        {
            insert_runtime_body_contact_override(burns, *player, 0, contacts.players); // 0x0013EBE0
        }
    }
    if (burns != nullptr)
    {
        suspend_runtime_body_contact_overrides(*burns); // 0x0013EE10
    }
    // 0x00122B00, the radius times DAT_0032B1E0.
    Vec3 origin = point;
    const i32 count = entity_collect_contact_probes(origin, furnace.radius_xz * 50.f, 0x20002);
    if (count != 0)
    {
        RuntimeKnockdownSphereNotice notice{0x16, 0, &furnace, nullptr, {}};
        const std::array<Contact, 16> kept = keep_contacts(count);
        for (i32 index = 0; index < count; ++index)
        {
            notice.point = point;
            notice.contacts = kept[index].records;
            auto& object = contacts.contact_actor(*kept[index].object);
            object.message(&notice); // Vtable +0xC.
            engine_heap_free(kept[index].records); // HeapFree on the block's heap (0x000F7100).
            if (notice.reply != 0)
            {
                insert_runtime_body_contact_override(burns, object, 1, contacts.players); // 0x0013EBE0
                burst_runtime_furnace_flame(point, object.actor, furnace, contacts); // 0x0006E2F0
            }
        }
    }
    if (burns != nullptr)
    {
        restore_runtime_body_collision_overrides(*burns); // The inlined 0x0013EE80.
    }
}

// Xbox 0x0006E6F0
void warm_runtime_furnace_contacts(Actor& furnace, const Vec3& point, GhoulySpawnPool*& warmths,
    RuntimeFurnaceContacts& contacts)
{
    if (warmths != nullptr)
    {
        age_runtime_body_contact_overrides(*warmths, contacts.clock); // 0x0013ED20
        expire_runtime_body_contact_overrides(warmths, 0.35f); // 0x0013EB60, DAT_0032B354
    }
    for (RuntimeSegmentContactActor* player : contacts.marked())
    {
        if (player->actor.mask_mode != 2 && player->actor.mask_flag == 0 && has_model(player->actor))
        {
            insert_runtime_body_contact_override(warmths, *player, 0, contacts.players); // 0x0013EBE0
        }
    }
    if (warmths != nullptr)
    {
        suspend_runtime_body_contact_overrides(*warmths); // 0x0013EE10
    }
    // 0x00122B00, the radius times DAT_0032B528.
    Vec3 origin = point;
    const i32 count = entity_collect_contact_probes(origin, furnace.radius_xz * 55.f, 0x20002);
    if (count != 0)
    {
        RuntimePropContactNotice notice{0x1B, 0, &furnace, 1, nullptr, {}};
        const std::array<Contact, 16> kept = keep_contacts(count);
        for (i32 index = 0; index < count; ++index)
        {
            notice.point = point;
            notice.contacts = kept[index].records;
            auto& object = contacts.contact_actor(*kept[index].object);
            object.message(&notice); // Vtable +0xC.
            engine_heap_free(kept[index].records); // HeapFree on the block's heap (0x000F7100).
            insert_runtime_body_contact_override(warmths, object, 1, contacts.players); // 0x0013EBE0
        }
    }
    if (warmths != nullptr)
    {
        restore_runtime_body_collision_overrides(*warmths); // The inlined 0x0013EE80.
    }
}

// Xbox 0x0006E260
void update_runtime_furnace(GhoulySpawnPool*& burns, GhoulySpawnPool*& warmths, ActorFrameState& state,
    ActorFrameContext& frame, RuntimeFurnaceContacts& contacts)
{
    update_actor_frame(state, frame); // 0x000420D0
    const Vec3 point = actor_world_position(state.actor); // 0x000412A0
    burn_runtime_furnace_contacts(state.actor, point, burns, contacts); // 0x0006E4D0
    warm_runtime_furnace_contacts(state.actor, point, warmths, contacts); // 0x0006E6F0
}
}
