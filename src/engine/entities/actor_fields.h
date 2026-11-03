#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The actor base's (type 2, 0x934 bytes) raw fields that the native Actor doesn't keep. See docs/RETAIL_LAYOUTS.md.
namespace actor_field
{
constexpr RetailField<u16, 0x194> bone{}; // 0: the animation moves the root bone (0x0002C2B0)
constexpr RetailField<Vec3, 0x318> eased{}; // eased a tenth of the way toward `ease_target` each tick when easing
constexpr RetailField<Vec3, 0x324> ease_target{};
constexpr RetailField<float, 0x924> age{}; // seconds at the actor's rate
constexpr RetailField<u32, 0x928> ticks{};
constexpr u32 kSize = 0x934;
} // namespace actor_field
} // namespace gbtg
