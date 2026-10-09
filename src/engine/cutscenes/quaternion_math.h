#pragma once
#include "gbtg/types.h"

namespace gbtg
{
Quat slerp(Quat first, Quat second, float fraction); // Xbox 0x00154030
Quat animation_quaternion_product(Quat first, Quat second); // Xbox 0x00153DC0
long double animation_wrap_angle(float angle); // Xbox 0x00153530
void cutscene_sine_cosine(float angle, float& sine, float& cosine); // Xbox 0x001535C0
void ordered_euler_quaternion(u32 order, const Vec3& euler, Quat& output); // Xbox 0x00154500
}
