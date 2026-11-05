#pragma once
#include "gbtg/types.h"

#include <array>

namespace gbtg
{
// input_tracking_field as a struct, for records kept as structs; 0x00109810 takes its retail_bytes.
struct InputTrackingRecord
{
    std::array<float, 2> axes{}; // +0, the last tick's
    float activity = 0.f; // +8, rises while the stick moves, up to 1
    float accumulation = 0.f; // +0xC, rises up to the preset's cap
    float hold_timer = 0.f; // +0x10, the state flips once it runs out
    u32 state = 0; // +0x14, 1 waits for full activity, 2 for none
};
static_assert(sizeof(InputTrackingRecord) == 0x18);
}
