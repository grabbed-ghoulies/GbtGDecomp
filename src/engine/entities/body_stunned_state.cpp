#include "body_stunned_state.h"
#include "mind_fields.h"
#include "body_walk_alert_tick.h"
#include "../scene/scene_types.h"
#include "../scripts/script_conditions.h"
#include "gbtg/object_factory.h"
#include "body_fields.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = stunned_state_field;
namespace parameter = stunned_state_parameter;

std::function<RuntimeStunnedStateContext&(u32)> stunned_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The four input counters (+0x309 .. +0x30C) and the half at +0x30 move together.
void step_holds(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
    const u16 count = u16(get(body, body_field::move_holds) + step);
    std::memcpy(body.data() + 0x30, &count, 2);
}

bool construct_stunned_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_stunned_state(state, parameters, stunned_context_of(body_address_of(state)));
}

void destroy_stunned_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_stunned_state(state, stunned_context_of(body_address_of(state)));
}

// Getter 0x000A31A0: +0x14 / +0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620.
const ObjectFactoryVtable* stunned_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C00, stunned_state_object_sizes, construct_stunned_state_view,
        destroy_stunned_state_view};
    return &table;
}
}

// Xbox 0x000A3180
void stunned_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A3200
bool initialize_runtime_stunned_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeStunnedStateContext& context)
{
    const auto body = context.body;
    set(state, field::wait, 0.f);
    step_holds(body, 1);
    if (!context.mind.empty())
    {
        std::memset(context.mind.data() + 0xC, 0, 4);
        if (get(context.mind, mind_field::target_record) != 0)
        {
            const std::array<u32, 2> message{0x64, 0};
            context.actor_message(message); // Mind +8 (the actor) vtable +0xC.
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
    set(state, field::shape_mode, set_runtime_body_shape_mode(2, context.shape)); // 0x0001FEB0, BL 2
    if (parameters[parameter::sound.offset] != 0)
    {
        queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
            reinterpret_cast<const char*>(parameters.data() + parameter::sound.offset), context.sounds); // 0x0012A260
    }
    return true;
}

// Xbox 0x000A3300
void destroy_runtime_stunned_state(std::span<u8> state, RuntimeStunnedStateContext& context)
{
    const auto body = context.body;
    if (const u32 held = read_u32(body, 0x50); held != 0)
    {
        std::array<u32, 1> message{0x54};
        context.message(held, message); // Body +0x50's vtable +0xC.
    }
    step_holds(body, -1);
    set_runtime_body_shape_mode(get(state, field::shape_mode), context.shape); // 0x0001FEB0, BL state +0x18
    release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
}

// Xbox 0x000A3380
void tick_runtime_stunned_state(std::span<u8> state, RuntimeStunnedStateContext& context)
{
    const auto body = context.body;
    set(state, body_state_field::elapsed, runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + get(state, body_state_field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    const float step = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    if (0.f < get(state, field::wait))
    {
        const float wait = get(state, field::wait) - step;
        set(state, field::wait, wait);
        if (wait <= 0.f)
        {
            set(state, field::wait, 0.f);
            set(state, body_state_field::next_key, context.action_key()); // Body vtable +0x20.
        }
    }
}

// Xbox 0x000A3460
void message_runtime_stunned_state(std::span<u8> state, std::span<u8> message, RuntimeStunnedStateContext& context)
{
    const u32 id = read_u32(message, 0);
    if (id == 0x43)
    {
        set(state, field::wait, 0.5f); // DAT_0032B0F4
    }
    else if (id == 0x53)
    {
        stop_runtime_walk_alert(context.body, message, 0, context.action_key, context.actions); // 0x00023890, ECX 0
    }
}

void install_runtime_stunned_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeStunnedStateContext&(u32 body_address)> context_of)
{
    stunned_context_of = std::move(context_of);
    types[0x89] = ObjectFactoryType{9, stunned_state_lifecycle}; // Getter 0x000A31A0.
}
}
