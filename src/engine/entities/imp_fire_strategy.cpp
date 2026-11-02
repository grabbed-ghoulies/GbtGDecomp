#include "imp_fire_strategy.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "body_animation_setup.h"
#include "body_message_helpers.h"
#include "actor_transform.h"
#include "player_strategy.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/player_avoidance.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <bit>
#include <cstring>
#include <cmath>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

constexpr u32 fire_parameters_address = 0x004287BC; // DAT_004287BC.

RuntimePlayerStrategyGoals& goals_of(RuntimeImpFireStrategy& context)
{
    return context.base.goals;
}

// The strategy's own bytes; the actor's +0x954 object names the same instance once attached.
std::span<u8> strategy_of(RuntimeImpFireStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of_actor(RuntimeImpFireStrategy& context)
{
    return goals_of(context).entry.body;
}

bool requests_pending(RuntimeImpFireStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

bool body_settled(std::span<const u8> body)
{
    return get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f;
}

void change_state(u16 state, RuntimeImpFireStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

// Actor +0x50 controller with a named "aid_" animation in slot 1 (slot +0x28 player, +0xB0
// resource, +0x2C name).
bool named_slot_animation(const AnimationController* controller)
{
    if (controller == nullptr)
    {
        return false;
    }
    const MinaPlayer* player = controller->slots.at(1).player.get();
    return player != nullptr && player->mina.data() != nullptr && player->resource_name.compare(0, 4, "aid_") == 0;
}

RuntimeMindTarget* selected_target(RuntimeImpFireStrategy& context, bool& found)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context));
    return target;
}
}

// Xbox 0x000B0B90
void imp_fire_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x50;
    output.parameters = 0x18;
}

// Xbox 0x000B43B0
void set_runtime_imp_fire_effect(u32 state, RuntimeImpFireStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    const u32 previous = read_u32(strategy, 0x40);
    if (state == previous)
    {
        return;
    }
    if (previous == 2)
    {
        if (read_u32(strategy, 0x44) != 0)
        {
            const std::array<u32, 2> message{5, 0};
            context.object_message(read_u32(strategy, 0x44), message);
        }
        AnimationController* controller = context.animation.controller.get();
        if (named_slot_animation(controller))
        {
            if (controller->slots.at(1).player != nullptr)
            {
                clear_animation_controller_slot(*controller, 1, true, context.registry);
            }
            controller->dirty = true;
        }
    }
    put(strategy, 0x40, state);
    if (state == 2 && previous != 2)
    {
        put(strategy, 0x44, context.create_effect(context.effect_name, read_u32(strategy, 8)));
    }
}

// Xbox 0x000B4380
void end_runtime_imp_fire_effect(RuntimeImpFireStrategy& context)
{
    const u32 object = read_u32(strategy_of(context), 0x44);
    if (object != 0)
    {
        const std::array<u32, 2> message{5, 0};
        context.object_message(object, message);
    }
}

// Xbox 0x000B4330
bool runtime_imp_fire_effect_playing(RuntimeImpFireStrategy& context)
{
    const bool playing = named_slot_animation(context.animation.controller.get());
    return read_u32(strategy_of(context), 0x40) == 2 && playing;
}

// Xbox 0x000B4570
bool initialize_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    put(strategy, 0x48, 0);
    put(strategy, 0x40, 0);
    set_runtime_imp_fire_effect(1, context);
    put(strategy, 0x4C, fire_parameters_address);
    return true;
}

// Xbox 0x000B45B0
void destroy_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context)
{
    set_runtime_imp_fire_effect(0, context);
}

// Xbox 0x000C2D90
void release_runtime_imp_fire_hold(RuntimeImpFireStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    const u32 held = read_u32(strategy, 0x3C);
    if (held == 0)
    {
        return;
    }
    const RuntimeStrategyBodyView body = context.body_at(held);
    if (read_u32(body.bytes, 0x50) == read_u32(strategy, 8))
    {
        put(body.bytes, 0x50, 0);
        put(strategy, 0x3C, 0);
    }
}

// Xbox 0x000B4010
float runtime_imp_fire_range_fraction(std::span<const u8> target, RuntimeImpFireStrategy& context)
{
    const Vec3 position = actor_world_position(goals_of(context).entry.actor);
    const float dx = position.x - get(target, mind_target_field::shape_position).x;
    const float dy = position.y - get(target, mind_target_field::shape_position).y;
    const float dz = position.z - get(target, mind_target_field::shape_position).z;
    const float nearest = read_f32(context.parameters, 0x18);
    const float farthest = read_f32(context.parameters, 0x1C);
    if (farthest == nearest)
    {
        return 1.f; // DAT_0032B0F8.
    }
    const long double distance = std::sqrt((static_cast<long double>(dz) * dz + static_cast<long double>(dx) * dx)
        + static_cast<long double>(dy) * dy);
    const float fraction = float((distance - nearest) / (static_cast<long double>(farthest) - nearest));
    if (fraction > 1.f)
    {
        return 1.f;
    }
    if (0.f > fraction)
    {
        return 0.f;
    }
    return fraction;
}

// Xbox 0x000B40E0
float blend_runtime_imp_fire_heading(float fraction, float angle)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const long double remainder = animation_remainder(angle, tau);
    float wrapped = float(remainder);
    if (0.L > remainder)
    {
        wrapped += tau; // DAT_0032B298.
    }
    const float opposite = wrapped + std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    const long double blended = animation_remainder((wrapped - opposite) * fraction + opposite, tau);
    float result = float(blended);
    if (0.L > blended)
    {
        result += tau;
    }
    return result;
}

// Xbox 0x000B4160
float select_runtime_imp_fire_heading(const RuntimeMindTarget& target, RuntimeImpFireStrategy& context)
{
    Actor& actor = goals_of(context).entry.actor;
    const float fraction = runtime_imp_fire_range_fraction(target.bytes, context);
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{get(target.bytes, mind_target_field::shape_position).x - position.x, get(target.bytes, mind_target_field::shape_position).y - position.y,
        get(target.bytes, mind_target_field::shape_position).z - position.z};
    // The output reuses the caller's argument slot, which holds the parameter block address.
    float angle = std::bit_cast<float>(read_u32(strategy_of(context), 0x4C));
    runtime_direction_angle(direction, angle);
    const float heading = blend_runtime_imp_fire_heading(fraction, angle);
    const float reflected = reflect_runtime_angle(angle, heading);
    const float facing = read_f32(goals_of(context).entry.actor_bytes, 0x18);
    const float chosen = nearest_runtime_angle(facing, heading, reflected);
    float scale;
    if (fraction > 0.5f) // DAT_0032B0F4.
    {
        scale = (1.f - fraction) * 2.f; // DAT_0032B110.
    }
    else
    {
        scale = fraction * 2.f;
    }
    const float width = read_f32(context.parameters, 0x20) * scale * std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    return randomize_runtime_navigation_heading(chosen, width, goals_of(context).random);
}

// Xbox 0x000B4260
bool select_runtime_imp_fire_position(const RuntimeMindTarget& target, const Vec3& origin, float heading, Vec3& output,
    RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const float range = read_f32(context.parameters, 4);
    RuntimeNavigationHeadingSearch search{origin, i32(get(target.bytes, mind_target_field::floor)), range - 0.5f, range + 0.5f, nullptr,
        goals.entry.allowed, &output, 1, 0xD, i16(read_u16(goals.entry.actor_bytes, 6)), goals.random,
        0.f}; // Flag 0x10 is clear, so the retained occupancy scratch is not read.
    return search_runtime_navigation_headings(search, heading, std::bit_cast<float>(0x3E9C61AAu), 10);
}

// Xbox 0x000B42D0
void approach_runtime_imp_fire_target(const RuntimeMindTarget& target, RuntimeImpFireStrategy& context)
{
    const float heading = select_runtime_imp_fire_heading(target, context);
    const Vec3 position = actor_world_position(goals_of(context).entry.actor);
    Vec3 selected;
    if (select_runtime_imp_fire_position(target, position, heading, selected, context))
    {
        request_runtime_strategy_position(selected, context.parameters, goals_of(context));
    }
}

// Xbox 0x000B4EF0
bool roll_runtime_imp_fire_attachment(RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<const u8> mind = goals.entry.events.mind;
    if (mind.data() == nullptr || get(mind, mind_field::target_record) == 0)
    {
        return false;
    }
    const Actor& target = goals.target_at(get(mind, mind_field::target_record))->actor;
    i32 index = context.attachment_limit;
    const RuntimeBodyModelNodes* model = context.model_of(target);
    const u32 count = count_runtime_body_model_kind(model, 4);
    if (count == 0)
    {
        return false;
    }
    if (index > i32(count - 1))
    {
        index = i32(count - 1);
    }
    if (find_runtime_body_model_record(model, 4, index) == nullptr)
    {
        return false;
    }
    return roll_runtime_actor_attack(context.checks.attributes, goals.random);
}

// Xbox 0x000B4F50
void grab_runtime_imp_fire_target(const Actor* target, RuntimeImpFireStrategy& context)
{
    if (target == nullptr)
    {
        return;
    }
    std::span<u8> strategy = strategy_of(context);
    const RuntimeStrategyBodyView body = context.body_of(*target);
    if (roll_runtime_imp_fire_attachment(context) && read_u32(body.bytes, 0x50) == 0
        && (body.bits->bytes[3] & 8) == 0 && read_u32(body.bytes, 0x58) != 0)
    {
        put(body.bytes, 0x50, read_u32(strategy, 8));
        put(strategy, 0x3C, body.address);
        change_state(0x40B, context);
        return;
    }
    change_state(0x3EE, context);
}

// Xbox 0x000B44D0
u32 engage_runtime_imp_fire_target(const RuntimeMindTarget& target, RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<const u8> body = body_of_actor(context);
    const EntityBitBlock& bits = *goals.body_bits;
    if (body[0x6B] != 0 || body[0x6A] != 0 || get(body, body_field::unk_70) > 0.f || !body_settled(body)
        || (bits.bytes[3] & 8) != 0 || runtime_body_bit_set(bits, 0x2C) || runtime_body_bit_set(bits, 0x2A))
    {
        return 0;
    }
    if (!join_runtime_strategy_target_queue(context.base.link, target.record, &goals.entry.actor, context.queue))
    {
        detach_runtime_strategy_link(context.base.link);
        return 0;
    }
    grab_runtime_imp_fire_target(&target.actor, context);
    return 1;
}

// Xbox 0x000B3F80
void update_runtime_imp_fire_seek(RuntimeImpFireStrategy& context)
{
    if (!body_settled(body_of_actor(context)) || requests_pending(context))
    {
        return;
    }
    bool found;
    RuntimeMindTarget* target = selected_target(context, found);
    if (!found)
    {
        return;
    }
    if (target == nullptr || runtime_strategy_abandons_target(*target, context.checks) != 0)
    {
        change_state(0x401, context);
        return;
    }
    if (engage_runtime_imp_fire_target(*target, context) != 0)
    {
        return;
    }
    approach_runtime_imp_fire_target(*target, context);
}

// Xbox 0x000B4AF0
void update_runtime_imp_fire_attack(RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const float step = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(goals.entry.actor);
    const float delay = read_f32(strategy, 0x38);
    if (delay > 0.f)
    {
        const float remaining = delay - rate * step;
        put_float(strategy, 0x38, remaining);
        if (0.f >= remaining)
        {
            put_float(strategy, 0x38, 0.f);
        }
    }
    if (read_f32(strategy, 0x38) != 0.f && requests_pending(context)
        && runtime_strategy_link_target(context.base.link, context.queue) != nullptr)
    {
        return;
    }
    const float minimum = read_f32(context.parameters, 0x2C);
    const float maximum = read_f32(context.parameters, 0x30);
    goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    put_float(body_of_actor(context), 0x70, (maximum - minimum) * fraction + minimum);
    change_state(0x40C, context);
}

// Xbox 0x000B4C20
void update_runtime_imp_fire_retreat(RuntimeImpFireStrategy& context)
{
    bool found;
    RuntimeMindTarget* target = selected_target(context, found);
    if (found)
    {
        if (runtime_strategy_abandons_target(*target, context.checks) == 0)
        {
            goals_of(context).room = nullptr; // Actor +0xCA0.
            change_state(0x40C, context);
            return;
        }
        if (retreat_runtime_strategy_from_reached_target(context.checks) != 0)
        {
            return;
        }
    }
    if (!requests_pending(context))
    {
        request_runtime_strategy_roam(context.parameters, context.checks);
    }
}

// Xbox 0x000B4CC0
void update_runtime_imp_fire_mutate(RuntimeImpFireStrategy& context)
{
    if (!body_settled(body_of_actor(context)) || requests_pending(context))
    {
        return;
    }
    bool found;
    selected_target(context, found);
    if (found)
    {
        change_state(0x3EF, context);
    }
}

// Xbox 0x000B4D50
void update_runtime_imp_fire_wait(RuntimeImpFireStrategy& context)
{
    if (!requests_pending(context))
    {
        change_state(0x40C, context);
    }
}

// Xbox 0x000B4DA0
u16 runtime_imp_fire_idle_state(RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    if (runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0)
    {
        return 0x40C;
    }
    return 0x3E8;
}

// Xbox 0x000B4DF0
void update_runtime_imp_fire_bite(RuntimeImpFireStrategy& context)
{
    bool found;
    selected_target(context, found);
    if (!found)
    {
        change_state(runtime_imp_fire_idle_state(context), context);
        return;
    }
    if (!body_settled(body_of_actor(context)) || requests_pending(context))
    {
        return;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<const u8> mind = goals.entry.events.mind;
    if (mind.data() != nullptr && get(mind, mind_field::target_record) != 0 && runtime_imp_fire_effect_playing(context))
    {
        std::span<u8> strategy = strategy_of(context);
        if (read_u32(strategy, 0x48) != 0)
        {
            load_runtime_mind_goal(context.goal_bite, goals.entry.goals);
            return;
        }
        strategy[0x1A] = 1;
        load_runtime_mind_goal(context.goal_bite_prepare, goals.entry.goals);
        return;
    }
    change_state(runtime_imp_fire_idle_state(context), context);
}

// Xbox 0x000B46B0
void dispatch_runtime_imp_fire_message(std::span<const u32> message, RuntimeImpFireStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    const u32 actor = read_u32(strategy, 8);
    if (react_runtime_strategy_message(message, context.checks) != 0 || message[0] - 0x14 > 0x70)
    {
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
    switch (message[0])
    {
    case 0x14:
        if (read_u32(strategy, 0x44) == message[1])
        {
            put(strategy, 0x44, 0);
        }
        return;
    case 0x26:
    case 0x27:
        if (read_u16(strategy, 0xE) == 0x3EF && read_u32(strategy, 0x48) != 0)
        {
            return;
        }
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    case 0x31:
    {
        const u32 held = read_u32(strategy, 0x3C);
        if (held == 0)
        {
            return;
        }
        if (message[1] == read_u32(context.body_at(held).bytes, 8))
        {
            release_runtime_imp_fire_hold(context);
        }
        return;
    }
    case 0x36:
        notice_runtime_strategy_hostile_target(0x10, context.checks);
        return;
    case 0x3B:
        put(strategy, 0x48, 1);
        return;
    case 0x43:
        if (message[1] == actor)
        {
            set_runtime_imp_fire_effect(1, context);
        }
        return;
    case 0x44:
        if (message[1] == actor)
        {
            end_runtime_imp_fire_effect(context);
        }
        return;
    case 0x52:
        if (read_u16(strategy, 0xE) == 0x3EF)
        {
            set_runtime_imp_fire_effect(1, context);
        }
        return;
    case 0x6E:
    {
        const bool own = message[1] == actor;
        const u16 state = read_u16(strategy, 0xE);
        dispatch_runtime_base_strategy_message(message, context.base);
        if (check_runtime_player_knockout(goals) != 0)
        {
            return;
        }
        if (own)
        {
            switch (state)
            {
            case 0x3EE:
            case 0x401:
                change_state(0x40C, context);
                return;
            case 0x3EF:
                change_state(0x3EF, context);
                return;
            case 0x40B:
                change_state(runtime_imp_fire_effect_playing(context) ? 0x3EF : 0x40C, context);
                return;
            default:
                return;
            }
        }
        if (state == 0x3EF)
        {
            return;
        }
        react_runtime_strategy_contact(message, -1.f, context.checks); // DAT_0032B120.
        return;
    }
    case 0x7C:
        if (message[1] == actor)
        {
            release_runtime_imp_fire_hold(context);
        }
        return;
    case 0x80:
        release_runtime_imp_fire_hold(context);
        return;
    case 0x82:
        react_runtime_strategy_cower(context.checks);
        return;
    case 0x84:
        react_runtime_strategy_startle(context.checks);
        return;
    default:
        dispatch_runtime_base_strategy_message(message, context.base);
        return;
    }
}

// Xbox 0x000B45D0
void update_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context)
{
    const u16 state = read_u16(strategy_of(context), 0xE);
    update_runtime_base_strategy(goals_of(context));
    switch (state)
    {
    case 0x3EE:
        update_runtime_imp_fire_attack(context);
        return;
    case 0x3EF:
        update_runtime_imp_fire_bite(context);
        return;
    case 0x401:
        update_runtime_imp_fire_retreat(context);
        return;
    case 0x40B:
        update_runtime_imp_fire_mutate(context);
        return;
    case 0x40C:
        update_runtime_imp_fire_seek(context);
        return;
    case 0x416:
        update_runtime_imp_fire_wait(context);
        return;
    default:
        return;
    }
}

// Xbox 0x000B4940
void enter_runtime_imp_fire_state(u16 state, u16 previous, RuntimeImpFireStrategy& context)
{
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = body_of_actor(context);
    body[0x328] = 2;
    switch (state)
    {
    case 0x3EE:
        body[0x328] = 4;
        load_runtime_mind_goal(context.goal_attack, goals.entry.goals);
        put(strategy, 0x38, read_u32(context.parameters, 0x28));
        return;
    case 0x3EF:
        body[0x328] = 6;
        return;
    case 0x401:
    case 0x416:
        return;
    case 0x40B:
        body[0x328] = 6;
        set_runtime_imp_fire_effect(2, context);
        load_runtime_mind_goal(context.goal_mutate, goals.entry.goals);
        return;
    case 0x40C:
        body[0x328] = 3;
        put_float(strategy, 0x38, 0.f);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state);
        return;
    }
}

// Xbox 0x000B4A60
void leave_runtime_imp_fire_state(u16 state, u16 next, RuntimeImpFireStrategy& context)
{
    std::span<u8> strategy = strategy_of(context);
    if (next != 0x3EE)
    {
        detach_runtime_strategy_link(context.base.link);
    }
    if (state == 0x3EF || state == 0x40B)
    {
        put(strategy, 0x48, 0);
        strategy[0x1A] = 0;
        if (next != 0x3EF)
        {
            set_runtime_imp_fire_effect(1, context);
            release_runtime_imp_fire_hold(context);
        }
        return;
    }
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy[0x1C];
    }
}
}
