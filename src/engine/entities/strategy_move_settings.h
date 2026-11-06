#pragma once

#include "gbtg/types.h"

namespace gbtg
{
// The six words several strategies' parameter blocks hold for their position requests: 0x00138310 reads the width and
// range, and 0x00139FA0 and 0x0013A000 copy the rest into the request. 0x0013A0A0 reads all of them.
struct StrategyMoveSettings
{
    float width; // +0x0, the heading's random spread.
    float range; // +0x4
    float radius; // +0x8
    float approach; // +0xC
    float speed_low; // +0x10
    float speed_high; // +0x14
};
static_assert(sizeof(StrategyMoveSettings) == 0x18);
}
