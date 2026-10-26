#pragma once

#include "actor_attachment_events.h"
#include "contact_prop_actor.h"

namespace gbtg
{
// entityAvatarMiscFurnace (factory type 0x17, getter 0x0006E1C0, vtable 0x00504CE8) over the actor (type 2): the
// shared constructor 0x000CFFD0 and the actor draw 0x0003C9E0 (through 0x0006EF20). Each frame a sphere of 50 times
// its +0x38 tells each actor it touches (players aside) {0x16, reply, sender, contacts, point}; one that answers is
// kept out of its contacts (+0x934) and gets the furnace's callout 0x16 between them (0x0006E2F0). A second sphere of
// 55 times +0x38 tells each actor it touches (the other marked actors with a model aside) {0x1B, 0, sender, 1,
// contacts, point} and keeps every one out of its other contacts (+0x938), which age out after 0.35 s.
struct RuntimeFurnaceContacts
{
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)>& contact_actor;
    const entity_detail::ListHead& players; // DAT_00511B1C.
    std::function<std::vector<RuntimeSegmentContactActor*>()> marked; // DAT_00511B38's players in list order.
    const RuntimeActorClock& clock;
    const u8* resource; // Actor +0x8C, the callout records.
    ActorAttachmentEventContext& callouts;
};
void furnace_object_sizes(ObjectSizes& sizes); // Xbox 0x0006E1A0
void release_runtime_furnace(std::span<u8> fields, GhoulySpawnPool*& burns,
    GhoulySpawnPool*& warmths); // Xbox 0x0006E210
void handle_runtime_furnace_message(std::span<u8> fields, GhoulySpawnPool* burns, GhoulySpawnPool* warmths,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings); // Xbox 0x0006E2A0
void burst_runtime_furnace_flame(const Vec3& point, Actor& object, Actor& furnace,
    RuntimeFurnaceContacts& contacts); // Xbox 0x0006E2F0, EAX point, ESI object
void burn_runtime_furnace_contacts(Actor& furnace, const Vec3& point, GhoulySpawnPool*& burns,
    RuntimeFurnaceContacts& contacts); // Xbox 0x0006E4D0
void warm_runtime_furnace_contacts(Actor& furnace, const Vec3& point, GhoulySpawnPool*& warmths,
    RuntimeFurnaceContacts& contacts); // Xbox 0x0006E6F0
void update_runtime_furnace(GhoulySpawnPool*& burns, GhoulySpawnPool*& warmths, ActorFrameState& state,
    ActorFrameContext& frame, RuntimeFurnaceContacts& contacts); // Xbox 0x0006E260
}
