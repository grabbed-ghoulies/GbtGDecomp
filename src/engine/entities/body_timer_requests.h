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
    RuntimeBodyTimerRecipient*& recipient; // Body +0x760.
    FactoryObject& object;
    RuntimePlayerChildContext& child;
    const std::unordered_map<const u8*, RuntimeBodyTimerObject*>& created;
};
void request_runtime_body_hit_timer(std::span<u8> body, float duration,
    u32 mode, RuntimeBodyTimerCreation& context); // Xbox 0x00027B60
}
