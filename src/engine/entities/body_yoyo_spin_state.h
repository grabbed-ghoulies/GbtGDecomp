#pragma once
#include <array>
#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_contacts.h"
#include "body_scoop_whack_state.h"
#include "body_state_actions.h"
#include "body_state_fields.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"
#include <functional>

namespace gbtg
{
// entityStateYoYoSpinAttack (factory type 0x90, getter 0x000A6A00, vtable 0x00505CE0) over the body state (type 9):
// a yo-yo thrown at the target. Phase 1 plays the wind-up (+0x24) to its end; phase 2 throws (+0x28, sounds +0x30 /
// +0xB0) toward the mind target's middle (+0x1D0, or 40 ahead) for 0.5 s; phase 3 turns the yo-yo's contact shapes
// on (tag +0x1B0, scale +0x1B4) and holds it out for 0.5 s or until body bit 0x15 calls out 0xE; phase 4 winds it
// back; phase 5 plays the catch (+0x2C) and hands over the next action. Each frame the body's point (+0x18) eases
// between the throw's start (+0x1C4) and its aim by the blend (+0x20). Messages 0x41 and 0x70 call out +0x1C0 and
// cut phase 3 short. The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace yoyo_spin_state_parameter
{
constexpr RetailField<u32, 0x0C> windup{}; // Animation names (XBE addresses)
constexpr RetailField<u32, 0x10> throw_animation{};
constexpr RetailField<u32, 0x14> catch_animation{};
constexpr RetailField<u16, 0x18> shape_tag{};
constexpr RetailField<float, 0x1C> shape_scale{};
constexpr RetailField<u32, 0x20> callout{};
constexpr RetailField<std::array<char, 0x80>, 0x24> throw_sound{};
constexpr RetailField<std::array<char, 0x80>, 0xA4> spin_sound{};
constexpr RetailField<std::array<char, 0x80>, 0x124> hold_sound{};
constexpr u32 kSize = 0x1A4;
static_assert(retail_fits(kSize, hold_sound));
} // namespace yoyo_spin_state_parameter
struct YoyoSpinState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18
    float timer = 0.f; // +0x1C
    float blend = 0.f; // +0x20
    u32 windup = 0; // +0x24, 0x0003AAA0 records; their native pointers are kept by state
    u32 throw_animation = 0; // +0x28
    u32 catch_animation = 0; // +0x2C
    std::array<char, 0x80> throw_sound{}; // +0x30
    std::array<char, 0x80> spin_sound{}; // +0xB0
    std::array<char, 0x80> hold_sound{}; // +0x130
    u16 shape_tag = 0; // +0x1B0
    std::array<u8, 2> unk_1B2{}; // +0x1B2, padding
    float shape_scale = 0.f; // +0x1B4
    u32 shapes_active = 0; // +0x1B8
    u32 shape_mode = 0; // +0x1BC, 0x0001FEB0's previous mode, restored on destroy
    u32 callout = 0; // +0x1C0
    Vec3 start{}; // +0x1C4
    Vec3 aim{}; // +0x1D0
};
static_assert(sizeof(YoyoSpinState) == 0x1DC);
struct RuntimeYoYoSpinStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    const EntityBitBlock& bits; // Body +0x15C.
    std::span<const u8> mind; // Actor +0x958; +0x10 the target record.
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0 pairs, 0x0001AFA0 and 0x00027650.
    const RuntimeActorClock& clock;
    std::function<RuntimeBodyStateContext()> state; // 0x0001FEB0's views.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    RuntimeActorSoundQueues& sounds;
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses.
    std::function<std::span<const u8>(u32)> target_at; // A DAT_00545988 target record's retail image.
    RuntimeActorCallout actor_callout; // 0x0003BE20 over an actor's records (+0x8C).
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void yoyo_spin_state_object_sizes(ObjectSizes& output); // Xbox 0x000A69E0
bool initialize_runtime_yoyo_spin_state(YoyoSpinState& state, std::span<const u8> parameters,
    RuntimeYoYoSpinStateContext& context); // Xbox 0x000A6A60
void destroy_runtime_yoyo_spin_state(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context); // Xbox 0x000A6C40
void tick_runtime_yoyo_spin_state(YoyoSpinState& state, RuntimeYoYoSpinStateContext& context); // Xbox 0x000A6CC0
void message_runtime_yoyo_spin_state(YoyoSpinState& state, std::span<const u32> message,
    RuntimeYoYoSpinStateContext& context); // Xbox 0x000A7030
// Type 0x90 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_yoyo_spin_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeYoYoSpinStateContext&(u32 body_address)> context_of);
}
