#pragma once

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_hit_state.h"
#include "body_state_fields.h"
#include "player_attachment.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x6B, entityStateFlip (getter 0x00090430, vtable 0x00505840) over the body state (type 9): the body
// is thrown into the air. It drops what it holds, its anchor rises 115 and its gravity (+0x324) becomes -200 while
// latch 0 is held; in the air the stick steers it at the parameters' speed (+0x10). Once it is back on the ground it
// plays the parameters' landing animation (+0xC) and recovers (0x00133F50) when that has played. The message and the
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace flip_state_parameter
{
constexpr RetailField<u32, 0x0C> landing{}; // the landing animation's name, an XBE string
constexpr RetailField<float, 0x10> steer_speed{};
constexpr u32 kSize = 0x14;
static_assert(retail_fits(kSize, steer_speed));
} // namespace flip_state_parameter
namespace flip_state_field
{
constexpr RetailField<u32, 0x18> phase{}; // 1 in the air, 2 landing
constexpr RetailField<float, 0x1C> gravity{}; // body +0x324 before the flip
constexpr RetailField<float, 0x20> steer_speed{}; // parameters +0x10
constexpr RetailField<u32, 0x24> airborne{}; // the body has left the ground (body +0x15C bit 0x29 clear)
constexpr RetailField<u32, 0x28> latched{}; // latch 0 is held
constexpr RetailField<u32, 0x2C> landing{}; // set when the landing animation's record was found
constexpr u32 kSize = 0x30;
static_assert(retail_fits(kSize, landing));
} // namespace flip_state_field
struct RuntimeFlipStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    EntityBitBlock& bits; // Body +0x15C.
    RuntimePlayerAttachment& attachment; // 0x0001B810's views.
    RuntimeBodyActionSelection& actions; // 0x0001AED0's lookup and 0x0001AFA0's start.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const RuntimeActorClock& clock;
    const ActorAnimationState& animation; // Actor +0x50.
    const RuntimeDialogueInputDevice* input; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState& input_state;
    const Vec3& default_position; // DAT_0050DD80.
    RuntimeBodyRecoveryAction& recovery; // 0x00133F50's views.
    std::function<const char*(u32)> image_string; // An XBE string, or null for 0.
};
void flip_state_object_sizes(ObjectSizes& output); // Xbox 0x00090410
bool initialize_runtime_flip_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeFlipStateContext& context); // Xbox 0x00090490
void destroy_runtime_flip_state(std::span<u8> state, RuntimeFlipStateContext& context); // Xbox 0x00090650
void tick_runtime_flip_state(std::span<u8> state, RuntimeFlipStateContext& context); // Xbox 0x00090760
// In the air the stick (when the body is settled) sets the body's motion (+0x318 / +0x320) at the steer speed.
void steer_runtime_flip_state(std::span<const u8> state, RuntimeFlipStateContext& context); // Xbox 0x000908D0, ECX input, ESI body
// Type 0x6B (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_flip_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFlipStateContext&(u32 body_address)> context_of);
}
