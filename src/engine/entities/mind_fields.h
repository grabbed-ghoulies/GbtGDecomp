#pragma once
#include "gbtg/retail_field.h"
#include "gbtg/types.h"

namespace gbtg
{
// The base mind's fields (0x7C bytes from 0x00029D10), shared by every mind type: the player, cinema and camera
// minds and the strategies' minds. Derived minds' own fields start at +0x7C (see docs/RETAIL_LAYOUTS.md).
namespace mind_field
{
// The mind's own actor: the parameters' +0x9C (0x00029F20). Messages naming it (0x0007C8B0) and the camera's yaw
// (0x000784B0) read it.
constexpr RetailField<u32, 0x08> actor{};
// The actor the mind targets; the camera rig (0x000781F0) and the cinema tick (0x0007CE30) follow it, and
// messages naming it clear it (0x0002A460).
constexpr RetailField<u32, 0x0C> target{};
// The target's record in the target pool (DAT_00545988) or a mind's own +0xB8 record; 0x00029C80 tests it.
constexpr RetailField<u32, 0x10> target_record{};
// The record the current action is aimed at: 0x000295E0 copies it from +0x10, 0x00136830 sets it from the
// request selection, and messages naming it clear it (0x0002A2E0).
constexpr RetailField<u32, 0x14> action_record{};
// The actor the mind acts on: the weapon the look-for-weapon and pick-up states take (0x0009DB30, 0x0009DF90),
// the nearest spawn 0x000295E0 picks, the cinema mind's movement goal (0x0007BF60). Messages naming it clear it
// (0x0002A460).
constexpr RetailField<u32, 0x18> action_actor{};
constexpr RetailField<u32, 0x1C> path_search{}; // The path search block 0x00136700 allocates.
// The target mode beside +0xC: 0x0007E6B0 sets 0 or 1 with the target, the cinema tick (0x0007CE30) 1 while the
// target is within its radius.
constexpr RetailField<u8, 0x20> target_mode{};
// Copied into each new or refreshed event's +0x14 (0x0002A190, 0x0002A280): a random value between the
// parameters' +0x88 and +0x8C (0x00029F20).
constexpr RetailField<float, 0x2C> unk_2C{};
// The model anchor the gaze follows. Message 0x85 naming it clears it with +0x34 and +0x38 (0x0002A2E0).
constexpr RetailField<u32, 0x30> gaze_anchor{};
// Every mind tick adds the frame step to +0x34 (0x0002A9F0, 0x0007FB10, 0x0007CE30); a change of +0x20 raises it
// to +0x38 (0x0007E6B0).
constexpr RetailField<float, 0x34> unk_34{};
constexpr RetailField<float, 0x38> unk_38{};
// A point the mind is told to look at: set by the ghoul intro (0x000F4560) and the jump-on state (0x00099AD0);
// 0x00137140 tests +0x3C.
constexpr RetailField<u32, 0x3C> gaze_point_set{};
constexpr RetailField<Vec3, 0x40> gaze_point{};
// The alert mode 0x0002A650 switches on (0x40 from 0x00029F20, up to 0x44); it sets body +0x158.
constexpr RetailField<u32, 0x4C> alert_mode{};
constexpr RetailField<float, 0x50> alert_timer{}; // Counts down in 0x0002A650; messages add to it (0x0007C8B0).
// The last finished request's result (0x0002AA60); request kind 0xA (0x000295E0) compares it.
constexpr RetailField<u32, 0x5C> last_request_result{};
// The target search's flags, categories and range: the parameters' +0x90, +0x94 and +0x98, the range replaced by
// the actor class's row when it has one (0x00029A60); 0x00029AC0 and 0x00029C80 read them.
constexpr RetailField<u32, 0x64> target_flags{};
constexpr RetailField<u32, 0x68> target_categories{};
constexpr RetailField<float, 0x6C> target_range{};
// The point the mind moves toward: the strategy goals' candidate (0x0013A0F0), the camera's eye (0x000781F0).
constexpr RetailField<Vec3, 0x70> goal_point{};
constexpr u32 kSize = 0x7C;
static_assert(retail_fits(kSize, goal_point));
} // namespace mind_field
} // namespace gbtg
