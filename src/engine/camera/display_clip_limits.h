#pragma once
#include "gbtg/entity.h"
#include <array>

namespace gbtg
{
struct DisplayClipBounds
{
    u8 active; // DAT_0050EA00.
    std::array<Vec3, 8> corners; // DAT_0050EA1C, stride 12.
};
void extend_display_clip_limits(std::span<const float, 12> transform,
    const DisplayClipBounds& bounds, float& far_clip, float& near_clip); // Xbox 0x0013C490
}
