#pragma once
#include <array>

#include "actor_clock.h"
#include "actor_trail_requests.h"
#include "body_action_selection.h"
#include "body_contacts.h"
#include "body_damage_tables.h"
#include "body_state_fields.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x83, entityStateSlide (getter 0x000A0C30, vtable 0x00505B40) over the body state (type 9): the
// body's actor is pushed along its heading (0x000A10D0) and plays the slide animation; the animation's event bits
// start the slide callout (bit 2, id 0x4D) and its contact shapes (bit 3), and when it ends the body holds for 0.2 s,
// plays the recovery animation and hands over its next action. Phase 1 the slide, 2 the hold, 3 the recovery; each
// phase swaps body +0x334 (kept at +0x330) and raises body +0x338. The sizes are 0x000A0C10's (0x3C / 0x1C). State +8
// is the body.
namespace slide_state_parameter
{
constexpr RetailField<u16, 0x0C> shape_tag{}; // the contact shapes' tag
constexpr RetailField<u32, 0x10> event{}; // the callout id messages 0x41 and 0x70 send
constexpr RetailField<u32, 0x14> slide{}; // the slide animation's name (an XBE address)
constexpr RetailField<u32, 0x18> recover{}; // and the recovery's
constexpr u32 kSize = 0x1C;
static_assert(retail_fits(kSize, recover));
} // namespace slide_state_parameter
struct SlideState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18, 1 the slide, 2 the hold, 3 the recovery
    u32 slide_animation = 0; // +0x1C, 0x0003AAA0's record; set when it was found
    u32 recover_animation = 0; // +0x20
    u16 shape_tag = 0; // +0x24
    std::array<u8, 2> unk_26{}; // +0x26, padding
    u32 sounding = 0; // +0x28, set by the slide callout, cleared when actor +0x614 is released
    u32 shapes_active = 0; // +0x2C
    u32 event = 0; // +0x30
    float hold_timer = 0.f; // +0x34, 0.2 s (DAT_0032B1AC)
    float trail_timer = 0.f; // +0x38, 0x0013B030's clock while the shapes are live
};
static_assert(sizeof(SlideState) == 0x3C);
struct RuntimeSlideStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes;
    RuntimeBodyActionSelection& actions; // 0x0003AAA0 / 0x0003A9E0's views.
    const RuntimeActorClock& clock;
    EntityBitBlock& bits; // Body +0x15C.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeActorSoundQueues& sounds;
    ActorTrailContext trails;
    const RuntimeBodyDamageTables& damage_tables; // 0x000259C0's tables.
    std::span<const float> mode_scales; // DAT_00217EC0.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    // 0x0003BE20 over the +0x8C callout records of the actor at an address, callback 0.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> callouts;
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void slide_state_object_sizes(ObjectSizes& output); // Xbox 0x000A0C10
bool initialize_runtime_slide_state(SlideState& state, std::span<const u8> parameters,
    RuntimeSlideStateContext& context); // Xbox 0x000A0C90
void destroy_runtime_slide_state(SlideState& state, RuntimeSlideStateContext& context); // Xbox 0x000A0E80
void tick_runtime_slide_state(SlideState& state, RuntimeSlideStateContext& context); // Xbox 0x000A0F90
void message_runtime_slide_state(const SlideState& state, std::span<const u8> message,
    RuntimeSlideStateContext& context); // Xbox 0x000A1070
// Type 0x83 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_slide_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSlideStateContext&(u32 body_address)> context_of);
}
