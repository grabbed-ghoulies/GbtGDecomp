#include "effect_placement.h"
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
Vec3 effect_vector(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
void store_effect_vector(std::span<u8> bytes, size_t offset, const Vec3& vector)
{
    std::memcpy(bytes.data() + offset, &vector, 12);
}
float effect_speed(std::span<const u8> effect, u32& random)
{
    const float lower = read_f32(effect, 0x114);
    const float upper = read_f32(effect, 0x118);
    const float fraction = placement_random_fraction(random);
    return (upper - lower) * fraction + lower;
}
}

// Xbox 0x000EEA70
void jitter_actor_effect_position(Vec3& position, std::span<const u8> effect, u32& random)
{
    const float x = read_f32(effect, 0x74);
    const float x_lower = 0.f - x;
    const float x_fraction = placement_random_fraction(random);
    position.x = ((x - x_lower) * x_fraction + position.x) + x_lower;
    const float y = read_f32(effect, 0x78);
    const float y_lower = 0.f - y;
    const float y_fraction = placement_random_fraction(random);
    position.y = ((y - y_lower) * y_fraction + y_lower) + position.y;
    const float z = read_f32(effect, 0x7C);
    const float z_lower = 0.f - z;
    const float z_fraction = placement_random_fraction(random);
    position.z = ((z - z_lower) * z_fraction + position.z) + z_lower;
}

// Xbox 0x000EE5C0
void initialize_actor_effect_velocity(std::span<u8> particle, std::span<const u8> effect, u32& random)
{
    float speed = effect_speed(effect, random);
    Vec3 velocity = effect_vector(particle, 0x18);
    const u32 mode = read_u32(effect, 0x110);
    switch (mode)
    {
    case 0:
        velocity = {0.f, 0.f, 0.f};
        break;
    case 1:
    case 2:
    {
        const Vec3 point = effect_vector(particle, 0);
        const Vec3 origin = effect_vector(effect, mode == 1 ? 0x1C : 0x34);
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
        euler.x = sample_runtime_angle_range(read_f32(effect, 0x128), read_f32(effect, 0x11C), random);
        euler.y = sample_runtime_angle_range(read_f32(effect, 0x12C), read_f32(effect, 0x120), random);
        euler.z = sample_runtime_angle_range(read_f32(effect, 0x130), read_f32(effect, 0x124), random);
        rotate_euler_vector(euler, velocity, velocity);
        rotate_euler_vector(effect_vector(effect, 0xA0), velocity, velocity);
        if (mode == 4)
        {
            rotate_euler_vector(effect_vector(effect, 0x50), velocity, velocity);
        }
        break;
    }
    }
    velocity.x = velocity.x + read_f32(effect, 0x8C);
    velocity.y = read_f32(effect, 0x90) + velocity.y;
    velocity.z = read_f32(effect, 0x94) + velocity.z;
    const float scale = read_f32(effect, 0xB8);
    velocity.x *= scale;
    velocity.y *= scale;
    velocity.z *= scale;
    store_effect_vector(particle, 0x18, velocity);
}

// Xbox 0x000EED50
void place_actor_effect_particle(std::span<const u8> effect, std::span<u8> particle,
    std::span<const u8> owner, Vec3& scratch, u32& random)
{
    const u32 kind = read_u32(effect, 0xDC);
    const i32 surface = effect[0x18] & 4;
    switch (kind)
    {
    case 0:
        scratch = {0.f, 0.f, 0.f};
        break;
    case 1:
        sample_runtime_line_position(scratch, read_f32(owner, 0xB8) * read_f32(effect, 0xE0), random);
        break;
    case 2:
        sample_runtime_ellipse_position({read_f32(effect, 0xE0) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE8) * read_f32(owner, 0xB8), 0.f}, scratch, surface, random);
        break;
    case 3:
        sample_runtime_rectangle_position({read_f32(effect, 0xE0) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE8) * read_f32(owner, 0xB8), 0.f}, scratch, surface, random);
        break;
    case 4:
        sample_runtime_box_position({read_f32(effect, 0xE0) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE4) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE8) * read_f32(owner, 0xB8)}, scratch, surface, random);
        break;
    case 5:
        sample_runtime_ellipsoid_position({read_f32(effect, 0xE0) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE4) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE8) * read_f32(owner, 0xB8)}, scratch, surface,
            read_f32(effect, 0xF8), read_f32(effect, 0xEC), read_f32(effect, 0xFC),
            read_f32(effect, 0xF0), random);
        break;
    case 6:
        sample_runtime_cylinder_position({read_f32(effect, 0xE0) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE4) * read_f32(owner, 0xB8),
            read_f32(effect, 0xE8) * read_f32(owner, 0xB8)}, scratch, surface,
            read_f32(effect, 0xF8), read_f32(effect, 0xEC), random);
        break;
    }
    if ((effect[0x18] & 0x10) != 0)
    {
        jitter_actor_effect_position(scratch, effect, random);
    }
    rotate_euler_vector(effect_vector(effect, 0xA0), scratch, scratch);
    if ((effect[0x18] & 2) != 0)
    {
        rotate_euler_vector(effect_vector(effect, 0x50), scratch, scratch);
    }
    const Vec3 origin = effect_vector(effect, 0x34);
    store_effect_vector(particle, 0, {origin.x + scratch.x, origin.y + scratch.y, origin.z + scratch.z});
    initialize_actor_effect_velocity(particle, effect, random);
    Vec3 velocity = effect_vector(particle, 0x18);
    velocity.x = velocity.x + read_f32(effect, 0x40);
    velocity.y = read_f32(effect, 0x44) + velocity.y;
    velocity.z = read_f32(effect, 0x48) + velocity.z;
    store_effect_vector(particle, 0x18, velocity);
    std::memcpy(particle.data() + 0xC, effect.data() + 0x50, 12);
}
}
