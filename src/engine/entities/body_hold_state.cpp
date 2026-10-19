#include "body_hold_state.h"
#include "actor_live_attachments.h"
#include "body_orientation.h"

#include <array>
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

// Body +0x6B, +0x30 (a word) and +0x309..+0x30C move together.
void adjust_hold_counters(std::span<u8> body, i32 step)
{
    body[0x6B] = u8(body[0x6B] + step);
    const u16 held = u16(read_u16(body, 0x30) + step);
    std::memcpy(body.data() + 0x30, &held, 2);
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}
}

// Xbox 0x000A34A0
void hold_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x24;
    output.parameters = 0x14;
}

// Xbox 0x000A3520
bool initialize_runtime_hold_state(std::span<u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    adjust_hold_counters(body, 1);
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits->bytes[6] |= 4;
    put_word(state, 0x18, read_u32(body, 0x240));
    put_word(state, 0x1C, read_u32(state, 0xC));
    const float scale = std::bit_cast<float>(0x3E19999Au); // DAT_0032B168.
    put_float(state, 0x18, (1.f - read_f32(body, 0x16C)) * scale + read_f32(state, 0x18)); // DAT_0032B0F8.
    const u32 bit = read_u32(state, 0x20);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    return true;
}

// Xbox 0x000A35E0
void destroy_runtime_hold_state(std::span<const u8> state, RuntimeBodyStateContext& context)
{
    adjust_hold_counters(context.body, -1);
    context.bits->bytes[6] &= 0xFB;
    const u32 bit = read_u32(state, 0x20);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
}

// Xbox 0x000A3670
void tick_runtime_hold_state(std::span<u8> state, RuntimeHoldStateTick& context)
{
    const auto body = context.body;
    put_float(state, 0x10, runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + read_f32(state, 0x10));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if (read_u32(state, 0x1C) != 0)
    {
        settle_runtime_body_turn(body, context.actor); // 0x00134720, ESI body.
    }
    const float timer = read_f32(state, 0x18);
    if (timer > 0.f)
    {
        const float left = timer - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
        put_float(state, 0x18, left);
        if (0.f >= left)
        {
            put_float(state, 0x18, 0.f);
        }
    }
    if (read_f32(state, 0x18) != 0.f) // DAT_0032B100; unordered also returns.
    {
        return;
    }
    auto& recovery = context.recovery;
    if (read_f32(body, 0x168) == 0.f)
    {
        const auto settings = recovery.settings; // Body +0x268.
        const u32 brake = settings.data() == nullptr ? 1 : (settings[0x1A4] == 0 ? 1 : 0);
        reset_runtime_body_turn(brake, body, recovery.first_timer); // 0x001346B0, ESI body.
    }
    const std::array<u32, 2> notice{0x75, context.actor_address};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(notice.data())); // 0x00122C70, EDI notice.
    select_runtime_body_goal_action(state, recovery); // 0x00134060, EBX body.
}
}
