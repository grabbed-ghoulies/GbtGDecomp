#include "euler_wrapping.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x001059B0
void wrap_runtime_euler_angles(Vec3& euler)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    constexpr float negative_pi = std::bit_cast<float>(0xC0490FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    float* angles[] = {&euler.x, &euler.y, &euler.z};
    for (size_t axis = 0; axis < 3; ++axis)
    {
        const long double remainder = animation_remainder(*angles[axis], turn);
        float result = float(remainder);
        if (negative_pi > remainder)
        {
            result += turn;
        }
        if (result >= pi)
        {
            result -= turn;
        }
        *angles[axis] = result;
    }
}
}
