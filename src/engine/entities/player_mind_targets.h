#pragma once
#include "player_mind_events.h"
#include "../scripts/script_actor_query.h"
#include "../navigation/navigation.h"

namespace gbtg
{
struct RuntimeMindTarget
{
    RuntimeMindTarget* next; // Link at +8, next at +0xC; DAT_00545988 points to link.
    std::span<const u8> bytes;
    u32 address;
    Actor& actor; // +0x10.
    std::span<const u8> actor_bytes;
    RuntimeScriptMindQuery& body; // Actor +0x95C.
    const OccupancyRecord* occupancy; // Actor +0xCA8.
};
bool runtime_mind_target_allowed(const RuntimeMindTarget& target,
    u32 flags); // Xbox 0x00129920
bool runtime_mind_target_reachable(const Actor& source, const OccupancyRecord* allowed,
    const RuntimeMindTarget& target, u32 flags); // Xbox 0x001299B0
RuntimeMindTarget* find_runtime_mind_target(const Actor& source, const OccupancyRecord* allowed,
    u16 include_class, u16 exclude_class, float* distance, u32 category_mask,
    u32 predicate_flags, RuntimeMindTarget* first); // Xbox 0x00129640
}
