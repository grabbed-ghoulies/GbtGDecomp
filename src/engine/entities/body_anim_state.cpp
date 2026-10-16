#include "body_anim_state.h"
#include "body_orientation.h"
#include "player_mind_events.h"
#include "gbtg/anim.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}
}

// Xbox 0x00088500
void anim_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x40;
    output.parameters = 0xB0;
}

// Xbox 0x00088580
bool initialize_runtime_anim_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeAnimStateContext& context)
{
    const auto body = context.body;
    put_half(state, 0x18, read_u16(parameters, 0xC));
    put_word(state, 0x2C, read_u32(parameters, 0x14));
    put_word(state, 0x30, read_u32(parameters, 0x18));
    put_word(state, 0x34, read_u32(parameters, 0x1C));
    put_word(state, 0x38, read_u32(parameters, 0xAC));
    put_word(state, 0x3C, read_u32(parameters, 0xA4));
    put_word(state, 0x1C, read_u32(parameters, 0x10));
    if (read_f32(parameters, 0x20) <= 0.f) // DAT_0032B100.
    {
        put_word(state, 0x24, 0);
    }
    else
    {
        put_word(state, 0x20, read_u32(parameters, 0x20));
        put_word(state, 0x24, 1);
    }
    if (parameters[0x24] != 0)
    {
        put_word(state, 0x28, 1);
        // Actor +0x614 is sound slot 4; the name is the inline string at parameters +0x24.
        queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
            reinterpret_cast<const char*>(parameters.data() + 0x24), context.sounds);
    }
    const u32 bit = read_u32(state, 0x1C);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    const u32 animation = read_u32(parameters, 0xA8);
    if (animation != 0)
    {
        const char* name = context.string_at(animation);
        if (name == nullptr)
        {
            throw Error("anim state parameters +0xA8 name no animation string");
        }
        select_runtime_body_animation(name, context.actions); // 0x0001AF10
    }
    body[0x38] = 1;
    body[0x39] = 1;
    return true;
}

// Xbox 0x00088660
void destroy_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context)
{
    const u32 bit = read_u32(state, 0x1C);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (read_u32(state, 0x28) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // Actor +0x614.
    }
}

// Xbox 0x00088960
void message_runtime_anim_state(std::span<u8> state, std::span<const u8> message,
    RuntimeAnimStateContext& context)
{
    if (read_u32(message, 0) != 0x25)
    {
        return;
    }
    u16 key = read_u16(state, 0x18);
    if (key == 0)
    {
        key = context.body_slot_key(0x20);
    }
    select_runtime_body_action(context.body, key, context.actions); // 0x00027650, EBX body, AX key
}

// Xbox 0x000886B0
void tick_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    put_float(state, 0x10, runtime_actor_step(actor, context.clock) + read_f32(state, 0x10));
    const u32 target = read_u32(state, 0x2C) == 0 ? 0 : context.mind_target();
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
        put_float(body, 0x72C, 0.f);
        put_float(body, 0x730, read_f32(state, 0x30) + angle);
        put_float(body, 0x734, 0.f);
    }
    else
    {
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
    }
    if (read_u32(state, 0x3C) != 0)
    {
        settle_runtime_body_turn(body, actor);
    }
    if (read_u32(state, 0x38) != 0)
    {
        const Vec3 velocity = actor.anchor; // Actor +0x3C.
        std::memcpy(body.data() + 0x318, &velocity, sizeof(velocity));
    }
    const AnimationController* controller = context.animation.controller.get();
    const MinaPlayer* player = controller == nullptr ? nullptr : controller->slots.at(0).player.get();
    const float position = player == nullptr ? 0.f : player->position;
    bool pushed = false;
    if (position > read_f32(state, 0x34))
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
    if (read_u32(state, 0x24) == 0)
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
        const float left = read_f32(state, 0x20);
        if (left > 0.f)
        {
            const float next = left - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            put_float(state, 0x20, next);
            if (!(0.f < next))
            {
                put_float(state, 0x20, 0.f);
                done = true;
            }
        }
    }
    if (!done && !pushed)
    {
        return;
    }
    const u16 key = read_u16(state, 0x18);
    put_half(state, 0xC, key != 0 ? key : context.body_slot_key(0x20));
}
}
