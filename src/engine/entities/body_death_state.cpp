#include "body_death_state.h"
#include "actor_live_attachments.h"
#include "body_damage_notices.h"
#include "body_death_effects.h"
#include "player_goal_cleanup.h"
#include "../scene/scene_types.h"
#include "../scripts/script_conditions.h"
#include "gbtg/object_factory.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = death_state_field;
namespace parameter = death_state_parameter;

std::function<RuntimeDeathStateContext&(u32)> death_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

void step_half(std::span<u8> body, size_t offset, i32 step)
{
    const u16 value = u16(read_u16(body, offset) + step);
    std::memcpy(body.data() + offset, &value, 2);
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

bool construct_death_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_death_state(state, parameters, death_context_of(body_address_of(state)));
}

void destroy_death_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_death_state(state, death_context_of(body_address_of(state)));
}

// Getter 0x0008C5A0: +0x14 / +0x18 / +0xC are the empty 0x001628B0 and +0x1C is 0x0002B620.
const ObjectFactoryVtable* death_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505740, death_state_object_sizes, construct_death_state_view,
        destroy_death_state_view};
    return &table;
}
}

// Xbox 0x00076BC0
void death_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008C600
bool initialize_runtime_death_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDeathStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const auto body = knockdown.body;
    knockdown.bits.bytes[3] |= 8;
    step_counters(body, 1);
    step_half(body, 0x30, 1);
    body[0x92] = 1;
    const float hold = 0.09f; // DAT_0032B2D8.
    body[0xF8] = 5;
    put_float(body, 0xF0, hold);
    body[0xEC] = 5;
    put_float(body, 0xE4, hold);
    body[0x38] = 1;
    body[0x39] = 1;
    increment_runtime_body_latches(body, knockdown.actor, context.latches); // 0x0001FC40, ECX body
    body[0x715] = 1;
    if (get(parameters, parameter::release_attachments) != 0)
    {
        // Actor +0x8F8: each live attachment is released (0x0012B0B0, ESI the record).
        auto* record = static_cast<entity_detail::ListHead*>(knockdown.live_attachments.next);
        while (record != nullptr)
        {
            auto* next = static_cast<entity_detail::ListHead*>(record->next);
            release_actor_live_attachment(*reinterpret_cast<ActorLiveAttachment*>(record));
            record = next;
        }
    }
    if (!context.mind.empty())
    {
        std::memset(context.mind.data() + 0xC, 0, 4);
        if (read_u32(context.mind, 0x10) != 0)
        {
            const std::array<u32, 2> message{0x64, 0};
            knockdown.actor_message(message); // Mind +8 (the actor) vtable +0xC.
        }
        std::memset(context.mind.data() + 0x10, 0, 8);
        for (LiveScript* event = context.mind_events == nullptr ? nullptr : context.mind_events->next; event != nullptr;
             event = event->next)
        {
            if (u16(event->type) == 3)
            {
                script_release_event(event); // 0x000E9210
                break;
            }
        }
    }
    set(state, field::waiting, 0u);
    set(state, field::shape_mode,
        set_runtime_body_shape_mode(get(parameters, parameter::shape_mode), knockdown.shape)); // 0x0001FEB0, BL
    notify_runtime_body_death_once(body, knockdown.actor_address, knockdown.game); // 0x00024910, ESI body
    return true;
}

// Xbox 0x0008C750
void destroy_runtime_death_state(std::span<u8> state, RuntimeDeathStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const auto body = knockdown.body;
    step_half(body, 0x30, -1);
    step_counters(body, -1);
    set_runtime_body_shape_mode(get(state, field::shape_mode), knockdown.shape); // 0x0001FEB0, BL state +0x20
    if (knockdown.spheres != nullptr) // Body +0x710.
    {
        RuntimeBodyCollisionSphere* end = knockdown.spheres + body[0x714];
        for (RuntimeBodyCollisionSphere* sphere = knockdown.spheres; sphere < end; ++sphere)
        {
            sphere->contact[0x26] = 0;
        }
    }
    body[0x715] = 0;
    body[0x716] = 0;
    decrement_runtime_body_latches(body, knockdown.actor, context.latches); // The tail jump to 0x0001FD20.
}

// Xbox 0x0008C7E0
void tick_runtime_death_state(std::span<u8> state, RuntimeDeathStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const auto body = knockdown.body;
    set(state, body_state_field::elapsed, runtime_actor_rate(knockdown.actor) * runtime_frame_step(knockdown.clock)
        + get(state, body_state_field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    switch (get(state, field::waiting))
    {
    case 0:
    {
        const MinaPlayer* player = first_player(knockdown.animation); // Actor +0x50 -> +0x58 -> [0].
        if (player != nullptr)
        {
            bool finished = false;
            if (player->mode == 1)
            {
                bool running;
                if (0.f > player->speed)
                {
                    running = 0.f < player->position;
                }
                else
                {
                    running = player->position < 1.f; // DAT_0032B0F8.
                }
                finished = !running;
            }
            if (!finished)
            {
                return;
            }
        }
        set(state, field::waiting, 1u);
        set(state, field::delay, 1.f); // DAT_0032B0F8.
        return;
    }
    case 1:
    {
        const float delay = get(state, field::delay);
        if (!(delay > 0.f))
        {
            return;
        }
        const float left = delay - runtime_actor_rate(knockdown.actor) * runtime_frame_step(knockdown.clock);
        set(state, field::delay, left);
        if (0.f < left)
        {
            return;
        }
        set(state, field::delay, 0.f);
        finish_runtime_body_death(1, knockdown.scene_flags, knockdown.restart_requested,
            knockdown.death); // 0x00134AE0, EAX actor
        return;
    }
    default:
        return;
    }
}

void install_runtime_death_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDeathStateContext&(u32 body_address)> context_of)
{
    death_context_of = std::move(context_of);
    types[0x63] = ObjectFactoryType{9, death_state_lifecycle}; // Getter 0x0008C5A0.
}
}
