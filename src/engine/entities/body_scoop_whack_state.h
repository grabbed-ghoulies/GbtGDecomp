#pragma once

#include "actor_clock.h"
#include "body_state_fields.h"
#include "entity_types.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x7E, entityStateScoopAndWhack (getter 0x0009EB40, vtable 0x00505AA0) over the body state (type 9):
// the haunted door's knockdown. The body holds still with its latches up while its one-shot animation plays at half
// speed; between 0.22 and 0.32 of it, a shape of the door (flags 0x20002) touching the target's bound sends the
// target message 0x4C at that point, once. Passing 0.32 restores the animation's own speed, and its end hands over
// the next action. Messages 0x41 and 0x70 run the parameters' callout (+0xC) on the actor they name. The +0x14 /
// +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace scoop_whack_state_parameter
{
constexpr RetailField<u32, 0x0C> callout{};
constexpr u32 kSize = 0x10;
static_assert(retail_fits(kSize, callout));
} // namespace scoop_whack_state_parameter
namespace scoop_whack_state_field
{
constexpr RetailField<u32, 0x18> callout{}; // parameters +0xC
constexpr RetailField<u32, 0x1C> struck{}; // message 0x4C went to the target
constexpr u32 kSize = 0x20;
static_assert(retail_fits(kSize, struck));
} // namespace scoop_whack_state_field
struct RuntimeScoopWhackStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> mind; // Actor +0x958; +0x10 the target record.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    std::function<std::span<const u8>(u32)> target_at; // A DAT_00545988 target record's retail image.
    std::function<Actor*(u32)> actor_at; // A scene actor by retail address.
    std::function<void(u32, std::span<const u32>)> message; // Vtable +0xC of the actor at an address.
    // 0x0003BE20 over the actor's records (+0x8C) with an id and the creation options.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callout;
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void scoop_whack_state_object_sizes(ObjectSizes& output); // Xbox 0x0009EB20
bool initialize_runtime_scoop_whack_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeScoopWhackStateContext& context); // Xbox 0x0009EBA0
void destroy_runtime_scoop_whack_state(RuntimeScoopWhackStateContext& context); // Xbox 0x0009EC40
void tick_runtime_scoop_whack_state(std::span<u8> state, RuntimeScoopWhackStateContext& context); // Xbox 0x0009EC90
void message_runtime_scoop_whack_state(std::span<const u8> state, std::span<const u32> message,
    RuntimeScoopWhackStateContext& context); // Xbox 0x0009EED0
// The first of the actor's shapes (flags 0x20002) whose sphere meets the target actor's bound: its centre.
bool find_runtime_scoop_whack_contact(const Actor& actor, const Actor& target,
    Vec3& point); // Xbox 0x0009EF30, EDX the target record
// Message 0x41's callout: the creation options at `point` (DAT_004E1C5C) and the actor's records for `id`.
void call_out_runtime_hit_point(const Vec3& point, u32 actor, u32 id,
    RuntimeScoopWhackStateContext& context); // Xbox 0x0013AD60, EAX point, ECX actor
// Type 0x7E (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_scoop_whack_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeScoopWhackStateContext&(u32 body_address)> context_of);
}
