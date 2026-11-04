#include "body_yoyo_spin_state.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_mind_requests.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = yoyo_spin_state_parameter;

std::function<RuntimeYoYoSpinStateContext&(u32)> yoyo_spin_context_of;

// The host halves of state +0x24 / +0x28 / +0x2C, by the state's bytes.
struct YoYoSpinAnimations
{
    const AnimationRecord* windup = nullptr;
    const AnimationRecord* throw_animation = nullptr;
    const AnimationRecord* catch_animation = nullptr;
};
std::unordered_map<const u8*, YoYoSpinAnimations> yoyo_spin_animations;

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void copy_name(char* output, const char* source)
{
    char character;
    do
    {
        character = *source++;
        *output++ = character;
    } while (character != 0);
}

// The throw's sound slots (+0x83C and +0x3EC) stop while phase 2 runs.
void release_throw_sounds(const YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    if (state.phase == 2)
    {
        release_actor_sound(context.actor.sound_slots[7], context.sounds); // 0x0012A3D0, ESI actor +0x83C
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
    }
}

// The inlined 0x0003C040: the actor's records (+0x8C) for `id`, unless the actor word is -0x8C.
void call_out_actor(u32 actor, u32 id, RuntimeYoYoSpinStateContext& context)
{
    if (actor + 0x8Cu == 0)
    {
        return;
    }
    context.actor_callout(actor, id, nullptr); // 0x0003BE20
}

// Phase 3's contact shapes and its 0.5 s hold.
void begin_hold(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    request_actor_sound_at(context.actor, 7, state.hold_sound.data(), nullptr, nullptr, 1.f,
        context.sounds); // 0x00041D40, ECX 7
    RuntimeBodyStateContext shape = context.state();
    set_runtime_body_shape_mode(0, shape); // 0x0001FEB0, BL 0, EDI body
    if (context.collision != nullptr) // Body +0x2C8.
    {
        activate_runtime_body_contact_shapes(*context.collision, context.actor, state.shape_tag, 0xFF,
            state.shape_scale); // 0x0013F420, XMM4 +0x1B4
        std::memset(context.body.data() + 0x2CC, 0, 4);
    }
    state.shapes_active = 1u;
    state.timer = .5f; // DAT_0032B0F4.
}

// Xbox 0x000A72E0, EBX state: the aim is the mind target's point raised by half its height (+0x30), else 40 ahead
// of the throw's start.
void aim_runtime_yoyo_spin(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    const std::span<const u8> mind = context.mind;
    if (mind.data() != nullptr && get(mind, mind_field::target_record) != 0)
    {
        const std::span<const u8> target = context.target_at(get(mind, mind_field::target_record));
        Vec3 aim{get(target, mind_target_field::shape_position).x, get(target, mind_target_field::shape_position).y, get(target, mind_target_field::shape_position).z};
        aim.y = get(target, mind_target_field::shape_height) * .5f + aim.y; // DAT_0032B0F4.
        state.aim = aim;
        return;
    }
    actor_world_position(context.actor); // 0x000412A0 into +0x1D0, overwritten next.
    Vec3 aim = state.start;
    const float heading = context.actor.euler.y; // Actor +0x18.
    const long double across = retail_sin(heading); // 0x00153320
    const long double along = retail_cos(heading); // 0x00153230
    aim.x = float(static_cast<long double>(float(across)) * 40.0L + aim.x); // DAT_0032B1B8.
    aim.z = float(along * 40.0L + aim.z);
    state.aim = aim;
}

// Xbox 0x000A7240, ESI state, EDI body: body +0x18 eases from the start to the aim by (sin(blend pi - pi/2) + 1) / 2.
void place_runtime_yoyo_spin_point(const YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    const float angle = float(static_cast<long double>(state.blend) * std::bit_cast<float>(0x40490FDBu)
        - std::bit_cast<float>(0x3FC90FDBu)); // DAT_0032B148, DAT_0032B140.
    const float weight = float((retail_sin(angle) + 1.0L) * .5L); // 0x00153320, DAT_0032B0F8, DAT_0032B0F4
    const Vec3 start = state.start;
    const Vec3 aim = state.aim;
    put_float(context.body, 0x18, (aim.x - start.x) * weight + start.x);
    put_float(context.body, 0x1C, (aim.y - start.y) * weight + start.y);
    put_float(context.body, 0x20, (aim.z - start.z) * weight + start.z);
}

// Xbox 0x000A70C0
void change_runtime_yoyo_spin_phase(YoyoSpinState& state, u32 phase, RuntimeYoYoSpinStateContext& context)
{
    const YoYoSpinAnimations& animations = yoyo_spin_animations.at(body_state_key(state));
    release_throw_sounds(state, context);
    state.phase = phase;
    const AnimationRecord* record = nullptr;
    switch (phase)
    {
    case 1:
        record = animations.windup;
        break;
    case 2:
        request_actor_sound_at(context.actor, 1, state.throw_sound.data(), nullptr, nullptr, 1.f,
            context.sounds); // 0x00041D40, ECX 1
        request_actor_sound_at(context.actor, 7, state.spin_sound.data(), nullptr, nullptr, 1.f,
            context.sounds); // 0x00041D40, ECX 7
        aim_runtime_yoyo_spin(state, context); // 0x000A72E0, EBX state
        if (animations.throw_animation != nullptr)
        {
            start_runtime_body_animation_record(*animations.throw_animation, context.actions); // 0x0001AFA0
        }
        state.timer = .5f; // DAT_0032B0F4.
        return;
    case 3:
        begin_hold(state, context);
        return;
    case 4:
        state.timer = .5f - state.timer; // DAT_0032B0F4.
        return;
    case 5:
        record = animations.catch_animation;
        break;
    default:
        return;
    }
    if (record != nullptr)
    {
        start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ESI body
    }
}

// Phase 3 cut short (messages 0x41 and 0x70, the body's bit 0x15): the wind-back takes what is left of the hold.
void cut_hold_short(YoyoSpinState& state)
{
    if (state.phase == 3)
    {
        state.phase = 4u;
        state.timer = .5f - state.timer; // DAT_0032B0F4.
    }
}

bool construct_yoyo_spin_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    YoyoSpinState& state = body_state_view<YoyoSpinState>({object.bytes.get(), object.size});
    return initialize_runtime_yoyo_spin_state(state, parameters, yoyo_spin_context_of(state.base.body));
}

void destroy_yoyo_spin_state_view(FactoryObject& object)
{
    YoyoSpinState& state = body_state_view<YoyoSpinState>({object.bytes.get(), object.size});
    destroy_runtime_yoyo_spin_state(state, yoyo_spin_context_of(state.base.body));
}

// Getter 0x000A6A00: +0x14 / +0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620.
const ObjectFactoryVtable* yoyo_spin_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505CE0, yoyo_spin_state_object_sizes, construct_yoyo_spin_state_view,
        destroy_yoyo_spin_state_view};
    return &table;
}
}

// Xbox 0x000A69E0
void yoyo_spin_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(YoyoSpinState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000A6A60
bool initialize_runtime_yoyo_spin_state(YoyoSpinState& state, std::span<const u8> parameters,
    RuntimeYoYoSpinStateContext& context)
{
    const std::span<u8> body = context.body;
    state.shape_tag = get(parameters, parameter::shape_tag);
    state.shape_scale = get(parameters, parameter::shape_scale);
    state.callout = get(parameters, parameter::callout);
    // The inlined 0x0003AAA0 pairs over the body's held actor (+0x48) +0xDB0 variant.
    YoYoSpinAnimations& animations = yoyo_spin_animations[body_state_key(state)];
    animations.windup = find_runtime_body_animation(context.image_string(get(parameters, parameter::windup)),
        context.actions);
    state.windup = animations.windup != nullptr ? 1u : 0u;
    animations.throw_animation = find_runtime_body_animation(
        context.image_string(get(parameters, parameter::throw_animation)), context.actions);
    state.throw_animation = animations.throw_animation != nullptr ? 1u : 0u;
    animations.catch_animation = find_runtime_body_animation(
        context.image_string(get(parameters, parameter::catch_animation)), context.actions);
    state.catch_animation = animations.catch_animation != nullptr ? 1u : 0u;
    copy_name(state.throw_sound.data(), field_data(parameters, parameter::throw_sound));
    copy_name(state.spin_sound.data(), field_data(parameters, parameter::spin_sound));
    copy_name(state.hold_sound.data(), field_data(parameters, parameter::hold_sound));
    body[0x6A] = u8(body[0x6A] + 1);
    body[0x38] = 1;
    body[0x39] = 1;
    RuntimeBodyStateContext shape = context.state();
    state.shape_mode = u32(set_runtime_body_shape_mode(4, shape)); // 0x0001FEB0, BL 4, EDI body
    state.start = actor_world_position(context.actor); // 0x000412A0, EDI state +0x1C4
    release_throw_sounds(state, context);
    state.phase = 1u;
    if (animations.windup != nullptr)
    {
        start_runtime_body_animation_record(*animations.windup, context.actions); // 0x0001AFA0, ESI body
    }
    return true;
}

// Xbox 0x000A6C40
void destroy_runtime_yoyo_spin_state(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    const std::span<u8> body = context.body;
    body[0x6A] = u8(body[0x6A] - 1);
    RuntimeBodyStateContext shape = context.state();
    set_runtime_body_shape_mode(u8(state.shape_mode), shape); // 0x0001FEB0, BL state +0x1BC
    if (state.shapes_active != 0)
    {
        if (context.collision != nullptr) // Body +0x2C8.
        {
            deactivate_runtime_body_contact_shapes(*context.collision, state.shape_tag); // 0x0013F4D0
        }
        state.shapes_active = 0u;
    }
    release_throw_sounds(state, context);
    state.phase = 0u;
    yoyo_spin_animations.erase(body_state_key(state));
}

// Xbox 0x000A6CC0
void tick_runtime_yoyo_spin_state(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context)
{
    const std::span<u8> body = context.body;
    state.base.elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    level_body_euler(body);
    const float timer = state.timer;
    if (0.f < timer)
    {
        const float left = timer - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
        state.timer = left;
        if (0.f >= left)
        {
            state.timer = 0.f;
        }
    }
    switch (state.phase)
    {
    case 1:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            change_runtime_yoyo_spin_phase(state, 2, context); // 0x000A70C0
        }
        break;
    case 2:
        if (state.timer == 0.f)
        {
            release_throw_sounds(state, context);
            state.phase = 3u;
            begin_hold(state, context);
        }
        break;
    case 3:
    {
        float blend = 1.f - state.timer * 2.f; // DAT_0032B0F8, DAT_0032B110.
        if (0.f > blend)
        {
            blend = 0.f;
        }
        else if (blend > 1.f)
        {
            blend = 1.f;
        }
        state.blend = blend;
        if ((context.bits.bytes[2] & 0x20) != 0) // Body +0x15C -> +8, bit 0x15.
        {
            call_out_actor(context.actor_address, 0xE, context); // 0x0003C040, ECX actor, EDX 0xE
            release_throw_sounds(state, context);
            state.timer = .5f - state.timer; // DAT_0032B0F4.
            state.phase = 4u;
        }
        else if (state.timer == 0.f)
        {
            state.phase = 4u;
            state.timer = .5f;
        }
        break;
    }
    case 4:
    {
        float blend = state.timer * 2.f; // DAT_0032B110.
        if (blend > 1.f)
        {
            blend = 1.f;
        }
        else if (0.f > blend)
        {
            blend = 0.f;
        }
        state.blend = blend;
        if (state.timer == 0.f)
        {
            release_throw_sounds(state, context);
            state.phase = 5u;
            const AnimationRecord* record = yoyo_spin_animations.at(body_state_key(state)).catch_animation;
            if (record != nullptr)
            {
                start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ESI body
            }
        }
        break;
    }
    case 5:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
        }
        break;
    default:
        break;
    }
    place_runtime_yoyo_spin_point(state, context); // 0x000A7240, ESI state, EDI body
}

// Xbox 0x000A7030
void message_runtime_yoyo_spin_state(YoyoSpinState& state, std::span<const u32> message,
    RuntimeYoYoSpinStateContext& context)
{
    switch (message[0])
    {
    case 0x25:
        select_runtime_body_action(context.body, context.body_slot_key(0x20), context.actions); // Vtable +0x20,
                                                                                               // 0x00027650.
        return;
    case 0x41:
    {
        const Vec3 point{std::bit_cast<float>(message[7]), std::bit_cast<float>(message[8]),
            std::bit_cast<float>(message[9])};
        call_out_runtime_hit_point(point, message[5], state.callout, context.actor_callout); // 0x0013AD60
        cut_hold_short(state);
        return;
    }
    case 0x70:
        if (message[2] != context.actor_address)
        {
            return;
        }
        call_out_actor(message[1], state.callout, context); // 0x0003C040, ECX message +4
        cut_hold_short(state);
        return;
    default:
        return;
    }
}

void install_runtime_yoyo_spin_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeYoYoSpinStateContext&(u32 body_address)> context_of)
{
    yoyo_spin_context_of = std::move(context_of);
    types[0x90] = ObjectFactoryType{9, yoyo_spin_state_lifecycle}; // Getter 0x000A6A00.
}
}
