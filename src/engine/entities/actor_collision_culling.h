#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
bool runtime_collision_shape_list_has_physical_shape(std::span<const u8> list); // Xbox 0x000559F0
bool runtime_collision_spheres_overlap(const Vec3& first, float first_radius,
    const Vec3& second, float second_radius); // Xbox 0x0015A270
bool runtime_actor_culls_segment(const Actor& actor, std::span<const u8> bytes,
    const Vec3& start, const Vec3& end); // Xbox 0x0003E4A0
bool runtime_actor_culls_swept_sphere(const Actor& actor, std::span<const u8> bytes,
    const Vec3& start, const Vec3& end, float radius); // Xbox 0x0003E590
bool runtime_actor_culls_sphere(const Actor& actor, std::span<const u8> bytes,
    const Vec3& center, float radius); // Xbox 0x0003E680
}
