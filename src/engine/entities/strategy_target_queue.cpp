#include "strategy_target_queue.h"
#include "../runtime/link_exchange.h"
#include "body_message_helpers.h"

namespace gbtg
{
namespace
{
RuntimeStrategyLink& slot_anchor(ActorAttachmentSlot& slot)
{
    // The slot's records pair and owner are the anchor's +0, +4 and +8 words.
    return *reinterpret_cast<RuntimeStrategyLink*>(&slot);
}

void unlink(RuntimeStrategyLink& link)
{
    if (link.previous != nullptr)
    {
        link.previous->next = link.next;
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
}

void append(RuntimeStrategyLink& link, RuntimeStrategyLink& anchor)
{
    RuntimeStrategyLink* last = &anchor;
    while (last->next != nullptr)
    {
        last = last->next;
    }
    last->next = &link;
    link.previous = last;
    link.next = nullptr;
}

float squared_distance(const RuntimeStrategyLink& link, const Vec3& position)
{
    const Vec3 actor = actor_world_position(*static_cast<const Actor*>(link.owner));
    const float z = position.z - actor.z;
    const float y = position.y - actor.y;
    const float x = position.x - actor.x;
    return z * z + y * y + x * x;
}

bool target_bit_set(const RuntimeMindTarget& target)
{
    // Body +0x15C bit block, byte 5 bit 4.
    return ((target.body.bits->bytes[5] >> 4) & 1) != 0;
}
}

// Xbox 0x0002CC30
void exchange_runtime_strategy_links(RuntimeStrategyLink& first, RuntimeStrategyLink& second)
{
    exchange_runtime_links(first, second);
}

float g_50e280 = 0.f;

// Xbox 0x001128A0
void resort_runtime_strategy_target_queues(float step, std::span<ActorAttachmentSlot, 20> slots)
{
    g_50e280 = g_50e280 - step;
    if (0.f < g_50e280)
    {
        return;
    }
    g_50e280 = g_50e280 + 1.f; // DAT_0032B0F8
    for (ActorAttachmentSlot& slot : slots)
    {
        if (slot.owner != nullptr)
        {
            sort_runtime_strategy_target_queue(slot_anchor(slot)); // 0x00112B40, EAX the slot
        }
    }
}

// Xbox 0x00112B40
void sort_runtime_strategy_target_queue(RuntimeStrategyLink& anchor)
{
    RuntimeStrategyLink* link = anchor.next;
    if (link == nullptr)
    {
        return;
    }
    // Owner +0x20: the target record's position.
    const auto* record = static_cast<const ActorLiveAttachment*>(anchor.owner);
    Vec3 position;
    std::memcpy(&position, record->shape.data() + 4, sizeof(position));
    float distance = squared_distance(*link, position);
    std::memcpy(&link->retained, &distance, 4);
    link = link->next;
    while (link != nullptr)
    {
        RuntimeStrategyLink* following = link->next;
        distance = squared_distance(*link, position);
        std::memcpy(&link->retained, &distance, 4);
        RuntimeStrategyLink* previous = link->previous;
        while (previous->previous != nullptr && previous->active == 0)
        {
            float own;
            float other;
            std::memcpy(&own, &link->retained, 4);
            std::memcpy(&other, &previous->retained, 4);
            if (own >= other) // COMISS/JNC: unordered distances still exchange.
            {
                break;
            }
            RuntimeStrategyLink* before = previous->previous;
            exchange_runtime_strategy_links(*link, *previous);
            previous = before;
            if (previous->previous == nullptr)
            {
                break;
            }
        }
        link = following;
    }
}

// Xbox 0x00112970
void queue_runtime_strategy_link(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context)
{
    detach_runtime_strategy_link(link);
    link.owner = actor;
    link.retained = 0;
    link.active = 0;
    for (ActorAttachmentSlot& slot : context.slots)
    {
        if (slot.owner == record)
        {
            RuntimeStrategyLink& anchor = slot_anchor(slot);
            append(link, anchor);
            sort_runtime_strategy_target_queue(anchor);
            return;
        }
    }
    for (ActorAttachmentSlot& slot : context.slots)
    {
        if (slot.owner == nullptr)
        {
            slot.owner = record;
            RuntimeStrategyLink& anchor = slot_anchor(slot);
            append(link, anchor);
            sort_runtime_strategy_target_queue(anchor);
            return;
        }
    }
}

// Xbox 0x00112A70
ActorLiveAttachment* runtime_strategy_link_target(RuntimeStrategyLink& link, const RuntimeStrategyTargetQueue& context)
{
    RuntimeStrategyLink* first = link.previous;
    if (first == nullptr)
    {
        return nullptr;
    }
    RuntimeStrategyLink* anchor = first;
    while (anchor->previous != nullptr)
    {
        anchor = anchor->previous;
    }
    auto* record = static_cast<ActorLiveAttachment*>(anchor->owner);
    if (record->mode == 0 && target_bit_set(*context.target_of(record)))
    {
        return record;
    }
    const std::span<const u8> actor = context.actor_bytes(link.owner);
    if (actor[0x96C] != 0 && record->mode == 0 && context.target_of(record)->body.case_actor != nullptr)
    {
        return record;
    }
    i32 depth = 0;
    for (RuntimeStrategyLink* node = first; node != nullptr; node = node->previous)
    {
        ++depth;
    }
    --depth;
    if (!(depth < i32(context.limits[0])))
    {
        return nullptr;
    }
    const i32 limit = i32(context.class_limits[read_u16(actor, 6)]);
    if (limit == 0)
    {
        return nullptr;
    }
    i32 position = 0;
    for (RuntimeStrategyLink* node = anchor->next; node != nullptr && node != &link; node = node->next)
    {
        ++position;
    }
    return position >= limit ? nullptr : record;
}

// Xbox 0x0002B910
bool join_runtime_strategy_target_queue(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context)
{
    void* queued = nullptr;
    if (link.previous != nullptr)
    {
        RuntimeStrategyLink* anchor = link.previous;
        while (anchor->previous != nullptr)
        {
            anchor = anchor->previous;
        }
        queued = anchor->owner;
    }
    if (record != queued)
    {
        detach_runtime_strategy_link(link);
    }
    if (record == nullptr)
    {
        return false;
    }
    if (link.previous == nullptr)
    {
        queue_runtime_strategy_link(link, record, actor, context);
    }
    return runtime_strategy_link_target(link, context) != nullptr;
}

// Xbox 0x0002B9D0
bool hold_runtime_strategy_target_queue(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context)
{
    if (join_runtime_strategy_target_queue(link, record, actor, context))
    {
        return true;
    }
    detach_runtime_strategy_link(link);
    return false;
}

// Xbox 0x0013A330
bool runtime_strategy_target_attackable(float range, RuntimeStrategyLink& link, RuntimeStrategyTargetChecks& checks,
    RuntimeStrategyTargetQueue& context)
{
    RuntimePlayerStrategyGoals& goals = checks.goals;
    const std::span<const u8> body = goals.entry.body;
    const EntityBitBlock& bits = *goals.body_bits;
    if (body[0x6B] != 0 || body[0x6A] != 0 || read_f32(body, 0x70) > 0.f || read_u16(body, 0x30) != 0
        || read_f32(body, 0x34) != 0.f || (bits.bytes[3] & 8) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(bits, 0x2C) || runtime_body_bit_set(bits, 0x2A))
    {
        return false;
    }
    const std::span<const u8> mind = goals.entry.events.mind;
    if (read_u32(mind, 0x10) == 0)
    {
        return false;
    }
    RuntimeMindTarget* target = goals.target_at(read_u32(mind, 0x10));
    if (target->bytes[0x15] != 0 || runtime_strategy_abandons_target(*target, checks) != 0)
    {
        return false;
    }
    if (!runtime_strategy_target_within(range, *target, goals.entry.actor))
    {
        return false;
    }
    return hold_runtime_strategy_target_queue(link, target->record, &goals.entry.actor, context);
}
}
