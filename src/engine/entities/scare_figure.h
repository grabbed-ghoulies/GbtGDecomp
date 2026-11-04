#pragma once
#include <array>
#include "actor_fields.h"
#include "actor_base_messages.h"
#include "actor_clock.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include <functional>

namespace gbtg
{
struct ActorDrawState;
struct ActorFrameDrawContext;
struct SceneModelDrawContext;
struct RuntimeGlowPass;
// entityAvatarMiscScareFigure, factory type 0x1D (getter 0x0006EFB0, vtable 0x00504E14) over the actor (type 2): a
// scare figure (DAT_0030CB18's names, marker kind 0x16) that waits (+0x94C) for the player to come near, fades in,
// plays, fades out and goes, or stays when +0xB78 is set. Its draw fades it through the glow pass (+0x944 alpha).
// States (+0x960): 0 waiting, 1 fading in, 2 playing, 3 waiting for no actor of tag 0x67, 4 fading out, 5 done.
namespace scare_figure_parameter
{
constexpr RetailField<float, 0x334> reach{};
constexpr RetailField<float, 0x338> delay{};
constexpr RetailField<float, 0x33C> chance{};
constexpr RetailField<u32, 0x340> unk_340{};
constexpr RetailField<u32, 0x344> unk_344{};
constexpr RetailField<Vec3, 0x348> trigger{};
constexpr RetailRecord<0x354, 0x80> appear_animation{};
constexpr RetailRecord<0x3D4, 0x80> appear_events{};
constexpr RetailRecord<0x454, 0x80> animation{};
constexpr RetailRecord<0x4D4, 0x80> play_events{};
constexpr RetailField<u32, 0x554> fade_in_plays{};
constexpr RetailField<i32, 0x558> alpha{};
constexpr RetailField<float, 0x55C> fade_in{};
constexpr RetailField<float, 0x560> fade_out{};
constexpr RetailField<u32, 0x564> stays{};
constexpr RetailField<u32, 0x568> scare_word{};
constexpr RetailField<float, 0x56C> facing{};
constexpr RetailField<i32, 0x570> facing_states{};
} // namespace scare_figure_parameter
struct ScareFigureState
{
    ActorBytes base{}; // +0
    Vec3 trigger{}; // +0x934, the point the player must come within +0x954 of
    std::array<u8, 4> unk_940{}; // +0x940, never read or written by translated code
    i32 alpha = 0; // +0x944, 0..+0xB6C; 0xFF draws without the glow pass
    u32 paused = 0; // +0x948, slot 0's player held by 0x0006FB10
    float timer = 0.f; // +0x94C, the current state's countdown
    float delay = 0.f; // +0x950, state 0's wait
    float reach = 0.f; // +0x954, zero or less needs no player near
    u32 unk_958 = 0; // +0x958
    u32 unk_95C = 0; // +0x95C
    i32 state = 0; // +0x960
    i32 facing_states = 0; // +0x964, states below this turn toward the player
    std::array<char, 0x80> appear_animation{}; // +0x968, an "aid_" name replaces the main animation in 1 and 3
    std::array<char, 0x80> appear_events{}; // +0x9E8
    std::array<char, 0x80> animation{}; // +0xA68
    std::array<char, 0x80> play_events{}; // +0xAE8, state 2's animation events
    u32 fade_in_plays = 0; // +0xB68, zero holds the main animation while it fades in
    i32 alpha_limit = 0; // +0xB6C
    float fade_in = 0.f; // +0xB70
    float fade_out = 0.f; // +0xB74, zero ends at state 2
    u32 stays = 0; // +0xB78, nonzero: drawn throughout and kept at the end
    u32 scare_word = 0; // +0xB7C, message 0xB2's third word to the players
    float facing = 0.f; // +0xB80, at or above zero: the yaw added to the turn toward the player
};
static_assert(sizeof(ScareFigureState) == 0xB84);
struct RuntimeScareFigure
{
    std::span<u8> bytes;
    Actor& actor;
    ActorAnimationState& animation; // Actor +0x48 / +0x50.
    ActorBaseContext& base;
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70 over retail message words.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    const RuntimeActorClock& clock;
    const u32& runtime_flags; // DAT_0050EA9C.
    std::function<const Actor*()> player; // DAT_00511B40 minus 0x93C; null for the null and sentinel tokens.
    std::function<void(std::span<const u32>)> tell_players; // 0x00122D10, EDI the message, over DAT_00511B40.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810.
    std::function<Actor*(u16)> first_tagged; // The first DAT_00511AA4 actor with this +6 tag, or null.
    std::function<void(Actor&, std::span<const u32>)> message; // An actor's vtable +0xC.
};
void scare_figure_object_sizes(ObjectSizes& sizes); // Xbox 0x0006EF90
bool initialize_runtime_scare_figure(RuntimeScareFigure& context, std::span<const u8> parameters); // Xbox 0x0006F000
void update_runtime_scare_figure(RuntimeScareFigure& context); // Xbox 0x0006F1C0, then 0x000420D0 and 0x0003D670
void dispatch_runtime_scare_figure_message(RuntimeScareFigure& context, std::span<u32> message); // Xbox 0x0006F5C0
u32 draw_runtime_scare_figure(RuntimeScareFigure& context, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw,
    RuntimeGlowPass& glow); // Xbox 0x0006F600
void set_runtime_scare_figure_state(i32 state, RuntimeScareFigure& context); // Xbox 0x0006F7C0, EAX actor
bool runtime_scare_figure_triggered(RuntimeScareFigure& context); // Xbox 0x0006FA40, EBX actor
// Slot 0 takes the named animation with the events (mix +0x24) and mode 1 unless continuous, then is held or let go.
void play_runtime_scare_figure_animation(const char* events, const char* name, bool hold, bool continuous,
    RuntimeScareFigure& context); // Xbox 0x0006FB10, EAX events
bool runtime_scare_blocker_present(RuntimeScareFigure& context); // Xbox 0x0006FC70
// Message 0x63 to the first actor of tag 6: the scare's cylinder (0x0003DD50).
void notify_runtime_scare_watcher(RuntimeScareFigure& context); // Xbox 0x0013D510
// The decay-gated countdown: false while DAT_0050E768 is held or DAT_0030F144[DAT_0051248C] is clear.
bool tick_runtime_decay_timer(const Actor& actor, float& timer,
    const RuntimeActorClock& clock); // Xbox 0x000423D0, ECX timer, EDX actor
}
