#pragma once
#include "player_avoidance.h"

namespace gbtg
{
struct RuntimeMindEscapePosition
{
    const Actor& actor;
    std::span<const u8> actor_bytes;
    std::span<const u8> target; // Actor +0x958 -> mind +0x10, or null.
    const u8* markers; // DAT_0051108C.
    const OccupancyRecord* allowed; // Actor +0xCA4.
    u32& random;
    float occupancy_initial;
};
bool select_runtime_mind_escape_position(RuntimeMindEscapePosition& context,
    float heading, float minimum, float maximum, Vec3& output); // Xbox 0x00135B60
}
