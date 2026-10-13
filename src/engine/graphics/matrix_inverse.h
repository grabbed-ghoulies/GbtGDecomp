#pragma once

#include "gbtg/types.h"
#include <span>

namespace gbtg
{
bool runtime_graphics_double_is_finite(double value); // Xbox 0x0016744D
void invert_runtime_graphics_matrix(std::span<float, 16> output, float* determinant,
    std::span<const float, 16> input); // Xbox 0x00151FAD
}
