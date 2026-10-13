#include "random_state.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00105140
float advance_runtime_random_fraction(u32& state)
{
    state = state * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}
