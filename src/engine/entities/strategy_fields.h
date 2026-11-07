#pragma once
#include "gbtg/retail_field.h"
#include <array>
#include <span>
#include <type_traits>

namespace gbtg
{
// The base strategy's factory parameters (0x0002B740).
namespace strategy_parameter
{
constexpr RetailField<u32, 0x08> actor{};
constexpr RetailField<u16, 0x0C> unk_0C{};
constexpr RetailField<u16, 0x0E> unk_0E{}; // copied to the strategy's unk_10
constexpr RetailField<u16, 0x10> after_wait_state{};
constexpr RetailField<u16, 0x12> resume_state{};
constexpr RetailField<u16, 0x14> flagged_resume_state{};
constexpr RetailField<u16, 0x16> unk_16{}; // copied to the strategy's unk_18
} // namespace strategy_parameter

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

// A strategy's bytes viewed as its struct.
template <typename State>
State& strategy_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<State*>(bytes.data());
}

template <typename State>
const State& strategy_view(std::span<const u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<const State*>(bytes.data());
}

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

// A strategy whose first word is the weapon it carries or goes for: the pirate's chest, the baron's cane. 0x000BEB80
// reads it for both.
struct CarriedWeaponStrategy
{
    StrategyBase base;
    u32 weapon = 0; // +0x38, by address
};
static_assert(sizeof(CarriedWeaponStrategy) == 0x3C);

// The player strategy (factory type 0x4F, 0x54 bytes).
struct PlayerStrategyState
{
    StrategyBase base;
    // +0x38: set on leaving state 0x3FB with a move hold added; cleared, with the hold, once body bit 6.0 is set. The
    // input device is reset while it stays set.
    u32 move_held = 0;
    u32 unk_3C = 0; // +0x3C, set by messages 0x2C and 0x82; state 0x408 clears it once the body's timer is idle
    u32 brew_wanted = 0; // +0x40, message 0x7D with the brew prompt available; state 0x408 moves to 0x3F1
    u32 unk_44 = 0; // +0x44, nothing here reads it
    u32 pause_held = 0; // +0x48, state 0x3FC holds DAT_00510CEC up, and leaving it or the destructor gives it back
    float idle_time = 0.f; // +0x4C, seconds idle; 0x000BF600 counts up, and 30 is enough
    float brew_timer = 0.f; // +0x50, state 0x3F1's prompt, 5 s
};
static_assert(sizeof(PlayerStrategyState) == 0x54);
} // namespace gbtg
