#pragma once

#include "body_contacts.h"
#include "body_knockdown_state.h"
#include "body_state_actions.h"

namespace gbtg
{
struct ActorRenderRegistration;
struct NativeActorFactoryInstance;
struct RuntimeActorSoundQueues;

// Factory type 0x68 entityStateExplode (getter 0x0008E760, vtable 0x005057E0; the factory table's row 0x67 carries the
// name) over the body state (type 9): a body blows up (the worm). Its message, +0x14 and +0x18 slots are the empty
// 0x001628B0 and +0x1C is 0x0002B620; the parameters (0xC bytes) are not read. Once its animation slot 0 is done the
// actor is hidden and taken out of collision with callout 0x17 (phase 1); for 0.3 seconds the blast, shrinking from
// 25 to 8, sends each actor in reach message 0x21 and, unanswered, a contact (phase 2); once its sounds end it dies
// (0x00134AE0). State +8 is the body.
namespace explode_state_field
{
constexpr RetailField<float, 0x10> elapsed{}; // Seconds at the actor's rate; nothing reads it back.
constexpr RetailField<u32, 0x18> phase{}; // 0 waiting on the animation, 1 the blast, 2 waiting on the sounds
constexpr RetailField<float, 0x1C> blast_time{}; // DAT_0032B268 (0.3) on the blast's start, down to 0
constexpr RetailField<u32, 0x20> overrides{}; // The blasted actors' contact overrides (0x0013EBE0), or 0
constexpr u32 kSize = 0x28;
static_assert(retail_fits(kSize, overrides));
} // namespace explode_state_field
struct RuntimeExplodeStateContext
{
    RuntimeKnockdownContext& knockdown; // Body, actor, bits, animation, clock, callouts and death views.
    RuntimeBodyStateContext& state; // 0x00136540's views.
    NativeActorFactoryInstance& actor_view; // 0x001364B0 / 0x00136540.
    ActorRenderRegistration& render; // 0x0003E850.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const RuntimeActorSoundQueues& sounds; // 0x00041CB0.
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor; // Probe - 0x1E0.
    const entity_detail::ListHead& players; // DAT_00511B1C.
    // 0x0001F8C0 with this body's contact report (message 0x41 to the actor, the DAT_0053E528 / DAT_00524E2C tallies).
    std::function<u32(Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
        const Vec3& velocity, u16 word)> report;
};
void explode_state_object_sizes(ObjectSizes& output); // Xbox 0x0008E740
bool initialize_runtime_explode_state(std::span<u8> state,
    RuntimeExplodeStateContext& context); // Xbox 0x0008E7C0
void destroy_runtime_explode_state(std::span<u8> state,
    RuntimeExplodeStateContext& context); // Xbox 0x0008E930
void tick_runtime_explode_state(std::span<u8> state,
    RuntimeExplodeStateContext& context); // Xbox 0x0008EA90
// The actor goes: hidden, out of collision, callout 0x17; the blast runs 0.3 seconds.
void start_runtime_explode_blast(std::span<u8> state,
    RuntimeExplodeStateContext& context); // Xbox 0x0008EB80, EAX actor
// One frame of the blast around the actor's middle; true once its time has run out.
bool blast_runtime_explode_contacts(std::span<u8> state, Actor& actor,
    RuntimeExplodeStateContext& context); // Xbox 0x0008EBD0, EAX body
// Type 0x68 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_explode_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeExplodeStateContext&(u32 body_address)> context_of);
}
