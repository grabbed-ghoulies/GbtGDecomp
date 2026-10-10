#pragma once
#include "player_children.h"

namespace gbtg
{
struct RuntimeMindGoalRow
{
    const char* name; // Resolved pointer at row +0, retail stride 0x84.
    std::span<const u8> bytes; // Resource name starts at +4.
};
struct RuntimeMindGoalTable
{
    i32 count;
    std::span<const RuntimeMindGoalRow> rows;
};
const RuntimeMindGoalRow* find_runtime_mind_goal(const RuntimeMindGoalTable& table,
    const char* name); // Xbox 0x0002A770
const RuntimeMindGoalRow* choose_runtime_mind_goal(const RuntimeMindGoalTable& table,
    const char* name, const char* wildcard, std::string& scratch,
    u32& random); // Xbox 0x0002A810, scratch aliases DAT_00508EB8.
RuntimePlayerMindRequest* allocate_runtime_mind_request(RuntimePlayerMindRequest& anchor,
    u16 kind, std::span<const u8> sizes); // Xbox 0x00029D90, anchor aliases mind +0x54.
struct RuntimeMindGoalContext
{
    const RuntimeMindGoalTable* table; // Mind +0x60.
    RuntimePlayerMindRequest& anchor;
    std::span<const u8> sizes; // DAT_00217F18, stride 8.
    const char* const* names; // Actual 49 entries at DAT_003397BC.
    std::string& scratch;
    u32& random;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
bool load_runtime_mind_goal(const char* name, RuntimeMindGoalContext& context); // Xbox 0x0002A8E0
}
