#pragma once

#include "entity_types.h"

#include <array>
#include <functional>

namespace gbtg
{
struct ActorLiveAttachment
{
    entity_detail::ListHead owner_link; // +0
    entity_detail::ListHead global_link; // +8
    Actor* actor = nullptr; // +0x10
    u8 mode = 0; // +0x14
    u8 hold = 0; // +0x15, held while nonzero (0x00042090 / 0x000420B0)
    u16 tag = 0; // +0x16
    u8 retained_18 = 0;
    u8 flag = 0; // +0x19
    std::array<u8, 2> retained_1a{};
    std::array<u8, 0x20> shape{}; // +0x1C
    std::array<u8, 0x20> secondary_shape{}; // +0x3C
    Vec3 origin{}; // +0x5C
};
struct ActorAttachmentSlot
{
    entity_detail::ListHead records; // +0, slot stride 0xC
    ActorLiveAttachment* owner = nullptr; // +8
};
struct ActorAttachmentReleaseMessage
{
    i32 id = 0x8D;
    ActorLiveAttachment* record;
    i32 value = 0;
};
extern std::array<ActorAttachmentSlot, 20> g_actor_attachment_slots; // DAT_0050E284
ActorLiveAttachment* allocate_actor_live_attachment(Actor& actor,
    entity_detail::ListHead& owner, entity_detail::ListHead& global,
    u8 mode, u8 flag, u16 tag, const u8* shape,
    const Vec3& origin); // Xbox 0x0012AFE0
// A native scene's walk of DAT_00511AA4 (each active actor's vtable +0xC), set while it runs.
extern std::function<void(const i32* message)> g_actor_broadcast;
void broadcast_actor_pulse_message(const i32* message); // Xbox 0x00122C70
void release_actor_attachment_slot(ActorLiveAttachment* owner); // Xbox 0x00112910
void suspend_actor_live_attachment(ActorLiveAttachment& record, bool suspend); // Xbox 0x0012B1A0
void release_actor_live_attachment(ActorLiveAttachment& record); // Xbox 0x0012B0B0
void clear_actor_live_attachments(entity_detail::ListHead& owner_head); // Xbox 0x00042040
// Each record on the actor's +0x8F4 list (actor +0x8F8 is the first) gains, or loses, a +0x15 hold.
void hold_actor_live_attachments(entity_detail::ListHead& owner_head); // Xbox 0x00042090, EAX actor
void unhold_actor_live_attachments(entity_detail::ListHead& owner_head); // Xbox 0x000420B0, EAX actor
}
