#include "body_state_actions.h"
#include "body_message_helpers.h"
#include "body_move_actions.h"
#include "actor_world_lists.h"
#include "gbtg/entity.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
std::function<RuntimeBodyStateContext(u32)> body_context_of;

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// Actor +0x50 controller: the first slot's player pause count (+0x58 -> +0 -> +0x125) and dirty flag (+0x5C).
void adjust_animation_pause(AnimationController* controller, i32 step)
{
    if (controller != nullptr)
    {
        if (controller->slots.at(0).player != nullptr)
        {
            controller->slots[0].player->paused = u8(controller->slots[0].player->paused + step);
        }
        controller->dirty = true;
    }
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Xbox 0x001628B0
void destroy_empty_state(FactoryObject&)
{
}

bool construct_stand_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_stand_alert(state, parameters, context);
}

void destroy_stand_alert_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_stand_alert(state, context);
}

const ObjectFactoryVtable* actor_move_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD72C, actor_move_object_sizes, initialize_runtime_weapon_action,
        destroy_empty_state};
    return &table;
}

const ObjectFactoryVtable* stand_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505BC0, stand_alert_object_sizes, construct_stand_alert_view,
        destroy_stand_alert_view};
    return &table;
}
}

// Xbox 0x0001FEB0
u8 set_runtime_body_shape_mode(u8 mode, RuntimeBodyStateContext& context)
{
    Actor& actor = context.actor;
    switch (mode)
    {
    case 0:
    case 1:
        actor.model_mode = mode == 0 ? 0 : 4;
        register_actor_shape_probe(actor, context.shape);
        actor.shape_mode = context.body[0x245];
        break;
    case 2:
    case 3:
    case 4:
        actor.model_mode = mode == 4 ? 4 : 0x20002;
        register_actor_shape_probe(actor, context.shape);
        actor.shape_mode = 4;
        break;
    default:
        break;
    }
    const u8 previous = context.body[0x244];
    context.body[0x244] = mode;
    return previous;
}

// Xbox 0x00095530
void actor_move_object_sizes(ObjectSizes& output)
{
    output.instance = 0x18;
    output.parameters = 0xC;
}

// Xbox 0x000A2510
void stand_alert_object_sizes(ObjectSizes& output)
{
    output.instance = 0x50;
    output.parameters = 0xBC;
}

// Xbox 0x000A2590
bool initialize_runtime_stand_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    std::memcpy(state.data() + 0x18, parameters.data() + 0xC, 2);
    std::memcpy(state.data() + 0x1A, parameters.data() + 0xE, 2);
    put_word(state, 0x1C, word(parameters, 0x10));
    put_word(state, 0x20, word(parameters, 0x14));
    put_word(state, 0x24, word(parameters, 0x18));
    put_word(state, 0x28, word(parameters, 0x1C));
    put_word(state, 0x48, word(parameters, 0xB4));
    put_word(state, 0x2C, word(parameters, 0x20));
    put_word(state, 0x30, word(parameters, 0x24));
    put_word(state, 0x34, word(parameters, 0x28));
    put_word(state, 0x38, word(parameters, 0x30));
    put_word(state, 0x3C, word(parameters, 0x2C));
    put_word(state, 0x40, u32(context.secondary_mode));
    context.bits->bytes[6] |= 1;
    body[0x38] = 1;
    body[0x39] = 1;
    std::memset(body.data() + 0x160, 0, 2);
    if (word(state, 0x24) != 0)
    {
        adjust_animation_pause(context.animation, 1);
    }
    if (word(state, 0x20) != 0)
    {
        adjust_body_counters(body, 1);
    }
    if (parameters[0x34] != 0)
    {
        if (parameters[0x34] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
                reinterpret_cast<const char*>(parameters.data() + 0x34), context.sounds);
        }
        put_word(state, 0x44, 1);
    }
    const u32 bit = word(state, 0x48);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    if (word(state, 0x2C) != 0xFFFFFFFF)
    {
        const u8 slot = state[0x2C];
        if (context.actor.model != nullptr && context.actor.model->runtime != nullptr)
        {
            set_runtime_model_node_state(*context.actor.model->runtime, slot, 1, context.node_states);
        }
        if (context.secondary != nullptr)
        {
            set_runtime_model_node_state(*context.secondary, slot, 1, context.node_states);
        }
    }
    if (word(parameters, 0xB8) == 0)
    {
        put_word(state, 0x4C, body[0x244]);
    }
    else
    {
        put_word(state, 0x4C, set_runtime_body_shape_mode(parameters[0xB8], context));
    }
    if (word(state, 0x28) != 0)
    {
        put_word(body, 0x258, word(body, 0x258) + 1);
    }
    if (word(state, 0x3C) != 0 && context.secondary != nullptr)
    {
        context.secondary_mode = 0;
    }
    return true;
}

// Xbox 0x000A2790
void destroy_runtime_stand_alert(std::span<u8> state, RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    context.bits->bytes[6] &= 0xFE;
    if (word(state, 0x20) != 0)
    {
        adjust_body_counters(body, -1);
    }
    if (word(state, 0x24) != 0)
    {
        adjust_animation_pause(context.animation, -1);
    }
    if (word(state, 0x44) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds);
    }
    const u32 bit = word(state, 0x48);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (word(state, 0x2C) != 0xFFFFFFFF && word(state, 0x30) == 0)
    {
        const u8 slot = state[0x2C];
        if (context.actor.model != nullptr && context.actor.model->runtime != nullptr)
        {
            set_runtime_model_node_state(*context.actor.model->runtime, slot, 0, context.node_states);
        }
        if (context.secondary != nullptr)
        {
            set_runtime_model_node_state(*context.secondary, slot, 0, context.node_states);
        }
    }
    set_runtime_body_shape_mode(state[0x4C], context);
    if (word(state, 0x28) != 0)
    {
        const u32 count = word(body, 0x258) - 1;
        put_word(body, 0x258, count);
        if (count == 0)
        {
            const u32 message[]{0x4A};
            context.body_message(message);
        }
    }
    if (word(state, 0x34) != 0 && context.attached != nullptr)
    {
        auto* attached = context.attached;
        detach_runtime_body_secondary(body, context.attached, context.receiver);
        const u32 message[]{0x97, 0};
        attached->message(message);
    }
    if (word(state, 0x3C) != 0 && context.secondary != nullptr)
    {
        context.secondary_mode = i32(word(state, 0x40));
    }
}

void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of)
{
    body_context_of = std::move(context_of);
    types[9] = ObjectFactoryType{1, actor_move_lifecycle};
    types[0x87] = ObjectFactoryType{9, stand_alert_lifecycle};
}
}
