#include "body_knockdown_state.h"
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

#include <bit>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gbtg
{
namespace
{
// The seven records the constructor finds (state +0x38..+0x50) live on the host; the state words say whether each
// was found.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 7>> knockdown_records;

// The +0xCC pool (stride 0x20, grown by 4) of actors that pushed against the flying body: +0 the actor, +4 the
// direction away from it, +0x10 the contact point, +0x1C sent, +0x1D the flag, +0x1E the bone. The rows live on
// the host; state +0xCC holds a nonzero marker while the pool exists.
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

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

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

void put_vec(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

const AnimationRecord* record_of(std::span<const u8> state, size_t slot)
{
    if (read_u32(state, 0x38 + slot * 4) == 0)
    {
        return nullptr;
    }
    return knockdown_records.at(state.data())[slot];
}

// 0x0001AFA0 with ECX the record at state +0x38 + slot * 4 (ESI the body).
void start_record(std::span<const u8> state, size_t slot, RuntimeKnockdownContext& context)
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
void leave_phase(std::span<u8> state, u32 phase, RuntimeKnockdownContext& context)
{
    if (read_u32(state, 0x18) == 1)
    {
        clear_runtime_player_attachment_events(state.subspan(0x70, 36), context.lists);
    }
    put_word(state, 0x18, phase);
    put_float(state, 0x1C, 0.f);
}

// The inline release of the getting-up count: body +0x69 drops once while the body lives.
void release_getting_up(std::span<u8> state, RuntimeKnockdownContext& context)
{
    if (read_u32(state, 0xE0) == 0 && read_u16(context.body, 0x94) != 0)
    {
        context.body[0x69] = u8(context.body[0x69] - 1);
        put_word(state, 0xE0, 1);
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

// The body's +0x710 sphere records (stride 0x58) are not viewed natively; every reader requires them absent.
void require_no_contact_records(std::span<const u8> body)
{
    if (read_u32(body, 0x710) != 0)
    {
        throw Error("body +0x710 collision sphere records have no native view");
    }
}
}

// Xbox 0x0009C550
void knockdown_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0xF4;
    output.parameters = 0x50;
}

// Xbox 0x00025FA0
void clear_runtime_body_contacts(RuntimeKnockdownContext& context)
{
    require_no_contact_records(context.body);
    context.body[0x715] = 0;
    context.body[0x716] = 0;
}

// Xbox 0x0009C150
void aim_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    const Vec3 push{0.f - read_f32(body, 0x230), 0.f - read_f32(body, 0x234), 0.f - read_f32(body, 0x238)};
    const u32 source = read_u32(body, 0x26C);
    float angle;
    if (runtime_direction_angle(push, angle))
    {
        put_float(state, 0xA8, angle);
        const float unit = next_unit(context.random);
        put_float(state, 0xA8, (unit * std::bit_cast<float>(0x3EB2B8C3u) + read_f32(state, 0xA8))
            - std::bit_cast<float>(0x3E32B8C3u)); // DAT_0032B160, DAT_0032B508.
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
        put_float(state, 0xA8, aim);
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
void launch_runtime_knockdown(std::span<u8> state, float distance, float height, float time, float pitch,
    RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    put_word(state, 0xA0, read_u32(body, 0x324));
    const float span = distance / float(i32(read_u16(body, 0x162)));
    Vec3 velocity;
    float gravity;
    float flight;
    runtime_knockdown_launch(read_f32(state, 0xA8) + std::bit_cast<float>(0x40490FDBu), height, span, 0.1f, time,
        0.1f, velocity, gravity, flight, context.random); // DAT_0032B148, DAT_0032B254.
    put_float(body, 0x324, gravity);
    put_float(state, 0xB0, flight);
    put_float(state, 0xAC, pitch * std::bit_cast<float>(0x3C8EFA35u)); // DAT_0032B0FC.
    context.actor.anchor = velocity;
    put_vec(body, 0x318, velocity);
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
void enter_runtime_knockdown_phase(std::span<u8> state, u32 phase, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    if (read_u32(state, 0x18) == 1)
    {
        clear_runtime_player_attachment_events(state.subspan(0x70, 36), context.lists);
    }
    put_word(state, 0x18, phase);
    put_float(state, 0x1C, 0.f);
    u16 key;
    switch (phase)
    {
    case 1:
    {
        put_word(state, 0x60, 0);
        start_record(state, 0, context);
        const float flight = read_f32(state, 0xB0);
        AnimationController* controller = context.animation.controller.get();
        if (controller != nullptr)
        {
            MinaPlayer* player = controller->slots.at(0).player.get();
            if (player != nullptr)
            {
                if (player->old_pose == nullptr)
                {
                    // 0x0002D3F0: a copy of the current pose (EBX player +0x10).
                    if (player->pose == nullptr)
                    {
                        throw Error("knockdown pose copy 0x0002D3F0 has no current pose");
                    }
                    player->old_pose = std::make_unique<AnimationPose>(*player->pose);
                }
                player->blend_remaining = flight;
                player->blend_duration = flight;
            }
            controller->dirty = true;
        }
        // 0x0013B7C0: callouts 0x4A record their tokens at state +0x70.
        context.events.command_attachments = state.subspan(0x70, 36);
        request_runtime_player_attachment_events(context.callout_records, context.events, 0x4A,
            context.address_of(state.data() + 0x70));
        return;
    }
    case 2:
    {
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body[0x717] = 0;
        set_runtime_body_shape_mode(2, context.shape);
        actor.anchor = Vec3{actor.anchor.x * 0.7f, actor.anchor.y, actor.anchor.z * 0.7f}; // DAT_0032B264.
        start_record(state, 1, context);
        put_float(state, 0xB8, actor.euler.x); // Actor +0x14.
        put_float(state, 0xB4, slot_position(context.animation));
        notify_down(context);
        signal_runtime_knockdown(read_vec(state, 0xD0), std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        if (read_u16(body, 0x94) != 0)
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
        put_float(body, 0x324, 0.f);
        start_record(state, read_u32(state, 0x44) != 0 ? 3 : 2, context);
        put_float(state, 0xB8, actor.euler.x);
        put_float(state, 0xBC, actor.euler.y);
        put_float(state, 0xB4, slot_position(context.animation));
        notify_down(context);
        const Vec3 landing = read_vec(state, 0xD0);
        signal_runtime_knockdown(landing, 0.f, read_f32(state, 0xA8), context);
        const std::array<u32, 5> notice{0x72, context.actor_address, std::bit_cast<u32>(landing.x),
            std::bit_cast<u32>(landing.y), std::bit_cast<u32>(landing.z)};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(notice.data()));
        return;
    }
    case 4:
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, 4, context);
        signal_runtime_knockdown(read_vec(state, 0xD0), 0.f, read_f32(state, 0xA8), context);
        return;
    case 5:
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body[0x717] = 0;
        start_record(state, 5, context);
        signal_runtime_knockdown(read_vec(state, 0xD0), std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        return;
    case 6:
        put_word(state, 0x60, 0);
        put_word(body, 0x324, read_u32(state, 0xA0));
        return;
    case 7:
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body[0x717] = 1;
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, 6, context);
        notify_down(context);
        put_vec(state, 0xD0, read_vec(body, 0x178));
        if (read_u16(body, 0x94) != 0)
        {
            return;
        }
        key = 0x69;
        break;
    case 8:
        if (read_u32(state, 0x2C) != 0)
        {
            clear_runtime_body_contacts(context);
            put_word(state, 0x2C, 0);
        }
        if (read_u16(body, 0x94) == 0)
        {
            const float end = read_f32(state, 0xE8);
            put_float(state, 0x20, end < 0.f ? 1.f : end); // DAT_0032B0F8.
            return;
        }
        put_float(state, 0x20, read_f32(body, 0x22C) / float(i32(read_u16(body, 0x160))));
        return;
    default:
        return;
    }
    if (has_action(context, key))
    {
        put_half(state, 0x5C, key);
    }
}

// Xbox 0x0009C5D0
bool initialize_runtime_knockdown_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    put_half(body, 0x30, u16(read_u16(body, 0x30) + 1));
    adjust_body_counters(body, 1);
    body[0x38] = 1;
    body[0x39] = 1;
    put_word(body, 0x164, 0);
    put_float(body, 0x16C, 0.99f); // DAT_0032B228.
    // Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains a +0x15 hold.
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->retained_15 = u8(live->retained_15 + 1);
    }
    body[0x6D] = u8(body[0x6D] + 1);
    context.bits.bytes[4] |= 0x10;
    body[0x69] = u8(body[0x69] + 1);
    if (body[0xA6] != 0)
    {
        put_float(body, 0xB4, 2.f); // DAT_0032B110.
    }
    release_runtime_player_attachment(0, 0, context.attachment);
    auto& records = knockdown_records[state.data()];
    for (size_t slot = 0; slot < 7; ++slot)
    {
        // 0x0003AAA0 with the attached actor's +0xDB0 variant, then without (0x0001AED0's pair).
        const char* name = context.string_at(read_u32(parameters, 0xC + slot * 4));
        records[slot] = find_runtime_body_animation(name, context.actions);
        put_word(state, 0x38 + slot * 4, records[slot] != nullptr ? 1 : 0);
    }
    put_half(state, 0x54, read_u16(parameters, 0x28));
    put_half(state, 0x56, read_u16(parameters, 0x2A));
    put_half(state, 0x5A, read_u16(parameters, 0x2C));
    put_half(state, 0x58, read_u16(parameters, 0x2E));
    put_word(state, 0xC8, read_u32(body, 0x270));
    state[0xA4] = set_runtime_body_shape_mode(3, context.shape);
    put_float(state, 0xC0, actor.radius_y * read_f32(parameters, 0x3C));
    put_word(state, 0xC4, read_u32(parameters, 0x40));
    put_word(state, 0xE8, read_u32(parameters, 0x38));
    put_word(state, 0x34, read_u32(parameters, 0x30));
    put_word(state, 0xEC, read_u32(parameters, 0x48));
    put_word(state, 0xF0, read_u32(parameters, 0x4C));
    body[0x715] = 1;
    put_word(state, 0x2C, 1);
    put_word(state, 0x30, body[0x717]);
    body[0x717] = 0;
    put_vec(state, 0x94, actor_world_position(actor));
    aim_runtime_knockdown(state, context);
    launch_runtime_knockdown(state, read_f32(body, 0x220), read_f32(body, 0x224), read_f32(body, 0x228),
        read_f32(parameters, 0x34), context);
    enter_runtime_knockdown_phase(state, 1, context);
    const u32 bit = read_u32(state, 0xEC);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    if (read_u16(body, 0x94) == 0 && read_u16(body, 0x96) != 0)
    {
        context.bits.bytes[3] |= 8;
        notify_runtime_body_death_once(body, context.actor_address, context.game);
    }
    switch (read_u32(parameters, 0x44))
    {
    case 0:
        put_word(state, 0xE4, 0);
        break;
    case 1:
        put_word(state, 0xE4, 1);
        break;
    case 2:
        put_word(state, 0xE4, read_u16(body, 0x94) == 0 ? 1 : 0);
        break;
    default:
        break;
    }
    if (read_u32(state, 0xE4) != 0)
    {
        context.hold_latches(); // 0x00027BE0
    }
    return true;
}

// Xbox 0x0009CA80
void destroy_runtime_knockdown_state(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    if (read_u32(state, 0xCC) != 0)
    {
        knockdown_contacts.erase(state.data()); // 0x000663E0
        put_word(state, 0xCC, 0);
    }
    if (read_u32(state, 0x2C) != 0)
    {
        clear_runtime_body_contacts(context);
        put_word(state, 0x2C, 0);
    }
    body[0x717] = state[0x30];
    put_half(body, 0x30, u16(read_u16(body, 0x30) - 1));
    put_word(body, 0x324, read_u32(state, 0xA0));
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->retained_15 = u8(live->retained_15 - 1);
    }
    body[0x6D] = u8(body[0x6D] - 1);
    adjust_body_counters(body, -1);
    set_runtime_body_shape_mode(state[0xA4], context.shape);
    context.bits.bytes[4] &= 0xEF;
    context.bits.bytes[4] &= 0xDF;
    const i32 count = std::bit_cast<i32>(read_u32(state, 0x70));
    for (i32 index = 0; index < count; ++index)
    {
        remove_runtime_body_attachment(u16(read_u32(state, 0x74 + size_t(index) * 4)), context.lists);
    }
    put_word(state, 0x70, 0);
    if (read_u32(state, 0xE0) == 0)
    {
        body[0x69] = u8(body[0x69] - 1);
        put_word(state, 0xE0, 1);
    }
    const u32 bit = read_u32(state, 0xEC);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    if (read_u32(state, 0xE4) != 0)
    {
        body[0x7E5] = u8(body[0x7E5] - 1);
    }
    knockdown_records.erase(state.data());
}

// Xbox 0x0009B720
void collide_runtime_knockdown(std::span<const u8> state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context)
{
    if (read_u32(state, 0x18) == 1 && read_u32(state, 0x40) != 0)
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
void message_runtime_knockdown_state(std::span<u8> state, RuntimePlayerCollisionReply& reply,
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
void message_runtime_knockdown_state(std::span<u8> state, std::span<const u8> message,
    RuntimeKnockdownContext& context)
{
    switch (read_u32(message, 0))
    {
    case 0xE:
        if (read_u32(state, 0x18) == 1)
        {
            // {0xE, actor, push, contact, bone} from the shape push (actor vtable +0x34).
            const Vec3 contact{read_f32(message, 0x14), read_f32(message, 0x18), read_f32(message, 0x1C)};
            record_runtime_knockdown_contact(state, read_u32(message, 4), contact,
                read_u32(state, 0x40) == 0 ? 1 : 0, read_u16(message, 0x20), context);
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
void record_runtime_knockdown_contact(std::span<u8> state, u32 other, const Vec3& contact, u8 flag, u16 bone,
    RuntimeKnockdownContext& context)
{
    if (read_u32(state, 0xC8) == other)
    {
        return;
    }
    if (read_u32(state, 0xCC) == 0)
    {
        knockdown_contacts[state.data()].reserve(4); // 0x000F72D0(0x90): 4 rows of 0x20.
        put_word(state, 0xCC, kContactPoolMarker);
    }
    auto& rows = knockdown_contacts[state.data()];
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
void forget_runtime_knockdown_contact(std::span<u8> state, u32 other)
{
    if (read_u32(state, 0xCC) == 0)
    {
        return;
    }
    auto& rows = knockdown_contacts[state.data()];
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
void sweep_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const u32 holder = read_u32(body, 0x274);
    u32 player = 0;
    if (holder != 0)
    {
        const auto source = context.record_at(holder, 0xCF8);
        player = read_u16(source, 0xCDC) == 2 ? 1 : (read_u32(source, 0x93C) != 0 ? 1 : 0);
    }
    Vec3 position = actor_world_position(actor);
    const float radius = float(static_cast<long double>(actor_model_sweep_radius(actor))
        * std::bit_cast<float>(0x3F2AAAABu)); // DAT_0032B204.
    const i32 found = entity_collect_contact_probes(position, radius, 0x140);
    i32 hits = found == 0 ? 0 : context.probe_messages(found, player, context.actor_address);
    if (read_u32(state, 0x60) != 0)
    {
        Vec3 previous = read_vec(state, 0x64);
        const i32 swept = entity_record_point_probe(position, previous, radius, 0x140);
        hits += swept == 0 ? 0 : context.probe_messages(swept, player, context.actor_address);
    }
    put_vec(state, 0x64, position);
    put_word(state, 0x60, 1);
    if (hits == 0)
    {
        return;
    }
    float damage = context.hit_damage[1];
    const u16 tag = read_u16(context.actor_bytes, 0xCF4);
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
        const i32 left = i32(read_u16(body, 0x94)) - amount;
        const u16 health = left < 1 ? 0 : u16(left);
        put_half(body, 0x94, health);
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
u32 runtime_knockdown_contact_kind(std::span<const u8>, RuntimeKnockdownContext& context)
{
    if (context.body[0x716] == 0)
    {
        return 0;
    }
    require_no_contact_records(context.body);
    // No records: the scan ends on a null record, whose +0x10 is read from address zero.
    throw Error("knockdown contact kind 0x0009C100 reads a null +0x710 record");
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
void bounce_runtime_knockdown(std::span<u8> state, std::span<const u8> record, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const float nx = read_f32(record, 0xC);
    const float ny = read_f32(record, 0x10);
    const float nz = read_f32(record, 0x14);
    put_vec(state, 0xD0, read_vec(record, 0));
    body[0x254] = record[0x1E];
    const bool dead = read_u16(body, 0x94) == 0;
    bool bounce = false;
    if (read_u32(state, 0x48) != 0)
    {
        const bool late = read_f32(state, 0x1C) > 0.25f; // DAT_0032B104.
        bool lucky = false;
        if (read_u16(state, 0x5A) != 0)
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
        const float scale = read_f32(state, 0xC4);
        actor.anchor = Vec3{(dot * nx - vx) * scale, scale * (ny * dot), (nz * dot - vz) * scale};
        float angle;
        if (runtime_direction_angle(actor.anchor, angle))
        {
            put_float(state, 0xA8, angle);
        }
        leave_phase(state, 4, context);
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, 4, context);
        signal_runtime_knockdown(read_vec(state, 0xD0), 0.f, read_f32(state, 0xA8), context);
    }
    else
    {
        float angle;
        if (runtime_direction_angle(Vec3{nx, ny, nz}, angle))
        {
            put_float(state, 0xA8, angle);
        }
        enter_runtime_knockdown_phase(state, 3, context);
    }
    context.callouts(0x3E, nullptr);
}

// Xbox 0x0009D5C0
void end_runtime_knockdown(std::span<u8> state, u16 key, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    leave_phase(state, 8, context);
    if (read_u32(state, 0x2C) != 0)
    {
        clear_runtime_body_contacts(context);
        put_word(state, 0x2C, 0);
    }
    float end;
    if (read_u16(body, 0x94) == 0)
    {
        end = 1.f; // DAT_0032B0F8.
        if (0.f <= read_f32(state, 0xE8))
        {
            end = read_f32(state, 0xE8);
        }
    }
    else
    {
        end = read_f32(body, 0x22C) / float(i32(read_u16(body, 0x160)));
    }
    put_float(state, 0x20, end);
    put_half(state, 0x5E, key);
    if (read_u16(body, 0x94) == 0)
    {
        // The message is the key's own stack slot.
        const u32 message[]{0x35};
        context.actor_message(message);
    }
}

// Xbox 0x0009D670
void finish_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    if (read_u16(body, 0x94) != 0)
    {
        if (context.table != nullptr) // Body +0x73C, held as its native view.
        {
            const u16 key = read_u16(state, 0x5E);
            u32 output;
            if (find_runtime_script_table_row(context.table, key, output) != nullptr)
            {
                put_half(state, 0xC, key);
            }
        }
        return;
    }
    u16 key = read_u16(state, 0x5C);
    if (key == 0)
    {
        key = read_u16(state, 0x58);
        if (key == 0)
        {
            finish_runtime_body_death(1, context.scene_flags, context.restart_requested, context.death);
            return;
        }
    }
    put_half(state, 0xC, key);
}

// Xbox 0x0009D6D0
void slow_runtime_knockdown(RuntimeKnockdownContext& context)
{
    // The planar velocity eases toward DAT_0050DD80 at DAT_0032B504 (0x00103880, as 0x00140660 does).
    ease_runtime_planar_velocity(context.actor.anchor, context.default_position, 3.5f, context.motion_steps);
}

// Xbox 0x0009D7D0
void settle_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const Vec3& velocity = context.actor.anchor;
    if (velocity.x == 0.f && velocity.z == 0.f && read_u32(state, 0x2C) != 0)
    {
        clear_runtime_body_contacts(context);
        put_word(state, 0x2C, 0);
    }
}

// Xbox 0x00136150
float runtime_floor_pitch(const Actor& actor, std::span<const u8> actor_bytes)
{
    using entity_detail::g_nav_grids;
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    const float reach = read_f32(actor_bytes, 0x1C4) * actor.radius_xz;
    const float behind = euler.y + std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    Vec3 back{float(static_cast<long double>(position.x) + retail_sin(behind) * reach), position.y,
        float(static_cast<long double>(position.z) + retail_cos(behind) * reach)};
    Vec3 front{float(static_cast<long double>(position.x) + retail_sin(euler.y) * reach), position.y,
        float(static_cast<long double>(position.z) + retail_cos(euler.y) * reach)};
    const i32 floor = i32(read_u32(actor_bytes, 0xCEC));
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
void pose_runtime_knockdown(std::span<u8> state, float timer, RuntimeKnockdownContext& context)
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
        const float mark = read_f32(state, 0xB4);
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
        put_vec(body, 0x72C, settled);
    };
    switch (read_u32(state, 0x18))
    {
    case 1:
    {
        const float flight = read_f32(state, 0xB0);
        const float fraction = flight == 0.f ? 1.f : clamp(timer / flight);
        put_float(body, 0x72C, interpolate_runtime_mind_angle(read_f32(state, 0xAC), 0.f, fraction));
        put_float(body, 0x730, read_f32(state, 0xA8));
        put_float(body, 0x734, 0.f);
        return;
    }
    case 2:
    {
        const float fraction = mark_fraction();
        const float pitch = interpolate_runtime_mind_angle(runtime_floor_pitch(actor, context.actor_bytes),
            read_f32(state, 0xB8), fraction);
        Vec3 euler = actor_world_euler(actor);
        euler.x = pitch;
        apply_euler(euler);
        return;
    }
    case 3:
    {
        const float first = mark_fraction();
        const float pitch = interpolate_runtime_mind_angle(0.f, read_f32(state, 0xB8), first);
        const float second = mark_fraction();
        const float yaw = interpolate_runtime_mind_angle(read_f32(state, 0xA8), read_f32(state, 0xBC), second);
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
        put_float(body, 0x72C, runtime_floor_pitch(actor, context.actor_bytes));
        put_float(body, 0x730, read_f32(state, 0xA8));
        put_float(body, 0x734, 0.f);
        return;
    }
}

// Xbox 0x0009BD40
void notify_runtime_knockdown_contacts(std::span<u8> state, RuntimeKnockdownContext& context)
{
    if (read_u32(state, 0xCC) == 0)
    {
        return;
    }
    bool bounced = false;
    // Indexed: a reply can reach 0x0009BEA0 and shift the rows.
    auto& rows = knockdown_contacts[state.data()];
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
void fly_runtime_knockdown(std::span<u8> state, float timer, RuntimeKnockdownContext& context)
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
    if (inside && read_u16(body, 0x94) != 0 && read_u32(state, 0x34) == 1
        && !(timer < read_f32(state, 0xB0) * 0.8f)) // DAT_0032B244.
    {
        end_runtime_knockdown(state, read_u16(state, 0x56), context);
        finish_runtime_knockdown(state, context);
        return;
    }
    if ((landed && !(0.f < velocity.y)) || falling)
    {
        if ((context.bits.bytes[5] & 2) != 0)
        {
            put_vec(state, 0xD0, read_vec(body, 0x178));
            body[0x254] = entity_detail::navigation_surface_kind(i32(read_u32(context.actor_bytes, 0xCEC)),
                read_vec(state, 0xD0));
        }
        else if (against)
        {
            require_no_contact_records(body);
        }
        else
        {
            put_vec(state, 0xD0, actor_world_position(context.actor));
        }
        if ((read_u32(state, 0x34) != 1 || read_u16(body, 0x94) == 0) && read_u32(state, 0x3C) != 0)
        {
            enter_runtime_knockdown_phase(state, 2, context);
            return;
        }
        end_runtime_knockdown(state, read_u16(state, 0x56), context);
        finish_runtime_knockdown(state, context);
        return;
    }
    if (read_u32(state, 0x40) == 0)
    {
        return;
    }
    const std::span<const u8> last_contact = body.subspan(0x1CC, 0x20);
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
    require_no_contact_records(body);
}

// Xbox 0x0009CBE0
void tick_runtime_knockdown_state(std::span<u8> state, RuntimeKnockdownContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const float timer = read_f32(state, 0x10);
    const float step = runtime_frame_step(context.clock);
    put_float(state, 0x1C, runtime_actor_rate(actor) * step + read_f32(state, 0x1C));
    put_float(state, 0x10, runtime_actor_rate(actor) * runtime_frame_step(context.clock) + read_f32(state, 0x10));
    if (read_u32(state, 0x28) == 0 && runtime_actor_rate(actor) * runtime_frame_step(context.clock) > 0.f)
    {
        const i32 id = std::bit_cast<i32>(read_u32(state, 0xF0));
        context.callouts(id < 1 ? 0x3B : u32(id), nullptr);
        put_word(state, 0x28, 1);
    }
    switch (read_u32(state, 0x18))
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
            end_runtime_knockdown(state, read_u16(state, 0x56), context);
        }
        break;
    case 3:
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            if (read_u32(state, 0x34) == 1 && read_u16(body, 0x94) != 0)
            {
                end_runtime_knockdown(state, read_u16(state, 0x56), context);
                finish_runtime_knockdown(state, context);
            }
            else
            {
                leave_phase(state, 6, context);
                put_word(state, 0x60, 0);
                put_word(body, 0x324, read_u32(state, 0xA0));
            }
        }
        break;
    case 4:
        if ((context.bits.bytes[5] & 2) != 0)
        {
            leave_phase(state, 5, context);
            release_getting_up(state, context);
            context.bits.bytes[4] |= 0x20;
            body[0x717] = 0;
            start_record(state, 5, context);
            signal_runtime_knockdown(read_vec(state, 0xD0), std::bit_cast<float>(0x3FC90FDBu), 0.f, context);
        }
        break;
    case 5:
        slow_runtime_knockdown(context);
        settle_runtime_knockdown(state, context);
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            end_runtime_knockdown(state, read_u16(state, 0x5A), context);
        }
        break;
    case 6:
    {
        sweep_runtime_knockdown(state, context);
        bool rise = false;
        // 0x0001F060: body +0x19E holds a floor height at +0x17C.
        if (body[0x19E] != 0)
        {
            const float above = actor_world_position(actor).y - read_f32(body, 0x17C);
            rise = read_f32(state, 0xC0) > above;
        }
        if (!rise && runtime_knockdown_contact_kind(state, context) != 2 && (context.bits.bytes[1] & 0x10) == 0
            && !runtime_body_bit_set(context.bits, 0x29))
        {
            break;
        }
        if (read_u32(state, 0x50) == 0)
        {
            end_runtime_knockdown(state, read_u16(state, 0x54), context);
            finish_runtime_knockdown(state, context);
            break;
        }
        leave_phase(state, 7, context);
        release_getting_up(state, context);
        context.bits.bytes[4] |= 0x20;
        body[0x717] = 1;
        set_runtime_body_shape_mode(2, context.shape);
        start_record(state, 6, context);
        notify_down(context);
        put_vec(state, 0xD0, read_vec(body, 0x178));
        if (read_u16(body, 0x94) == 0 && has_action(context, 0x69))
        {
            put_half(state, 0x5C, 0x69);
        }
        break;
    }
    case 7:
        slow_runtime_knockdown(context);
        settle_runtime_knockdown(state, context);
        if (runtime_actor_animation_slot_complete(context.animation, 0))
        {
            end_runtime_knockdown(state, read_u16(state, 0x54), context);
        }
        break;
    case 8:
    {
        slow_runtime_knockdown(context);
        // 0x00042360 over state +0x20.
        const float remaining = read_f32(state, 0x20);
        if (0.f < remaining)
        {
            const float next = remaining - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            put_float(state, 0x20, next);
            if (!(0.f < next))
            {
                put_float(state, 0x20, 0.f);
            }
        }
        if (read_f32(state, 0x20) == 0.f)
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
