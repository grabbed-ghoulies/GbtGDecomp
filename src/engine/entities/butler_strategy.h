#pragma once
#include "strategy_fields.h"
#include "player_strategy_goals.h"
#include "strategy_target_selection.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
// The butler's strategy (factory type 0x3B, getter 0x000AB040, vtable 0x00505D70) over the base strategy (type 8).
struct ButlerStrategyState
{
    StrategyBase base;
    // +0x38: the parameter block's +0x18 string, copied to body +0x288 while scared.
    std::array<char, 0x60> scared_name{};
};
static_assert(sizeof(ButlerStrategyState) == 0x98);
struct RuntimeButlerStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and the 0x00128DF0 query.
    std::function<u32(const Actor&)> actor_address; // Retail address of an actor.
    std::function<u32(u32)> first_live_attachment_at; // Actor +0x8F8 (its target record) by retail address.
    std::function<std::span<const u8>(u32)> actor_bytes_at; // Actor bytes by retail address; +0x93C, +0xCDC.
    const char* goal_scared; // "scared", 0x00321234.
    const char* goal_far; // "far", 0x00321824.
};
void butler_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AB020
bool initialize_runtime_butler_strategy(std::span<u8> strategy, std::span<u8> actor,
    const char* name); // Xbox 0x000AB090, name is parameter +0x18
u32 find_runtime_butler_threat(RuntimeButlerStrategy& context); // Xbox 0x000AB260, EDI strategy
void update_runtime_butler_strategy(RuntimeButlerStrategy& context); // Xbox 0x000AB0C0, vtable +0x10
void dispatch_runtime_butler_message(std::span<const u32> message,
    RuntimeButlerStrategy& context); // Xbox 0x000AB170, vtable +0xC
void react_runtime_butler_hit(std::span<const u32> message, RuntimeButlerStrategy& context); // Xbox 0x000AB2D0
void enter_runtime_butler_state(u16 state, u16 previous,
    RuntimeButlerStrategy& context); // Xbox 0x000AB1A0, vtable +0x14
void leave_runtime_butler_state(u16 state, RuntimeButlerStrategy& context); // Xbox 0x000AB220, vtable +0x18
}
