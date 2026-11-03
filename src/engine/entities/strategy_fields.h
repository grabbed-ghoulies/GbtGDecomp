#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The base strategy's (factory type 8) fields its derived strategies read, at their retail offsets (see
// docs/RETAIL_LAYOUTS.md).
namespace strategy_field
{
// The states the goals move the strategy to (player_strategy_goals.cpp) are kept beside the state itself.
constexpr RetailField<u16, 0x0C> unk_0C{}; // taken once the actor's mask is clear, unless the state is 0x3F3
// The strategy's state: the skeleton's 0x000BFAC0 switches on it, and 0x405 is its weapon fetch.
constexpr RetailField<u16, 0x0E> state{};
constexpr RetailField<u16, 0x10> unk_10{};
constexpr RetailField<u16, 0x12> after_wait_state{}; // taken when wait_timer runs out in state 0x415
constexpr RetailField<u16, 0x14> resume_state{}; // message 0x64 without its flag
constexpr RetailField<u16, 0x16> flagged_resume_state{}; // message 0x64 with its flag
// Set by derived strategies (0x3EF the worm, 0x401 and 0x405 the barons); the goals fall back to it.
constexpr RetailField<u16, 0x18> unk_18{};
constexpr RetailField<float, 0x34> wait_timer{}; // state 0x415's
} // namespace strategy_field
} // namespace gbtg
