#pragma once

#include "body_state_actions.h"
#include "actor_clock.h"
#include "actor_attachment_events.h"
#include "body_attachment_cleanup.h"
#include "body_state_fields.h"
#include "player_mind_events.h"

namespace gbtg
{
// Factory type 0x66 (getter 0x0008D930, vtable 0x005057A0): the body leaps in an arc from where it stands to the front
// of its goal ring (actor +0x980), turning toward it, between an optional take-off animation and an optional landing
// one (the skeletons out of the snooker room's portraits). The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C
// is 0x0002B620. State +8 is the body.
namespace arc_state_parameter
{
constexpr RetailField<u32, 0x0C> take_off{}; // the take-off animation's name (an XBE address), or 0
constexpr RetailField<u32, 0x10> flight{}; // the flight animation's name
constexpr RetailField<u32, 0x14> landing{}; // the landing animation's name
constexpr RetailField<u16, 0x18> end_key{};
constexpr RetailField<u32, 0x1C> callout{}; // the actor's resource callout at take-off
constexpr RetailField<float, 0x20> duration{};
constexpr RetailField<u32, 0x24> keep_latches{}; // set: the landing leaves the body latches held
constexpr u32 kSize = 0x28;
static_assert(retail_fits(kSize, keep_latches));
} // namespace arc_state_parameter
// The arc state's own fields at their retail offsets (0x90 bytes), after the body-state base (body_state_field).
namespace arc_state_field
{
constexpr RetailField<u16, 0x18> end_key{}; // next_key when it ends; 0 for body slot 0x20's
constexpr RetailField<u32, 0x1C> flight{}; // set: the flight animation was found (the record is the context's)
constexpr RetailField<u32, 0x20> landing{}; // set: the landing animation was found
constexpr RetailField<float, 0x24> time{};
constexpr RetailField<float, 0x28> duration{};
constexpr RetailField<Vec3, 0x2C> start_euler{};
constexpr RetailField<Vec3, 0x38> target_euler{}; // (0, the yaw toward the target, 0)
constexpr RetailField<u32, 0x44> phase{}; // 1 take-off, 2 flight, 3 landing, 0 done
constexpr RetailField<u32, 0x48> counted{}; // holds a body +0x258 count
constexpr RetailField<u32, 0x4C> latched{}; // holds the body latches (0x0001FC40)
constexpr RetailField<u32, 0x50> keep_latches{};
constexpr RetailField<Vec3, 0x54> start{};
constexpr RetailField<Vec3, 0x60> target{};
constexpr RetailField<u32, 0x6C> events{}; // the 0x51 callout's attachment tokens: a count, then the tokens
constexpr u32 kSize = 0x90;
static_assert(retail_fits(kSize, events));
} // namespace arc_state_field
struct RuntimeArcStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    const RuntimePlayerGoalRing* ring; // Actor +0x980.
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    ActorAttachmentEventContext& callouts;
    RuntimeBodyAttachmentLists& lists;
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    std::function<u32(const void*)> address_of;
};
void arc_state_object_sizes(ObjectSizes& output); // Xbox 0x0008D910
bool initialize_runtime_arc_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeArcStateContext& context); // Xbox 0x0008D990
void destroy_runtime_arc_state(std::span<u8> state, RuntimeArcStateContext& context); // Xbox 0x0008DBF0
void message_runtime_arc_state(std::span<u8> state, std::span<const u32> message,
    RuntimeArcStateContext& context); // Xbox 0x0008DD30
void tick_runtime_arc_state(std::span<u8> state, RuntimeArcStateContext& context); // Xbox 0x0008DC70
// Type 0x66 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_arc_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcStateContext&(u32 body_address)> context_of);
}
