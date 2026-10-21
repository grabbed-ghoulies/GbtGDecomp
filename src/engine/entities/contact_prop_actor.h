#pragma once

#include "actor_frame_update.h"
#include "actor_word_messages.h"
#include "body_contacts.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
struct RuntimePropContactNotice
{
    u32 id;
    u32 reply;
    Actor* sender;
    u32 value;
    GhoulySpawnPool* contacts;
    Vec3 point;
};
struct RuntimeActorShapeContacts
{
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)>& contact_actor;
    const entity_detail::ListHead& players;
};
i32 collect_runtime_actor_shape_contacts(Actor& actor, GhoulySpawnPool*& overrides,
    RuntimeActorShapeContacts& context); // Xbox 0x0013EEB0
void runtime_contact_prop_sizes(ObjectSizes& sizes); // Xbox 0x0006EE00
bool initialize_runtime_contact_prop(std::span<u8> fields, Actor& actor); // Xbox 0x0006EE70
void release_runtime_contact_prop(std::span<u8> fields,
    GhoulySpawnPool*& overrides); // Xbox 0x0006EEA0
void handle_runtime_contact_prop_message(std::span<u8> fields, GhoulySpawnPool* overrides,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings); // Xbox 0x0006EF30
void update_runtime_contact_prop(std::span<u8> fields, GhoulySpawnPool*& overrides,
    ActorFrameState& state, ActorFrameContext& frame, const RuntimeActorClock& clock,
    RuntimeActorShapeContacts& contacts); // Xbox 0x0006EED0

// entityAvatarMiscKnockDownSphere (factory type 0x19, getter 0x0006EB40, vtable 0x00504D60) over the actor (type 2):
// once message 1 switches it on, every frame it tells each actor its sphere (radius +0x938, the parameters' +0x334)
// touches {0x17, reply, sender, contacts, point}, and keeps the ones that answer out of its own contacts (+0x934).
// Its destructor is the contact prop's 0x0006EEA0.
struct RuntimeKnockdownSphereNotice
{
    u32 id; // 0x17.
    u32 reply;
    Actor* sender;
    GhoulySpawnPool* contacts; // +0xC
    Vec3 point; // +0x10, the sphere's world position.
};
struct RuntimeKnockdownSphereContacts
{
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)>& contact_actor;
    const entity_detail::ListHead& players; // DAT_00511B1C.
    std::function<std::vector<RuntimeSegmentContactActor*>()> marked; // DAT_00511B38's players in list order.
};
void runtime_knockdown_sphere_sizes(ObjectSizes& sizes); // Xbox 0x0006EB20
bool initialize_runtime_knockdown_sphere(std::span<u8> fields, std::span<const u8> parameters); // Xbox 0x0006EB80
void handle_runtime_knockdown_sphere_message(std::span<u8> fields, GhoulySpawnPool* overrides,
    std::span<u32> message, const std::function<Actor*(u32)>& actor_at, ActorBaseContext& base,
    const RuntimeActorWordBindings& bindings); // Xbox 0x0006EBD0
void push_runtime_knockdown_sphere(Actor& actor, std::span<const u8> fields, GhoulySpawnPool*& overrides,
    const RuntimeActorClock& clock, RuntimeKnockdownSphereContacts& contacts); // Xbox 0x0006EC10, EAX actor
void update_runtime_knockdown_sphere(std::span<u8> fields, GhoulySpawnPool*& overrides,
    ActorFrameState& state, ActorFrameContext& frame, const RuntimeActorClock& clock,
    RuntimeKnockdownSphereContacts& contacts); // Xbox 0x0006EBA0
}
