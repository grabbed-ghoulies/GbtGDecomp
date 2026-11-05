#pragma once
#include "mind_fields.h"
#include "player_parent_lifecycle.h"
#include "player_mind_goals.h"
#include "actor_clock.h"

namespace gbtg
{
// A mind event (0x0002A190): what the mind is engaged with, watching or was hit by, timed. DAT_003398C8 gives each
// kind's size: kind 0 is 0x20 bytes, kinds 1 and 2 (contacts) 0x30, kinds 3 (the engaged actor) and 4 (the watched
// target record) 0x24. A view must fit the event's kind, so the kinds' fields are separate records.
struct MindEventRecord
{
    std::array<u8, 8> links{}; // +0, retail's list words; the host keeps LiveScript's
    u16 kind = 0; // +8
    u8 unk_0A = 0; // +0xA, cleared when the event is refreshed
    u8 unk_0B = 0; // +0xB, never read or written
    float time_left = 0.f; // +0xC
    float duration = 0.f; // +0x10
    float cooldown = 0.f; // +0x14, from the mind's unk_2C, counting down
    float age = 0.f; // +0x18
    float unk_1C = 0.f; // +0x1C, counts up; cleared when the event is refreshed
};
static_assert(sizeof(MindEventRecord) == 0x20);
struct MindSubjectEvent
{
    MindEventRecord base{}; // +0
    u32 subject = 0; // +0x20
};
static_assert(sizeof(MindSubjectEvent) == 0x24);
struct MindContactEvent
{
    MindEventRecord base{}; // +0
    u32 subject = 0; // +0x20
    u32 other = 0; // +0x24
    u32 count = 0; // +0x28
    u32 payload = 0; // +0x2C
};
static_assert(sizeof(MindContactEvent) == 0x30);
GBTG_MIND_VIEW(mind_event, MindEventRecord)
GBTG_MIND_VIEW(mind_subject_event, MindSubjectEvent)
GBTG_MIND_VIEW(mind_contact_event, MindContactEvent)

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
// The +0x20 word of the mind's first kind 3 event (the actor it is engaged with), or 0; 0 without a mind.
u32 runtime_mind_engaged_actor(const RuntimeMindEvents* context); // Xbox 0x00028C50, EAX actor
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
