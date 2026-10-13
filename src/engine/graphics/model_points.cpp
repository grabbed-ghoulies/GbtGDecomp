#include "model_points.h"

namespace gbtg
{
// Xbox 0x0010BD40
void average_runtime_model_points(Vec3& output, std::span<const Vec3> points)
{
    output.z = 0.f;
    output.y = 0.f;
    output.x = 0.f;
    const u32 count = u32(points.size());
    if (count != 0)
    {
        // The four-point unroll and tail add each coordinate in source order.
        for (u32 index = 0; index < count; ++index)
        {
            output.x = points[index].x + output.x;
            output.y = points[index].y + output.y;
            output.z = points[index].z + output.z;
        }
        // Retail keeps the reciprocal in x87 through all three products.
        const long double reciprocal = 1.L / static_cast<long double>(count);
        output.x = float(reciprocal * output.x);
        output.y = float(reciprocal * output.y);
        output.z = float(reciprocal * output.z);
    }
}
}
