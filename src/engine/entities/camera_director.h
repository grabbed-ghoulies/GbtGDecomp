#pragma once
#include "mind_fields.h"
#include "player_children.h"
#include "player_strategy_goals.h"
#include "../frontend/dialogue_input.h"
#include <functional>
#include <vector>

namespace gbtg
{
// One actor the director is considering (0x2C bytes, on the stack in 0x000AA590).
struct CameraDirectorEntry
{
    u32 actor = 0; // +0, by address; 0 for a group's centre
    i32 priority = 0; // +4, the highest sort first
    float distance = 0.f; // +8, the nearest sort first
    MindTargetShape shape{}; // +0xC
};
static_assert(sizeof(CameraDirectorEntry) == 0x2C);

// One player actor as the camera director reads it through its +0x934 node on DAT_00511B38.
struct RuntimeDirectorActor
{
    u32 address = 0; // Retail actor address (node - 0x934).
    Actor* actor = nullptr;
    std::span<const u8> bytes; // Actor bytes: +0x960 input, +0x98C, +0xCEC floor.
    std::span<const u8> strategy; // Actor +0x954 object; empty when null.
    std::span<const u8> mind; // Actor +0x958 object; empty when null.
    std::span<const u8> body; // Actor +0x95C object.
    const EntityBitBlock* body_bits = nullptr; // Body +0x15C.
};
// The director tunables at DAT_00428708..DAT_00428730 and the near range DAT_0043400C.
struct RuntimeDirectorTunables
{
    const u32& keep_state; // DAT_00428708: nonzero skips the 0x000AA480 watcher checks.
    const i32& minimum_count; // DAT_0042870C.
    const i32& minimum_priority; // DAT_00428710.
    const u32& ground_heights; // DAT_00428714.
    const float& range_extension; // DAT_00428718.
    const float& lead_distance; // DAT_0042871C.
    const u32& focus_targets; // DAT_00428720.
    const u32& carried_targets; // DAT_00428724.
    const i32& region_mode; // DAT_00428728.
    const u32& skip_candidates; // DAT_0042872C.
    const u32& input_heading; // DAT_00428730.
    const float& near_range; // DAT_0043400C.
};
struct RuntimeDirectorTargets
{
    const RuntimeDirectorTunables& tunables;
    const u32& runtime_flags; // DAT_0050EA9C.
    std::function<std::vector<RuntimeDirectorActor>()> marks; // DAT_00511B38 nodes in list order.
    std::function<RuntimeDirectorActor(u32)> actor_at; // The actor at a retail address; raises when absent.
    std::function<void(std::span<u8, 0x20>, u32)> camera_target; // 0x0013D000 with EAX the source actor.
    std::function<const RuntimeDialogueInputDevice*(u32)> input_device; // Native view of an actor +0x960 word.
    const RuntimeDialogueInputState& input;
};
bool runtime_actor_strategy_waiting(const RuntimeDirectorActor& actor); // Xbox 0x00028CC0, ECX actor
u32 camera_target_watched_elsewhere(u32 self, u32 target,
    const RuntimeDirectorTargets& context); // Xbox 0x000AA480, ESI self, EDX target
CameraDirectorEntry* reserve_camera_director_entry(CameraDirectorEntry* entries, i32& count, u32 actor, i32 priority,
    i32 limit); // Xbox 0x000AA4D0, EAX entries, EDI count, EBX actor
u32 add_camera_director_entry(CameraDirectorEntry* entries, i32& count, u32 actor, i32 priority, i32 limit, float distance,
    const RuntimeDirectorTargets& context); // Xbox 0x000AA520, EAX priority, ECX limit, EDX actor, EDI count
int compare_camera_director_entries(const void* left, const void* right); // Xbox 0x000AA560
i32 collect_camera_director_targets(u32 camera, u32 target, std::span<u8> output, i32 limit,
    const RuntimeDirectorTargets& context); // Xbox 0x000AA590
float camera_director_input_heading(const RuntimeDirectorActor& actor,
    const RuntimeDirectorTargets& context); // Xbox 0x0013DD80
u32 read_camera_director_input_heading(const RuntimeDirectorActor& actor, float& heading,
    const RuntimeDirectorTargets& context); // Xbox 0x000AADE0, EDI actor
bool camera_director_scene_start_pending(); // Xbox 0x0013D1E0
// Body +0x258 counts holds; the last release sends the body message 0x4A.
void release_runtime_body_hold(std::span<u8> body,
    const std::function<void(std::span<const u32>)>& message); // Xbox 0x00020000, EAX body
struct RuntimeDirectorUpdate
{
    FactoryObject& strategy; // +8 actor, +0xE state, +0x38, +0x3C, +0x40.
    Actor& actor; // Strategy +8.
    u32 actor_address;
    std::span<u8> mind; // Actor +0x958; +0xC target.
    std::span<u8> body; // Actor +0x95C; +0x258 hold count.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
    RuntimePlayerStrategyGoals& goals; // 0x0002C130.
    RuntimePlayerStrategySetup& setup; // 0x0002B8D0.
    RuntimePlayerMindRequest& requests; // Mind +0x54 anchor for 0x00029D90.
    std::span<const u8> request_sizes; // DAT_00217F18.
    RuntimeDirectorTargets& targets;
};
void update_runtime_director_strategy(RuntimeDirectorUpdate& context); // Xbox 0x000AA250
}
