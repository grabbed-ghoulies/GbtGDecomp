#pragma once

#include "body_hit_state.h"
#include "actor_render_registration.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
// Native views of the three star objects the dizzy state places around the body's actor (state +0x164, +0x170,
// +0x17C hold their actor addresses; +0x15C / +0x160 of each 0xC-byte slot are its orbit and bob angles).
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
