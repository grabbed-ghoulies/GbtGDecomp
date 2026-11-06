#pragma once

#include "gbtg/types.h"

namespace gbtg
{
// A value that scales with one of an actor class's key bytes: DAT_00433C9C holds 0x48 rows, one per key, and the body's
// health (DAT_00433CA8), friction (DAT_00433D68) and chain limits (DAT_00433D5C) are rows of it. With both keys equal, or
// no class, the value is value_last.
struct BodyDamageCurve
{
    u8 key_first; // +0x0
    u8 key_last; // +0x1
    u8 pad_02[2]; // +0x2
    float value_first; // +0x4
    float value_last; // +0x8
};
static_assert(sizeof(BodyDamageCurve) == 0xC);
}
