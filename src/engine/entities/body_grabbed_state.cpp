#include "body_grabbed_state.h"
#include "actor_live_attachments.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "body_state_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace field = grabbed_state_field;
namespace parameter = grabbed_state_parameter;
namespace base = body_state_field;

namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_holds(std::span<u8> body, entity_detail::ListHead& live_attachments, i32 step)
{
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + step));
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
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
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
    set(state, field::move_hold, 1);
    set(state, field::release_mark, get(parameters, parameter::release_mark));
    set(state, field::holds_latches, get(parameters, parameter::holds_latches));
    set(state, field::body_bit, get(parameters, parameter::body_bit));
    if (get(state, field::holds_latches) != 0)
    {
        hold_runtime_body_latches(body, context.hold); // 0x00027BE0
    }
    const u32 bit = get(state, field::body_bit);
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
    if (get(state, field::move_hold) != 0)
    {
        set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
        set(state, field::move_hold, 0);
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
    if (get(state, field::holds_latches) != 0)
    {
        --body[0x7E5];
    }
    const u32 bit = get(state, field::body_bit);
    if (bit != 0)
    {
        pickup.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (context.spheres != nullptr) // Body +0x710.
    {
        RuntimeBodyCollisionSphere* end = context.spheres + body[0x714];
        for (RuntimeBodyCollisionSphere* sphere = context.spheres; sphere < end; ++sphere)
        {
            sphere->contact[0x26] = 0;
        }
    }
    body[0x715] = 0;
    body[0x716] = 0;
}

// Xbox 0x00094530
void tick_runtime_grabbed_state(std::span<u8> state, RuntimeGrabbedStateContext& context)
{
    auto& pickup = context.pickup;
    const std::span<u8> body = pickup.body;
    const float elapsed =
        runtime_actor_rate(pickup.actor) * runtime_frame_step(pickup.clock) + get(state, base::elapsed);
    set(state, base::elapsed, elapsed);
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
    if (crossed(*player, get(state, field::release_mark)) && get(state, field::move_hold) != 0)
    {
        set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
        set(state, field::move_hold, 0);
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
        if (!finished && (!pushed || get(state, field::move_hold) != 0))
        {
            return;
        }
    }
    select_runtime_body_goal_action(state, context.recovery); // 0x00134060
}
}
