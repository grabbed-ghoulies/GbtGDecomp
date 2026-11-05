#include "player_mind_requests.h"
#include "weapon_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "mind_request_fields.h"
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "../scripts/script_vm.h"
#include "../runtime/name_lookup.h"
#include "../graphics/actor_effect_stream.h"
#include "body_fields.h"
#include <bit>
#include <cstring>
#include <cmath>

namespace gbtg
{
// Xbox 0x0007F8C0
bool notify_runtime_player_boost(RuntimeMindBoost& context, RuntimePlayerMindFeedback& feedback)
{
    const bool result = request_runtime_mind_boost(context);
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
bool notify_runtime_player_turn(RuntimeMindBoost& context,
    RuntimePlayerMindFeedback& feedback, const Vec3& point)
{
    const bool result = request_runtime_mind_turn(context, point);
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
    if (attached != nullptr && weapon_base(attached->bytes).unk_93D == 4
        && (context.flags.bytes[5] & 0x80) != 0 && action_target != nullptr && !action_target->empty() // Mind +0x14.
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

// Xbox 0x0003B1F0
bool runtime_actor_animation_crossed_position(const ActorAnimationState& animation,
    u32 slot, float position)
{
    if (animation.controller == nullptr || animation.controller->slots[slot].player == nullptr)
    {
        return false;
    }
    const auto& player = *animation.controller->slots[slot].player;
    if (player.position == player.previous)
    {
        return false;
    }
    float end = player.position;
    float start = player.previous;
    if (0.f > player.speed)
    {
        start = player.position;
        end = player.previous;
    }
    if (!(end > start))
    {
        if (position < start || std::isnan(position) || std::isnan(start))
        {
            return end > position;
        }
        return true;
    }
    if (position < start || std::isnan(position) || std::isnan(start))
    {
        return false;
    }
    return end > position;
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
    const float margin = body_record(body).reach * radius;
    float model_radius = 0.f;
    const u8 category = attached == nullptr ? 0 : weapon_base(attached->bytes).unk_93D;
    if (category == 1 && attached->actor->model != nullptr &&
        attached->actor->model->cpu.data() != nullptr)
    {
        model_radius = actor_model_radius(*attached->actor);
    }
    const float model_margin = model_radius + margin;
    return body_record(body).radius_scale * radius + model_margin + target.radius_xz;
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
    const u16 kind = get(request, mind_request_field::kind);
    switch (kind)
    {
    case 5:
        load_runtime_mind_goal(field_data(request, named_request::name), context.goals);
        return 2;
    case 6:
        dispatch_actor_resource_callout(context.resource,
            i32(runtime_player_callout_name_id(field_data(request, named_request::name), context.callout_names)),
            nullptr, context.callout);
        return 1;
    case 9:
        return actor_event_chance(context.random, get(request, value_request::chance)) ? 3 : 1;
    case 0xA:
        return mind_record(mind).last_request_result != get(request, value_request::value) ? 1 : 3;
    case 0xB:
        if (mind_record(mind).target_record != 0)
        {
            const Vec3 position = actor_world_position(context.actor);
            const float z = position.z - mind_target_record(context.target_bytes).shape.position.z;
            const float x = position.x - mind_target_record(context.target_bytes).shape.position.x;
            const float squared = z * z + x * x;
            float radius;
            if (get(request, reach_request::uses_target_radius) == 0)
            {
                radius = context.body_actor.radius_xz * body_record(body).radius_scale +
                    mind_target_record(context.target_bytes).shape.radius;
            }
            else
            {
                radius = runtime_player_target_radius(body, context.body_actor, actions.attached,
                    *context.target_actor);
            }
            radius = get(request, reach_request::radius) + radius;
            if (!(squared > radius * radius))
            {
                return 1;
            }
        }
        return 3;
    case 0xC:
        assign_runtime_body_bit(context.flags, 7, get(request, value_request::value));
        return 1;
    case 0x1A:
    {
        actor_set_uniform_scale(context.actor, context.actor.radius_xz * get(request, value_request::scale));
        actor_refresh_transform(context.actor);
        u32 notice[]{0x4A};
        context.actor_message(notice);
        return 1;
    }
    case 0x1B:
    {
        const u16 key = runtime_body_action_name_id(field_data(request, named_request::name), context.action_names);
        mind_record(mind).action_record = mind_record(mind).target_record;
        if (!select_runtime_attachment_request(context.attachment, key))
        {
            mind_record(mind).action_record = 0;
        }
        return 1;
    }
    case 0x1C:
        set_runtime_body_bit(context.flags, runtime_body_flag_name_index(field_data(request, named_request::name), context.flag_names));
        return 1;
    case 0x1D:
    {
        u32 notice[]{0x3B, 0, runtime_player_goal_flag_name_id(field_data(request, named_request::name), context.goal_flag_names)};
        context.actor_message(notice);
        return notice[1] != 0 ? 3 : 1;
    }
    case 0x1E:
        body_record(body).unk_70 = runtime_body_random_range(context.random, get(request, body_range_request::low),
            get(request, body_range_request::high));
        return 1;
    case 0x1F:
        select_runtime_body_action(body, runtime_body_action_name_id(field_data(request, named_request::name),
            context.action_names), actions);
        return 1;
    case 0x20:
        if (!runtime_actor_animation_slot_complete(context.animation, get(request, slot_action_request::slot)))
        {
            return 0;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(field_data(request, slot_action_request::action),
            context.action_names), actions);
        return 1;
    case 0x21:
    {
        const u32 index = runtime_body_flag_name_index(field_data(request, flag_action_request::flag), context.flag_names);
        if (!runtime_body_bit_matches(context.flags, index, get(request, flag_action_request::expected)))
        {
            return get(request, flag_action_request::waits) != 0 ? 0 : 1;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(field_data(request, named_request::name),
            context.action_names), actions);
        return 1;
    }
    case 0x22:
    {
        const u32 index = runtime_body_flag_name_index(field_data(request, flag_action_request::flag), context.flag_names);
        if (!runtime_body_bit_matches(context.flags, index, get(request, flag_action_request::expected)))
        {
            return get(request, flag_action_request::chance_waits) != 0 ? 0 : 1;
        }
        if (!actor_event_chance(context.random, get(request, flag_action_request::chance)))
        {
            return 1;
        }
        select_runtime_body_action(body, runtime_body_action_name_id(field_data(request, named_request::name),
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
        mind_record(mind).action_actor = context.actor_address(find_runtime_player_nearest_spawn(context.actor,
            nullptr, 0x1F));
        return 1;
    case 0x29:
        return get(request, mind_request_field::elapsed) >= get(request, wait_request::duration) ? 1 : 0;
    case 0x2A:
        return runtime_body_bit_matches(context.flags,
            runtime_body_flag_name_index(field_data(request, flag_test_request::flag), context.flag_names),
            get(request, flag_test_request::expected)) ? 1 : 0;
    case 0x2B:
        return script_mind_is_idle(context.body_query, i32(get(request, value_request::value))) ? 1 : 0;
    case 0x2C:
        return runtime_body_timer_is_idle(body) ? 1 : 0;
    default:
        return 1;
    }
}
}
