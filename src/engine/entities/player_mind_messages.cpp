#include "player_mind_messages.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_vm.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

Vec3 message_point(std::span<const u32> message, size_t index)
{
    return {std::bit_cast<float>(message[index]), std::bit_cast<float>(message[index + 1]),
        std::bit_cast<float>(message[index + 2])};
}

std::span<const u8> message_bytes(std::span<const u32> message)
{
    return {reinterpret_cast<const u8*>(message.data()), message.size_bytes()};
}

void notify_health_event(RuntimePlayerMindMessages& context)
{
    const u16 health = read_u16(context.feedback.body, 0x94);
    bool warn;
    if (!scene_end_flag_set())
    {
        if (health <= 5)
        {
            notify_runtime_player_mind_event(context.feedback, 10);
            return;
        }
        warn = health <= 10;
    }
    else
    {
        if (health <= 2)
        {
            notify_runtime_player_mind_event(context.feedback, 10);
            return;
        }
        warn = health <= 5;
    }
    if (warn)
    {
        notify_runtime_player_mind_event(context.feedback, 0xB);
    }
}
}

// Xbox 0x000357B0
bool runtime_resource_name_is_aid(const char* name)
{
    if (name == nullptr)
    {
        return false;
    }
    u32 tag;
    std::memcpy(&tag, name, 4);
    return tag == 0x5F646961; // "aid_".
}

// Xbox 0x00049DB0
void clear_runtime_goal_message(std::span<u32, 9> message)
{
    for (size_t index = 1; index < 9; ++index)
    {
        message[index] = 0;
    }
    message[0] = 0x4D;
}

// Xbox 0x0007F1E0
void dispatch_runtime_player_mind_message(RuntimePlayerMindMessages& context, std::span<const u32> message,
    std::span<const u8> message_target)
{
    RuntimePlayerMindFeedback& feedback = context.feedback;
    std::span<u8> mind = feedback.mind;
    switch (message[0])
    {
    case 0x23:
        register_runtime_controlled_player(context.binding);
        break;
    case 0x29:
        reset_runtime_player_mind_body_event(feedback, context.target, context.actor_mind);
        break;
    case 0x2A:
        notify_runtime_player_mind_impact(feedback);
        put(mind, 0x150, read_u32(mind, 0x150) + 1);
        break;
    case 0x2B:
        notify_runtime_player_mind_event(feedback, 6);
        break;
    case 0x2D:
    {
        const Actor* other = context.actor_at(message[2]);
        const std::array<u32, 4> notice{0xB6, context.actor_address, message[2], other->class_key};
        script_queue_event(context.game, {reinterpret_cast<const u8*>(notice.data()), sizeof(notice)});
        break;
    }
    case 0x31:
        notify_runtime_player_mind_attachment_callout(context.requests, context.action_target, message[1]);
        break;
    case 0x34:
        notify_runtime_player_mind_event(feedback, i32(message[1] != 0) + 0xC);
        break;
    case 0x37:
        notify_runtime_player_turn(context.boost, feedback, message_point(message, 1));
        break;
    case 0x3D:
        notify_runtime_player_mind_event(feedback, 7);
        break;
    case 0x3E:
        notify_runtime_player_mind_release(feedback);
        break;
    case 0x40:
        record_runtime_player_mind_distance(mind, std::bit_cast<float>(message[2]));
        break;
    case 0x41:
        track_runtime_player_mind_source(mind, context.body_state, message[0xA], message[5]);
        break;
    case 0x42:
        put(mind, 0x7C, message[1]);
        break;
    case 0x44:
        if (message[2] == context.actor_address)
        {
            notify_runtime_player_mind_activation(feedback);
        }
        break;
    case 0x46:
        notify_runtime_player_mind_event(feedback, 2);
        break;
    case 0x48:
        notify_runtime_player_mind_event(feedback, 9);
        break;
    case 0x49:
    case 0x5A:
        track_runtime_player_mind_source(mind, context.body_state, message[0xB], message[6]);
        break;
    case 0x50:
    {
        const char* name = context.message_name(message[1]);
        if (runtime_resource_name_is_aid(name))
        {
            shake_runtime_player_mind_camera(feedback, name);
        }
        else
        {
            notify_runtime_player_mind_event(feedback, 2);
        }
        break;
    }
    case 0x5B:
        notify_runtime_player_boost(context.boost, feedback);
        break;
    case 0x6E:
        if (message[1] == context.actor_address)
        {
            notify_runtime_player_mind_contact(feedback, message[5], message[4]);
            put(mind, 0x150, read_u32(mind, 0x150) + 1);
            const float limit = 15.f; // DAT_0032B214.
            const float alert = get(mind, mind_field::alert_timer) + 5.f; // DAT_0032B184.
            set(mind, mind_field::alert_timer, alert);
            if (alert > limit)
            {
                set(mind, mind_field::alert_timer, limit);
            }
        }
        else if (message[2] == context.actor_address)
        {
            notify_runtime_player_mind_source(feedback, message_bytes(message));
        }
        break;
    case 0x70:
        if (message[2] == context.actor_address)
        {
            notify_runtime_player_mind_target(feedback, message_bytes(message));
        }
        break;
    case 0x75:
        if (read_u32(mind, 0x88) != 0 && read_u32(*context.event_source, 0x10) == message[1])
        {
            clear_runtime_player_mind_body_event(feedback.body);
        }
        break;
    case 0x7C:
        clear_runtime_player_mind_reference(mind, 0x84, message[1]);
        clear_runtime_player_mind_reference(mind, 0x134, message[1]);
        break;
    case 0x7D:
        initialize_runtime_player_mind_navigation(mind, i32(read_u32(context.actor_bytes, 0xCEC)), context.grids,
            context.nodes, context.storage, context.address_of);
        if (context.chapter_mode == 1)
        {
            std::array<u32, 9> goal;
            clear_runtime_goal_message(goal);
            const Vec3 position = actor_world_position(feedback.actor);
            const Vec3 rotation = actor_world_euler(feedback.actor);
            goal[1] = 0x12;
            goal[2] = std::bit_cast<u32>(position.x);
            goal[3] = std::bit_cast<u32>(position.y);
            goal[4] = std::bit_cast<u32>(position.z);
            goal[5] = std::bit_cast<u32>(rotation.x);
            goal[6] = std::bit_cast<u32>(rotation.y);
            goal[7] = std::bit_cast<u32>(rotation.z);
            context.events.actor_message(goal);
            const float duration = runtime_actor_animation_duration(context.animation, 0);
            start_runtime_camera_pulse(duration, 10.f, 0.f, 0.f, 1.f, 0, context.runtime_flags, context.pulse);
        }
        break;
    case 0x86:
        if (message[1] == context.actor_address)
        {
            notify_runtime_player_mind_event(feedback, i32(runtime_player_mind_uses_special_event(message[2])) + 0x10);
        }
        break;
    case 0x8D:
        if (read_u32(mind, 0x88) == message[1])
        {
            put(mind, 0x88, 0);
            clear_runtime_player_mind_body_event(feedback.body);
        }
        break;
    case 0x90:
        if (runtime_player_can_resume_strategy(context.actor_bytes, context.resume) != 0
            && runtime_player_mind_active(context.actor_mind) != 0)
        {
            notify_health_event(context);
        }
        break;
    case 0x94:
        record_runtime_player_mind_position(mind, feedback.actor, message[1], message_point(message, 2));
        break;
    case 0xB2:
        if (message[2] != 0)
        {
            assign_runtime_body_bit(context.flags, 0xE, message[1] == 0 ? 1 : 0);
        }
        break;
    default:
        break;
    }
    dispatch_runtime_parent_mind_message(context.events, message, message_target);
}
}
