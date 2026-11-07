#pragma once
#include "body_state_fields.h"
#include "input_tracking_record.h"

#include "body_hit_state.h"
#include "actor_render_registration.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
// Factory type 0x64's state (see RuntimeDizzyStateContext). +0xC is the next key and +0x10 the state clock
// (`BodyStateBase`).
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
// One of the three stars the state keeps orbiting the actor (state +0x15C).
struct DizzyStar
{
    float orbit = 0.f; // +0
    float bob = 0.f; // +4
    u32 actor = 0; // +8, cleared when message 0x7C names it
};
static_assert(sizeof(DizzyStar) == 0xC);
struct DizzyState
{
    BodyStateBase base{}; // +0
    // +0x18, +0x30: the two input trackers 0x00109810 runs over (the stick's main and secondary axes). Either one's
    // state at 2 speeds the stars up by DAT_00428670.
    InputTrackingRecord primary_tracking{};
    InputTrackingRecord secondary_tracking{};
    u32 player_input = 0; // +0x48, the actor is the controlled player (actor +0xCDC 2, or +0x93C)
    u32 waits_for_animation = 0; // +0x4C, the state ends as the animation crosses its end
    u32 settles = 0; // +0x50, the body's turn settles each tick (0x00134720)
    float star_scale = 0.f; // +0x54, of the actor's +0x38 radius, by 0.2
    float time_left = 0.f; // +0x58, body +0x23C at entry
    std::array<std::array<char, 0x80>, 2> star_names{}; // +0x5C, into the stars' parameters +0x28 and +0xA8
    std::array<DizzyStar, 3> stars{}; // +0x15C
    u32 body_bit = 0; // +0x180
};
static_assert(sizeof(DizzyState) == 0x184);
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
void place_runtime_dizzy_star(const DizzyState& state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler); // Xbox 0x0008D230
void spawn_runtime_dizzy_stars(DizzyState& state, RuntimeDizzyStateContext& context); // Xbox 0x0008CEF0
// Moves each live star along its orbit by `step` (XMM0) and places it.
void orbit_runtime_dizzy_stars(DizzyState& state, float step,
    RuntimeDizzyStateContext& context); // Xbox 0x0008D0E0
bool initialize_runtime_dizzy_state(DizzyState& state, std::span<const u8> parameters,
    RuntimeDizzyStateContext& context); // Xbox 0x0008C9A0
void destroy_runtime_dizzy_state(DizzyState& state, RuntimeDizzyStateContext& context); // Xbox 0x0008CB30
void message_runtime_dizzy_state(DizzyState& state, std::span<const u8> message); // Xbox 0x0008CEA0
void tick_runtime_dizzy_state(DizzyState& state, RuntimeDizzyStateContext& context); // Xbox 0x0008CC40
}
