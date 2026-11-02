#include "body_state_actions.h"
#include "body_message_helpers.h"
#include "body_move_actions.h"
#include "actor_world_lists.h"
#include "body_hit_state.h"
#include "body_dizzy_state.h"
#include "body_knockdown_state.h"
#include "body_die_state.h"
#include "body_anim_state.h"
#include "body_pickup_state.h"
#include "body_struggle_state.h"
#include "body_player_death_state.h"
#include "body_grabbed_state.h"
#include "body_walk_target_state.h"
#include "body_hold_state.h"
#include "body_throw_state.h"
#include "gbtg/entity.h"
#include "body_fields.h"
#include <bit>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gbtg
{
namespace
{
std::function<RuntimeBodyStateContext(u32)> body_context_of;
std::function<RuntimeHitStateLifecycle(u32)> hit_context_of;
std::function<RuntimeDizzyStateContext&(u32)> dizzy_context_of;
std::function<RuntimeKnockdownContext&(u32)> knockdown_context_of;
std::function<RuntimeDieStateContext&(u32)> die_context_of;
std::function<RuntimePlayerDeathContext&(u32)> player_death_context_of;
std::function<RuntimeGrabbedStateContext&(u32)> grabbed_context_of;
std::function<RuntimeThrowStateContext&(u32)> throw_context_of;
std::function<RuntimeAnimStateContext&(u32)> anim_context_of;
std::function<RuntimePickupStateContext&(u32)> pickup_context_of;
// Walk-alert speed-band records by state, the host side of state +0x30 + 0x10 * index.
std::unordered_map<const u8*, std::vector<const AnimationRecord*>> walk_alert_records;
// The fly state's looping animation record (+0x74), by the state's bytes.
std::unordered_map<const u8*, const AnimationRecord*> fly_loop_records;

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

bool construct_walk_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_walk_alert(state, parameters, context);
}

void destroy_walk_alert_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_walk_alert(state, context);
}

bool construct_fly_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_fly_state(state, parameters, context);
}

void destroy_fly_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_fly_state(state, context);
}

// Body +0x15C: the bit block whose +8 bytes the furnace state sets.
RuntimeBodyBitStorage& bits_of(const RuntimeBodyStateContext& context)
{
    if (context.bits == nullptr)
    {
        throw Error("body state has no body +0x15C bit block");
    }
    return *context.bits;
}

bool construct_hit_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = hit_context_of(word(state, 8));
    return initialize_runtime_hit_state(state, parameters, context);
}

void destroy_hit_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = hit_context_of(word(state, 8));
    destroy_runtime_hit_state(state, context);
}

const ObjectFactoryVtable* hit_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505940, hit_state_object_sizes, construct_hit_state_view,
        destroy_hit_state_view};
    return &table;
}

bool construct_hit_furnace_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_hit_furnace_state(state, parameters, context.body, bits_of(context));
}

void destroy_hit_furnace_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_hit_furnace_state(state, context.body, bits_of(context));
}

const ObjectFactoryVtable* hit_furnace_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505960, hit_furnace_state_object_sizes,
        construct_hit_furnace_state_view, destroy_hit_furnace_state_view};
    return &table;
}

bool construct_dizzy_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_dizzy_state(state, parameters, dizzy_context_of(word(state, 8)));
}

void destroy_dizzy_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_dizzy_state(state, dizzy_context_of(word(state, 8)));
}

const ObjectFactoryVtable* dizzy_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505760, dizzy_state_object_sizes, construct_dizzy_state_view,
        destroy_dizzy_state_view};
    return &table;
}

bool construct_anim_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_anim_state(state, parameters, anim_context_of(word(state, 8)));
}

void destroy_anim_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_anim_state(state, anim_context_of(word(state, 8)));
}

const ObjectFactoryVtable* anim_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005056A0, anim_state_object_sizes, construct_anim_state_view,
        destroy_anim_state_view};
    return &table;
}

bool construct_die_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_die_state(state, parameters, die_context_of(word(state, 8)));
}

void destroy_die_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_die_state(state, die_context_of(word(state, 8)));
}

const ObjectFactoryVtable* die_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505720, die_state_object_sizes, construct_die_state_view,
        destroy_die_state_view};
    return &table;
}

bool construct_knockdown_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_knockdown_state(state, parameters, knockdown_context_of(word(state, 8)));
}

void destroy_knockdown_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_knockdown_state(state, knockdown_context_of(word(state, 8)));
}

const ObjectFactoryVtable* knockdown_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A20, knockdown_state_object_sizes, construct_knockdown_state_view,
        destroy_knockdown_state_view};
    return &table;
}

bool construct_pickup_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_pickup_state(state, parameters, pickup_context_of(word(state, 8)));
}

void destroy_pickup_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_pickup_state(state, pickup_context_of(word(state, 8)));
}

const ObjectFactoryVtable* pickup_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A60, pickup_state_object_sizes, construct_pickup_state_view,
        destroy_pickup_state_view};
    return &table;
}

bool construct_lift_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_lift_state(state, parameters, pickup_context_of(word(state, 8)));
}

void destroy_lift_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_lift_state(state, pickup_context_of(word(state, 8)));
}

const ObjectFactoryVtable* lift_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A80, lift_state_object_sizes, construct_lift_state_view,
        destroy_lift_state_view};
    return &table;
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

bool construct_grabbed_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_grabbed_state(state, parameters, grabbed_context_of(word(state, 8)));
}

void destroy_grabbed_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_grabbed_state(state, grabbed_context_of(word(state, 8)));
}

const ObjectFactoryVtable* grabbed_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005058E0, grabbed_state_object_sizes, construct_grabbed_state_view,
        destroy_grabbed_state_view};
    return &table;
}

bool construct_player_death_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_player_death_state(state, parameters, player_death_context_of(word(state, 8)));
}

void destroy_player_death_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_player_death_state(state, player_death_context_of(word(state, 8)));
}

const ObjectFactoryVtable* player_death_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505800, player_death_state_object_sizes,
        construct_player_death_state_view, destroy_player_death_state_view};
    return &table;
}

bool construct_struggle_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_struggle_state(state, parameters, context);
}

void destroy_struggle_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_struggle_state(state, context);
}

const ObjectFactoryVtable* struggle_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505BE0, struggle_state_object_sizes, construct_struggle_state_view,
        destroy_struggle_state_view};
    return &table;
}

bool construct_walk_target_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_walk_target(state, parameters, context);
}

void destroy_walk_target_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_walk_target(state, context);
}

bool construct_fly_alert_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_fly_alert(state, parameters, context);
}

void destroy_fly_alert_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_fly_alert(state, context);
}

bool construct_throw_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_throw_state(state, parameters, throw_context_of(word(state, 8)));
}

void destroy_throw_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_throw_state(state, throw_context_of(word(state, 8)));
}

const ObjectFactoryVtable* throw_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C40, throw_state_object_sizes, construct_throw_state_view,
        destroy_throw_state_view};
    return &table;
}

bool construct_hold_state_view(FactoryObject& object, std::span<const u8>)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    return initialize_runtime_hold_state(state, context);
}

void destroy_hold_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    auto context = body_context_of(word(state, 8));
    destroy_runtime_hold_state(state, context);
}

const ObjectFactoryVtable* hold_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C20, hold_state_object_sizes, construct_hold_state_view,
        destroy_hold_state_view};
    return &table;
}

// Getter 0x00091AB0: the walk target's sizes 0x00091A90.
const ObjectFactoryVtable* fly_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x005058A0, walk_target_object_sizes, construct_fly_alert_view,
        destroy_fly_alert_view};
    return &table;
}

const ObjectFactoryVtable* walk_target_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505CC0, walk_target_object_sizes, construct_walk_target_view,
        destroy_walk_target_view};
    return &table;
}

const ObjectFactoryVtable* fly_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505880, fly_state_object_sizes, construct_fly_state_view,
        destroy_fly_state_view};
    return &table;
}

const ObjectFactoryVtable* walk_alert_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505CA0, walk_alert_object_sizes, construct_walk_alert_view,
        destroy_walk_alert_view};
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
        set(body, body_field::holds, get(body, body_field::holds) + 1);
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
        const u32 count = get(body, body_field::holds) - 1;
        set(body, body_field::holds, count);
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

// Xbox 0x000A55D0
void walk_alert_object_sizes(ObjectSizes& output)
{
    output.instance = 0x88;
    output.parameters = 0xB8;
}

// Xbox 0x000A5650
bool initialize_runtime_walk_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    const i16 count = i16(read_u16(parameters, 0xC));
    std::memcpy(state.data() + 0x2C, &count, 2);
    auto& records = walk_alert_records[state.data()];
    records.assign(count > 0 ? size_t(count) : 0, nullptr);
    for (i16 index = 0; index < count; ++index)
    {
        const auto entry = context.resolve(word(parameters, 0x10) + u32(index) * 0x10, 0x10);
        const u32 name_address = word(entry, 0);
        const char* name = name_address == 0 ? nullptr
            : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
        // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
        const AnimationRecord* record = find_runtime_body_animation(name, context.actions);
        records[size_t(index)] = record;
        put_word(state, 0x30 + size_t(index) * 0x10, record != nullptr ? 1 : 0);
        put_word(state, 0x34 + size_t(index) * 0x10, word(entry, 4));
        put_word(state, 0x38 + size_t(index) * 0x10, word(entry, 8));
        put_word(state, 0x3C + size_t(index) * 0x10, word(entry, 0xC));
    }
    std::memset(state.data() + 0x2E, 0, 2);
    std::memcpy(state.data() + 0x1A, parameters.data() + 0x94, 2);
    std::memcpy(state.data() + 0x1C, parameters.data() + 0x96, 2);
    put_word(state, 0x20, word(parameters, 0x98));
    put_word(state, 0x24, word(parameters, 0x9C));
    put_word(state, 0x28, word(parameters, 0xA0));
    put_word(state, 0x74, word(parameters, 0xA4));
    put_word(state, 0x78, word(parameters, 0xA8));
    put_word(state, 0x7C, word(parameters, 0xAC));
    put_word(state, 0x80, word(parameters, 0xB0));
    put_word(state, 0x84, word(parameters, 0xB4));
    context.bits->bytes[6] |= 0x80;
    if (parameters[0x14] != 0)
    {
        if (parameters[0x14] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
                reinterpret_cast<const char*>(parameters.data() + 0x14), context.sounds);
        }
        put_word(state, 0x70, 1);
    }
    const u32 bit = word(state, 0x84);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    // The inlined 0x0003A9E0 for band +0x2E (0): 0x000322A0 with actor +0x84, the +0x50 controller (0x0003ACB0
    // inline) and 0x00030880, then actor +0x84 is cleared.
    const u16 band = read_u16(state, 0x2E);
    if (band >= records.size() || records[band] == nullptr)
    {
        throw Error("walk-alert band record is null; 0x000A5650 reads its +0x117");
    }
    start_actor_animation(context.actions.animation, *records[band], context.actions.registry,
        context.actions.error, context.actions.random);
    notify_runtime_body_animation(context.actions);
    body[0x38] = 1;
    body[0x39] = 1;
    return true;
}

// Xbox 0x000A5950
void destroy_runtime_walk_alert(std::span<u8> state, RuntimeBodyStateContext& context)
{
    if (word(state, 0x70) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds);
    }
    const u32 bit = word(state, 0x84);
    if (bit != 0)
    {
        context.bits->bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    context.bits->bytes[6] &= 0x7F;
    walk_alert_records.erase(state.data());
}

// Xbox 0x00090D10
void fly_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x78;
    output.parameters = 0xA8;
}

// Xbox 0x00090D90
bool initialize_runtime_fly_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context)
{
    const auto body = context.body;
    const i16 count = i16(read_u16(parameters, 0xC));
    std::memcpy(state.data() + 0x2C, &count, 2);
    auto& records = walk_alert_records[state.data()];
    records.assign(count > 0 ? size_t(count) : 0, nullptr);
    for (i16 index = 0; index < count; ++index)
    {
        const auto entry = context.resolve(word(parameters, 0x10) + u32(index) * 0x10, 0x10);
        const u32 name_address = word(entry, 0);
        const char* name = name_address == 0 ? nullptr
            : reinterpret_cast<const char*>(context.resolve(name_address, 0).data());
        // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
        const AnimationRecord* record = find_runtime_body_animation(name, context.actions);
        records[size_t(index)] = record;
        put_word(state, 0x30 + size_t(index) * 0x10, record != nullptr ? 1 : 0);
        put_word(state, 0x34 + size_t(index) * 0x10, word(entry, 4));
        put_word(state, 0x38 + size_t(index) * 0x10, word(entry, 8));
        put_word(state, 0x3C + size_t(index) * 0x10, word(entry, 0xC));
    }
    std::memset(state.data() + 0x2E, 0, 2);
    std::memcpy(state.data() + 0x1A, parameters.data() + 0x94, 2);
    std::memcpy(state.data() + 0x1C, parameters.data() + 0x96, 2);
    put_word(state, 0x20, word(parameters, 0x98));
    put_word(state, 0x24, word(parameters, 0x9C));
    put_word(state, 0x28, word(parameters, 0xA0));
    if (parameters[0x14] != 0)
    {
        if (parameters[0x14] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
                reinterpret_cast<const char*>(parameters.data() + 0x14), context.sounds); // 0x0012A260
        }
        put_word(state, 0x70, 1);
    }
    // The looping animation (+0xA4): the inlined 0x0003AAA0 pair, then the inlined 0x0001AFA0.
    const u32 loop_address = word(parameters, 0xA4);
    const char* loop_name = loop_address == 0 ? nullptr
        : reinterpret_cast<const char*>(context.resolve(loop_address, 0).data());
    const AnimationRecord* loop = find_runtime_body_animation(loop_name, context.actions);
    fly_loop_records[state.data()] = loop;
    put_word(state, 0x74, loop != nullptr ? 1 : 0);
    if (loop != nullptr)
    {
        start_actor_animation(context.actions.animation, *loop, context.actions.registry, context.actions.error,
            context.actions.random);
        notify_runtime_body_animation(context.actions); // 0x0001B020
    }
    // The inlined 0x0001AFA0 for band +0x2E (0).
    const u16 band = read_u16(state, 0x2E);
    if (band >= records.size() || records[band] == nullptr)
    {
        throw Error("fly band record is null; 0x00090D90 reads its +0x117");
    }
    start_actor_animation(context.actions.animation, *records[band], context.actions.registry,
        context.actions.error, context.actions.random);
    notify_runtime_body_animation(context.actions); // 0x0001B020
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits->bytes[6] |= 0x80;
    return true;
}

// Xbox 0x00091110
void destroy_runtime_fly_state(std::span<u8> state, RuntimeBodyStateContext& context)
{
    if (word(state, 0x70) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0
    }
    if (word(state, 0x74) != 0)
    {
        // The loop's slot fades out over 0.25 s (DAT_0032B104).
        const AnimationRecord* loop = fly_loop_records[state.data()];
        AnimationController* controller = context.animation; // Actor +0x50.
        const i32 slot = loop == nullptr ? 0 : i32(loop->slot); // Record +0x117.
        if (controller != nullptr && slot != 0)
        {
            AnimationControllerSlot& entry = controller->slots.at(size_t(slot));
            if (entry.player != nullptr)
            {
                entry.fade_duration = 0.25f; // +0x1C.
                entry.fade_mode = 3; // +0x18.
            }
            controller->dirty = true; // +0x5C.
        }
    }
    context.bits->bytes[6] &= 0x7F;
    walk_alert_records.erase(state.data());
    fly_loop_records.erase(state.data());
}

const AnimationRecord* runtime_walk_alert_record(std::span<const u8> state, u16 index)
{
    const auto found = walk_alert_records.find(state.data());
    if (found == walk_alert_records.end() || index >= found->second.size())
    {
        throw Error("walk-alert state has no record for its speed band");
    }
    return found->second[index];
}

void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of, std::function<RuntimeHitStateLifecycle(u32)> hit_of,
    std::function<RuntimeDizzyStateContext&(u32)> dizzy_of, std::function<RuntimeKnockdownContext&(u32)> knockdown_of,
    std::function<RuntimeDieStateContext&(u32)> die_of, std::function<RuntimeAnimStateContext&(u32)> anim_of,
    std::function<RuntimePickupStateContext&(u32)> pickup_of,
    std::function<RuntimePlayerDeathContext&(u32)> player_death_of,
    std::function<RuntimeGrabbedStateContext&(u32)> grabbed_of,
    std::function<RuntimeThrowStateContext&(u32)> throw_of)
{
    throw_context_of = std::move(throw_of);
    if (throw_context_of)
    {
        types[0x8B] = ObjectFactoryType{9, throw_state_lifecycle};
    }
    grabbed_context_of = std::move(grabbed_of);
    types[0x70] = ObjectFactoryType{9, grabbed_state_lifecycle};
    player_death_context_of = std::move(player_death_of);
    types[0x69] = ObjectFactoryType{9, player_death_state_lifecycle};
    pickup_context_of = std::move(pickup_of);
    types[0x7C] = ObjectFactoryType{9, pickup_state_lifecycle};
    types[0x7D] = ObjectFactoryType{9, lift_state_lifecycle};
    anim_context_of = std::move(anim_of);
    types[0x5E] = ObjectFactoryType{9, anim_state_lifecycle};
    die_context_of = std::move(die_of);
    types[0x62] = ObjectFactoryType{9, die_state_lifecycle};
    knockdown_context_of = std::move(knockdown_of);
    types[0x7A] = ObjectFactoryType{9, knockdown_state_lifecycle};
    body_context_of = std::move(context_of);
    hit_context_of = std::move(hit_of);
    dizzy_context_of = std::move(dizzy_of);
    types[0x64] = ObjectFactoryType{9, dizzy_state_lifecycle};
    types[0x73] = ObjectFactoryType{9, hit_state_lifecycle};
    types[0x74] = ObjectFactoryType{9, hit_furnace_state_lifecycle};
    types[9] = ObjectFactoryType{1, actor_move_lifecycle};
    types[0x87] = ObjectFactoryType{9, stand_alert_lifecycle};
    types[0x88] = ObjectFactoryType{9, struggle_state_lifecycle};
    types[0x8E] = ObjectFactoryType{9, walk_alert_lifecycle};
    types[0x8F] = ObjectFactoryType{9, walk_target_lifecycle};
    types[0x6E] = ObjectFactoryType{9, fly_alert_lifecycle};
    types[0x6D] = ObjectFactoryType{9, fly_state_lifecycle};
    types[0x8A] = ObjectFactoryType{9, hold_state_lifecycle};
}
}
