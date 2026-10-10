#include "player_mind_events.h"
#include "entity_heap.h"
#include <bit>
#include <cstring>
#include <new>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void prepend(LiveScript& anchor, LiveScript& event)
{
    LiveScript* root = &anchor;
    while (root->prev != nullptr)
    {
        root = root->prev;
    }
    LiveScript* next = root->next;
    root->next = &event;
    event.prev = root;
    event.next = next;
    if (next != nullptr)
    {
        next->prev = &event;
    }
}
}

// Xbox 0x0002A190
RuntimeMindEvent* allocate_runtime_mind_event(RuntimeMindEvents& context, u16 kind, float duration)
{
    if ((context.descriptors[size_t(kind) * 12 + 4] & 1) != 0)
    {
        for (LiveScript* event = context.anchor.next; event != nullptr; event = event->next)
        {
            if (u16(event->type) == kind)
            {
                return refresh_runtime_mind_event(context, *static_cast<RuntimeMindEvent*>(event));
            }
        }
    }
    const u32 size = read_u32(context.descriptors, size_t(kind) * 12);
    auto* block = static_cast<u8*>(engine_heap_allocate(i32(sizeof(RuntimeMindEvent) + size)));
    if (block == nullptr)
    {
        return nullptr;
    }
    auto* event = new (block) RuntimeMindEvent{};
    event->bytes = {block + sizeof(RuntimeMindEvent), size};
    std::memset(event->bytes.data(), 0, size);
    event->type = kind;
    std::memcpy(event->bytes.data() + 8, &kind, 2);
    put(event->bytes, 0xC, std::bit_cast<u32>(duration));
    put(event->bytes, 0x10, std::bit_cast<u32>(duration));
    put(event->bytes, 0x14, read_u32(context.mind, 0x2C));
    prepend(context.anchor, *event);
    return event;
}

// Xbox 0x0002A280
RuntimeMindEvent* refresh_runtime_mind_event(RuntimeMindEvents& context, RuntimeMindEvent& event)
{
    put(event.bytes, 0xC, read_u32(event.bytes, 0x10));
    put(event.bytes, 0x14, read_u32(context.mind, 0x2C));
    put(event.bytes, 0x1C, 0);
    event.bytes[0xA] = 0;
    if (event.prev != nullptr)
    {
        event.prev->next = event.next;
    }
    if (event.next != nullptr)
    {
        event.next->prev = event.prev;
    }
    event.prev = nullptr;
    event.next = nullptr;
    prepend(context.anchor, event);
    return &event;
}

// Xbox 0x0002A0C0
void refresh_runtime_mind_target_event(RuntimeMindEvents& context)
{
    for (LiveScript* event = context.anchor.next; event != nullptr; event = event->next)
    {
        if (u16(event->type) == 4)
        {
            auto& target_event = *static_cast<RuntimeMindEvent*>(event);
            if (read_u32(target_event.bytes, 0x20) == read_u32(context.mind, 0x10))
            {
                refresh_runtime_mind_event(context, target_event);
                return;
            }
        }
    }
    auto* event = allocate_runtime_mind_event(context, 4, 20.f);
    put(event->bytes, 0x20, read_u32(context.mind, 0x10));
}

// Xbox 0x0002A030
void set_runtime_mind_target(RuntimeMindEvents& context, u32 target)
{
    if (target == 0 && read_u32(context.mind, 0x10) != 0)
    {
        const u32 message[]{0x64, 0};
        context.actor_message(message);
    }
    put(context.mind, 0x10, target);
    if (target != 0)
    {
        refresh_runtime_mind_target_event(context);
    }
}

// Xbox 0x00136A70
RuntimePlayerMindRequest* append_runtime_position_request(RuntimePlayerMindRequest& anchor,
    std::span<const u8> sizes, std::span<const u8, 12> position)
{
    auto* request = allocate_runtime_mind_request(anchor, 0x2E, sizes);
    put(request->bytes, 0x1C, 0x3F800000);
    put(request->bytes, 0x20, 0x3F800000);
    put(request->bytes, 0x28, 0x41F00000);
    put(request->bytes, 0x10, read_u32(position, 0));
    put(request->bytes, 0x14, read_u32(position, 4));
    put(request->bytes, 0x18, read_u32(position, 8));
    return request;
}

// Xbox 0x000283A0
u32 runtime_player_goal_flags(const RuntimePlayerGoalRing* ring, std::span<const u8> address_zero)
{
    if (ring == nullptr)
    {
        return 0;
    }
    if (ring->tail == ring->capacity)
    {
        return read_u32(address_zero, 0x1C);
    }
    return read_u32(ring->rows, size_t(ring->head) * ring->stride + 0x1C);
}

// Xbox 0x00105F90
bool runtime_direction_angle(const Vec3& direction, float& angle)
{
    const float epsilon = std::bit_cast<float>(0x3456BF95u);
    if (!(animation_absolute(direction.x) > epsilon))
    {
        if (!(animation_absolute(direction.z) > epsilon))
        {
            angle = 0.f;
            return false;
        }
    }
    angle = float(retail_atan2(direction.x, direction.z));
    return true;
}
}
