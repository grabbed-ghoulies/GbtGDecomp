#include "player_mind_requests.h"
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "../scripts/script_vm.h"
#include "../runtime/name_lookup.h"
#include "../graphics/actor_effect_stream.h"
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
const char* name(std::span<const u8> bytes, size_t offset)
{
    return reinterpret_cast<const char*>(bytes.data() + offset);
}
}

// Xbox 0x00137A80
bool request_runtime_player_boost(RuntimePlayerMindBoost& context)
{
    auto body = context.attachment.body;
    if (read_u16(context.actor_bytes, 0xCDC) == 2 || read_u32(context.actor_bytes, 0x93C) != 0)
    {
        if ((runtime_player_can_resume_strategy(context.actor_bytes, context.resume) != 0
                || (context.story_word & 0x80) != 0)
            && read_u16(body, 0x94) != 0 && (context.flags.bytes[7] & 2) == 0
            && (context.flags.bytes[7] & 4) == 0)
        {
            const u16 key = context.range_action_key();
            if (runtime_body_has_action_id(context.attachment.actions.table, key))
            {
                put(context.boost, 0xC, 0);
                put(context.boost, 8, 0);
                put(context.boost, 0x14, 0);
                put(context.boost, 0x10, 0);
                put(context.boost, 0x18, 0);
                put(context.boost, 0x1C, 1);
                put(context.boost, 0x24, 0);
                put(context.boost, 0x20, 0);
                put(context.boost, 0x2C, 0);
                put(context.boost, 0x28, 0);
                put(context.boost, 0x34, 1);
                put(context.boost, 0x30, 0);
                if (!(read_f32(context.boost, 4) > 0.f))
                {
                    const u32 fresh = test_and_set_runtime_story_bit(context.story_bits, 1);
                    put(context.boost, 0, fresh);
                    put(context.boost, 4, std::bit_cast<u32>(fresh != 0 ? 2.5f : 5.f));
                    set_runtime_body_bit(context.flags, 0x2C);
                    release_runtime_player_attachment(0, 0, context.attachment);
                    if (runtime_body_timer_is_idle(body))
                    {
                        select_runtime_body_action(body, key, context.attachment.actions);
                    }
                    ++body[0x309];
                    ++body[0x30A];
                    ++body[0x30B];
                    ++body[0x30C];
                    const std::array<i32, 2> message{0x77, i32(context.actor_address)};
                    broadcast_actor_pulse_message(message.data());
                    return true;
                }
                put(context.boost, 4,
                    std::bit_cast<u32>(read_u32(context.boost, 0) != 0 ? 2.5f : 5.f));
                if (!runtime_body_bit_set(context.flags, 0x1E))
                {
                    select_runtime_body_action(body, key, context.attachment.actions);
                }
            }
            return false;
        }
    }
    return false;
}

// Xbox 0x00137C30
bool request_runtime_player_turn(RuntimePlayerMindBoost& context, const Vec3& point)
{
    auto body = context.attachment.body;
    if ((read_u16(context.actor_bytes, 0xCDC) == 2 || read_u32(context.actor_bytes, 0x93C) != 0)
        && !(read_f32(context.boost, 4) > 0.f)
        && (context.flags.bytes[7] & 2) == 0 && (context.flags.bytes[7] & 4) == 0
        && read_u16(body, 0x94) != 0)
    {
        if (runtime_body_timer_is_idle(body) && body[0x6A] == 0)
        {
            const u16 key = context.contact_action_key();
            if (!runtime_body_has_action_id(context.attachment.actions.table, key))
            {
                return false;
            }
            const float angle = runtime_actor_direction_to_point(context.actor, point);
            set_runtime_player_actor_rotation(context.actor, context.body_actor, body, Vec3{0.f, angle, 0.f});
            select_runtime_body_action(body, key, context.attachment.actions);
            return true;
        }
    }
    return false;
}

// Xbox 0x0007F8C0
bool notify_runtime_player_boost(RuntimePlayerMindBoost& context, RuntimePlayerMindFeedback& feedback)
{
    const bool result = request_runtime_player_boost(context);
    if (result && read_u32(feedback.input, 4) != 0)
    {
        if (runtime_input_is_selected(feedback.input, feedback.input_address, feedback.activation))
        {
            const Vec3 position = actor_world_position(feedback.actor);
            dispatch_runtime_body_position_event(position, 0x29, feedback.events);
        }
    }
    return result;
}

// Xbox 0x0007F920
bool notify_runtime_player_turn(RuntimePlayerMindBoost& context,
    RuntimePlayerMindFeedback& feedback, const Vec3& point)
{
    const bool result = request_runtime_player_turn(context, point);
    if (result)
    {
        if (read_u32(feedback.input, 4) != 0)
        {
            if (runtime_input_is_selected(feedback.input, feedback.input_address, feedback.activation))
            {
                const Vec3 position = actor_world_position(feedback.actor);
                dispatch_runtime_body_position_event(position, 0x29, feedback.events);
            }
        }
        return result;
    }
    return false;
}

// Xbox 0x0007DD30
void notify_runtime_player_mind_attachment_callout(RuntimeParentMindRequests& context,
    const std::span<const u8>* action_target, u32 sender)
{
    const auto* attached = context.attachment.actions.attached;
    if (attached != nullptr && attached->bytes[0x93D] == 4
        && (context.flags.bytes[5] & 0x80) != 0 && action_target != nullptr
        && read_u32(*action_target, 0x10) == sender)
    {
        if (actor_event_chance(context.random, 0.5f))
        {
            dispatch_actor_resource_callout(context.resource, 0x3F, nullptr, context.callout);
        }
    }
}

// Xbox 0x0003BFA0
u32 runtime_player_callout_name_id(const char* value, const char* const* names)
{
    if (std::strlen(value) != 0)
    {
        const i32 index = binary_search_names(0x55, names, value);
        if (index != -1)
        {
            return u32(index) + 1;
        }
    }
    return 0;
}

// Xbox 0x0003B290
bool runtime_actor_animation_slot_complete(const ActorAnimationState& animation, u32 slot)
{
    if (animation.controller == nullptr || animation.controller->slots[slot].player == nullptr)
    {
        return true;
    }
    const auto& player = *animation.controller->slots[slot].player;
    if (player.mode == 1)
    {
        if (0.f > player.speed)
        {
            return !(0.f < player.position) && !std::isnan(player.position);
        }
        return !(player.position < 1.f) && !std::isnan(player.position);
    }
    return false;
}

// Xbox 0x001416D0
float runtime_player_target_radius(std::span<const u8> body, const Actor& body_actor,
    const RuntimeBodyAttachedActor* attached, const Actor& target)
{
    const float radius = body_actor.radius_xz;
    const float margin = read_f32(body, 0x78) * radius;
    float model_radius = 0.f;
    const u8 category = attached == nullptr ? 0 : attached->bytes[0x93D];
    if (category == 1 && attached->actor->model != nullptr &&
        attached->actor->model->cpu.data() != nullptr)
    {
        model_radius = actor_model_radius(*attached->actor);
    }
    const float model_margin = model_radius + margin;
    return read_f32(body, 0x888) * radius + model_margin + target.radius_xz;
}

// Xbox 0x00129C20
Actor* find_runtime_player_nearest_spawn(const Actor& actor, float* distance, u8 flags)
{
    const Vec3 position = actor_world_position(actor);
    return find_nearest_spawn_actor(position, &actor, distance, flags);
}

// Xbox 0x000295E0
i32 update_runtime_parent_mind_request(RuntimeParentMindRequests& context, std::span<const u8> request)
{
    auto mind = context.events.mind;
    auto body = context.attachment.body;
    auto& actions = context.attachment.actions;
    const u16 kind = read_u16(request, 8);
    switch (kind)
    {
    case 5:
        load_runtime_mind_goal(name(request, 0x10), context.goals);
        return 2;
    case 6:
        dispatch_actor_resource_callout(context.resource,
            i32(runtime_player_callout_name_id(name(request, 0x10), context.callout_names)),
            nullptr, context.callout);
        return 1;
    case 9:
        return actor_event_chance(context.random, read_f32(request, 0x10)) ? 3 : 1;
    case 0xA:
        return read_u32(mind, 0x5C) != read_u32(request, 0x10) ? 1 : 3;
    case 0xB:
        if (read_u32(mind, 0x10) != 0)
        {
            const Vec3 position = actor_world_position(context.actor);
            const float z = position.z - read_f32(context.target_bytes, 0x28);
            const float x = position.x - read_f32(context.target_bytes, 0x20);
            const float squared = z * z + x * x;
            float radius;
            if (read_u32(request, 0x14) == 0)
            {
                radius = context.body_actor.radius_xz * read_f32(body, 0x888) +
                    read_f32(context.target_bytes, 0x38);
            }
            else
            {
                radius = runtime_player_target_radius(body, context.body_actor, actions.attached,
                    *context.target_actor);
            }
            radius = read_f32(request, 0x10) + radius;
            if (!(squared > radius * radius))
            {
                return 1;
            }
        }
        return 3;
    case 0xC:
        assign_runtime_body_bit(context.flags, 7, read_u32(request, 0x10));
        return 1;
    case 0x1A:
    {
        actor_set_uniform_scale(context.actor, context.actor.radius_xz * read_f32(request, 0x10));
        actor_refresh_transform(context.actor);
        u32 notice[]{0x4A};
        context.actor_message(notice);
        return 1;
    }
    case 0x1B:
    {
        const u16 key = runtime_body_action_name_id(name(request, 0x10), context.action_names);
        put(mind, 0x14, read_u32(mind, 0x10));
        if (!select_runtime_attachment_request(context.attachment, key))
        {
            put(mind, 0x14, 0);
        }
        return 1;
    }
    case 0x1C:
        set_runtime_body_bit(context.flags, runtime_body_flag_name_index(name(request, 0x10), context.flag_names));
        return 1;
    case 0x1D:
    {
        u32 notice[]{0x3B, 0, runtime_player_goal_flag_name_id(name(request, 0x10), context.goal_flag_names)};
        context.actor_message(notice);
        return notice[1] != 0 ? 3 : 1;
    }
    case 0x1E:
        put(body, 0x70, std::bit_cast<u32>(runtime_body_random_range(context.random,
            read_f32(request, 0x10), read_f32(request, 0x14))));
        return 1;
    case 0x1F:
        select_runtime_body_action(body, runtime_body_action_name_id(name(request, 0x10),
            context.action_names), actions);
        return 1;
    case 0x20:
        if (!runtime_actor_animation_slot_complete(context.animation, read_u32(request, 0x10)))
        {
            return 0;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(name(request, 0x14),
            context.action_names), actions);
        return 1;
    case 0x21:
    {
        const u32 index = runtime_body_flag_name_index(name(request, 0x50), context.flag_names);
        if (!runtime_body_bit_matches(context.flags, index, read_u32(request, 0x80)))
        {
            return read_u32(request, 0x84) != 0 ? 0 : 1;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(name(request, 0x10),
            context.action_names), actions);
        return 1;
    }
    case 0x22:
    {
        const u32 index = runtime_body_flag_name_index(name(request, 0x50), context.flag_names);
        if (!runtime_body_bit_matches(context.flags, index, read_u32(request, 0x80)))
        {
            return read_u32(request, 0x88) != 0 ? 0 : 1;
        }
        if (!actor_event_chance(context.random, read_f32(request, 0x84)))
        {
            return 1;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(name(request, 0x10),
            context.action_names), actions);
        return 1;
    }
    case 0x23:
    {
        const u16 key = context.body_effect_key();
        if (key != 0 && runtime_body_has_action_id(actions.table, key))
        {
            select_runtime_body_action(body, key, actions);
            return 1;
        }
        return 4;
    }
    case 0x28:
        put(mind, 0x18, context.actor_address(find_runtime_player_nearest_spawn(context.actor, nullptr, 0x1F)));
        return 1;
    case 0x29:
        return read_f32(request, 0xC) >= read_f32(request, 0x18) ? 1 : 0;
    case 0x2A:
        return runtime_body_bit_matches(context.flags,
            runtime_body_flag_name_index(name(request, 0x10), context.flag_names), read_u32(request, 0x40)) ? 1 : 0;
    case 0x2B:
        return script_mind_is_idle(context.body_query, i32(read_u32(request, 0x10))) ? 1 : 0;
    case 0x2C:
        return runtime_body_timer_is_idle(body) ? 1 : 0;
    default:
        return 1;
    }
}
}
