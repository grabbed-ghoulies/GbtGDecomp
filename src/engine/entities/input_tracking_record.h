#pragma once
#include "gbtg/types.h"

#include <array>

namespace gbtg
{
// input_tracking_field as a struct, for records kept as structs; 0x00109810 takes its retail_bytes.
struct InputTrackingRecord
{
    std::array<float, 2> axes{}; // +0
    float activity = 0.f; // +8
    float accumulation = 0.f; // +0xC
    float hold_timer = 0.f; // +0x10
    u32 state = 0; // +0x14
};
static_assert(sizeof(InputTrackingRecord) == 0x18);
}
