#pragma once
#include "player_navigation.h"
#include "spawn_candidates.h"

namespace gbtg
{
struct RuntimeNavigationHeadingSearch
{
    const Vec3& origin;
    i32 floor;
    float minimum;
    float maximum;
    const OccupancyRecord* required;
    const OccupancyRecord* allowed;
    Vec3* output;
    u32 keep_sample;
    u8 flags;
    i16 group;
    u32& random;
    float occupancy_initial; // Original caller's retained occupancy distance.
};
bool probe_runtime_navigation_heading(RuntimeNavigationHeadingSearch& context,
    float heading); // Xbox 0x0013AC80
bool search_runtime_navigation_headings(RuntimeNavigationHeadingSearch& context,
    float heading, float increment, i32 attempts); // Xbox 0x0013AA50
float randomize_runtime_navigation_heading(float heading, float width,
    u32& random); // Xbox 0x00100C70
struct RuntimePlayerNavigationAvoidance
{
    std::span<u8> mind;
    std::span<const u8> body;
    const Actor& actor;
    const i32& floor;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    RuntimeInputAxisWriteDevice& writable;
    std::span<u8> keyboard;
    u32& random;
    float& occupancy_initial;
    float& retained_direction; // 0x00105F90 preserves output for a zero vector.
};
bool avoid_runtime_player_navigation(RuntimePlayerNavigationAvoidance& context); // Xbox 0x0007F980
}
