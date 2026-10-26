#include "body_jump_on_state.h"
#include "actor_bind_tree.h"
#include "actor_live_attachments.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "body_animation_setup.h"
#include "body_hit_state.h"
#include "body_orientation.h"
#include "player_attachment_helpers.h"
#include "player_goal_cleanup.h"
#include "player_mind_control.h"
#include "player_mind_requests.h"
#include "player_mind_targets.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = jump_on_state_field;
namespace parameter = jump_on_state_parameter;
namespace ride = ride_field;

std::function<RuntimeJumpOnStateContext&(u32)> jump_on_context_of;

// The five animation records (+0x148, +0x14C, +0x88, +0x8C, +0x90), by the state's bytes.
struct JumpOnAnimations
{
    const AnimationRecord* take_off = nullptr;
    const AnimationRecord* jump_off = nullptr;
    const AnimationRecord* ride = nullptr;
    const AnimationRecord* hit = nullptr;
    const AnimationRecord* hold = nullptr;
};
std::unordered_map<const u8*, JumpOnAnimations> jump_on_animations;

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

float global(RuntimeJumpOnStateContext& context, u32 address)
{
    return read_f32(context.globals, address - 0x00428680);
}

i32 head_marker(RuntimeJumpOnStateContext& context)
{
    return i32(read_u32(context.globals, 0x00428684 - 0x00428680));
}

float roll(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

float actor_step(RuntimeJumpOnStateContext& context)
{
    return runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
}

std::span<u8> ride_of(std::span<u8> state)
{
    return state.subspan(field::kRide, ride::kSize);
}

std::span<u8> state_of_ride(std::span<u8> ride_bytes)
{
    return {ride_bytes.data() - field::kRide, field::kSize};
}

JumpOnAnimations& animations_of(std::span<const u8> state)
{
    return jump_on_animations[state.data()];
}

const AnimationRecord& record(const AnimationRecord* found, const char* caller)
{
    if (found == nullptr)
    {
        throw Error(std::string(caller) + " starts a jump-on animation the body's tables do not hold");
    }
    return *found;
}

// 0x0001AFA0 with ECX the record.
void start_record(const AnimationRecord* found, const char* caller, RuntimeJumpOnStateContext& context)
{
    start_runtime_body_animation_record(record(found, caller), context.actions);
}

// 0x00030DB0 on the actor's controller slot.
void retime_slot(i32 slot, float duration, RuntimeJumpOnStateContext& context)
{
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller == nullptr)
    {
        return;
    }
    retime_mina_playback(controller->slots.at(size_t(slot)).player.get(), duration, controller->dirty);
}

// The inlined 0x0001AFF0: the record started, then its slot retimed.
void start_retimed(const AnimationRecord* found, float duration, const char* caller,
    RuntimeJumpOnStateContext& context)
{
    const AnimationRecord& started = record(found, caller);
    start_runtime_body_animation_record(started, context.actions);
    retime_slot(i32(started.slot), duration, context);
}

const MinaPlayer* first_player(RuntimeJumpOnStateContext& context)
{
    const AnimationController* controller = context.animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

// 0x00028C30: the mind's target record, or 0 without a mind.
u32 mind_target(RuntimeJumpOnStateContext& context)
{
    return context.mind.empty() ? 0u : read_u32(context.mind, 0x10);
}

// Mind +0x10's record +0x10: the target actor (the record read through address zero without one).
RuntimeJumpOnPeer target_peer(RuntimeJumpOnStateContext& context, const char* caller)
{
    const u32 target = mind_target(context);
    if (target == 0)
    {
        throw Error(std::string(caller) + " reads the target actor of a mind with no target");
    }
    return context.peer_at(read_u32(context.target_at(target), 0x10));
}

// 0x0003B8B0 and the host's views.
RuntimeJumpOnPeer host_of(RuntimeJumpOnStateContext& context, const char* caller)
{
    Actor* host = actor_bound_parent(context.actor);
    if (host == nullptr)
    {
        throw Error(std::string(caller) + " reads the host of a rider bound to none");
    }
    return context.peer_at(context.address_of(host));
}

// The head marker record (kind 4, index DAT_00428684 clamped to the count), or null.
const u8* head_record(const RuntimeJumpOnPeer& peer, i32 index)
{
    const u32 count = count_runtime_body_model_kind(peer.model, 4); // 0x00040980
    if (count == 0)
    {
        return nullptr;
    }
    if (i32(count - 1) < index)
    {
        index = i32(count - 1);
    }
    return find_runtime_body_model_record(peer.model, 4, index); // 0x000409D0
}

const ModelPoseBuffer* pose_of(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return nullptr;
    }
    const RuntimeModelInstance& model = *actor.model->runtime; // Actor +0x204: +0x168, else +0x164.
    return model.alternate != nullptr ? model.alternate : model.primary;
}

const ModelBoneBinding& binding_of(const u8* row)
{
    // The 0x34-byte locator record as 0x00051CE0 / 0x00051E40 read it.
    return *reinterpret_cast<const ModelBoneBinding*>(row);
}

// The inlined test of 0x0009DF90: the animation passed `time` since its last step (+0x10C the previous position).
bool window_crossed(const MinaPlayer& player, float time)
{
    if (player.position == player.previous)
    {
        return false;
    }
    float from = player.previous;
    float to = player.position;
    if (0.f > player.speed)
    {
        from = player.position;
        to = player.previous;
    }
    if (to > from)
    {
        return time >= from && to > time;
    }
    return time >= from || to > time;
}

// The leap's progress for the arc's height: 1 for a zero length, else the elapsed fraction clamped to 0..1.
float leap_fraction(std::span<const u8> leap)
{
    const float length = read_f32(leap, 0x48);
    if (0.f == length)
    {
        return 1.f; // DAT_0032B0F8.
    }
    const float fraction = (read_f32(leap, 0x4C) - length) / (0.f - length);
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

// The body point along the leap with the arc's height (sin(fraction * pi) * DAT_00428680) added.
void place_on_leap(std::span<const u8> state, RuntimeJumpOnStateContext& context)
{
    const std::span<const u8> leap = state.subspan(0x18, 0x50);
    const float x = read_f32(state, 0x54);
    const float y = read_f32(state, 0x58);
    const float z = read_f32(state, 0x5C);
    const float fraction = leap_fraction(leap);
    const long double arc = retail_sin(fraction * 3.1415927410125732f); // 0x00153320, DAT_0032B148.
    put_float(context.body, 0x18, x);
    put_float(context.body, 0x20, z);
    put_float(context.body, 0x1C, float(arc * static_cast<long double>(global(context, 0x00428680))
        + static_cast<long double>(y)));
}
}

namespace
{
u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

bool construct_jump_on_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_jump_on_state(state, parameters, jump_on_context_of(body_address_of(state)));
}

void destroy_jump_on_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_jump_on_state(state, jump_on_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* jump_on_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A00, jump_on_state_object_sizes, construct_jump_on_state_view,
        destroy_jump_on_state_view};
    return &table;
}
}

// Xbox 0x0009A0F0
void jump_on_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0009A4C0
bool initialize_runtime_jump_on_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeJumpOnStateContext& context)
{
    const std::span<u8> body = context.body;
    put_u16(body, 0x30, u16(read_u16(body, 0x30) + 1));
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + 1);
    }
    body[0x6A] = u8(body[0x6A] + 1);
    set(state, field::knocked, 0u);
    set(state, field::keep_riding, get(parameters, parameter::keep_riding));
    set(state, field::aborted, 0u);
    set(state, field::claimed, 0u);
    set(state, field::target, 0u);
    // Each name through the inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
    const auto find = [&](u32 offset)
    {
        return find_runtime_body_animation(context.image_string(read_u32(parameters, offset)), context.actions);
    };
    JumpOnAnimations& found = animations_of(state);
    found.take_off = find(parameter::take_off.offset);
    set(state, field::take_off, found.take_off != nullptr ? 1u : 0u);
    found.jump_off = find(parameter::jump_off.offset);
    set(state, field::jump_off, found.jump_off != nullptr ? 1u : 0u);
    const std::span<u8> riding = ride_of(state);
    found.ride = find(parameter::ride.offset);
    set(riding, ride::ride, found.ride != nullptr ? 1u : 0u);
    found.hit = find(parameter::hit.offset);
    set(riding, ride::hit, found.hit != nullptr ? 1u : 0u);
    found.hold = find(parameter::hold.offset);
    set(riding, ride::hold, found.hold != nullptr ? 1u : 0u);
    put_float(context.globals, 0x004286B0 - 0x00428680, get(parameters, parameter::take_off_blend));
    const char* sound = reinterpret_cast<const char*>(parameters.data() + parameter::hold_sound.offset);
    std::memcpy(riding.data() + ride::hold_sound.offset, sound, std::strlen(sound) + 1);
    set(state, field::shape_mode, u32(set_runtime_body_shape_mode(0, context.shape))); // 0x0001FEB0, BL 0
    set(state, field::released, 1u);
    set(state, field::phase, 1u);
    start_retimed(found.take_off, global(context, 0x004286B0), "0x0009A4C0", context); // 0x0001AFF0
    return true;
}

// Xbox 0x0009A710
void destroy_runtime_jump_on_state(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    const std::span<u8> body = context.body;
    const u32 target = mind_target(context);
    put_u16(body, 0x30, u16(read_u16(body, 0x30) - 1));
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] - 1);
    }
    set_runtime_body_shape_mode(u8(get(state, field::shape_mode)), context.shape); // 0x0001FEB0
    body[0x6A] = u8(body[0x6A] - 1);
    if (get(state, field::released) == 0)
    {
        hold_runtime_jump_on_body(1, state, context); // The inlined 0x0009A420 release.
    }
    if (target != 0 && read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const std::span<const u8> record_bytes = context.target_at(target);
        const Vec3 point{read_f32(record_bytes, 0x20), read_f32(record_bytes, 0x24), read_f32(record_bytes, 0x28)};
        face_runtime_body_point(body, context.actor, point); // 0x00134510
    }
    release_runtime_jump_on_claim(state, context); // The inlined 0x0009B300.
    if (get(state, field::phase) == 3)
    {
        end_runtime_ride(ride_of(state), context); // 0x00099550
    }
    set(state, field::phase, 0u);
    jump_on_animations.erase(state.data());
}

// Xbox 0x0009AB00
void tick_runtime_jump_on_state(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    set(state, body_state_field::elapsed, actor_step(context) + get(state, body_state_field::elapsed));
    if (get(state, field::aborted) != 0)
    {
        end_runtime_jump_on_state(context); // 0x0009B290
        return;
    }
    // Each phase change below is the inlined 0x0009B160: a ride (phase 3) ends first.
    const auto leave_ride = [&]() -> Actor*
    {
        return get(state, field::phase) == 3 ? end_runtime_ride(ride_of(state), context) : nullptr; // 0x00099550
    };
    switch (get(state, field::phase))
    {
    case 1:
    {
        if (!runtime_jump_on_window_open(global(context, 0x004286A0), context)) // 0x0009A170
        {
            return;
        }
        const u32 target = mind_target(context); // 0x00028C30
        if (target == 0)
        {
            end_runtime_jump_on_state(context);
            return;
        }
        set(state, field::target, read_u32(context.target_at(target), 0x10));
        const RuntimeJumpOnPeer peer = context.peer_at(get(state, field::target));
        if (!claim_runtime_rider(&peer, context.actor_address)) // 0x00138140
        {
            set(state, field::target, 0u);
            end_runtime_jump_on_state(context);
            return;
        }
        set(state, field::claimed, 1u);
        if (runtime_jump_on_blocked(state, context)) // 0x0009A250
        {
            release_runtime_rider(&peer, context.actor_address); // 0x00138190
            end_runtime_jump_on_state(context);
            return;
        }
        set_runtime_jump_on_phase(2, state, context); // 0x0009B160
        return;
    }
    case 2:
    {
        Vec3 position;
        Vec3 euler;
        if (!runtime_jump_on_target_pose(position, euler, context)) // 0x0009A2E0
        {
            Actor* ridden = leave_ride();
            set(state, field::phase, 4u);
            start_runtime_jump_off_leap(ridden, state, context); // 0x0009B010
            return;
        }
        set(state, field::leap_end, position);
        set(state, field::leap_target_euler, euler);
        if (!step_runtime_jump_on_leap(state, context)) // 0x0009A870
        {
            return;
        }
        leave_ride();
        set(state, field::phase, 3u);
        start_runtime_ride(ride_of(state), context); // 0x000994E0
        return;
    }
    case 3:
    {
        if (!tick_runtime_ride(state, get(state, field::keep_riding), context)) // 0x00099610
        {
            return;
        }
        if (get(state, field::keep_riding) != 0)
        {
            set(state, field::knocked, 1u);
        }
        Actor* ridden = leave_ride();
        set(state, field::phase, 4u);
        start_runtime_jump_off_leap(ridden, state, context); // 0x0009B010
        return;
    }
    case 4:
        if (!step_runtime_jump_off_leap(state, context)) // 0x0009A9C0
        {
            return;
        }
        leave_ride();
        set(state, field::phase, 5u);
        hold_runtime_jump_on_body(1, state, context); // 0x0009A420
        retime_slot(0, global(context, 0x004286C0), context); // 0x00030DB0, EAX 0
        release_runtime_jump_on_claim(state, context); // 0x0009B300
        return;
    case 5:
        if (!runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
        {
            return;
        }
        leave_ride();
        set(state, field::phase, 6u);
        if (get(state, field::knocked) != 0)
        {
            put_float(context.body, 0x23C, 0.75f); // DAT_0032B2AC.
            set(state, body_state_field::next_key, context.body_slot_key(0x34));
            return;
        }
        set(state, body_state_field::next_key, context.body_slot_key(0x20));
        return;
    default:
        return;
    }
}

// Xbox 0x0009AE60
void message_runtime_jump_on_state(std::span<u8> state, std::span<const u32> message,
    RuntimeJumpOnStateContext& context)
{
    switch (message[0])
    {
    case 0x7C:
        if (get(state, field::claimed) != 0 && message[1] == get(state, field::target))
        {
            release_runtime_jump_on_claim(state, context); // 0x0009B300
            set(state, field::aborted, 1u);
        }
        break;
    case 0x86:
        if (message[2] != 7)
        {
            break;
        }
        set(state, field::aborted, 1u);
        break;
    case 0x5D:
        set(state, field::aborted, 1u);
        break;
    case 0x80:
        set(state, field::aborted, 1u);
        release_runtime_jump_on_claim(state, context); // 0x0009B300
        break;
    default:
        break;
    }
    const i32 phase = i32(get(state, field::phase));
    if (phase <= 0)
    {
        return;
    }
    if (phase <= 2)
    {
        if (message[0] != 0x6E)
        {
            return;
        }
        const u32 target = mind_target(context);
        if (target == 0)
        {
            throw Error("0x0009AE60 reads the target actor of a mind with no target");
        }
        if (message[1] == read_u32(context.target_at(target), 0x10) && message[4] != 0)
        {
            end_runtime_jump_on_state(context); // 0x0009B290
        }
        return;
    }
    if (phase == 3)
    {
        message_runtime_ride(ride_of(state), message, context); // 0x00099940
    }
}

// Xbox 0x0009A170, EAX actor
bool runtime_jump_on_window_open(float time, RuntimeJumpOnStateContext& context)
{
    const MinaPlayer* player = first_player(context);
    if (player == nullptr)
    {
        return false;
    }
    return window_crossed(*player, time);
}

// Xbox 0x0009A1E0, EAX target
bool runtime_jump_on_target_open(u32 target, RuntimeJumpOnStateContext& context)
{
    const u32 actor = read_u32(context.target_at(target), 0x10);
    if (actor == 0)
    {
        return false;
    }
    const RuntimeJumpOnPeer peer = context.peer_at(actor);
    if (head_record(peer, head_marker(context)) == nullptr || read_u16(peer.body, 0x94) == 0)
    {
        return false;
    }
    return ((~u32(peer.bits->bytes[7])) >> 1 & 1u) != 0;
}

// Xbox 0x0009A250, EAX state, EDI actor
bool runtime_jump_on_blocked(std::span<const u8> state, RuntimeJumpOnStateContext& context)
{
    if (get(state, field::aborted) != 0 || get(state, field::claimed) == 0 || (context.runtime_flags & 0x400) != 0)
    {
        return true;
    }
    const u32 target = mind_target(context);
    if (target == 0 || !runtime_jump_on_target_open(target, context))
    {
        return true;
    }
    const std::span<const u8> record_bytes = context.target_at(target);
    const Vec3 point{read_f32(record_bytes, 0x20), read_f32(record_bytes, 0x24), read_f32(record_bytes, 0x28)};
    // 0x0012C970: from the actor's point (0x000412A0) along the floor, region mode 1, the actor's class.
    i32 last_cell = 0;
    const Vec3 from = actor_world_position(context.actor);
    return !entity_detail::navigation_path_clear(from, point, 1, 0, 0, 0, nullptr, &last_cell,
        i16(context.actor.class_key)); // 0x0012CC50
}

// Xbox 0x0009A2E0, EAX actor, EBX euler
bool runtime_jump_on_target_pose(Vec3& position, Vec3& euler, RuntimeJumpOnStateContext& context)
{
    const u32 target = mind_target(context);
    if (target == 0)
    {
        return false;
    }
    const u32 actor = read_u32(context.target_at(target), 0x10);
    if (actor == 0 || !runtime_jump_on_target_open(target, context))
    {
        return false;
    }
    const RuntimeJumpOnPeer peer = context.peer_at(actor);
    runtime_jump_on_marker_position(peer, head_marker(context), position); // 0x0009A330
    runtime_jump_on_marker_euler(peer, head_marker(context), euler); // 0x0009A3A0
    return true;
}

// Xbox 0x0009A330, EAX target
void runtime_jump_on_marker_position(const RuntimeJumpOnPeer& target, i32 marker, Vec3& output)
{
    const u8* row = head_record(target, marker);
    if (row == nullptr)
    {
        throw Error("0x0009A330 transforms the head marker of a target with none");
    }
    AnimationMatrix world;
    std::memcpy(world.data(), target.actor->xform, sizeof(target.actor->xform)); // Target +0x2AC.
    model_bone_world_point(binding_of(row), pose_of(*target.actor), &world, output); // 0x00051CE0
}

// Xbox 0x0009A3A0, EAX target, EBX output
void runtime_jump_on_marker_euler(const RuntimeJumpOnPeer& target, i32 marker, Vec3& output)
{
    const u8* row = head_record(target, marker);
    if (row == nullptr)
    {
        throw Error("0x0009A3A0 transforms the head marker of a target with none");
    }
    AnimationMatrix world;
    AnimationMatrix matrix;
    std::memcpy(world.data(), target.actor->xform, sizeof(target.actor->xform)); // Target +0x2AC.
    model_bone_world_matrix(binding_of(row), pose_of(*target.actor), world, matrix); // 0x00051E40
    output = actor_matrix_euler(matrix.data()); // 0x00104040, EBX output
}

// Xbox 0x0009A420, ECX release, EAX state
void hold_runtime_jump_on_body(u32 release, std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    const std::span<u8> body = context.body;
    const auto adjust_holds = [&](i32 step)
    {
        // Actor +0x8F8: each live attachment's +0x15 hold count.
        for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
             live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
        {
            live->retained_15 = u8(live->retained_15 + step);
        }
    };
    if (release != 0)
    {
        if (get(state, field::released) != 0)
        {
            return;
        }
        set(state, field::released, 1u);
        adjust_holds(-1);
        body[0x6D] = u8(body[0x6D] - 1);
        decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
        body[0x69] = u8(body[0x69] - 1);
        std::array<u8, 0x20> scratch{};
        notify_runtime_body_spatial_shape(body, scratch, context.spatial); // 0x0001D780
        return;
    }
    if (get(state, field::released) == 0)
    {
        return;
    }
    set(state, field::released, 0u);
    adjust_holds(1);
    body[0x6D] = u8(body[0x6D] + 1);
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    body[0x69] = u8(body[0x69] + 1);
    auto* first = static_cast<ActorLiveAttachment*>(context.live_attachments.next); // Actor +0x8F8.
    if (first == nullptr)
    {
        throw Error("0x0009A420 releases the first live attachment of an actor with none");
    }
    release_actor_live_attachment(*first); // 0x0012B0B0, ESI the record
}

// Xbox 0x0009A870, EAX state, EBX body
bool step_runtime_jump_on_leap(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    const bool done = step_runtime_leap(state.subspan(0x18, 0x50), actor_step(context)); // 0x001036D0
    place_on_leap(state, context);
    const Vec3 euler = get(state, RetailField<Vec3, 0x30>{});
    normalize_euler_angles(context.actor.euler, euler); // 0x00105AA0, EDI actor +0x14
    context.actor.xform_valid = 0; // Actor +0x2A8.
    return done;
}

// Xbox 0x0009A9C0, EBX state, EDI body
bool step_runtime_jump_off_leap(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    const bool done = step_runtime_leap(state.subspan(0x18, 0x50), actor_step(context)); // 0x001036D0
    place_on_leap(state, context);
    const u32 target = mind_target(context);
    const std::span<u8> body = context.body;
    if (target != 0 && read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const std::span<const u8> record_bytes = context.target_at(target);
        const Vec3 point{read_f32(record_bytes, 0x20), read_f32(record_bytes, 0x24), read_f32(record_bytes, 0x28)};
        face_runtime_body_point(body, context.actor, point); // 0x00134510
    }
    return done;
}

// Xbox 0x0009AF60, EAX actor
void start_runtime_jump_on_leap(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    const Vec3 start = actor_world_position(context.actor); // 0x000412A0
    const Vec3 start_euler = actor_world_euler(context.actor); // 0x00041820
    Vec3 end;
    Vec3 target_euler;
    if (!runtime_jump_on_target_pose(end, target_euler, context))
    {
        // The leap would start from the stack's leftovers.
        throw Error("0x0009AF60 starts a leap onto a target whose head marker is gone");
    }
    const MinaPlayer* player = first_player(context);
    const float duration = player != nullptr ? player->duration : 0.f; // Slot 0 player +0x20.
    start_runtime_leap(state.subspan(0x18, 0x50), target_euler, start_euler, start, end,
        (global(context, 0x004286A4) - global(context, 0x004286A0)) * duration); // 0x00103650
}

// Xbox 0x0009B010, EAX ridden
void start_runtime_jump_off_leap(Actor* ridden, std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    if (ridden != nullptr)
    {
        context.actor_callouts(context.address_of(ridden), 0x50); // 0x0003BE20 over ridden +0x8C
    }
    const RuntimeJumpOnPeer from = context.peer_at(ridden != nullptr ? context.address_of(ridden)
                                                                     : context.actor_address);
    Vec3 landing;
    if (!find_runtime_jump_landing(from, 10.f, landing, context)) // 0x00138220, 0x41200000
    {
        landing = get(state, RetailField<Vec3, 0x3C>{}); // Where the leap on began.
    }
    start_retimed(animations_of(state).jump_off, global(context, 0x004286BC), "0x0009B010", context);
    const MinaPlayer* player = first_player(context);
    const float position = player != nullptr ? player->position : 0.f; // Slot 0 player +0x14.
    const float duration = player != nullptr ? player->duration : 0.f; // +0x20.
    // x87: (DAT_004286AC - position) * duration, stored as a float.
    const float length = float((static_cast<long double>(global(context, 0x004286AC)) - position)
        * static_cast<long double>(duration));
    const Vec3 start = actor_world_position(context.actor); // 0x000412A0
    const Vec3 start_euler = actor_world_euler(context.actor); // 0x00041820
    start_runtime_leap(state.subspan(0x18, 0x50), context.default_euler, start_euler, start, landing,
        length); // 0x00103650, ESI DAT_0050DD74
}

// Xbox 0x0009B160, ESI state
void set_runtime_jump_on_phase(u32 phase, std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    Actor* ridden = get(state, field::phase) == 3 ? end_runtime_ride(ride_of(state), context) : nullptr; // 0x00099550
    set(state, field::phase, phase);
    switch (phase)
    {
    case 1:
        start_retimed(animations_of(state).take_off, global(context, 0x004286B0), "0x0009B160", context); // 0x0001AFF0
        return;
    case 2:
        retime_slot(0, global(context, 0x004286B4), context); // 0x00030DB0, EAX 0
        start_runtime_jump_on_leap(state, context); // 0x0009AF60
        hold_runtime_jump_on_body(0, state, context); // The tail jump to 0x0009A420, ECX 0
        return;
    case 3:
        start_runtime_ride(ride_of(state), context); // The tail jump to 0x000994E0
        return;
    case 4:
        start_runtime_jump_off_leap(ridden, state, context); // 0x0009B010
        return;
    case 5:
        hold_runtime_jump_on_body(1, state, context); // 0x0009A420
        retime_slot(0, global(context, 0x004286C0), context); // 0x00030DB0, EAX 0
        release_runtime_jump_on_claim(state, context); // 0x0009B300
        return;
    case 6:
        if (get(state, field::knocked) != 0)
        {
            put_float(context.body, 0x23C, 0.75f); // DAT_0032B2AC.
            set(state, body_state_field::next_key, context.body_slot_key(0x34));
            return;
        }
        set(state, body_state_field::next_key, context.body_slot_key(0x20));
        return;
    default:
        return;
    }
}

// Xbox 0x0009B290
void end_runtime_jump_on_state(RuntimeJumpOnStateContext& context)
{
    select_runtime_body_action(context.body, context.body_slot_key(0x20), context.actions); // The inlined 0x00027650
    std::array<u32, 2> message{0x52, 0}; // Word 1 is the caller's stack slot.
    context.message(context.actor_address, message); // Actor vtable +0xC.
}

// Xbox 0x0009B300, EDX state, EDI actor
void release_runtime_jump_on_claim(std::span<u8> state, RuntimeJumpOnStateContext& context)
{
    if (get(state, field::claimed) == 0)
    {
        return;
    }
    const u32 target = get(state, field::target);
    if (target != 0)
    {
        const RuntimeJumpOnPeer peer = context.peer_at(target);
        const u32 rider = read_u32(peer.body, 0x718);
        if (rider != 0 && rider == context.actor_address)
        {
            put_word(peer.body, 0x718, 0);
        }
    }
    set(state, field::claimed, 0u);
    set(state, field::target, 0u);
}

// Xbox 0x000994E0, ECX ride, EAX actor
void start_runtime_ride(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    put_float(ride_bytes, 0x30, 0.f);
    put_float(ride_bytes, 0x2C, 0.f);
    put_float(ride_bytes, 0x38, 0.f);
    put_float(ride_bytes, 0x34, 0.f);
    set(ride_bytes, ride::shake_state, 1u);
    put_float(ride_bytes, 0x3C, 0.f);
    attach_runtime_ride(context); // 0x00099DF0
    context.actor.model_flags &= 0xFFFDFFFDu; // Actor +0x1F0.
    put_word(context.body, 0x258, read_u32(context.body, 0x258) + 1);
    set(ride_bytes, ride::phase, 0u);
    set_runtime_ride_phase(ride_bytes, 1, context); // 0x00099CA0
}

// Xbox 0x00099550, EBX actor
Actor* end_runtime_ride(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    const u32 phase = get(ride_bytes, ride::phase);
    if (phase == 2)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    else if (phase == 3)
    {
        set(ride_bytes, ride::ride_time, (global(context, 0x0042869C) - global(context, 0x00428698))
            * roll(context.random) + global(context, 0x00428698));
    }
    set(ride_bytes, ride::phase, 0u);
    Actor* host = detach_runtime_ride(context); // 0x00099EE0
    context.actor.model_flags |= 0x20002u;
    const u32 count = read_u32(context.body, 0x258) - 1;
    put_word(context.body, 0x258, count);
    if (count == 0)
    {
        const std::array<u32, 2> message{0x4A, 0}; // Word 1 is the caller's stack slot.
        context.body_message(message); // Body vtable +0xC.
    }
    return host;
}

// Xbox 0x00099610, EBX ride
bool tick_runtime_ride(std::span<u8> state, u32 keep_riding, RuntimeJumpOnStateContext& context)
{
    const std::span<u8> ride_bytes = ride_of(state);
    const std::span<u8> body = context.body;
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820, EDI body +0x72C
    std::memcpy(body.data() + 0x72C, &euler, sizeof(euler));
    const std::span<const u8> actor_bytes = context.actor_bytes;
    const u32 group = (read_u16(actor_bytes, 0xCDC) == 2 || read_u32(actor_bytes, 0x93C) != 0) ? 1u : 0u;
    if (context.player_blocked[group] == 0 && context.player_modes[context.player_mode] != 0)
    {
        const float step = actor_step(context);
        const auto count = [&](size_t offset)
        {
            float value = read_f32(ride_bytes, offset);
            if (0.f < value)
            {
                value = value - step;
                put_float(ride_bytes, offset, value);
                if (!(0.f < value))
                {
                    put_float(ride_bytes, offset, 0.f);
                }
            }
        };
        count(ride::ride_time.offset);
        count(ride::hold_time.offset);
        track_runtime_ride_input(ride_bytes, context); // 0x0009A060
    }
    if (runtime_jump_on_blocked(state, context)) // 0x0009A250
    {
        return true;
    }
    // The inlined 0x00099CA0 to phase 3: its hit animation.
    const auto hit = [&]()
    {
        set_runtime_ride_phase(ride_bytes, 3, context);
    };
    switch (get(ride_bytes, ride::phase))
    {
    case 1:
        if (keep_riding != 0 && get(ride_bytes, ride::shake_state) == 2 && query_runtime_ride_host(1, context) != 0)
        {
            set_runtime_ride_phase(ride_bytes, 2, context); // 0x00099CA0
            return false;
        }
        steer_runtime_ride_host(ride_bytes, context); // 0x00099AD0
        if (keep_riding != 0 && get(ride_bytes, ride::ride_time) != 0.f)
        {
            return false;
        }
        hit();
        return false;
    case 2:
        if (get(ride_bytes, ride::shake_state) == 1 && query_runtime_ride_host(0, context) != 0)
        {
            return true;
        }
        if (get(ride_bytes, ride::hold_time) == 0.f && get(ride_bytes, ride::shaken) == global(context, 0x004286DC))
        {
            return true;
        }
        if (get(ride_bytes, ride::ride_time) != 0.f)
        {
            return false;
        }
        hit();
        return false;
    case 3:
        if (keep_riding != 0)
        {
            if ((context.bits.bytes[0] & 4) != 0) // Body +0x15C.
            {
                strike_runtime_ride_host(ride_bytes, context); // 0x00099F60
            }
            if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, ECX 0
            {
                set_runtime_ride_phase(ride_bytes, 1, context); // 0x00099CA0
            }
            return false;
        }
        if (!runtime_actor_animation_slot_complete(context.animation, 0))
        {
            return false;
        }
        strike_runtime_ride_host(ride_bytes, context);
        return true;
    default:
        return false;
    }
}

// Xbox 0x00099940, ECX ride, EAX actor, ESI message
void message_runtime_ride(std::span<u8> ride_bytes, std::span<const u32> message, RuntimeJumpOnStateContext& context)
{
    const auto end_action = [&]()
    {
        select_runtime_body_action(context.body, context.body_slot_key(0x34), context.actions); // 0x00027650
    };
    const auto host_address = [&]()
    {
        Actor* host = actor_bound_parent(context.actor); // 0x0003B8B0
        return host == nullptr ? 0u : context.address_of(host);
    };
    switch (message[0])
    {
    case 0x31:
        if (message[1] == host_address())
        {
            end_action();
        }
        return;
    case 0x6E:
        if (message[1] == host_address() && message[4] != 0)
        {
            end_action();
        }
        return;
    case 0x80:
        end_action();
        return;
    case 0x54:
        if (get(ride_bytes, ride::phase) == 2)
        {
            set_runtime_ride_phase(ride_bytes, 1, context); // 0x00099CA0
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00099A20, EAX body
u32 query_runtime_ride_host(u32 value, RuntimeJumpOnStateContext& context)
{
    const RuntimeJumpOnPeer host = host_of(context, "0x00099A20");
    std::array<u32, 3> message{0x53, value, 0};
    context.message(host.address, message); // Host vtable +0xC; the reply is word 2.
    return message[2];
}

// Xbox 0x00099A60, ECX ride
void reset_runtime_ride_wobble(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    set(ride_bytes, ride::wobble_maximum, 1.2f); // DAT_0032B24C.
    set(ride_bytes, ride::wobble_minimum, 0.5f); // DAT_0032B0F4.
    set(ride_bytes, ride::wobble_delay, roll(context.random) * 0.7f + 0.5f); // DAT_0032B2BC.
    set(ride_bytes, ride::wobble, 0.f);
}

// Xbox 0x00099AD0, EAX actor
void steer_runtime_ride_host(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    const RuntimeJumpOnPeer host = host_of(context, "0x00099AD0");
    if (tick_runtime_random_timer(ride_bytes.subspan(0x14, 0xC), context.random, context.clock)) // 0x001050A0
    {
        const float angle = (roll(context.random) * 10.f + 20.f) * 0.01745329238474369f; // DAT_0032B10C, DAT_0032B234.
        const float side = roll(context.random);
        set(ride_bytes, ride::wobble, 0.5f < side ? 0.f - angle : angle); // DAT_0032B0F4.
    }
    Vec3 point = actor_world_position(*host.actor); // 0x000412A0
    point.y = host.actor->radius_y * read_f32(host.body, 0x884) + point.y; // Host +0x34, body +0x884.
    Vec3 direction{};
    add_sound_direction(direction, global(context, 0x004286C8),
        host.actor->euler.y + get(ride_bytes, ride::wobble), global(context, 0x004286C4)); // 0x00106220
    if (host.mind.empty())
    {
        throw Error("0x00099AD0 steers a host with no mind");
    }
    put_word(host.mind, 0x3C, 1);
    put_float(host.mind, 0x40, point.x + direction.x);
    put_float(host.mind, 0x44, direction.y + point.y);
    put_float(host.mind, 0x48, point.z + direction.z);
}

// Xbox 0x00099C40
void roll_runtime_ride_idle(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    set(ride_bytes, ride::ride_time, (global(context, 0x00428694) - global(context, 0x00428690))
        * roll(context.random) + global(context, 0x00428690));
}

// Xbox 0x00099CA0, EDI ride
void set_runtime_ride_phase(std::span<u8> ride_bytes, u32 phase, RuntimeJumpOnStateContext& context)
{
    const u32 previous = get(ride_bytes, ride::phase);
    if (previous == 2)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    else if (previous == 3)
    {
        set(ride_bytes, ride::ride_time, (global(context, 0x0042869C) - global(context, 0x00428698))
            * roll(context.random) + global(context, 0x00428698));
    }
    set(ride_bytes, ride::phase, phase);
    const JumpOnAnimations& found = animations_of(state_of_ride(ride_bytes));
    switch (phase)
    {
    case 1:
        start_record(found.ride, "0x00099CA0", context); // 0x0001AFA0, ECX ride +8
        if (previous == 0)
        {
            reset_runtime_ride_wobble(ride_bytes, context); // 0x00099A60
            roll_runtime_ride_idle(ride_bytes, context); // 0x00099C40
        }
        return;
    case 2:
    {
        start_record(found.hold, "0x00099CA0", context); // 0x0001AFA0, ECX ride +0x10
        const char* sound = reinterpret_cast<const char*>(ride_bytes.data() + ride::hold_sound.offset);
        if (sound[0] != '\0')
        {
            queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f, sound,
                context.sounds); // 0x0012A260, EBX actor +0x614
        }
        set(ride_bytes, ride::hold_time, (global(context, 0x0042868C) - global(context, 0x00428688))
            * roll(context.random) + global(context, 0x00428688));
        return;
    }
    case 3:
        start_record(found.hit, "0x00099CA0", context); // 0x0001AFA0, ECX ride +0xC
        return;
    default:
        return;
    }
}

// Xbox 0x00099DF0, EBX actor
void attach_runtime_ride(RuntimeJumpOnStateContext& context)
{
    const RuntimeJumpOnPeer target = target_peer(context, "0x00099DF0");
    const u8* row = head_record(target, head_marker(context));
    if (row == nullptr)
    {
        throw Error("0x00099DF0 binds the rider to a head marker its target does not have");
    }
    bind_runtime_actor_marker_now(context.actor, target.actor, row, 0, context.default_position,
        context.default_euler); // 0x0003B500, ECX actor, EAX target
    put_word(context.body, 0x728, 0);
    if (target.mind.empty())
    {
        throw Error("0x00099DF0 clears the chosen actor of a target with no mind");
    }
    put_word(target.mind, 0xC, 0);
    normalize_euler_angles(context.actor.euler, context.default_euler); // 0x00105AA0, ESI DAT_0050DD74
    context.actor.xform_valid = 0; // Actor +0x2A8.
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820, EDI body +0x72C
    std::memcpy(context.body.data() + 0x72C, &euler, sizeof(euler));
    if (context.shape.secondary != nullptr) // Actor +0x310.
    {
        context.shape.secondary_mode = 0; // Actor +0x30C.
    }
    std::array<u32, 3> message{0x44, context.actor_address, target.address};
    context.message(target.address, message); // Target vtable +0xC.
    context.message(context.actor_address, message); // Actor vtable +0xC.
}

// Xbox 0x00099EE0, EAX actor
Actor* detach_runtime_ride(RuntimeJumpOnStateContext& context)
{
    Actor* host = actor_bound_parent(context.actor); // 0x0003B8B0
    if (host != nullptr)
    {
        detach_actor_bind(context.actor); // 0x0003B560, EBX actor
    }
    put_word(context.body, 0x728, 1);
    if (host != nullptr)
    {
        const u32 address = context.address_of(host);
        std::array<u32, 3> message{0x43, context.actor_address, address};
        context.message(address, message); // Host vtable +0xC.
        context.message(context.actor_address, message); // Actor vtable +0xC.
    }
    if (context.shape.secondary != nullptr) // Actor +0x310.
    {
        context.shape.secondary_mode = 1; // Actor +0x30C.
    }
    return host;
}

// Xbox 0x00099F60, EAX actor
void strike_runtime_ride_host(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    const RuntimeJumpOnPeer host = host_of(context, "0x00099F60");
    Vec3 point;
    bool placed = false;
    if (host.actor->model != nullptr && host.actor->model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(host.actor->model->cpu, 4) & 0xFFu; // 0x00052FC0
        if (joint != 0xFFu && joint != 0xFFFFFFFFu)
        {
            actor_transform_joint_point(*host.actor, u16(joint), context.default_position, point); // 0x0003AEB0
            placed = true;
        }
    }
    if (!placed)
    {
        throw Error("0x00099F60 sends a hit at a host joint the host's model does not hold");
    }
    const RuntimeJumpOnPeer target = target_peer(context, "0x00099F60");
    std::array<u32, 5> message{0x2A, context.actor_address, std::bit_cast<u32>(point.x),
        std::bit_cast<u32>(point.y), std::bit_cast<u32>(point.z)};
    context.message(target.address, message); // Target vtable +0xC.
    const u32 strikes = get(ride_bytes, ride::strikes) + 1;
    set(ride_bytes, ride::strikes, strikes);
    if (i32(strikes) >= 5 && get(ride_bytes, ride::hat_intro) == 0 && read_u16(target.body, 0x94) != 0
        && queue_runtime_imp_hat_intro(context.story_byte, context.intro) != 0) // 0x0013FE60
    {
        set(ride_bytes, ride::hat_intro, 1u);
    }
}

// Xbox 0x0009A060, EAX actor
void track_runtime_ride_input(std::span<u8> ride_bytes, RuntimeJumpOnStateContext& context)
{
    const RuntimeJumpOnPeer host = host_of(context, "0x0009A060");
    std::array<float, 2> axes{};
    if (host.input == nullptr)
    {
        throw Error("0x0009A060 reads the pad of a host with none (actor +0x960)");
    }
    runtime_input_fourth_axes(*host.input, axes, context.input_state); // 0x001087D0, EAX host +0x960
    bool busy;
    if ((host.bits->bytes[3] & 2) != 0)
    {
        busy = true;
    }
    else if ((host.bits->bytes[2] & 2) != 0)
    {
        busy = runtime_body_target_state_active(host.body); // 0x00134650, ECX host body
    }
    else
    {
        busy = false;
    }
    const u32 accumulate = get(ride_bytes, ride::hold_time) == 0.f && !busy && get(ride_bytes, ride::phase) == 2
        ? 1u : 0u;
    update_runtime_player_input_tracking(ride_bytes.subspan(ride::kTracking, 0x18), axes,
        context.globals.subspan(0x004286D0 - 0x00428680, 0x10), accumulate, context.clock); // 0x00109810
}

// Xbox 0x001050A0, ECX timer
bool tick_runtime_random_timer(std::span<u8> timer, u32& random, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    float value = read_f32(timer, 0);
    if (!(0.f < value))
    {
        return false;
    }
    value = value - step;
    put_float(timer, 0, value);
    if (0.f < value)
    {
        return false;
    }
    put_float(timer, 0, 0.f);
    put_float(timer, 0, (read_f32(timer, 8) - read_f32(timer, 4)) * roll(random) + read_f32(timer, 4));
    return true;
}

// Xbox 0x00103650
void start_runtime_leap(std::span<u8> leap, const Vec3& target_euler, const Vec3& start_euler, const Vec3& start,
    const Vec3& end, float length)
{
    std::memcpy(leap.data() + 0x24, &start, sizeof(start));
    std::memcpy(leap.data() + 0x3C, &start, sizeof(start));
    std::memcpy(leap.data() + 0x30, &end, sizeof(end));
    std::memcpy(leap.data() + 0x0C, &start_euler, sizeof(start_euler));
    std::memcpy(leap.data() + 0x18, &start_euler, sizeof(start_euler));
    std::memcpy(leap.data() + 0x00, &target_euler, sizeof(target_euler));
    put_float(leap, 0x48, length);
    put_float(leap, 0x4C, length);
}

// Xbox 0x001036D0, ESI leap
bool step_runtime_leap(std::span<u8> leap, float step)
{
    float left = read_f32(leap, 0x4C);
    if (left > 0.f)
    {
        left = left - step;
        put_float(leap, 0x4C, left);
        if (!(0.f < left))
        {
            put_float(leap, 0x4C, 0.f);
        }
    }
    const float fraction = leap_fraction(leap);
    for (size_t axis = 0; axis < 3; ++axis)
    {
        const float from = read_f32(leap, 0x24 + axis * 4);
        put_float(leap, 0x3C + axis * 4, (read_f32(leap, 0x30 + axis * 4) - from) * fraction + from);
    }
    for (size_t axis = 0; axis < 3; ++axis)
    {
        put_float(leap, 0x18 + axis * 4, step_runtime_angle_toward(read_f32(leap, axis * 4),
            read_f32(leap, 0xC + axis * 4), fraction)); // 0x001013A0
    }
    return read_f32(leap, 0x4C) == 0.f;
}

// Xbox 0x00138220, EAX from, the actor on the stack
bool find_runtime_jump_landing(const RuntimeJumpOnPeer& from, float extra, Vec3& output,
    RuntimeJumpOnStateContext& context)
{
    const Vec3 center = actor_world_position(*from.actor); // 0x000412A0
    const float scale = from.actor->radius_xz; // From +0x38.
    const float body_reach = read_f32(from.body, 0x78) * scale;
    float sword = 0.f;
    const u32 held = read_u32(from.body, 0x48);
    if (held != 0)
    {
        const RuntimeJumpOnPeer weapon = context.peer_at(held);
        if (weapon.bytes[0x93D] == 1 && weapon.actor->model != nullptr && weapon.actor->model->cpu.data() != nullptr)
        {
            sword = float(actor_model_sweep_radius(*weapon.actor)); // 0x0003D9A0, EDI the weapon
        }
    }
    const float inner = read_f32(from.body, 0x888) * scale + (sword + body_reach);
    const float minimum = context.actor.radius_xz * read_f32(context.body, 0x888) + inner;
    return entity_detail::select_navigation_spawn_position(i32(read_u32(context.actor_bytes, 0xCEC)), nullptr,
        center, minimum, minimum + extra, nullptr, nullptr, 0, 0, output, context.random,
        context.occupancy_initial); // 0x00111420, EAX 0
}

// Xbox 0x000265A0, EAX body
bool claim_runtime_body_rider(std::span<u8> body, const EntityBitBlock& bits, u32 rider)
{
    if (read_u32(body, 0x718) == 0 && (bits.bytes[3] & 8) == 0)
    {
        put_word(body, 0x718, rider);
        return true;
    }
    return false;
}

// Xbox 0x00138140, EAX target
bool claim_runtime_rider(const RuntimeJumpOnPeer* target, u32 rider)
{
    if (target == nullptr || target->actor == nullptr)
    {
        return false;
    }
    if ((target->bits->bytes[3] & 8) != 0 || read_u16(target->body, 0x94) == 0 || read_u32(target->body, 0x718) != 0)
    {
        return false;
    }
    claim_runtime_body_rider(target->body, *target->bits, rider); // 0x000265A0
    return true;
}

// Xbox 0x00138190, EAX target, EDX rider
bool release_runtime_rider(const RuntimeJumpOnPeer* target, u32 rider)
{
    if (target == nullptr || target->actor == nullptr)
    {
        return false;
    }
    const u32 current = read_u32(target->body, 0x718);
    if (current != 0 && current == rider)
    {
        put_word(target->body, 0x718, 0);
        return true;
    }
    return false;
}

// Xbox 0x0013FE60
u32 queue_runtime_imp_hat_intro(u8& story_byte, RuntimeIntroNotices& context)
{
    if (runtime_intro_notice_ready(context) == 0) // 0x0013FF70
    {
        return 0;
    }
    if ((story_byte & 8) == 0)
    {
        story_byte = u8(story_byte | 8);
        const char* name = "intros_imphat"; // 0x0032A3B4 + 1.
        size_t index = 0;
        char value;
        do
        {
            value = name[index];
            context.intro_name[index] = value;
            ++index;
        } while (value != '\0');
        context.intro_pending = 1;
    }
    return 1;
}

void install_runtime_jump_on_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeJumpOnStateContext&(u32 body_address)> context_of)
{
    jump_on_context_of = std::move(context_of);
    types[0x79] = ObjectFactoryType{9, jump_on_state_lifecycle}; // Getter 0x0009A110.
}
}
