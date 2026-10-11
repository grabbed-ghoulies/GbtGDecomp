#pragma once
#include "cinema_mind_messages.h"
#include "player_mind_control.h"
#include "../navigation/player_avoidance.h"

namespace gbtg
{
struct RuntimeCinemaMindAvoidance
{
    RuntimeMindTick& base;
    const RuntimePlayerGoalRing* goals;
    std::span<const u8> address_zero;
    RuntimePlayerMindRequest& request_anchor;
    std::span<const u8> request_sizes;
    i32 floor;
    const OccupancyRecord* allowed; // Actor +0xCA4.
    u32& random;
    float& occupancy_initial;
};
void update_runtime_cinema_mind_avoidance(RuntimeCinemaMindAvoidance& context); // Xbox 0x0007CAB0
struct RuntimeCinemaMindTick
{
    RuntimeCinemaMindMessages& messages;
    RuntimeCinemaMindAvoidance& avoidance;
    RuntimePlayerMindGaze& gaze;
    RuntimePlayerMindControl& control;
    RuntimeInputAxisWriteDevice& writable;
    std::span<u8> keyboard;
};
void tick_runtime_cinema_mind(RuntimeCinemaMindTick& context); // Xbox 0x0007CE30
}
