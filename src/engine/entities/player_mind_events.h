#pragma once
#include "player_parent_lifecycle.h"
#include "player_mind_goals.h"
#include "actor_clock.h"

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
void record_runtime_mind_contact(RuntimeMindEvents& context, u32 first, u32 second,
    u32 payload); // Xbox 0x0002A3E0
void clear_runtime_mind_actor_events(RuntimeMindEvents& context,
    u32 actor); // Xbox 0x0002A460
void end_runtime_mind_target(RuntimeMindEvents& context, std::span<const u32> message,
    std::span<const u8> target); // Xbox 0x0002A5A0
void dispatch_runtime_parent_mind_message(RuntimeMindEvents& context,
    std::span<const u32> message, std::span<const u8> target); // Xbox 0x0002A2E0
struct RuntimeMindTick
{
    RuntimeMindEvents& events;
    RuntimePlayerMindRequests& requests;
    Actor& actor;
    Actor& body_actor; // Mind +8 -> body +8.
    std::span<u8> body;
    const RuntimeActorClock& clock;
    std::function<i32(RuntimePlayerMindRequest&)> initialize_request; // Mind vtable +0x18.
    std::function<i32(RuntimePlayerMindRequest&)> update_request; // Mind vtable +0x14.
};
void update_runtime_mind_requests(RuntimeMindTick& context); // Xbox 0x0002AA60
void update_runtime_mind_events(RuntimeMindTick& context); // Xbox 0x0002AB50
u32 runtime_mind_alert_level(float timer); // Xbox 0x0002A740
void update_runtime_mind_alert(RuntimeMindTick& context); // Xbox 0x0002A650
void tick_runtime_parent_mind(RuntimeMindTick& context); // Xbox 0x0002A9F0
RuntimePlayerMindRequest* append_runtime_position_request(RuntimePlayerMindRequest& anchor,
    std::span<const u8> sizes, std::span<const u8, 12> position); // Xbox 0x00136A70
RuntimePlayerMindRequest* append_runtime_range_request(RuntimePlayerMindRequest& anchor,
    std::span<const u8> sizes, float range); // Xbox 0x00136AC0, EAX mind
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
