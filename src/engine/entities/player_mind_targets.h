#pragma once
#include "player_mind_events.h"
#include "../scripts/script_actor_query.h"
#include "../navigation/navigation.h"
#include "player_attachment_helpers.h"

namespace gbtg
{
// The retail selector retains its first local distance word when the cone
// query finds nothing. Supply that caller-stack storage instead of inventing
// a distance for the two-empty-results path.
Actor* select_runtime_player_near_actor(Actor& requester, float& distance,
    float& retained_cone_distance,
    const RuntimeScriptActorQueryContext& context); // Xbox 0x0007E600
struct RuntimePlayerMindFocus
{
    std::span<u8> mind;
    Actor& actor;
    Actor*& target; // Mind +0xC.
    const RuntimePlayerAttachmentEligibility& body;
    const std::unordered_map<Actor*, const RuntimePlayerAttachmentEligibility*>& bodies;
    const std::unordered_map<Actor*, u32>& addresses;
    const RuntimeScriptActorQueryContext& query;
    float& retained_cone_distance; // Original 0x0007E600 caller-stack word.
};
Actor* update_runtime_player_mind_focus(RuntimePlayerMindFocus& context); // Xbox 0x0007E6B0
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
struct RuntimePlayerTargetSearch
{
    Actor& actor;
    std::span<const u8> actor_bytes;
    const RuntimePlayerAttachmentEligibility& body;
    RuntimeMindTarget* first;
    std::span<const u8> address_zero;
    float& retained_angle; // Original 0x00137DE0 caller-stack word.
    float angular_limit; // DAT_004275B8.
};
u32 runtime_player_target_search_mode(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> policy); // Xbox 0x0007D770
bool runtime_player_keeps_held_target(const RuntimeMindTarget* target,
    const RuntimePlayerAttachmentEligibility& body, const Actor& origin,
    float heading, float* angle, float angular_limit); // Xbox 0x0007D7C0
RuntimeMindTarget* search_runtime_player_target_windows(RuntimePlayerTargetSearch& context,
    float heading, float scale, u32 category_mask, u32 predicate_flags,
    u32 mode, float* angle); // Xbox 0x00137DE0
RuntimeMindTarget* search_runtime_player_target_categories(RuntimePlayerTargetSearch& context,
    float heading, float* angle, u32 category_mask, u32 predicate_flags,
    u32 mode); // Xbox 0x00137D00
RuntimeMindTarget* select_runtime_player_shape_target(RuntimePlayerTargetSearch& context,
    std::span<const u8> policy, float* angle, RuntimeMindTarget* current,
    float heading); // Xbox 0x0007D6B0
bool runtime_mind_target_allowed(const RuntimeMindTarget& target,
    u32 flags); // Xbox 0x00129920
bool runtime_body_target_state_active(std::span<const u8> body); // Xbox 0x00134650
bool runtime_mind_target_requires_action(const RuntimeMindTarget& target,
    std::span<const u8> body); // Xbox 0x001413C0
bool runtime_mind_target_requires_special_action(const RuntimeMindTarget& target,
    std::span<const u8> body, const u32& first_mode,
    const u32& second_mode); // Xbox 0x0001D130
bool runtime_mind_target_occupancy_differs(const RuntimeMindTarget& target,
    u16 owner_class, const OccupancyRecord* allowed); // Xbox 0x00141760
bool runtime_mind_target_is_controlled_player(const RuntimeMindTarget& target); // Xbox 0x001414C0
bool runtime_player_mind_saved_path_clear(const Actor& actor, std::span<const u8> mind,
    const OccupancyRecord* occupancy, const OccupancyRecord* allowed,
    u16 owner_class); // Xbox 0x0013A2A0
bool runtime_mind_target_reachable(const Actor& source, const OccupancyRecord* allowed,
    const RuntimeMindTarget& target, u32 flags); // Xbox 0x001299B0
RuntimeMindTarget* find_runtime_mind_target(const Actor& source, const OccupancyRecord* allowed,
    u16 include_class, u16 exclude_class, float* distance, u32 category_mask,
    u32 predicate_flags, RuntimeMindTarget* first); // Xbox 0x00129640
struct RuntimeMindTargetScore
{
    float distance; // +4.
    float angle; // +8.
    float score; // +0xC.
    float relative_angle; // +0x10.
};
void score_runtime_mind_target(const Vec3& origin, const Vec3& position,
    float heading, float half_angle, float radius,
    RuntimeMindTargetScore& score); // Xbox 0x00129B20
RuntimeMindTarget* find_runtime_mind_target_in_angle(const Actor& source,
    const OccupancyRecord* allowed, float heading, float half_angle,
    u16 include_class, u16 exclude_class, float* angle, float* distance,
    u32 category_mask, u32 predicate_flags, float radius,
    RuntimeMindTarget* first); // Xbox 0x00129770
}
