#pragma once
#include "gbtg/retail_field.h"

#include <array>
#include <span>
#include <type_traits>

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

// The start of every scene control: type 3's 0x24-byte instance under the factory's header (0x00107520, 0x00107700).
struct SceneControlBase
{
    u32 vtable = 0; // +0, the type's vtable address
    u16 type = 0; // +4, the factory type
    u16 unk_6 = 0; // +6, the parameters' +2
    // +8 (pending, then active), +0x10 (updates) and +0x18 (disposal): list links, which SceneControlInstance keeps.
    std::array<u32, 6> links{};
    float elapsed = 0.f; // +0x20, seconds the control has ticked
};
static_assert(sizeof(SceneControlBase) == 0x24);
static_assert(scene_control_field::elapsed.offset == 0x20);

// A scene control's bytes viewed as its struct.
template <typename State>
State& scene_control_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<State*>(bytes.data());
}

// The walkabout music control (0x00080570): tracks played in a shuffled order, or the timer bank in chapter 9.
struct WalkaboutControlState
{
    SceneControlBase base{}; // +0
    u32 unk_24 = 0; // +0x24, retail's shuffled track order, which SceneControlInstance::playlist keeps
    u32 position = 0; // +0x28, the next track's place in the playlist
    u32 count = 0; // +0x2C
    u32 track = 0; // +0x30, the playing track, 0xFFFFFFFF for none
    u32 start_countdown = 0; // +0x34, ticks before the first track
    u32 plays_timer = 0; // +0x38, plays the timer bank instead (chapter 9)
};
static_assert(sizeof(WalkaboutControlState) == 0x3C);
} // namespace gbtg
