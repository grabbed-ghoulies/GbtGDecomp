#include "body_shocked_state.h"
#include "player_goal_cleanup.h"
#include "gbtg/object_factory.h"
#include "body_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace parameter = shocked_state_parameter;

constexpr float kLatchHold = std::bit_cast<float>(0x3E99999Au); // 0.3 s.

std::function<RuntimeShockedStateContext&(u32)> shocked_context_of;

void step_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

void step_half(std::span<u8> body, size_t offset, i32 step)
{
    const u16 value = u16(read_u16(body, offset) + step);
    std::memcpy(body.data() + offset, &value, 2);
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

bool construct_shocked_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    ShockedState& state = body_state_view<ShockedState>({object.bytes.get(), object.size});
    return initialize_runtime_shocked_state(state, parameters, shocked_context_of(state.base.body));
}

void destroy_shocked_state_view(FactoryObject& object)
{
    ShockedState& state = body_state_view<ShockedState>({object.bytes.get(), object.size});
    destroy_runtime_shocked_state(state, shocked_context_of(state.base.body));
}

const ObjectFactoryVtable* shocked_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505AE0, shocked_state_object_sizes, construct_shocked_state_view,
        destroy_shocked_state_view};
    return &table;
}
}

// Xbox 0x0009F400
void shocked_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(ShockedState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0009F480
bool initialize_runtime_shocked_state(ShockedState& state, std::span<const u8> parameters,
    RuntimeShockedStateContext& context)
{
    const auto body = context.body;
    release_runtime_player_attachment(0, 0, context.attachment); // 0x0001B810, EAX body
    body[0x6B] = u8(body[0x6B] + 1);
    step_counters(body, 1);
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    step_half(body, 0x30, 1);
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[2] |= 4;
    auto clone = clone_executable_resource(context.resources,
        field_data(parameters, parameter::effect)); // 0x00107720, EAX parameters +0xC
    if (clone.bytes == nullptr)
    {
        state.effect = 0u;
        return true;
    }
    std::memcpy(clone.bytes.get() + 8, &context.actor_address, 4);
    const u32 effect = context.create_object({clone.bytes.get(), clone.size}); // 0x00107520
    clone.bytes.reset(); // 0x000663E0
    state.effect = effect;
    return true;
}

// Xbox 0x0009F550
void destroy_runtime_shocked_state(ShockedState& state, RuntimeShockedStateContext& context)
{
    const auto body = context.body;
    step_half(body, 0x30, -1);
    body[0x6B] = u8(body[0x6B] - 1);
    decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
    hold_runtime_body_latch_timer(body, kLatchHold, context.actor, context.latches); // 0x0001F4F0, ESI body
    step_counters(body, -1);
    context.bits.bytes[2] &= 0xFB;
    const u32 effect = state.effect;
    if (effect != 0)
    {
        context.release_frame_object(context.actor, effect); // 0x0003DE30, ECX actor, ESI effect
        state.effect = 0u;
    }
}

// Xbox 0x0009F5F0
void tick_runtime_shocked_state(ShockedState& state, RuntimeShockedStateContext& context)
{
    const auto body = context.body;
    state.base.elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    hold_runtime_body_latch_timer(body, kLatchHold, context.actor, context.latches); // 0x0001F4F0, ESI body
    level_body_euler(body);
    const MinaPlayer* player = first_player(context.animation); // Actor +0x50 -> +0x58 -> [0].
    if (player != nullptr)
    {
        bool finished = false;
        if (player->mode == 1)
        {
            bool running;
            if (0.f > player->speed)
            {
                running = 0.f < player->position;
            }
            else
            {
                running = player->position < 1.f; // DAT_0032B0F8.
            }
            finished = !running;
        }
        if (!finished)
        {
            return;
        }
    }
    select_runtime_body_recovery_action(state.base, 0, context.recovery); // 0x00133F50
}

void install_runtime_shocked_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeShockedStateContext&(u32 body_address)> context_of)
{
    shocked_context_of = std::move(context_of);
    types[0x80] = ObjectFactoryType{9, shocked_state_lifecycle}; // Getter 0x0009F420.
}
}
