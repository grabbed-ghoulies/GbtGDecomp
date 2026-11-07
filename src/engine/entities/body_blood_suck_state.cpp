#include "body_blood_suck_state.h"
#include <cstddef>
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_collision_culling.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_pose_updates.h"
#include "actor_render_registration.h"
#include "actor_runtime_queries.h"
#include "actor_shape_bounds.h"
#include "actor_spawn_eligibility.h"
#include "actor_transform.h"
#include "body_damage_notices.h"
#include "body_damage_processors.h"
#include "body_damage_tables.h"
#include "body_message_helpers.h"
#include "body_scoop_whack_state.h"
#include "camera_director.h"
#include "player_goal_cleanup.h"
#include "player_mind_control.h"
#include "player_mind_feedback.h"
#include "player_mind_requests.h"
#include "shape_transform.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "body_fields.h"
#include "player_fields.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = blood_suck_state_parameter;

constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.
constexpr float kHoldHeight = std::bit_cast<float>(0x3E9EB852u); // DAT_0032B51C, 0.31.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kDrainEpsilon = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4, 0.001.
constexpr float kStruggleFull = 30.f; // DAT_0032B108.
constexpr float kThrowPush = std::bit_cast<float>(0x3F7D70A4u); // DAT_0032B228, 0.99.
constexpr float kThrowSpread = std::bit_cast<float>(0x3E4CCCD0u); // DAT_0032B5D4.
constexpr float kThrowLeast = std::bit_cast<float>(0x3F666666u); // DAT_0032B174, 0.9.

std::function<RuntimeBloodSuckStateContext&(u32)> blood_suck_context_of;

// The four animation records the state found (0x0003AAA0), held natively beside the state's +0x3C .. +0x48 flags.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 4>> blood_suck_records;

// The inlined 0x0003AAA0 pair (the attached actor's +0xDB0 variant first, then the plain name).
const AnimationRecord* find_animation(u32 name, RuntimeBloodSuckStateContext& context)
{
    return find_runtime_body_animation(context.image_string(name), context.actions);
}

RuntimeBloodSuckTarget target_of(const BloodSuckState& state, RuntimeBloodSuckStateContext& context,
    const char* where)
{
    RuntimeBloodSuckTarget target = context.target_at(state.target);
    if (target.peer.actor == nullptr || target.peer.body.empty() || target.peer.bits == nullptr
        || target.view == nullptr)
    {
        throw Error(std::string(where) + " reads the body (+0x95C) of a target that is not a player");
    }
    return target;
}

// Each of the actor's live attachment records (+0x8F8) holds `step` more (0x00042090 / 0x000420B0).
void adjust_live_holds(entity_detail::ListHead& live_attachments, i32 step)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// The body's +0x309..+0x30C, +0x6D and +0x6A counts move by `step`.
void adjust_body_counts(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// The target's render bins come back (0x0003CAE0), and its held weapon's while that is still its own (0x001460C0).
void show_target(RuntimeBloodSuckTarget& target)
{
    register_actor_render_bins(target.view->render); // 0x0003CAE0
    if (target.held != nullptr && target.held_view != nullptr && actor_spawn_owner_unavailable(*target.held)) // 0x001460C0
    {
        register_actor_render_bins(target.held_view->render); // 0x0003CAE0
    }
}

void hide_target(RuntimeBloodSuckTarget& target)
{
    unregister_actor_render_bins(target.view->render); // 0x0003CBB0, EDI target
    if (target.held != nullptr && target.held_view != nullptr && actor_spawn_owner_unavailable(*target.held)) // 0x001460C0
    {
        unregister_actor_render_bins(target.held_view->render); // 0x0003CBB0, EDI held
    }
}

float target_hold_height(const RuntimeBloodSuckTarget& target)
{
    return target.peer.actor->radius_y * body_record(target.peer.body).height_scale;
}

bool construct_blood_suck_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    BloodSuckState& state = body_state_view<BloodSuckState>({object.bytes.get(), object.size});
    return initialize_runtime_blood_suck_state(state, parameters, blood_suck_context_of(state.base.body));
}

void destroy_blood_suck_state_view(FactoryObject& object)
{
    BloodSuckState& state = body_state_view<BloodSuckState>({object.bytes.get(), object.size});
    destroy_runtime_blood_suck_state(state, blood_suck_context_of(state.base.body));
}

const ObjectFactoryVtable* blood_suck_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505700, blood_suck_state_object_sizes,
        construct_blood_suck_state_view, destroy_blood_suck_state_view};
    return &table;
}
}

// Xbox 0x0008B1E0
void blood_suck_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(BloodSuckState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0008B260
bool initialize_runtime_blood_suck_state(BloodSuckState& state, std::span<const u8> parameters,
    RuntimeBloodSuckStateContext& context)
{
    const auto body = context.body;
    const u32 record = context.mind.empty() ? 0u : mind_record(context.mind).target_record;
    state.grabbed = 0u;
    state.unk_28 = 0u;
    state.latched = 0u;
    state.hidden = 0u;
    state.held = 0u;
    state.thrown = 0u;
    state.grab_callout = get(parameters, parameter::grab_callout);
    state.grab_shape = get(parameters, parameter::grab_shape);
    auto& records = blood_suck_records[body_state_key(state)];
    if (get(parameters, parameter::reach_animation) != 0)
    {
        records[0] = find_animation(get(parameters, parameter::reach_animation), context);
        state.reach_animation = records[0] != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::bite_animation) != 0)
    {
        records[1] = find_animation(get(parameters, parameter::bite_animation), context);
        state.bite_animation = records[1] != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::struggle_animation) != 0)
    {
        records[2] = find_animation(get(parameters, parameter::struggle_animation), context);
        state.struggle_animation = records[2] != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::throw_animation) != 0)
    {
        records[3] = find_animation(get(parameters, parameter::throw_animation), context);
        state.throw_animation = records[3] != nullptr ? 1u : 0u;
    }
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    adjust_live_holds(context.live_attachments, 1);
    body_record(body).unk_6D = u8(body_record(body).unk_6D + 1);
    body_record(body).unk_6A = u8(body_record(body).unk_6A + 1);
    adjust_body_counts(body, 1);
    state.tracking.axes[1] = 0.f;
    state.tracking.axes[0] = 0.f;
    state.tracking.accumulation = 0.f;
    state.tracking.activity = 0.f;
    state.tracking.state = 1u;
    state.tracking.hold_timer = 0.f;
    state.bite_sound = get(parameters, parameter::bite_sound);
    state.struggle_sound = get(parameters, parameter::struggle_sound);
    state.sound_playing = 0u;
    if (record == 0)
    {
        throw Error("blood suck state 0x0008B260 reads the +0x14 of a mind target record it has none of");
    }
    const std::span<const u8> target_record = context.target_record(record);
    if (target_record[0x14] == 0)
    {
        state.target = mind_target_record(target_record).actor;
    }
    bool grab = false;
    if (state.target != 0)
    {
        const RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B260");
        grab = (target.peer.bits->bytes[3] & 8) == 0 && body_record(target.peer.body).energy != 0
            && body_record(target.peer.body).rider == 0;
        if (grab)
        {
            claim_runtime_body_rider(target.peer.body, *target.peer.bits, context.actor_address); // 0x000265A0
        }
    }
    set_runtime_blood_suck_phase(grab ? 1u : 5u, state, context); // 0x0008BBD0, EDI state
    context.bits.bytes[1] |= 1;
    return true;
}

// Xbox 0x0008B530
void destroy_runtime_blood_suck_state(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const auto body = context.body;
    adjust_body_counts(body, -1);
    adjust_live_holds(context.live_attachments, -1);
    body_record(body).unk_6D = u8(body_record(body).unk_6D - 1);
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
    if (state.latched != 0)
    {
        decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
        state.latched = 0u;
    }
    if (state.sound_playing != 0)
    {
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
        state.sound_playing = 0u;
    }
    if (state.target != 0)
    {
        RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B530");
        if (state.hidden != 0)
        {
            register_actor_render_bins(target.view->render); // 0x0003CAE0
            state.hidden = 0u;
            if (target.held != nullptr && target.held_view != nullptr
                && actor_spawn_owner_unavailable(*target.held)) // 0x001460C0
            {
                register_actor_render_bins(target.held_view->render); // 0x0003CAE0
            }
        }
        if (state.held != 0)
        {
            const std::span<u8> target_body = target.peer.body;
            const u32 holds = body_record(target_body).holds - 1;
           body_record(target_body).holds = holds;
            if (holds == 0)
            {
                const std::array<u32, 1> release{0x4A};
                target.body_message(release); // Target body vtable +0xC.
            }
            const u16 count = u16(body_record(target_body).move_holds - 1);
            body_record(target_body).move_holds = count;
            adjust_live_holds(target.view->live_attachments, -1);
            register_actor_model_anchors(*target.peer.actor, target.view->anchors); // 0x0003EEB0, EBX target
            target.peer.bits->bytes[7] &= 0xFB;
            state.held = 0u;
        }
        if (state.thrown == 0)
        {
            throw_runtime_blood_suck_target(state, context); // 0x0008BF90, EAX actor
        }
        if (state.target != 0)
        {
            const RuntimeBloodSuckTarget rider = target_of(state, context, "blood suck state 0x0008B530");
            const u32 claimed = body_record(rider.peer.body).rider;
            if (claimed != 0 && claimed == context.actor_address)
            {
               body_record(rider.peer.body).rider = 0;
            }
        }
    }
    context.bits.bytes[1] &= 0xFE;
    blood_suck_records.erase(body_state_key(state));
}

// Xbox 0x0008B6C0
void tick_runtime_blood_suck_state(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    state.base.elapsed = rate * step + state.base.elapsed;
    switch (state.phase)
    {
    case 1:
    {
        if (state.grabbed == 0)
        {
            const u32 record = context.mind.empty() ? 0u : mind_record(context.mind).target_record;
            if (record == 0)
            {
                set_runtime_blood_suck_phase(5, state, context); // 0x0008BBD0, EDI state
                return;
            }
            Vec3 point;
            if (runtime_blood_suck_grab_point(record, state, point, context)) // 0x0008BD80, EAX record, ESI actor
            {
                const u32 target_address = state.target;
                std::array<u32, 6> message{0x3C, 0, context.actor_address, std::bit_cast<u32>(point.x),
                    std::bit_cast<u32>(point.y), std::bit_cast<u32>(point.z)};
                context.message(target_address, message); // Target vtable +0xC.
                if (message[1] != 0)
                {
                    RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B6C0");
                    context.message(target_address, message); // Target vtable +0xC, its reply set.
                    call_out_runtime_hit_point(point, target_address, state.grab_callout,
                        context.actor_callout); // 0x0013AD60, EAX point, ECX target
                    state.grabbed = 1u;
                    state.grab_start = Vec3{point.x, point.y - target_hold_height(target) * kHalf, point.z};
                    // 0x0003B1B0, EAX actor, ECX 0
                    state.grab_position = runtime_actor_animation_slot_position(context.animation, 0);
                    increment_runtime_body_latches(context.body, context.actor, context.latches); // 0x0001FC40, ECX body
                    state.latched = 1u;
                    if (state.held == 0)
                    {
                        const std::span<u8> target_body = target.peer.body;
                       body_record(target_body).holds = body_record(target_body).holds + 1;
                        const u16 count = u16(body_record(target_body).move_holds + 1);
                        body_record(target_body).move_holds = count;
                        adjust_live_holds(target.view->live_attachments, 1); // 0x00042090, EAX target
                        clear_actor_model_anchors(target.view->anchors); // 0x0003EF40, EAX target
                        set_runtime_body_bit(*target.peer.bits, 0x3A); // 0x0001EFD0, EAX target body
                        state.held = 1u;
                    }
                }
            }
        }
        if (!runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0, EAX actor
        {
            return;
        }
        if (state.grabbed == 0)
        {
            set_runtime_blood_suck_phase(5, state, context); // 0x0008BBD0, EDI state
            return;
        }
        state.drain_timer = kOne;
        if (state.hidden == 0)
        {
            RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B6C0");
            hide_target(target);
            state.hidden = 1u;
        }
        set_runtime_blood_suck_phase(2, state, context); // 0x0008BBD0, EDI state
        return;
    }
    case 2:
        if (!drain_runtime_blood_suck_target(state, context)) // 0x0008BE60, EAX state
        {
            track_runtime_blood_suck_struggle(state, context); // 0x0008BF50, EAX state
            if (state.tracking.state == 2)
            {
                set_runtime_blood_suck_phase(3, state, context); // 0x0008BBD0, EDI state
            }
        }
        return;
    case 3:
        if (!drain_runtime_blood_suck_target(state, context)) // 0x0008BE60, EAX state
        {
            track_runtime_blood_suck_struggle(state, context); // 0x0008BF50, EAX state
            if (state.tracking.state == 1)
            {
                set_runtime_blood_suck_phase(2, state, context); // 0x0008BBD0, EDI state
                return;
            }
            if (state.tracking.accumulation == kStruggleFull) // UCOMISS; JP keeps it.
            {
                set_runtime_blood_suck_phase(4, state, context); // 0x0008BBD0, EDI state
            }
        }
        return;
    case 4:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0, EAX actor
        {
            set_runtime_blood_suck_phase(5, state, context); // 0x0008BBD0, EDI state
        }
        return;
    default:
        return;
    }
}

// Xbox 0x0008B9C0
void step_runtime_blood_suck_state(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const u32 phase = state.phase;
    if (phase == 1)
    {
        if (state.grabbed == 0)
        {
            return;
        }
        RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B9C0");
        const Vec3 position = actor_world_position(context.actor); // 0x000412A0
        const float height = context.actor.radius_y * body_record(context.body).height_scale * kHoldHeight;
        const float slot = runtime_actor_animation_slot_position(context.animation, 0); // The inlined 0x0003B1B0.
        const Vec3 start = state.grab_start;
        const float from = state.grab_position;
        const float share = (slot - from) / (kOne - from);
        const Vec3 point{(position.x - start.x) * share + start.x, (height - start.y) * share + start.y,
            (position.z - start.z) * share + start.z};
        const Vec3 euler{0.f, context.actor.euler.y + kPi, 0.f};
        Actor& held = *target.peer.actor;
        actor_set_world_position(held, point); // 0x00041730, EDI target, ESI point
        const Vec3 placed = actor_world_position(held); // 0x000412A0, EDI target body +0x18
        std::memcpy(target.peer.body.data() + offsetof(BodyRecord, position.x), &placed, sizeof(placed));
        normalize_euler_angles(held.euler, euler); // 0x00105AA0, ESI euler, EDI target +0x14
        held.xform_valid = 0; // Target +0x2A8.
        const Vec3 world = actor_world_euler(held); // 0x00041820, EDI target body +0x72C
        std::memcpy(target.peer.body.data() + offsetof(BodyRecord, euler.x), &world, sizeof(world));
        return;
    }
    if (phase <= 1 || phase > 3)
    {
        return;
    }
    RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008B9C0");
    Vec3 point = actor_world_position(context.actor); // 0x000412A0
    point.y = context.actor.radius_y * body_record(context.body).height_scale * kHoldHeight;
    const Vec3 euler{0.f, context.actor.euler.y + kPi, 0.f};
    Actor& held = *target.peer.actor;
    actor_set_world_position(held, point); // 0x00041730, EDI target, ESI point
    const Vec3 placed = actor_world_position(held); // 0x000412A0, EDI target body +0x18
    std::memcpy(target.peer.body.data() + offsetof(BodyRecord, position.x), &placed, sizeof(placed));
    set_runtime_player_actor_rotation(held, held, target.peer.body, euler); // 0x0002B560, EAX target, ECX euler
}

// Xbox 0x0008BB90
void message_runtime_blood_suck_state(BloodSuckState& state, std::span<const u32> message,
    RuntimeBloodSuckStateContext& context)
{
    if (message[0] == 0x7C)
    {
        if (message[1] == state.target)
        {
            state.target = 0u;
        }
    }
    else if (message[0] == 0x80)
    {
        set_runtime_blood_suck_phase(5, state, context); // 0x0008BBD0
    }
}

// Xbox 0x0008BBD0, EDI state
void set_runtime_blood_suck_phase(u32 phase, BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const u32 previous = state.phase;
    if (previous == phase)
    {
        return;
    }
    if (previous >= 2 && previous <= 3 && state.sound_playing != 0)
    {
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
        state.sound_playing = 0u;
    }
    state.phase = phase;
    start_runtime_blood_suck_animation(state, context); // 0x0008BD30, EAX state
    switch (phase)
    {
    case 2:
    case 3:
    {
        const char* sound = phase == 2 ? state.bite_sound.data() : state.struggle_sound.data();
        if (sound[0] == 0)
        {
            return;
        }
        state.sound_playing = 1u;
        request_actor_sound(context.actor, 1, sound, context.sounds); // The tail jump to 0x00041D00, EDX actor
        return;
    }
    case 4:
    {
        RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008BBD0");
        if (state.thrown == 0)
        {
            throw_runtime_blood_suck_target(state, context); // 0x0008BF90, EAX actor
        }
        if (state.held != 0)
        {
            release_runtime_body_hold(target.peer.body, target.body_message); // 0x00020000, EAX target body
            const u16 count = u16(body_record(target.peer.body).move_holds - 1);
            body_record(target.peer.body).move_holds = count;
            adjust_live_holds(target.view->live_attachments, -1); // 0x000420B0, EAX target
            register_actor_model_anchors(*target.peer.actor, target.view->anchors); // 0x0003EEB0, EBX target
            clear_runtime_body_bit(*target.peer.bits, 0x3A); // 0x0001F030, EAX target body
            state.held = 0u;
        }
        if (state.hidden != 0)
        {
            register_actor_render_bins(target.view->render); // 0x0003CAE0
            state.hidden = 0u;
            if (target.held != nullptr && target.held_view != nullptr
                && actor_spawn_owner_unavailable(*target.held)) // 0x001460C0
            {
                register_actor_render_bins(target.held_view->render); // 0x0003CAE0
            }
        }
        return;
    }
    case 5:
    {
        const u32 target_address = state.target;
        const RuntimeJumpOnPeer* rider = nullptr;
        RuntimeBloodSuckTarget target;
        if (target_address != 0)
        {
            target = context.target_at(target_address);
            rider = &target.peer;
        }
        release_runtime_rider(rider, context.actor_address); // 0x00138190, EAX target, EDX actor
        const u16 key = context.body_slot_key(state.grabbed != 0 ? 0x38 : 0x20); // Body vtable +0x38 / +0x20.
        state.base.next_key = key;
        return;
    }
    default:
        return;
    }
}

// Xbox 0x0008BD30, EAX state
void start_runtime_blood_suck_animation(const BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const u32 phase = state.phase;
    if (phase < 1 || phase > 4)
    {
        return;
    }
    const std::array<u32, 4> found_words{state.reach_animation, state.bite_animation, state.struggle_animation,
        state.throw_animation};
    if (found_words[size_t(phase - 1)] == 0)
    {
        return;
    }
    const auto found = blood_suck_records.find(body_state_key(state));
    const AnimationRecord* record = found == blood_suck_records.end() ? nullptr : found->second[phase - 1];
    if (record == nullptr)
    {
        throw Error("blood suck state starts an animation record it has no host view of");
    }
    start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0, ECX record, ESI body
}

// Xbox 0x0008BD80, EAX target record, ESI actor
bool runtime_blood_suck_grab_point(u32 target_record, const BloodSuckState& state, Vec3& output,
    RuntimeBloodSuckStateContext& context)
{
    if (target_record == 0)
    {
        return false;
    }
    const u32 target_address = read_u32(context.target_record(target_record), 0x10);
    const RuntimeBloodSuckTarget target = context.target_at(target_address);
    if (target.peer.actor == nullptr)
    {
        throw Error("blood suck state 0x0008BD80 reads the bound of a target that is not a scene actor");
    }
    Vec3 center;
    float radius;
    actor_model_world_bound(*target.peer.actor, center, radius); // 0x0003DA00, EAX target
    const u8* shape = find_runtime_actor_shape(context.actor, 0x20002, 0,
        i16(state.grab_shape)); // 0x00042560, EAX actor, BL 0, DI the shape
    if (shape == nullptr)
    {
        return false;
    }
    const EntityBonePose* pose = nullptr;
    if (context.actor.model != nullptr)
    {
        pose = context.actor.model->alternate_shape_pose; // Actor +0x204 -> +0x168.
        if (pose == nullptr)
        {
            pose = context.actor.model->primary_shape_pose; // +0x164.
        }
    }
    Vec3 point;
    const float reach = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape), context.actor.xform,
        pose, point); // 0x00056330, EAX actor +0x2AC
    if (!runtime_collision_spheres_overlap(center, radius, point, reach)) // 0x0015A270
    {
        return false;
    }
    output = point;
    return true;
}

// Xbox 0x0008BE60, EAX state
bool drain_runtime_blood_suck_target(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = state.drain_timer;
    if (0.f < timer)
    {
        const float left = timer - rate * step;
        state.drain_timer = left;
        if (!(0.f < left)) // COMISS; JC keeps it.
        {
            state.drain_timer = 0.f;
        }
    }
    if (!(kDrainEpsilon > state.drain_timer)) // COMISS; JBE skips the bite.
    {
        return false;
    }
    if (state.target == 0)
    {
        return true;
    }
    const RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008BE60");
    const std::span<u8> target_body = target.peer.body;
    auto& processor = target.damage->processor;
    if (!runtime_body_down_or_unhurtable(target_body, *target.peer.actor, *target.peer.bits,
            processor.player_blocked, processor.player_modes, processor.player_mode)) // 0x00021190, ECX target body
    {
        const i32 energy = i32(body_record(target_body).energy) - 1;
        const u16 left = energy <= 0 ? u16(0) : u16(energy);
        body_record(target_body).energy = left;
        if (left == 0)
        {
            set_runtime_blood_suck_phase(4, state, context); // 0x0008BBD0, EDI state
            return true;
        }
        notify_runtime_body_damage(context.game, target.peer.address, context.actor_address, 1, 0, 0, 0, 1,
            0); // 0x00024A00, ECX target, EDX actor
    }
    state.drain_timer = kOne;
    return false;
}

// Xbox 0x0008BF50, EAX state
void track_runtime_blood_suck_struggle(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008BF50");
    if (target.peer.input == nullptr)
    {
        throw Error("blood suck state 0x0008BF50 reads the pad of a target with none (actor +0x960)");
    }
    std::array<float, 2> axes{};
    runtime_input_axes(*target.peer.input, axes, context.input_state); // 0x001085F0, EAX target +0x960
    update_runtime_player_input_tracking(retail_bytes(state.tracking), axes,
        context.struggle_preset, 1, context.clock); // 0x00109810, EDI 0x00302728
}

// Xbox 0x0008BF90, EAX actor
void throw_runtime_blood_suck_target(BloodSuckState& state, RuntimeBloodSuckStateContext& context)
{
    const RuntimeBloodSuckTarget target = target_of(state, context, "blood suck state 0x0008BF90");
    const std::span<u8> target_body = target.peer.body;
    const Actor& target_actor = *target.peer.actor;
    auto& processor = target.damage->processor;
    const u16 actor_class = player_record(context.actor_bytes).damage_class;
    const bool target_player = target_actor.mask_mode == 2 || target_actor.mask_flag != 0;
    u32 amount;
    if (target_player && actor_class != 0)
    {
        amount = u32(i32(runtime_class_damage_value(0x47, actor_class, processor.tables))); // 0x00133BD0; CVTTSS2SI
    }
    else
    {
        amount = u32(runtime_actor_damage_value(0x28, target_actor.damage_class, processor.tables)); // 0x00028DA0
        const bool live =
            runtime_actor_is_live_player(context.actor, player_record(context.actor_bytes).live) // 0x00028D20
            && processor.player_damage != 0;
        if (amount != 0 && live)
        {
            amount = body_record(target_body).energy;
        }
    }
    if ((target.peer.bits->bytes[0] & 0x80) != 0
        || (target_player && runtime_player_damage_enabled(target_actor, processor.player_blocked,
                processor.player_modes, processor.player_mode) == 0)) // 0x00028D50
    {
        amount = 0;
    }
    subtract_runtime_body_damage_health(target_body, target_actor, *target.peer.bits, amount, 1,
        processor.player_blocked, processor.player_modes, processor.player_mode); // 0x00021120, ESI target body
   body_record(target_body).unk_164 = 0;
   body_record(target_body).stagger = kThrowPush;
   body_record(target_body).unk_278 = 0;
   body_record(target_body).damage_kind = 6;
   body_record(target_body).damage_amount = amount;
   body_record(target_body).unk_26C = context.actor_address;
   body_record(target_body).attacker = context.actor_address;
   body_record(target_body).unk_274 = context.actor_address;
    const u16 zero = 0;
    body_record(target_body).unk_162 = zero;
    auto& values = processor.parameter_scratch;
    runtime_body_damage_parameters(target_actor, actor_class, 0x6A, processor.tables, processor.push_keys,
        values[0], values[1], values[2], values[3]); // 0x00023A50, EBX target body, ESI 0x6A
    const float yaw = context.actor.euler.y; // Actor +0x18.
    const Vec3 direction{float(retail_sin(yaw)), 0.f, float(retail_cos(yaw))}; // 0x00153320, 0x00153230
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - kOne;
    const float scaled = (fraction * kThrowSpread + kThrowLeast) * values[3];
    select_runtime_body_death_motion(target_body, direction, values[0], values[1], values[2], scaled,
        processor); // 0x00024760, EAX target body, ESI direction
    notify_runtime_body_damage(context.game, target.peer.address, context.actor_address, amount, 1, 0, 0, 1,
        0); // The inlined 0x00024A00: 0x00122C70 and 0x00116810.
    state.thrown = 1u;
}

// Xbox 0x00021190, ECX body
bool runtime_body_down_or_unhurtable(std::span<const u8> body, const Actor& actor,
    const EntityBitBlock& bits, std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode)
{
    (void)body;
    if ((bits.bytes[0] & 0x80) != 0)
    {
        return true;
    }
    return (actor.mask_mode == 2 || actor.mask_flag != 0)
        && runtime_player_damage_enabled(actor, blocked, modes, mode) == 0; // 0x00028D50
}

// Xbox 0x0003B1B0, EAX actor, ECX slot
float runtime_actor_animation_slot_position(const ActorAnimationState& animation, u32 slot)
{
    if (animation.controller == nullptr || animation.controller->slots[slot].player == nullptr)
    {
        return 0.f;
    }
    return animation.controller->slots[slot].player->position;
}

void install_runtime_blood_suck_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeBloodSuckStateContext&(u32 body_address)> context_of)
{
    blood_suck_context_of = std::move(context_of);
    types[0x61] = ObjectFactoryType{9, blood_suck_state_lifecycle}; // Getter 0x0008B200.
}
}
