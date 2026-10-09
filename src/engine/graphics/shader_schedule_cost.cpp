#include "shader_schedule_cost.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00185750
long double runtime_shader_schedule_tail_cost(const ShaderScheduleCostState& state, u32& reason)
{
    if (state.enabled == 0)
    {
        reason = 0x4B;
        return 18.f; // DAT_0032B67C: 0x41900000
    }
    float pending = 0.f;
    reason = 0x20;
    long double cost = 0;
    i32 index = 0;
    do
    {
        if (state.pending[index] != 0)
        {
            pending = static_cast<float>(static_cast<long double>(pending) + 1.0L); // double DAT_0032B370
        }
        ++index;
    } while (index < 16);
    if (state.issued < pending)
    {
        cost = static_cast<long double>(pending) - static_cast<long double>(state.issued);
        reason = 0x4A;
    }
    const double minimum = std::bit_cast<double>(u64{0x400199999999999A}); // DAT_00203740
    if (static_cast<long double>(state.issued) < static_cast<long double>(minimum))
    {
        const float shortfall = static_cast<float>(static_cast<long double>(minimum)
            - static_cast<long double>(state.issued));
        if (cost < static_cast<long double>(shortfall))
        {
            reason = 0x4C;
            cost = shortfall;
        }
    }
    return cost;
}
}
