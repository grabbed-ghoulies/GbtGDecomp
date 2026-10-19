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
// ESI body: clears the turn words +0x72C / +0x734, first easing toward the body's +0x184 point when +0x19E is set.
void settle_runtime_body_turn(std::span<u8> body, const Actor& actor); // Xbox 0x00134720
// EAX body: the heading from its actor (body +8) to the point at body +0x3C.
float runtime_body_point_heading(std::span<const u8> body, const Actor& actor); // Xbox 0x000197B0
}
