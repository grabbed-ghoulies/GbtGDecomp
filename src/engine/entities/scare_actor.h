#pragma once
#include <array>
#include "actor_fields.h"
#include "actor_clock.h"
#include "../audio/bank_streams.h"
#include "../camera/camera_pulse.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"

#include <functional>
#include <span>

namespace gbtg
{
namespace entity_detail
{
struct DisplayView;
}
// The scare 0x0013B270 spawns (aid_objparams_ghoulies_misc_scare, factory type 0x1C, getter 0x0006FCC0, vtable
// 0x00504E50) over the actor (type 2): its model plays an animation once, turned to face the camera; its texture
// flickers with the animation; three half-second pulses tick down; past 0.95 of the clip it starts its effect; while
// the camera is inside its reach the view is tinted and it is not drawn; and it retires when the clip ends. A scare
// that silences the music holds XACT_SILENCE until it is released.
namespace scare_actor_parameter
{
constexpr RetailField<float, 0x334> intensity{}; // 0x0013B270's intensity; the clip plays over its length / this
constexpr RetailField<u32, 0x33C> silence{}; // 0x0013B270's value: nonzero holds XACT_SILENCE
constexpr RetailField<std::array<char, 0x80>, 0x340> animation{};
constexpr RetailField<std::array<char, 0x80>, 0x3C0> events{};
constexpr RetailField<std::array<char, 0x80>, 0x440> effect{}; // the effect stream started past 0.95 of the clip
constexpr u32 kSize = 0x4C0;
static_assert(retail_fits(kSize, effect));
} // namespace scare_actor_parameter
struct ScareActorState
{
    ActorBase base{}; // +0
    u32 pulsing = 0; // +0x934, 1 while pulses remain
    i32 pulses = 0; // +0x938, 3 at the start
    float pulse_time = 0.f; // +0x93C, counts down by the actor step, then half a second more
    u32 texture = 0; // +0x940, shader_lambert3's colour0 texture, a native address, or 0
    u32 player_flag = 0; // +0x944, the case player's body +0x15C byte 5 bit 4
    u32 camera_inside = 0; // +0x948, the view is within `reach`
    float unk_94C = 0.f; // +0x94C, 0.2 when `player_flag` turns on
    float reach = 0.f; // +0x950, 0x00070410: joint 0's scale times the actor's
    std::array<char, 0x80> effect{}; // +0x954, copied from the parameters' +0x440
    u32 silence = 0; // +0x9D4, the parameters' +0x33C
    u32 silenced = 0; // +0x9D8, XACT_SILENCE is held
};
static_assert(sizeof(ScareActorState) == 0x9DC);
struct RuntimeScareActor
{
    std::span<u8> bytes;
    Actor& actor;
    ActorAnimationState& animation; // Actor +0x50.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    RuntimeScreenTint& tint; // DAT_00504450.
    u32& runtime_flags; // DAT_0050EA9C.
    AudioBankStreams& streams; // DAT_004DF370.
    ActorEffectStreamContext& effects;
    std::function<u32()> player_flag; // DAT_00511B40's body +0x15C byte 5 bit 4, 0 without a case player.
    std::function<void(std::span<const u32>, const Vec3&, float)> broadcast; // 0x00122D80 over DAT_00511B38.
    std::function<u8*(u32)> texture_bytes; // A texture's bytes by its native address.
};
void scare_actor_object_sizes(ObjectSizes& output); // Xbox 0x0006FCA0
bool initialize_runtime_scare_actor(RuntimeScareActor& context, std::span<const u8> parameters); // Xbox 0x0006FD20
void release_runtime_scare_actor(RuntimeScareActor& context); // Xbox 0x0006FF20
// Joint 0's scale (its matrix's diagonal) times the actor's (+0x200 times +0x30 + +0x34 + +0x38), each a third.
float runtime_scare_actor_reach(const Actor& actor); // Xbox 0x00070410, ESI the actor, XMM0 the result
void start_runtime_scare_actor_effect(RuntimeScareActor& context); // Xbox 0x000703A0, EAX the actor
// After the actor update 0x000420D0 (which the caller runs first).
void update_runtime_scare_actor(RuntimeScareActor& context); // Xbox 0x0006FF90
// False (nothing drawn) while the camera is inside; otherwise the actor faces the camera and the caller draws it
// (0x0003C9E0).
bool face_runtime_scare_actor(RuntimeScareActor& context); // Xbox 0x000702C0
}
