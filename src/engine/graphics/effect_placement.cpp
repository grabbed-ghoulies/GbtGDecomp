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
    const auto speed = get(effect, actor_effect_field::speed);
    const float lower = speed[0];
    const float upper = speed[1];
    const float fraction = placement_random_fraction(random);
    return (upper - lower) * fraction + lower;
}
}

// Xbox 0x000EEA70
void jitter_actor_effect_position(Vec3& position, std::span<const u8> effect, u32& random)
{
    const float x = get(effect, actor_effect_field::jitter).x;
    const float x_lower = 0.f - x;
    const float x_fraction = placement_random_fraction(random);
    position.x = ((x - x_lower) * x_fraction + position.x) + x_lower;
    const float y = get(effect, actor_effect_field::jitter).y;
    const float y_lower = 0.f - y;
    const float y_fraction = placement_random_fraction(random);
    position.y = ((y - y_lower) * y_fraction + y_lower) + position.y;
    const float z = get(effect, actor_effect_field::jitter).z;
    const float z_lower = 0.f - z;
    const float z_fraction = placement_random_fraction(random);
    position.z = ((z - z_lower) * z_fraction + position.z) + z_lower;
}

// Xbox 0x000EE5C0
void initialize_actor_effect_velocity(EffectParticle& particle, std::span<const u8> effect, u32& random)
{
    float speed = effect_speed(effect, random);
    Vec3 velocity = particle.velocity;
    const u32 mode = get(effect, actor_effect_field::velocity_mode);
    switch (mode)
    {
    case 0:
        velocity = {0.f, 0.f, 0.f};
        break;
    case 1:
    case 2:
    {
        const Vec3 point = particle.position;
        const Vec3 origin = mode == 1 ? get(effect, actor_effect_field::position)
            : get(effect, actor_effect_field::emit_point);
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
        euler.x = sample_runtime_angle_range(get(effect, actor_effect_field::angle_upper).x,
            get(effect, actor_effect_field::angle_lower).x, random);
        euler.y = sample_runtime_angle_range(get(effect, actor_effect_field::angle_upper).y,
            get(effect, actor_effect_field::angle_lower).y, random);
        euler.z = sample_runtime_angle_range(get(effect, actor_effect_field::angle_upper).z,
            get(effect, actor_effect_field::angle_lower).z, random);
        rotate_euler_vector(euler, velocity, velocity);
        rotate_euler_vector(get(effect, actor_effect_field::volume_euler), velocity, velocity);
        if (mode == 4)
        {
            rotate_euler_vector(get(effect, actor_effect_field::euler), velocity, velocity);
        }
        break;
    }
    }
    velocity.x = velocity.x + get(effect, actor_effect_field::velocity).x;
    velocity.y = get(effect, actor_effect_field::velocity).y + velocity.y;
    velocity.z = get(effect, actor_effect_field::velocity).z + velocity.z;
    const float scale = get(effect, actor_effect_field::scale);
    velocity.x *= scale;
    velocity.y *= scale;
    velocity.z *= scale;
    particle.velocity = velocity;
}

// Xbox 0x000EED50
void place_actor_effect_particle(std::span<const u8> effect, EffectParticle& particle,
    std::span<const u8> owner, Vec3& scratch, u32& random)
{
    const u32 kind = get(effect, actor_effect_field::volume);
    const i32 surface = get(effect, actor_effect_field::flags) & 4;
    switch (kind)
    {
    case 0:
        scratch = {0.f, 0.f, 0.f};
        break;
    case 1:
        sample_runtime_line_position(
            scratch, get(owner, actor_effect_field::scale) * get(effect, actor_effect_field::volume_size).x, random);
        break;
    case 2:
        sample_runtime_ellipse_position(
            {get(effect, actor_effect_field::volume_size).x * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).z * get(owner, actor_effect_field::scale), 0.f},
            scratch, surface, random);
        break;
    case 3:
        sample_runtime_rectangle_position(
            {get(effect, actor_effect_field::volume_size).x * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).z * get(owner, actor_effect_field::scale), 0.f},
            scratch, surface, random);
        break;
    case 4:
        sample_runtime_box_position(
            {get(effect, actor_effect_field::volume_size).x * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).y * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).z * get(owner, actor_effect_field::scale)},
            scratch, surface, random);
        break;
    case 5:
        sample_runtime_ellipsoid_position(
            {get(effect, actor_effect_field::volume_size).x * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).y * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).z * get(owner, actor_effect_field::scale)},
            scratch, surface, get(effect, actor_effect_field::volume_upper).x,
            get(effect, actor_effect_field::volume_lower).x, get(effect, actor_effect_field::volume_upper).y,
            get(effect, actor_effect_field::volume_lower).y, random);
        break;
    case 6:
        sample_runtime_cylinder_position(
            {get(effect, actor_effect_field::volume_size).x * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).y * get(owner, actor_effect_field::scale),
             get(effect, actor_effect_field::volume_size).z * get(owner, actor_effect_field::scale)},
            scratch, surface, get(effect, actor_effect_field::volume_upper).x,
            get(effect, actor_effect_field::volume_lower).x, random);
        break;
    }
    if ((get(effect, actor_effect_field::flags) & 0x10) != 0)
    {
        jitter_actor_effect_position(scratch, effect, random);
    }
    rotate_euler_vector(get(effect, actor_effect_field::volume_euler), scratch, scratch);
    if ((get(effect, actor_effect_field::flags) & 2) != 0)
    {
        rotate_euler_vector(get(effect, actor_effect_field::euler), scratch, scratch);
    }
    const Vec3 origin = get(effect, actor_effect_field::emit_point);
    particle.position = {origin.x + scratch.x, origin.y + scratch.y, origin.z + scratch.z};
    initialize_actor_effect_velocity(particle, effect, random);
    Vec3 velocity = particle.velocity;
    velocity.x = velocity.x + get(effect, actor_effect_field::motion).x;
    velocity.y = get(effect, actor_effect_field::motion).y + velocity.y;
    velocity.z = get(effect, actor_effect_field::motion).z + velocity.z;
    particle.velocity = velocity;
    particle.euler = get(effect, actor_effect_field::euler);
}
}
