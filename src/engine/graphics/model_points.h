#pragma once

#include "gbtg/types.h"
#include <span>

namespace gbtg
{
void average_runtime_model_points(Vec3& output,
    std::span<const Vec3> points); // Xbox 0x0010BD40
}
