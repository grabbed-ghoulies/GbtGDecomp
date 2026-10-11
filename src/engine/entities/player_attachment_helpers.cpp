#include "player_attachment_helpers.h"
#include "entity_heap.h"
#include <cstring>
#include <new>

namespace gbtg
{
// Xbox 0x00146190
i32 runtime_attachment_class_value(std::span<const u8> attached, u16 host_class)
{
    const i32 count = i16(read_u16(attached, 0x944));
    i32 index = 0;
    if (count > 0)
    {
        do
        {
            const size_t offset = 0x948 + size_t(index) * 0x10;
            if (read_u16(attached, offset) == host_class)
            {
                return i16(read_u16(attached, offset + 2));
            }
            ++index;
        }
        while (index < count);
    }
    return -1;
}

// Xbox 0x001461C0
void set_runtime_attachment_host(RuntimeBodyAttachedActor& attached,
    const RuntimeBodyAttachedActor& host)
{
    i32 index = 0;
    const u16 host_class = read_u16(host.bytes, 6);
    if (i16(read_u16(attached.bytes, 0x944)) > 0)
    {
        while (read_u16(attached.bytes, 0x948 + size_t(index) * 0x10) != host_class)
        {
            const i32 count = i16(read_u16(attached.bytes, 0x944));
            ++index;
            if (index >= count)
            {
                index = -1;
                break;
            }
        }
    }
    else
    {
        index = -1;
    }
    const u16 selected = u16(index);
    std::memcpy(attached.bytes.data() + 0x946, &selected, 2);
    std::memcpy(attached.bytes.data() + 0xD8C, &host.address, 4);
    attached.host = const_cast<RuntimeBodyAttachedActor*>(&host);
    const u32 message[]{0x91, host.address};
    attached.message(message);
}

// Xbox 0x00146100
bool runtime_attachment_available(std::span<const u8> attached, const RuntimeBodyAttachedActor* host)
{
    if (attached[0x93F] == 1)
    {
        return false;
    }
    if (attached[0x93F] == 2)
    {
        if (host == nullptr)
        {
            return false;
        }
        if (read_u16(host->bytes, 0xCDC) != 2 && read_u32(host->bytes, 0x93C) == 0)
        {
            return false;
        }
    }
    return attached[0x941] == 0;
}

// Xbox 0x0003B9D0
ActorBind* ensure_runtime_actor_bind(Actor& actor)
{
    if (actor.bind == nullptr)
    {
        void* block = engine_heap_allocate(i32(sizeof(ActorBind)));
        actor.bind = static_cast<ActorBind*>(block);
        if (block != nullptr)
        {
            actor.bind = new (block) ActorBind{};
        }
    }
    return actor.bind;
}

// Xbox 0x0003B840
void bind_runtime_actor_joint(Actor& actor, Actor* parent, i32 joint_key, u8 flags)
{
    if (parent != nullptr && parent->model != nullptr && parent->model->cpu.data() != nullptr
        && joint_key != -1 && parent->model != nullptr && parent->model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(parent->model->cpu, i16(joint_key)) & 0xFFu;
        if (joint != 0xFFu && joint != 0xFFFFFFFFu && ensure_runtime_actor_bind(actor) != nullptr)
        {
            actor.bind->mode_parent = 2;
            actor.bind->parent = parent;
            actor.bind->flags = flags;
            actor.bind->joint = i32(joint);
        }
    }
}

// Xbox 0x0001B7C0
void notify_runtime_secondary_attachment_host(RuntimeBodyAttachedActor& host, RuntimeBodyAttachedActor& attached)
{
    const u32 message[]{0x5F, attached.address};
    host.message(message);
    if (read_u16(host.bytes, 0xCDC) != 2 && read_u32(host.bytes, 0x93C) == 0)
    {
        const u32 active = 1;
        std::memcpy(attached.bytes.data() + 0x9A4, &active, 4);
    }
}

// Xbox 0x0001B770
void notify_runtime_attachment_host(RuntimeBodyAttachedActor& host, RuntimeBodyAttachedActor& attached)
{
    const u32 message[]{0x5E, attached.address};
    host.message(message);
    if (read_u16(host.bytes, 0xCDC) != 2 && read_u32(host.bytes, 0x93C) == 0)
    {
        const u32 active = 1;
        std::memcpy(attached.bytes.data() + 0x9A4, &active, 4);
    }
}
}
