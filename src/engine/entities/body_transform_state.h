#pragma once
#include <array>

#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "actor_render_registration.h"
#include "body_action_selection.h"
#include "body_death_effects.h"
#include "body_state_actions.h"
#include "body_state_fields.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
struct ExecutableResourcePool;
// Factory type 0x8C, entityStateTransform (getter 0x000A43A0, vtable 0x00505C60; the factory table's row 0x8B carries
// the name) over the body state (type 9): a body turns into another actor (the farmyard chicken into a vampire
// chicken). A morph actor made from the parameters' template (+0x10, 0x000A4B00) starts hidden and is shown node by
// node as the body's own animation (+0xC) plays; the body's render bins then go. Three phases follow, each with a sound
// on actor slot 1 or 4 and an animation of the morph actor (+0x110, +0x114, +0x118), the second and third after the
// 1.25 s holds. At the end the final actor is made from +0x11C at the body's pose (0x000A4BB0), told to emerge (message
// 0x65, state 0x3FD), the morph actor bursts into bits (0x0013B1F0) and a sound plays on the new actor's slot 1. A hit
// (messages 0x15, 0x18, 0x19, 0x1B, 0x1D) instead brings the body back into view and finishes it (0x00134AE0). The
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace transform_state_parameter
{
constexpr RetailField<u32, 0x0C> body_animation{}; // an XBE string address
constexpr RetailField<std::array<char, 0x80>, 0x10> morph{}; // the morph actor's template (AID)
constexpr RetailField<std::array<char, 0x80>, 0x90> bits_effect{}; // copied to +0x24 for 0x0013B1F0
constexpr RetailField<u32, 0x110> morph_animation{}; // phase 1, an XBE string address
constexpr RetailField<u32, 0x114> second_animation{}; // phase 2
constexpr RetailField<u32, 0x118> third_animation{}; // phase 3
constexpr RetailField<std::array<char, 0x80>, 0x11C> result{}; // the final actor's template (AID)
constexpr RetailField<std::array<char, 0x80>, 0x19C> first_sound{}; // slot 1 at the start
constexpr RetailField<std::array<char, 0x80>, 0x21C> second_sound{}; // slot 4 at phase 2
constexpr RetailField<std::array<char, 0x80>, 0x29C> third_sound{}; // slot 4 at phase 3
constexpr RetailField<std::array<char, 0x80>, 0x31C> result_sound{}; // the new actor's slot 1
constexpr RetailField<std::array<char, 0x80>, 0x39C> hit_sound{}; // slot 1 when a hit ends it
constexpr u32 kSize = 0x41C;
static_assert(retail_fits(kSize, hit_sound));
} // namespace transform_state_parameter
struct TransformState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18, 1 showing the morph, 2 and 3 the holds, 4 done
    u32 body_animation = 0; // +0x1C, 0x0003AAA0's record (its host half kept beside the state)
    u32 morph = 0; // +0x20, the morph actor's address, or 0
    std::array<char, 0x80> bits_effect{}; // +0x24
    u32 morph_animation = 0; // +0xA4, the morph actor's records, host halves beside the state
    u32 second_animation = 0; // +0xA8
    u32 third_animation = 0; // +0xAC
    float second_hold = 0.f; // +0xB0, seconds, at the actor's rate
    float third_hold = 0.f; // +0xB4
    u32 shown = 0; // +0xB8, every morph node is shown and the body's render bins went
    std::array<char, 0x80> result{}; // +0xBC
    std::array<char, 0x80> first_sound{}; // +0x13C
    std::array<char, 0x80> second_sound{}; // +0x1BC
    std::array<char, 0x80> third_sound{}; // +0x23C
    std::array<char, 0x80> result_sound{}; // +0x2BC
    std::array<char, 0x80> hit_sound{}; // +0x33C
};
static_assert(sizeof(TransformState) == 0x3BC);
struct RuntimeTransformStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes; // +0x1FC the marker key.
    EntityBitBlock& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    ActorRenderRegistration& render; // 0x0003CAE0 / 0x0003CBB0.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0 pairs and 0x0001AFA0.
    std::function<RuntimeBodyStateContext()> state; // 0x0001FEB0's views.
    const RuntimeActorClock& clock;
    RuntimeActorSoundQueues& sounds;
    std::span<ExecutableResourcePool> resources; // 0x00107720's templates.
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<NativeActorFactoryInstance&(u32 address)> actor_view; // The morph and new actors.
    std::function<void(u32 address, std::span<const u32> message)> message; // An object's vtable +0xC.
    std::function<u16(i16 key)> spawn_type; // 0x0011F2E0.
    std::function<const char*(u32 address)> image_string;
    std::span<u32> node_states; // DAT_005078D8.
    RuntimeBodyDeathEffects& death; // 0x00134AE0.
    u32& scene_flags; // DAT_0050EA9C.
    u32& restart_requested; // DAT_00430D0C.
    u32& random; // DAT_0054DD7C.
};
void transform_state_object_sizes(ObjectSizes& output); // Xbox 0x000A4380
bool initialize_runtime_transform_state(TransformState& state, std::span<const u8> parameters,
    RuntimeTransformStateContext& context); // Xbox 0x000A4400
void destroy_runtime_transform_state(TransformState& state, RuntimeTransformStateContext& context); // Xbox 0x000A4690
void tick_runtime_transform_state(TransformState& state, RuntimeTransformStateContext& context); // Xbox 0x000A46D0
void message_runtime_transform_state(TransformState& state, std::span<const u32> message,
    RuntimeTransformStateContext& context); // Xbox 0x000A49D0
// Type 0x8C (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_transform_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeTransformStateContext&(u32 body_address)> context_of);
}
