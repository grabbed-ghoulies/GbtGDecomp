#pragma once
#include <array>

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_state_fields.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
struct ExecutableResourcePool;
// Factory type 0x84 (getter 0x000A1450, vtable 0x00505B60; the retail name is not tied to it) over the body state
// (type 9): the body plays an entry animation (parameters +0x10C), then a held one (+0x110) with a sound on actor slot
// 1 (+0xC) while it takes hits (messages 0x15, 0x19 and 0x1D). Once it has taken a random count of them (+0x114 ..
// +0x118), or its mind's target (actor +0x958 -> +0x10) is gone, it makes an object from the parameters' template
// (+0x11C, its +8 the actor) with a sound on slot 6 (+0x8C), and after a hold (+0x19C, at the actor's rate) sets body
// bit 0x2B. The target's going away (message 0x7C) ends the hits at once. Body +0x890 holds 0x20000 meanwhile. The
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace endure_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> held_sound{}; // actor slot 1 while the hits count
constexpr RetailField<std::array<char, 0x80>, 0x8C> end_sound{}; // actor slot 6 with the object
constexpr RetailField<u32, 0x10C> entry_animation{}; // an XBE string address
constexpr RetailField<u32, 0x110> held_animation{};
constexpr RetailField<i32, 0x114> least_hits{};
constexpr RetailField<i32, 0x118> most_hits{};
constexpr RetailField<std::array<char, 0x80>, 0x11C> object{}; // an object template (AID)
constexpr RetailField<float, 0x19C> hold{}; // seconds
constexpr u32 kSize = 0x1A0;
static_assert(retail_fits(kSize, hold));
} // namespace endure_state_parameter
struct EndureState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18, 1 entry, 2 taking hits, 3 holding
    u32 sounding = 0; // +0x1C, the held sound plays on slot 1
    std::array<char, 0x80> held_sound{}; // +0x20
    std::array<char, 0x80> object{}; // +0xA0
    float hold = 0.f; // +0x120
    u32 entry_animation = 0; // +0x124, 0x0003AAA0's record (its host half kept beside the state)
    u32 held_animation = 0; // +0x128
    u32 saved_890 = 0; // +0x12C, body +0x890 at entry
    i32 hits = 0; // +0x130
    i32 needed_hits = 0; // +0x134
    u32 made = 0; // +0x138, the object made at phase 3, or 0
    std::array<char, 0x80> end_sound{}; // +0x13C
};
static_assert(sizeof(EndureState) == 0x1BC);
struct RuntimeEndureStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0 / 0x0001AFA0.
    EntityBitBlock& bits; // Body +0x15C.
    const RuntimeActorClock& clock;
    RuntimeActorSoundQueues& sounds;
    std::span<const u8> mind; // Actor +0x958, or empty.
    std::function<std::span<const u8>(u32 address)> target_at; // A mind +0x10 target record's retail image.
    std::span<ExecutableResourcePool> resources; // 0x00107720's templates.
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<void(u32 address, std::span<u32> message)> message; // An object's vtable +0xC.
    std::function<const char*(u32 address)> image_string;
    u32& random; // DAT_0054DD7C.
};
void endure_state_object_sizes(ObjectSizes& output); // Xbox 0x000A1430
bool initialize_runtime_endure_state(EndureState& state, std::span<const u8> parameters,
    RuntimeEndureStateContext& context); // Xbox 0x000A14B0
void destroy_runtime_endure_state(EndureState& state, RuntimeEndureStateContext& context); // Xbox 0x000A1670
void tick_runtime_endure_state(EndureState& state, RuntimeEndureStateContext& context); // Xbox 0x000A1710
void message_runtime_endure_state(EndureState& state, std::span<const u32> message,
    RuntimeEndureStateContext& context); // Xbox 0x000A1850
// The phase's entry: 1 the entry animation (body +0x890 to 0x20000), 2 the held animation and its sound, 3 the object
// and the end sound.
void enter_runtime_endure_phase(EndureState& state, u32 phase,
    RuntimeEndureStateContext& context); // Xbox 0x000A1930, EDI state
// Type 0x84 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_endure_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeEndureStateContext&(u32 body_address)> context_of);
}
