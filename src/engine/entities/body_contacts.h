#pragma once
#include "actor_clock.h"
#include "body_resources.h"
#include "entity_types.h"
#include <functional>

namespace gbtg
{
// Actor owning a segment probe (probe link at actor +0x1E0).
struct RuntimeSegmentContactActor
{
    Actor& actor; // Transform (0x000412A0) and +0x1F0 collision filter (model_flags).
    u32 child_link; // Actor +0x908; nonzero refuses an override.
    const entity_detail::ListHead* body_node; // Actor +0x934 node.
    std::function<void(void*)> message; // Actual actor vtable +0xC.
};
// 0x14-byte stack record 0x0013FB50 passes to the contact callback. Byte +3 is never written.
struct RuntimeSegmentContactRecord
{
    u8 kind;
    u8 size;
    u8 flag;
    u8 unused;
    Vec3 point; // +4.
    float radius; // +0x10.
};
using RuntimeBodyContactCallback = std::function<u32(Actor& actor, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const RuntimeSegmentContactRecord& record,
    const Vec3& velocity, u16 word)>;
struct RuntimeBodyContactWalk
{
    Actor& actor; // Body +8.
    RuntimeBodyCollisionState& state; // Body +0x2C8.
    RuntimeBodyContactCallback callback;
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor;
    const entity_detail::ListHead& players; // DAT_00511B1C; actor +0x934 nodes.
    const RuntimeActorClock& clock;
};

void age_runtime_body_contact_overrides(GhoulySpawnPool& overrides,
    const RuntimeActorClock& clock); // Xbox 0x0013ED20, EAX overrides
bool expire_runtime_body_contact_overrides(GhoulySpawnPool* overrides,
    float maximum_age); // Xbox 0x0013EB60
void forget_runtime_body_contact_actor(GhoulySpawnPool* overrides,
    Actor* object); // Xbox 0x0013F110
void suspend_runtime_body_contact_overrides(GhoulySpawnPool& overrides); // Xbox 0x0013EE10, EDI overrides
void remove_runtime_body_contact_override(GhoulySpawnPool* overrides,
    Actor& object); // Xbox 0x0013EDD0
void remove_runtime_body_contact_actor(RuntimeBodyCollisionState& state,
    Actor& object); // Xbox 0x0013F720
void request_runtime_body_contact_override(RuntimeBodyCollisionState* state,
    RuntimeSegmentContactActor* object, const entity_detail::ListHead& players); // Xbox 0x0001F890
void request_runtime_body_shape_override(RuntimeBodyCollisionState* state,
    RuntimeSegmentContactActor* object, const entity_detail::ListHead& players); // Xbox 0x0001F850
void activate_runtime_body_contact_shapes(RuntimeBodyCollisionState& state,
    Actor& actor, u16 tag, u8 flags, float scale); // Xbox 0x0013F420
void deactivate_runtime_body_contact_shapes(RuntimeBodyCollisionState& state,
    u16 tag); // Xbox 0x0013F4D0
u32 insert_runtime_body_contact_override(GhoulySpawnPool*& overrides,
    RuntimeSegmentContactActor& object, u32 suspend,
    const entity_detail::ListHead& players); // Xbox 0x0013EBE0
u32 notify_runtime_body_contact(RuntimeBodyContactWalk& walk,
    const RuntimeBodyCollisionRecord& shape, float radius, const Vec3& point,
    GhoulySpawnPool* contacts, entity_detail::SegmentProbe& object,
    const Vec3& delta); // Xbox 0x0013FB50, EAX object, EBX delta
void walk_runtime_body_contacts(RuntimeBodyContactWalk& walk); // Xbox 0x0013F7B0
void update_runtime_body_contacts(RuntimeBodyContactWalk& walk); // Xbox 0x0013F2C0, EBX state

// Player contact callback (passed by 0x0002C2B0).
struct RuntimePlayerContactReport
{
    std::span<u8> body; // Actor +0x95C; contact count at +0x2CC.
    std::span<const u8> actor; // Tag at +6.
    std::span<u32> totals; // DAT_0053E528, 0x3D words per tag.
    std::span<u32> session; // DAT_00524E2C, 0x3D words per tag.
    std::function<void(void*)> message; // Actual actor vtable +0xC.
};
// Message 0x15 to the hit actor; the receiver may set the reply.
struct RuntimeContactNotice
{
    u32 id;
    u32 reply;
    Actor* sender;
    Vec3 velocity;
    GhoulySpawnPool* contacts;
    Vec3 point;
};
// Message 0x41 to the player.
struct RuntimeContactReport
{
    u32 id;
    i32 word;
    Vec3 velocity;
    RuntimeSegmentContactActor* object;
    GhoulySpawnPool* contacts;
    Vec3 point;
    u32 reply;
};
u32 report_runtime_player_contact(Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word,
    RuntimePlayerContactReport& context); // Xbox 0x0001F8C0
}
