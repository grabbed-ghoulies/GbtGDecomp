#include "body_grabbed_state.h"
#include "actor_live_attachments.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_holds(std::span<u8> body, entity_detail::ListHead& live_attachments, i32 step)
{
    const u16 count = u16(read_u16(body, 0x30) + step);
    std::memcpy(body.data() + 0x30, &count, 2);
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
    body[0x6D] = u8(body[0x6D] + step);
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

// The inlined test: slot 0 passed `mark` since the last step (+0x10C the previous position).
bool crossed(const MinaPlayer& player, float mark)
{
    if (player.position == player.previous)
    {
        return false;
    }
    float from = player.previous;
    float to = player.position;
    if (0.f > player.speed)
    {
        from = player.position;
        to = player.previous;
    }
    if (to > from)
    {
        return mark >= from && to > mark;
    }
    return mark >= from || to > mark;
}
}

// Xbox 0x000942D0
void grabbed_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x28;
    output.parameters = 0x18;
}

// Xbox 0x00094350
bool initialize_runtime_grabbed_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeGrabbedStateContext& context)
{
    auto& pickup = context.pickup;
    const std::span<u8> body = pickup.body;
    step_holds(body, pickup.live_attachments, 1);
    body[0x38] = 1;
    body[0x39] = 1;
    pickup.bits.bytes[4] |= 0x20;
    pickup.bits.bytes[3] |= 0x80;
    put_word(state, 0x1C, 1);
    put_word(state, 0x18, word(parameters, 0xC));
    put_word(state, 0x20, word(parameters, 0x10));
    put_word(state, 0x24, word(parameters, 0x14));
    if (word(state, 0x20) != 0)
    {
        hold_runtime_body_latches(body, context.hold); // 0x00027BE0
    }
    const u32 bit = word(state, 0x24);
    if (bit != 0)
    {
        pickup.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    body[0x715] = 1;
    const std::array<u32, 1> message{0x39};
    pickup.actor_message(message); // Actor vtable +0xC.
    return true;
}

// Xbox 0x00094450
void destroy_runtime_grabbed_state(std::span<u8> state, RuntimeGrabbedStateContext& context)
{
    auto& pickup = context.pickup;
    const std::span<u8> body = pickup.body;
    if (word(state, 0x1C) != 0)
    {
        const u16 count = u16(read_u16(body, 0x30) - 1);
        std::memcpy(body.data() + 0x30, &count, 2);
        put_word(state, 0x1C, 0);
    }
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        --body[offset];
    }
    for (auto* live = static_cast<ActorLiveAttachment*>(pickup.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold - 1);
    }
    --body[0x6D];
    pickup.bits.bytes[4] &= 0xDF;
    pickup.bits.bytes[3] &= 0x7F;
    if (word(state, 0x20) != 0)
    {
        --body[0x7E5];
    }
    const u32 bit = word(state, 0x24);
    if (bit != 0)
    {
        pickup.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (word(body, 0x710) != 0)
    {
        throw Error("grabbed destructor 0x00094450 reads body +0x710 collision spheres without a native view");
    }
    body[0x715] = 0;
    body[0x716] = 0;
}

// Xbox 0x00094530
void tick_runtime_grabbed_state(std::span<u8> state, RuntimeGrabbedStateContext& context)
{
    auto& pickup = context.pickup;
    const std::span<u8> body = pickup.body;
    const float elapsed = runtime_actor_rate(pickup.actor) * runtime_frame_step(pickup.clock) + read_f32(state, 0x10);
    put_float(state, 0x10, elapsed);
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    const MinaPlayer* player = first_player(pickup.animation);
    if (player != nullptr && crossed(*player, std::bit_cast<float>(0x3F19999Au))) // DAT_0032B248, 0.6
    {
        pickup.bits.bytes[4] &= 0xDF;
    }
    if (pickup.input == nullptr || pickup.input_state == nullptr)
    {
        throw Error("grabbed state stick check without the tick's input device");
    }
    std::array<float, 2> axes{};
    runtime_input_axes(*pickup.input, axes, *pickup.input_state); // 0x001085F0
    bool pushed = axes[1] * axes[1] + axes[0] * axes[0] > pickup.stick_threshold; // DAT_0032B22C
    if (!pushed)
    {
        runtime_input_secondary_axes(*pickup.input, axes, *pickup.input_state); // 0x001088D0
        pushed = axes[1] * axes[1] + axes[0] * axes[0] > pickup.stick_threshold;
    }
    if (player == nullptr)
    {
        select_runtime_body_goal_action(state, context.recovery); // 0x00134060
        return;
    }
    if (crossed(*player, read_f32(state, 0x18)) && word(state, 0x1C) != 0)
    {
        const u16 count = u16(read_u16(body, 0x30) - 1);
        std::memcpy(body.data() + 0x30, &count, 2);
        put_word(state, 0x1C, 0);
    }
    player = first_player(pickup.animation);
    if (player != nullptr)
    {
        bool finished = false;
        if (player->mode == 1)
        {
            const bool running = 0.f > player->speed ? 0.f < player->position : player->position < 1.f;
            finished = !running;
        }
        if (!finished && (!pushed || word(state, 0x1C) != 0))
        {
            return;
        }
    }
    select_runtime_body_goal_action(state, context.recovery); // 0x00134060
}
}
