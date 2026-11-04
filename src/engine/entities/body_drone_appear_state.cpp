#include "body_drone_appear_state.h"
#include "actor_transform.h"
#include "player_goal_cleanup.h"
#include "../audio/listener_settings.h"
#include "body_fields.h"
#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace parameter = drone_appear_state_parameter;

constexpr float kStartTime = 0.5f; // DAT_0032B0F4.
constexpr float kOpaque = 255.f; // DAT_0032B158.
constexpr float kFadeRate = 2.f; // DAT_0032B110.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.
constexpr float kWide = 1.5f; // DAT_0032B210.
constexpr float kTall = 5.f; // DAT_0032B184.
constexpr float kThin = 0.01f; // DAT_0032B124.
constexpr float kAppearPop = 0.3f; // DAT_0032B268.
constexpr float kVanishPop = 0.19999999f; // DAT_0032B3A4.
constexpr float kSettleRate = 5.0000005f; // DAT_0032B3A8.
constexpr float kPopRate = 3.3333333f; // DAT_0032B3B4.
constexpr float kTallRate = 4.5f; // DAT_0032B3B0.
constexpr float kWideRate = 1.49f; // DAT_0032B3AC.

std::function<RuntimeDroneAppearStateContext&(u32)> drone_appear_context_of;

bool construct_drone_appear_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    DroneAppearState& state = body_state_view<DroneAppearState>({object.bytes.get(), object.size});
    return initialize_runtime_drone_appear_state(state, parameters, drone_appear_context_of(state.base.body));
}

void destroy_drone_appear_state_view(FactoryObject& object)
{
    DroneAppearState& state = body_state_view<DroneAppearState>({object.bytes.get(), object.size});
    destroy_runtime_drone_appear_state(state, drone_appear_context_of(state.base.body));
}

const ObjectFactoryVtable* drone_appear_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505780, drone_appear_state_object_sizes,
        construct_drone_appear_state_view, destroy_drone_appear_state_view};
    return &table;
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

}

// Xbox 0x0008D330
void drone_appear_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(DroneAppearState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0008D5F0, EAX state, ESI actor
void fade_runtime_drone_appear(const DroneAppearState& state, RuntimeDroneAppearStateContext& context)
{
    float alpha;
    if (state.appear != 0)
    {
        const float level = state.time * kFadeRate * kOpaque;
        alpha = level > kOpaque ? kOpaque : 0.f > level ? 0.f : level;
    }
    else
    {
        const float level = kOpaque - state.time * kFadeRate * kOpaque;
        alpha = 0.f > level ? 0.f : level > kOpaque ? kOpaque : level;
    }
    const u8 value = u8(truncate_runtime_float(alpha)); // 0x001673D8
    context.color = (context.color & 0x00FFFFFFu) | (u32(value) << 24); // Actor +0x19B.
}

// Xbox 0x0008D690, EAX state, ECX actor
void scale_runtime_drone_appear(const DroneAppearState& state, Actor& actor)
{
    const float time = state.time;
    const float start = state.scale;
    float across;
    float up;
    if (state.appear != 0)
    {
        if (kAppearPop > time)
        {
            // The pop: tall and thin, settling toward wide.
            const float pop = time * kPopRate;
            const float tall = kTall - pop * kTallRate;
            up = kHalf > tall ? kHalf : tall > kTall ? kTall : tall;
            const float wide = pop * kWideRate + kThin;
            across = wide > kWide ? kWide : kThin > wide ? kThin : wide;
        }
        else
        {
            // Settling from (1.5, 0.5) toward the start scale.
            const float settle = (time - kAppearPop) * kSettleRate;
            const float tall = (start - kHalf) * settle + kHalf;
            if (start > kHalf)
            {
                up = tall > start ? start : kHalf > tall ? kHalf : tall;
            }
            else
            {
                up = start > tall ? start : tall > kHalf ? kHalf : tall;
            }
            const float wide = (start - kWide) * settle + kWide;
            if (start > kWide)
            {
                across = wide > start ? start : kWide > wide ? kWide : wide;
            }
            else
            {
                across = start > wide ? start : wide > kWide ? kWide : wide;
            }
        }
    }
    else if (time > kVanishPop)
    {
        // Popping away: thin and tall.
        const float pop = (time - kVanishPop) * kPopRate;
        const float tall = pop * kTallRate + kHalf;
        up = tall > kTall ? kTall : kHalf > tall ? kHalf : tall;
        const float wide = kWide - pop * kWideRate;
        across = kThin > wide ? kThin : wide > kWide ? kWide : wide;
    }
    else
    {
        // From the start scale toward (1.5, 0.5).
        const float settle = time * kSettleRate;
        const float tall = (kHalf - start) * settle + start;
        if (kHalf > start)
        {
            up = tall > kHalf ? kHalf : start > tall ? start : tall;
        }
        else
        {
            up = kHalf > tall ? kHalf : tall > start ? start : tall;
        }
        const float wide = (kWide - start) * settle + start;
        if (kWide > start)
        {
            across = wide > kWide ? kWide : start > wide ? start : wide;
        }
        else
        {
            across = kWide > wide ? kWide : wide > start ? start : wide;
        }
    }
    if (across != actor.scale_x || up != actor.radius_y || across != actor.radius_xz) // UCOMISS; a NaN differs
    {
        actor.scale_x = across;
        actor.radius_y = up;
        actor.radius_xz = across;
        actor.xform_valid = 0;
    }
}

// Xbox 0x0008D3B0
bool initialize_runtime_drone_appear_state(DroneAppearState& state, std::span<const u8> parameters,
    RuntimeDroneAppearStateContext& context)
{
    const std::span<u8> body = context.body;
    put_u16(body, 0x30, u16(get(body, body_field::move_holds) + 1));
    body[0x38] = 1;
    body[0x39] = 1;
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    state.time = kStartTime;
    state.appear = get(parameters, parameter::appear);
    state.scale = context.actor.radius_xz; // Actor +0x38.
    context.bits.bytes[1] = u8(context.bits.bytes[1] | 8);
    // Actor +0x8C is never null (the actor is not -0x8C): callout 0x15 appearing, 0x14 vanishing.
    dispatch_actor_attachment_records(context.resource.resource.data(),
        0x14 + (state.appear != 0 ? 1 : 0), nullptr, nullptr, context.callouts, 0); // 0x0003BE20
    if (state.appear != 0)
    {
        unregister_actor_segment_probes(context.render); // 0x0003E850, EAX actor
    }
    return true;
}

// Xbox 0x0008D440
void destroy_runtime_drone_appear_state(DroneAppearState& state, RuntimeDroneAppearStateContext& context)
{
    const std::span<u8> body = context.body;
    if (state.time > 0.f)
    {
        state.time = 0.f;
        fade_runtime_drone_appear(state, context); // 0x0008D5F0
        scale_runtime_drone_appear(state, context.actor); // 0x0008D690
    }
    decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
    put_u16(body, 0x30, u16(get(body, body_field::move_holds) - 1));
    context.bits.bytes[1] = u8(context.bits.bytes[1] & 0xF7);
    if (state.appear == 0)
    {
        actor_refresh_transform(context.actor); // 0x00040DE0
        const std::array<u32, 2> message{0x4A, 0};
        context.actor_message(message); // Actor vtable +0xC.
        return;
    }
    if (state.activated == 0)
    {
        activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI actor
        state.activated = 1u;
    }
}

// Xbox 0x0008D4D0
void tick_runtime_drone_appear_state(DroneAppearState& state, RuntimeDroneAppearStateContext& context)
{
    state.base.elapsed = runtime_actor_step(context.actor, context.clock) + state.base.elapsed;
    level_body_euler(context.body);
    bool done = false;
    const float time = state.time;
    if (time > 0.f)
    {
        const float left = time - runtime_actor_step(context.actor, context.clock);
        state.time = left;
        if (!(0.f < left)) // COMISS; JC keeps a positive time.
        {
            state.time = 0.f;
            done = true;
        }
    }
    fade_runtime_drone_appear(state, context); // 0x0008D5F0, EAX state, ESI actor
    scale_runtime_drone_appear(state, context.actor); // 0x0008D690, ECX actor
    if (!done)
    {
        return;
    }
    if (state.appear != 0)
    {
        activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI actor
        state.activated = 1u;
        return;
    }
    state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
}

void install_runtime_drone_appear_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDroneAppearStateContext&(u32 body_address)> context_of)
{
    drone_appear_context_of = std::move(context_of);
    types[0x65] = ObjectFactoryType{9, drone_appear_state_lifecycle}; // Getter 0x0008D350.
}
}
