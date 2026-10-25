#include "body_spin_dizzy_state.h"
#include "gbtg/anim.h"
#include "gbtg/sound_runtime.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = spin_dizzy_state_field;
namespace parameter = spin_dizzy_state_parameter;

std::function<RuntimeSpinDizzyStateContext&(u32)> spin_dizzy_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

bool construct_spin_dizzy_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_spin_dizzy_state(state, parameters, spin_dizzy_context_of(body_address_of(state)));
}

void destroy_spin_dizzy_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_spin_dizzy_state(state, spin_dizzy_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* spin_dizzy_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505BA0, spin_dizzy_state_object_sizes,
        construct_spin_dizzy_state_view, destroy_spin_dizzy_state_view};
    return &table;
}
}

// Xbox 0x000A21D0
void spin_dizzy_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A2250
bool initialize_runtime_spin_dizzy_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    set(state, field::turn_rate, get(parameters, parameter::turn_rate));
    set(state, field::reach_rate, get(parameters, parameter::reach_rate));
    set(state, field::speed, get(parameters, parameter::speed));
    set(state, field::body_71c, get(parameters, parameter::body_71c));
    set(state, field::body_720, get(parameters, parameter::body_720));
    set(state, field::body_724, get(parameters, parameter::body_724));
    dizzy.random = dizzy.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
    const float unit = std::bit_cast<float>((dizzy.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
    const float least = context.length_range[0];
    set(state, field::dizzy_left, (context.length_range[1] - least) * unit + least);
    const char* sound = field_data(parameters, parameter::sound);
    if (sound[0] != 0)
    {
        set(state, field::sounding, 1u);
        if (sound[0] != 0)
        {
            queue_actor_sound(dizzy.actor.sound_slots[1], 0, &dizzy.actor, 1, nullptr, nullptr, 1.f, sound,
                dizzy.sounds); // 0x0012A260, EBX actor +0x3EC
        }
    }
    return true;
}

// Xbox 0x000A2340
void destroy_runtime_spin_dizzy_state(std::span<u8> state, RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    put_word(dizzy.body, 0x71C, context.rest_71c[0]);
    put_word(dizzy.body, 0x720, context.rest_71c[1]);
    put_word(dizzy.body, 0x724, context.rest_71c[2]);
    if (get(state, field::sounding) != 0)
    {
        release_actor_sound(dizzy.actor.sound_slots[1], dizzy.sounds); // 0x0012A3D0, ESI actor +0x3EC
    }
}

// Xbox 0x000A23A0
void tick_runtime_spin_dizzy_state(std::span<u8> state, RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    tick_runtime_dizzy_state(state, dizzy); // 0x0008CC40, the same two arguments
    const float left = get(state, field::dizzy_left);
    const float angle = get(state, field::turn_rate) * left;
    const float reach = get(state, field::reach_rate) * left;
    const float speed = get(state, field::speed);
    put_word(dizzy.body, 0x71C, get(state, field::body_71c));
    put_word(dizzy.body, 0x720, get(state, field::body_720));
    put_word(dizzy.body, 0x724, get(state, field::body_724));
    const float side = float(retail_sin(angle) * reach); // 0x00153320
    const float forward = float(retail_cos(angle) * reach); // 0x00153230
    const Vec3 centre = get(state, field::unk_19c);
    put_float(dizzy.body, 0x318, ((centre.x + side) - centre.x) * speed);
    put_float(dizzy.body, 0x31C, (centre.y - centre.y) * speed);
    put_float(dizzy.body, 0x320, ((centre.z + forward) - centre.z) * speed);
}

// Xbox 0x000A24B0
void message_runtime_spin_dizzy_state(std::span<u8> state, std::span<const u32> message,
    RuntimeSpinDizzyStateContext& context)
{
    if (message[0] == 0x7C)
    {
        if (message[1] == get(state, field::first_star))
        {
            set(state, field::first_star, 0u);
        }
        if (message[1] == get(state, field::second_star))
        {
            set(state, field::second_star, 0u);
        }
        if (message[1] == get(state, field::third_star))
        {
            set(state, field::third_star, 0u);
        }
    }
    if (message[0] == 0x27)
    {
        set(state, field::next_key, context.primary_key()); // 0x00026950, the body
    }
}

void install_runtime_spin_dizzy_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSpinDizzyStateContext&(u32 body_address)> context_of)
{
    spin_dizzy_context_of = std::move(context_of);
    types[0x86] = ObjectFactoryType{0x64, spin_dizzy_state_lifecycle}; // Getter 0x000A21F0.
}
}
