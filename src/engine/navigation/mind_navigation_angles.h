#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
void scale_runtime_planar_direction(std::array<float, 2>& direction,
    float length); // Xbox 0x00105DD0
float interpolate_runtime_mind_angle(float angle, float reference,
    float fraction); // Xbox 0x00100E90
float reflect_runtime_angle(float angle, float reference); // Xbox 0x00100D00
float nearest_runtime_angle(float reference, float first, float second); // Xbox 0x00100B80
void randomize_runtime_mind_escape_heading(const Actor& actor,
    std::span<const u8> target, const Vec3& destination, float& heading,
    u32& random); // Xbox 0x00136030
}
