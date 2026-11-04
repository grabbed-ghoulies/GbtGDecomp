#include "body_throw_state.h"
#include "player_mind_targets.h"
#include "actor_transform.h"
#include "actor_live_attachments.h"
#include "body_action_selection.h"
#include "body_animation_commands.h"
#include "body_hit_state.h"
#include "body_message_helpers.h"
#include "body_orientation.h"
#include "bounded_channels.h"
#include "player_mind_events.h"
#include "../camera/camera_mind_rig.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
Vec3 read_vector(std::span<const u8> bytes, size_t offset)
{
    return Vec3{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains or drops a +0x15 hold.
void adjust_live_attachment_holds(entity_detail::ListHead& live_attachments, i32 step)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

// The aim's lift above the actor: half the actor's +0x34 height scaled by body +0x884 (DAT_0032B0F4).
float throw_lift(const RuntimeThrowStateContext& context)
{
    return context.actor.radius_y * get(context.body, body_field::height_scale) * 0.5f;
}
}

// Xbox 0x000A37D0
void throw_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x68;
    output.parameters = 0x2C;
}

// Xbox 0x000A3850
bool initialize_runtime_throw_state(ThrowState& state, std::span<const u8> parameters,
    RuntimeThrowStateContext& context)
{
    const auto body = context.body;
    state.aim_angle = get(parameters, throw_state_parameter::aim_angle_degrees) * std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC.
    state.aim_mode = get(parameters, throw_state_parameter::aim_mode);
    state.spread = get(parameters, throw_state_parameter::spread);
    state.end_key = get(parameters, throw_state_parameter::end_key);
    state.turn_to_aim = get(parameters, throw_state_parameter::turn_to_aim);
    state.smooth_launch = get(parameters, throw_state_parameter::smooth_launch);
    state.settle_turn = get(parameters, throw_state_parameter::settle_turn);
    state.near_range = get(parameters, throw_state_parameter::near_range);
    state.ended = 0;
    body[0x38] = 1;
    body[0x39] = 1;
    body[0x6A] = u8(body[0x6A] + 1);
    adjust_live_attachment_holds(context.live_attachments, 1);
    body[0x6D] = u8(body[0x6D] + 1);
    adjust_body_counters(body, 1);
    context.bits.bytes[6] |= 8;
    // 0x00041820, ESI actor, EDI state +0x18.
    state.entry_euler = actor_world_euler(context.actor);
    return true;
}

// Xbox 0x000A3910
void destroy_runtime_throw_state(std::span<u8>, RuntimeThrowStateContext& context)
{
    const auto body = context.body;
    adjust_body_counters(body, -1);
    body[0x6A] = u8(body[0x6A] - 1);
    adjust_live_attachment_holds(context.live_attachments, -1);
    body[0x6D] = u8(body[0x6D] - 1);
    context.bits.bytes[6] &= 0xF7;
    // The tail jump into 0x0001B720: a weapon still at +0x4C (with nothing in the hand, or one whose +0x93D is 2)
    // is let go (0x0001B570) and told {0x97, 0}.
    auto& attachment = context.attachment;
    RuntimeBodyAttachedActor* held = attachment.secondary;
    RuntimeBodyAttachedActor* hand = attachment.ownership.attached;
    if (held != nullptr && (hand == nullptr || hand->bytes[0x93D] == 2))
    {
        detach_runtime_body_secondary(body, attachment.secondary, attachment.actions.second_receiver); // 0x0001B570
        const std::array<u32, 2> drop{0x97, 0};
        held->message(drop);
    }
}

// Xbox 0x000A3D50
void turn_runtime_throw_state(ThrowState& state, RuntimeThrowStateContext& context)
{
    if (state.launch_seeded == 0 || state.smooth_launch == 0)
    {
        state.launch = state.aim;
        state.launch_seeded = 1;
    }
    else
    {
        // x87: the actor's rate times the frame step times 100 (DAT_0032B12C), stored as a float.
        const float amount = float(static_cast<long double>(runtime_actor_rate(context.actor))
            * runtime_frame_step(context.clock) * 100.f);
        const Vec3 aim = state.aim;
        Vec3 launch = state.launch;
        launch.x = approach_runtime_scalar(launch.x, aim.x, amount);
        launch.y = approach_runtime_scalar(launch.y, aim.y, amount);
        launch.z = approach_runtime_scalar(launch.z, aim.z, amount);
        state.launch = launch;
    }
    Vec3 euler{};
    if (turn_runtime_body_toward(state.launch, context.body,
            state.entry_euler, euler,
            context.hit, context.actor, context.animation)) // 0x0001D5B0, EAX state +0x30
    {
        normalize_euler_angles(context.actor.euler, euler); // 0x00105AA0
        context.actor.xform_valid = 0; // +0x2A8.
        set(context.body, body_field::euler, actor_world_euler(context.actor)); // 0x00041820, actor +0x95C -> +0x72C
    }
}

// Xbox 0x000A3E80
void aim_runtime_throw_by_stick(Vec3& aim, RuntimeThrowStateContext& context)
{
    const auto body = context.body;
    std::array<float, 2> axes{0.f, 0.f};
    if (context.own_input != nullptr && get(body, body_field::move_holds) == 0 &&
        get(body, body_field::move_hold_time) == 0.f)
    {
        runtime_input_secondary_axes(*context.own_input, axes, context.input_state); // 0x001088D0
    }
    const long double root = retail_square_root(axes[0] * axes[0] + axes[1] * axes[1]);
    float magnitude = float(root);
    if (!(1.0L > root)) // DAT_0032B0F8.
    {
        magnitude = 1.f;
    }
    float angle;
    if (axes[0] == 0.f && axes[1] == 0.f)
    {
        angle = 0.f;
    }
    else
    {
        angle = float(retail_atan2(axes[1], axes[0]));
    }
    if (magnitude == 0.f)
    {
        angle = context.actor.euler.y; // Actor +0x18.
    }
    aim = actor_world_position(context.actor); // 0x000412A0
    const float side = float(retail_sin(angle));
    aim.x = float(static_cast<long double>(side) * 20.f + aim.x); // DAT_0032B234.
    aim.z = float(retail_cos(angle) * 20.f + aim.z);
}

// Xbox 0x000A3FE0
void spread_runtime_throw(float spread, Vec3& point, const Actor& actor, u32& random)
{
    if (spread == 0.f) // DAT_0032B100.
    {
        return;
    }
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI actor
    const Vec3 direction{point.x - position.x, point.y - position.y, point.z - position.z};
    float angle = 0.f;
    runtime_direction_angle(direction, angle); // 0x00105F90, EBX angle
    const float across = float(static_cast<long double>(angle) + std::bit_cast<float>(0x3FC90FDBu)); // DAT_0032B140.
    random = random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
    const float roll = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const float lower = 0.f - spread;
    const float offset = (spread - lower) * roll + lower;
    const float side = float(retail_sin(across)); // 0x00153320
    const float forward = float(retail_cos(across)); // 0x00153230
    point.x = side * offset + point.x;
    point.z = forward * offset + point.z;
}

// Xbox 0x000413F0
long double runtime_actor_flat_distance(const Actor& actor, const Vec3& point)
{
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float dz = position.z - point.z;
    const float dx = position.x - point.x;
    return retail_square_root(dz * dz + dx * dx); // 0x00152B30
}

// Xbox 0x000A4100
void lead_runtime_throw_target(Vec3& aim, RuntimeThrowStateContext& context)
{
    const auto target = context.target;
    const Vec3 seen = read_vector(target, 0x20);
    if (!(get(context.actor_bytes, player_field::mode) == 2 || get(context.actor_bytes, player_field::case_link) != 0))
    {
        aim = seen;
        return;
    }
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0, ESI actor
    const float lead = read_f32(context.body, 0x7C);
    aim = seen;
    const Actor& moving = context.actor_at(get(target, mind_target_field::actor));
    if (!(moving.animation_pause > 0.f)) // Target actor +0x930; DAT_0032B100.
    {
        aim.x = get(target, mind_target_field::direction).x * lead + aim.x;
        aim.y = get(target, mind_target_field::direction).y * lead + aim.y;
        aim.z = get(target, mind_target_field::direction).z * lead + aim.z;
    }
    aim.y = seen.y;
    // The led direction and the direct one, each normalized when not zero (x87 from the reciprocal root).
    float ax = aim.x - position.x;
    float ay = seen.y - position.y;
    float az = aim.z - position.z;
    float bx = seen.x - position.x;
    float by = ay;
    float bz = seen.z - position.z;
    const float led = (az * az + ay * ay) + ax * ax;
    if (led != 0.f) // DAT_0032B100.
    {
        const long double scale = engine_reciprocal_sqrt(led); // 0x001534C0
        ax = float(ax * scale);
        ay = float(ay * scale);
        az = float(az * scale);
    }
    const float direct = (bz * bz + by * by) + bx * bx;
    if (direct != 0.f)
    {
        const long double scale = engine_reciprocal_sqrt(direct);
        bx = float(bx * scale);
        by = float(by * scale);
        bz = float(bz * scale);
    }
    const float dot = (bz * az + by * ay) + bx * ax;
    if (0.f > dot) // DAT_004286F8.
    {
        aim = seen;
    }
}

// Xbox 0x000A3990
void tick_runtime_throw_state(ThrowState& state, RuntimeThrowStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const u32 held = read_u32(body, 0x4C);
    const bool targeted = context.target.data() != nullptr;
    state.base.elapsed = runtime_actor_step(actor, context.clock) + state.base.elapsed;
    const u32 mode = state.aim_mode;
    if (mode == 3)
    {
        if (held != 0)
        {
            Vec3 aim{};
            aim_runtime_throw_by_stick(aim, context); // 0x000A3E80, EBX state +0x24
            state.aim = aim;
        }
    }
    else if (targeted)
    {
        Vec3 aim{};
        lead_runtime_throw_target(aim, context); // 0x000A4100, EBX the target record
        const float height = get(context.target, mind_target_field::shape_height);
        if (mode == 0)
        {
            aim.y = height * 0.8f + aim.y; // DAT_0032B244.
        }
        else if (mode == 1)
        {
            aim.y = height * 0.5f + aim.y; // DAT_0032B0F4.
        }
        else if (mode == 2)
        {
            aim.y = height * 0.1f + aim.y; // DAT_0032B254.
        }
        state.aim = aim;
    }
    else
    {
        // 50 ahead of the actor along its yaw turned by +0x54, at half its height (x87 for the sums).
        Vec3 aim = actor_world_position(actor); // 0x000412A0, EDI state +0x24
        aim.y = throw_lift(context) + aim.y;
        const float angle = float(static_cast<long double>(state.aim_angle) + actor.euler.y);
        const float side = float(retail_sin(angle)); // 0x00153320, stored
        const long double forward = retail_cos(angle); // 0x00153230, kept in x87
        aim.x = float(static_cast<long double>(side) * 50.f + aim.x); // DAT_0032B1E0.
        aim.z = float(forward * 50.f + aim.z);
        state.aim = aim;
        state.launch = aim;
    }
    if (state.thrown == 0 && state.turn_to_aim != 0)
    {
        turn_runtime_throw_state(state, context); // 0x000A3D50
    }
    if (state.settle_turn != 0)
    {
        settle_runtime_body_turn(body, actor); // 0x00134720, ESI body
    }
    if (held != 0 && (context.bits.bytes[0] & 4) != 0)
    {
        // The release key: launch from +0x30, or from in front of the actor when the target is within +0x40.
        Vec3 launch = state.launch;
        const float near = state.near_range;
        if (targeted && near != 0.f)
        {
            const long double distance = runtime_actor_flat_distance(actor, launch); // 0x000413F0, EBX state +0x30
            if (static_cast<long double>(near) >= distance)
            {
                launch = actor_world_position(actor); // 0x000412A0
                offset_camera_point(launch, state.aim_angle + actor.euler.y,
                    40.f); // 0x001062A0
                launch.y = throw_lift(context) + launch.y;
            }
        }
        if (mode != 3)
        {
            if (mode == 4)
            {
                const RuntimeBodyAttachedActor* weapon = context.attachment.secondary;
                if (weapon == nullptr || weapon->actor == nullptr)
                {
                    throw Error("throw state mode 4 has no body +0x4C actor view");
                }
                launch.y = actor_world_position(*weapon->actor).y; // 0x000412A0, ESI the weapon
            }
            spread_runtime_throw(state.spread, launch, actor,
                context.random); // 0x000A3FE0, ECX actor
        }
        auto& attachment = context.attachment;
        teleport_runtime_body_secondary(body, attachment.ownership.attached, attachment.secondary,
            attachment.actions.second_receiver, launch); // 0x0001B5D0, EAX body, EBX the launch point
        state.thrown = 1;
    }
    const MinaPlayer* player = first_player(context.animation);
    bool done = true;
    if (player != nullptr)
    {
        done = false;
        if (player->mode == 1)
        {
            if (0.f > player->speed)
            {
                done = !(0.f < player->position);
            }
            else
            {
                done = !(player->position < 1.f); // DAT_0032B0F8.
            }
        }
    }
    if (done)
    {
        state.ended = 1;
    }
    if (state.ended == 0)
    {
        return;
    }
    u16 key = state.end_key;
    if (key == 0)
    {
        key = context.body_slot_key(0x20);
    }
    state.base.next_key = key;
}

// Xbox 0x000A3D20
void message_runtime_throw_state(const ThrowState& state, std::span<const u8> message,
    RuntimeThrowStateContext& context)
{
    if (read_u32(message, 0) != 0x25)
    {
        return;
    }
    u16 key = state.end_key;
    if (key == 0)
    {
        key = context.body_slot_key(0x20);
    }
    select_runtime_body_action(context.body, key, context.attachment.actions); // 0x00027650, EBX body, AX key
}
}
