#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The fields every scene control (factory type 3 and its derived types) keeps at the start of its state, at their
// retail offsets (see docs/RETAIL_LAYOUTS.md). The list links at +8 (pending, then active), +0x10 (updates) and
// +0x18 (disposal) are SceneControlInstance members.
namespace scene_control_field
{
// Seconds the control has ticked. The attract, game-select, trophy-heads and Super Scary ticks (0x00084960 among
// them) each add the frame step to it first.
constexpr RetailField<float, 0x20> elapsed{};
} // namespace scene_control_field
} // namespace gbtg
