#include "body_spin_dizzy_state.h"
#include <cstddef>
#include "body_fields.h"
#include "gbtg/anim.h"
#include "gbtg/sound_runtime.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace parameter = spin_dizzy_state_parameter;

std::function<RuntimeSpinDizzyStateContext&(u32)> spin_dizzy_context_of;

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

bool construct_spin_dizzy_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    SpinDizzyState& state = body_state_view<SpinDizzyState>({object.bytes.get(), object.size});
    return initialize_runtime_spin_dizzy_state(state, parameters, spin_dizzy_context_of(state.dizzy.base.body));
}

void destroy_spin_dizzy_state_view(FactoryObject& object)
{
    SpinDizzyState& state = body_state_view<SpinDizzyState>({object.bytes.get(), object.size});
    destroy_runtime_spin_dizzy_state(state, spin_dizzy_context_of(state.dizzy.base.body));
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
    output.instance = sizeof(SpinDizzyState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000A2250
bool initialize_runtime_spin_dizzy_state(SpinDizzyState& state, std::span<const u8> parameters,
    RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    state.turn_rate = get(parameters, parameter::turn_rate);
    state.reach_rate = get(parameters, parameter::reach_rate);
    state.speed = get(parameters, parameter::speed);
    state.body_71c = get(parameters, parameter::body_71c);
    state.body_720 = get(parameters, parameter::body_720);
    state.body_724 = get(parameters, parameter::body_724);
    dizzy.random = dizzy.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
    const float unit = std::bit_cast<float>((dizzy.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
    const float least = context.length_range[0];
    state.dizzy.time_left = (context.length_range[1] - least) * unit + least;
    const char* sound = field_data(parameters, parameter::sound);
    if (sound[0] != 0)
    {
        state.sounding = 1u;
        if (sound[0] != 0)
        {
            queue_actor_sound(dizzy.actor.sound_slots[1], 0, &dizzy.actor, 1, nullptr, nullptr, 1.f, sound,
                dizzy.sounds); // 0x0012A260, EBX actor +0x3EC
        }
    }
    return true;
}

// Xbox 0x000A2340
void destroy_runtime_spin_dizzy_state(SpinDizzyState& state, RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates), context.rest_71c[0]);
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates) + 4, context.rest_71c[1]);
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates) + 8, context.rest_71c[2]);
    if (state.sounding != 0)
    {
        release_actor_sound(dizzy.actor.sound_slots[1], dizzy.sounds); // 0x0012A3D0, ESI actor +0x3EC
    }
}

// Xbox 0x000A23A0
void tick_runtime_spin_dizzy_state(SpinDizzyState& state, RuntimeSpinDizzyStateContext& context)
{
    auto& dizzy = context.dizzy;
    tick_runtime_dizzy_state(state.dizzy, dizzy); // 0x0008CC40, the same two arguments
    const float left = state.dizzy.time_left;
    const float angle = state.turn_rate * left;
    const float reach = state.reach_rate * left;
    const float speed = state.speed;
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates), state.body_71c);
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates) + 4, state.body_720);
    put_word(dizzy.body, offsetof(BodyRecord, turn_rates) + 8, state.body_724);
    const float side = float(retail_sin(angle) * reach); // 0x00153320
    const float forward = float(retail_cos(angle) * reach); // 0x00153230
    const Vec3 centre = state.unk_19c;
   body_record(dizzy.body).velocity.x = ((centre.x + side) - centre.x) * speed;
   body_record(dizzy.body).velocity.y = (centre.y - centre.y) * speed;
   body_record(dizzy.body).velocity.z = ((centre.z + forward) - centre.z) * speed;
}

// Xbox 0x000A24B0
void message_runtime_spin_dizzy_state(SpinDizzyState& state, std::span<const u32> message,
    RuntimeSpinDizzyStateContext& context)
{
    if (message[0] == 0x7C)
    {
        if (message[1] == state.dizzy.stars[0].actor)
        {
            state.dizzy.stars[0].actor = 0u;
        }
        if (message[1] == state.dizzy.stars[1].actor)
        {
            state.dizzy.stars[1].actor = 0u;
        }
        if (message[1] == state.dizzy.stars[2].actor)
        {
            state.dizzy.stars[2].actor = 0u;
        }
    }
    if (message[0] == 0x27)
    {
        state.dizzy.base.next_key = context.primary_key(); // 0x00026950, the body
    }
}

void install_runtime_spin_dizzy_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSpinDizzyStateContext&(u32 body_address)> context_of)
{
    spin_dizzy_context_of = std::move(context_of);
    types[0x86] = ObjectFactoryType{0x64, spin_dizzy_state_lifecycle}; // Getter 0x000A21F0.
}
}
