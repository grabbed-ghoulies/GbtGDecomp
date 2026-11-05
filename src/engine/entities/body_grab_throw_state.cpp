#include "body_grab_throw_state.h"
#include "actor_fields.h"
#include <cstddef>
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "body_message_helpers.h"
#include "player_mind_feedback.h"
#include "player_mind_control.h"
#include "camera_director.h"
#include "actor_spawn_eligibility.h"
#include "actor_collision_culling.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_pose_updates.h"
#include "actor_runtime_queries.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "body_damage_notices.h"
#include "body_damage_processors.h"
#include "body_damage_tables.h"
#include "body_hit_state.h"
#include "body_orientation.h"
#include "body_scoop_whack_state.h"
#include "contact_sweep.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "player_mind_requests.h"
#include "segment_records.h"
#include "shape_transform.h"
#include "../navigation/navigation.h"
#include "../scene/scene_markers.h"
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
namespace parameter = grab_throw_state_parameter;

constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kReachScale = 0.75f; // DAT_0032B2AC.
constexpr float kGrabFrom = std::bit_cast<float>(0x3E19999Au); // DAT_0032B168, 0.15.
constexpr float kGrabUntil = 0.25f; // DAT_0032B104.
constexpr float kThrowAt = std::bit_cast<float>(0x3F0CCCCDu); // DAT_0032B52C and the 0x0003B1F0 push, 0.55.
constexpr float kThrowPush = std::bit_cast<float>(0x3F7D70A4u); // DAT_0032B228, 0.99.
constexpr float kThrowSpread = std::bit_cast<float>(0x3E4CCCD0u); // DAT_0032B5D4.
constexpr float kThrowLeast = std::bit_cast<float>(0x3F666666u); // DAT_0032B174, 0.9.
constexpr i16 kThrowMarkerKind = 0x1A;

std::function<RuntimeGrabThrowStateContext&(u32)> grab_throw_context_of;

// The model binding (0x000408D0) behind the state's +0x20 word, held natively by the state's bytes.
std::unordered_map<const u8*, const u8*> grab_throw_bindings;

// Each of the actor's live attachment records (+0x8F8) holds `step` more.
void adjust_live_holds(entity_detail::ListHead& live_attachments, i32 step)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// Actor +0x50's slot 0 player position, or 0 with no controller or player.
bool slot_position(const ActorAnimationState& animation, float& position)
{
    if (animation.controller == nullptr)
    {
        position = 0.f;
        return false;
    }
    const MinaPlayer* player = animation.controller->slots[0].player.get();
    position = player == nullptr ? 0.f : player->position;
    return true;
}

RuntimeBloodSuckTarget victim_of(const GrabThrowState& state, RuntimeGrabThrowStateContext& context, const char* reader)
{
    RuntimeBloodSuckTarget victim = context.target_at(state.victim);
    if (victim.peer.actor == nullptr || victim.peer.body.empty() || victim.peer.bits == nullptr)
    {
        throw Error(std::string(reader) + " reads a grabbed victim that is not a scene actor with a body");
    }
    return victim;
}

// The victim body's +0x718 holder, cleared when it is this actor.
void release_victim(const GrabThrowState& state, RuntimeGrabThrowStateContext& context)
{
    if (state.victim == 0)
    {
        return;
    }
    const RuntimeBloodSuckTarget victim = victim_of(state, context, "grab and throw state");
    const u32 holder = body_record(victim.peer.body).rider;
    if (holder != 0 && holder == context.actor_address)
    {
       body_record(victim.peer.body).rider = 0;
    }
}

bool construct_grab_throw_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    GrabThrowState& state = body_state_view<GrabThrowState>({object.bytes.get(), object.size});
    return initialize_runtime_grab_throw_state(state, parameters, grab_throw_context_of(state.base.body));
}

void destroy_grab_throw_state_view(FactoryObject& object)
{
    GrabThrowState& state = body_state_view<GrabThrowState>({object.bytes.get(), object.size});
    destroy_runtime_grab_throw_state(state, grab_throw_context_of(state.base.body));
    grab_throw_bindings.erase(body_state_key(state));
}

const ObjectFactoryVtable* grab_throw_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505900, grab_throw_state_object_sizes, construct_grab_throw_state_view,
        destroy_grab_throw_state_view};
    return &table;
}
}

// Xbox 0x00094780
void grab_throw_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(GrabThrowState);
    output.parameters = 0x14;
}

// Xbox 0x00094800
bool initialize_runtime_grab_throw_state(GrabThrowState& state, std::span<const u8> parameters,
    RuntimeGrabThrowStateContext& context)
{
    const std::span<u8> body = context.body;
    const u32 record = context.mind.empty() ? 0u : mind_record(context.mind).target_record;
    if (record == 0)
    {
        throw Error("grab and throw state 0x00094800 reads the mind's target record through address zero");
    }
    const std::span<const u8> target = context.target_record(record);
    state.grab_callout = get(parameters, parameter::grab_callout);
    state.hand_shape = get(parameters, parameter::hand_shape);
    const u8* binding = find_actor_model_binding(context.actor, i16(get(parameters, parameter::binding))); // 0x000408D0
    grab_throw_bindings[body_state_key(state)] = binding;
    state.abandoned = 0u;
    state.binding = binding == nullptr ? 0u : 1u;
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    state.hand = Vec3{};
    state.hand_radius = 0.f;
    if (target[0x14] == 0)
    {
        state.victim = mind_target_record(target).actor;
    }
    const u16 key = actor_marker_id(context.actor_bytes);
    if (key != 0)
    {
        u16 id = next_runtime_marker_id(0, kThrowMarkerKind, 1, context.markers); // 0x0011F1C0, AX 0
        while (id != 0)
        {
            const u8* marker = find_runtime_marker(context.markers, i16(id));
            const u16 value = marker == nullptr ? u16(0) : read_u16({marker, 0xA}, 8);
            if (value == key)
            {
                state.marker = id;
                break;
            }
            id = next_runtime_marker_id(id, kThrowMarkerKind, 0, context.markers); // 0x0011F1C0, AX the id
        }
    }
    if (state.marker == 0)
    {
        state.marker = farthest_runtime_marker(kThrowMarkerKind, position, context.markers); // 0x0011F0B0
    }
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    adjust_live_holds(context.live_attachments, 1);
    body_record(body).unk_6D = u8(body_record(body).unk_6D + 1);
    body_record(body).unk_6A = u8(body_record(body).unk_6A + 1);
    body_record(body).unk_309 = u8(body_record(body).unk_309 + 1);
    body_record(body).unk_30A = u8(body_record(body).unk_30A + 1);
    body_record(body).unk_30B = u8(body_record(body).unk_30B + 1);
    body_record(body).unk_30C = u8(body_record(body).unk_30C + 1);
    bool held = false;
    if (state.victim != 0)
    {
        const RuntimeBloodSuckTarget victim = victim_of(state, context, "grab and throw state 0x00094800");
        const std::span<u8> victim_body = victim.peer.body;
        if ((victim.peer.bits->bytes[3] & 8) == 0 && body_record(victim_body).energy != 0
            && body_record(victim_body).rider == 0)
        {
           body_record(victim_body).rider = context.actor_address;
            held = true;
        }
    }
    if (!held)
    {
        state.abandoned = 1u;
    }
    const Vec3 point{mind_target_record(target).shape.position.x, mind_target_record(target).shape.position.y,
        mind_target_record(target).shape.position.z};
    i32 last_cell = 0;
    if (!entity_detail::navigation_path_clear(actor_world_position(context.actor), point, 1, 0, 0, 0, nullptr,
            &last_cell, i16(read_u16(context.actor_bytes, 6)))) // 0x0012CC50, EAX the actor's point
    {
        state.abandoned = 1u;
    }
    return true;
}

// Xbox 0x00094A30
void destroy_runtime_grab_throw_state(GrabThrowState& state, RuntimeGrabThrowStateContext& context)
{
    const std::span<u8> body = context.body;
    body_record(body).unk_309 = u8(body_record(body).unk_309 - 1);
    body_record(body).unk_30A = u8(body_record(body).unk_30A - 1);
    body_record(body).unk_30B = u8(body_record(body).unk_30B - 1);
    body_record(body).unk_30C = u8(body_record(body).unk_30C - 1);
    adjust_live_holds(context.live_attachments, -1);
    body_record(body).unk_6D = u8(body_record(body).unk_6D - 1);
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
    if (state.latched != 0)
    {
        decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
        state.latched = 0u;
    }
    if (state.thrown == 0 && state.grabbed != 0 && state.victim != 0)
    {
        throw_runtime_grab_throw_victim(state, context); // 0x000951B0
        state.thrown = 1u;
    }
    release_victim(state, context);
}

// Xbox 0x00094B00
void tick_runtime_grab_throw_state(GrabThrowState& state, RuntimeGrabThrowStateContext& context)
{
    const std::span<u8> body = context.body;
    Actor& actor = context.actor;
    const u32 record = context.mind.empty() ? 0u : mind_record(context.mind).target_record;
    state.base.elapsed = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    level_body_euler(body);
    if (state.thrown == 0)
    {
        if (state.grabbed == 0 && record != 0)
        {
            const std::span<const u8> target = context.target_record(record);
            const Vec3 point{mind_target_record(target).shape.position.x,
                mind_target_record(target).shape.position.y, mind_target_record(target).shape.position.z};
            i32 last_cell = 0;
            if (!entity_detail::navigation_path_clear(actor_world_position(actor), point, 1, 0, 0, 0, nullptr,
                    &last_cell, i16(read_u16(context.actor_bytes, 6)))) // 0x0012C970, EAX actor
            {
                state.abandoned = 1u;
            }
            else
            {
                float position = 0.f;
                const bool controlled = slot_position(context.animation, position);
                if (!controlled || !(kGrabUntil < position)) // COMISS; JC skips the turn past 0.25.
                {
                    face_runtime_body_target(body, actor, target); // 0x001345F0, ECX body
                }
                Vec3 hand;
                if (!(position < kGrabFrom) && !(kGrabUntil < position)
                    && runtime_grab_throw_hand_reaches(state, record, hand, context)) // 0x00095000, EDX record
                {
                    const u32 victim_address = mind_target_record(target).actor;
                    std::array<u32, 6> grab{0x3C, 0, context.actor_address, std::bit_cast<u32>(hand.x),
                        std::bit_cast<u32>(hand.y), std::bit_cast<u32>(hand.z)};
                    context.message(victim_address, grab); // Target vtable +0xC; the reply lands at +4.
                    if (grab[1] != 0)
                    {
                        std::array<u32, 1> held{0x3D};
                        context.message(victim_address, held); // Target vtable +0xC.
                        call_out_runtime_hit_point(hand, victim_address, state.grab_callout,
                            context.actor_callout); // 0x0013AD60, EAX point, ECX target
                        state.grabbed = 1u;
                        state.start_euler = actor_world_euler(actor); // 0x00041820, EDI state +0x3C
                        state.grab_position = position;
                        increment_runtime_body_latches(body, actor, context.latches); // 0x0001FC40, ECX body
                        state.latched = 1u;
                    }
                }
            }
        }
        if (state.thrown == 0 && state.grabbed != 0 && state.victim != 0)
        {
            if (state.marker != 0)
            {
                const Vec3 from = actor_world_position(actor); // 0x000412A0
                Vec3 toward{};
                if (const u8* marker = find_runtime_marker(context.markers, i16(state.marker)))
                {
                    toward = Vec3{read_f32({marker, 0x18}, 0xC), read_f32({marker, 0x18}, 0x10),
                        read_f32({marker, 0x18}, 0x14)}; // 0x0011F260, DI the marker
                }
                float yaw = from.y; // EBX points at the start's +4, which 0x00105F90 overwrites.
                runtime_direction_angle(Vec3{toward.x - from.x, toward.y - from.y, toward.z - from.z}, yaw); // 0x00105F90
                float position = 0.f;
                slot_position(context.animation, position);
                const float start = state.grab_position;
                float fraction = kOne;
                if (kThrowAt != start) // UCOMISS; JNP keeps 1 when they are equal.
                {
                    fraction = (position - start) / (kThrowAt - start);
                    if (fraction > kOne)
                    {
                        fraction = kOne;
                    }
                    else if (0.f > fraction)
                    {
                        fraction = 0.f;
                    }
                }
                (void)actor_world_euler(actor); // 0x00041820 into the euler the three steps then overwrite.
                const Vec3 begin = state.start_euler;
                const Vec3 euler{step_runtime_angle_toward(0.f, begin.x, fraction),
                    step_runtime_angle_toward(yaw, begin.y, fraction),
                    step_runtime_angle_toward(0.f, begin.z, fraction)}; // 0x001013A0, XMM0 the target
                set_runtime_actor_body_euler(actor, euler); // 0x0002B560, EAX actor, ECX euler
            }
            if (runtime_actor_animation_crossed_position(context.animation, 0, kThrowAt)) // 0x0003B1F0, ECX 0
            {
                throw_runtime_grab_throw_victim(state, context); // 0x000951B0
                state.thrown = 1u;
            }
        }
    }
    if (state.abandoned == 0 && context.animation.controller != nullptr)
    {
        const MinaPlayer* player = context.animation.controller->slots[0].player.get();
        if (player != nullptr)
        {
            bool done = false;
            if (player->mode == 1)
            {
                done = 0.f > player->speed ? !(0.f < player->position) : !(player->position < kOne);
            }
            if (!done)
            {
                return;
            }
        }
    }
    state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
}

// Xbox 0x00094F10
void step_runtime_grab_throw_state(GrabThrowState& state, RuntimeGrabThrowStateContext& context)
{
    if (state.grabbed == 0 || state.thrown != 0 || state.victim == 0)
    {
        return;
    }
    const u8* record = grab_throw_bindings[body_state_key(state)];
    if (record == nullptr)
    {
        throw Error("grab and throw state 0x00094F10 places its victim at a model binding it does not have");
    }
    ModelBoneBinding binding;
    std::memcpy(&binding.kind, record, 2);
    std::memcpy(&binding.joint, record + 2, 2);
    std::memcpy(binding.matrix.data(), record + 4, 48);
    Vec3 position;
    actor_binding_world_position(context.actor, binding, position); // 0x00040A60, EDX actor, ESI the point
    const Vec3 euler{0.f, context.actor.euler.y + kPi, 0.f}; // Actor +0x18, DAT_0032B148.
    const RuntimeBloodSuckTarget victim = victim_of(state, context, "grab and throw state 0x00094F10");
    Actor& victim_actor = *victim.peer.actor;
    actor_set_world_position(victim_actor, position); // 0x00041730, EDI victim
   body_record(victim.peer.body).position = actor_world_position(victim_actor); // 0x000412A0, EDI body +0x18
    set_runtime_actor_body_euler(victim_actor, euler); // 0x0002B560, EAX victim
    actor_refresh_transform(victim_actor); // 0x00040DE0
}

// Xbox 0x00094FA0
void message_runtime_grab_throw_state(GrabThrowState& state, std::span<const u32> message,
    RuntimeGrabThrowStateContext& context)
{
    if (message[0] == 0x7C)
    {
        const u32 victim = state.victim;
        if (message[1] != victim)
        {
            return;
        }
        state.victim = 0u;
        if (victim == 0)
        {
            return;
        }
        const RuntimeBloodSuckTarget released = context.target_at(victim);
        if (released.peer.body.empty())
        {
            throw Error("grab and throw state 0x00094FA0 reads a released victim with no body");
        }
        const u32 holder = body_record(released.peer.body).rider;
        if (holder != 0 && holder == context.actor_address)
        {
           body_record(released.peer.body).rider = 0;
        }
        return;
    }
    if (message[0] != 0x80 || state.victim == 0)
    {
        return;
    }
    release_victim(state, context);
}

// Xbox 0x00095000
bool runtime_grab_throw_hand_reaches(GrabThrowState& state, u32 target_record, Vec3& output,
    RuntimeGrabThrowStateContext& context)
{
    const u32 target_address = read_u32(context.target_record(target_record), 0x10);
    const RuntimeBloodSuckTarget target = context.target_at(target_address);
    if (target.peer.actor == nullptr)
    {
        throw Error("grab and throw state 0x00095000 reads the bound of a target that is not a scene actor");
    }
    Vec3 center;
    float radius;
    actor_model_world_bound(*target.peer.actor, center, radius); // 0x0003DA00, EAX target
    radius = radius * kReachScale;
    const u8* shape = find_runtime_actor_shape(context.actor, 0x20002, 0,
        i16(state.hand_shape)); // 0x00042560, EAX actor, BL 0, DI the shape
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
    Vec3 hand;
    const float reach = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape), context.actor.xform,
        pose, hand); // 0x00056330, EAX actor +0x2AC
    const Vec3 last = state.hand;
    bool touches;
    if (last.x == 0.f && last.y == 0.f && last.z == 0.f) // UCOMISS; JP leaves the first zero test.
    {
        touches = runtime_collision_spheres_overlap(center, radius, hand, reach); // 0x0015A270
    }
    else
    {
        const Vec3 movement{hand.x - last.x, hand.y - last.y, hand.z - last.z};
        const Vec3 stationary{}; // DAT_0050DD80.
        float fraction = radius;
        touches = sweep_runtime_collision_sphere_pair(last, state.hand_radius, movement, center,
            radius, stationary, kOne, fraction); // 0x0015AB50
    }
    if (touches)
    {
        output = hand;
    }
    state.hand = hand;
    state.hand_radius = reach;
    return touches;
}

// Xbox 0x000951B0
void throw_runtime_grab_throw_victim(GrabThrowState& state, RuntimeGrabThrowStateContext& context)
{
    const RuntimeBloodSuckTarget victim = victim_of(state, context, "grab and throw state 0x000951B0");
    const std::span<u8> victim_body = victim.peer.body;
    Actor& victim_actor = *victim.peer.actor;
    const u32 victim_address = state.victim;
    Vec3 point = actor_world_position(context.actor); // 0x000412A0
    Vec3 place = actor_world_position(victim_actor); // 0x000412A0
    point.y = place.y;
    const EntityMaskHost host{victim_actor.mask_flag, victim_actor.goal_ring, victim_actor.mask_mode};
    const float radius = body_record(context.body).radius_scale * context.actor.radius_xz;
    if (entity_collect_sphere_contacts(point, radius, 4, camera_collision_mask(host)) != 0) // 0x001227E0, 0x0001FE00
    {
        auto contacts = entity_merge_contact_records(); // 0x00122310
        if (contacts.stride != 0 && (contacts.cursor - contacts.begin) / contacts.stride != 0)
        {
            Vec3 separated;
            separate_runtime_segment_contacts(contacts, point,
                                              context.actor.radius_xz * body_record(context.body).radius_scale,
                                              separated, actor_transform_origin); // 0x000FC3F0
            point = separated;
        }
        engine_heap_free(contacts.block); // 0x000663E0
    }
    if (entity_record_sweep_probe(place, point, 4, camera_collision_mask(host)) != 0) // 0x00122480, EBX the place
    {
        actor_set_world_position(victim_actor, place); // 0x00041730, EDI victim, ESI the place
       body_record(victim_body).position = actor_world_position(victim_actor); // 0x000412A0, EDI body +0x18
    }
    std::array<u32, 3> release{0x57, context.actor_address, victim_address};
    context.message(context.actor_address, release); // The actor's vtable +0xC.
    auto& processor = victim.damage->processor;
    const u16 actor_class = player_record(context.actor_bytes).damage_class;
    const bool victim_player = victim_actor.mask_mode == 2 || victim_actor.mask_flag != 0;
    u32 amount;
    if (victim_player && actor_class != 0)
    {
        amount = u32(i32(runtime_class_damage_value(0x34, actor_class, processor.tables))); // 0x00133BD0; CVTTSS2SI
    }
    else
    {
        amount = u32(runtime_actor_damage_value(0x15, victim_actor.damage_class, processor.tables)); // 0x00028DA0
        const bool live =
            runtime_actor_is_live_player(context.actor, player_record(context.actor_bytes).live) // 0x00028D20
            && processor.player_damage != 0;
        if (amount != 0 && live)
        {
            amount = body_record(victim_body).energy;
        }
    }
    if ((victim.peer.bits->bytes[0] & 0x80) != 0
        || (victim_player && runtime_player_damage_enabled(victim_actor, processor.player_blocked,
                processor.player_modes, processor.player_mode) == 0)) // 0x00028D50
    {
        amount = 0;
    }
    subtract_runtime_body_damage_health(victim_body, victim_actor, *victim.peer.bits, amount, 1,
        processor.player_blocked, processor.player_modes, processor.player_mode); // 0x00021120, ESI victim body
   body_record(victim_body).unk_164 = 0;
   body_record(victim_body).stagger = kThrowPush;
   body_record(victim_body).damage_amount = amount;
   body_record(victim_body).unk_278 = 0;
   body_record(victim_body).damage_kind = 3;
   body_record(victim_body).unk_26C = context.actor_address;
   body_record(victim_body).attacker = context.actor_address;
   body_record(victim_body).unk_274 = context.actor_address;
    const u16 zero = 0;
    body_record(victim_body).unk_162 = zero;
    auto& values = processor.parameter_scratch;
    runtime_body_damage_parameters(victim_actor, actor_class, 0x68, processor.tables, processor.push_keys,
        values[0], values[1], values[2], values[3]); // 0x00023A50, EBX victim body, ESI 0x68
    const float yaw = context.actor.euler.y; // Actor +0x18.
    const Vec3 direction{float(retail_sin(yaw)), 0.f, float(retail_cos(yaw))}; // 0x00153320, 0x00153230
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - kOne;
    const float scaled = (fraction * kThrowSpread + kThrowLeast) * values[3];
    select_runtime_body_death_motion(victim_body, direction, values[0], values[1], values[2], scaled,
        processor); // 0x00024760, EAX victim body, ESI direction
    notify_runtime_body_damage(context.game, victim_address, context.actor_address, amount, 1, 0, 0, 1,
        0); // 0x00122C70 and 0x00116810 over {0x6E, victim, actor, amount, 1, 0, 0, 1}.
}

void install_runtime_grab_throw_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGrabThrowStateContext&(u32 body_address)> context_of)
{
    grab_throw_context_of = std::move(context_of);
    types[0x72] = ObjectFactoryType{9, grab_throw_state_lifecycle}; // Getter 0x000947A0.
}
}
