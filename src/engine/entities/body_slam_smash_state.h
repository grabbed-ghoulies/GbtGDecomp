#pragma once

#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "body_action_selection.h"
#include "body_contacts.h"
#include "body_death_effects.h"
#include "body_state_actions.h"
#include "body_state_fields.h"
#include "../graphics/actor_effect_stream.h"

#include <functional>

namespace gbtg
{
// Factory type 0x82, entityStateSlamSmashAttack (getter 0x000A03D0, vtable 0x00505B20; the factory table's row 0x81
// carries the name) over the body state (type 9): a leap that lands as a slam. A take-off animation plays; then the
// body picks its landing point (its mind's target, else 40 ahead) and crouches for half a second; it leaps up 200
// over the point for 0.7 s (callout 0x3F where it left), drops onto it in half a second at scale 2 with its contact
// shapes live, and once down runs the parameters' "aid_" effect where it stands, callout 0x4C and the body's death
// effect. The message and the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the
// body.
namespace slam_smash_state_parameter
{
// The three animations' names (XBE addresses): take-off, crouch and drop.
constexpr RetailField<u32, 0x0C> take_off_animation{};
constexpr RetailField<u32, 0x10> crouch_animation{};
constexpr RetailField<u32, 0x14> drop_animation{};
constexpr RetailField<u16, 0x18> shape_tag{}; // the contact shapes the drop activates
constexpr RetailField<float, 0x1C> shape_scale{};
constexpr RetailField<std::array<char, 0x80>, 0x20> effect{}; // run on landing when it begins "aid_"
constexpr u32 kSize = 0xA0;
static_assert(retail_fits(kSize, effect));
} // namespace slam_smash_state_parameter
namespace slam_smash_state_field
{
constexpr RetailField<u32, 0x18> phase{}; // 1 take-off, 2 crouch, 3 leap, 4 drop, 5 landed
constexpr RetailField<float, 0x1C> timer{}; // the phase's time left
// Three animation records (0x0003AAA0); set when the record was found.
constexpr RetailField<u32, 0x20> take_off_animation{};
constexpr RetailField<u32, 0x24> crouch_animation{};
constexpr RetailField<u32, 0x28> drop_animation{};
constexpr RetailField<u16, 0x2C> shape_tag{};
constexpr RetailField<float, 0x30> shape_scale{};
constexpr RetailField<u32, 0x34> shapes_active{}; // set while the drop's contact shapes are live
constexpr RetailField<u32, 0x38> paused{}; // set while slot 0's animation is held (player +0x125)
constexpr RetailField<u32, 0x3C> shape_mode{}; // 0x0001FEB0's previous mode, restored by the destructor
constexpr RetailField<Vec3, 0x40> start{}; // the actor's world position at entry
constexpr RetailField<Vec3, 0x4C> landing{}; // the target's position, or 40 ahead, on its floor's cell bottom
constexpr RetailField<std::array<char, 0x80>, 0x58> effect{};
constexpr u32 kSize = 0xD8;
static_assert(retail_fits(kSize, effect));
} // namespace slam_smash_state_field
struct RuntimeSlamSmashStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes; // +0xCEC the actor's floor.
    u32 actor_address;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    RuntimeBodyStateContext& shape; // 0x0001FEB0's views.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    NativeActorFactoryInstance& view; // The actor's segment probes (0x0003E850) and shape probe (0x0003C700).
    std::span<const u8> mind; // Actor +0x958, or empty.
    std::function<std::span<const u8>(u32 address)> target_at; // A mind +0x10 target record's retail image.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    ActorEffectStreamContext& effects; // 0x00053170's views.
    RuntimeBodyDeathEffects& death; // 0x00024890's views.
    // 0x0003BE20 over the actor's records (+0x8C) with an id and the creation options.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callout;
    std::function<void(std::span<const u32>)> body_message; // The body's own vtable +0xC.
};
void slam_smash_state_object_sizes(ObjectSizes& output); // Xbox 0x000A03B0
bool initialize_runtime_slam_smash_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSlamSmashStateContext& context); // Xbox 0x000A0430
void destroy_runtime_slam_smash_state(std::span<u8> state, RuntimeSlamSmashStateContext& context); // Xbox 0x000A05A0
void tick_runtime_slam_smash_state(std::span<u8> state, RuntimeSlamSmashStateContext& context); // Xbox 0x000A0630
// Type 0x82 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_slam_smash_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSlamSmashStateContext&(u32 body_address)> context_of);
}
