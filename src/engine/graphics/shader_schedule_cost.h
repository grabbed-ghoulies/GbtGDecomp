#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct ShaderScheduleCostState
{
    std::array<u8, 16> pending; // scheduler +0..+0xF
    u8 enabled; // +0x10
    float issued; // +0x14
};
long double runtime_shader_schedule_tail_cost(const ShaderScheduleCostState& state,
    u32& reason); // Xbox 0x00185750
}
