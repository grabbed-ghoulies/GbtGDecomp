#include "body_anim_state.h"
#include "body_orientation.h"
#include "player_mind_events.h"
#include "body_fields.h"
#include "body_state_fields.h"
#include "gbtg/anim.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace parameter = anim_state_parameter;

namespace
{
}

// Xbox 0x00088500
void anim_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(AnimState);
    output.parameters = parameter::kSize;
}

// Xbox 0x00088580
bool initialize_runtime_anim_state(AnimState& state, std::span<const u8> parameters,
    RuntimeAnimStateContext& context)
{
    const auto body = context.body;
    state.next_key = get(parameters, parameter::next_key);
    state.faces_target = get(parameters, parameter::faces_target);
    state.heading_offset = get(parameters, parameter::heading_offset);
    state.stick_point = get(parameters, parameter::stick_point);
    state.carries_velocity = get(parameters, parameter::carries_velocity);
    state.settles = get(parameters, parameter::settles);
    state.body_bit = get(parameters, parameter::body_bit);
    if (get(parameters, parameter::time) <= 0.f) // DAT_0032B100.
    {
        state.timed = 0;
    }
    else
    {
        state.time_left = get(parameters, parameter::time);
        state.timed = 1;
    }
    const char* sound = field_data(parameters, parameter::sound);
    if (sound[0] != 0)
    {
        state.sound_started = 1;
        // Actor +0x614 is sound slot 4.
        queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f, sound,
            context.sounds);
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    const u32 animation = get(parameters, parameter::animation);
    if (animation != 0)
    {
        const char* name = context.string_at(animation);
        if (name == nullptr)
        {
            throw Error("anim state parameters name no animation string");
        }
        select_runtime_body_animation(name, context.actions); // 0x0001AF10
    }
    body[0x38] = 1;
    body[0x39] = 1;
    return true;
}

// Xbox 0x00088660
void destroy_runtime_anim_state(AnimState& state, RuntimeAnimStateContext& context)
{
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (state.sound_started != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // Actor +0x614.
    }
}

// Xbox 0x00088960
void message_runtime_anim_state(AnimState& state, std::span<const u8> message,
    RuntimeAnimStateContext& context)
{
    if (read_u32(message, 0) != 0x25)
    {
        return;
    }
    u16 key = state.next_key;
    if (key == 0)
    {
        key = context.body_slot_key(0x20);
    }
    select_runtime_body_action(context.body, key, context.actions); // 0x00027650, EBX body, AX key
}

// Xbox 0x000886B0
void tick_runtime_anim_state(AnimState& state, RuntimeAnimStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    state.base.elapsed = runtime_actor_step(actor, context.clock) + state.base.elapsed;
    const u32 target = state.faces_target == 0 ? 0 : context.mind_target();
    if (target != 0)
    {
        const Actor* other = context.actor_at(target);
        if (other == nullptr)
        {
            throw Error("anim state mind target " + std::to_string(target) + " is not a scene actor");
        }
        const Vec3 to = actor_world_position(*other);
        const Vec3 from = actor_world_position(actor);
        const Vec3 direction{to.x - from.x, to.y - from.y, to.z - from.z};
        float angle = 0.f;
        runtime_direction_angle(direction, angle);
        set(body, body_field::euler, Vec3{0.f, state.heading_offset + angle, 0.f});
    }
    else
    {
        level_body_euler(body);
    }
    if (state.settles != 0)
    {
        settle_runtime_body_turn(body, actor);
    }
    if (state.carries_velocity != 0)
    {
        set(body, body_field::velocity, actor.anchor); // Actor +0x3C.
    }
    const AnimationController* controller = context.animation.controller.get();
    const MinaPlayer* player = controller == nullptr ? nullptr : controller->slots.at(0).player.get();
    const float position = player == nullptr ? 0.f : player->position;
    bool pushed = false;
    if (position > state.stick_point)
    {
        if (context.input == nullptr || context.input_state == nullptr)
        {
            throw Error("anim state stick check without the tick's input device");
        }
        std::array<float, 2> axes{};
        runtime_input_axes(*context.input, axes, *context.input_state); // 0x001085F0
        pushed = axes[1] * axes[1] + axes[0] * axes[0] > context.stick_threshold;
    }
    bool done = false;
    if (state.timed == 0)
    {
        if (controller == nullptr)
        {
            done = true;
        }
        else if (player == nullptr)
        {
            done = true;
        }
        else if (player->mode == 1)
        {
            if (0.f > player->speed)
            {
                done = 0.f >= player->position;
            }
            else
            {
                done = player->position >= 1.f; // DAT_0032B0F8.
            }
        }
    }
    else
    {
        const float left = state.time_left;
        if (left > 0.f)
        {
            const float next = left - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            state.time_left = next;
            if (!(0.f < next))
            {
                state.time_left = 0.f;
                done = true;
            }
        }
    }
    if (!done && !pushed)
    {
        return;
    }
    const u16 key = state.next_key;
    state.base.next_key = key != 0 ? key : context.body_slot_key(0x20);
}
}
