#pragma once

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_state_fields.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x7B, entityStateLookForWeapon (getter 0x0009D8C0, vtable 0x00505A40) over the body state (type 9): the
// body stands puzzled with a question mark over its head (the parameters' +0xC sprite at the model's kind 4 locator)
// for a random wait between +0x18C and +0x190 s. If by then the mind has a weapon to go for (mind +0x18: a usable one
// it already chose, else the nearest 0x00129D70 finds), a light bulb comes on (+0x10C and +0x8C sprites, the
// GMISC_LIGHTBULBON sound) and the body plays the parameters' animation (+0x194) turned toward the weapon; the
// state ends with that animation, or at once when no weapon was found. The message and the +0x14 / +0x18 slots are the
// empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace look_for_weapon_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> puzzled{}; // the question mark's sprite (AID)
constexpr RetailField<std::array<char, 0x80>, 0x8C> bulb{}; // the light bulb's sprites (AIDs)
constexpr RetailField<std::array<char, 0x80>, 0x10C> glow{};
constexpr RetailField<float, 0x18C> shortest_wait{};
constexpr RetailField<float, 0x190> longest_wait{};
constexpr RetailField<u32, 0x194> found_animation{}; // an XBE string
constexpr u32 kSize = 0x198;
static_assert(retail_fits(kSize, found_animation));
} // namespace look_for_weapon_state_parameter
namespace look_for_weapon_state_field
{
constexpr RetailField<u32, 0x18> found{}; // 0 searching, 1 a weapon was found
constexpr RetailField<u32, 0x1C> joint{}; // the kind 4 locator's joint, or -1
constexpr RetailField<float, 0x20> wait{};
constexpr RetailField<u32, 0x24> found_animation{}; // set when the record was found
// The three sprites (0x00035E90), set when loaded; the resources stay on the host.
constexpr RetailField<u32, 0x28> puzzled{};
constexpr RetailField<u32, 0x2C> bulb{};
constexpr RetailField<u32, 0x30> glow{};
constexpr u32 kSize = 0x34;
static_assert(retail_fits(kSize, glow));
} // namespace look_for_weapon_state_field
struct RuntimeLookForWeaponStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    const RuntimeActorClock& clock;
    std::span<u8> mind; // Actor +0x958; +0x18 the weapon it goes for.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0's lookup and 0x0001AFA0's start.
    std::function<const char*(u32)> image_string; // An XBE string, or null for 0.
    AnimationResourceRegistry& registry; // 0x00035E90 / 0x00035C00.
    ResourceErrorState& error; // 0x00032700.
    RuntimeActorSoundQueues& sounds;
    u32& random; // DAT_0054DD7C.
    std::function<Actor*(u32)> actor_at; // A scene actor by retail address.
    std::function<std::span<const u8>(u32)> bytes_at; // A scene actor's instance bytes (a weapon's +0x93C).
    std::function<u32(const Actor&)> address_of; // A scene actor's retail address.
    std::function<u16()> next_key; // Body vtable +0x20.
};
void look_for_weapon_state_object_sizes(ObjectSizes& output); // Xbox 0x0009D8A0
bool initialize_runtime_look_for_weapon_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeLookForWeaponStateContext& context); // Xbox 0x0009D920
void destroy_runtime_look_for_weapon_state(std::span<u8> state,
    RuntimeLookForWeaponStateContext& context); // Xbox 0x0009DAE0
void tick_runtime_look_for_weapon_state(std::span<u8> state,
    RuntimeLookForWeaponStateContext& context); // Xbox 0x0009DB30
// Whether the mind has a weapon to go for: its usable choice (mind +0x18), else the nearest (kept at mind +0x18).
bool find_runtime_mind_weapon(RuntimeLookForWeaponStateContext& context); // Xbox 0x0009DC70, EAX actor
// The question mark, or the bulb and its glow, above the locator.
void draw_runtime_look_for_weapon_state(std::span<const u8> state,
    RuntimeLookForWeaponStateContext& context); // Xbox 0x0009DCD0, EAX actor, ESI state
// The body turns to the actor at `target` while it is free to turn.
void face_runtime_body_actor(std::span<u8> body, const Actor& actor, const Actor& target); // Xbox 0x001345B0, EBX body, ESI target
// Type 0x7B (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_look_for_weapon_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeLookForWeaponStateContext&(u32 body_address)> context_of);
}
