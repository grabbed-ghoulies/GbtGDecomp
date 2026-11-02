#include "body_gravity_motion.h"
#include "actor_sound_pose.h"
#include "player_overlay.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float motion_angle(float value)
{
    constexpr float circle = std::bit_cast<float>(0x40C90FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    const long double remainder = animation_remainder(value, circle);
    float result = float(remainder);
    if (remainder < -static_cast<long double>(pi))
    {
        result += circle;
    }
    if (result >= pi)
    {
        result -= circle;
    }
    return result;
}
}

// Xbox 0x000259C0
float runtime_body_motion_speed(std::span<const u8> body, std::span<const u8> actor,
    const RuntimeBodyDamageTables& tables)
{
    const u16 tag = read_u16(actor, 0xCF4);
    const u8 key = body[0x328];
    if (tag == 0)
    {
        return read_f32(body, 0x32C) * read_f32(tables.values, size_t(key) * 12 + 8);
    }
    return read_f32(body, 0x32C) * interpolate_runtime_damage_value(key,
        tables.classes[size_t(tag) * 0x87 + key - 0x48], tables);
}

// Xbox 0x0002B300
void limit_runtime_body_backward_motion(Vec3& motion, std::span<const u8> body,
    const EntityBitBlock& flags, float lower)
{
    if ((motion.x != 0.f || motion.y != 0.f || motion.z != 0.f)
        && (flags.bytes[5] & 2) != 0 && body[0x19E] != 0)
    {
        Vec3 direction{motion.x, 0.f, motion.z};
        normalize_vector_in_place(&direction.x);
        float dot = (direction.z * read_f32(body, 0x18C) + direction.y * read_f32(body, 0x188))
            + direction.x * read_f32(body, 0x184);
        if (dot < 0.f)
        {
            if (dot < lower)
            {
                dot = lower;
            }
            const float factor = dot + 1.f;
            motion.x *= factor;
            motion.z *= factor;
        }
    }
}

// Xbox 0x0002AE90
void step_runtime_gravity_motion(std::span<u8> body, std::span<const u8> actor_bytes,
    Actor& actor, const ActorAnimationState& animation, const RuntimeActorClock& clock,
    const EntityBitBlock& flags, const RuntimeBodyDamageTables& tables,
    std::span<float, 8> phases, float ratio_scale, float backward_lower,
    float turn_threshold, u32 limit, u32 reverse)
{
    const Vec3 start = get(body, body_field::position);
    const Vec3 velocity = actor.anchor;
    const float elapsed = runtime_frame_step(clock) * runtime_actor_rate(actor);
    const auto* controller = animation.controller.get();
    const MinaPlayer* player = controller == nullptr ? nullptr : controller->slots.at(0).player.get();
    Vec3 motion;
    if (player != nullptr && !player->mina.empty() && read_u16(player->mina, 0x1A) != 0)
    {
        if (controller->selected_root == nullptr)
        {
            throw Error("gravity motion reads animation controller +0x54 without its selected root");
        }
        const float scale = actor.scale_200 * actor.radius_xz;
        const Vec3 root{scale * controller->selected_root->translation.x, 0.f,
            scale * controller->selected_root->translation.z};
        float yaw = motion_angle(read_f32(body, 0x33C));
        if (reverse != 0)
        {
            const float delta = motion_angle(float(static_cast<long double>(yaw) - actor.euler.y));
            if (engine_absolute(delta) > turn_threshold)
            {
                yaw -= std::bit_cast<float>(0x40490FDBu);
            }
        }
        rotate_euler_vector(Vec3{0.f, yaw, 0.f}, root, motion);
        motion.y = velocity.y * elapsed + motion.y;
    }
    else
    {
        const float speed = float(retail_square_root(velocity.x * velocity.x + velocity.z * velocity.z));
        float ratio = speed / runtime_body_motion_speed(body, actor_bytes, tables);
        if (!(1.f > ratio))
        {
            ratio = 1.f;
        }
        const float spread = 0.5f - ratio * ratio_scale;
        phases[1] = 1.f - spread;
        phases[3] = spread + 1.f;
        phases[5] = phases[1];
        phases[7] = phases[3];
        const float position = player == nullptr ? 0.f : player->position;
        i32 first = 0;
        i32 last = 3;
        float lower = phases[0];
        float upper = phases[6];
        if (lower > position)
        {
            first = 3;
            last = 0;
            upper -= 1.f;
            std::swap(lower, upper);
        }
        else if (position > upper)
        {
            first = 3;
            last = 0;
            lower += 1.f;
            std::swap(lower, upper);
        }
        else
        {
            do
            {
                const i32 middle = (last - first) / 2 + first;
                if (position >= phases[size_t(middle) * 2])
                {
                    first = middle;
                }
                else
                {
                    last = middle;
                }
            } while (first + 1 < last);
            lower = phases[size_t(first) * 2];
            upper = phases[size_t(last) * 2];
        }
        const float a = phases[size_t(first) * 2 + 1];
        const float b = phases[size_t(last) * 2 + 1];
        float factor = b;
        if (upper != lower)
        {
            const float interpolated = ((position - lower) / (upper - lower)) * (b - a) + a;
            if (b > a)
            {
                if (!(interpolated > b))
                {
                    factor = a > interpolated ? a : interpolated;
                }
            }
            else if (!(b > interpolated))
            {
                factor = interpolated > a ? a : interpolated;
            }
        }
        motion = Vec3{(factor * velocity.x) * elapsed, velocity.y * elapsed,
            (factor * velocity.z) * elapsed};
    }
    if (limit != 0)
    {
        limit_runtime_body_backward_motion(motion, body, flags, backward_lower);
    }
    const Vec3 result{motion.x + start.x, start.y + motion.y, motion.z + start.z};
    std::memcpy(body.data() + 0x18, &result, sizeof(result));
}
}
