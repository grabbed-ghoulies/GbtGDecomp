#pragma once
#include "gbtg/types.h"

namespace gbtg
{
bool runtime_shader_issue_value_near(float value, float reference, float margin); // Xbox 0x001856D5
void update_runtime_shader_issue_schedule(u8* table, u32 latency, u8 reg, u8 mask,
    float issued, i32 reason); // Xbox 0x00185961
}
