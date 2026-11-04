#pragma once
#include <array>

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
// The arc state's own fields at their retail offsets (0x90 bytes), after the body-state base (BodyStateBase).
struct ArcState
{
    BodyStateBase base{}; // +0
    u16 end_key = 0; // +0x18, next_key when it ends; 0 for body slot 0x20's
    std::array<u8, 2> unk_1A{}; // +0x1A, padding
    u32 flight = 0; // +0x1C, set: the flight animation was found (the record is the context's)
    u32 landing = 0; // +0x20, set: the landing animation was found
    float time = 0.f; // +0x24
    float duration = 0.f; // +0x28
    Vec3 start_euler{}; // +0x2C
    Vec3 target_euler{}; // +0x38, (0, the yaw toward the target, 0)
    u32 phase = 0; // +0x44, 1 take-off, 2 flight, 3 landing, 0 done
    u32 counted = 0; // +0x48, holds a body +0x258 count
    u32 latched = 0; // +0x4C, holds the body latches (0x0001FC40)
    u32 keep_latches = 0; // +0x50
    Vec3 start{}; // +0x54
    Vec3 target{}; // +0x60
    std::array<u8, 36> events{}; // +0x6C, the 0x51 callout's attachment tokens: a count, then the tokens
};
static_assert(sizeof(ArcState) == 0x90);
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
bool initialize_runtime_arc_state(ArcState& state, std::span<const u8> parameters,
    RuntimeArcStateContext& context); // Xbox 0x0008D990
void destroy_runtime_arc_state(ArcState& state, RuntimeArcStateContext& context); // Xbox 0x0008DBF0
void message_runtime_arc_state(ArcState& state, std::span<const u32> message,
    RuntimeArcStateContext& context); // Xbox 0x0008DD30
void tick_runtime_arc_state(ArcState& state, RuntimeArcStateContext& context); // Xbox 0x0008DC70
// Type 0x66 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_arc_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcStateContext&(u32 body_address)> context_of);
}
