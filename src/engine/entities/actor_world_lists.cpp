#include "actor_world_lists.h"
#include "entity.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0003E810
void register_actor_segment_probe(const Actor& actor, entity_detail::SegmentProbe& probe)
{
    if (actor.model_flags != 0)
    {
        auto& world = entity_detail::g_world;
        // Native fields keep the sentinel and callback separate from the retail link.
        auto* previous = reinterpret_cast<entity_detail::SegmentProbe*>(&world.segment_prev);
        entity_detail::SegmentProbe** tail = &world.segment;
        if (world.segment != nullptr)
        {
            do
            {
                previous = *tail;
                tail = &previous->next;
            }
            while (previous->next != nullptr);
        }
        *tail = &probe;
        probe.prev = previous;
        probe.next = nullptr;
    }
}

// Xbox 0x0003C770
void register_actor_shape_probe(const Actor& actor, entity_detail::ShapeProbe& probe)
{
    auto& world = entity_detail::g_world;
    auto* sentinel = reinterpret_cast<entity_detail::ShapeProbe*>(&world.shape_prev);
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        u32 index = 0;
        const u32 count = read_u32(cpu, 4);
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 4)
                {
                    const u32 offset = read_u32(cpu, table + index * 8 + 4);
                    if (offset != 0)
                    {
                        const u8* node = cpu.data() + offset + 8;
                        i32 remaining = std::bit_cast<i32>(read_u32(cpu, offset));
                        u32 mask = 0;
                        if (remaining > 0)
                        {
                            do
                            {
                                u32 node_mask;
                                std::memcpy(&node_mask, node + 4, 4);
                                mask |= node_mask;
                                if (node[1] == 0)
                                {
                                    node = nullptr;
                                }
                                else
                                {
                                    node += node[1];
                                }
                                --remaining;
                            }
                            while (remaining != 0);
                        }
                        if ((u32(actor.model_mode) & mask) != 0)
                        {
                            if (probe.prev != nullptr)
                            {
                                return;
                            }
                            entity_detail::ShapeProbe* root = sentinel;
                            auto** previous = &world.shape_prev;
                            while (*previous != nullptr)
                            {
                                root = *previous;
                                previous = &root->prev;
                            }
                            auto** next = root == sentinel ? &world.shape : &root->next;
                            auto* successor = *next;
                            *next = &probe;
                            probe.prev = root;
                            probe.next = successor;
                            if (successor != nullptr)
                            {
                                successor->prev = &probe;
                            }
                            return;
                        }
                    }
                    break;
                }
                ++index;
            }
            while (index < count);
        }
    }
    if (probe.prev != nullptr)
    {
        auto** next = probe.prev == sentinel ? &world.shape : &probe.prev->next;
        *next = probe.next;
    }
    if (probe.next != nullptr)
    {
        probe.next->prev = probe.prev;
    }
    probe.prev = nullptr;
    probe.next = nullptr;
}

// Xbox 0x0003C700
void clear_actor_shape_probe(Actor& actor, entity_detail::ShapeProbe& probe)
{
    actor.model_mode = 0;
    register_actor_shape_probe(actor, probe);
    auto& world = entity_detail::g_world;
    auto* sentinel = reinterpret_cast<entity_detail::ShapeProbe*>(&world.shape_prev);
    if (probe.prev != nullptr)
    {
        auto** next = probe.prev == sentinel ? &world.shape : &probe.prev->next;
        *next = probe.next;
    }
    if (probe.next != nullptr)
    {
        probe.next->prev = probe.prev;
    }
    probe.prev = nullptr;
    probe.next = nullptr;
}

}
