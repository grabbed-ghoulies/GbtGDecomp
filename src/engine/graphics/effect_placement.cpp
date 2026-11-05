#include "effect_placement.h"
#include "effect_particles.h"
#include "../entities/random_placement.h"
#include "../entities/actor_sound_pose.h"
#include "../camera/entity_camera.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float placement_random_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
float effect_speed(std::span<const u8> effect, u32& random)
{
    const auto speed = effect_header(effect).speed;
    const float lower = speed[0];
    const float upper = speed[1];
    const float fraction = placement_random_fraction(random);
    return (upper - lower) * fraction + lower;
}
}

// Xbox 0x000EEA70
void jitter_actor_effect_position(Vec3& position, std::span<const u8> effect, u32& random)
{
    const float x = effect_header(effect).jitter.x;
    const float x_lower = 0.f - x;
    const float x_fraction = placement_random_fraction(random);
    position.x = ((x - x_lower) * x_fraction + position.x) + x_lower;
    const float y = effect_header(effect).jitter.y;
    const float y_lower = 0.f - y;
    const float y_fraction = placement_random_fraction(random);
    position.y = ((y - y_lower) * y_fraction + y_lower) + position.y;
    const float z = effect_header(effect).jitter.z;
    const float z_lower = 0.f - z;
    const float z_fraction = placement_random_fraction(random);
    position.z = ((z - z_lower) * z_fraction + position.z) + z_lower;
}

// Xbox 0x000EE5C0
void initialize_actor_effect_velocity(EffectParticle& particle, std::span<const u8> effect, u32& random)
{
    float speed = effect_speed(effect, random);
    Vec3 velocity = particle.velocity;
    const u32 mode = effect_header(effect).velocity_mode;
    switch (mode)
    {
    case 0:
        velocity = {0.f, 0.f, 0.f};
        break;
    case 1:
    case 2:
    {
        const Vec3 point = particle.position;
        const Vec3 origin = mode == 1 ? effect_header(effect).position
            : effect_header(effect).emit_point;
        const Vec3 direction{point.x - origin.x, point.y - origin.y, point.z - origin.z};
        Vec3 euler;
        direction_to_yaw_pitch(direction.x, direction.y, direction.z, euler.y, euler.x);
        euler.z = 0.f;
        speed = effect_speed(effect, random);
        rotate_euler_vector(euler, {0.f, 0.f, speed}, velocity);
        break;
    }
    case 3:
    case 4:
    case 5:
    {
        if (mode == 5)
        {
            speed = effect_speed(effect, random);
        }
        velocity = {0.f, 0.f, speed};
        Vec3 euler;
        euler.x = sample_runtime_angle_range(effect_header(effect).angle_upper.x,
            effect_header(effect).angle_lower.x, random);
        euler.y = sample_runtime_angle_range(effect_header(effect).angle_upper.y,
            effect_header(effect).angle_lower.y, random);
        euler.z = sample_runtime_angle_range(effect_header(effect).angle_upper.z,
            effect_header(effect).angle_lower.z, random);
        rotate_euler_vector(euler, velocity, velocity);
        rotate_euler_vector(effect_header(effect).volume_euler, velocity, velocity);
        if (mode == 4)
        {
            rotate_euler_vector(effect_header(effect).euler, velocity, velocity);
        }
        break;
    }
    }
    velocity.x = velocity.x + effect_header(effect).velocity.x;
    velocity.y = effect_header(effect).velocity.y + velocity.y;
    velocity.z = effect_header(effect).velocity.z + velocity.z;
    const float scale = effect_header(effect).scale;
    velocity.x *= scale;
    velocity.y *= scale;
    velocity.z *= scale;
    particle.velocity = velocity;
}

// Xbox 0x000EED50
void place_actor_effect_particle(std::span<const u8> effect, EffectParticle& particle,
    std::span<const u8> owner, Vec3& scratch, u32& random)
{
    const u32 kind = effect_header(effect).volume;
    const i32 surface = effect_header(effect).flags & 4;
    switch (kind)
    {
    case 0:
        scratch = {0.f, 0.f, 0.f};
        break;
    case 1:
        sample_runtime_line_position(
            scratch, effect_header(owner).scale * effect_header(effect).volume_size.x, random);
        break;
    case 2:
        sample_runtime_ellipse_position(
            {effect_header(effect).volume_size.x * effect_header(owner).scale,
             effect_header(effect).volume_size.z * effect_header(owner).scale, 0.f},
            scratch, surface, random);
        break;
    case 3:
        sample_runtime_rectangle_position(
            {effect_header(effect).volume_size.x * effect_header(owner).scale,
             effect_header(effect).volume_size.z * effect_header(owner).scale, 0.f},
            scratch, surface, random);
        break;
    case 4:
        sample_runtime_box_position(
            {effect_header(effect).volume_size.x * effect_header(owner).scale,
             effect_header(effect).volume_size.y * effect_header(owner).scale,
             effect_header(effect).volume_size.z * effect_header(owner).scale},
            scratch, surface, random);
        break;
    case 5:
        sample_runtime_ellipsoid_position(
            {effect_header(effect).volume_size.x * effect_header(owner).scale,
             effect_header(effect).volume_size.y * effect_header(owner).scale,
             effect_header(effect).volume_size.z * effect_header(owner).scale},
            scratch, surface, effect_header(effect).volume_upper.x,
            effect_header(effect).volume_lower.x, effect_header(effect).volume_upper.y,
            effect_header(effect).volume_lower.y, random);
        break;
    case 6:
        sample_runtime_cylinder_position(
            {effect_header(effect).volume_size.x * effect_header(owner).scale,
             effect_header(effect).volume_size.y * effect_header(owner).scale,
             effect_header(effect).volume_size.z * effect_header(owner).scale},
            scratch, surface, effect_header(effect).volume_upper.x,
            effect_header(effect).volume_lower.x, random);
        break;
    }
    if ((effect_header(effect).flags & 0x10) != 0)
    {
        jitter_actor_effect_position(scratch, effect, random);
    }
    rotate_euler_vector(effect_header(effect).volume_euler, scratch, scratch);
    if ((effect_header(effect).flags & 2) != 0)
    {
        rotate_euler_vector(effect_header(effect).euler, scratch, scratch);
    }
    const Vec3 origin = effect_header(effect).emit_point;
    particle.position = {origin.x + scratch.x, origin.y + scratch.y, origin.z + scratch.z};
    initialize_actor_effect_velocity(particle, effect, random);
    Vec3 velocity = particle.velocity;
    velocity.x = velocity.x + effect_header(effect).motion.x;
    velocity.y = effect_header(effect).motion.y + velocity.y;
    velocity.z = effect_header(effect).motion.z + velocity.z;
    particle.velocity = velocity;
    particle.euler = effect_header(effect).euler;
}
}
