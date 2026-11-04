#pragma once
#include "gbtg/retail_field.h"
#include <array>
#include <span>

namespace gbtg
{
// The base strategy (factory type 8, 0x38 bytes), the start of every strategy (see docs/RETAIL_LAYOUTS.md).
struct StrategyBase
{
    u32 vtable = 0; // +0, the type's vtable address
    u16 type = 0; // +4, the factory type
    u16 unk_6 = 0; // +6
    u32 actor = 0; // +8, the strategy's actor, by address
    // The states the goals move the strategy to (player_strategy_goals.cpp) are kept beside the state itself.
    u16 unk_0C = 0; // +0xC, taken once the actor's mask is clear, unless the state is 0x3F3
    // +0xE: the strategy's state: the skeleton's 0x000BFAC0 switches on it, and 0x405 is its weapon fetch.
    u16 state = 0;
    u16 unk_10 = 0; // +0x10
    u16 after_wait_state = 0; // +0x12, taken when wait_timer runs out in state 0x415
    u16 resume_state = 0; // +0x14, message 0x64 without its flag
    u16 flagged_resume_state = 0; // +0x16, message 0x64 with its flag
    // +0x18: set by derived strategies (0x3EF the worm, 0x401 and 0x405 the barons); the goals fall back to it.
    u16 unk_18 = 0;
    u8 attacking = 0; // +0x1A, 0x00028CC0 tests it; the ghoulie girl's knockdown sets it too
    // +0x1B: the haunted coat, door and picture set it while waking, the TV while generating. The camera director's
    // 0x000AA590 picks a candidate that has it.
    u8 unk_1B = 0;
    u8 pause_count = 0; // +0x1C
    std::array<u8, 3> unk_1D{}; // +0x1D, unused here
    // +0x20: the target queue link, which the host keeps as RuntimeStrategyLink (0x14 bytes in retail).
    std::array<u32, 5> link{};
    float wait_timer = 0.f; // +0x34, state 0x415's
};
static_assert(sizeof(StrategyBase) == 0x38);

// A strategy's bytes viewed as its base.
inline StrategyBase& strategy_base(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(StrategyBase));
    return *reinterpret_cast<StrategyBase*>(bytes.data());
}

inline const StrategyBase& strategy_base(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(StrategyBase));
    return *reinterpret_cast<const StrategyBase*>(bytes.data());
}
} // namespace gbtg
