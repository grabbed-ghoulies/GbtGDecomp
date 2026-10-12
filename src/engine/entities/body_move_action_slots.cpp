#include "body_move_action_slots.h"
#include "actor_transform.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
Vec3 read_vec3(std::span<const u8> bytes, size_t offset)
{
    return Vec3{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}

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
    const float gain = read_f32(context, 0xC);
    output.x = read_f32(context, 0) - state.x;
    output.y = read_f32(context, 4) - state.y;
    output.z = read_f32(context, 8) - state.z;
    output.x = gain * output.x;
    output.y = gain * output.y;
    output.z = gain * output.z;
}

// Xbox 0x00140770
void runtime_motion_settle_target(std::span<const u8> context, const Vec3& state, Vec3& output)
{
    output.x = settle_component(read_f32(context, 0), state.x, output.x);
    output.y = settle_component(read_f32(context, 4), state.y, output.y);
    output.z = settle_component(read_f32(context, 8), state.z, output.z);
}

// Xbox 0x000803B0
void ease_runtime_body_velocity(std::span<const u8> body, Actor& actor, const i32& steps)
{
    std::array<u8, 0x10> context{};
    std::memcpy(context.data(), body.data() + 0x318, 0xC);
    std::memcpy(context.data() + 0xC, body.data() + 0x334, 4);
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

// Xbox 0x00080340
void tick_runtime_velocity_action(std::span<u8> action, std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock) + read_f32(action, 0xC);
    std::memcpy(action.data() + 0xC, &elapsed, 4);
    ease_runtime_body_velocity(body, actor, steps);
}

// Xbox 0x0002AD30
void step_runtime_body_motion(std::span<u8> body, Actor& actor, const ActorAnimationState& animation,
    const RuntimeActorClock& clock)
{
    const float elapsed = runtime_actor_rate(actor) * runtime_frame_step(clock);
    const Vec3 position = read_vec3(body, 0x18);
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
        rooted = player != nullptr && player->mina.data() != nullptr && read_u16(player->mina, 0x1A) != 0;
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
    std::memcpy(body.data() + 0x18, &moved.x, 4);
    std::memcpy(body.data() + 0x1C, &moved.y, 4);
    std::memcpy(body.data() + 0x20, &moved.z, 4);
}
}
