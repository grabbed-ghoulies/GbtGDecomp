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
namespace parameter = scene_fx_parameter;

// One 0x0054DD7C step and its [0, 1) fraction (the mantissa as a float in [1, 2), less DAT_0032B0F8).
float next_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}

SceneFxSlot& slot_at(SceneFxControlState& state, size_t slot)
{
    if (slot >= state.slots.size())
    {
        throw Error("entitySceneControlFx copy " + std::to_string(slot) + " lies outside the control's 0x1EC bytes");
    }
    return state.slots[slot];
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
    output.instance = sizeof(SceneFxControlState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000832F0
bool construct_runtime_scene_fx_control(SceneFxControlState& state, std::span<const u8> parameters,
    SceneFxControlServices& services)
{
    state.on_time = get(parameters, parameter::on_time);
    state.off_time = get(parameters, parameter::off_time);
    state.scale = get(parameters, parameter::scale);
    state.rate = get(parameters, parameter::rate);
    state.count = get(parameters, parameter::count);
    state.radius = get(parameters, parameter::radius);
    state.prerun = get(parameters, parameter::prerun);
    state.effect = get(parameters, parameter::effect);
    for (i32 index = 0; index < state.count; ++index)
    {
        SceneFxSlot& slot = slot_at(state, size_t(index));
        slot.effect = -1;
        Vec3 position = get(parameters, parameter::position);
        const float radius = state.radius;
        if (radius != 0.f)
        {
            const float distance = next_fraction(services.random) * radius;
            const float angle = next_fraction(services.random) * 6.2831855f - 3.1415927f; // DAT_0032B298, DAT_0032B148.
            position.x = float(retail_sin(angle)) * distance + position.x; // 0x00153320
            position.z = float(retail_cos(angle)) * distance + position.z; // 0x00153230
        }
        slot.position = position;
        slot.euler = get(parameters, parameter::euler);
        const auto on = state.on_time;
        const auto off = state.off_time;
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
void stop_runtime_scene_fx(SceneFxControlState& state, size_t slot, SceneFxControlServices& services)
{
    SceneFxSlot& copy = slot_at(state, slot);
    copy.on = 0u;
    const auto off = state.off_time;
    copy.timer = (off[1] - off[0]) * next_fraction(services.random) + off[0];
    const i32 index = copy.effect;
    if (index != -1)
    {
        ActorEffectNode& node = effect_node(index);
        node.bytes[0x18] = u8(node.bytes[0x18] | 8);
        put_float(node.bytes, 8, -1.f); // DAT_0032B120.
        copy.effect = -1;
    }
}

// Xbox 0x00083080, EAX slot, EDI state
void start_runtime_scene_fx(SceneFxControlState& state, size_t slot, u32 prerun, SceneFxControlServices& services)
{
    SceneFxSlot& copy = slot_at(state, slot);
    copy.on = 1u;
    const auto on = state.on_time;
    copy.timer = (on[1] - on[0]) * next_fraction(services.random) + on[0];
    g_actor_creation_options.position = copy.position;
    g_actor_creation_options.euler = copy.euler;
    g_actor_creation_options.scale = state.scale; // DAT_004E1C88.
    g_actor_creation_options.flags = 0x45;
    const auto name = state.effect;
    load_actor_effect_stream(name.data(), &g_actor_creation_options, services.effects); // 0x00053170
    const i32 index = g_last_actor_effect; // DAT_0042A434.
    g_last_actor_effect = -1;
    copy.effect = index;
    if (index == -1)
    {
        return;
    }
    ActorEffectNode& node = effect_node(index);
    const auto rate = state.rate;
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
    if (prerun == 0 || !(state.prerun > 0.f))
    {
        return;
    }
    float elapsed = 0.f;
    do
    {
        services.tick_effect(node, std::bit_cast<float>(0x3C888889u)); // 0x000EF2D0, EAX the node
        elapsed += std::bit_cast<float>(0x3C888889u); // DAT_0032B150.
    }
    while (state.prerun > elapsed);
}

// Xbox 0x00083280, EAX state
void step_runtime_scene_fx(SceneFxControlState& state, float step, u32 prerun, SceneFxControlServices& services)
{
    for (i32 index = 0; index < state.count; ++index)
    {
        SceneFxSlot& copy = slot_at(state, size_t(index));
        const float left = copy.timer;
        if (!(0.f < left))
        {
            continue;
        }
        const float next = left - step;
        copy.timer = next;
        if (!(next < 0.f))
        {
            continue;
        }
        if (std::bit_cast<float>(copy.on) == 0.f) // The word read as a float.
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
void tick_runtime_scene_fx_control(SceneFxControlState& state, SceneFxControlServices& services)
{
    const float step = runtime_frame_step(services.clock);
    state.base.elapsed = state.base.elapsed + step;
    step_runtime_scene_fx(state, step, 0, services); // 0x00083280, with 0 under a freeze
}
}
