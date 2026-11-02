#include "body_die_state.h"
#include "body_orientation.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <bit>
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

// Xbox 0x0008C1C0
void die_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x6C;
    output.parameters = 0x20;
}

// Xbox 0x0008C240
bool initialize_runtime_die_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDieStateContext& context)
{
    const auto body = context.body;
    put_word(state, 0x58, read_u32(parameters, 0xC));
    put_word(state, 0x5C, read_u32(parameters, 0x14));
    put_word(state, 0x60, read_u32(parameters, 0x18));
    put_word(state, 0x64, read_u32(parameters, 0x1C));
    put_word(state, 0x68, context.mind_target());
    const char* name = context.string_at(read_u32(parameters, 0x10));
    if (name == nullptr)
    {
        throw Error("die state parameters +0x10 names no animation string");
    }
    char* output = reinterpret_cast<char*>(state.data() + 0x18);
    char character;
    do
    {
        character = *name++;
        *output++ = character;
    } while (character != '\0');
    body[0x38] = 1;
    body[0x39] = 1;
    const u32 bit = read_u32(state, 0x58);
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
        reinterpret_cast<const char*>(state.data() + 0x18), context.actions);
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
    const u32 bit = read_u32(state, 0x58);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
}

// Xbox 0x0008C4D0
void message_runtime_die_state(std::span<u8> state, std::span<const u8> message)
{
    if (read_u32(message, 0) == 0x7C && read_u32(message, 4) == read_u32(state, 0x68))
    {
        put_word(state, 0x68, 0);
    }
}

// Xbox 0x0008C310
void tick_runtime_die_state(std::span<u8> state, RuntimeDieStateContext& context)
{
    const auto body = context.body;
    const Actor& actor = context.actor;
    put_float(state, 0x10, runtime_actor_step(actor, context.clock) + read_f32(state, 0x10));
    if (read_u32(state, 0x5C) != 0)
    {
        const float x = read_f32(body, 0x248);
        const float y = read_f32(body, 0x24C);
        const float z = read_f32(body, 0x250);
        u16 key = 0;
        if (z * z + y * y + x * x > 0.f && (context.bits.bytes[0] & 2) != 0)
        {
            key = context.body_slot_key(0x40);
        }
        put_half(state, 0xC, key);
    }
    if (read_u32(state, 0x60) != 0)
    {
        const Vec3 point = context.view_position;
        face_runtime_body_point(body, actor, point);
    }
    else
    {
        const u32 target = read_u32(state, 0x68);
        if (target == 0)
        {
            put_float(body, 0x734, 0.f);
            put_float(body, 0x72C, 0.f);
        }
        else if (get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f)
        {
            const Actor* other = context.actor_at(target);
            if (other == nullptr)
            {
                throw Error("die state +0x68 target " + std::to_string(target) + " is not a scene actor");
            }
            face_runtime_body_point(body, actor, actor_world_position(*other));
        }
    }
    if (read_u32(state, 0x64) != 0)
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
    put_half(state, 0xC, context.body_slot_key(0x20));
}
}
