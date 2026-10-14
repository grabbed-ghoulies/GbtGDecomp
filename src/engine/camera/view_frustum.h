#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
// Display view +0x138 block: origins, then six planes from +0x48 (float 18).
u32 classify_runtime_sphere_frustum(std::span<const float, 42> frustum, const Vec3& center, float radius,
    u8& plane); // Xbox 0x00159AD0, 0 outside, 1 crossing, 2 inside
// The box between two corners against the six planes, starting at `plane`; an outside plane is kept there.
u8 classify_runtime_box_frustum(std::span<const float, 42> frustum, const Vec3& first, const Vec3& second,
    u8& plane); // Xbox 0x00159BA0, 0 outside, 1 crossing, 2 inside
}
