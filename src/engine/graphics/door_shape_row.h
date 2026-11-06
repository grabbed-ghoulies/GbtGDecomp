#pragma once

#include "gbtg/types.h"
#include <array>

namespace gbtg
{
// One door shape, DAT_00424994.
inline constexpr u32 kDoorShapes = 0x00424994;
struct DoorShapeRow
{
    float x; // +0x0, also its shadow's centre.
    float z; // +0x4, also its shadow's depth.
    float near_opacity; // +0x8, the shadow's.
    float far_opacity; // +0xC
    std::array<char, 0x84> breakable_model; // +0x10, what 0x0013B1F0 bursts the door into.
};
static_assert(sizeof(DoorShapeRow) == 0x94);
}
