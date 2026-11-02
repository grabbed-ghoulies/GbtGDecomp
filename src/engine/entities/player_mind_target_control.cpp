#include "player_mind_target_control.h"
#include "mind_fields.h"
#include "../scripts/script_conditions.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0007DDA0
u32 select_runtime_player_target_action_direction(std::span<u8> mind,
    const RuntimePlayerAttachmentEligibility& body, const Actor& actor,
    const RuntimeMindTarget& target, std::span<const u8> target_body, const Actor* focus)
{
    const bool same_target = read_u32(mind, 0x88) == target.address;
    if (!same_target)
    {
        mind[0x90] = 0;
    }
    if (target.bytes[0x14] == 3)
    {
        return 0xFFFFFFFFu;
    }
    if (focus != nullptr && focus != &target.actor)
    {
        const float distance = runtime_actor_squared_distance(actor, *focus);
        if (std::bit_cast<float>(0x441C4000u) > distance)
        {
            const float angle = runtime_actor_angle_toward(*focus, actor);
            return runtime_body_target_direction(body.body_actor, target.bytes, angle);
        }
        return 0xFFFFFFFFu;
    }
    u32 direction;
    const bool near = runtime_player_body_point_is_near(body, target.bytes);
    const u8 held_kind = body.attached == nullptr ? 0 : body.attached->bytes[0x93D];
    if (!near && held_kind == 0 && read_u32(mind, 0x80) == 0
        && !runtime_mind_target_requires_action(target, target_body))
    {
        direction = 1;
    }
    else
    {
        direction = runtime_body_target_direction(body.body_actor, target.bytes, body.body_actor.euler.y);
    }
    if (focus == &target.actor && same_target && mind[0x90] != 0)
    {
        if (direction == 0 || direction == 2)
        {
            return 1;
        }
        if (direction == 3)
        {
            return runtime_body_target_turn_direction(body.body_actor, target.bytes, nullptr) != 1
                ? 2u : 0u;
        }
    }
    return direction;
}

// Xbox 0x0007E880
RuntimePlayerTargetView* update_runtime_player_target_proposal(RuntimePlayerMindProposal& context,
    u32 enabled, float heading)
{
    LiveScript* event = context.events.anchor.next;
    while (event != nullptr && u16(event->type) != 3)
    {
        event = event->next;
    }
    RuntimePlayerTargetView* current = nullptr;
    if (event != nullptr)
    {
        const u32 address = read_u32(static_cast<RuntimeMindEvent*>(event)->bytes, 0x20);
        current = address == 0 ? nullptr : context.targets.at(address);
    }
    if (enabled != 0)
    {
        RuntimeMindTarget* selected = select_runtime_player_shape_target(context.search,
            context.events.mind, nullptr, current == nullptr ? nullptr : current->searchable, heading);
        RuntimePlayerTargetView* proposal;
        if (selected == nullptr)
        {
            const auto shape = refresh_runtime_player_shape_target(context.shape, heading, 80.f);
            proposal = shape.empty() ? nullptr : &context.embedded;
        }
        else
        {
            proposal = context.targets.at(selected->address);
            const auto* attached = context.search.body.attached;
            if (attached == nullptr || attached->bytes[0x93D] <= 1
                || (attached->bytes[0x93D] == 4 && read_u32(context.events.mind, 0xB0) != 3))
            {
                const auto redirected = redirect_runtime_player_shape_target(context.shape,
                    context.search.body, proposal->bytes, heading);
                proposal = redirected.empty() ? nullptr
                    : redirected.data() == context.embedded.bytes.data() ? &context.embedded : proposal;
            }
        }
        if (proposal != nullptr)
        {
            RuntimeMindEvent* target_event = allocate_runtime_mind_event(context.events, 3, .5f);
            std::memcpy(target_event->bytes.data() + 0x20, &proposal->address, 4);
            return proposal;
        }
    }
    event = context.events.anchor.next;
    while (event != nullptr && u16(event->type) != 3)
    {
        event = event->next;
    }
    if (event != nullptr)
    {
        script_release_event(event);
    }
    return nullptr;
}

// Xbox 0x0007E4B0
void update_runtime_player_target_control(RuntimePlayerMindTargetControl& context,
    u32 enabled, float heading)
{
    auto mind = context.focus.mind;
    if ((context.focus.body.flags.bytes[3] & 8) == 0)
    {
        Actor* focus = update_runtime_player_mind_focus(context.focus);
        RuntimePlayerTargetView* proposal = update_runtime_player_target_proposal(context.proposal,
            enabled, heading);
        if (focus != nullptr && proposal != nullptr && focus != proposal->owner
            && proposal->bytes[0x14] == 0)
        {
            const auto& body = *context.focus.bodies.at(focus);
            if (runtime_player_mind_accepts_target(context.focus.body, body.body, body.body_actor,
                proposal->bytes, context.proposal.events.anchor, context.address_zero))
            {
                context.focus.target = proposal->owner;
                const u32 address = read_u32(proposal->bytes, 0x10);
                set(mind, mind_field::target, address);
            }
        }
        if (read_u32(mind, 0x124) != 0)
        {
            float period = (read_f32(mind, 0x120) * std::bit_cast<float>(0x3CA3D70Au)) * 3.f;
            if (period > 3.f)
            {
                period = 3.f;
            }
            else if (0.f > period)
            {
                period = 0.f;
            }
            const float timer = read_f32(mind, 0x128) - runtime_frame_step(context.clock);
            std::memcpy(mind.data() + 0x128, &timer, 4);
            if (0.f >= timer)
            {
                const float next = timer + period;
                std::memcpy(mind.data() + 0x128, &next, 4);
                notify_runtime_player_mind_event(context.feedback, 0xE);
                const u32 zero = 0;
                std::memcpy(mind.data() + 0x124, &zero, 4);
            }
        }
    }
}
}
