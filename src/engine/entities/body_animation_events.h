#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The body's own animation events (0x0001A8F0 runs them by kind; animation_event_field is the header). See
// actor_event_messages.h for the kinds every actor runs.

// Kinds 2 (the body's own, 0x0001AAF0) and 0xB (the attached actor's, 0x0001A290): contact shapes on while the event runs.
namespace contact_shape_event
{
constexpr RetailField<u16, 0x10> tag{};
constexpr RetailField<u16, 0x12> flags{}; // the low byte is used
constexpr RetailField<float, 0x14> scale{};
} // namespace contact_shape_event

// Kind 9 (0x000199F0): a push toward the mind's target.
namespace target_push_event
{
constexpr RetailField<i32, 0x10> speed{};
} // namespace target_push_event

// Kind 0xA (0x00019A50): a push on the actor's facing.
namespace facing_push_event
{
constexpr RetailField<i32, 0x10> speed{};
constexpr RetailField<float, 0x14> degrees{}; // from the facing
} // namespace facing_push_event

// Kind 0x14 (0x0001A690): which attached actors to let go.
namespace detach_event
{
constexpr RetailField<u32, 0x10> primary{};
constexpr RetailField<u32, 0x14> secondary{};
} // namespace detach_event

// Kind 0x18 (0x0001ADB0): message 0x37 to the actors within a radius.
namespace proximity_event
{
constexpr RetailField<float, 0x10> radius{};
} // namespace proximity_event

// Kind 0x1A (0x0001A750): the secondary actor is sent to a point ahead.
namespace teleport_event
{
constexpr RetailField<float, 0x10> pitch{};
constexpr RetailField<float, 0x14> distance{};
} // namespace teleport_event
} // namespace gbtg
