#include "body_knockdown_state.h"
#include <cstddef>
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "actor_shape_bounds.h"
#include "body_action_ids.h"
#include "body_move_action_slots.h"
#include "player_mind_requests.h"
#include "player_overlay.h"
#include "body_animation_commands.h"
#include "body_damage_notices.h"
#include "body_message_helpers.h"
#include "direction_pitch.h"
#include "entity_collision.h"
#include "player_mind_events.h"
#include "../camera/camera_body_slots.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/navigation.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"

#include <bit>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gbtg
{
namespace parameter = knockdown_state_parameter;

namespace
{
// The seven records the constructor finds live on the host; the state's animations_found flags say whether each
// was found.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 7>> knockdown_records;

// The contact_pool (stride 0x20, grown by 4) of actors that pushed against the flying body: +0 the actor, +4 the
// direction away from it, +0x10 the contact point, +0x1C sent, +0x1D the flag, +0x1E the bone. The rows live on
// the host; the state's contact_pool holds a nonzero marker while the pool exists.
struct KnockdownContact
{
    u32 actor;
    Vec3 direction;
    Vec3 contact;
    u8 sent;
    u8 flag;
    u16 bone;
};
std::unordered_map<const u8*, std::vector<KnockdownContact>> knockdown_contacts;
constexpr u32 kContactPoolMarker = 1;

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

float next_unit(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

Vec3 read_vec(std::span<const u8> bytes, size_t offset)
{
    Vec3 value;
    std::memcpy(&value, bytes.data() + offset, 12);
    return value;
}

bool animation_found(const KnockdownState& state, size_t slot)
{
    return state.animations_found[slot] != 0;
}

const AnimationRecord* record_of(const KnockdownState& state, size_t slot)
{
    if (!animation_found(state, slot))
    {
        return nullptr;
    }
    return knockdown_records.at(body_state_key(state))[slot];
}

// 0x0001AFA0 with ECX the record the slot's found flag stands for (ESI the body).
void start_record(const KnockdownState& state, size_t slot, RuntimeKnockdownContext& context)
{
    const AnimationRecord* record = record_of(state, slot);
    if (record == nullptr)
    {
        throw Error("knockdown animation record is null; 0x0001AFA0 reads its +0x117");
    }
    start_runtime_body_animation_record(*record, context.actions);
}

const MinaPlayer* slot_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    if (controller == nullptr)
    {
        return nullptr;
    }
    return controller->slots.at(0).player.get();
}

float slot_position(const ActorAnimationState& animation)
{
    const MinaPlayer* player = slot_player(animation);
    return player == nullptr ? 0.f : player->position;
}

// The inline phase end: leaving phase 1 drops its callout tokens (0x0013B810).
void leave_phase(KnockdownState& state, u32 phase, RuntimeKnockdownContext& context)
{
    if (state.phase == 1)
    {
        clear_runtime_player_attachment_events(retail_bytes(state.callouts), context.lists);
    }
    state.phase = phase;
    state.phase_time = 0.f;
}

// The inline release of the getting-up count: body +0x69 drops once while the body lives.
void release_getting_up(KnockdownState& state, RuntimeKnockdownContext& context)
{
    if (state.getting_up_released == 0 && body_record(context.body).energy != 0)
    {
        body_record(context.body).unk_69 = u8(body_record(context.body).unk_69 - 1);
        state.getting_up_released = 1;
    }
}

// 0x0009D7B0: message 0x46 to the actor.
void notify_down(RuntimeKnockdownContext& context)
{
    const u32 message[]{0x46};
    context.actor_message(message);
}

// 0x00027620 over the body's +0x73C table.
bool has_action(RuntimeKnockdownContext& context, u16 key)
{
    return runtime_body_has_action_id(context.table, key);
}

// A sphere record's contact copy (0x000FCD70's segment record) as the wall test and the bounce read it.
std::span<const u8> sphere_record(const RuntimeBodyCollisionSphere* sphere)
{
    if (sphere == nullptr)
    {
        throw Error("knockdown reads a null body +0x710 sphere record");
    }
    return {sphere->contact.data(), sphere->contact.size()};
}
}

// Xbox 0x0009C550
void knockdown_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(KnockdownState);
    output.parameters = parameter::kSize;
}

// Xbox 0x00025FA0
void clear_runtime_body_contacts(RuntimeKnockdownContext& context)
{
    if (context.spheres != nullptr)
    {
        RuntimeBodyCollisionSphere* end = context.spheres + body_record(context.body).sphere_count;
        for (RuntimeBodyCollisionSphere* sphere = context.spheres; sphere < end; ++sphere)
        {
            sphere->contact[0x26] = 0;
        }
    }
    body_record(context.body).unk_715 = 0;
    body_record(context.body).unk_716 = 0;
}

// Xbox 0x0009C150
void aim_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    const Vec3 push{0.f - body_record(body).unk_230.x, 0.f - body_record(body).unk_230.y, 0.f
        - body_record(body).unk_230.z};
    const u32 source = body_record(body).unk_26C;
    float angle;
    if (runtime_direction_angle(push, angle))
    {
        state.yaw = angle;
        const float unit = next_unit(context.random);
        // DAT_0032B160, DAT_0032B508.
        state.yaw = (unit * std::bit_cast<float>(0x3EB2B8C3u) + state.yaw) - std::bit_cast<float>(0x3E32B8C3u);
        return;
    }
    if (source != 0)
    {
        // 0x00041900 with EAX the source and this actor pushed: the angle from this actor toward the source.
        const Vec3 first = context.actor_position(source);
        const Vec3 second = actor_world_position(context.actor);
        const Vec3 toward{first.x - second.x, first.y - second.y, first.z - second.z};
        float aim;
        runtime_direction_angle(toward, aim);
        state.yaw = aim;
    }
}

// Xbox 0x00140BF0
void runtime_knockdown_launch(float angle, float height, float distance, float jitter, float time, float spread,
    Vec3& velocity, float& gravity, float& flight, u32& random)
{
    const float first = next_unit(random);
    const float scale = (spread - (0.f - spread)) * first + (0.f - spread);
    time = scale * time + time;
    const float second = next_unit(random);
    const float rise = (scale * height + height) * 2.f; // DAT_0032B110.
    const float lift = rise / (time * 0.5f); // DAT_0032B0F4.
    const float reach = (((jitter - (0.f - jitter)) * second + (0.f - jitter)) * distance + distance) / time;
    velocity = Vec3{0.f, 0.f, 0.f};
    const long double side = retail_sin(angle);
    const long double forward = retail_cos(angle);
    velocity.x = float(side) * reach + velocity.x;
    velocity.z = float(forward) * reach + velocity.z;
    velocity.y = lift;
    gravity = 0.f - (lift * lift) / rise;
    flight = time;
}

// Xbox 0x0009B350
void launch_runtime_knockdown(KnockdownState& state, float distance, float height, float time, float pitch,
    RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    state.saved_gravity = body_record(body).gravity;
    const float span = distance / float(i32(body_record(body).unk_162));
    Vec3 velocity;
    float gravity;
    float flight;
    runtime_knockdown_launch(state.yaw + std::bit_cast<float>(0x40490FDBu), height, span, 0.1f, time,
        0.1f, velocity, gravity, flight, context.random); // DAT_0032B148, DAT_0032B254.
    body_record(body).gravity = gravity;
    state.flight_time = flight;
    state.pitch = pitch * std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    context.actor.anchor = velocity;
    body_record(body).velocity = velocity;
}

// Xbox 0x0009D810
void signal_runtime_knockdown(const Vec3& position, float first, float second, RuntimeKnockdownContext& context)
{
    auto& options = g_actor_creation_options;
    options.flags = 0;
    options.position = position;
    options.euler = Vec3{first, second, 0.f};
    options.flags = 5;
    context.callouts(0x3D, &options);
}

// Xbox 0x0009D150
void enter_runtime_knockdown_phase(KnockdownState& state, u32 phase, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    if (state.phase == 1)
    {
        clear_runtime_player_attachment_events(retail_bytes(state.callouts), context.lists);
    }
    state.phase = phase;
    state.phase_time = 0.f;
    u16 key;
    switch (phase)
    {
    case 1:
    {
        state.swept = 0;
        start_record(state, kKnockdownFlight, context);
        const float flight = state.flight_time;
        AnimationController* controller = context.animation.controller.get();
        if (controller != nullptr)
        {
            MinaPlayer* player = controller->slots.at(0).player.get();
            if (player != nullptr)
            {
                bool blend = true;
                if (player->old_pose == nullptr)
                {
                    // EBX is the player's current pose (+0x10).
                    player->old_pose = copy_animation_pose(*player->pose); // 0x0002D3F0
                    blend = player->old_pose != nullptr;
                }
                if (blend)
                {
                    player->blend_remaining = flight;
                    player->blend_duration = flight;
                }
            }
            controller->dirty = true;
        }
        // 0x0013B7C0: callouts 0x4A record their tokens in the state's callout_tokens.
        context.events.command_attachments = retail_bytes(state.callouts);
        request_runtime_player_attachment_events(context.callout_records, context.events, 0x4A,
            context.address_of(retail_bytes(state.callouts).data()));
        return;
    }
    case 2:
    {
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body_record(body).collision_kind = 0;
        set_runtime_body_shape_mode(2, context.shape);
        actor.anchor = Vec3{actor.anchor.x * 0.7f, actor.anchor.y, actor.anchor.z * 0.7f}; // DAT_0032B264.
        start_record(state, kKnockdownBounceLanding, context);
        state.start_pitch = actor.euler.x; // Actor +0x14.
        state.pose_mark = slot_position(context.animation);
        notify_down(context);
        signal_runtime_knockdown(state.landing_point, std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        if (body_record(body).energy != 0)
        {
            return;
        }
        key = 0x68;
        break;
    }
    case 3:
    {
        set_runtime_body_shape_mode(2, context.shape);
        actor.anchor = Vec3{0.f, 0.f, 0.f};
        body_record(body).gravity = 0.f;
        start_record(state,
            animation_found(state, kKnockdownLandedVariant) ? kKnockdownLandedVariant : kKnockdownLanded,
            context);
        state.start_pitch = actor.euler.x;
        state.start_yaw = actor.euler.y;
        state.pose_mark = slot_position(context.animation);
        notify_down(context);
        const Vec3 landing = state.landing_point;
        signal_runtime_knockdown(landing, 0.f, state.yaw, context);
        const std::array<u32, 5> notice{0x72, context.actor_address, std::bit_cast<u32>(landing.x),
            std::bit_cast<u32>(landing.y), std::bit_cast<u32>(landing.z)};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(notice.data()));
        return;
    }
    case 4:
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, kKnockdownBounce, context);
        signal_runtime_knockdown(state.landing_point, 0.f, state.yaw, context);
        return;
    case 5:
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body_record(body).collision_kind = 0;
        start_record(state, kKnockdownBounceRecovery, context);
        signal_runtime_knockdown(state.landing_point, std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        return;
    case 6:
        state.swept = 0;
        body_record(body).gravity = state.saved_gravity;
        return;
    case 7:
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body_record(body).collision_kind = 1;
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, kKnockdownGettingUp, context);
        notify_down(context);
        state.landing_point = body_record(body).ground_point;
        if (body_record(body).energy != 0)
        {
            return;
        }
        key = 0x69;
        break;
    case 8:
        if (state.contacts_raised != 0)
        {
            clear_runtime_body_contacts(context);
            state.contacts_raised = 0;
        }
        if (body_record(body).energy == 0)
        {
            const float end = state.end_time;
            state.end_timer = end < 0.f ? 1.f : end; // DAT_0032B0F8.
            return;
        }
        state.end_timer = body_record(body).unk_22C / float(i32(body_record(body).unk_160));
        return;
    default:
        return;
    }
    if (has_action(context, key))
    {
        state.death_key = key;
    }
}

// Xbox 0x0009C5D0
bool initialize_runtime_knockdown_state(KnockdownState& state, std::span<const u8> parameters,
    RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    body_record(body).move_holds = u16(body_record(body).move_holds + 1);
    adjust_body_counters(body, 1);
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
   body_record(body).unk_164 = 0;
    body_record(body).stagger = 0.99f; // DAT_0032B228.
    // Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains a +0x15 hold.
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + 1);
    }
    body_record(body).unk_6D = u8(body_record(body).unk_6D + 1);
    context.bits.bytes[4] |= 0x10;
    body_record(body).unk_69 = u8(body_record(body).unk_69 + 1);
    if (body_record(body).overlay_enabled != 0)
    {
       body_record(body).overlay_hold_time = 2.f; // DAT_0032B110.
    }
    release_runtime_player_attachment(0, 0, context.attachment);
    auto& records = knockdown_records[body_state_key(state)];
    for (size_t slot = 0; slot < kKnockdownAnimationCount; ++slot)
    {
        // 0x0003AAA0 with the attached actor's +0xDB0 variant, then without (0x0001AED0's pair).
        const char* name = context.string_at(field_data(parameters, parameter::animations)[slot]);
        records[slot] = find_runtime_body_animation(name, context.actions);
        state.animations_found[slot] = records[slot] != nullptr ? 1 : 0;
    }
    state.getting_up_key = get(parameters, parameter::getting_up_key);
    state.landing_key = get(parameters, parameter::landing_key);
    state.bounce_key = get(parameters, parameter::bounce_key);
    state.fallback_death_key = get(parameters, parameter::fallback_death_key);
    state.attacker = body_record(body).attacker;
    state.saved_shape_mode = set_runtime_body_shape_mode(3, context.shape);
    state.rise_height = actor.radius_y * get(parameters, parameter::rise_height);
    state.bounce_scale = get(parameters, parameter::bounce_scale);
    state.end_time = get(parameters, parameter::end_time);
    state.recovery_mode = get(parameters, parameter::recovery_mode);
    state.body_bit = get(parameters, parameter::body_bit);
    state.first_callout = get(parameters, parameter::first_callout);
    body_record(body).unk_715 = 1;
    state.contacts_raised = 1;
    state.saved_717 = body_record(body).collision_kind;
    body_record(body).collision_kind = 0;
    state.start_point = actor_world_position(actor);
    aim_runtime_knockdown(state, context);
    launch_runtime_knockdown(state, body_record(body).unk_220, body_record(body).unk_224, body_record(body).unk_228,
        get(parameters, parameter::pitch), context);
    enter_runtime_knockdown_phase(state, 1, context);
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    if (body_record(body).energy == 0 && body_record(body).max_energy != 0)
    {
        context.bits.bytes[3] |= 8;
        notify_runtime_body_death_once(body, context.actor_address, context.game);
    }
    switch (get(parameters, parameter::latch_mode))
    {
    case 0:
        state.holds_latches = 0;
        break;
    case 1:
        state.holds_latches = 1;
        break;
    case 2:
        state.holds_latches = body_record(body).energy == 0 ? 1 : 0;
        break;
    default:
        break;
    }
    if (state.holds_latches != 0)
    {
        context.hold_latches(); // 0x00027BE0
    }
    return true;
}

// Xbox 0x0009CA80
void destroy_runtime_knockdown_state(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    if (state.contact_pool != 0)
    {
        knockdown_contacts.erase(body_state_key(state)); // 0x000663E0
        state.contact_pool = 0;
    }
    if (state.contacts_raised != 0)
    {
        clear_runtime_body_contacts(context);
        state.contacts_raised = 0;
    }
    body_record(body).collision_kind = u8(state.saved_717);
    body_record(body).move_holds = u16(body_record(body).move_holds - 1);
    body_record(body).gravity = state.saved_gravity;
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold - 1);
    }
    body_record(body).unk_6D = u8(body_record(body).unk_6D - 1);
    adjust_body_counters(body, -1);
    set_runtime_body_shape_mode(state.saved_shape_mode, context.shape);
    context.bits.bytes[4] &= 0xEF;
    context.bits.bytes[4] &= 0xDF;
    const i32 count = std::bit_cast<i32>(state.callouts.count);
    for (i32 index = 0; index < count; ++index)
    {
        remove_runtime_body_attachment(u16(state.callouts.ids[index]), context.lists);
    }
    state.callouts.count = 0;
    if (state.getting_up_released == 0)
    {
        body_record(body).unk_69 = u8(body_record(body).unk_69 - 1);
        state.getting_up_released = 1;
    }
    const u32 bit = state.body_bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (state.holds_latches != 0)
    {
        body_record(body).unk_7E5 = u8(body_record(body).unk_7E5 - 1);
    }
    knockdown_records.erase(body_state_key(state));
}

// Xbox 0x0009B720
void collide_runtime_knockdown(const KnockdownState& state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context)
{
    if (state.phase == 1 && animation_found(state, kKnockdownLanded))
    {
        reply.accepted = 1;
        const Vec3 velocity = context.actor.anchor;
        if (runtime_knockdown_hits_wall({reply.contact, 0x20}, velocity))
        {
            reply.handled = 1;
        }
    }
}

// Xbox 0x0009D0E0
void message_runtime_knockdown_state(KnockdownState& state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context)
{
    if (reply.id == 0x62)
    {
        collide_runtime_knockdown(state, reply, context);
        return;
    }
    const std::array<u32, 2> words{reply.id, reply.accepted};
    message_runtime_knockdown_state(state, {reinterpret_cast<const u8*>(words.data()), 8}, context);
}

// Xbox 0x0009D0E0
void message_runtime_knockdown_state(KnockdownState& state, std::span<const u8> message,
    RuntimeKnockdownContext& context)
{
    switch (read_u32(message, 0))
    {
    case 0xE:
        if (state.phase == 1)
        {
            // {0xE, actor, push, contact, bone} from the shape push (actor vtable +0x34).
            const Vec3 contact{read_f32(message, 0x14), read_f32(message, 0x18), read_f32(message, 0x1C)};
            record_runtime_knockdown_contact(state, read_u32(message, 4), contact,
                animation_found(state, kKnockdownLanded) ? 0 : 1, read_u16(message, 0x20), context);
        }
        return;
    case 0x62:
        throw Error("knockdown message 0x62 needs the collision reply's contact record");
    case 0x7C:
        forget_runtime_knockdown_contact(state, read_u32(message, 4)); // 0x0009BEA0
        return;
    default:
        return;
    }
}

// Xbox 0x0009BB60
void record_runtime_knockdown_contact(KnockdownState& state, u32 other, const Vec3& contact, u8 flag, u16 bone,
    RuntimeKnockdownContext& context)
{
    if (state.attacker == other)
    {
        return;
    }
    if (state.contact_pool == 0)
    {
        knockdown_contacts[body_state_key(state)].reserve(4); // 0x000F72D0(0x90): 4 rows of 0x20.
        state.contact_pool = kContactPoolMarker;
    }
    auto& rows = knockdown_contacts[body_state_key(state)];
    for (const auto& row : rows)
    {
        if (row.actor == other)
        {
            return;
        }
    }
    if (rows.size() == rows.capacity())
    {
        rows.reserve(rows.size() + 4); // 0x000F73A0: grown by the pool's +2 step.
    }
    KnockdownContact row{};
    row.actor = other;
    row.flag = flag;
    row.bone = bone;
    row.sent = 0;
    row.contact = contact;
    const Vec3 from = context.actor_position(other); // 0x000412A0
    const Vec3 to = actor_world_position(context.actor); // 0x000412A0
    row.direction = Vec3{to.x - from.x, 0.f, to.z - from.z};
    if (row.direction.x != 0.f || row.direction.y != 0.f || row.direction.z != 0.f)
    {
        const float length = row.direction.x * row.direction.x + row.direction.y * row.direction.y
            + row.direction.z * row.direction.z;
        if (length != 0.f)
        {
            const long double factor = retail_reciprocal_sqrt(length); // 0x001534C0
            row.direction.x = float(factor * static_cast<long double>(row.direction.x));
            row.direction.y = float(factor * static_cast<long double>(row.direction.y));
            row.direction.z = float(factor * static_cast<long double>(row.direction.z));
        }
    }
    rows.push_back(row);
}

// Xbox 0x0009BEA0
void forget_runtime_knockdown_contact(KnockdownState& state, u32 other)
{
    if (state.contact_pool == 0)
    {
        return;
    }
    auto& rows = knockdown_contacts[body_state_key(state)];
    for (auto row = rows.begin(); row != rows.end(); ++row)
    {
        if (row->actor == other)
        {
            rows.erase(row); // 0x000F71F0 shifts the later rows down.
            return;
        }
    }
}

// Xbox 0x0009BEE0
void sweep_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const u32 holder = body_record(body).unk_274;
    u32 player = 0;
    if (holder != 0)
    {
        const auto source = context.record_at(holder, 0xCF8);
        player = player_record(source).mode == 2 ? 1 : (player_record(source).case_link != 0 ? 1 : 0);
    }
    Vec3 position = actor_world_position(actor);
    const float radius = float(static_cast<long double>(actor_model_sweep_radius(actor))
        * std::bit_cast<float>(0x3F2AAAABu)); // DAT_0032B204.
    const i32 found = entity_collect_contact_probes(position, radius, 0x140);
    i32 hits = found == 0 ? 0 : context.probe_messages(found, player, context.actor_address);
    if (state.swept != 0)
    {
        Vec3 previous = state.sweep_point;
        const i32 swept = entity_record_point_probe(position, previous, radius, 0x140);
        hits += swept == 0 ? 0 : context.probe_messages(swept, player, context.actor_address);
    }
    state.sweep_point = position;
    state.swept = 1;
    if (hits == 0)
    {
        return;
    }
    float damage = context.hit_damage[1];
    const u16 tag = player_record(context.actor_bytes).damage_class;
    if (tag != 0)
    {
        const float low = float(i32(context.hit_bands[0]));
        const float high = float(i32(context.hit_bands[1]));
        if (high != low)
        {
            const float level = float(i32(context.tag_rows[size_t(tag) * 0x87 - 0x3F]));
            const float far = context.hit_damage[1];
            const float near = context.hit_damage[0];
            const float scaled = ((level - low) / (high - low)) * (far - near) + near;
            if (far > near)
            {
                damage = scaled > far ? far : (near > scaled ? near : scaled);
            }
            else
            {
                damage = far > scaled ? far : (scaled > near ? near : scaled);
            }
        }
    }
    const i32 amount = i32(damage) * hits;
    if (0 < amount)
    {
        const i32 left = i32(body_record(body).energy) - amount;
        const u16 health = left < 1 ? 0 : u16(left);
        body_record(body).energy = health;
        if (health == 0)
        {
            context.bits.bytes[3] |= 8;
            notify_runtime_body_death_once(body, context.actor_address, context.game);
            if (runtime_body_death_effect_selected(context.settings, context.random))
            {
                request_runtime_body_death_effect(1, context.death);
            }
        }
    }
}

// Xbox 0x0009C100
u32 runtime_knockdown_contact_kind(const KnockdownState&, RuntimeKnockdownContext& context)
{
    if (body_record(context.body).unk_716 == 0)
    {
        return 0;
    }
    // A scan that finds no marked record reads +0x10 from address zero.
    const std::span<const u8> record = sphere_record(first_hit_runtime_body_sphere(context.body, context.spheres));
    return read_f32(record, 0x10) > 0.9f ? 2 : 1; // DAT_0032B174; COMISS, JA
}

// Xbox 0x0009BA40
bool runtime_knockdown_hits_wall(std::span<const u8> record, const Vec3& velocity)
{
    float x = velocity.x;
    float y = 0.f;
    float z = velocity.z;
    if (x == 0.f && z == 0.f)
    {
        return false;
    }
    const float length = z * z + x * x;
    if (length != 0.f)
    {
        const long double factor = retail_reciprocal_sqrt(length);
        x = float(static_cast<long double>(x) * factor);
        y = float(0.0L * factor); // DAT_0032B100.
        z = float(static_cast<long double>(z) * factor);
    }
    if (-0.8f > read_f32(record, 0x10)) // DAT_0032B3A0.
    {
        return false;
    }
    std::array<float, 3> normal{read_f32(record, 0xC), 0.f, read_f32(record, 0x14)};
    if (normal[0] == 0.f && normal[2] == 0.f)
    {
        return false;
    }
    normalize_vector_in_place(normal.data());
    return 0.f - (normal[2] * z + normal[1] * y + normal[0] * x) > 0.7f; // DAT_0032B264.
}

// Xbox 0x0009B780
void bounce_runtime_knockdown(KnockdownState& state, std::span<const u8> record, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const float nx = read_f32(record, 0xC);
    const float ny = read_f32(record, 0x10);
    const float nz = read_f32(record, 0x14);
    state.landing_point = read_vec(record, 0);
    body_record(body).unk_254 = record[0x1E];
    const bool dead = body_record(body).energy == 0;
    bool bounce = false;
    if (animation_found(state, kKnockdownBounce))
    {
        const bool late = state.phase_time > 0.25f; // DAT_0032B104.
        bool lucky = false;
        if (state.bounce_key != 0)
        {
            lucky = !(next_unit(context.random) > 0.5f); // DAT_0032B0F4.
        }
        bounce = late && (dead || lucky);
    }
    const auto settings = context.settings;
    if (settings.data() != nullptr && read_u32(settings, 8) != 0)
    {
        const float roll = next_unit(context.random);
        if (!(read_f32(settings, 0xC) < roll) && dead)
        {
            request_runtime_body_death_effect(1, context.death);
            context.callouts(0x3E, nullptr);
            return;
        }
    }
    if (bounce)
    {
        const float vx = actor.anchor.x * -1.f; // DAT_0032B120.
        const float vz = actor.anchor.z * -1.f;
        const float dot = (vz * nz + vx * nx + ny * -0.f) * 2.f; // DAT_0032B4D4, DAT_0032B110.
        const float scale = state.bounce_scale;
        actor.anchor = Vec3{(dot * nx - vx) * scale, scale * (ny * dot), (nz * dot - vz) * scale};
        float angle;
        if (runtime_direction_angle(actor.anchor, angle))
        {
            state.yaw = angle;
        }
        leave_phase(state, 4, context);
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, kKnockdownBounce, context);
        signal_runtime_knockdown(state.landing_point, 0.f, state.yaw, context);
    }
    else
    {
        float angle;
        if (runtime_direction_angle(Vec3{nx, ny, nz}, angle))
        {
            state.yaw = angle;
        }
        enter_runtime_knockdown_phase(state, 3, context);
    }
    context.callouts(0x3E, nullptr);
}

// Xbox 0x0009D5C0
void end_runtime_knockdown(KnockdownState& state, u16 key, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    leave_phase(state, 8, context);
    if (state.contacts_raised != 0)
    {
        clear_runtime_body_contacts(context);
        state.contacts_raised = 0;
    }
    float end;
    if (body_record(body).energy == 0)
    {
        end = 1.f; // DAT_0032B0F8.
        if (0.f <= state.end_time)
        {
            end = state.end_time;
        }
    }
    else
    {
        end = body_record(body).unk_22C / float(i32(body_record(body).unk_160));
    }
    state.end_timer = end;
    state.end_key = key;
    if (body_record(body).energy == 0)
    {
        // The message is the key's own stack slot.
        const u32 message[]{0x35};
        context.actor_message(message);
    }
}

// Xbox 0x0009D670
void finish_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    if (body_record(body).energy != 0)
    {
        if (context.table != nullptr) // Body +0x73C, held as its native view.
        {
            const u16 key = state.end_key;
            u32 output;
            if (find_runtime_script_table_row(context.table, key, output) != nullptr)
            {
                state.base.next_key = key;
            }
        }
        return;
    }
    u16 key = state.death_key;
    if (key == 0)
    {
        key = state.fallback_death_key;
        if (key == 0)
        {
            finish_runtime_body_death(1, context.scene_flags, context.restart_requested, context.death);
            return;
        }
    }
    state.base.next_key = key;
}

// Xbox 0x0009D6D0
void slow_runtime_knockdown(RuntimeKnockdownContext& context)
{
    // The planar velocity eases toward DAT_0050DD80 at DAT_0032B504 (0x00103880, as 0x00140660 does).
    ease_runtime_planar_velocity(context.actor.anchor, context.default_position, 3.5f, context.motion_steps);
}

// Xbox 0x0009D7D0
void settle_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const Vec3& velocity = context.actor.anchor;
    if (velocity.x == 0.f && velocity.z == 0.f && state.contacts_raised != 0)
    {
        clear_runtime_body_contacts(context);
        state.contacts_raised = 0;
    }
}

// Xbox 0x00136150
float runtime_floor_pitch(const Actor& actor, std::span<const u8> actor_bytes)
{
    using entity_detail::g_nav_grids;
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    const float reach = actor.model_size_x * actor.radius_xz; // +0x1C4
    const float behind = euler.y + std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    Vec3 back{float(static_cast<long double>(position.x) + retail_sin(behind) * reach), position.y,
        float(static_cast<long double>(position.z) + retail_cos(behind) * reach)};
    Vec3 front{float(static_cast<long double>(position.x) + retail_sin(euler.y) * reach), position.y,
        float(static_cast<long double>(position.z) + retail_cos(euler.y) * reach)};
    const i32 floor = i32(player_record(actor_bytes).floor);
    const i32 back_cell = entity_detail::navigation_cell_index(floor, back);
    const i32 front_cell = entity_detail::navigation_cell_index(floor, front);
    if (back_cell == -1 || front_cell == -1 || back_cell == front_cell)
    {
        return 0.f;
    }
    const auto& cells = g_nav_grids[floor]->cells;
    const float back_height = float(cells.at(size_t(back_cell)).bottom);
    const float front_height = float(cells.at(size_t(front_cell)).bottom);
    const auto step = []()
    {
        return g_nav_grids[0] == nullptr ? 0.f : float(g_nav_grids[0]->cell);
    };
    const auto clamp_height = [&](float height, float& output)
    {
        if (step() > animation_absolute(height - position.y))
        {
            const float limit = step();
            const float low = position.y - limit;
            const float high = position.y + limit;
            output = low > height ? low : (height > high ? high : height);
        }
    };
    clamp_height(back_height, back.y);
    clamp_height(front_height, front.y);
    const Vec3 slope{front.x - back.x, front.y - back.y, front.z - back.z};
    float pitch;
    runtime_direction_pitch(slope, pitch);
    return pitch;
}

// Xbox 0x0009C220
void pose_runtime_knockdown(KnockdownState& state, float timer, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const auto clamp = [](float value)
    {
        return value > 1.f ? 1.f : (0.f > value ? 0.f : value);
    };
    const auto mark_fraction = [&]()
    {
        const float position = slot_position(context.animation);
        const float mark = state.pose_mark;
        const float end = mark + 0.2f; // DAT_0032B1AC.
        if (end == mark)
        {
            return 1.f;
        }
        return clamp((position - mark) / (end - mark));
    };
    const auto apply_euler = [&](const Vec3& euler)
    {
        wrap_runtime_euler(euler, actor.euler);
        actor.xform_valid = 0;
        const Vec3 settled = actor_world_euler(actor);
        body_record(body).euler = settled;
    };
    switch (state.phase)
    {
    case 1:
    {
        const float flight = state.flight_time;
        const float fraction = flight == 0.f ? 1.f : clamp(timer / flight);
        body_record(body).euler = Vec3{interpolate_runtime_mind_angle(state.pitch, 0.f, fraction), state.yaw, 0.f};
        return;
    }
    case 2:
    {
        const float fraction = mark_fraction();
        const float pitch = interpolate_runtime_mind_angle(runtime_floor_pitch(actor, context.actor_bytes),
            state.start_pitch, fraction);
        Vec3 euler = actor_world_euler(actor);
        euler.x = pitch;
        apply_euler(euler);
        return;
    }
    case 3:
    {
        const float first = mark_fraction();
        const float pitch = interpolate_runtime_mind_angle(0.f, state.start_pitch, first);
        const float second = mark_fraction();
        const float yaw = interpolate_runtime_mind_angle(state.yaw, state.start_yaw, second);
        Vec3 euler = actor_world_euler(actor);
        euler.x = pitch;
        euler.y = yaw;
        apply_euler(euler);
        return;
    }
    default:
        if ((context.bits.bytes[5] & 2) != 0)
        {
            return;
        }
        body_record(body).euler = Vec3{runtime_floor_pitch(actor, context.actor_bytes), state.yaw, 0.f};
        return;
    }
}

// Xbox 0x0009BD40
void notify_runtime_knockdown_contacts(KnockdownState& state, RuntimeKnockdownContext& context)
{
    if (state.contact_pool == 0)
    {
        return;
    }
    bool bounced = false;
    // Indexed: a reply can reach 0x0009BEA0 and shift the rows.
    auto& rows = knockdown_contacts[body_state_key(state)];
    for (size_t index = 0; index < rows.size(); ++index)
    {
        if (rows[index].sent != 0)
        {
            continue;
        }
        const KnockdownContact row = rows[index];
        // {0x45, reply, reply, this actor, direction, contact, flag << 8 | bone << 16}.
        std::array<u32, 0x42> message{};
        message[0] = 0x45;
        message[3] = context.actor_address;
        message[4] = std::bit_cast<u32>(row.direction.x);
        message[5] = std::bit_cast<u32>(row.direction.y);
        message[6] = std::bit_cast<u32>(row.direction.z);
        message[7] = std::bit_cast<u32>(row.contact.x);
        message[8] = std::bit_cast<u32>(row.contact.y);
        message[9] = std::bit_cast<u32>(row.contact.z);
        message[10] = (u32(row.flag) << 8) | (u32(row.bone) << 16);
        context.send(row.actor, message); // Vtable +0xC.
        if (message[2] != 0 && message[1] != 0)
        {
            bounced = true;
        }
        if (index < rows.size())
        {
            rows[index].sent = 1;
        }
    }
    if (bounced)
    {
        // DAT_0032B0F4 (0.5) halves the body's horizontal velocity (actor +0x3C, +0x44).
        context.actor.anchor.z = context.actor.anchor.z * 0.5f;
        context.actor.anchor.x = context.actor.anchor.x * 0.5f;
    }
}

// Xbox 0x0009B430
void fly_runtime_knockdown(KnockdownState& state, float timer, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    sweep_runtime_knockdown(state, context);
    const Vec3 velocity = context.actor.anchor;
    const u32 kind = runtime_knockdown_contact_kind(state, context);
    const bool touching = kind == 1;
    const bool against = kind == 2;
    const bool landed = (context.bits.bytes[5] & 2) != 0 || against;
    const bool falling = ((context.bits.bytes[1] >> 4) & 1) != 0;
    if (timer == 0.f) // DAT_0032B100.
    {
        return;
    }
    const Vec3 position = actor_world_position(context.actor);
    bool inside = true;
    if (context.bounded != 0)
    {
        // DAT_0050EA7C, +0x80, +0x84, +0x88, +0x8C, +0x90.
        const std::array<float, 6> box{*context.bounds[0], *context.bounds[1], *context.bounds[2],
            *context.bounds[3], *context.bounds[4], *context.bounds[5]};
        float top = box[4];
        if (!(top >= position.y))
        {
            top = position.y;
        }
        inside = !(box[0] > position.x) && position.x < box[3] && !(box[1] > position.y)
            && position.y < top + 1.f && !(box[2] > position.z) && position.z < box[5];
    }
    if (inside && body_record(body).energy != 0 && state.recovery_mode == 1
        && !(timer < state.flight_time * 0.8f)) // DAT_0032B244.
    {
        end_runtime_knockdown(state, state.landing_key, context);
        finish_runtime_knockdown(state, context);
        return;
    }
    if ((landed && !(0.f < velocity.y)) || falling)
    {
        if ((context.bits.bytes[5] & 2) != 0)
        {
            state.landing_point = body_record(body).ground_point;
            body_record(body).unk_254
                = entity_detail::navigation_surface_kind(i32(player_record(context.actor_bytes).floor),
                state.landing_point);
        }
        else if (against)
        {
            const std::span<const u8> record = sphere_record(
                first_hit_runtime_body_sphere(body, context.spheres)); // 0x00025FE0
            state.landing_point = read_vec(record, 0);
            body_record(body).unk_254
                = entity_detail::navigation_surface_kind(i32(player_record(context.actor_bytes).floor),
                state.landing_point); // 0x001117A0
        }
        else
        {
            state.landing_point = actor_world_position(context.actor);
        }
        if ((state.recovery_mode != 1 || body_record(body).energy == 0)
            && animation_found(state, kKnockdownBounceLanding))
        {
            enter_runtime_knockdown_phase(state, 2, context);
            return;
        }
        end_runtime_knockdown(state, state.landing_key, context);
        finish_runtime_knockdown(state, context);
        return;
    }
    if (!animation_found(state, kKnockdownLanded))
    {
        return;
    }
    const std::span<const u8> last_contact = body.subspan(offsetof(BodyRecord, last_contact), 0x20);
    if ((context.bits.bytes[2] & 0x20) != 0
        && (runtime_knockdown_hits_wall(last_contact, velocity) || runtime_body_bit_set(context.bits, 0xD)))
    {
        bounce_runtime_knockdown(state, last_contact, context);
        return;
    }
    if (!touching)
    {
        return;
    }
    const std::span<const u8> record = sphere_record(first_hit_runtime_body_sphere(body, context.spheres)); // 0x00025FE0
    if (runtime_knockdown_hits_wall(record, velocity)) // 0x0009BA40, ECX record
    {
        bounce_runtime_knockdown(state, record, context); // 0x0009B780, EAX record
    }
}

// Xbox 0x0009CBE0
void tick_runtime_knockdown_state(KnockdownState& state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const float timer = state.base.elapsed;
    const float step = runtime_frame_step(context.clock);
    state.phase_time = runtime_actor_rate(actor) * step + state.phase_time;
    state.base.elapsed = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    if (state.first_callout_sent == 0 &&
        runtime_actor_rate(actor) * runtime_frame_step(context.clock) > 0.f)
    {
        const i32 id = std::bit_cast<i32>(state.first_callout);
        context.callouts(id < 1 ? 0x3B : u32(id), nullptr);
        state.first_callout_sent = 1;
    }
    switch (state.phase)
    {
    case 1:
        fly_runtime_knockdown(state, timer, context);
        break;
    case 2:
        sweep_runtime_knockdown(state, context);
        slow_runtime_knockdown(context);
        settle_runtime_knockdown(state, context);
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            end_runtime_knockdown(state, state.landing_key, context);
        }
        break;
    case 3:
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            if (state.recovery_mode == 1 && body_record(body).energy != 0)
            {
                end_runtime_knockdown(state, state.landing_key, context);
                finish_runtime_knockdown(state, context);
            }
            else
            {
                leave_phase(state, 6, context);
                state.swept = 0;
                body_record(body).gravity = state.saved_gravity;
            }
        }
        break;
    case 4:
        if ((context.bits.bytes[5] & 2) != 0)
        {
            leave_phase(state, 5, context);
            release_getting_up(state, context);
            context.bits.bytes[4] |= 0x20;
            body_record(body).collision_kind = 0;
            start_record(state, kKnockdownBounceRecovery, context);
            signal_runtime_knockdown(state.landing_point, std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        }
        break;
    case 5:
        slow_runtime_knockdown(context);
        settle_runtime_knockdown(state, context);
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            end_runtime_knockdown(state, state.bounce_key, context);
        }
        break;
    case 6:
    {
        sweep_runtime_knockdown(state, context);
        bool rise = false;
        // 0x0001F060: body +0x19E holds a floor height at +0x17C.
        if (body_record(body).has_ground != 0)
        {
            const float above = actor_world_position(actor).y - body_record(body).ground_point.y;
            rise = state.rise_height > above;
        }
        if (!rise && runtime_knockdown_contact_kind(state, context) != 2 && (context.bits.bytes[1] & 0x10) == 0
            && !runtime_body_bit_set(context.bits, 0x29))
        {
            break;
        }
        if (!animation_found(state, kKnockdownGettingUp))
        {
            end_runtime_knockdown(state, state.getting_up_key, context);
            finish_runtime_knockdown(state, context);
            break;
        }
        leave_phase(state, 7, context);
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body_record(body).collision_kind = 1;
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, kKnockdownGettingUp, context);
        notify_down(context);
        state.landing_point = body_record(body).ground_point;
        if (body_record(body).energy == 0 && has_action(context, 0x69))
        {
            state.death_key = 0x69;
        }
        break;
    }
    case 7:
        slow_runtime_knockdown(context);
        settle_runtime_knockdown(state, context);
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            end_runtime_knockdown(state, state.getting_up_key, context);
        }
        break;
    case 8:
    {
        slow_runtime_knockdown(context);
        // 0x00042360 over the end timer.
        const float remaining = state.end_timer;
        if (0.f < remaining)
        {
            const float next = remaining - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            state.end_timer = next;
            if (!(0.f < next))
            {
                state.end_timer = 0.f;
            }
        }
        if (state.end_timer == 0.f)
        {
            finish_runtime_knockdown(state, context);
        }
        break;
    }
    default:
        break;
    }
    pose_runtime_knockdown(state, timer, context);
    notify_runtime_knockdown_contacts(state, context);
}
}
