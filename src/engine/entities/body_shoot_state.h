#pragma once

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_state_fields.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x81, entityStateShoot (getter 0x0009F6E0, vtable 0x00505B00) over the body state (type 9): the body
// aims the gun it holds (body +0x48) at the mind's target (mind +0x14) and fires it. Phase 1 plays the aim
// animations (+0x28, +0x2C) with controller slot 2 weighted by the aim's pitch between the parameters' limits; once
// the gun's muzzle faces the target it is told 0xA2 (0x0009FF90), and while the right stick stays out the spray's
// strength follows it (0xA1). Letting go ends the state; an empty gun plays the parameters' sound (+0x24) first. A
// gun that answers 0x9A goes to phase 2 instead (+0x30, +0x34), which waits for the animation and aims again while
// the mind still has an attacker. The message and the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
namespace shoot_state_parameter
{
// The four animations' names (XBE addresses): the aim pair and the second phase's pair.
constexpr RetailField<u32, 0x0C> aim{};
constexpr RetailField<u32, 0x10> aim_blend{};
constexpr RetailField<u32, 0x14> second{};
constexpr RetailField<u32, 0x18> second_blend{};
constexpr RetailField<float, 0x1C> lowest_pitch{}; // degrees
constexpr RetailField<float, 0x20> highest_pitch{}; // degrees
constexpr RetailField<std::array<char, 0x80>, 0x24> empty_sound{};
constexpr u32 kSize = 0xA4;
static_assert(retail_fits(kSize, empty_sound));
} // namespace shoot_state_parameter
namespace shoot_state_field
{
constexpr RetailField<u32, 0x18> phase{}; // 1 aiming and firing, 2 the second phase
constexpr RetailField<u32, 0x1C> answered{}; // the gun answered message 0x9A
constexpr RetailField<u32, 0x20> firing{}; // the gun was told 0xA2
constexpr RetailField<float, 0x24> timer{}; // 0.35 s (DAT_0032B354) from phase 1's start
// The four animations (0x0003AAA0), in the parameters' order; set when the record was found.
constexpr RetailField<u32, 0x28> aim{};
constexpr RetailField<u32, 0x2C> aim_blend{};
constexpr RetailField<u32, 0x30> second{};
constexpr RetailField<u32, 0x34> second_blend{};
constexpr RetailField<float, 0x38> lowest_pitch{}; // radians
constexpr RetailField<float, 0x3C> highest_pitch{};
constexpr RetailField<float, 0x40> weight{}; // slot 2's weight, following +0x44 at 2 a second
constexpr RetailField<float, 0x44> aimed_weight{}; // the target's pitch between the limits, 0..1
constexpr RetailField<u32, 0x48> aimed{}; // set once +0x44 was taken from a target (0x000A0160)
constexpr RetailField<std::array<char, 0x80>, 0x4C> empty_sound{};
constexpr RetailField<u32, 0xCC> last_target{}; // the target record of the last tick
constexpr u32 kSize = 0xD0;
static_assert(retail_fits(kSize, last_target));
} // namespace shoot_state_field
struct RuntimeShootStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    EntityBitBlock& bits; // Body +0x15C.
    const RuntimeActorClock& clock;
    std::span<const u8> mind; // Actor +0x958; +0x14 the target record.
    std::function<std::span<const u8>(u32)> target_at; // A DAT_00545988 target record's retail image.
    std::span<const u8> hit; // Body +0x84's record (0x0001D540), or empty.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0's lookup and 0x0001AFA0's start.
    std::function<const char*(u32)> image_string; // An XBE string, or null for 0.
    u32 weapon; // Body +0x48, the held gun's address, or 0.
    std::function<void(u32, std::span<const u32>)> message; // Vtable +0xC of the actor at an address; replies come back.
    std::function<std::span<const u8>(u32)> bytes_at; // An actor's instance bytes (the gun's +0x93C).
    std::function<u32()> attacker; // 0x00028C50: the mind's kind 3 event's actor, or 0.
    const RuntimeDialogueInputDevice* input; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState& input_state;
    RuntimeActorSoundQueues& sounds;
    std::function<u16()> next_key; // Body vtable +0x20.
};
void shoot_state_object_sizes(ObjectSizes& output); // Xbox 0x0009F6C0
bool initialize_runtime_shoot_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeShootStateContext& context); // Xbox 0x0009F740
void destroy_runtime_shoot_state(std::span<u8> state, RuntimeShootStateContext& context); // Xbox 0x0009F9B0
void tick_runtime_shoot_state(std::span<u8> state, RuntimeShootStateContext& context); // Xbox 0x0009FA80
// Leaving phase 1 tells the gun 0xA3; phase 1 or 2 starts its animation pair with slot 2 at +0x40.
void set_runtime_shoot_phase(std::span<u8> state, u32 phase, RuntimeShootStateContext& context); // Xbox 0x0009FED0, EDI state
// Once the gun's muzzle (0x9F) faces `point` (within 30 degrees, or the target is near and the aim settled), the gun
// is told {0xA2, target, point}. Whether it is firing.
bool fire_runtime_shoot_state(std::span<u8> state, u32 target, float radius, const Vec3& point,
    RuntimeShootStateContext& context); // Xbox 0x0009FF90, EDI state, ESI point
// Slot 2's weight follows the target's pitch from the body's eye height.
void aim_runtime_shoot_state(std::span<u8> state, std::span<const u8> target,
    RuntimeShootStateContext& context); // Xbox 0x000A0160, EAX target, ECX actor, EBX state
// The turn toward the target, relative to the body's hit heading.
Vec3 runtime_shoot_turn(std::span<const u8> target, RuntimeShootStateContext& context); // Xbox 0x000A0320, EAX target, EDX body
// The right stick's strength: its length less 0.4, by 5/3, clamped to 0..1.
float runtime_input_stick_strength(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state); // Xbox 0x00140A60, EAX device
// Type 0x81 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_shoot_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeShootStateContext&(u32 body_address)> context_of);
}
