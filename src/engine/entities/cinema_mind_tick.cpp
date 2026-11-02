#include "cinema_mind_tick.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void value(std::span<u8> bytes, size_t offset, float number)
{
    word(bytes, offset, std::bit_cast<u32>(number));
}
}

// Xbox 0x0007CAB0
void update_runtime_cinema_mind_avoidance(RuntimeCinemaMindAvoidance& context)
{
    auto& base = context.base;
    auto mind = base.events.mind;
    if (context.goals != nullptr)
    {
        const auto& ring = *context.goals;
        const auto row = ring.tail == ring.capacity ? context.address_zero :
            ring.rows.subspan(size_t(ring.head) * ring.stride);
        if (read_u16(row, 0) != 0)
        {
            return;
        }
    }
    const float step = runtime_frame_step(base.clock);
    const float rate = runtime_actor_rate(base.actor);
    const float timer = read_f32(mind, 0xCC) - rate * step;
    value(mind, 0xCC, timer);
    if (timer <= 0.f)
    {
        value(mind, 0xCC, timer + 1.f);
        clear_runtime_player_mind_requests(base.requests);
    }
    if (base.requests.first == nullptr) // Native pointer view of mind +0x58.
    {
        const Vec3 position = actor_world_position(base.actor);
        const float heading = randomize_runtime_navigation_heading(base.actor.euler.y,
            std::bit_cast<float>(0x3F32B8C2u), context.random);
        Vec3 destination;
        RuntimeNavigationHeadingSearch search{position, context.floor, 12.f, 15.f,
            nullptr, context.allowed, &destination, 1, 0xD, i16(base.actor.class_key),
            context.random, context.occupancy_initial};
        if (search_runtime_navigation_headings(search, heading,
            std::bit_cast<float>(0x3E9C61AAu), 10))
        {
            const auto bytes = std::span<const u8, 12>{reinterpret_cast<const u8*>(&destination), 12};
            auto* request = append_runtime_position_request(context.request_anchor, context.request_sizes, bytes);
            value(request->bytes, 0x1C, 1.f);
            value(request->bytes, 0x20, 1.f);
            value(request->bytes, 0x28, 0.f);
        }
    }
}

// Xbox 0x0007CE30
void tick_runtime_cinema_mind(RuntimeCinemaMindTick& context)
{
    auto& base = context.avoidance.base;
    auto& messages = context.messages;
    auto& parent = messages.parent;
    auto mind = base.events.mind;
    auto body = base.body;
    if (read_f32(mind, 0x98) > 0.f)
    {
        update_runtime_cinema_mind_avoidance(context.avoidance);
    }
    select_runtime_player_mind_alert(mind, body, parent.flags, context.player_link);
    update_runtime_mind_requests(base);
    update_runtime_mind_events(base);
    const float step = runtime_frame_step(base.clock);
    const float rate = runtime_actor_rate(base.actor);
    set(mind, mind_field::unk_34, rate * step + get(mind, mind_field::unk_34));
    update_runtime_mind_alert(base);
    update_runtime_player_mind_gaze(context.gaze, 0, 20.f, 80.f);
    if (get(mind, mind_field::target) == 0)
    {
        if (get(mind, mind_field::target_mode) != 0 && get(mind, mind_field::unk_38) > get(mind, mind_field::unk_34))
        {
            set(mind, mind_field::unk_34, get(mind, mind_field::unk_38));
        }
        set(mind, mind_field::target_mode, 0);
    }
    else
    {
        const Vec3 first = actor_world_position(base.actor);
        const Vec3 second = actor_world_position(*context.gaze.target);
        const float x = first.x - second.x;
        const float y = first.y - second.y;
        const float z = first.z - second.z;
        const float distance = (z * z + y * y) + x * x;
        if (get(mind, mind_field::target_mode) == 0)
        {
            const float radius = read_f32(mind, 0x7C);
            if (!(radius > 0.f))
            {
                if (get(mind, mind_field::unk_38) > get(mind, mind_field::unk_34))
                {
                    set(mind, mind_field::unk_34, get(mind, mind_field::unk_38));
                }
                set(mind, mind_field::target_mode, 1);
            }
            else
            {
                const bool active = radius * radius > distance;
                if (active && get(mind, mind_field::unk_38) > get(mind, mind_field::unk_34))
                {
                    set(mind, mind_field::unk_34, get(mind, mind_field::unk_38));
                }
                set(mind, mind_field::target_mode, u8(active));
            }
        }
        else
        {
            const float radius = read_f32(mind, 0x80);
            if (radius > 0.f)
            {
                const u8 active = u8(radius * radius > distance);
                if (active != get(mind, mind_field::target_mode) && get(mind, mind_field::unk_38) > get(mind, mind_field::unk_34))
                {
                    set(mind, mind_field::unk_34, get(mind, mind_field::unk_38));
                }
                set(mind, mind_field::target_mode, active);
            }
        }
    }
    const u32 target_address = get(mind, mind_field::target_record);
    if (target_address != 0 && body[0x6A] != 0)
    {
        const float angle = runtime_actor_bound_angle(messages.target_at(target_address).bytes, base.actor);
        const std::array<float, 2> axes{float(retail_sin(angle)), float(retail_cos(angle))};
        set_runtime_input_secondary_axes(context.writable, axes, context.keyboard);
    }
    if (body[0x6A] == 0)
    {
        word(body, 0x88, 0);
        set(mind, mind_field::action_record, 0);
    }
    body[0xAA] = 0;
    if (target_address != 0 && (get(messages.actor_bytes, player_field::countdown_56) > 0.f
        || messages.actor_bytes[0x969] != 0)
        && !runtime_mind_target_is_eligible(mind, messages.actor_bytes, base.actor,
            messages.actor_address, messages.target_at(target_address), true))
    {
        body[0xAA] = 1;
    }
    update_runtime_player_boost_timer(messages.boost, context.control, nullptr, 1.f);
    const i32 notices = i32(read_u32(mind, 0x90));
    if (notices > 0)
    {
        word(mind, 0x90, u32(notices - 1));
    }
}
}
