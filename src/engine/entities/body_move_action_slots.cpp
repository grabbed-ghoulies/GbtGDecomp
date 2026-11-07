#include "body_move_action_slots.h"
#include <cstddef>
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_fields.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
float settle_component(float target, float state, float output)
{
    if ((target > state && output > target) || (state > target && target > output))
    {
        return target;
    }
    constexpr float kTolerance = std::bit_cast<float>(0x3727C5ACu); // DAT_0032B230.
    if (kTolerance > engine_absolute(output - target))
    {
        return target;
    }
    return output;
}
}

// Xbox 0x00103880
void integrate_runtime_motion(const Vec3& state, const Vec3& rate, Vec3& state_out, Vec3& rate_out,
    std::span<const u8> context, float step, i32 steps, const RuntimeMotionDerivative& derivative,
    const RuntimeMotionConstraint& state_constraint, const RuntimeMotionConstraint& rate_constraint)
{
    if (steps == 0 || step == 0.f)
    {
        state_out = state;
        rate_out = rate;
        return;
    }
    const float half = step * 0.5f; // DAT_0032B0F4.
    Vec3 position = state;
    Vec3 velocity = rate;
    Vec3 next_position;
    Vec3 next_velocity;
    u32 remaining = u32(steps);
    do
    {
        Vec3 slope{};
        derivative(context, position, velocity, slope);
        next_position = Vec3{slope.x * step + position.x, slope.y * step + position.y, slope.z * step + position.z};
        state_constraint(context, position, next_position);
        next_velocity = Vec3{(next_position.x + position.x) * half + velocity.x,
            (next_position.y + position.y) * half + velocity.y, (next_position.z + position.z) * half + velocity.z};
        rate_constraint(context, velocity, next_velocity);
        position = next_position;
        velocity = next_velocity;
    } while (--remaining != 0);
    state_out = position;
    rate_out = velocity;
}

// Xbox 0x00140700
void runtime_motion_toward_target(std::span<const u8> context, const Vec3& state, const Vec3&, Vec3& output)
{
    const float gain = get(context, motion_target::gain);
    const Vec3 target = get(context, motion_target::target);
    output.x = target.x - state.x;
    output.y = target.y - state.y;
    output.z = target.z - state.z;
    output.x = gain * output.x;
    output.y = gain * output.y;
    output.z = gain * output.z;
}

// Xbox 0x00140770
void runtime_motion_settle_target(std::span<const u8> context, const Vec3& state, Vec3& output)
{
    const Vec3 target = get(context, motion_target::target);
    output.x = settle_component(target.x, state.x, output.x);
    output.y = settle_component(target.y, state.y, output.y);
    output.z = settle_component(target.z, state.z, output.z);
}

// Xbox 0x000803B0
void ease_runtime_body_velocity(std::span<const u8> body, Actor& actor, const i32& steps)
{
    std::array<u8, 0x10> context{};
    std::memcpy(context.data(), body.data() + offsetof(BodyRecord, velocity.x), 0xC);
    std::memcpy(context.data() + 0xC, body.data() + offsetof(BodyRecord, ease_rate), 4);
    const Vec3 rate{};
    Vec3 velocity = actor.anchor;
    Vec3 rate_out{};
    integrate_runtime_motion(velocity, rate, velocity, rate_out, context, std::bit_cast<float>(0x3B5A740Eu), steps,
        runtime_motion_toward_target, runtime_motion_settle_target,
        [](std::span<const u8>, const Vec3&, Vec3&)
        {
            // 0x001628B0 returns at once.
        });
    actor.anchor = velocity;
}

// Xbox 0x00080200
void tick_runtime_xz_gravity_action(std::span<u8> action, std::span<u8> body, Actor& actor,
    const RuntimeActorClock& clock)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock) + get(action, move_action_field::elapsed);
    set(action, move_action_field::elapsed, elapsed);
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(actor);
    const Vec3 velocity = actor.anchor; // Actor +0x3C.
    body_record(body).velocity.x = velocity.x;
    body_record(body).velocity.z = velocity.z;
    const float vertical = rate * step * body_record(body).gravity + velocity.y;
    body_record(body).velocity.y = vertical;
    actor.anchor = Vec3{velocity.x, vertical, velocity.z};
}

// Xbox 0x00080340
void tick_runtime_velocity_action(std::span<u8> action, std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock) + get(action, move_action_field::elapsed);
    set(action, move_action_field::elapsed, elapsed);
    ease_runtime_body_velocity(body, actor, steps);
}

// Xbox 0x00140660
void ease_runtime_planar_velocity(Vec3& velocity, const Vec3& target, float gain, const i32& steps)
{
    const float vertical = velocity.y;
    std::array<u8, 0x10> context;
    std::memcpy(context.data(), &target, 12);
    std::memcpy(context.data() + 12, &gain, 4);
    Vec3 rate{};
    Vec3 rate_out;
    integrate_runtime_motion(velocity, rate, velocity, rate_out, context,
        std::bit_cast<float>(0x3B5A740Eu), steps, runtime_motion_toward_target, runtime_motion_settle_target,
        [](std::span<const u8>, const Vec3&, Vec3&)
        {
            // Xbox 0x001628B0 returns at once.
        });
    velocity.y = vertical;
}

// Xbox 0x00133E50
void update_runtime_gravity_velocity(std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps)
{
    Vec3 velocity = actor.anchor;
    const Vec3 target = body_record(body).velocity;
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(actor);
    ease_runtime_planar_velocity(velocity, target, body_record(body).ease_rate, steps);
    velocity.y = rate * step * body_record(body).gravity + velocity.y;
    actor.anchor = velocity;
}

// Xbox 0x0007FD70
void tick_runtime_gravity_action(std::span<u8> action, std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock) + get(action, move_action_field::elapsed);
    set(action, move_action_field::elapsed, elapsed);
    update_runtime_gravity_velocity(body, actor, clock, steps);
}

// Xbox 0x0002AD30
void step_runtime_body_motion(std::span<u8> body, Actor& actor, const ActorAnimationState& animation,
    const RuntimeActorClock& clock)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock);
    const Vec3 position = body_record(body).position;
    Vec3 moved;
    const AnimationController* controller = animation.controller.get();
    bool rooted = false;
    if (controller != nullptr)
    {
        if (controller->slots.empty())
        {
            throw Error("body motion reads animation controller +0x58 with no slot array");
        }
        const MinaPlayer* player = controller->slots[0].player.get();
        rooted = player != nullptr && player->mina.data() != nullptr && mina_root_motion(player->mina) != 0;
    }
    if (rooted)
    {
        if (controller->selected_root == nullptr)
        {
            throw Error("body motion reads animation controller +0x54 through address zero");
        }
        const float speed = actor.scale_200 * actor.radius_xz * controller->selected_root->translation.z;
        const float rise = actor.anchor.y;
        const float yaw = actor_world_euler(actor).y;
        moved.x = float(retail_sin(yaw) * speed + position.x);
        moved.y = rise * elapsed + position.y;
        moved.z = float(retail_cos(yaw) * speed + position.z);
    }
    else
    {
        moved = Vec3{actor.anchor.x * elapsed + position.x, actor.anchor.y * elapsed + position.y,
            actor.anchor.z * elapsed + position.z};
    }
    body_record(body).position.x = moved.x;
    body_record(body).position.y = moved.y;
    body_record(body).position.z = moved.z;
}

// Xbox 0x001405E0
void ease_runtime_vector_toward(Vec3& value, const Vec3& target, float gain, const i32& steps)
{
    std::array<u8, 0x10> context;
    std::memcpy(context.data(), &target, 12);
    std::memcpy(context.data() + 12, &gain, 4);
    const Vec3 rate{};
    Vec3 rate_out;
    integrate_runtime_motion(value, rate, value, rate_out, context, std::bit_cast<float>(0x3B5A740Eu), steps,
        runtime_motion_toward_target, runtime_motion_settle_target,
        [](std::span<const u8>, const Vec3&, Vec3&)
        {
            // Xbox 0x001628B0 returns at once.
        });
}

// Xbox 0x0007FE70
void message_runtime_stand_action(std::span<u8> action, std::span<const u8> message,
    RuntimeStandActionContext& context)
{
    const u32 id = read_u32(message, 0);
    if (id != 3)
    {
        if (id != 0x22)
        {
            return;
        }
        const Vec3 position = get(message, stand_action_message::position);
        actor_set_world_position(context.actor, position); // 0x00041730
        const Vec3 world = actor_world_position(context.actor); // 0x000412A0 into body +0x18.
        std::memcpy(context.body.data() + offsetof(BodyRecord, position.x), &world, 12);
    }
    set(action, stand_action_field::placed, 1u);
}

// Xbox 0x000800A0
void follow_runtime_stand_focus(std::span<u8> action, RuntimeStandActionContext& context)
{
    if (body_record(context.body).look_point_set == 0)
    {
        return;
    }
    const Vec3 focus{body_record(context.body).look_point.x, body_record(context.body).look_point.y,
        body_record(context.body).look_point.z};
    const u32 target = mind_record(context.mind).target;
    float speed;
    if (get(context.tunables, stand_action_tunables::follows_target_speed) == 0) // DAT_004275C4.
    {
        speed = 0.f;
    }
    else
    {
        const Actor* target_actor = context.actor_at(target);
        if (target_actor == nullptr || target_actor->euler_out == nullptr || target_actor->euler_out->actor == nullptr)
        {
            throw Error("camera stand focus target has no +0x95C body actor");
        }
        const Vec3& velocity = target_actor->euler_out->actor->anchor; // Body actor +0x3C.
        speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z); // 0x00152B30
    }
    const Vec3 from = get(action, stand_action_field::focus);
    Vec3 offset;
    context.aim(offset, from, focus, target, get(context.tunables, stand_action_tunables::unk_0), speed,
        get(context.tunables, stand_action_tunables::unk_4)); // DAT_004275BC, DAT_004275C0.
    Vec3 rate = get(action, stand_action_field::focus_rate);
    ease_runtime_vector_toward(rate, offset, body_record(context.body).ease_rate, context.steps);
    set(action, stand_action_field::focus_rate, rate);
    if (get(context.tunables, stand_action_tunables::snaps_focus) != 0) // DAT_004275C8.
    {
        set(action, stand_action_field::focus_rate, offset);
    }
}

// Xbox 0x0007FEC0
void tick_runtime_stand_action(std::span<u8> action, RuntimeStandActionContext& context)
{
    const float elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + get(action, move_action_field::elapsed);
    set(action, move_action_field::elapsed, elapsed);
    if (get(action, stand_action_field::placed) != 0)
    {
        std::memcpy(context.body.data() + offsetof(BodyRecord, position.x), context.mind.data()
            + offsetof(MindRecord, goal_point), 12);
        context.actor.anchor = {0.f, 0.f, 0.f}; // Actor +0x3C.
        if (body_record(context.body).look_point_set != 0)
        {
            set(action, stand_action_field::focus, body_record(context.body).look_point);
        }
        set(action, stand_action_field::focus_rate, Vec3{0.f, 0.f, 0.f});
        return;
    }
    ease_runtime_body_velocity(context.body, context.actor, context.steps); // 0x000803B0
    follow_runtime_stand_focus(action, context);
}

// Xbox 0x0007FFB0
void step_runtime_stand_action(std::span<u8> action, RuntimeStandActionContext& context,
    const ActorAnimationState& animation)
{
    step_runtime_body_motion(context.body, context.actor, animation, context.clock); // 0x0002AD30
    const float step = runtime_frame_step(context.clock);
    const float scaled = runtime_actor_rate(context.actor) * step;
    const bool keep_planar = get(context.tunables, stand_action_tunables::keeps_planar) != 0; // DAT_004275CC.
    const Vec3 rate = get(action, stand_action_field::focus_rate);
    const Vec3 focus = get(action, stand_action_field::focus);
    set(action, stand_action_field::focus, Vec3{rate.x * scaled + focus.x, rate.y * scaled + focus.y, rate.z * scaled + focus.z});
    if (keep_planar && body_record(context.body).look_point_set != 0)
    {
        const Vec3 look = body_record(context.body).look_point;
        Vec3 kept = get(action, stand_action_field::focus);
        kept.x = look.x;
        kept.z = look.z;
        set(action, stand_action_field::focus, kept);
    }
    body_record(context.body).look_point = get(action, stand_action_field::focus);
    body_record(context.body).look_point_set = 1;
    set(action, stand_action_field::placed, 0u);
}
}
