#pragma once
#include <array>
#include "body_state_fields.h"

#include "body_blood_suck_state.h"

namespace gbtg
{
// Factory type 0x72 entityStateGrabAndThrow (getter 0x000947A0, vtable 0x00505900; the factory table's row 0x71
// carries the name) over the body state (type 9): a ghouly grabs its mind target and throws it toward a marker. On
// construction it takes the target (the mind's record +0x10, unless the record's +0x14 is set) as its victim and marks
// it held (victim body +0x718), and picks the kind 0x1A marker whose +8 word is its actor's +0x1FC key, else the one
// farthest from it. Its hand shape (parameters +0x10) reaching the victim between 0.15 and 0.25 of its animation
// sends the victim message 0x3C, and on the reply 0x3D: the victim is grabbed (+0x4C), held at the hand's model
// binding (parameters +0x12) by the +0x14 slot, turned toward the marker, and thrown (0x000951B0) once the animation
// passes 0.55. The +0x18 slot is the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
struct GrabThrowState
{
    BodyStateBase base{}; // +0
    u32 grab_callout = 0; // +0x18, parameters +0xC: the victim's callout on the grab (0x0013AD60)
    u16 hand_shape = 0; // +0x1C, parameters +0x10: the 0x00042560 shape id the hand reaches with
    std::array<u8, 2> unk_1E{}; // +0x1E, padding
    u32 binding = 0; // +0x20, the model binding (parameters +0x12) the victim is held at, or 0
    u32 victim = 0; // +0x24, the grabbed actor; message 0x7C from it forgets it
    u16 marker = 0; // +0x28, the kind 0x1A marker it throws toward
    std::array<u8, 2> unk_2A{}; // +0x2A, padding
    Vec3 hand{}; // +0x2C, last frame's hand centre (0x00095000); zero before the first test
    float hand_radius = 0.f; // +0x38
    Vec3 start_euler{}; // +0x3C, the actor's world euler on the grab
    float grab_position = 0.f; // +0x48, animation slot 0's position on the grab
    u32 grabbed = 0; // +0x4C
    u32 thrown = 0; // +0x50
    u32 latched = 0; // +0x54, the body's latches were raised on the grab (0x0001FC40)
    u32 abandoned = 0; // +0x58, no victim, or no way to it: the state ends with its animation
};
static_assert(sizeof(GrabThrowState) == 0x5C);
namespace grab_throw_state_parameter
{
constexpr RetailField<u32, 0xC> grab_callout{};
constexpr RetailField<u16, 0x10> hand_shape{};
constexpr RetailField<u16, 0x12> binding{};
} // namespace grab_throw_state_parameter
struct RuntimeGrabThrowStateContext
{
    Game& game;
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes; // +6 the class, +0x1FC the marker key, +0xCF4 the damage class.
    const RuntimeActorClock& clock;
    ActorAnimationState& animation; // Actor +0x50.
    std::span<const u8> mind; // Actor +0x958, or empty.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    u32& random; // DAT_0054DD7C.
    const u8* markers; // DAT_0051108C.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    std::function<void(u32 address, std::span<u32>)> message; // Any actor's vtable +0xC; replies land in place.
    // 0x0003BE20 over an actor's records (+0x8C) with an id and the creation options.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callout;
    std::function<std::span<const u8>(u32 address)> target_record; // A mind +0x10 target record (DAT_00545988).
    std::function<RuntimeBloodSuckTarget(u32 actor_address)> target_at;
};
void grab_throw_state_object_sizes(ObjectSizes& output); // Xbox 0x00094780
bool initialize_runtime_grab_throw_state(GrabThrowState& state, std::span<const u8> parameters,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00094800
void destroy_runtime_grab_throw_state(GrabThrowState& state,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00094A30
void tick_runtime_grab_throw_state(GrabThrowState& state,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00094B00
void step_runtime_grab_throw_state(GrabThrowState& state,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00094F10
void message_runtime_grab_throw_state(GrabThrowState& state, std::span<const u32> message,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00094FA0
// The hand shape against the target's bound, swept from last frame's hand: the hand centre where it touches.
bool runtime_grab_throw_hand_reaches(GrabThrowState& state, u32 target_record, Vec3& output,
    RuntimeGrabThrowStateContext& context); // Xbox 0x00095000, EAX actor, EDX target record
// The victim is let go level with the actor, hurt, and knocked away along the actor's heading.
void throw_runtime_grab_throw_victim(GrabThrowState& state,
    RuntimeGrabThrowStateContext& context); // Xbox 0x000951B0
// Type 0x72 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_grab_throw_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGrabThrowStateContext&(u32 body_address)> context_of);
}
