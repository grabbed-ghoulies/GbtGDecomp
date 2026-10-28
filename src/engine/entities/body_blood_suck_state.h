#pragma once

#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "body_action_selection.h"
#include "body_damage_dispatch.h"
#include "body_jump_on_state.h"
#include "body_state_fields.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
struct Game;
// Factory type 0x61, entityStateBloodSuck (getter 0x0008B200, vtable 0x00505700; the factory table's row 0x60 carries
// the name) over the body state (type 9): the vampire's bite. Phase (+0x4C) 1 reaches for the mind's target: once its
// grab point (the parameters' shape) meets the target's bound, the target accepts message 0x3C and is pulled in over
// the grab animation; 2 and 3 hold it and drain its energy one a second while its pad shakes it free (2 bites, 3
// struggles); 4 throws it off with the parameters' hit; 5 ends with the body's next action. +0x14 is 0x0008B9C0
// (the target held in front), +0x18 the empty 0x001628B0 and +0x1C 0x0002B620. State +8 is the body.
namespace blood_suck_state_parameter
{
constexpr RetailField<u32, 0x0C> grab_callout{}; // the callout id at the grab point, on the target
constexpr RetailField<u16, 0x10> grab_shape{}; // the actor's shape (mask 0x20002) whose sphere grabs
// The four animations' names (XBE addresses): reach, bite, struggle and throw.
constexpr RetailField<u32, 0x14> reach_animation{};
constexpr RetailField<u32, 0x18> bite_animation{};
constexpr RetailField<u32, 0x1C> struggle_animation{};
constexpr RetailField<u32, 0x20> throw_animation{};
constexpr RetailField<std::array<char, 0x80>, 0x24> bite_sound{};
constexpr RetailField<std::array<char, 0x80>, 0xA4> struggle_sound{};
constexpr u32 kSize = 0x124;
static_assert(retail_fits(kSize, struggle_sound));
} // namespace blood_suck_state_parameter
namespace blood_suck_state_field
{
constexpr RetailField<u32, 0x18> grab_callout{};
constexpr RetailField<u16, 0x1C> grab_shape{};
constexpr RetailField<u32, 0x20> target{}; // the target's actor; message 0x7C from it clears it
constexpr RetailField<u32, 0x24> grabbed{}; // the target accepted message 0x3C
constexpr RetailField<u32, 0x28> unk_28{}; // cleared by the constructor only
constexpr RetailField<u32, 0x2C> latched{}; // the body's latches are raised (0x0001FC40)
constexpr RetailField<u32, 0x30> hidden{}; // the target's render bins (and its held weapon's) are off
constexpr RetailField<u32, 0x34> held{}; // the target body's holds, live holds and bit 0x3A are raised
constexpr RetailField<u32, 0x38> thrown{}; // the throw (0x0008BF90) has hit the target
// Four animation records (0x0003AAA0); set when the record was found.
constexpr RetailField<u32, 0x3C> reach_animation{};
constexpr RetailField<u32, 0x40> bite_animation{};
constexpr RetailField<u32, 0x44> struggle_animation{};
constexpr RetailField<u32, 0x48> throw_animation{};
constexpr RetailField<u32, 0x4C> phase{};
constexpr RetailField<float, 0x50> drain_timer{}; // a second between bites
constexpr u32 kTracking = 0x54; // 0x00109810's record over the target's pad: +0x14 (state +0x68) 1 bite, 2 struggle
constexpr RetailField<float, 0x60> struggle{}; // the tracking's +0xC; 30 (DAT_0032B108) throws
constexpr RetailField<u32, 0x68> tracking_state{};
constexpr RetailField<Vec3, 0x6C> grab_start{}; // where the target was grabbed, its height under the bound centre
constexpr RetailField<float, 0x78> grab_position{}; // slot 0's animation position when grabbed (0x0003B1B0)
constexpr RetailField<std::array<char, 0x80>, 0x7C> bite_sound{};
constexpr RetailField<std::array<char, 0x80>, 0xFC> struggle_sound{};
constexpr RetailField<u32, 0x17C> sound_playing{}; // actor sound slot 1
constexpr u32 kSize = 0x180;
static_assert(retail_fits(kSize, sound_playing));
} // namespace blood_suck_state_field
// The target the state reads: a player, with its scene views.
struct RuntimeBloodSuckTarget
{
    RuntimeJumpOnPeer peer; // Its actor, body (+0x95C), bit block and pad (+0x960).
    NativeActorFactoryInstance* view = nullptr; // Its render bins, model anchors and live attachments.
    RuntimeBodyDamageEntry* damage = nullptr; // Its body's damage entry.
    std::function<void(std::span<const u32>)> body_message; // Its body's vtable +0xC.
    Actor* held = nullptr; // Body +0x48's actor, or null.
    NativeActorFactoryInstance* held_view = nullptr;
};
struct RuntimeBloodSuckStateContext
{
    Game& game;
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes;
    EntityBitBlock& bits; // Body +0x15C.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    ActorAnimationState& animation; // Actor +0x50.
    std::span<const u8> mind; // Actor +0x958, or empty.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeActorSoundQueues& sounds;
    u32& random; // DAT_0054DD7C.
    const RuntimeDialogueInputState& input_state;
    std::span<const u8> struggle_preset; // 0x00302728, 0x10 bytes.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE names; null for 0.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x38.
    std::function<void(u32 address, std::span<u32>)> message; // Any actor's vtable +0xC; replies land in place.
    // 0x0003BE20 over an actor's records (+0x8C) with an id and the creation options.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callout;
    std::function<std::span<const u8>(u32 address)> target_record; // A mind +0x10 target record (DAT_00545988).
    std::function<RuntimeBloodSuckTarget(u32 actor_address)> target_at;
};
void blood_suck_state_object_sizes(ObjectSizes& output); // Xbox 0x0008B1E0
bool initialize_runtime_blood_suck_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008B260
void destroy_runtime_blood_suck_state(std::span<u8> state, RuntimeBloodSuckStateContext& context); // Xbox 0x0008B530
void tick_runtime_blood_suck_state(std::span<u8> state, RuntimeBloodSuckStateContext& context); // Xbox 0x0008B6C0
// The target held in front of the actor, facing it: pulled in over the grab (phase 1), then kept there (2, 3).
void step_runtime_blood_suck_state(std::span<u8> state, RuntimeBloodSuckStateContext& context); // Xbox 0x0008B9C0
void message_runtime_blood_suck_state(std::span<u8> state, std::span<const u32> message,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008BB90
void set_runtime_blood_suck_phase(u32 phase, std::span<u8> state,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008BBD0, EDI state
void start_runtime_blood_suck_animation(std::span<const u8> state,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008BD30, EAX state
// The grab point: the actor's grab shape's sphere centre, when it meets the target's model bound.
bool runtime_blood_suck_grab_point(u32 target_record, std::span<const u8> state, Vec3& output,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008BD80, EAX target record, ESI actor
// A bite a second: the target loses one energy (phase 4 at none) unless it is down or may not be hurt.
bool drain_runtime_blood_suck_target(std::span<u8> state, RuntimeBloodSuckStateContext& context); // Xbox 0x0008BE60, EAX state
void track_runtime_blood_suck_struggle(std::span<u8> state,
    RuntimeBloodSuckStateContext& context); // Xbox 0x0008BF50, EAX state
// The throw: the parameters' hit (0x47, else the actor's 0x28) on the target, its death motion, and message 0x6E.
void throw_runtime_blood_suck_target(std::span<u8> state, RuntimeBloodSuckStateContext& context); // Xbox 0x0008BF90, EAX actor
// The body is down (bit block byte 0 bit 7), or a player that may not be hurt now.
bool runtime_body_down_or_unhurtable(std::span<const u8> body, const Actor& actor,
    const EntityBitBlock& bits, std::span<const u32, 2> blocked, std::span<const u8> modes,
    u32 mode); // Xbox 0x00021190, ECX body
// Slot 0's animation position (+0x14 of its player), or 0 without one.
float runtime_actor_animation_slot_position(const ActorAnimationState& animation, u32 slot); // Xbox 0x0003B1B0, EAX actor, ECX slot
// Type 0x61 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_blood_suck_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeBloodSuckStateContext&(u32 body_address)> context_of);
}
