#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The fields every body state (the states a body runs, such as the hit, walk-target and throw states) keeps at the
// start of its object, at their retail offsets (see docs/RETAIL_LAYOUTS.md). +8 is the state's body, a native view.
namespace body_state_field
{
// The key the state hands its body next: the states' vtable +0x1C (0x00091F70 among them) returns it, and the
// states store their parameters' keys into it when they end.
constexpr RetailField<u16, 0x0C> next_key{};
// Copied with next_key as one word (the hold state 0x000A3520); nothing reads it alone.
constexpr RetailField<u16, 0x0E> unk_E{};
// Seconds the state has run: each state's tick adds the actor's step (0x000A6520 among them).
constexpr RetailField<float, 0x10> elapsed{};
} // namespace body_state_field
} // namespace gbtg
