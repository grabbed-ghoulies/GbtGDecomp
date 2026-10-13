#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void build_runtime_pose_matrix(float matrix[12], const Vec3& position, const Vec3& euler,
    const Vec3& scale, float* inverse = nullptr); // Xbox 0x00103CB0
}
