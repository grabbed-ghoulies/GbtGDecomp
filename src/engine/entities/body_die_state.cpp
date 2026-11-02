#include "body_die_state.h"
#include "body_orientation.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "body_state_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace field = die_state_field;
namespace parameter = die_state_parameter;
namespace base = body_state_field;

namespace
{
}

// Xbox 0x0008C1C0
void die_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008C240
bool initialize_runtime_die_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDieStateContext& context)
{
    const auto body = context.body;
    set(state, field::body_bit, get(parameters, parameter::body_bit));
    set(state, field::moving_key, get(parameters, parameter::moving_key));
    set(state, field::faces_view, get(parameters, parameter::faces_view));
    set(state, field::settles, get(parameters, parameter::settles));
    set(state, field::target, context.mind_target());
    const char* name = context.string_at(get(parameters, parameter::animation));
    if (name == nullptr)
    {
        throw Error("die state parameters name no animation string");
    }
    char* output = field_data(state, field::animation);
    char character;
    do
    {
        character = *name++;
        *output++ = character;
    } while (character != '\0');
    body[0x38] = 1;
    body[0x39] = 1;
    const u32 bit = get(state, field::body_bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    start_runtime_die_animation(state, context);
    return true;
}

// Xbox 0x0008C4F0
void start_runtime_die_animation(std::span<u8> state, RuntimeDieStateContext& context)
{
    const AnimationRecord* record = select_runtime_body_animation(
        field_data(state, field::animation), context.actions);
    if (record == nullptr)
    {
        return;
    }
    AnimationController* controller = context.animation.controller.get();
    float duration = 0.f;
    if (controller != nullptr)
    {
        const MinaPlayer* player = controller->slots.at(record->slot).player.get();
        if (player != nullptr)
        {
            duration = player->duration;
        }
    }
    context.random = context.random * 0x19660D + 0x3C6EF35F;
    const float random = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000);
    if (controller == nullptr)
    {
        return;
    }
    const float time = ((random - 1.f) * std::bit_cast<float>(0x3E4CCCD0u) // DAT_0032B0F8, DAT_0032B5D4.
        + 0.9f) * duration; // DAT_0032B174.
    retime_mina_playback(controller->slots.at(record->slot).player.get(), time, controller->dirty); // 0x00030DB0
}

// Xbox 0x0008C2D0
void destroy_runtime_die_state(std::span<u8> state, RuntimeDieStateContext& context)
{
    const u32 bit = get(state, field::body_bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
}

// Xbox 0x0008C4D0
void message_runtime_die_state(std::span<u8> state, std::span<const u8> message)
{
    if (read_u32(message, 0) == 0x7C && read_u32(message, 4) == get(state, field::target))
    {
        set(state, field::target, 0);
    }
}

// Xbox 0x0008C310
void tick_runtime_die_state(std::span<u8> state, RuntimeDieStateContext& context)
{
    const auto body = context.body;
    const Actor& actor = context.actor;
    set(state, base::elapsed, runtime_actor_step(actor, context.clock) + get(state, base::elapsed));
    if (get(state, field::moving_key) != 0)
    {
        const float x = get(body, body_field::motion).x;
        const float y = get(body, body_field::motion).y;
        const float z = get(body, body_field::motion).z;
        u16 key = 0;
        if (z * z + y * y + x * x > 0.f && (context.bits.bytes[0] & 2) != 0)
        {
            key = context.body_slot_key(0x40);
        }
        set(state, base::next_key, key);
    }
    if (get(state, field::faces_view) != 0)
    {
        const Vec3 point = context.view_position;
        face_runtime_body_point(body, actor, point);
    }
    else
    {
        const u32 target = get(state, field::target);
        if (target == 0)
        {
            level_body_euler(body);
        }
        else if (get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f)
        {
            const Actor* other = context.actor_at(target);
            if (other == nullptr)
            {
                throw Error("die state target " + std::to_string(target) + " is not a scene actor");
            }
            face_runtime_body_point(body, actor, actor_world_position(*other));
        }
    }
    if (get(state, field::settles) != 0)
    {
        settle_runtime_body_turn(body, actor);
    }
    const AnimationController* controller = context.animation.controller.get();
    if (controller != nullptr)
    {
        const MinaPlayer* player = controller->slots.at(0).player.get();
        if (player != nullptr)
        {
            bool finished = false;
            if (player->mode == 1)
            {
                if (0.f > player->speed)
                {
                    finished = 0.f >= player->position;
                }
                else
                {
                    finished = player->position >= 1.f; // DAT_0032B0F8.
                }
            }
            if (!finished)
            {
                return;
            }
        }
    }
    set(state, base::next_key, context.body_slot_key(0x20));
}
}
