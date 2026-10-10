#pragma once
#include "player_mind_events.h"
#include "body_action_selection.h"
#include "entity_types.h"

namespace gbtg
{
struct RuntimePlayerScriptOverrides
{
    u32& enabled; // DAT_005121E8.
    u32& value; // DAT_005121EC.
    u32& argument; // DAT_005121F0.
    u32& name; // DAT_005121F4.
    u32& player_enabled; // DAT_00512308.
    u32& player_value; // DAT_0051230C.
    u32& player_argument; // DAT_00512310.
    u32& player_name; // DAT_00512314.
};
u8 runtime_body_latch_mask(std::span<const u8> body); // Xbox 0x0001F7A0
void increment_runtime_body_latches(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first); // Xbox 0x0001FC40
void decrement_runtime_body_latches(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first); // Xbox 0x0001FD20
bool runtime_goal_requires_latches(u16 kind); // Xbox 0x000283D0
bool restore_runtime_player_script_overrides(RuntimeBodyAttachedActor& actor,
    RuntimePlayerScriptOverrides& context); // Xbox 0x00138030
struct RuntimePlayerGoalCleanup
{
    RuntimeBodyAttachedActor& actor;
    RuntimePlayerGoalRing*& ring; // Actor +0x980, heap-owned native header and row storage.
    std::span<u8>& body; // Actual actor +0x95C body; reread after notifications.
    Actor& body_owner; // Actual body +8 native owner.
    entity_detail::LatchNode*& first; // DAT_00511AA4.
    std::span<const u8> address_zero;
    RuntimePlayerScriptOverrides& overrides;
};
void remove_runtime_player_goal(RuntimePlayerGoalCleanup& context); // Xbox 0x00028240
}
