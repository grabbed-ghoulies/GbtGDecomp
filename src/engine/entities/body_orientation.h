#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
float runtime_actor_relative_point_angle(const Actor& actor,
    const Vec3& point); // Xbox 0x00041AE0
void face_runtime_body_point(std::span<u8> body, const Actor& actor,
    const Vec3& point); // Xbox 0x00134510
void face_runtime_body_target(std::span<u8> body, const Actor& actor,
    std::span<const u8> target); // Xbox 0x001345F0
}
