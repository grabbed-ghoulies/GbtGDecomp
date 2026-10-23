#pragma once

#include "actor_clock.h"
#include "body_contacts.h"
#include "body_state_fields.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x6A, entityStateFlameThrower (getter 0x0008F6F0, vtable 0x00505820) over the body state (type 9): the
// body breathes fire. Its sound (the parameters' +0xC) plays on actor sound slot 4 and its flame effect (+0x8C, an
// "aid_" stream) is bound to the model's kind 5 locator. It aims at the mind's target (mind +0x14) for 1.25 s, then
// sways its head for 3.7 s before the body's next action. Each frame five growing spheres sweep along the flame from
// the locator; each actor they touch hears 0x15 and the body's own actor hears 0x41, and the touched actors stay out
// of the next sweeps. Message 0x7C lets one back in. The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
namespace flamethrower_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> sound{};
constexpr RetailField<std::array<char, 0x80>, 0x8C> effect{};
constexpr u32 kSize = 0x10C;
static_assert(retail_fits(kSize, effect));
} // namespace flamethrower_state_parameter
namespace flamethrower_state_field
{
constexpr RetailField<u32, 0x18> phase{}; // 0 aiming, 1 swaying; 2 and 3 turn up and down
constexpr RetailField<float, 0x1C> yaw{}; // the actor's yaw at entry
constexpr RetailField<float, 0x20> sway{}; // the sway's angle, 150 degrees a second
constexpr RetailField<float, 0x24> timer{}; // 1.25 s (DAT_0032B114), then 3.7 s (DAT_0032B514)
constexpr RetailField<std::array<char, 0x80>, 0x28> sound{}; // parameters +0xC
constexpr RetailField<std::array<char, 0x80>, 0xA8> effect{}; // parameters +0x8C
constexpr RetailField<i32, 0x128> effect_slot{}; // its DAT_00506930 index, -1 for none
constexpr RetailField<u32, 0x12C> effect_joint{}; // the kind 5 locator's joint
constexpr RetailField<u32, 0x130> overrides{}; // the touched actors' contact overrides (0x0013EBE0), or 0
constexpr RetailField<float, 0x134> flame_pitch{}; // the flame's direction, following the actor at 100 degrees a second
constexpr RetailField<float, 0x138> flame_yaw{};
constexpr RetailField<float, 0x13C> reach{}; // 0.01 (DAT_0032B124) growing to 1 at 1.98 a second
constexpr RetailField<u32, 0x140> aimed{}; // set once the flame's direction was taken from the actor
constexpr u32 kSize = 0x144;
static_assert(retail_fits(kSize, aimed));
} // namespace flamethrower_state_field
struct RuntimeFlamethrowerStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    const RuntimeActorClock& clock;
    std::span<const u8> mind; // Actor +0x958; +0x14 the target record.
    std::function<std::span<const u8>(u32)> target_at; // A DAT_00545988 target record's retail image.
    ActorEffectStreamContext& effects;
    RuntimeActorSoundQueues& sounds;
    std::function<u16()> next_key; // Body vtable +0x20.
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor; // Probe - 0x1E0.
    const entity_detail::ListHead& players; // DAT_00511B1C.
    std::function<void(RuntimeContactReport&)> report; // Message 0x41 through the actor's vtable +0xC.
    std::function<Actor*(u32)> actor_at; // Message 0x7C's actor.
};
void flamethrower_state_object_sizes(ObjectSizes& output); // Xbox 0x0008F6D0
bool initialize_runtime_flamethrower_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008F750
void destroy_runtime_flamethrower_state(std::span<u8> state,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008F850
void tick_runtime_flamethrower_state(std::span<u8> state,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008F8F0
void message_runtime_flamethrower_state(std::span<u8> state, std::span<const u32> message,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008FBF0
// The pitch toward the mind's target from the body's eye height, less 60 degrees; 0 without a target.
float runtime_flamethrower_aim_pitch(RuntimeFlamethrowerStateContext& context); // Xbox 0x0008FC20
// The flame effect is started at the kind 5 locator, facing 60 degrees below the actor.
void start_runtime_flamethrower_effect(std::span<u8> state,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008FCD0
void move_runtime_flamethrower_effect(std::span<const u8> state,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x0008FE10
void sweep_runtime_flamethrower(std::span<u8> state, RuntimeFlamethrowerStateContext& context); // Xbox 0x0008FEC0
// One touched actor: kept out of the next sweeps, told 0x15 and reported to the actor as 0x41.
void report_runtime_flamethrower_contact(std::span<u8> state, RuntimeSegmentContactActor& object,
    const Vec3& direction, const Vec3& point, GhoulySpawnPool* contacts,
    RuntimeFlamethrowerStateContext& context); // Xbox 0x000902A0, EAX state, ECX direction, ESI point, EDI object
// Type 0x6A (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_flamethrower_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFlamethrowerStateContext&(u32 body_address)> context_of);
}
