#include "actor_model_anchors.h"
#include "entity_heap.h"
#include "actor_live_attachments.h"
#include "gbtg/model_runtime.h"

#include <cstring>
#include <bit>

namespace gbtg
{
// Xbox 0x0011BDA0
void release_actor_model_anchor(ActorModelAnchor& anchor)
{
    ActorModelAnchorReleaseMessage message{0x85, &anchor};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message));
    if (anchor.owner_link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(anchor.owner_link.prev)->next = anchor.owner_link.next;
    }
    if (anchor.owner_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(anchor.owner_link.next)->prev = anchor.owner_link.prev;
    }
    anchor.owner_link.prev = nullptr;
    anchor.owner_link.next = nullptr;
    if (anchor.global_link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(anchor.global_link.prev)->next = anchor.global_link.next;
    }
    if (anchor.global_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(anchor.global_link.next)->prev = anchor.global_link.prev;
    }
    anchor.global_link.prev = nullptr;
    anchor.global_link.next = nullptr;
    engine_heap_free(&anchor);
}

entity_detail::ListHead g_actor_model_anchors;

// Xbox 0x000408D0
const u8* find_actor_model_binding(const Actor& actor, i16 kind)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        u32 index = 0;
        const u32 count = read_u32(cpu, 4);
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            while (read_u32(cpu, table + index * 8) != 2)
            {
                ++index;
                if (count <= index)
                {
                    return nullptr;
                }
            }
            const u32 offset = read_u32(cpu, table + index * 8 + 4);
            if (offset != 0)
            {
                const u8* block = cpu.data() + offset;
                u32 records;
                std::memcpy(&records, block, 4);
                const u8* entry = block + 4;
                const u8* end = entry + records * 0x34;
                for (; entry < end; entry += 0x34)
                {
                    i16 value;
                    std::memcpy(&value, entry, 2);
                    if (value == kind)
                    {
                        return entry;
                    }
                }
            }
        }
    }
    return nullptr;
}

// Xbox 0x0011C0B0
ActorModelAnchor* allocate_actor_model_anchor(Actor& actor, entity_detail::ListHead& owner_head,
    i32 mode)
{
    auto* anchor = static_cast<ActorModelAnchor*>(engine_heap_allocate(sizeof(ActorModelAnchor)));
    std::memset(anchor, 0, sizeof(*anchor));
    anchor->mode = mode;
    anchor->owner = &actor;
    auto* root = &owner_head;
    while (root->prev != nullptr)
    {
        root = static_cast<entity_detail::ListHead*>(root->prev);
    }
    auto* successor = static_cast<entity_detail::ListHead*>(root->next);
    root->next = &anchor->owner_link;
    anchor->owner_link.prev = root;
    anchor->owner_link.next = successor;
    if (successor != nullptr)
    {
        successor->prev = &anchor->owner_link;
    }
    root = &g_actor_model_anchors;
    while (root->prev != nullptr)
    {
        root = static_cast<entity_detail::ListHead*>(root->prev);
    }
    successor = static_cast<entity_detail::ListHead*>(root->next);
    root->next = &anchor->global_link;
    anchor->global_link.prev = root;
    anchor->global_link.next = successor;
    if (successor != nullptr)
    {
        successor->prev = &anchor->global_link;
    }
    return anchor;
}

// Xbox 0x0011BE40
void update_actor_model_anchor(ActorModelAnchor& anchor)
{
    if (anchor.mode == 0)
    {
        const ModelPoseBuffer* pose;
        if (anchor.owner->model == nullptr)
        {
            pose = nullptr;
        }
        else
        {
            const RuntimeModelInstance& model = *anchor.owner->model->runtime;
            pose = model.alternate;
            if (pose == nullptr)
            {
                pose = model.primary;
            }
        }
        // Host view of the same 0x34-byte locator record passed to 0x00051CE0.
        ModelBoneBinding binding;
        std::memcpy(&binding.kind, anchor.binding, 2);
        std::memcpy(&binding.joint, anchor.binding + 2, 2);
        std::memcpy(binding.matrix.data(), anchor.binding + 4, 48);
        AnimationMatrix world;
        std::memcpy(world.data(), anchor.owner->xform, 48);
        model_bone_world_point(binding, pose, &world, anchor.output);
    }
    else
    {
        if (anchor.mode == 1)
        {
            actor_transform_joint_point(*anchor.owner, anchor.joint, actor_transform_origin, anchor.output);
            return;
        }
        if (anchor.mode == 2)
        {
            actor_transform_point(*anchor.owner, std::bit_cast<i16>(anchor.joint), anchor.local, anchor.output);
            return;
        }
    }
}

// Xbox 0x0003EEB0
void register_actor_model_anchors(Actor& actor, entity_detail::ListHead& owner_head)
{
    const u8* record = nullptr;
    while (true)
    {
        if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
        {
            return;
        }
        const auto cpu = actor.model->cpu;
        u32 index = 0;
        const u32 count = read_u32(cpu, 4);
        if (count == 0)
        {
            return;
        }
        const u32 table = read_u32(cpu, 0);
        while (read_u32(cpu, table + index * 8) != 2)
        {
            ++index;
            if (count <= index)
            {
                return;
            }
        }
        const u32 offset = read_u32(cpu, table + index * 8 + 4);
        if (offset == 0)
        {
            break;
        }
        const u8* block = cpu.data() + offset;
        u32 records;
        std::memcpy(&records, block, 4);
        const u8* limit;
        if (record == nullptr)
        {
            record = block + 4;
            limit = record + records * 0x34;
        }
        else
        {
            record += 0x34;
            limit = block + records * 0x34 + 4;
        }
        if (limit <= record)
        {
            return;
        }
        while (std::bit_cast<i16>(read_u16({record, 0x34}, 0)) != 3)
        {
            record += 0x34;
            if (limit <= record)
            {
                return;
            }
        }
        auto* anchor = allocate_actor_model_anchor(actor, owner_head, 0);
        anchor->binding = record;
        update_actor_model_anchor(*anchor);
    }
}
}
