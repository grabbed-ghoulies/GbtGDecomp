#pragma once

#include "body_state_actions.h"
#include "actor_clock.h"
#include "actor_attachment_events.h"
#include "actor_render_registration.h"
#include "body_state_fields.h"

namespace gbtg
{
// Factory type 0x65 (getter 0x0008D350, vtable 0x00505780; the factory table's row 0x64 carries the name
// entityStateDroneAppear): a ghoulie appearing (+0x18 set) or vanishing over half a second. Its alpha (actor +0x19B)
// and scale (actor +0x30..+0x38) ease with the time left (0x0008D5F0, 0x0008D690); at the end an appearing ghoulie's
// bind tree is activated (0x00112CC0) and a vanishing one takes the body's next action. The +0xC, +0x14 and +0x18
// slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace drone_appear_state_parameter
{
constexpr RetailField<u32, 0x0C> appear{}; // set: the ghoulie appears; clear: it vanishes
constexpr u32 kSize = 0x10;
static_assert(retail_fits(kSize, appear));
} // namespace drone_appear_state_parameter
// The state's own fields at their retail offsets (0x28 bytes), after the body-state base (body_state_field).
namespace drone_appear_state_field
{
constexpr RetailField<u32, 0x18> appear{};
constexpr RetailField<u32, 0x1C> activated{}; // The bind tree was activated
constexpr RetailField<float, 0x20> time{}; // Seconds left, from 0.5 (DAT_0032B0F4)
constexpr RetailField<float, 0x24> scale{}; // The actor's scale (+0x38) at the start
constexpr u32 kSize = 0x28;
static_assert(retail_fits(kSize, scale));
} // namespace drone_appear_state_field
struct RuntimeDroneAppearStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32& color; // Actor +0x198; +0x19B is its alpha.
    EntityBitBlock& bits; // Body +0x15C.
    const RuntimeActorClock& clock;
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    ActorAttachmentEventContext& callouts;
    ActorRenderRegistration& render; // 0x0003E850's probes.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void drone_appear_state_object_sizes(ObjectSizes& output); // Xbox 0x0008D330
bool initialize_runtime_drone_appear_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDroneAppearStateContext& context); // Xbox 0x0008D3B0
void destroy_runtime_drone_appear_state(std::span<u8> state,
    RuntimeDroneAppearStateContext& context); // Xbox 0x0008D440
void tick_runtime_drone_appear_state(std::span<u8> state, RuntimeDroneAppearStateContext& context); // Xbox 0x0008D4D0
// The actor's alpha (+0x19B) from the time left: rising to 255 while appearing, falling while vanishing.
void fade_runtime_drone_appear(std::span<const u8> state, RuntimeDroneAppearStateContext& context); // Xbox 0x0008D5F0, EAX state, ESI actor
// The actor's scale (+0x30 / +0x38 across, +0x34 up) from the time left; a change marks the transform stale (+0x2A8).
void scale_runtime_drone_appear(std::span<const u8> state, Actor& actor); // Xbox 0x0008D690, EAX state, ECX actor
// Type 0x65 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_drone_appear_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDroneAppearStateContext&(u32 body_address)> context_of);
}
