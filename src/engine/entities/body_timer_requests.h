#pragma once
#include "body_timers.h"
#include "player_children.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeBodyTimerObject
{
    RuntimeBodyTimerRecipient& recipient;
    u32 address;
};
struct RuntimeBodyTimerCreation
{
    RuntimeBodyTimerRecipient*& recipient; // Body +0x760 (or +0x7F0).
    RuntimePlayerChildContext& child;
    // The scene keeps the object 0x00107520 made and gives its native recipient view and retail address.
    std::function<RuntimeBodyTimerObject&(FactoryObject)> keep;
};
void request_runtime_body_hit_timer(std::span<u8> body, float duration,
    u32 mode, RuntimeBodyTimerCreation& context); // Xbox 0x00027B60
// The hold timer (body +0x7E4..+0x7F4): `timer.recipient` is the native view of body +0x7F0.
struct RuntimeBodyHoldTimerContext
{
    RuntimeBodyTimerCreation& timer;
    Actor& owner; // Body +8.
    entity_detail::LatchNode*& first; // DAT_00511AA4.
};
void hold_runtime_body_latches(std::span<u8> body, RuntimeBodyHoldTimerContext& context); // Xbox 0x00027BE0
}
