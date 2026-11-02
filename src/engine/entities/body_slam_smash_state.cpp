#include "body_slam_smash_state.h"
#include "mind_fields.h"
#include "actor_render_registration.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "body_scoop_whack_state.h"
#include "player_mind_requests.h"
#include "../graphics/actor_effect_nodes.h"
#include "../navigation/navigation.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "body_fields.h"

#include <array>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = slam_smash_state_field;
namespace parameter = slam_smash_state_parameter;

constexpr float kCrouchTime = 0.5f; // DAT_0032B0F4, also the drop's time.
constexpr float kLeapTime = 0.7f; // DAT_0032B264.
constexpr float kLeapRate = 1.4285715f; // DAT_0032B2E0, 1 / 0.7.
constexpr float kDropRate = 2.f; // DAT_0032B110, also the drop's scale.
constexpr float kLeapHeight = 200.f; // DAT_0032B330.
constexpr float kAhead = 40.f; // DAT_0032B1B8.
constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kRetainForever = -1.f; // DAT_0032B120.
constexpr u32 kAidPrefix = 0x5F646961u; // "aid_".

std::function<RuntimeSlamSmashStateContext&(u32)> slam_smash_context_of;

// The three animation records the state found (0x0003AAA0), held natively beside the state's +0x20 .. +0x28 flags.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 3>> slam_smash_records;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The inlined 0x0003AAA0 pair (the attached actor's +0xDB0 variant first, then the plain name).
const AnimationRecord* find_animation(u32 name, RuntimeSlamSmashStateContext& context)
{
    return find_runtime_body_animation(context.image_string(name), context.actions);
}

// 0x0001AFA0 on one of the three records, when it was found.
void start_animation(std::span<const u8> state, size_t index, size_t offset, RuntimeSlamSmashStateContext& context)
{
    if (read_u32(state, offset) == 0)
    {
        return;
    }
    const auto found = slam_smash_records.find(state.data());
    const AnimationRecord* record = found == slam_smash_records.end() ? nullptr : found->second[index];
    if (record == nullptr)
    {
        throw Error("slam smash state starts an animation record it has no host view of");
    }
    start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ECX record, ESI body
}

// The actor +0x50 controller's slot 0 player pause count (+0x58 -> +0 -> +0x125) moves by `step` and the controller
// is marked (+0x5C).
void hold_animation(i32 step, RuntimeSlamSmashStateContext& context)
{
    AnimationController* controller = context.animation.controller.get();
    if (controller == nullptr)
    {
        return;
    }
    if (!controller->slots.empty() && controller->slots[0].player != nullptr)
    {
        controller->slots[0].player->paused = u8(controller->slots[0].player->paused + step);
    }
    controller->dirty = true;
}

void call_out(const Vec3& point, u32 id, RuntimeSlamSmashStateContext& context)
{
    call_out_runtime_hit_point(point, context.actor_address, id, context.actor_callout); // 0x0013AD60
}

// Xbox 0x000A0B50: the landing point is the mind's target (+0x10, its position +0x20 and floor +0x19), else 40
// ahead of where the state began along the actor's yaw (actor +0x18) on the actor's floor (+0xCEC); a cell of that
// floor under the point gives its height.
void choose_landing(std::span<u8> state, RuntimeSlamSmashStateContext& context)
{
    const u32 target = context.mind.empty() ? 0u : get(context.mind, mind_field::target_record);
    Vec3 landing{};
    u32 floor = 0;
    if (target != 0)
    {
        const std::span<const u8> record = context.target_at(target);
        landing = Vec3{read_f32(record, 0x20), read_f32(record, 0x24), read_f32(record, 0x28)};
        floor = record[0x19];
    }
    else
    {
        // 0x000412A0 writes the actor's position here first; the start overwrites it.
        landing = get(state, field::start);
        const float yaw = context.actor.euler.y; // Actor +0x18.
        landing.x = float(retail_sin(yaw) * kAhead + landing.x); // 0x00153320
        landing.z = float(retail_cos(yaw) * kAhead + landing.z); // 0x00153230
        floor = read_u32(context.actor_bytes, 0xCEC);
    }
    const i32 cell = entity_detail::navigation_cell_index(i32(floor), landing); // 0x00110EF0, EAX floor, EBX point
    if (cell != -1)
    {
        landing.y = float(entity_detail::g_nav_grids[floor]->cells.at(size_t(cell)).bottom); // DAT_0050E240
    }
    set(state, field::landing, landing);
}

// Xbox 0x000A09B0, ECX state, EAX phase
void set_phase(std::span<u8> state, u32 phase, RuntimeSlamSmashStateContext& context)
{
    set(state, field::phase, phase);
    switch (phase)
    {
    case 1:
        start_animation(state, 0, field::take_off_animation.offset, context);
        return;
    case 2:
        choose_landing(state, context); // 0x000A0B50
        start_animation(state, 1, field::crouch_animation.offset, context);
        set(state, field::timer, kCrouchTime);
        return;
    case 3:
    {
        const Vec3 point = actor_world_position(context.actor); // 0x000412A0
        call_out(point, 0x3F, context); // 0x0013AD60, EAX point, ECX actor
        set(state, field::timer, kLeapTime);
        set_runtime_body_shape_mode(0, context.shape); // 0x0001FEB0, BL 0, EDI body
        return;
    }
    case 4:
    {
        start_animation(state, 2, field::drop_animation.offset, context);
        hold_animation(1, context);
        set(state, field::paused, 1u);
        Actor& actor = context.actor;
        if (kDropRate != actor.scale_x || kDropRate != actor.radius_y || kDropRate != actor.radius_xz) // JP sets.
        {
            actor.scale_x = kDropRate; // +0x30.
            actor.radius_y = kDropRate; // +0x34.
            actor.radius_xz = kDropRate; // +0x38.
            actor.xform_valid = 0; // +0x2A8.
        }
        clear_actor_shape_probe(actor, context.view.shape); // 0x0003C700, ESI actor
        unregister_actor_segment_probes(context.view.render); // 0x0003E850, EAX actor
        if (context.collision != nullptr)
        {
            activate_runtime_body_contact_shapes(*context.collision, actor, get(state, field::shape_tag), 0xFF,
                get(state, field::shape_scale)); // 0x0013F420, XMM4 +0x30
            put_float(context.body, 0x2CC, 0.f);
        }
        set(state, field::shapes_active, 1u);
        set(state, field::timer, kCrouchTime);
        Vec3 landing = get(state, field::landing);
        landing.y = float(landing.y - actor_model_sweep_radius(actor)); // 0x0003D9A0, EDI actor
        set(state, field::landing, landing);
        return;
    }
    default:
        return;
    }
}

// The leap and the drop's share of their time, held to [0, 1].
float share(float timer, float rate)
{
    const float value = kOne - timer * rate;
    if (0.f > value)
    {
        return 0.f;
    }
    return value > kOne ? kOne : value;
}

void place_body(const Vec3& point, RuntimeSlamSmashStateContext& context)
{
    put_float(context.body, 0x18, point.x);
    put_float(context.body, 0x1C, point.y);
    put_float(context.body, 0x20, point.z);
}

bool construct_slam_smash_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_slam_smash_state(state, parameters, slam_smash_context_of(body_address_of(state)));
}

void destroy_slam_smash_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_slam_smash_state(state, slam_smash_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* slam_smash_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505B20, slam_smash_state_object_sizes,
        construct_slam_smash_state_view, destroy_slam_smash_state_view};
    return &table;
}
}

// Xbox 0x000A03B0
void slam_smash_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A0430
bool initialize_runtime_slam_smash_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSlamSmashStateContext& context)
{
    const auto body = context.body;
    auto& records = slam_smash_records[state.data()];
    records[0] = find_animation(get(parameters, parameter::take_off_animation), context);
    set(state, field::take_off_animation, records[0] != nullptr ? 1u : 0u);
    records[1] = find_animation(get(parameters, parameter::crouch_animation), context);
    set(state, field::crouch_animation, records[1] != nullptr ? 1u : 0u);
    records[2] = find_animation(get(parameters, parameter::drop_animation), context);
    set(state, field::drop_animation, records[2] != nullptr ? 1u : 0u);
    set(state, field::shape_tag, get(parameters, parameter::shape_tag));
    set(state, field::shape_scale, get(parameters, parameter::shape_scale));
    const auto source = get(parameters, parameter::effect);
    std::array<char, 0x80> effect{};
    std::strncpy(effect.data(), source.data(), effect.size() - 1); // The inlined strcpy.
    set(state, field::effect, effect);
    body[0x6A] = u8(body[0x6A] + 1);
    body[0x38] = 1;
    body[0x39] = 1;
    set(state, field::shape_mode, u32(set_runtime_body_shape_mode(4, context.shape))); // 0x0001FEB0, BL 4
    const u32 holds = get(body, body_field::holds) + 1;
    std::memcpy(body.data() + 0x258, &holds, 4);
    set(state, field::start, actor_world_position(context.actor)); // 0x000412A0
    set(state, field::phase, 1u);
    start_animation(state, 0, field::take_off_animation.offset, context);
    return true;
}

// Xbox 0x000A05A0
void destroy_runtime_slam_smash_state(std::span<u8> state, RuntimeSlamSmashStateContext& context)
{
    const auto body = context.body;
    body[0x6A] = u8(body[0x6A] - 1);
    set_runtime_body_shape_mode(u8(get(state, field::shape_mode)), context.shape); // 0x0001FEB0, BL +0x3C
    const u32 holds = get(body, body_field::holds) - 1;
    std::memcpy(body.data() + 0x258, &holds, 4);
    if (holds == 0)
    {
        std::array<u32, 0x42> message{0x4A};
        context.body_message(message); // Body vtable +0xC.
    }
    if (get(state, field::shapes_active) != 0)
    {
        if (context.collision != nullptr)
        {
            deactivate_runtime_body_contact_shapes(*context.collision, get(state, field::shape_tag)); // 0x0013F4D0
        }
        set(state, field::shapes_active, 0u);
    }
    if (get(state, field::paused) != 0)
    {
        hold_animation(-1, context);
        set(state, field::paused, 0u);
    }
    slam_smash_records.erase(state.data());
}

// Xbox 0x000A0630
void tick_runtime_slam_smash_state(std::span<u8> state, RuntimeSlamSmashStateContext& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    set(state, body_state_field::elapsed, rate * step + get(state, body_state_field::elapsed));
    put_float(context.body, 0x734, 0.f);
    put_float(context.body, 0x72C, 0.f);
    float timer = get(state, field::timer);
    if (0.f < timer)
    {
        timer = timer - rate * step;
        if (!(0.f < timer)) // COMISS; JC keeps it.
        {
            timer = 0.f;
        }
        set(state, field::timer, timer);
    }
    const Vec3 start = get(state, field::start);
    const Vec3 landing = get(state, field::landing);
    switch (get(state, field::phase))
    {
    case 1:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0, EAX actor
        {
            set_phase(state, 2, context); // The inlined 0x000A09B0, EAX 2.
        }
        return;
    case 2:
        if (get(state, field::timer) == 0.f) // UCOMISS; JP keeps the crouch.
        {
            set_phase(state, 3, context); // The inlined 0x000A09B0, EAX 3.
        }
        return;
    case 3:
    {
        const float t = share(get(state, field::timer), kLeapRate);
        place_body(Vec3{(landing.x - start.x) * t + start.x,
                       t * t * t * ((landing.y + kLeapHeight) - start.y) + start.y,
                       (landing.z - start.z) * t + start.z},
            context);
        if (get(state, field::timer) == 0.f)
        {
            set_phase(state, 4, context); // 0x000A09B0, EAX 4, ECX state
        }
        return;
    }
    case 4:
    {
        const float t = share(get(state, field::timer), kDropRate);
        const float above = landing.y + kLeapHeight;
        place_body(Vec3{(landing.x - landing.x) * t + landing.x, (landing.y - above) * t + above,
                       (landing.z - landing.z) * t + landing.z},
            context);
        if (get(state, field::timer) == 0.f)
        {
            set(state, field::phase, 5u);
        }
        return;
    }
    case 5:
    {
        const Vec3 point = actor_world_position(context.actor); // 0x000412A0
        if (read_u32(state, field::effect.offset) == kAidPrefix)
        {
            RuntimeActorCreationOptions& options = g_actor_creation_options; // DAT_004E1C5C.
            options.position = point;
            options.flags = 1;
            if (load_actor_effect_stream(field_data(state, field::effect), &options, context.effects) != 0) // 0x00053170
            {
                retain_last_actor_effect(kRetainForever); // 0x0013E750, XMM0 DAT_0032B120
            }
        }
        call_out(point, 0x4C, context); // 0x0013AD60, EAX point, ECX actor
        request_runtime_body_death_effect(1, context.death); // 0x00024890, EAX body
        return;
    }
    default:
        return;
    }
}

void install_runtime_slam_smash_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSlamSmashStateContext&(u32 body_address)> context_of)
{
    slam_smash_context_of = std::move(context_of);
    types[0x82] = ObjectFactoryType{9, slam_smash_state_lifecycle}; // Getter 0x000A03D0.
}
}
