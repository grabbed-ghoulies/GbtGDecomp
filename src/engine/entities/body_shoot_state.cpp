#include "body_shoot_state.h"
#include "player_fields.h"
#include <cstddef>
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "body_gaze.h"
#include "direction_pitch.h"
#include "player_mind_events.h"
#include "player_mind_requests.h"
#include "../camera/entity_camera.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <array>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = shoot_state_parameter;

std::function<RuntimeShootStateContext&(u32)> shoot_context_of;
// The four animation records (+0x28..+0x34), by the state's bytes.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 4>> shoot_animations;

constexpr float kDegree = 0.01745329238474369f; // DAT_0032B0FC.

// The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
const AnimationRecord* find_animation(std::span<const u8> parameters, u32 offset, RuntimeShootStateContext& context)
{
    return find_runtime_body_animation(context.image_string(read_u32(parameters, offset)), context.actions);
}

// 0x0001AFA0 with ECX the record.
void start_animation(const AnimationRecord* record, RuntimeShootStateContext& context)
{
    if (record == nullptr)
    {
        throw Error("shoot state animation record is null; 0x0001AFA0 reads its +0x117");
    }
    start_runtime_body_animation_record(*record, context.actions);
}

// Actor +0x50's controller: slot 2's weight follows +0x40 when it plays, and the controller is marked.
void weigh_aim_slot(const ShootState& state, u32 dirty, RuntimeShootStateContext& context)
{
    AnimationController* controller = context.actions.animation.controller.get();
    if (controller == nullptr)
    {
        return;
    }
    if (controller->slots.size() > 2 && controller->slots[2].player != nullptr)
    {
        controller->slots[2].weight = state.weight; // Slot 2 +0x20.
    }
    controller->dirty = dirty != 0; // +0x5C.
}

// The aim's place between the parameters' pitch limits: 1 when they are equal, else clamped to 0..1.
float pitch_fraction(float pitch, float lowest, float highest)
{
    if (highest == lowest)
    {
        return 1.f; // DAT_0032B0F8.
    }
    const float fraction = (pitch - lowest) / (highest - lowest);
    if (fraction > 1.f)
    {
        return 1.f;
    }
    if (0.f > fraction)
    {
        return 0.f;
    }
    return fraction;
}

// The inlined 0x00042360 over +0x24.
void count_down(ShootState& state, RuntimeShootStateContext& context)
{
    const float timer = state.timer;
    if (0.f < timer)
    {
        const float left = timer - runtime_actor_step(context.actor, context.clock);
        state.timer = left;
        if (!(0.f < left))
        {
            state.timer = 0.f;
        }
    }
}

// Xbox 0x001014E0, XMM0 angle: whether `angle - reference`, wrapped into [-pi, pi), lies within +-`limit`.
bool runtime_angle_within(float angle, float reference, float limit)
{
    const float turn = runtime_relative_angle(angle, reference); // The inlined 0x00100E40 wrap.
    return 0.f - limit <= turn && turn <= limit;
}

// The held gun's message with a 7-word reply.
std::array<u32, 8> ask_weapon(u32 id, RuntimeShootStateContext& context)
{
    std::array<u32, 8> words{};
    words[0] = id;
    if (context.weapon == 0)
    {
        throw Error("shoot state messages its held gun (body +0x48) through address zero");
    }
    context.message(context.weapon, words); // Vtable +0xC.
    return words;
}

bool construct_shoot_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    ShootState& state = body_state_view<ShootState>({object.bytes.get(), object.size});
    return initialize_runtime_shoot_state(state, parameters, shoot_context_of(state.base.body));
}

void destroy_shoot_state_view(FactoryObject& object)
{
    ShootState& state = body_state_view<ShootState>({object.bytes.get(), object.size});
    destroy_runtime_shoot_state(state, shoot_context_of(state.base.body));
    shoot_animations.erase(body_state_key(state));
}

const ObjectFactoryVtable* shoot_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505B00, shoot_state_object_sizes, construct_shoot_state_view,
        destroy_shoot_state_view};
    return &table;
}
}

// Xbox 0x0009F6C0
void shoot_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(ShootState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0009F740
bool initialize_runtime_shoot_state(ShootState& state, std::span<const u8> parameters,
    RuntimeShootStateContext& context)
{
    const auto body = context.body;
    auto& records = shoot_animations[body_state_key(state)];
    records[0] = find_animation(parameters, parameter::aim.offset, context);
    state.aim = records[0] != nullptr ? 1u : 0u;
    records[1] = find_animation(parameters, parameter::aim_blend.offset, context);
    state.aim_blend = records[1] != nullptr ? 1u : 0u;
    records[2] = find_animation(parameters, parameter::second.offset, context);
    state.second = records[2] != nullptr ? 1u : 0u;
    records[3] = find_animation(parameters, parameter::second_blend.offset, context);
    state.second_blend = records[3] != nullptr ? 1u : 0u;
    state.lowest_pitch = get(parameters, parameter::lowest_pitch) * kDegree;
    state.highest_pitch = get(parameters, parameter::highest_pitch) * kDegree;
    const float fraction = pitch_fraction(0.f, state.lowest_pitch, state.highest_pitch);
    state.weight = fraction;
    state.aimed_weight = fraction;
    const char* sound = field_data(parameters, parameter::empty_sound);
    char* copy = state.empty_sound.data();
    size_t index = 0;
    do
    {
        copy[index] = sound[index];
    }
    while (sound[index++] != 0);
    state.last_target = context.mind.data() == nullptr ? 0u : mind_record(context.mind).action_record;
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    ++body_record(body).unk_6A;
    for (u32 counter = 0; counter < 4; ++counter)
    {
        if (((1u << counter) & 0xFFFFu) != 0)
        {
            ++body[offsetof(BodyRecord, unk_309) + counter];
        }
    }
    context.bits.bytes[5] |= 0x80;
    const auto reply = ask_weapon(0x9A, context); // Vtable +0xC of body +0x48.
    state.answered = reply[1] != 0 ? 1u : 0u;
    set_runtime_shoot_phase(state, 1, context); // 0x0009FED0
    return true;
}

// Xbox 0x0009F9B0
void destroy_runtime_shoot_state(ShootState& state, RuntimeShootStateContext& context)
{
    const auto body = context.body;
    if (state.phase == 1)
    {
        if (context.weapon != 0 && state.firing != 0)
        {
            const std::array<u32, 1> stop{0xA3};
            context.message(context.weapon, stop); // Vtable +0xC.
        }
        state.firing = 0u;
        state.aimed = 0u;
    }
    state.phase = 0u;
    --body_record(body).unk_6A;
    --body_record(body).unk_309;
    --body_record(body).unk_30A;
    --body_record(body).unk_30C;
    --body_record(body).unk_30B;
    context.bits.bytes[5] &= 0x7F;
    AnimationController* controller = context.actions.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        // Slot 2 fades out over 0.25 s (DAT_0032B104).
        if (controller->slots.size() > 2 && controller->slots[2].player != nullptr)
        {
            controller->slots[2].fade_duration = 0.25f; // +0x1C.
            controller->slots[2].fade_mode = 3; // +0x18.
        }
        controller->dirty = true; // +0x5C.
    }
}

// Xbox 0x0009FED0, EDI state
void set_runtime_shoot_phase(ShootState& state, u32 phase, RuntimeShootStateContext& context)
{
    if (state.phase == 1)
    {
        if (context.weapon != 0 && state.firing != 0)
        {
            const std::array<u32, 1> stop{0xA3};
            context.message(context.weapon, stop); // Vtable +0xC.
        }
        state.firing = 0u;
        state.aimed = 0u;
    }
    state.phase = phase;
    const auto& records = shoot_animations.at(body_state_key(state));
    if (phase == 1)
    {
        start_animation(records[0], context); // +0x28
        start_animation(records[1], context); // +0x2C
        weigh_aim_slot(state, 1, context);
        state.timer = 0.3499999940395355f; // DAT_0032B354.
    }
    else if (phase == 2)
    {
        start_animation(records[2], context); // +0x30
        start_animation(records[3], context); // +0x34
        weigh_aim_slot(state, 1, context);
    }
}

// Xbox 0x0009FA80
void tick_runtime_shoot_state(ShootState& state, RuntimeShootStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    state.base.elapsed = runtime_actor_step(actor, context.clock) + state.base.elapsed;
    const u32 target = context.mind.data() == nullptr ? 0u : mind_record(context.mind).action_record;
    ask_weapon(0x9F, context); // The muzzle; this tick does not read the reply.
    count_down(state, context);
    const u32 phase = state.phase;
    if (phase == 2)
    {
        if (target != 0)
        {
            body_record(body).euler = runtime_shoot_turn(context.target_at(target), context); // 0x000A0320
        }
        else
        {
            level_body_euler(body);
        }
        if (!runtime_actor_animation_slot_complete(context.actions.animation, 0)) // The inlined 0x0003B290.
        {
            state.last_target = target;
            return;
        }
        if (context.attacker() != 0) // 0x00028C50
        {
            set_runtime_shoot_phase(state, 1, context); // 0x0009FED0
            state.last_target = target;
            return;
        }
        state.base.next_key = context.next_key(); // Body vtable +0x20.
        state.last_target = target;
        return;
    }
    if (phase != 1)
    {
        state.last_target = target;
        return;
    }
    bool stop;
    std::span<const u8> record;
    if (target != 0)
    {
        record = context.target_at(target);
        const Vec3 point{read_f32(record, 0x20), read_f32(record, 0x30) * 0.5f + read_f32(record, 0x24),
            read_f32(record, 0x28)}; // DAT_0032B0F4.
        fire_runtime_shoot_state(state, target, read_f32(record, 0x38), point, context); // 0x0009FF90
        body_record(body).euler = runtime_shoot_turn(record, context); // 0x000A0320
        bool changed = false;
        if (target != state.last_target)
        {
            const u32 side = runtime_body_target_direction(actor, record, actor.euler.y); // 0x0001CC10
            const u32 hit_side = context.hit.data() == nullptr ? 1u : u32(context.hit[6]);
            changed = side != hit_side;
        }
        if (changed)
        {
            stop = true;
        }
        else
        {
            stop = target != context.attacker(); // 0x00028C50
            if (state.timer > 0.f && context.attacker() == 0)
            {
                stop = false;
            }
        }
    }
    else
    {
        stop = true;
        level_body_euler(body);
    }
    aim_runtime_shoot_state(state, record, context); // 0x000A0160, EAX target
    if (context.input == nullptr)
    {
        throw Error("0x0009FA80 reads the right stick of input address zero");
    }
    std::array<float, 2> axes{};
    runtime_input_secondary_axes(*context.input, axes, context.input_state); // 0x001088D0
    const bool released = !(axes[1] * axes[1] + axes[0] * axes[0] > 0.010000000707805157f) // DAT_0032B22C.
        && state.timer == 0.f;
    if (!stop)
    {
        // An empty gun (+0x93C clear) plays the empty sound as the state ends.
        const u8 charged = context.bytes_at(context.weapon)[offsetof(PlayerRecord, case_link)];
        if (charged == 0)
        {
            request_actor_sound(actor, 1, state.empty_sound.data(), context.sounds); // 0x00041D00
        }
        else if (!released)
        {
            if (state.firing == 0)
            {
                state.last_target = target;
                return;
            }
            const u32 answered = state.answered;
            if (answered == 0)
            {
                const std::array<u32, 2> strength{0xA1,
                    std::bit_cast<u32>(runtime_input_stick_strength(*context.input, context.input_state))}; // 0x00140A60
                context.message(context.weapon, strength); // Vtable +0xC.
                state.last_target = target;
                return;
            }
            if (answered == 1)
            {
                set_runtime_shoot_phase(state, 2, context); // 0x0009FED0
            }
            state.last_target = target;
            return;
        }
    }
    state.base.next_key = context.next_key(); // Body vtable +0x20.
    state.last_target = target;
}

// Xbox 0x0009FF90, EDI state, ESI point
bool fire_runtime_shoot_state(ShootState& state, u32 target, float radius, const Vec3& point,
    RuntimeShootStateContext& context)
{
    if (state.firing != 0)
    {
        return true;
    }
    const auto muzzle = ask_weapon(0x9F, context); // Vtable +0xC of the gun.
    if (muzzle[1] == 0)
    {
        return false;
    }
    const Vec3 from{std::bit_cast<float>(muzzle[2]), std::bit_cast<float>(muzzle[3]),
        std::bit_cast<float>(muzzle[4])};
    const Vec3 facing{std::bit_cast<float>(muzzle[5]), std::bit_cast<float>(muzzle[6]),
        std::bit_cast<float>(muzzle[7])};
    // Near: within the target's radius and 5 (DAT_0032B184) of the muzzle, with the aim settled within 0.05
    // (DAT_0032B250) of its pitch.
    bool near = false;
    if (state.aimed != 0)
    {
        const float reach = radius + 5.f;
        const float dx = from.x - point.x;
        const float dz = from.z - point.z;
        if (reach * reach >= dz * dz + dx * dx)
        {
            const float weight = state.weight;
            const float aimed = state.aimed_weight;
            near = aimed >= weight - 0.05f && weight + 0.05f >= aimed;
        }
    }
    float yaw_to = 0.f;
    float pitch_to = 0.f;
    direction_to_yaw_pitch(point.x - from.x, point.y - from.y, point.z - from.z, yaw_to, pitch_to); // 0x00105E20
    float yaw_facing = 0.f;
    float pitch_facing = 0.f;
    direction_to_yaw_pitch(facing.x, facing.y, facing.z, yaw_facing, pitch_facing); // 0x00105E20
    if (!near)
    {
        constexpr float kThirty = 0.5235987901687622f; // 0x3F060A92.
        if (!runtime_angle_within(yaw_facing, yaw_to, kThirty) // 0x001014E0
            || !runtime_angle_within(pitch_facing, pitch_to, kThirty))
        {
            return state.firing != 0;
        }
    }
    const std::array<u32, 5> fire{0xA2, target, std::bit_cast<u32>(point.x), std::bit_cast<u32>(point.y),
        std::bit_cast<u32>(point.z)};
    context.message(context.weapon, fire); // Vtable +0xC.
    state.firing = 1u;
    return true;
}

// Xbox 0x000A0160, EAX target, ECX actor, EBX state
void aim_runtime_shoot_state(ShootState& state, std::span<const u8> target, RuntimeShootStateContext& context)
{
    if (target.data() == nullptr || state.aim_blend == 0)
    {
        return;
    }
    const Actor& actor = context.actor;
    const Vec3 point{mind_target_record(target).shape.position.x, mind_target_record(target).shape.height * 0.75f
        + mind_target_record(target).shape.position.y,
        mind_target_record(target).shape.position.z}; // DAT_0032B2AC.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float eye = actor.radius_y * body_record(context.body).height_scale * 0.75f + position.y;
    const Vec3 direction{point.x - position.x, point.y - eye, point.z - position.z};
    float pitch = 0.f;
    runtime_direction_pitch(direction, pitch); // 0x00106000
    const float aimed = pitch_fraction(pitch, state.lowest_pitch, state.highest_pitch);
    state.aimed_weight = aimed;
    state.aimed = 1u;
    const float weight = state.weight;
    if (aimed > weight)
    {
        const float raised = runtime_actor_step(actor, context.clock) * 2.f + weight; // DAT_0032B110.
        state.weight = raised;
        if (raised > aimed)
        {
            state.weight = aimed;
        }
    }
    else if (weight > aimed)
    {
        const float lowered = weight - runtime_actor_step(actor, context.clock) * 2.f; // 0x00042320
        state.weight = lowered;
        if (aimed > lowered)
        {
            state.weight = aimed;
        }
    }
    weigh_aim_slot(state, 1, context);
}

// Xbox 0x000A0320, EAX target, EDX body
Vec3 runtime_shoot_turn(std::span<const u8> target, RuntimeShootStateContext& context)
{
    const float heading = runtime_body_hit_angle(context.hit); // 0x0001D540
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 direction{mind_target_record(target).shape.position.x - position.x,
        mind_target_record(target).shape.position.y - position.y,
        mind_target_record(target).shape.position.z - position.z};
    float angle = 0.f;
    runtime_direction_angle(direction, angle); // 0x00105F90
    return Vec3{0.f, angle - heading, 0.f};
}

// Xbox 0x00140A60, EAX device
float runtime_input_stick_strength(const RuntimeDialogueInputDevice& device, const RuntimeDialogueInputState& state)
{
    std::array<float, 2> axes{};
    runtime_input_secondary_axes(device, axes, state); // 0x001088D0
    const float strength = float((retail_square_root(axes[1] * axes[1] + axes[0] * axes[0]) - 0.4000000059604645L)
        * 1.6666666269302368L); // DAT_0032B11C, DAT_0032B118.
    if (strength > 1.f)
    {
        return 1.f;
    }
    if (0.f > strength)
    {
        return 0.f;
    }
    return strength;
}

void install_runtime_shoot_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeShootStateContext&(u32 body_address)> context_of)
{
    shoot_context_of = std::move(context_of);
    types[0x81] = ObjectFactoryType{9, shoot_state_lifecycle}; // Getter 0x0009F6E0.
}
}
