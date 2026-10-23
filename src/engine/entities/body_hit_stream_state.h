#pragma once

#include "actor_attachment_events.h"
#include "actor_clock.h"
#include "body_attachment_cleanup.h"
#include "body_state_fields.h"
#include "body_timers.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x76, entityStateHitStream (getter 0x00097200, vtable 0x005059A0) over the body state (type 9): the
// body stands in a stream (the extinguisher's spray, message 0x18) and loses body +0x280 energy a second while it
// lasts, pushed away from its attacker (body +0x270) at the parameters' speed (+0xC). Each hit renews the stream for
// 0.5 s and turns the parameters' callout 0x36 effects (the actor's +0x8C records, kept at +0x13C) to face along it;
// the stream's sound (+0x12) plays on actor sound slot 1 while the actor runs. Its end hands the body its next key;
// emptied, the body takes key 0x82 (or body vtable +0x4C with +0x130 set) and every actor hears {0x6E, ...}. The
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace hit_stream_state_parameter
{
constexpr RetailField<float, 0x0C> push_speed{};
constexpr RetailField<u16, 0x10> unk_10{}; // copied to state +0x18
constexpr RetailField<std::array<char, 0x80>, 0x12> sound{};
constexpr RetailField<std::array<char, 0x80>, 0x92> start_sound{};
constexpr RetailField<std::array<char, 0x80>, 0x112> effect{}; // an object template (AID) made with the actor
constexpr RetailField<u32, 0x194> unk_194{}; // state +0x130
constexpr u32 kSize = 0x198;
static_assert(retail_fits(kSize, unk_194));
} // namespace hit_stream_state_parameter
namespace hit_stream_state_field
{
constexpr RetailField<u16, 0x18> unk_18{}; // parameters +0x10
constexpr RetailField<std::array<char, 0x80>, 0x1A> sound{}; // parameters +0x12
constexpr RetailField<std::array<char, 0x80>, 0x9A> start_sound{}; // parameters +0x92, played on slot 4 at entry
constexpr RetailField<float, 0x11C> push_speed{}; // parameters +0xC
constexpr RetailField<float, 0x120> stream_left{}; // 0.5 s (DAT_0032B0F4) after each hit
constexpr RetailField<float, 0x128> drained{}; // the energy fraction not yet taken
constexpr RetailField<u32, 0x12C> hit{}; // set by a hit this tick (and at entry)
constexpr RetailField<u32, 0x130> unk_130{}; // parameters +0x194: an emptied body takes vtable +0x4C, not key 0x82
constexpr RetailField<u32, 0x134> sounding{}; // the stream's sound plays
constexpr RetailField<u32, 0x138> effect{}; // the object made from parameters +0x112, or 0
constexpr u32 kEffects = 0x13C; // Callout 0x36's tokens: a count, then up to eight words (0x0013B8B0).
constexpr u32 kSize = 0x160;
} // namespace hit_stream_state_field
struct RuntimeHitStreamStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes; // The actor's instance bytes (+0xCDC, +0x93C).
    EntityBitBlock& bits; // Body +0x15C.
    const RuntimeActorClock& clock;
    RuntimeActorSoundQueues& sounds;
    ActorAttachmentEventContext& events; // 0x0013B8B0's record views.
    const u8* callout_records; // Actor +0x8C.
    RuntimeBodyAttachmentLists& lists; // Actor +0x1B8 / +0x1B0 and DAT_00506930 (0x0003CFD0).
    std::span<ExecutableResourcePool> resources; // 0x00107720's templates.
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<bool(Actor& actor, u32 object)> release_frame_object; // 0x0003DE30, ECX actor, ESI object.
    std::function<u32(const void*)> address_of; // The retail word of the state's +0x13C record.
    std::function<Actor*(u32)> actor_at; // Body +0x270's attacker.
    std::function<bool()> damage_enabled; // 0x00028D50 on the actor.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x4C.
    RuntimeBodyTimerRecipient* first_timer; // Body +0x760.
};
void hit_stream_state_object_sizes(ObjectSizes& output); // Xbox 0x000971E0
bool initialize_runtime_hit_stream_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeHitStreamStateContext& context); // Xbox 0x00097260
void destroy_runtime_hit_stream_state(std::span<u8> state, RuntimeHitStreamStateContext& context); // Xbox 0x00097400
void tick_runtime_hit_stream_state(std::span<u8> state, RuntimeHitStreamStateContext& context); // Xbox 0x00097500
// Message 0x18 renews the stream and points the callout effects along its velocity at its point.
void message_runtime_hit_stream_state(std::span<u8> state, std::span<const u32> message,
    RuntimeHitStreamStateContext& context); // Xbox 0x00097890
// The energy the stream takes this tick (whole units of body +0x280 a second, into `taken`); the body's energy left.
u16 drain_runtime_hit_stream(std::span<u8> state, u32& taken,
    RuntimeHitStreamStateContext& context); // Xbox 0x000979F0, EAX state, EDI body
// Type 0x76 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_hit_stream_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeHitStreamStateContext&(u32 body_address)> context_of);
}
