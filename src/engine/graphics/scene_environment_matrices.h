#pragma once
#include "gbtg/model_render.h"

namespace gbtg
{
void make_environment_direction_rotation(const Vec3& first, const Vec3& second,
    Quat& rotation); // Xbox 0x00153EB0
void build_runtime_environment_matrices(float* primary, float* secondary,
    RuntimeSceneLightDirection& lighting,
    const std::function<void(Vec3&, const Vec3&)>& normalize); // Xbox 0x000FCDF0
}
