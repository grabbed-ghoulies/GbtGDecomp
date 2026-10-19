#include "player_mind_events.h"
#include "entity_heap.h"
#include <bit>
#include <cmath>
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

// Xbox 0x0002A3E0
void record_runtime_mind_contact(RuntimeMindEvents& context, u32 first, u32 second, u32 payload)
{
    for (LiveScript* node = context.anchor.next; node != nullptr; node = node->next)
    {
        if (u16(node->type) == 1)
        {
            auto& event = *static_cast<RuntimeMindEvent*>(node);
            if (read_u32(event.bytes, 0x20) == first && read_u32(event.bytes, 0x24) == second)
            {
                put(event.bytes, 0x2C, payload);
                put(event.bytes, 0x24, second);
                put(event.bytes, 0x28, read_u32(event.bytes, 0x28) + 1);
                refresh_runtime_mind_event(context, event);
                return;
            }
        }
    }
    auto* event = allocate_runtime_mind_event(context, 1, 2.f);
    put(event->bytes, 0x28, 1);
    put(event->bytes, 0x20, first);
    put(event->bytes, 0x24, second);
    put(event->bytes, 0x2C, payload);
}

// Xbox 0x0002A460
void clear_runtime_mind_actor_events(RuntimeMindEvents& context, u32 actor)
{
    if (read_u32(context.mind, 0xC) == actor)
    {
        put(context.mind, 0xC, 0);
    }
    if (read_u32(context.mind, 0x18) == actor)
    {
        put(context.mind, 0x18, 0);
    }
    LiveScript* node = context.anchor.next;
    while (node != nullptr && u16(node->type) != 1)
    {
        node = node->next;
    }
    while (node != nullptr)
    {
        LiveScript* next = node->next;
        while (next != nullptr && u16(next->type) != 1)
        {
            next = next->next;
        }
        auto& event = *static_cast<RuntimeMindEvent*>(node);
        if (read_u32(event.bytes, 0x20) == actor || read_u32(event.bytes, 0x24) == actor)
        {
            script_release_event(node);
        }
        node = next;
    }
    node = context.anchor.next;
    while (node != nullptr && u16(node->type) != 4)
    {
        node = node->next;
    }
    while (node != nullptr)
    {
        LiveScript* next = node->next;
        while (next != nullptr && u16(next->type) != 4)
        {
            next = next->next;
        }
        auto& event = *static_cast<RuntimeMindEvent*>(node);
        if (read_u32(event.bytes, 0x20) == actor)
        {
            if (node->prev != nullptr)
            {
                node->prev->next = node->next;
            }
            if (node->next != nullptr)
            {
                node->next->prev = node->prev;
            }
            node->prev = nullptr;
            node->next = nullptr;
            engine_heap_free(node);
        }
        node = next;
    }
}

// Xbox 0x0002A5A0
void end_runtime_mind_target(RuntimeMindEvents& context, std::span<const u32> message,
    std::span<const u8> target)
{
    if (read_u32(context.mind, 0x10) == message[1])
    {
        if (read_u32(context.mind, 0x10) != 0)
        {
            const u32 notice[]{0x64, message[2]};
            context.actor_message(notice);
        }
        put(context.mind, 0x10, 0);
        if (message[2] != 0 && read_u32(context.mind, 0xC) != 0 &&
            read_u32(context.mind, 0xC) == read_u32(target, 0x10))
        {
            put(context.mind, 0xC, 0);
        }
    }
    if (read_u32(context.mind, 0x14) == message[1])
    {
        put(context.mind, 0x14, 0);
    }
    LiveScript* head = context.anchor.next;
    LiveScript* event = head;
    while (event != nullptr && u16(event->type) != 3)
    {
        event = event->next;
    }
    const u32 actor = event == nullptr ? 0 : read_u32(static_cast<RuntimeMindEvent*>(event)->bytes, 0x20);
    if (actor == message[1] && head != nullptr)
    {
        event = head;
        while (u16(event->type) != 3)
        {
            event = event->next;
            if (event == nullptr)
            {
                return;
            }
        }
        script_release_event(event);
    }
}

// Xbox 0x0002A2E0
void dispatch_runtime_parent_mind_message(RuntimeMindEvents& context,
    std::span<const u32> message, std::span<const u8> target)
{
    switch (message[0])
    {
    case 0x6E:
        if (message[2] != 0)
        {
            record_runtime_mind_contact(context, message[1], message[2], message[4]);
        }
        return;
    case 0x7C:
        clear_runtime_mind_actor_events(context, message[1]);
        return;
    case 0x80:
        if (read_u32(context.mind, 0x10) != 0)
        {
            const u32 notice[]{0x64, 0};
            context.actor_message(notice);
        }
        put(context.mind, 0x10, 0);
        return;
    case 0x85:
        if (read_u32(context.mind, 0x30) == message[1])
        {
            put(context.mind, 0x30, 0);
            put(context.mind, 0x34, 0);
            put(context.mind, 0x38, 0);
        }
        return;
    case 0x8D:
        end_runtime_mind_target(context, message, target);
        return;
    default:
        return;
    }
}

// Xbox 0x0002AA60
void update_runtime_mind_requests(RuntimeMindTick& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    RuntimePlayerMindRequest* request = context.requests.first;
    while (request != nullptr)
    {
        RuntimePlayerMindRequest* current = request;
        if (request->bytes[0xA] == 0)
        {
            const i32 initialized = context.initialize_request(*request);
            request->bytes[0xA] = u8(request->bytes[0xA] + 1);
            request->active = request->bytes[0xA];
            if (initialized == 0)
            {
                request = request->next;
                release_runtime_player_mind_request(*current, context.requests);
                continue;
            }
        }
        const i32 result = context.update_request(*request);
        if (result == 3)
        {
            put(context.events.mind, 0x5C, 3);
            clear_runtime_player_mind_requests(context.requests);
            return;
        }
        put(request->bytes, 0xC, std::bit_cast<u32>(read_f32(request->bytes, 0xC) + elapsed));
        if (result == 0)
        {
            return;
        }
        put(context.events.mind, 0x5C, u32(result));
        request = request->next;
        release_runtime_player_mind_request(*current, context.requests);
        if (result == 2)
        {
            return;
        }
    }
}

// Xbox 0x0002AB50
void update_runtime_mind_events(RuntimeMindTick& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    LiveScript* node = context.events.anchor.next;
    while (node != nullptr)
    {
        LiveScript* next = node->next;
        auto& event = *static_cast<RuntimeMindEvent*>(node);
        float timer = read_f32(event.bytes, 0x14);
        if (timer > 0.f)
        {
            timer -= elapsed;
            put(event.bytes, 0x14, std::bit_cast<u32>(timer));
            if (0.f > timer)
            {
                put(event.bytes, 0x14, 0);
            }
        }
        timer = read_f32(event.bytes, 0xC);
        if (timer > 0.f)
        {
            timer -= elapsed;
            put(event.bytes, 0xC, std::bit_cast<u32>(timer));
            // COMISS zero,timer; JC retains unordered values too.
            if (!(0.f < timer) && !std::isnan(timer))
            {
                put(event.bytes, 0xC, 0);
                script_release_event(node);
                node = next;
                continue;
            }
        }
        put(event.bytes, 0x18, std::bit_cast<u32>(read_f32(event.bytes, 0x18) + elapsed));
        put(event.bytes, 0x1C, std::bit_cast<u32>(read_f32(event.bytes, 0x1C) + elapsed));
        node = next;
    }
}

// Xbox 0x0002A740
u32 runtime_mind_alert_level(float timer)
{
    if (5.f > timer)
    {
        return 1;
    }
    if (10.f > timer)
    {
        return 2;
    }
    return 3;
}

// Xbox 0x0002A650
void update_runtime_mind_alert(RuntimeMindTick& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.body_actor);
    float timer = read_f32(context.events.mind, 0x50);
    if (timer > 0.f)
    {
        timer -= rate * step;
        put(context.events.mind, 0x50, std::bit_cast<u32>(timer));
        if (!(0.f < timer) && !std::isnan(timer))
        {
            put(context.events.mind, 0x50, 0);
        }
    }
    switch (read_u32(context.events.mind, 0x4C))
    {
    case 0x40:
        put(context.body, 0x158, 0);
        return;
    case 0x41:
        put(context.body, 0x158, runtime_mind_alert_level(read_f32(context.events.mind, 0x50)));
        return;
    case 0x42:
        put(context.body, 0x158, 4);
        return;
    case 0x43:
        put(context.body, 0x158, 9);
        return;
    case 0x44:
        put(context.body, 0x158, 7);
        return;
    case 0x45:
        put(context.body, 0x158, 8);
        return;
    case 0x46:
        put(context.body, 0x158, 6);
        return;
    default:
        return;
    }
}

// Xbox 0x0002A9F0
void tick_runtime_parent_mind(RuntimeMindTick& context)
{
    update_runtime_mind_requests(context);
    update_runtime_mind_events(context);
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    put(context.events.mind, 0x34,
        std::bit_cast<u32>(rate * step + read_f32(context.events.mind, 0x34)));
    update_runtime_mind_alert(context);
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

// Xbox 0x00136AC0
RuntimePlayerMindRequest* append_runtime_range_request(RuntimePlayerMindRequest& anchor, std::span<const u8> sizes,
    float range)
{
    auto* request = allocate_runtime_mind_request(anchor, 0x2F, sizes);
    put(request->bytes, 0x10, 0x3F800000);
    put(request->bytes, 0x14, 0x3F800000);
    put(request->bytes, 0x1C, 0x41F00000);
    put(request->bytes, 0x18, std::bit_cast<u32>(range));
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
