#include "medusa_strategy.h"
#include <cstddef>
#include "body_fields.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "body_damage_motion.h"
#include "body_message_helpers.h"
#include "body_orientation.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "segment_records.h"
#include "../camera/camera_body_slots.h"
#include "../navigation/navigation.h"
#include "../scene/trophy_heads_control.h"
#include "gbtg/anim.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
MedusaStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<MedusaStrategyState>(strategy);
}

const MedusaStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<MedusaStrategyState>(strategy);
}

namespace cone = light_cone_field;

void put_u32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_f32(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

RuntimePlayerStrategyGoals& goals_of(RuntimeMedusaStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeMedusaStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeMedusaStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeMedusaStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeMedusaStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeMedusaStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeMedusaStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeMedusaStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

float parameter(RuntimeMedusaStrategy& context, size_t offset)
{
    return read_f32(context.parameters, offset);
}

EntityBitBlock& body_bits(RuntimeMedusaStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

// The actor's frame step: none while frozen (DAT_004312DC), the actor's +0x92C rate unless +0x930 holds it.
float actor_step(RuntimeMedusaStrategy& context)
{
    return runtime_actor_rate(actor_of(context)) * runtime_frame_step(goals_of(context).clock);
}

// A running timer counts down by the step and stops at zero.
void count_down(float& timer, float step)
{
    float value = timer;
    if (0.f < value)
    {
        value = value - step;
        if (!(0.f < value))
        {
            value = 0.f;
        }
        timer = value;
    }
}

// The attributes' (+0x964) first two floats bound the next gaze's delay; 0 without them.
float roll_special_delay(RuntimeMedusaStrategy& context)
{
    const std::span<const u8> attributes = context.shared.checks.attributes;
    if (attributes.data() == nullptr)
    {
        return 0.f;
    }
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float low = read_f32(attributes, 0);
    const float high = read_f32(attributes, 4);
    return (high - low) * (std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f) + low;
}

// The attributes' +8 is exactly 1: the Medusa gazes whenever she can.
bool attributes_always_special(RuntimeMedusaStrategy& context)
{
    const std::span<const u8> attributes = context.shared.checks.attributes;
    return attributes.data() != nullptr && read_f32(attributes, 8) == 1.f;
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeMedusaStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = get(mind, mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

RuntimeLightCones& cones_of(RuntimeMedusaStrategy& context, const char* caller)
{
    if (context.cones == nullptr)
    {
        throw Error(std::string(caller) + " reaches the light cones (DAT_0050EF20) before the scene has them");
    }
    return *context.cones;
}

// A cone's +4: drawn or not.
void show_cone(RuntimeMedusaStrategy& context, i32 index, u32 visible, const char* caller)
{
    set(std::span<u8>(cones_of(context, caller).records.at(size_t(index)).bytes), cone::visible, visible);
}

// Xbox 0x000BA6D0, ECX strategy: both cones are hidden.
void hide_cones(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).first_cone >= 0)
    {
        show_cone(context, fields_of(strategy).first_cone, 0, "0x000BA6D0");
    }
    if (fields_of(strategy).second_cone >= 0)
    {
        show_cone(context, fields_of(strategy).second_cone, 0, "0x000BA6D0");
    }
}

// Xbox 0x000BA700, EAX strategy, EDI actor: the two cones (joint 7, then joint 6) and the rattle are made once.
void make_cones(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const u32 address = goals_of(context).address_of(goals_of(context).entry.actor_bytes.data());
    constexpr float kDegree = 0.01745329238474369f; // DAT_0032B0FC.
    if (fields_of(strategy).first_cone < 0)
    {
        fields_of(strategy).first_cone = add_runtime_light_cone(cones_of(context, "0x000BA700"), actor, address, 7,
            parameter(context, 0x30) * kDegree, parameter(context, 0x34) * kDegree, parameter(context, 0x38),
            parameter(context, 0x3C), parameter(context, 0x40), parameter(context, 0x44)); // 0x0011AEE0
    }
    if (fields_of(strategy).second_cone < 0)
    {
        fields_of(strategy).second_cone = add_runtime_light_cone(cones_of(context, "0x000BA700"), actor, address, 6,
            parameter(context, 0x30) * kDegree, parameter(context, 0x34) * -0.01745329238474369f, // DAT_0032B54C.
            parameter(context, 0x38), parameter(context, 0x3C), parameter(context, 0x40),
            parameter(context, 0x44)); // 0x0011AEE0
    }
    if (fields_of(strategy).rattle_sound == 0)
    {
        queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f,
            "XACT_SOUNDBANK_GMEDUSA_RATTLESNAKE", context.sounds); // 0x0012A260, EBX actor +0x614
        fields_of(strategy).rattle_sound = 1u;
    }
    hide_cones(context);
}

// Xbox 0x000BA840, EDI strategy: both cones are hidden and given back, and the sounds stop.
void release_cones(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    hide_cones(context);
    const auto give_back = [&](i32& slot)
    {
        const i32 index = slot;
        if (index < 0)
        {
            return;
        }
        RuntimeLightCones& cones = cones_of(context, "0x000BA840");
        set(std::span<u8>(cones.records.at(size_t(index)).bytes), cone::used, 0u);
        cones.count = cones.count - 1;
        slot = -1;
    };
    give_back(fields_of(strategy).first_cone);
    give_back(fields_of(strategy).second_cone);
    if (fields_of(strategy).rattle_sound != 0)
    {
        release_actor_sound(actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    if (fields_of(strategy).unk_164 != 0)
    {
        release_actor_sound(actor.sound_slots[7], context.sounds); // 0x0012A3D0, ESI actor +0x83C
    }
}

// Xbox 0x000BA010, EDI strategy: body +0x84 takes the 0xF8 attack row and the body attacks.
void request_attack(RuntimeMedusaStrategy& context)
{
    const std::span<u8> body = body_of(context);
    if (context.attack_resource.data() == nullptr)
    {
        throw Error("0x000BA010 reads the body +0x5C attack tables through address zero");
    }
    const u8* row = find_runtime_attachment_first_request(*context.attack_tables.at(context.attack_resource.data()),
        0xF8); // 0x0001C560, EAX body +0x5C, EBX 0xF8
   body_record(body).hit_record = row == nullptr ? 0u : context.address_of(row);
    body_record(body).unk_6A = u8(body_record(body).unk_6A + 1);
   body_record(body).unk_74C = 1;
    fields_of(strategy_of(context)).attack_request = 1u;
}

// Xbox 0x000BA050, ECX strategy: the attack row is dropped and the cones hidden.
void drop_attack(RuntimeMedusaStrategy& context)
{
    const std::span<u8> body = body_of(context);
   body_record(body).hit_record = 0;
   body_record(body).unk_88 = 0;
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
   body_record(body).unk_74C = 0;
    hide_cones(context);
    fields_of(strategy_of(context)).attack_request = 0u;
}

// Xbox 0x000BA680, EAX strategy, EDX actor: the Super Scary challenge from the two names.
void start_challenge(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters,
        fields_of(strategy).challenge_first.data(),
        goals.address_of(goals.entry.actor_bytes.data()), -1.f, -1.f, nullptr, nullptr, 0, nullptr, 0, 0, 1,
        fields_of(strategy).challenge_second.data(),
        context.runtime_flags); // 0x000873A0
    context.create_object(parameters); // 0x00107520
}

// Xbox 0x000BA520, EAX direction, ECX point, EDX the object: message 0x15 to the object hit, 0x41 to the Medusa,
// then the contacts are freed.
void report_gaze_hit(RuntimeMedusaStrategy& context, entity_detail::SegmentProbe& probe, const Vec3& direction,
    const Vec3& point, GhoulySpawnPool* contacts)
{
    RuntimeSegmentContactActor& object = context.contact_actor(probe);
    RuntimeContactNotice notice{0x15, 0, &actor_of(context), direction, contacts, point};
    object.message(&notice); // Vtable +0xC.
    RuntimeContactReport report{0x41, 0, direction, &object, contacts, point, 0};
    context.report(report); // The actor's vtable +0xC.
    engine_heap_free(contacts); // HeapFree on the block's heap (0x000F7100).
}

// Xbox 0x000BA300, EAX the eye: the gaze from the eye along `euler` reaches a live player target.
bool gaze_reaches_target(RuntimeMedusaStrategy& context, const Vec3& eye, const Vec3& euler,
    entity_detail::SegmentProbe*& hit)
{
    if ((context.runtime_flags & 0x400) != 0)
    {
        return false;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    Actor& actor = actor_of(context);
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr || !runtime_strategy_target_watching_player(*target)) // 0x001414F0
    {
        return false;
    }
    if (target->body.energy == 0 || target->body.bits == nullptr || (target->body.bits->bytes[3] & 8) != 0)
    {
        return false;
    }
    Vec3 from = eye;
    Vec3 marker{};
    actor_marker_world_position(actor, 0, marker); // 0x0003AE60, EDX 0
    if (entity_record_sweep_probe(from, marker, 4, 0) != 0) // 0x00122480, EBX the eye
    {
        return false;
    }
    Vec3 near = eye;
    Vec3 far = eye;
    add_sound_direction(near, euler.x, euler.y, parameter(context, 0x28)); // 0x00106220
    add_sound_direction(far, euler.x, euler.y, parameter(context, 0x2C)); // 0x00106220
    const float radius = parameter(context, 0x28);
    entity_record_movement_probe(far, near, std::bit_cast<u32>(radius), 4, 0); // 0x001226C0, EBX far
    if ((context.runtime_flags & 0x400) != 0)
    {
        return false;
    }
    const u32 record = runtime_player_mind_target(goals); // 0x00028C30, EAX actor
    const RuntimeMindTarget* other = record == 0 ? nullptr : goals.target_at(record);
    if (other == nullptr || other->body.energy == 0)
    {
        return false;
    }
    if (entity_record_point_probe(far, near, radius, 2) != 1) // 0x001229F0, EBX far
    {
        return false;
    }
    auto& found = entity_detail::g_segment_slots[0]; // DAT_00511A1C / DAT_00511A20.
    hit = found.object;
    if (found.object == nullptr || &context.contact_actor(*found.object).actor != &other->actor)
    {
        return false;
    }
    const Vec3 direction{far.x - near.x, far.y - near.y, far.z - near.z};
    report_gaze_hit(context, *found.object, direction, far, copy_segment_records(found.pool)); // 0x000FCC90
    return true;
}

// Xbox 0x000BA0B0, EAX strategy: the cones are shown and, with an attack row, the gaze tested; a hit starts the
// challenge and turns the Medusa toward the target.
void gaze(RuntimeMedusaStrategy& context, u32 attacking)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    constexpr float kDegree = 0.01745329238474369f; // DAT_0032B0FC.
    const float pitch_offset = parameter(context, 0x20) * kDegree;
    const float yaw_offset = parameter(context, 0x24) * kDegree;
    Vec3 eye{};
    Vec3 euler{};
    Vec3 second_euler{};
    const auto joint = [&actor](i16 kind) -> u32
    {
        if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
        {
            return 0xFF;
        }
        return actor_lookup_joint_index(actor.model->cpu, kind) & 0xFF; // 0x00052FC0
    };
    if (const u32 index = joint(7); index != 0xFF)
    {
        actor_transform_joint_point(actor, u16(index), actor_transform_origin, eye); // 0x0003AEB0, EBX DAT_0050DD80
    }
    if (const u32 index = joint(6); index != 0xFF)
    {
        actor_transform_joint_point(actor, u16(index), actor_transform_origin, euler); // 0x0003AEB0
    }
    if (const u32 index = joint(7); index != 0xFF)
    {
        actor_joint_world_euler(actor, u16(index), euler); // 0x0003B050
    }
    if (const u32 index = joint(6); index != 0xFF)
    {
        actor_joint_world_euler(actor, u16(index), second_euler); // 0x0003B050
    }
    const u32 saved_flags = actor.model_flags; // Actor +0x1F0.
    actor.model_flags = 0;
    show_cone(context, fields_of(strategy).first_cone, 1, "0x000BA0B0");
    euler.x = euler.x + pitch_offset;
    euler.y = euler.y + yaw_offset;
    show_cone(context, fields_of(strategy).second_cone, 1, "0x000BA0B0");
    if (attacking != 0)
    {
        entity_detail::SegmentProbe* hit = nullptr;
        const bool reached = gaze_reaches_target(context, eye, euler, hit); // 0x000BA300
        fields_of(strategy).gaze_toggle = fields_of(strategy).gaze_toggle == 0 ? 1u : 0u;
        if (reached)
        {
            const std::span<u8> body = body_of(context);
            fields_of(strategy).gaze_hit = 1u;
            start_challenge(context); // 0x000BA680
            const float angle = runtime_actor_angle_toward(context.contact_actor(*hit).actor, actor); // 0x00041900
            Vec3 turned = actor_world_euler(actor); // 0x00041820
            turned.y = angle;
            wrap_runtime_euler(turned, actor.euler); // 0x00105AA0
            const u32 stale = 0;
            std::memcpy(goals_of(context).entry.actor_bytes.data() + 0x2A8, &stale, 4);
            actor.xform_valid = 0;
           body_record(body).euler.y = angle;
        }
    }
    actor.model_flags = saved_flags;
}

// The target's point lies within the parameters' reach (+0).
bool target_near(RuntimeMedusaStrategy& context)
{
    return runtime_mind_target_within(parameter(context, 0), goals_of(context)); // 0x000BABF0
}

// Xbox 0x000BAC90, ECX strategy, EBX actor: a live, standing target near enough and in a clear walk, with no attack
// delay left.
bool can_attack(RuntimeMedusaStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr || target->body.energy == 0 || target->body.bits == nullptr
        || (target->body.bits->bytes[3] & 8) != 0)
    {
        return false;
    }
    if (fields_of(strategy_of(context)).attack_delay != 0.f)
    {
        return false;
    }
    if (!target_near(context)) // 0x000BABF0
    {
        return false;
    }
    const Actor& actor = actor_of(context);
    const Vec3 to = actor_world_position(target->actor); // 0x000412A0, ESI the target's actor
    const Vec3 from = actor_world_position(actor); // 0x0012C970's 0x000412A0
    i32 cell = 0;
    return navigation_path_clear(from, to, 1, 0, 0, 0, nullptr, &cell, i16(actor.class_key)); // 0x0012C970
}

// Xbox 0x000BAE60: a target in front of the Medusa (along its +0x5C / +0x64 facing) gives the body's turn (+0x730)
// toward a point the parameters' +0x18 out from it.
bool face_attack(RuntimeMedusaStrategy& context)
{
    const float out = parameter(context, 0x18);
    if (!(out > 0.f))
    {
        return false;
    }
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const Vec3 at{get(target->bytes, mind_target_field::shape_position).x, get(target->bytes, mind_target_field::shape_position).y, get(target->bytes, mind_target_field::shape_position).z};
    const float facing_x = get(target->bytes, mind_target_field::direction).x;
    const float facing_z = get(target->bytes, mind_target_field::direction).z;
    Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    if (facing_z * (position.z - at.z) + (position.x - at.x) * facing_x + (position.y - at.y) * 0.f > 0.f)
    {
        const Vec3 point{facing_x * out + at.x, out * 0.f + at.y, facing_z * out + at.z};
        position = actor_world_position(actor_of(context)); // 0x000412A0
       body_record(body_of(context)).euler.y = runtime_horizontal_angle_between(point, position); // 0x00106140
    }
    return true;
}

// Xbox 0x000BADF0, EAX mode, ESI strategy: walking at random (1) or attacking (2).
void set_walk_mode(u32 mode, RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).walk_mode == mode)
    {
        return;
    }
    if (fields_of(strategy).walk_mode == 2)
    {
        fields_of(strategy).attack_delay = parameter(context, 0x1C);
    }
    fields_of(strategy).walk_mode = mode;
    if (mode == 1)
    {
        strategy_base(strategy).attacking = 0;
        return;
    }
    if (mode != 2)
    {
        return;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
    load_goal(context.goal_attack, context);
    goals.room = nullptr; // Actor +0xCA0.
    face_attack(context); // 0x000BAE60
    strategy_base(strategy).attacking = 1;
}

// Xbox 0x000BAD30, ECX strategy: the gaze is due (always, or once the delay has run down).
bool special_due(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).always_special != 0)
    {
        return true;
    }
    const std::span<const u8> attributes = context.shared.checks.attributes;
    if (attributes.data() == nullptr || read_f32(attributes, 8) == 0.f)
    {
        return false;
    }
    count_down(fields_of(strategy).special_delay, actor_step(context));
    return fields_of(strategy).special_delay <= 0.001f; // DAT_0032B1A4.
}

// Xbox 0x000BB100, EAX actor: the target's point lies within the body's reach (0x0001D3D0) plus 15.
bool target_in_reach(RuntimeMedusaStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const float reach = context.attachment_radius() + 15.f; // 0x0001D3D0, DAT_0032B214
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const float dx = position.x - get(target->bytes, mind_target_field::shape_position).x;
    const float dz = position.z - get(target->bytes, mind_target_field::shape_position).z;
    const float dy = position.y - get(target->bytes, mind_target_field::shape_position).y;
    return dx * dx + dz * dz + dy * dy < reach * reach;
}

// Xbox 0x000BB090, EAX step, ECX strategy: recovering (2) attacks a target in reach, else gazes.
void set_dizzy_step(u32 step, RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).dizzy_step == step)
    {
        return;
    }
    fields_of(strategy).dizzy_step = step;
    if (step != 2)
    {
        return;
    }
    if (!target_in_reach(context)) // 0x000BB100
    {
        change_state(0x3EF, context);
        return;
    }
    if (mind_target(context) != nullptr)
    {
        goals_of(context).room = nullptr; // The strategy's actor +0xCA0.
    }
    load_goal(context.goal_attack, context);
}

// Xbox 0x000B9D60, EAX strategy: the gaze (0x3EF).
void tick_gaze(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeMindTarget* target = nullptr;
    select_runtime_strategy_target(target, 0, goals); // 0x00139AA0
    count_down(fields_of(strategy).gaze_time, actor_step(context));
    if ((body_bits(context, "0x000B9D60").bytes[5] & 0x20) != 0)
    {
        make_cones(context); // 0x000BA700
        gaze(context, fields_of(strategy).attack_request); // 0x000BA0B0
    }
    else
    {
        if (fields_of(strategy).first_cone >= 0)
        {
            show_cone(context, fields_of(strategy).first_cone, 0, "0x000B9D60");
        }
        if (fields_of(strategy).second_cone >= 0)
        {
            show_cone(context, fields_of(strategy).second_cone, 0, "0x000B9D60");
        }
    }
    const u32 phase = fields_of(strategy).gaze_phase;
    const auto walk = [&]()
    {
        // LAB 0x000B9FCF: on to walking with a fresh spin delay.
        if (fields_of(strategy).gaze_phase == 3)
        {
            return;
        }
        fields_of(strategy).gaze_phase = 3u;
        fields_of(strategy).spin_delay = runtime_body_random_range(goals.random, parameter(context, 0xC),
            parameter(context, 0xC)); // 0x000191C0
    };
    switch (phase)
    {
    case 1:
        if (requests_pending(context) || phase == 2)
        {
            return;
        }
        fields_of(strategy).gaze_phase = 2u;
        if (fields_of(strategy).attack_request == 0)
        {
            request_attack(context); // 0x000BA010
        }
        fields_of(strategy).face_delay = parameter(context, 0x14);
        return;
    case 2:
    {
        if (target == nullptr)
        {
            walk();
            return;
        }
        float delay = fields_of(strategy).face_delay;
        const bool done = count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0x14C
        fields_of(strategy).face_delay = delay;
        if (done)
        {
            walk();
            return;
        }
        if (requests_pending(context))
        {
            return;
        }
        face_runtime_body_target(body_of(context), actor_of(context), target->bytes); // 0x001345F0, ECX body
        return;
    }
    case 3:
    {
        if (fields_of(strategy).always_special == 0 && !(fields_of(strategy).gaze_time > 0.001f))
        {
            hide_cones(context); // 0x000BA6D0
            if (fields_of(strategy).gaze_phase == 5)
            {
                return;
            }
            fields_of(strategy).gaze_phase = 5u;
            change_state(0x3F0, context);
            return;
        }
        float delay = fields_of(strategy).spin_delay;
        count_down_runtime_strategy_timer(delay, goals); // 0x0002B9F0, ECX +0x148
        fields_of(strategy).spin_delay = delay;
        if (0.001f > delay) // DAT_0032B1A4.
        {
            if (fields_of(strategy).gaze_phase == 4)
            {
                return;
            }
            fields_of(strategy).gaze_phase = 4u;
            clear_runtime_player_mind_requests(*goals.setup.mind); // 0x00029E80, EBX mind
            load_goal(context.goal_spin, context);
            return;
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_special_walk, context);
        }
        return;
    }
    case 4:
        if (requests_pending(context))
        {
            return;
        }
        walk();
        return;
    default:
        return;
    }
}

// Xbox 0x000BAAB0, EAX actor, ECX strategy: walking (0x401).
void tick_walk(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeMindTarget* target = nullptr;
    select_runtime_strategy_target(target, 0, goals); // 0x00139AA0
    count_down(fields_of(strategy).attack_delay, actor_step(context));
    if (special_due(context)) // 0x000BAD30
    {
        change_state(0x3EF, context);
        return;
    }
    switch (fields_of(strategy).walk_mode)
    {
    case 1:
        if (can_attack(context)) // 0x000BAC90
        {
            set_walk_mode(2, context); // 0x000BADF0
            return;
        }
        if (!requests_pending(context))
        {
            load_goal(context.goal_random_walk, context);
        }
        return;
    case 2:
        if (requests_pending(context))
        {
            return;
        }
        fields_of(strategy).attack_delay = parameter(context, 0x1C);
        fields_of(strategy).walk_mode = 1u;
        strategy_base(strategy).attacking = 0;
        return;
    default:
        return;
    }
}

// Xbox 0x000BB040, ECX strategy, EDX actor: dizzy (0x409).
void tick_dizzy(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (fields_of(strategy).dizzy_step)
    {
    case 1:
        if ((body_bits(context, "0x000BB040").bytes[6] & 1) != 0)
        {
            set_dizzy_step(2, context); // 0x000BB090
        }
        return;
    case 2:
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    default:
        return;
    }
}
}

// Xbox 0x000BB1B0
void medusa_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(MedusaStrategyState);
    output.parameters = 0x118;
}

// Xbox 0x000BB220
bool initialize_runtime_medusa_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeMedusaStrategy& context)
{
    fields_of(strategy).first_cone = -1;
    fields_of(strategy).second_cone = -1;
    fields_of(strategy).parameters = 0x004287F0u;
    fields_of(strategy).gaze_phase = 0u;
    fields_of(strategy).unk_17C = 0u;
    fields_of(strategy).unk_180 = 0u;
    fields_of(strategy).attack_request = 0u;
    fields_of(strategy).shock_won = 0u;
    fields_of(strategy).gazed = 0u;
    fields_of(strategy).always_special = 0u;
    const char* first = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(fields_of(strategy).challenge_first.data(), first, std::strlen(first) + 1);
    const char* second = reinterpret_cast<const char*>(parameters.data() + 0x98);
    std::memcpy(fields_of(strategy).challenge_second.data(), second, std::strlen(second) + 1);
    fields_of(strategy).special_delay = roll_special_delay(context);
    if (attributes_always_special(context))
    {
        fields_of(strategy).always_special = 1u;
    }
    return true;
}

// Xbox 0x000BB340
void update_runtime_medusa_strategy(RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    if (attributes_always_special(context))
    {
        fields_of(strategy).always_special = 1u;
    }
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3EF:
        tick_gaze(context); // 0x000B9D60
        return;
    case 0x3F0:
        // The tail jump to 0x000BA9D0.
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    case 0x401:
        tick_walk(context); // The tail jump to 0x000BAAB0.
        return;
    case 0x409:
        tick_dizzy(context); // The tail jump to 0x000BB040.
        return;
    default:
        return;
    }
}

// Xbox 0x000BB400
void dispatch_runtime_medusa_message(std::span<const u32> message, RuntimeMedusaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    const u16 state = strategy_base(strategy).state;
    const bool attacking = state >= 0x3ED && state <= 0x3EF; // 0x000B7120
    switch (message[0])
    {
    case 0x31:
        if (!attacking)
        {
            react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x15, shared.checks); // 0x0013A4F0
        return;
    case 0x3B:
        if (message[2] == 2)
        {
            request_attack(context); // 0x000BA010
        }
        return;
    case 0x64:
        return;
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        if (state != 0x3EF && !attacking)
        {
            react_runtime_strategy_contact(message, 1.f, shared.checks); // The tail jump to 0x001395C0.
            return;
        }
        if (message[1] != actor && !runtime_message_lands_on_actor(message, actor)) // 0x001408A0
        {
            return;
        }
        if (strategy_base(strategy).pause_count != 0)
        {
            return;
        }
        const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
        const u32 kind =
            get(actor_bytes, player_field::mode) == 2 || get(actor_bytes, player_field::case_link) != 0 ? 1u : 0u;
        if (goals.resume.references[kind] != 0 || goals.resume.decay_enabled[goals.resume.decay_index] == 0)
        {
            return;
        }
        // The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
        const RuntimePlayerGoalRing* ring = goals.entry.ring;
        if (ring != nullptr
            && (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                             : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                != 0)
        {
            return;
        }
        change_state(0x401, context);
        return;
    }
    case 0x82:
        if (!attacking)
        {
            react_runtime_strategy_cower(shared.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        react_runtime_strategy_startle(shared.checks); // The tail jump to 0x001399A0.
        return;
    case 0x8E:
    {
        if (fields_of(strategy).gaze_hit == 0)
        {
            return;
        }
        const std::span<u8> body = body_of(context);
        Actor& owner = actor_of(context);
        const auto release_shock = [&]()
        {
            if (fields_of(strategy).shocked == 0)
            {
                return;
            }
            clear_runtime_body_bit(body_bits(context, "0x000BB400"), 0x3E); // 0x0001F030, EAX body
            decrement_runtime_body_latch_mask(body, 0x3F, owner, context.latches); // 0x0001F2F0, ECX body
            fields_of(strategy).shocked = 0u;
        };
        switch (message[1])
        {
        case 0:
            body_record(body).unk_92 = 1;
            select_runtime_body_animation(context.shock_camera_first, context.actions); // 0x0001AF10
            if (fields_of(strategy).shocked != 0)
            {
                return;
            }
            set_runtime_body_bit(body_bits(context, "0x000BB400"), 0x3E); // 0x0001EFD0, EAX body
            increment_runtime_body_latch_mask(body, 0x3F, owner, context.latches); // 0x0001F3F0, ECX body
            fields_of(strategy).shocked = 1u;
            return;
        case 2:
            select_runtime_body_animation(context.shock_camera_second, context.actions); // 0x0001AF10
            return;
        case 7:
            reset_runtime_body_flinch(body, goals.random); // 0x0001D9D0, ECX body
            fields_of(strategy).shock_won = 1u;
            release_shock();
            return;
        case 8:
            reset_runtime_body_flinch(body, goals.random); // 0x0001D9D0, ECX body
            fields_of(strategy).shock_won = 0u;
            release_shock();
            return;
        default:
            return;
        }
    }
    default:
        dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000BB720
void enter_runtime_medusa_state(u16 state, u16 previous, RuntimeMedusaStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    std::span<u8> body = body_of(context);
    body_record(body).move_class = 2;
    switch (state)
    {
    case 0x3EF:
        // 0x000B9C00, ESI strategy.
        fields_of(strategy).special_delay = 1000000000.f; // DAT_0032B30C.
        fields_of(strategy).shock_won = 0u;
        fields_of(strategy).gaze_hit = 0u;
        fields_of(strategy).gazed = 1u;
        fields_of(strategy).rattle_sound = 0u;
        fields_of(strategy).unk_164 = 0u;
        fields_of(strategy).shocked = 0u;
        load_goal(context.goal_special_attack, context);
        body_record(body).move_class = 6;
        strategy_base(strategy).attacking = 1;
        if (fields_of(strategy).gaze_phase != 1)
        {
            fields_of(strategy).gaze_phase = 1u;
            fields_of(strategy).gaze_time = runtime_body_random_range(goals.random, parameter(context, 4),
                parameter(context, 8)); // 0x000191C0
        }
        put_u32(body, offsetof(BodyRecord, turn_rates), read_u32(context.parameters, 0x48));
       body_record(body).turn_rates[1] = parameter(context, 0x4C);
       body_record(body).turn_rates[2] = parameter(context, 0x50);
        fields_of(strategy).gaze_toggle = 0u;
        return;
    case 0x3F0:
        // 0x000BA8F0.
        if (fields_of(strategy).gazed == 0)
        {
            change_state(0x401, context);
        }
        else if (fields_of(strategy).gaze_hit == 0)
        {
            load_goal(context.goal_back_to_normal, context);
        }
        else if (fields_of(strategy).shock_won != 0)
        {
            load_goal(context.goal_special_succeed, context);
        }
        else
        {
            change_state(0x3F2, context);
        }
        fields_of(strategy).gaze_hit = 0u;
        fields_of(strategy).gazed = 0u;
        return;
    case 0x401:
        // The tail jump to 0x000BAA30.
        fields_of(strategy).attack_delay = 0.f;
        if (fields_of(strategy).walk_mode != 1)
        {
            if (fields_of(strategy).walk_mode == 2)
            {
                fields_of(strategy).attack_delay = parameter(context, 0x1C);
            }
            fields_of(strategy).walk_mode = 1u;
            strategy_base(strategy).attacking = 0;
        }
        if (fields_of(strategy).special_delay > 60.f) // DAT_0032B28C.
        {
            fields_of(strategy).special_delay = roll_special_delay(context); // The tail jump to 0x000BB870.
        }
        return;
    case 0x403:
        // 0x000BAF90, ESI strategy.
        if (attributes_always_special(context))
        {
            fields_of(strategy).always_special = 1u;
        }
        change_state(0x401, context);
        return;
    case 0x409:
        // The tail jump to 0x000BB000.
        fields_of(strategy).unk_17C = 1u;
        fields_of(strategy).unk_180 = 0u;
        body_record(body).move_class = 5;
        if (fields_of(strategy).dizzy_step != 1)
        {
            fields_of(strategy).dizzy_step = 1u;
        }
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000BB7D0
void leave_runtime_medusa_state(u16 state, RuntimeMedusaStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EF:
    {
        // The tail jump to 0x000B9CD0, EAX strategy.
        if (fields_of(strategy).attack_request != 0)
        {
            drop_attack(context); // 0x000BA050
        }
        strategy_base(strategy).attacking = 0;
        const std::span<u8> body = body_of(context);
        put_f32(body, offsetof(BodyRecord, turn_rates), 0.5585053563117981f); // DAT_0032B220.
       body_record(body).turn_rates[1] = 12.566370964050293f; // DAT_0032B21C.
       body_record(body).turn_rates[2] = 6.f; // DAT_0032B218.
        body_bits(context, "0x000B9CD0").bytes[5] &= 0xDF;
        release_cones(context); // 0x000BA840, EDI strategy
        if (fields_of(strategy).gaze_phase != 0)
        {
            fields_of(strategy).gaze_phase = 0u;
        }
        return;
    }
    case 0x3F0:
        fields_of(strategy).special_delay = roll_special_delay(context); // The tail jump to 0x000BB870.
        return;
    case 0x401:
        // The tail jump to 0x000BAA80.
        if (fields_of(strategy).walk_mode != 0)
        {
            if (fields_of(strategy).walk_mode == 2)
            {
                fields_of(strategy).attack_delay = parameter(context, 0x1C);
            }
            fields_of(strategy).walk_mode = 0u;
        }
        return;
    case 0x403:
        return;
    case 0x409:
        if (fields_of(strategy).dizzy_step != 0)
        {
            fields_of(strategy).dizzy_step = 0u;
        }
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy).pause_count;
        }
        return;
    }
}

// Xbox 0x0001D9D0, ECX body
void reset_runtime_body_flinch(std::span<u8> body, u32& random)
{
    body_record(body).channel_keys = 2;
    body_record(body).channel_key = 0;
    body_record(body).unk_92 = 2;
    random = random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C; +0x8C held DAT_00338EE0 first.
   body_record(body).channel_timer = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// Xbox 0x00106140, ESI first, EDI second
float runtime_horizontal_angle_between(const Vec3& first, const Vec3& second)
{
    const float second_length = float(retail_square_root(second.x * second.x + second.z * second.z)); // 0x00152B30
    const long double first_length = retail_square_root(first.x * first.x + first.z * first.z);
    const float dot = second.x * first.x + second.z * first.z;
    const float product = float(first_length * second_length);
    if (9.999999974752427e-07f > product) // DAT_0032B278.
    {
        return 0.f;
    }
    const float magnitude = engine_absolute(dot); // 0x00152BB0
    const float divisor = magnitude > product ? magnitude : product;
    return float(retail_acos(dot / divisor)); // 0x00152C40
}
}
