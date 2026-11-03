#include "player_mind_events.h"
#include "mind_fields.h"
#include "body_fields.h"
#include "player_mind_targets.h"
#include "mind_request_fields.h"
#include "entity_heap.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <new>

namespace gbtg
{
namespace
{
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
    set(event->bytes, mind_event_field::kind, kind);
    set(event->bytes, mind_event_field::time_left, duration);
    set(event->bytes, mind_event_field::duration, duration);
    set(event->bytes, mind_event_field::cooldown, get(context.mind, mind_field::unk_2C));
    prepend(context.anchor, *event);
    return event;
}

// Xbox 0x0002A280
RuntimeMindEvent* refresh_runtime_mind_event(RuntimeMindEvents& context, RuntimeMindEvent& event)
{
    set(event.bytes, mind_event_field::time_left, get(event.bytes, mind_event_field::duration));
    set(event.bytes, mind_event_field::cooldown, get(context.mind, mind_field::unk_2C));
    set(event.bytes, mind_event_field::unk_1C, 0);
    set(event.bytes, mind_event_field::unk_0A, 0);
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

// Xbox 0x00028C50, EAX actor
u32 runtime_mind_engaged_actor(const RuntimeMindEvents* context)
{
    if (context == nullptr)
    {
        return 0;
    }
    for (const LiveScript* event = context->anchor.next; event != nullptr; event = event->next)
    {
        if (u16(event->type) == 3)
        {
            return get(static_cast<const RuntimeMindEvent*>(event)->bytes, mind_event_field::subject);
        }
    }
    return 0;
}

// Xbox 0x0002A0C0
void refresh_runtime_mind_target_event(RuntimeMindEvents& context)
{
    for (LiveScript* event = context.anchor.next; event != nullptr; event = event->next)
    {
        if (u16(event->type) == 4)
        {
            auto& target_event = *static_cast<RuntimeMindEvent*>(event);
            if (get(target_event.bytes, mind_event_field::subject) == get(context.mind, mind_field::target_record))
            {
                refresh_runtime_mind_event(context, target_event);
                return;
            }
        }
    }
    auto* event = allocate_runtime_mind_event(context, 4, 20.f);
    set(event->bytes, mind_event_field::subject, get(context.mind, mind_field::target_record));
}

// Xbox 0x0002A030
void set_runtime_mind_target(RuntimeMindEvents& context, u32 target)
{
    if (target == 0 && get(context.mind, mind_field::target_record) != 0)
    {
        const u32 message[]{0x64, 0};
        context.actor_message(message);
    }
    set(context.mind, mind_field::target_record, target);
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
            if (get(event.bytes, mind_event_field::subject) == first && get(event.bytes, mind_event_field::other) == second)
            {
                set(event.bytes, mind_event_field::payload, payload);
                set(event.bytes, mind_event_field::other, second);
                set(event.bytes, mind_event_field::count, get(event.bytes, mind_event_field::count) + 1);
                refresh_runtime_mind_event(context, event);
                return;
            }
        }
    }
    auto* event = allocate_runtime_mind_event(context, 1, 2.f);
    set(event->bytes, mind_event_field::count, 1);
    set(event->bytes, mind_event_field::subject, first);
    set(event->bytes, mind_event_field::other, second);
    set(event->bytes, mind_event_field::payload, payload);
}

// Xbox 0x0002A460
void clear_runtime_mind_actor_events(RuntimeMindEvents& context, u32 actor)
{
    if (get(context.mind, mind_field::target) == actor)
    {
        set(context.mind, mind_field::target, 0);
    }
    if (get(context.mind, mind_field::action_actor) == actor)
    {
        set(context.mind, mind_field::action_actor, 0);
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
        if (get(event.bytes, mind_event_field::subject) == actor || get(event.bytes, mind_event_field::other) == actor)
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
        if (get(event.bytes, mind_event_field::subject) == actor)
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
    if (get(context.mind, mind_field::target_record) == message[1])
    {
        if (get(context.mind, mind_field::target_record) != 0)
        {
            const u32 notice[]{0x64, message[2]};
            context.actor_message(notice);
        }
        set(context.mind, mind_field::target_record, 0);
        if (message[2] != 0 && get(context.mind, mind_field::target) != 0 &&
            get(context.mind, mind_field::target) == get(target, mind_target_field::actor))
        {
            set(context.mind, mind_field::target, 0);
        }
    }
    if (get(context.mind, mind_field::action_record) == message[1])
    {
        set(context.mind, mind_field::action_record, 0);
    }
    LiveScript* head = context.anchor.next;
    LiveScript* event = head;
    while (event != nullptr && u16(event->type) != 3)
    {
        event = event->next;
    }
    const u32 actor = event == nullptr ? 0 : get(static_cast<RuntimeMindEvent*>(event)->bytes, mind_event_field::subject);
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
        if (get(context.mind, mind_field::target_record) != 0)
        {
            const u32 notice[]{0x64, 0};
            context.actor_message(notice);
        }
        set(context.mind, mind_field::target_record, 0);
        return;
    case 0x85:
        if (get(context.mind, mind_field::gaze_anchor) == message[1])
        {
            set(context.mind, mind_field::gaze_anchor, 0);
            set(context.mind, mind_field::unk_34, 0.f);
            set(context.mind, mind_field::unk_38, 0.f);
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
        if (get(request->bytes, mind_request_field::started) == 0)
        {
            const i32 initialized = context.initialize_request(*request);
            set(request->bytes, mind_request_field::started, u8(get(request->bytes, mind_request_field::started) + 1));
            request->active = get(request->bytes, mind_request_field::started);
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
            set(context.events.mind, mind_field::last_request_result, 3);
            clear_runtime_player_mind_requests(context.requests);
            return;
        }
        set(request->bytes, mind_request_field::elapsed, get(request->bytes, mind_request_field::elapsed) + elapsed);
        if (result == 0)
        {
            return;
        }
        set(context.events.mind, mind_field::last_request_result, u32(result));
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
        float timer = get(event.bytes, mind_event_field::cooldown);
        if (timer > 0.f)
        {
            timer -= elapsed;
            set(event.bytes, mind_event_field::cooldown, timer);
            if (0.f > timer)
            {
                set(event.bytes, mind_event_field::cooldown, 0.f);
            }
        }
        timer = get(event.bytes, mind_event_field::time_left);
        if (timer > 0.f)
        {
            timer -= elapsed;
            set(event.bytes, mind_event_field::time_left, timer);
            // COMISS zero,timer; JC retains unordered values too.
            if (!(0.f < timer) && !std::isnan(timer))
            {
                set(event.bytes, mind_event_field::time_left, 0.f);
                script_release_event(node);
                node = next;
                continue;
            }
        }
        set(event.bytes, mind_event_field::age, get(event.bytes, mind_event_field::age) + elapsed);
        set(event.bytes, mind_event_field::unk_1C, get(event.bytes, mind_event_field::unk_1C) + elapsed);
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
    float timer = get(context.events.mind, mind_field::alert_timer);
    if (timer > 0.f)
    {
        timer -= rate * step;
        set(context.events.mind, mind_field::alert_timer, timer);
        if (!(0.f < timer) && !std::isnan(timer))
        {
            set(context.events.mind, mind_field::alert_timer, 0.f);
        }
    }
    switch (get(context.events.mind, mind_field::alert_mode))
    {
    case 0x40:
        set(context.body, body_field::alert_level, 0);
        return;
    case 0x41:
        set(context.body, body_field::alert_level, runtime_mind_alert_level(get(context.events.mind, mind_field::alert_timer)));
        return;
    case 0x42:
        set(context.body, body_field::alert_level, 4);
        return;
    case 0x43:
        set(context.body, body_field::alert_level, 9);
        return;
    case 0x44:
        set(context.body, body_field::alert_level, 7);
        return;
    case 0x45:
        set(context.body, body_field::alert_level, 8);
        return;
    case 0x46:
        set(context.body, body_field::alert_level, 6);
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
    set(context.events.mind, mind_field::unk_34, rate * step + get(context.events.mind, mind_field::unk_34));
    update_runtime_mind_alert(context);
}

// Xbox 0x00136A70
RuntimePlayerMindRequest* append_runtime_position_request(RuntimePlayerMindRequest& anchor,
    std::span<const u8> sizes, std::span<const u8, 12> position)
{
    auto* request = allocate_runtime_mind_request(anchor, 0x2E, sizes);
    set(request->bytes, navigation_request::unk_1C, 1.f);
    set(request->bytes, navigation_request::unk_20, 1.f);
    set(request->bytes, navigation_request::approach, 30.f);
    std::memcpy(field_data(request->bytes, navigation_request::point), position.data(), position.size());
    return request;
}

// Xbox 0x00136AC0
RuntimePlayerMindRequest* append_runtime_range_request(RuntimePlayerMindRequest& anchor, std::span<const u8> sizes,
    float range)
{
    auto* request = allocate_runtime_mind_request(anchor, 0x2F, sizes);
    set(request->bytes, target_position_request::unk_10, 1.f);
    set(request->bytes, target_position_request::unk_14, 1.f);
    set(request->bytes, target_position_request::approach, 30.f);
    set(request->bytes, target_position_request::radius, range);
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
