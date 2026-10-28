#include "body_grabbed_hold_state.h"
#include "body_state_actions.h"
#include "player_goal_cleanup.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
std::function<RuntimeGrabbedHoldStateContext&(u32)> grabbed_hold_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

// The body's +0x309..+0x30C counts move by `step`.
void step_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

bool construct_grabbed_hold_state_view(FactoryObject& object, std::span<const u8>)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_grabbed_hold_state(state, grabbed_hold_context_of(body_address_of(state)));
}

void destroy_grabbed_hold_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_grabbed_hold_state(state, grabbed_hold_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* grabbed_hold_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505920, actor_move_object_sizes, // 0x00095530
        construct_grabbed_hold_state_view, destroy_grabbed_hold_state_view};
    return &table;
}
}

// Xbox 0x000955B0
bool initialize_runtime_grabbed_hold_state(std::span<u8> state, RuntimeGrabbedHoldStateContext& context)
{
    (void)state;
    const auto body = context.body;
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[4] |= 2;
    step_counters(body, 1);
    const u16 count = u16(read_u16(body, 0x30) + 1);
    std::memcpy(body.data() + 0x30, &count, 2);
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    const u32 holds = read_u32(body, 0x258) + 1;
    std::memcpy(body.data() + 0x258, &holds, 4);
    release_runtime_player_attachment(0, 0, context.attachment); // 0x0001B810, EAX body
    return true;
}

// Xbox 0x00095640
void destroy_runtime_grabbed_hold_state(std::span<u8> state, RuntimeGrabbedHoldStateContext& context)
{
    (void)state;
    const auto body = context.body;
    context.bits.bytes[4] &= 0xFD;
    step_counters(body, -1);
    const u16 count = u16(read_u16(body, 0x30) - 1);
    std::memcpy(body.data() + 0x30, &count, 2);
    decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
    const u32 holds = read_u32(body, 0x258) - 1;
    std::memcpy(body.data() + 0x258, &holds, 4);
    if (holds == 0)
    {
        const std::array<u32, 1> release{0x4A};
        context.body_message(release); // Body vtable +0xC.
    }
}

// Xbox 0x000956C0
void tick_runtime_grabbed_hold_state(std::span<u8> state, RuntimeGrabbedHoldStateContext& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    set(state, body_state_field::elapsed, rate * step + get(state, body_state_field::elapsed));
}

void install_runtime_grabbed_hold_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGrabbedHoldStateContext&(u32 body_address)> context_of)
{
    grabbed_hold_context_of = std::move(context_of);
    types[0x71] = ObjectFactoryType{9, grabbed_hold_state_lifecycle}; // Getter 0x00095550.
}
}
