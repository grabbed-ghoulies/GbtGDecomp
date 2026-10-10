#pragma once
#include "player_mind_targets.h"

namespace gbtg
{
struct RuntimePlayerStrategyEntry
{
    std::span<u8> strategy;
    Actor& actor; // Strategy +8 and mind +8 share actual actor owner.
    std::span<u8> actor_bytes;
    std::span<u8> body;
    RuntimeMindEvents& events;
    RuntimeMindGoalContext& goals;
    RuntimePlayerGoalRing*& ring; // Actor +0x980.
    std::span<const u8> address_zero;
    const OccupancyRecord*& allowed; // Actor +0xCA4.
    RuntimeMindTarget*& targets; // DAT_00545988's linked registry.
    const char* goal_3e9; // Actual executable string 0x0031B554.
    const char* goal_default; // 0x0031B548.
    const char* goal_3f4; // 0x0031B540.
    const char* goal_418; // 0x0031B538.
    const char* goal_target; // 0x0031C7AC.
};
void enter_runtime_player_goal(RuntimePlayerStrategyEntry& context); // Xbox 0x00138870
void enter_runtime_base_strategy_state(RuntimePlayerStrategyEntry& context,
    u16 state); // Xbox 0x0002BAF0
}
