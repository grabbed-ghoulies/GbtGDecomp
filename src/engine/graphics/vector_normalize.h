#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
long double runtime_vector_length_squared(const Vec3& vector); // Xbox 0x00184F05
bool runtime_graphics_scalars_equal(float first, float second); // Xbox 0x00184600
void divide_runtime_graphics_vector(Vec3& output, const Vec3& vector,
    float divisor); // Xbox 0x00184EDD
void normalize_runtime_graphics_vector(Vec3& output, const Vec3& vector); // Xbox 0x00184F99
}
