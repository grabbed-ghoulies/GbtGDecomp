#include "body_hover_state.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include "body_fields.h"

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = hover_state_parameter;

std::function<RuntimeHoverStateContext&(u32)> hover_context_of;
// The hover animation records (+0x20), by the state's bytes.
std::unordered_map<const u8*, const AnimationRecord*> hover_animations;

constexpr float kTau = 6.2831854820251465f; // DAT_0032B298.

const RuntimeDialogueInputDevice& input_of(RuntimeHoverStateContext& context)
{
    if (context.input == nullptr)
    {
        throw Error("0x00097DC0 reads the hover's input record through address zero");
    }
    return *context.input;
}

bool construct_hover_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    HoverState& state = body_state_view<HoverState>({object.bytes.get(), object.size});
    return initialize_runtime_hover_state(state, parameters, hover_context_of(state.base.body));
}

void destroy_hover_state_view(FactoryObject& object)
{
    HoverState& state = body_state_view<HoverState>({object.bytes.get(), object.size});
    destroy_runtime_hover_state(state, hover_context_of(state.base.body));
    hover_animations.erase(body_state_key(state));
}

const ObjectFactoryVtable* hover_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005059C0, hover_state_object_sizes, construct_hover_state_view,
        destroy_hover_state_view};
    return &table;
}
}

// Xbox 0x00097B00
void hover_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(HoverState);
    output.parameters = parameter::kSize;
}

// Xbox 0x00097B80
bool initialize_runtime_hover_state(HoverState& state, std::span<const u8> parameters,
    RuntimeHoverStateContext& context)
{
    const auto body = context.body;
    context.bits.bytes[6] |= 1;
    // DAT_0032B484, DAT_0032B63C, DAT_0032B218.
    body_record(body).turn_rates = {1.0471975803375244f, 2.268928050994873f, 6.f};
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
   body_record(body).unk_160 = 0;
    state.next = get(parameters, parameter::next);
    state.next_with_target = get(parameters, parameter::next_with_target);
    state.turns = get(parameters, parameter::turns);
    // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
    const AnimationRecord* record = find_runtime_body_animation(context.image_string(get(parameters,
        parameter::animation)), context.actions);
    hover_animations[body_state_key(state)] = record;
    state.animation = record != nullptr ? 1u : 0u;
    if (record != nullptr)
    {
        start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ECX the record
    }
    return true;
}

// Xbox 0x00097C60
void destroy_runtime_hover_state(const HoverState& state, RuntimeHoverStateContext& context)
{
    const auto body = context.body;
    // DAT_0032B220, DAT_0032B21C, DAT_0032B218.
    body_record(body).turn_rates = {0.5585053563117981f, 12.566370964050293f, 6.f};
    context.bits.bytes[6] &= 0xFE;
    if (state.animation == 0)
    {
        return;
    }
    const AnimationRecord* record = hover_animations[body_state_key(state)];
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    const i32 slot = record == nullptr ? 0 : i32(record->slot); // Record +0x117.
    if (controller == nullptr || slot == 0)
    {
        return;
    }
    AnimationControllerSlot& entry = controller->slots.at(size_t(slot));
    if (entry.player != nullptr)
    {
        entry.fade_duration = 0.25f; // +0x1C, DAT_0032B104.
        entry.fade_mode = 3; // +0x18.
    }
    controller->dirty = true; // +0x5C.
}

// Xbox 0x00097CF0
void tick_runtime_hover_state(HoverState& state, RuntimeHoverStateContext& context)
{
    const auto body = context.body;
    state.base.elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    const auto mind = context.mind;
    if (mind.data() != nullptr && mind_record(mind).target_mode != 0 && mind_record(mind).target != 0)
    {
        const Actor* chosen = context.actor_at(mind_record(mind).target);
        if (chosen == nullptr)
        {
            throw Error("0x00097CF0 reads the position of a chosen actor outside the scene");
        }
        const Vec3 point = actor_world_position(*chosen); // 0x000412A0, ESI the actor, EDI body +0x3C
       body_record(body).focus_point.x = point.x;
       body_record(body).focus_point.y = point.y;
       body_record(body).focus_point.z = point.z;
    }
    if (body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f &&
        state.turns != 0)
    {
        turn_runtime_hover_state(context); // 0x00097E60, EAX actor
    }
    check_runtime_hover_stick(state, context); // 0x00097DC0, ESI input
}

// Xbox 0x00097DC0, ESI input, EBX state
void check_runtime_hover_stick(HoverState& state, RuntimeHoverStateContext& context)
{
    std::array<float, 2> axes{};
    runtime_input_axes(input_of(context), axes, context.input_state); // 0x001085F0
    if (!(axes[1] * axes[1] + axes[0] * axes[0] > 0.01f)) // DAT_0032B22C.
    {
        runtime_input_tertiary_axes(input_of(context), axes, context.input_state); // 0x00108AB0
        if (!(std::fabs(axes[1]) > 0.1f)) // 0x00152BB0, DAT_0032B254.
        {
            return;
        }
    }
    const u16 with_target = state.next_with_target;
    if (with_target != 0 && context.mind.data() != nullptr && mind_record(context.mind).target_mode != 0)
    {
        state.base.next_key = with_target;
        return;
    }
    state.base.next_key = state.next;
}

// Xbox 0x00097E60, EAX actor
void turn_runtime_hover_state(RuntimeHoverStateContext& context)
{
    const auto body = context.body;
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 direction{body_record(body).focus_point.x - position.x,
                         body_record(body).focus_point.y - position.y,
                         body_record(body).focus_point.z - position.z};
    float angle = 0.f; // 0x00105F90 leaves the slot unwritten for a zero direction.
    runtime_direction_angle(direction, angle); // 0x00105F90
    // The 0x00152BE0 remainder into [-pi, pi): the x87 remainder is compared, its float kept.
    const long double remainder = animation_remainder(angle - context.actor.euler.y, kTau); // Actor +0x18.
    float off = float(remainder);
    if (static_cast<long double>(-3.1415927410125732f) > remainder) // DAT_0032B29C.
    {
        off += kTau;
    }
    if (off >= 3.1415927410125732f) // DAT_0032B148.
    {
        off -= kTau;
    }
    level_body_euler(body);
    if (std::fabs(off) > 0.8726646304130554f) // 0x00152BB0, DAT_0032B478.
    {
       body_record(body).euler.y = angle;
    }
}

void install_runtime_hover_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeHoverStateContext&(u32 body_address)> context_of)
{
    hover_context_of = std::move(context_of);
    types[0x77] = ObjectFactoryType{9, hover_state_lifecycle}; // Getter 0x00097B20.
}
}
