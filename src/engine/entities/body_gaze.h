#pragma once
#include "actor_transform.h"
#include "gbtg/anim.h"

namespace gbtg
{
long double runtime_input_axes_angle(const std::array<float, 2>& axes); // Xbox 0x00105D90
float runtime_relative_angle(float angle, float reference); // Xbox 0x00100E40
Vec3 runtime_body_gaze_origin(std::span<const u8> body, const Actor& actor); // Xbox 0x000251D0
void set_runtime_body_gaze_angle(std::span<u8> body, const Actor& actor,
    float angle); // Xbox 0x00025250
}
