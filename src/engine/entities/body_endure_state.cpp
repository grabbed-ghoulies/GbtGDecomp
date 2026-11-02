#include "body_endure_state.h"
#include "mind_fields.h"
#include "player_mind_requests.h"
#include "gbtg/anim.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = endure_state_field;
namespace parameter = endure_state_parameter;

std::function<RuntimeEndureStateContext&(u32)> endure_context_of;

// The host halves of +0x124 and +0x128: the records 0x0003AAA0 found.
struct EndureRecords
{
    const AnimationRecord* entry = nullptr;
    const AnimationRecord* held = nullptr;
};
std::unordered_map<const u8*, EndureRecords> endure_records;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void copy_name(char* target, const char* source)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

void start_animation(const AnimationRecord* record, RuntimeEndureStateContext& context)
{
    if (record == nullptr)
    {
        throw Error("type 0x84 state starts an animation through a record 0x0003AAA0 did not find");
    }
    start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ECX the record
}

// Body +0x890 holds 0x20000 while the state lasts; its earlier value goes to state +0x12C.
void hold_890(std::span<u8> state, RuntimeEndureStateContext& context)
{
    set(state, field::saved_890, read_u32(context.body, 0x890));
    put_word(context.body, 0x890, 0x20000);
}

bool construct_endure_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_endure_state(state, parameters, endure_context_of(body_address_of(state)));
}

void destroy_endure_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_endure_state(state, endure_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* endure_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505B60, endure_state_object_sizes, construct_endure_state_view,
        destroy_endure_state_view};
    return &table;
}
}

// Xbox 0x000A1430
void endure_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A14B0
bool initialize_runtime_endure_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeEndureStateContext& context)
{
    context.bits.bytes[1] |= 2;
    set(state, field::made, 0u);
    set(state, field::hits, 0);
    std::memcpy(field_data(state, field::end_sound), field_data(parameters, parameter::end_sound), 0x80);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
    const float unit = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
    const i32 least = get(parameters, parameter::least_hits);
    const i32 most = get(parameters, parameter::most_hits);
    set(state, field::needed_hits, i32(float(most - least) * unit + float(least)));
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    auto& records = endure_records[state.data()];
    records.entry = find_runtime_body_animation(context.image_string(get(parameters, parameter::entry_animation)),
        context.actions);
    set(state, field::entry_animation, records.entry != nullptr ? 1u : 0u);
    records.held = find_runtime_body_animation(context.image_string(get(parameters, parameter::held_animation)),
        context.actions);
    set(state, field::held_animation, records.held != nullptr ? 1u : 0u);
    std::memcpy(field_data(state, field::held_sound), field_data(parameters, parameter::held_sound), 0x80);
    set(state, field::sounding, 0u);
    set(state, field::hold, get(parameters, parameter::hold));
    copy_name(field_data(state, field::object), field_data(parameters, parameter::object));
    start_animation(records.entry, context); // 0x0001AFA0, ECX +0x124
    hold_890(state, context);
    set(state, field::phase, 1u);
    return true;
}

// Xbox 0x000A1670
void destroy_runtime_endure_state(std::span<u8> state, RuntimeEndureStateContext& context)
{
    context.bits.bytes[5] &= 0xF7;
    context.bits.bytes[1] &= 0xFD;
    put_word(context.body, 0x890, get(state, field::saved_890));
    if (get(state, field::sounding) != 0)
    {
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
        set(state, field::sounding, 0u);
    }
    const u32 made = get(state, field::made);
    if (made != 0)
    {
        std::array<u32, 2> message{5, 0};
        context.message(made, message); // The object's vtable +0xC
        set(state, field::made, 0u);
    }
    endure_records.erase(state.data());
}

// Xbox 0x000A1930, EDI state
void enter_runtime_endure_phase(std::span<u8> state, u32 phase, RuntimeEndureStateContext& context)
{
    const auto& records = endure_records.at(state.data());
    if (phase == 1)
    {
        start_animation(records.entry, context); // 0x0001AFA0, ECX +0x124
        hold_890(state, context);
    }
    else if (phase == 2)
    {
        start_animation(records.held, context); // 0x0001AFA0, ECX +0x128
        const char* sound = field_data(state, field::held_sound);
        if (sound[0] != 0)
        {
            set(state, field::sounding, 1u);
            request_actor_sound(context.actor, 1, sound, context.sounds); // 0x00041D00, ECX 1, EDX the actor
        }
    }
    else if (phase == 3)
    {
        u32 made = 0;
        auto clone = clone_executable_resource(context.resources, field_data(state, field::object)); // 0x00107720
        if (clone.bytes != nullptr)
        {
            std::memcpy(clone.bytes.get() + 8, &context.actor_address, 4);
            made = context.create_object({clone.bytes.get(), clone.size}); // 0x00107520
            clone.bytes.reset(); // 0x000663E0
        }
        set(state, field::made, made);
        const char* sound = field_data(state, field::end_sound);
        if (sound[0] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[6], 0, &context.actor, 6, nullptr, nullptr, 1.f, sound,
                context.sounds); // 0x0012A260, EBX actor +0x784
        }
    }
    set(state, field::phase, phase);
}

// Xbox 0x000A1710
void tick_runtime_endure_state(std::span<u8> state, RuntimeEndureStateContext& context)
{
    switch (get(state, field::phase))
    {
    case 1:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
        {
            // The inlined phase 2 of 0x000A1930.
            enter_runtime_endure_phase(state, 2, context);
        }
        return;
    case 2:
    {
        bool holding = false;
        if (!context.mind.empty() && get(context.mind, mind_field::target_record) != 0)
        {
            holding = get(state, field::hits) < get(state, field::needed_hits);
        }
        if (holding)
        {
            return;
        }
        if (get(state, field::sounding) != 0)
        {
            release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
            set(state, field::sounding, 0u);
        }
        enter_runtime_endure_phase(state, 3, context); // 0x000A1930
        return;
    }
    case 3:
    {
        const float step = context.clock.freeze <= 0.f ? context.clock.dt * context.clock.scale : 0.f;
        const float rate = context.actor.animation_pause > 0.f ? 0.f : context.actor.animation_rate; // +0x930, +0x92C
        float hold = get(state, field::hold);
        if (0.f < hold)
        {
            hold = hold - rate * step;
            set(state, field::hold, hold);
            if (hold <= 0.f)
            {
                set(state, field::hold, 0.f);
                context.bits.bytes[5] |= 8;
            }
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000A1850
void message_runtime_endure_state(std::span<u8> state, std::span<const u32> message,
    RuntimeEndureStateContext& context)
{
    switch (message[0])
    {
    case 0x15:
    case 0x19:
    case 0x1D:
        if (get(state, field::phase) == 2)
        {
            set(state, field::hits, get(state, field::hits) + 1);
        }
        return;
    case 0x7C:
    {
        if (context.mind.empty())
        {
            return;
        }
        const u32 target = get(context.mind, mind_field::target_record);
        if (target != 0 && message[1] == read_u32(context.target_at(target), 0x10))
        {
            set(state, field::hits, get(state, field::needed_hits));
        }
        return;
    }
    default:
        return;
    }
}

void install_runtime_endure_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeEndureStateContext&(u32 body_address)> context_of)
{
    endure_context_of = std::move(context_of);
    types[0x84] = ObjectFactoryType{9, endure_state_lifecycle}; // Getter 0x000A1450.
}
}
