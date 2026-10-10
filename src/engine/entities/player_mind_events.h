#pragma once
#include "player_parent_lifecycle.h"
#include "player_mind_goals.h"

namespace gbtg
{
struct RuntimeMindEvent : LiveScript
{
    std::span<u8> bytes;
};
struct RuntimeMindEvents
{
    LiveScript& anchor; // Embedded mind +0x24; next aliases mind +0x28.
    std::span<u8> mind;
    std::span<const u8> descriptors; // DAT_003398C8, stride 12.
    std::function<void(std::span<const u32>)> actor_message; // Actual actor vtable +0xC.
};
RuntimeMindEvent* allocate_runtime_mind_event(RuntimeMindEvents& context,
    u16 kind, float duration); // Xbox 0x0002A190
RuntimeMindEvent* refresh_runtime_mind_event(RuntimeMindEvents& context,
    RuntimeMindEvent& event); // Xbox 0x0002A280
void refresh_runtime_mind_target_event(RuntimeMindEvents& context); // Xbox 0x0002A0C0
void set_runtime_mind_target(RuntimeMindEvents& context, u32 target); // Xbox 0x0002A030
RuntimePlayerMindRequest* append_runtime_position_request(RuntimePlayerMindRequest& anchor,
    std::span<const u8> sizes, std::span<const u8, 12> position); // Xbox 0x00136A70
struct RuntimePlayerGoalRing
{
    u16 stride;
    u16 capacity;
    u16 head;
    u16 tail;
    std::span<const u8> rows;
};
u32 runtime_player_goal_flags(const RuntimePlayerGoalRing* ring,
    std::span<const u8> address_zero); // Xbox 0x000283A0
bool runtime_direction_angle(const Vec3& direction, float& angle); // Xbox 0x00105F90
}
