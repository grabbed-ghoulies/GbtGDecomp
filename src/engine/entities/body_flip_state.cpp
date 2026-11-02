#include "body_flip_state.h"
#include "player_goal_cleanup.h"
#include "body_fields.h"

#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = flip_state_field;
namespace parameter = flip_state_parameter;

std::function<RuntimeFlipStateContext&(u32)> flip_context_of;
// The landing animation records (+0x2C), by the state's bytes.
std::unordered_map<const u8*, const AnimationRecord*> flip_landings;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_counters(std::span<u8> body, i32 step)
{
    for (size_t offset : {0x309u, 0x30Au, 0x30Bu, 0x30Cu})
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Body +0x15C bit 0x29 (byte 5 bit 1): the body stands on the ground.
bool on_ground(const RuntimeFlipStateContext& context)
{
    return (context.bits.bytes[5] & 2) != 0;
}

bool construct_flip_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_flip_state(state, parameters, flip_context_of(body_address_of(state)));
}

void destroy_flip_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_flip_state(state, flip_context_of(body_address_of(state)));
    flip_landings.erase(state.data());
}

const ObjectFactoryVtable* flip_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505840, flip_state_object_sizes, construct_flip_state_view,
        destroy_flip_state_view};
    return &table;
}
}

// Xbox 0x00090410
void flip_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x00090490
bool initialize_runtime_flip_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeFlipStateContext& context)
{
    const auto body = context.body;
    // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
    const AnimationRecord* landing = find_runtime_body_animation(context.image_string(get(parameters,
        parameter::landing)), context.actions);
    flip_landings[state.data()] = landing;
    set(state, field::landing, landing != nullptr ? 1u : 0u);
    set(state, field::steer_speed, get(parameters, parameter::steer_speed));
    set(state, field::phase, 1u);
    release_runtime_player_attachment(0, 0, context.attachment); // 0x0001B810, EAX body
    body[0x38] = 1;
    body[0x39] = 1;
    step_counters(body, 1);
    body[0x6B] = u8(body[0x6B] + 1);
    context.actor.anchor.y += 115.f; // Actor +0x40, DAT_0032B5AC.
    set(state, field::gravity, get(body, body_field::gravity));
    set(body, body_field::gravity, -200.f); // DAT_0032B224.
    increment_runtime_body_latch_mask(body, 1, context.actor, context.latches); // The inlined 0x0001F3F0, byte 0.
    set(state, field::latched, 1u);
    return true;
}

// Xbox 0x00090650
void destroy_runtime_flip_state(std::span<u8> state, RuntimeFlipStateContext& context)
{
    const auto body = context.body;
    body[0x6B] = u8(body[0x6B] - 1);
    set(body, body_field::gravity, get(state, field::gravity));
    step_counters(body, -1);
    if (get(state, field::latched) != 0)
    {
        decrement_runtime_body_latch_mask(body, 1, context.actor, context.latches); // The inlined 0x0001F2F0.
        set(state, field::latched, 0u);
    }
}

// Xbox 0x000908D0, ECX input, ESI body
void steer_runtime_flip_state(std::span<const u8> state, RuntimeFlipStateContext& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{};
    if (context.input != nullptr && get(body, body_field::move_holds) == 0 &&
        get(body, body_field::move_hold_time) == 0.f)
    {
        runtime_input_axes(*context.input, axes, context.input_state); // 0x001085F0
    }
    float magnitude = float(retail_square_root(axes[0] * axes[0] + axes[1] * axes[1])); // 0x00152B30
    if (!(1.f > magnitude)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float heading = 0.f;
    if (axes[0] != 0.f || axes[1] != 0.f)
    {
        heading = float(retail_atan2(axes[1], axes[0])); // 0x001530E0
    }
    float across = 0.f;
    float along = 0.f;
    if (magnitude > 0.f)
    {
        const float speed = magnitude * get(state, field::steer_speed);
        across = float(retail_sin(heading) * speed); // 0x00153320
        along = float(retail_cos(heading) * speed); // 0x00153230
    }
    put_float(body, 0x318, across);
    put_float(body, 0x31C, 0.f);
    put_float(body, 0x320, along);
    level_body_euler(body);
}

// Xbox 0x00090760
void tick_runtime_flip_state(std::span<u8> state, RuntimeFlipStateContext& context)
{
    const auto body = context.body;
    set(state, body_state_field::elapsed, runtime_actor_rate(context.actor) * runtime_frame_step(context.clock)
        + get(state, body_state_field::elapsed));
    const u32 phase = get(state, field::phase);
    if (phase == 1)
    {
        if (get(state, field::airborne) == 0 && !on_ground(context))
        {
            set(state, field::airborne, 1u);
        }
        steer_runtime_flip_state(state, context); // 0x000908D0
        if (get(state, field::airborne) == 0 || !on_ground(context))
        {
            return;
        }
        if (get(state, field::latched) != 0)
        {
            decrement_runtime_body_latch_mask(body, 1, context.actor, context.latches); // 0x0001F2F0, ECX body
            set(state, field::latched, 0u);
        }
        const AnimationRecord* landing = flip_landings[state.data()];
        if (landing == nullptr)
        {
            throw Error("0x0001AFA0 reads +0x117 of the flip's landing animation, which 0x0003AAA0 did not find");
        }
        start_runtime_body_animation_record(*landing, context.actions); // 0x0001AFA0, ECX +0x2C
        set(state, field::phase, 2u);
        return;
    }
    if (phase != 2)
    {
        return;
    }
    put_float(body, 0x318, context.default_position.x);
    put_float(body, 0x31C, context.default_position.y);
    put_float(body, 0x320, context.default_position.z);
    level_body_euler(body);
    const AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    const MinaPlayer* player = controller == nullptr || controller->slots.empty() ? nullptr
                                                                                  : controller->slots[0].player.get();
    if (player != nullptr)
    {
        bool finished = false;
        if (player->mode == 1)
        {
            finished = 0.f > player->speed ? !(0.f < player->position) : !(player->position < 1.f); // DAT_0032B0F8.
        }
        if (!finished)
        {
            return;
        }
    }
    select_runtime_body_recovery_action(state, 0, context.recovery); // 0x00133F50, EAX body
}

void install_runtime_flip_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFlipStateContext&(u32 body_address)> context_of)
{
    flip_context_of = std::move(context_of);
    types[0x6B] = ObjectFactoryType{9, flip_state_lifecycle}; // Getter 0x00090430.
}
}
