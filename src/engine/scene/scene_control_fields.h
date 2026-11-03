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

// The walkabout music control (0x00080570): tracks played in a shuffled order, or the timer bank in chapter 9.
namespace walkabout_control_field
{
constexpr u32 playlist_offset = 0x24; // the shuffled track order (SceneControlInstance::playlist)
constexpr RetailField<u32, 0x28> position{}; // the next track's place in the playlist
constexpr RetailField<u32, 0x2C> count{};
constexpr RetailField<u32, 0x30> track{}; // the playing track, 0xFFFFFFFF for none
constexpr RetailField<u32, 0x34> start_countdown{}; // ticks before the first track
constexpr RetailField<u32, 0x38> plays_timer{}; // plays the timer bank instead (chapter 9)
} // namespace walkabout_control_field
} // namespace gbtg
