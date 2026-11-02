#pragma once

#include "body_hit_state.h"
#include "actor_render_registration.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
// Factory type 0x64's state (see RuntimeDizzyStateContext). +0xC is the next key and +0x10 the state clock
// (`body_state_field`).
namespace dizzy_state_parameter
{
constexpr RetailField<u32, 0x0C> settles{};
constexpr RetailRecord<0x10, 0x80, 2> star_names{}; // two NUL-terminated names for the stars' parameters
constexpr RetailField<float, 0x110> star_scale{};
constexpr RetailField<u32, 0x114> waits_for_animation{};
constexpr RetailField<u32, 0x118> body_bit{};
constexpr u32 kSize = 0x11C;
static_assert(retail_fits(kSize, body_bit));
} // namespace dizzy_state_parameter
// One of the three stars (0xC bytes): its orbit and bob angles and its actor.
namespace dizzy_star_field
{
constexpr RetailField<float, 0x0> orbit{};
constexpr RetailField<float, 0x4> bob{};
constexpr RetailField<u32, 0x8> actor{}; // cleared when message 0x7C names it
constexpr u32 kSize = 0xC;
static_assert(retail_fits(kSize, actor));
} // namespace dizzy_star_field
namespace dizzy_state_field
{
// The two input trackers 0x00109810 runs over (the stick's main and secondary axes); +0x14 of each is its state.
constexpr RetailRecord<0x18, 0x18> primary_tracking{};
constexpr RetailRecord<0x30, 0x18> secondary_tracking{};
constexpr RetailField<u32, 0x2C> primary_tracking_state{}; // 2 speeds the stars up by DAT_00428670
constexpr RetailField<u32, 0x44> secondary_tracking_state{};
constexpr RetailField<u32, 0x48> player_input{}; // the actor is the controlled player (actor +0xCDC 2, or +0x93C)
constexpr RetailField<u32, 0x4C> waits_for_animation{}; // the state ends as the animation crosses its end
constexpr RetailField<u32, 0x50> settles{}; // the body's turn settles each tick (0x00134720)
constexpr RetailField<float, 0x54> star_scale{}; // of the actor's +0x38 radius, by 0.2
constexpr RetailField<float, 0x58> time_left{}; // body +0x23C at entry
constexpr RetailRecord<0x5C, 0x80, 2> star_names{}; // into the stars' parameters +0x28 and +0xA8
constexpr RetailRecord<0x15C, dizzy_star_field::kSize, 3> stars{};
constexpr RetailField<u32, 0x180> body_bit{};
constexpr u32 kSize = 0x184;
static_assert(retail_fits(kSize, body_bit));
} // namespace dizzy_state_field
// Native views of the three star objects the dizzy state places around the body's actor (the state's `stars`).
struct RuntimeDizzyStars
{
    std::function<u32(std::span<const u8> parameters)> create; // 0x00107520: the new object's address, or 0.
    std::function<Actor&(u32 address)> actor;
    std::function<ActorAnimationState&(u32 address)> animation; // Actor +0x50.
    std::function<ActorRenderRegistration&(u32 address)> render; // Actor +0x908 node and its list.
    std::function<void(u32 address, std::span<u32> message)> message; // Actor vtable +0xC.
};

// Factory type 0x64 (getter 0x0008C940, vtable 0x00505760; named entityStateDroneAppear in the type table): the
// body's dizzy state, used by the imp's actorstate_imp_dizzy row. +0x14/+0x18 are the empty 0x001628B0 and +0x1C
// is 0x0002B620. State +8 is the body.
struct RuntimeDizzyStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes;
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    const ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    RuntimeDizzyStars& stars;
    u32 actor_address;
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    const char* empty_name; // DAT_00546208.
    u32& random; // DAT_0054DD7C.
    RuntimeActorSoundQueues& sounds;
    RuntimeBodyRecoveryAction& recovery; // 0x00134060 and the 0x001346B0 views.
    const RuntimeDialogueInputDevice* input = nullptr; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState* input_state = nullptr;
    std::span<const u8> input_preset{}; // DAT_00428660.
    float input_scale = 0.f; // DAT_00428670.
};
void dizzy_state_object_sizes(ObjectSizes& output); // Xbox 0x0008C920
// The star's position (ESI) and euler (EBX) for slot `index` (ECX) around the body (EAX).
void place_runtime_dizzy_star(std::span<const u8> state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler); // Xbox 0x0008D230
void spawn_runtime_dizzy_stars(std::span<u8> state, RuntimeDizzyStateContext& context); // Xbox 0x0008CEF0
// Moves each live star along its orbit by `step` (XMM0) and places it.
void orbit_runtime_dizzy_stars(std::span<u8> state, float step,
    RuntimeDizzyStateContext& context); // Xbox 0x0008D0E0
bool initialize_runtime_dizzy_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDizzyStateContext& context); // Xbox 0x0008C9A0
void destroy_runtime_dizzy_state(std::span<u8> state, RuntimeDizzyStateContext& context); // Xbox 0x0008CB30
void message_runtime_dizzy_state(std::span<u8> state, std::span<const u8> message); // Xbox 0x0008CEA0
void tick_runtime_dizzy_state(std::span<u8> state, RuntimeDizzyStateContext& context); // Xbox 0x0008CC40
}
