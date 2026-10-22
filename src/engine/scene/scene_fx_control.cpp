#include "scene_fx_control.h"
#include "scene_control_fields.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = scene_fx_field;
namespace parameter = scene_fx_parameter;
namespace slot_field = scene_fx_slot;

// One 0x0054DD7C step and its [0, 1) fraction (the mantissa as a float in [1, 2), less DAT_0032B0F8).
float next_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}

std::span<u8> slot_at(std::span<u8> state, size_t slot)
{
    const size_t offset = field::kSlots + slot * field::kSlotStride;
    if (offset + field::kSlotStride > state.size())
    {
        throw Error("entitySceneControlFx copy " + std::to_string(slot) + " lies outside the control's 0x1EC bytes");
    }
    return state.subspan(offset, field::kSlotStride);
}

ActorEffectNode& effect_node(i32 index)
{
    ActorEffectNode* node = index < 0 || size_t(index) >= g_actor_effect_nodes.size() ? nullptr
                                                                                       : g_actor_effect_nodes[size_t(index)];
    if (node == nullptr)
    {
        throw Error("entitySceneControlFx reads DAT_00506930 slot " + std::to_string(index) + ", which holds no effect");
    }
    return *node;
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x00082F80
void scene_fx_control_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000832F0
bool construct_runtime_scene_fx_control(std::span<u8> state, std::span<const u8> parameters,
    SceneFxControlServices& services)
{
    set(state, field::on_time, get(parameters, parameter::on_time));
    set(state, field::off_time, get(parameters, parameter::off_time));
    set(state, field::scale, get(parameters, parameter::scale));
    set(state, field::rate, get(parameters, parameter::rate));
    set(state, field::count, get(parameters, parameter::count));
    set(state, field::radius, get(parameters, parameter::radius));
    set(state, field::prerun, get(parameters, parameter::prerun));
    set(state, field::effect, get(parameters, parameter::effect));
    for (i32 index = 0; index < get(state, field::count); ++index)
    {
        const auto slot = slot_at(state, size_t(index));
        set(slot, slot_field::effect, -1);
        Vec3 position = get(parameters, parameter::position);
        const float radius = get(state, field::radius);
        if (radius != 0.f)
        {
            const float distance = next_fraction(services.random) * radius;
            const float angle = next_fraction(services.random) * 6.2831855f - 3.1415927f; // DAT_0032B298, DAT_0032B148.
            position.x = float(retail_sin(angle)) * distance + position.x; // 0x00153320
            position.z = float(retail_cos(angle)) * distance + position.z; // 0x00153230
        }
        set(slot, slot_field::position, position);
        set(slot, slot_field::euler, get(parameters, parameter::euler));
        const auto on = get(state, field::on_time);
        const auto off = get(state, field::off_time);
        if ((on[0] == 0.f && on[1] == 0.f) || (off[0] == 0.f && off[1] == 0.f))
        {
            start_runtime_scene_fx(state, size_t(index), 1, services); // 0x00083080, EAX the copy
        }
        else if (next_fraction(services.random) <= 0.5f) // DAT_0032B0F4; JBE also takes an unordered roll.
        {
            stop_runtime_scene_fx(state, size_t(index), services); // 0x00082FF0, ECX the copy
        }
        else
        {
            start_runtime_scene_fx(state, size_t(index), 1, services);
        }
    }
    return true;
}

// Xbox 0x00082FF0, ECX slot, EAX state
void stop_runtime_scene_fx(std::span<u8> state, size_t slot, SceneFxControlServices& services)
{
    const auto copy = slot_at(state, slot);
    set(copy, slot_field::on, 0u);
    const auto off = get(state, field::off_time);
    set(copy, slot_field::timer, (off[1] - off[0]) * next_fraction(services.random) + off[0]);
    const i32 index = get(copy, slot_field::effect);
    if (index != -1)
    {
        ActorEffectNode& node = effect_node(index);
        node.bytes[0x18] = u8(node.bytes[0x18] | 8);
        put_float(node.bytes, 8, -1.f); // DAT_0032B120.
        set(copy, slot_field::effect, -1);
    }
}

// Xbox 0x00083080, EAX slot, EDI state
void start_runtime_scene_fx(std::span<u8> state, size_t slot, u32 prerun, SceneFxControlServices& services)
{
    const auto copy = slot_at(state, slot);
    set(copy, slot_field::on, 1u);
    const auto on = get(state, field::on_time);
    set(copy, slot_field::timer, (on[1] - on[0]) * next_fraction(services.random) + on[0]);
    g_actor_creation_options.position = get(copy, slot_field::position);
    g_actor_creation_options.euler = get(copy, slot_field::euler);
    g_actor_creation_options.scale = get(state, field::scale); // DAT_004E1C88.
    g_actor_creation_options.flags = 0x45;
    const auto name = get(state, field::effect);
    load_actor_effect_stream(name.data(), &g_actor_creation_options, services.effects); // 0x00053170
    const i32 index = g_last_actor_effect; // DAT_0042A434.
    g_last_actor_effect = -1;
    set(copy, slot_field::effect, index);
    if (index == -1)
    {
        return;
    }
    ActorEffectNode& node = effect_node(index);
    const auto rate = get(state, field::rate);
    if (rate[0] != 0.f && rate[1] != 0.f) // UCOMISS; JNP skips only an ordered zero.
    {
        const float pick = (rate[1] - rate[0]) * next_fraction(services.random) + rate[0];
        if (pick > 0.f)
        {
            const float period = 1.f / pick; // DAT_0032B0F8.
            put_float(node.bytes, 8, period);
            if (read_f32(node.bytes, 0xC) > period)
            {
                put_float(node.bytes, 0xC, period);
            }
        }
        else
        {
            put_float(node.bytes, 8, pick);
        }
    }
    if (prerun == 0 || !(get(state, field::prerun) > 0.f))
    {
        return;
    }
    float elapsed = 0.f;
    do
    {
        services.tick_effect(node, std::bit_cast<float>(0x3C888889u)); // 0x000EF2D0, EAX the node
        elapsed += std::bit_cast<float>(0x3C888889u); // DAT_0032B150.
    }
    while (get(state, field::prerun) > elapsed);
}

// Xbox 0x00083280, EAX state
void step_runtime_scene_fx(std::span<u8> state, float step, u32 prerun, SceneFxControlServices& services)
{
    for (i32 index = 0; index < get(state, field::count); ++index)
    {
        const auto copy = slot_at(state, size_t(index));
        const float left = get(copy, slot_field::timer);
        if (!(0.f < left))
        {
            continue;
        }
        const float next = left - step;
        set(copy, slot_field::timer, next);
        if (!(next < 0.f))
        {
            continue;
        }
        if (std::bit_cast<float>(get(copy, slot_field::on)) == 0.f) // The word read as a float.
        {
            start_runtime_scene_fx(state, size_t(index), prerun, services); // 0x00083080
        }
        else
        {
            stop_runtime_scene_fx(state, size_t(index), services); // 0x00082FF0
        }
    }
}

// Xbox 0x00083570
void tick_runtime_scene_fx_control(std::span<u8> state, SceneFxControlServices& services)
{
    const float step = runtime_frame_step(services.clock);
    set(state, scene_control_field::elapsed, get(state, scene_control_field::elapsed) + step);
    step_runtime_scene_fx(state, step, 0, services); // 0x00083280, with 0 under a freeze
}
}
