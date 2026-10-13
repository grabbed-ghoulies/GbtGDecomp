#include "direction_pitch.h"
#include "entity.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00106000
bool runtime_direction_pitch(const Vec3& direction, float& pitch)
{
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    const float y = direction.y;
    const float horizontal = float(retail_square_root(direction.z * direction.z + direction.x * direction.x));
    if (!(engine_absolute(y) > epsilon))
    {
        if (!(engine_absolute(horizontal) > epsilon))
        {
            pitch = 0.f;
            return false;
        }
    }
    pitch = float(retail_atan2(0.f - y, horizontal));
    return true;
}
}
