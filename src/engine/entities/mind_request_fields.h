#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The camera's request kinds, sized by DAT_00217F18. Kind 1 (0x2A4 bytes, 0x000781F0): follow the mind's target, with
// up to 20 target shapes (0x20 bytes each, MindTargetShape), filled by the director (0x000AA250).
namespace camera_target_request
{
constexpr size_t shapes = 0x14;
constexpr RetailField<u32, 0x294> shape_count{};
constexpr RetailField<u32, 0x298> fixed_heading{}; // set: head for `heading` rather than choosing
constexpr RetailField<float, 0x29C> heading{};
constexpr RetailField<u32, 0x2A0> sets_goal_point{}; // set while a scene start is pending: the placed position becomes the mind's goal point
} // namespace camera_target_request
// Kind 3 (0x18 bytes, 0x00079B80): follow an actor by address.
namespace camera_follow_request
{
constexpr RetailField<u32, 0x10> target{};
} // namespace camera_follow_request

// A mind request (a mind's queued step: its kind at +8 picks how the rest is read). The cinema mind runs them in
// 0x0007C0E0; each kind's own fields follow the header. See docs/RETAIL_LAYOUTS.md.
namespace mind_request_field
{
constexpr RetailField<u16, 0x08> kind{};
constexpr RetailField<u8, 0x0A> started{}; // raised once the request is initialized
constexpr RetailField<float, 0x0C> elapsed{}; // seconds the request has run
} // namespace mind_request_field

// Kind 2 (0x0007B090): dispatch the action as a target request when forced or the target isn't held.
namespace target_action_request
{
constexpr RetailField<u32, 0x10> action{};
constexpr RetailField<u32, 0x14> forced{};
} // namespace target_action_request

// Kind 0xD (0x0007B170): walk away from the escape target until it is beyond safe_distance, picking a new point
// between the two distances every two seconds (0x00135B60).
namespace escape_request
{
constexpr RetailField<float, 0x10> min_distance{};
constexpr RetailField<float, 0x14> max_distance{};
constexpr RetailField<float, 0x18> safe_low{}; // safe_distance's range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x1C> safe_high{};
constexpr RetailField<float, 0x20> speed{};
constexpr RetailField<float, 0x28> safe_distance{};
constexpr RetailField<float, 0x2C> heading{}; // written: the heading the last point was picked on
constexpr RetailField<float, 0x30> retry_timer{};
} // namespace escape_request

// Kind 0xE (0x0007B2F0): the escape for a body that moves in height too (0x00136D60's height and tolerance).
namespace vertical_escape_request
{
constexpr RetailField<float, 0x10> height{};
constexpr RetailField<float, 0x14> tolerance{};
constexpr RetailField<float, 0x18> min_distance{};
constexpr RetailField<float, 0x1C> max_distance{};
constexpr RetailField<float, 0x20> safe_low{}; // safe_distance's range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x24> safe_high{};
constexpr RetailField<float, 0x28> speed{};
constexpr RetailField<float, 0x2C> vertical_speed{};
constexpr RetailField<float, 0x34> safe_distance{};
constexpr RetailField<float, 0x38> heading{};
constexpr RetailField<float, 0x3C> retry_timer{};
} // namespace vertical_escape_request

// Kind 0xF (0x0007B5A0): go to the mind's goal point in height too.
namespace vertical_position_request
{
constexpr RetailField<float, 0x10> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x14> speed_high{};
constexpr RetailField<float, 0x18> height{};
constexpr RetailField<float, 0x1C> tolerance{};
constexpr RetailField<float, 0x20> radius{}; // plus 1
constexpr RetailField<float, 0x24> approach{};
constexpr RetailField<float, 0x28> vertical_speed{};
constexpr RetailField<float, 0x2C> speed{};
} // namespace vertical_position_request

// Kind 0x10 (0x00136680): go to the request's own point in height too.
namespace vertical_point_request
{
constexpr RetailField<Vec3, 0x10> point{};
constexpr RetailField<float, 0x1C> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x20> speed_high{};
constexpr RetailField<float, 0x24> height{};
constexpr RetailField<float, 0x28> tolerance{};
constexpr RetailField<float, 0x2C> radius{}; // plus 2.5
constexpr RetailField<float, 0x30> approach{};
constexpr RetailField<float, 0x34> vertical_speed{};
constexpr RetailField<float, 0x38> speed{};
} // namespace vertical_point_request

// Kind 0x11 (0x0007B620): go to the mind's target in height too.
namespace vertical_target_request
{
constexpr RetailField<float, 0x10> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x14> speed_high{};
constexpr RetailField<float, 0x18> height{};
constexpr RetailField<float, 0x1C> tolerance{};
constexpr RetailField<float, 0x20> radius{}; // added to the target's radius, plus 1
constexpr RetailField<float, 0x24> approach{};
constexpr RetailField<float, 0x28> vertical_speed{};
constexpr RetailField<float, 0x2C> lead{}; // from behind the target, aim this far along its facing
constexpr RetailField<u32, 0x30> waits_for_idle{}; // arrival counts once the body is idle
constexpr RetailField<u32, 0x34> needs_movable{}; // fails while the target can't be moved
constexpr RetailField<float, 0x38> speed{};
} // namespace vertical_target_request

// Kind 0x14 (0x0007B8A0): pick a goal point between two distances (0x00111420).
namespace random_position_request
{
constexpr RetailField<float, 0x10> min_distance{};
constexpr RetailField<float, 0x14> max_distance{};
} // namespace random_position_request

// Kind 0x15 (0x0007B940): pick a goal point on a heading near the actor's, trying headings `step` apart.
namespace heading_position_request
{
constexpr RetailField<float, 0x10> spread{}; // the heading's random width (0x00100C70)
constexpr RetailField<float, 0x14> min_distance{};
constexpr RetailField<float, 0x18> max_distance{}; // plus 0.5
constexpr RetailField<float, 0x1C> step{};
} // namespace heading_position_request

// Kind 0x24 (0x0007BA20): move on a heading for a time, steered or pushed straight.
namespace timed_motion_request
{
constexpr RetailField<float, 0x10> heading_offset{}; // degrees, from the target or (with own_heading) the actor's facing
constexpr RetailField<float, 0x14> heading_spread{}; // degrees either side, drawn at the start
constexpr RetailField<float, 0x18> duration{}; // against the header's elapsed
constexpr RetailField<float, 0x1C> speed{}; // of the body's step speed
constexpr RetailField<u32, 0x20> own_heading{}; // set: starts from the actor's facing, not the target
constexpr RetailField<u32, 0x24> direct{}; // set: the stick is pushed on the heading, no navigation
constexpr RetailField<float, 0x28> heading{};
} // namespace timed_motion_request

// Kinds 0x25..0x27 (0x0007C0E0): face the goal point (0x25), a point (0x26) or the target (0x27) within an angle.
namespace facing_request
{
constexpr RetailField<float, 0x10> degrees{}; // kinds 0x25 and 0x27
constexpr RetailField<Vec3, 0x10> point{}; // kind 0x26
constexpr RetailField<float, 0x1C> point_degrees{}; // kind 0x26
} // namespace facing_request

// Kind 0x2D (0x0007BC20): go to the mind's goal point.
namespace position_request
{
constexpr RetailField<float, 0x10> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x14> speed_high{};
constexpr RetailField<float, 0x18> radius{}; // plus 1
constexpr RetailField<float, 0x1C> approach{};
constexpr RetailField<float, 0x20> speed{};
} // namespace position_request

// Kind 0x2E (0x00136620): go to the request's own point.
namespace navigation_request
{
constexpr RetailField<Vec3, 0x10> point{};
constexpr RetailField<float, 0x1C> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x20> speed_high{};
constexpr RetailField<float, 0x24> radius{}; // plus 2.5
constexpr RetailField<float, 0x28> approach{};
constexpr RetailField<float, 0x2C> speed{};
} // namespace navigation_request

// Kind 0x2F (0x0007BC80): go to the mind's target.
namespace target_position_request
{
constexpr RetailField<float, 0x10> speed_low{}; // `speed`'s range, drawn at the start (0x0007C580)
constexpr RetailField<float, 0x14> speed_high{};
constexpr RetailField<float, 0x18> radius{}; // added to the reach and the target's radius, plus 1
constexpr RetailField<float, 0x1C> approach{};
constexpr RetailField<float, 0x20> lead{}; // from behind the target, aim this far along its facing
constexpr RetailField<u32, 0x24> waits_for_idle{};
constexpr RetailField<u32, 0x28> needs_movable{};
constexpr RetailField<float, 0x2C> speed{};
} // namespace target_position_request

// The parent mind's requests (0x000295E0). Kinds 5, 6, 0x1B, 0x1C, 0x1D, 0x1F, 0x21 and 0x22 name a goal,
// callout, action or flag at +0x10.
namespace named_request
{
// The shortest request holding a name is 0x44 bytes, so 0x30 is all a name can be sure of.
constexpr RetailField<std::array<char, 0x30>, 0x10> name{};
} // namespace named_request

// Kind 9 passes with a chance; 0xA when the mind's last result differs from `value`; 0xC sets body bit 7 to it;
// 0x1A scales the actor; 0x2B tests the body's idle mode.
namespace value_request
{
constexpr RetailField<float, 0x10> chance{};
constexpr RetailField<u32, 0x10> value{};
constexpr RetailField<float, 0x10> scale{};
} // namespace value_request

// Kind 0xB: wait until the target is within `radius` (plus the body's radius, or with `uses_target_radius` the
// pair's).
namespace reach_request
{
constexpr RetailField<float, 0x10> radius{};
constexpr RetailField<u32, 0x14> uses_target_radius{};
} // namespace reach_request

// Kind 0x1E: the body's unk_70 drawn between low and high.
namespace body_range_request
{
constexpr RetailField<float, 0x10> low{};
constexpr RetailField<float, 0x14> high{};
} // namespace body_range_request

// Kind 0x20: an action once an animation slot completes.
namespace slot_action_request
{
constexpr RetailField<u32, 0x10> slot{};
constexpr RetailField<std::array<char, 0x30>, 0x14> action{};
} // namespace slot_action_request

// Kinds 0x21 and 0x22: an action while a body flag matches `expected` (0x22 with a chance too); otherwise the
// request waits when `waits` is set.
namespace flag_action_request
{
constexpr RetailField<std::array<char, 0x40>, 0x10> action{};
constexpr RetailField<std::array<char, 0x30>, 0x50> flag{};
constexpr RetailField<u32, 0x80> expected{};
constexpr RetailField<u32, 0x84> waits{}; // kind 0x21
constexpr RetailField<float, 0x84> chance{}; // kind 0x22
constexpr RetailField<u32, 0x88> chance_waits{}; // kind 0x22
} // namespace flag_action_request

// Kind 8 (0x0007C0E0): a body flag's test, then a chance.
namespace chance_flag_request
{
constexpr RetailField<std::array<char, 0x30>, 0x10> flag{};
constexpr RetailField<float, 0x40> chance{};
} // namespace chance_flag_request

// Kind 0x29: wait out a duration (against the header's elapsed).
namespace wait_request
{
constexpr RetailField<float, 0x10> duration_low{}; // `duration`'s range, drawn at the start (0x00029EC0, 0x0007C580)
constexpr RetailField<float, 0x14> duration_high{};
constexpr RetailField<float, 0x18> duration{};
} // namespace wait_request

// Kind 0x2A: test a named body flag against `expected`.
namespace flag_test_request
{
constexpr RetailField<std::array<char, 0x30>, 0x10> flag{};
constexpr RetailField<u32, 0x40> expected{};
} // namespace flag_test_request
} // namespace gbtg
