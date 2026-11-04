#pragma once
#include <array>
#include "actor_fields.h"
#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "gbtg/retail_field.h"
#include <functional>

namespace gbtg
{
struct ActorDrawState;
struct ActorFrameDrawContext;
struct AnimationResourceRegistry;
struct ResourceErrorState;
struct RuntimeActorPauseGroups;
struct RuntimeGlowPass;
struct SceneModelDrawContext;
// entityAvatarMiscSSSDropper, factory type 0x21 (getter 0x00071820, vtable 0x00504F04) over the actor (type 2): a
// Super Scary figure that drops onto the player. The construction roll must not exceed the parameters' chance
// (+0x33C when the scene restarts, else +0x338). It waits at its marker until the player is free and within its reach
// (or always, without one), then drops ("drop") to its target: across in 0.2 s while falling at 500 a second
// squared, leaving a short after-image every 1/15 s and turning to face the player. Landing ("land") starts the Super
// Scary challenge (type 0x98) with its two clips; then it waits ("scare") until message 0x8E 7 or 8 sends it back up
// ("leapoff") over 5 s, after which it releases itself. Its draw (pass 2 only) goes through the glow capture,
// stamped back at alpha 0xC4.
namespace sss_dropper_parameter
{
constexpr RetailField<float, 0x334> reach{}; // 0 or less: any distance
constexpr RetailField<float, 0x338> chance{};
constexpr RetailField<float, 0x33C> restart_chance{}; // used while DAT_00430D0C is 1
constexpr RetailField<Vec3, 0x340> target{};
constexpr RetailRecord<0x34C, 0x80> first_clip{};
constexpr RetailRecord<0x3CC, 0x80> second_clip{};
constexpr u32 kSize = 0x44C;
static_assert(retail_fits(kSize, second_clip));
} // namespace sss_dropper_parameter
struct SssDropperState
{
    ActorBytes base{}; // +0
    float reach = 0.f; // +0x934
    Vec3 start{}; // +0x938, the world position at construction
    Vec3 target{}; // +0x944
    std::array<u8, 4> unk_950{}; // +0x950, never read or written by translated code
    u32 phase = 0; // +0x954, 0 waiting, 1 dropping, 2 landing, 3 scaring, 4 leaping off
    float fall_speed = 0.f; // +0x958
    float timer = 0.f; // +0x95C, the drop's 0.2 s, then the leap's 5 s
    float trail_timer = 0.f; // +0x960, the next after-image
    std::array<char, 0x80> first_clip{}; // +0x964
    std::array<char, 0x80> second_clip{}; // +0x9E4
};
static_assert(sizeof(SssDropperState) == 0xA64);
struct RuntimeSssDropper
{
    std::span<u8> bytes;
    NativeActorFactoryInstance& view;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    const RuntimeActorClock& clock;
    const i32& restarting; // DAT_00430D0C.
    u32& runtime_flags; // DAT_0050EA9C.
    const char* default_name; // 0x00069590's default emitter name.
    std::function<void(std::span<u32>)> base_message; // 0x0003EF70 over retail message words.
    std::function<const Actor*()> player; // DAT_00511B40 minus 0x93C; null for no player.
    std::function<std::span<const u8>(const Actor&)> player_body; // That player's +0x95C body.
    std::function<u32(const Actor&)> address_of; // An actor's retail address.
    std::function<void(u32, std::span<u32>)> message; // Vtable +0xC of the object at an address.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520; the scene keeps the object.
    std::function<RuntimeActorPauseGroups&()> pause; // 0x00117400.
    std::function<bool(float&)> timer_elapsed; // 0x00042360 on the actor with the scene's frame clock.
};
void sss_dropper_object_sizes(ObjectSizes& sizes); // Xbox 0x00071800
bool initialize_runtime_sss_dropper(RuntimeSssDropper& context, std::span<const u8> parameters); // Xbox 0x00071870
// `update_actor` is the actor update 0x000420D0 that ends every phase.
void update_runtime_sss_dropper(RuntimeSssDropper& context,
    const std::function<void()>& update_actor); // Xbox 0x00071960
void dispatch_runtime_sss_dropper_message(RuntimeSssDropper& context, std::span<u32> message); // Xbox 0x00071D50
u32 draw_runtime_sss_dropper(ActorDrawState& state, i32 kind, const ActorFrameDrawContext& attachments,
    SceneModelDrawContext& draw, RuntimeGlowPass& glow); // Xbox 0x00071DF0
// EDI phase, ESI actor: 1 drops, 2 lands and starts the challenge, 3 scares, 4 leaps off.
void set_runtime_sss_dropper_phase(RuntimeSssDropper& context, u32 phase); // Xbox 0x00071F30
// The player is free (its body still, no cutscene hold, decay on) and within reach.
bool runtime_sss_dropper_triggered(RuntimeSssDropper& context); // Xbox 0x00072100, EBX actor
void start_runtime_sss_dropper_challenge(RuntimeSssDropper& context); // Xbox 0x00072200, EDX actor
}
