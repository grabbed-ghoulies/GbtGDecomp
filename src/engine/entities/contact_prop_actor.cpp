#include "contact_prop_actor.h"
#include "actor_shape_bounds.h"
#include "actor_runtime_queries.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "shape_transform.h"
#include "actor_transform.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0013EEB0
i32 collect_runtime_actor_shape_contacts(Actor& actor, GhoulySpawnPool*& overrides,
    RuntimeActorShapeContacts& context)
{
    i32 total = 0;
    if (overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*overrides);
    }
    const u8* shape = find_runtime_actor_shape(actor, 1, 0, -1);
    while (shape != nullptr)
    {
        const EntityBonePose* pose = nullptr;
        if (actor.model != nullptr)
        {
            pose = actor.model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor.model->primary_shape_pose;
            }
        }
        Vec3 center;
        const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape),
            actor.xform, pose, center);
        const i32 count = entity_collect_contact_probes(center, radius, 0x20002);
        if (count != 0)
        {
            total = std::bit_cast<i32>(u32(total) + u32(count));
            RuntimePropContactNotice notice{0x1B, 0, &actor, 0, nullptr, {}};
            struct Contact
            {
                entity_detail::SegmentProbe* object;
                GhoulySpawnPool* records;
            };
            std::array<Contact, 16> kept;
            i32 index = 0;
            if (count > 0)
            {
                do
                {
                    auto& slot = entity_detail::g_segment_slots[index];
                    kept[index] = {slot.object, copy_segment_records(slot.pool)};
                    ++index;
                }
                while (index < count);
            }
            index = 0;
            if (count > 0)
            {
                do
                {
                    notice.point = center;
                    notice.contacts = kept[index].records;
                    auto& object = context.contact_actor(*kept[index].object);
                    object.message(&notice);
                    engine_heap_free(kept[index].records);
                    insert_runtime_body_contact_override(overrides, object, 1, context.players);
                    ++index;
                }
                while (index < count);
            }
        }
        shape = next_runtime_actor_shape(actor, shape, 1, 0, -1);
    }
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides);
    }
    return total;
}

// Xbox 0x0006EE00
void runtime_contact_prop_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0x940;
    sizes.parameters = 0x334;
}

// Xbox 0x0006EE70
bool initialize_runtime_contact_prop(std::span<u8> fields, Actor& actor)
{
    const u32 enabled = runtime_actor_physical_shape_flags(actor) & 1;
    std::memcpy(fields.data() + 0x93C, &enabled, 4);
    std::memcpy(fields.data() + 0x938, &enabled, 4);
    return true;
}

// Xbox 0x0006EEA0
void release_runtime_contact_prop(std::span<u8> fields, GhoulySpawnPool*& overrides)
{
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides);
        engine_heap_free(overrides);
        overrides = nullptr;
        const u32 zero = 0;
        std::memcpy(fields.data() + 0x934, &zero, 4);
    }
}

// Xbox 0x0006EF30
void handle_runtime_contact_prop_message(std::span<u8> fields, GhoulySpawnPool* overrides,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings)
{
    dispatch_base_actor_word_message(base, message, bindings);
    const u32 id = message[0];
    if (id == 0x1E)
    {
        const u32 zero = 0;
        std::memcpy(fields.data() + 0x938, &zero, 4);
    }
    else
    {
        if (id == 0x1F)
        {
            const u32 one = 1;
            std::memcpy(fields.data() + 0x938, &one, 4);
            return;
        }
        if (id == 0x7C && read_u32(fields, 0x938) != 0)
        {
            forget_runtime_body_contact_actor(overrides, actor_at(message[1]));
            return;
        }
    }
}

// Xbox 0x0006EED0
void update_runtime_contact_prop(std::span<u8> fields, GhoulySpawnPool*& overrides,
    ActorFrameState& state, ActorFrameContext& frame, const RuntimeActorClock& clock,
    RuntimeActorShapeContacts& contacts)
{
    update_actor_frame(state, frame);
    if (read_u32(fields, 0x938) != 0)
    {
        if (overrides != nullptr)
        {
            age_runtime_body_contact_overrides(*overrides, clock);
            expire_runtime_body_contact_overrides(overrides, std::bit_cast<float>(0x3EB33333u)); // DAT_0032B354.
        }
        collect_runtime_actor_shape_contacts(state.actor, overrides, contacts);
    }
}

// Xbox 0x0006EB20
void runtime_knockdown_sphere_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0x940;
    sizes.parameters = 0x338;
}

// Xbox 0x0006EB80
bool initialize_runtime_knockdown_sphere(std::span<u8> fields, std::span<const u8> parameters)
{
    std::memcpy(fields.data() + 0x938, parameters.data() + 0x334, 4);
    return true;
}

// Xbox 0x0006EBD0
void handle_runtime_knockdown_sphere_message(std::span<u8> fields, GhoulySpawnPool* overrides,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings)
{
    dispatch_base_actor_word_message(base, message, bindings); // 0x0003EF70
    const u32 id = message[0];
    if (id == 1)
    {
        const u32 one = 1;
        std::memcpy(fields.data() + 0x93C, &one, 4);
        return;
    }
    if (id == 0x7C && overrides != nullptr)
    {
        // EDX the message's actor: 0x0013EDD0 compares it with each row's actor and dereferences nothing else, so
        // an address with no scene actor matches no row.
        if (Actor* object = actor_at(message[1]); object != nullptr)
        {
            remove_runtime_body_contact_override(overrides, *object); // 0x0013EDD0, ESI overrides
        }
    }
}

// Xbox 0x0006EC10, EAX actor
void push_runtime_knockdown_sphere(Actor& actor, std::span<const u8> fields, GhoulySpawnPool*& overrides,
    const RuntimeActorClock& clock, RuntimeKnockdownSphereContacts& contacts)
{
    Vec3 origin = actor_world_position(actor); // 0x000412A0
    if (overrides != nullptr)
    {
        age_runtime_body_contact_overrides(*overrides, clock); // 0x0013ED20
    }
    for (RuntimeSegmentContactActor* player : contacts.marked())
    {
        // Node - 0x934: a player (+0xCDC 2, or +0x93C set) stays out of the sphere's own contacts.
        if (player->actor.mask_mode == 2 || player->actor.mask_flag != 0)
        {
            insert_runtime_body_contact_override(overrides, *player, 0, contacts.players); // 0x0013EBE0
        }
    }
    if (overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*overrides); // 0x0013EE10, EDI overrides
    }
    const float radius = read_f32(fields, 0x938);
    const i32 count = entity_collect_contact_probes(origin, radius, 0x20002); // 0x00122B00
    if (count != 0)
    {
        RuntimeKnockdownSphereNotice notice{0x17, 0, &actor, nullptr, {}};
        struct Contact
        {
            entity_detail::SegmentProbe* object;
            GhoulySpawnPool* records;
        };
        std::array<Contact, 16> kept;
        for (i32 index = 0; index < count; ++index)
        {
            auto& slot = entity_detail::g_segment_slots[index]; // DAT_00511A1C.
            kept[index] = {slot.object, copy_segment_records(slot.pool)}; // 0x000FCC90
        }
        for (i32 index = 0; index < count; ++index)
        {
            notice.point = origin;
            notice.contacts = kept[index].records;
            auto& object = contacts.contact_actor(*kept[index].object);
            object.message(&notice); // Vtable +0xC.
            engine_heap_free(kept[index].records); // HeapFree on the block's heap (0x000F7100).
            if (notice.reply != 0)
            {
                insert_runtime_body_contact_override(overrides, object, 1, contacts.players); // 0x0013EBE0
            }
        }
    }
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides); // The inlined 0x0013EE80.
    }
}

// Xbox 0x0006EBA0
void update_runtime_knockdown_sphere(std::span<u8> fields, GhoulySpawnPool*& overrides,
    ActorFrameState& state, ActorFrameContext& frame, const RuntimeActorClock& clock,
    RuntimeKnockdownSphereContacts& contacts)
{
    update_actor_frame(state, frame); // 0x000420D0
    if (read_u32(fields, 0x93C) != 0)
    {
        push_runtime_knockdown_sphere(state.actor, fields, overrides, clock, contacts); // 0x0006EC10, the tail jump
    }
}
}
