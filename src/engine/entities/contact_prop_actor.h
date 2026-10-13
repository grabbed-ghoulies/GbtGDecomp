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
}
