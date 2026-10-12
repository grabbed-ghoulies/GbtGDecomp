#include "vector_normalize.h"
#include <bit>
#include <cmath>

namespace gbtg
{
// Xbox 0x00184F05
long double runtime_vector_length_squared(const Vec3& vector)
{
    return (static_cast<long double>(vector.x) * vector.x
        + static_cast<long double>(vector.y) * vector.y)
        + static_cast<long double>(vector.z) * vector.z;
}

// Xbox 0x00184600
bool runtime_graphics_scalars_equal(float first, float second)
{
    const long double difference = static_cast<long double>(first) - second;
    return std::bit_cast<float>(0xB727C5ACu) <= difference
        && difference <= std::bit_cast<float>(0x3727C5ACu);
}

// Xbox 0x00184EDD
void divide_runtime_graphics_vector(Vec3& output, const Vec3& vector, float divisor)
{
    const Vec3 source = vector;
    const long double reciprocal = 1.L / divisor;
    const float rounded = float(reciprocal);
    output.x = float(static_cast<long double>(rounded) * source.x);
    output.y = float(static_cast<long double>(rounded) * source.y);
    output.z = float(reciprocal * source.z);
}

// Xbox 0x00184F99
void normalize_runtime_graphics_vector(Vec3& output, const Vec3& vector)
{
    const float squared = float(runtime_vector_length_squared(vector));
    if (!runtime_graphics_scalars_equal(squared, 1.f))
    {
        if (!(squared > std::bit_cast<float>(0x2EDBE6FEu))) // DAT_00203738; unordered clears too.
        {
            output = {0.f, 0.f, 0.f};
            return;
        }
        Vec3 divided;
        divide_runtime_graphics_vector(divided, vector, float(std::sqrt(static_cast<long double>(squared))));
        output = divided;
        return;
    }
    if (&output != &vector)
    {
        output = vector;
    }
}
}
