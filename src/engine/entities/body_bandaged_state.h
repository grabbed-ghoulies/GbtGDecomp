#pragma once
#include <array>
#include "input_tracking_record.h"

#include "actor_clock.h"
#include "actor_factory_owner.h"
#include "body_action_selection.h"
#include "body_hit_state.h"
#include "body_state_fields.h"
#include "player_attachment.h"
#include "../frontend/dialogue_input.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x60, entityStateBandaged (getter 0x0008A300, vtable 0x005056E0; the factory table's row 0x5F carries
// the name) over the body state (type 9): a mummy's bandages bind the body. Phase (+0x18) 1 wraps it: a bandage
// wrap avatar (the parameters' wrap model) plays the bind animation over the body; once it has played past 0.9 the
// body hides behind a mummified avatar (the parameters' model) and phase 2 holds it still (2) or struggling (3) while
// its pad shakes it free; full struggle (20) or the timer running out (8 s) tears the bandages off (4): the wrap
// unravels (type 0x14), dust and bandage bits fly, and the body takes its next action when its escape animation ends.
// +0x14 and +0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace bandaged_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> mummified_model{};
// The three animations' names (XBE addresses), from the body's table: still, struggle and escape.
constexpr RetailField<u32, 0x8C> still_animation{};
constexpr RetailField<u32, 0x90> struggle_animation{};
constexpr RetailField<u32, 0x94> escape_animation{};
constexpr RetailField<std::array<char, 0x80>, 0x98> wrap_model{};
constexpr RetailField<std::array<char, 0x80>, 0x118> bind_animation{};
constexpr RetailField<std::array<char, 0x80>, 0x198> dust_effect{};
constexpr RetailField<std::array<char, 0x80>, 0x218> bits_effect{};
constexpr RetailField<std::array<char, 0x80>, 0x298> still_sound{};
constexpr RetailField<std::array<char, 0x80>, 0x318> struggle_sound{};
constexpr u32 kSize = 0x398;
static_assert(retail_fits(kSize, struggle_sound));
} // namespace bandaged_state_parameter
struct BandagedState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18
    std::array<char, 0x80> wrap_model{}; // +0x1C
    std::array<char, 0x80> bind_animation{}; // +0x9C
    std::array<char, 0x80> dust_effect{}; // +0x11C
    std::array<char, 0x80> bits_effect{}; // +0x19C
    std::array<char, 0x80> mummified_model{}; // +0x21C
    u32 mummified = 0; // +0x29C, the mummified avatar; message 0x7C naming it clears it
    u32 wrap = 0; // +0x2A0, the bandage wrap avatar; message 0x7C naming it clears it
    // +0x2A4: 0x00109810's record over the pad: its state is 1 still, 2 struggle; its accumulation at 20 (DAT_0032B234)
    // escapes.
    InputTrackingRecord tracking{};
    // +0x2BC: Three animation records (0x0003AAA0), held natively beside these words.
    u32 still_animation = 0;
    u32 struggle_animation = 0; // +0x2C0
    u32 escape_animation = 0; // +0x2C4
    std::array<char, 0x80> still_sound{}; // +0x2C8
    std::array<char, 0x80> struggle_sound{}; // +0x348
    float unk_3C8 = 0.f; // +0x3C8, 8 (DAT_0032B26C), set by the constructor only
    float timer = 0.f; // +0x3CC, 8 s (DAT_0032B26C) of binding while the body has energy
    float scale = 0.f; // +0x3D0, the avatars' scale: actor +0x34 * body +0x884 * 0.05
    u32 hidden = 0; // +0x3D4, the body and its held actor are hidden behind the mummified avatar
    u32 held = 0; // +0x3D8, the body's +0x30 hold count is raised
};
static_assert(sizeof(BandagedState) == 0x3DC);
struct RuntimeBandagedStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes;
    EntityBitBlock& bits; // Body +0x15C.
    ActorRenderRegistration& render; // The actor's render bins (0x0003CAE0 / 0x0003CBB0).
    RuntimePlayerAttachment& attachment; // 0x0001B810's views.
    RuntimeBodyActionSelection& actions; // The body's animation table and attached actor (0x0003AAA0).
    const RuntimeActorClock& clock;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeActorSoundQueues& sounds;
    ActorEffectStreamContext& effects;
    const Vec3& default_position; // DAT_0050DD80.
    const char* empty_name; // DAT_00546208, for 0x000411A0.
    std::span<const u8> struggle_preset; // 0x0030260C, 0x10 bytes.
    const RuntimeDialogueInputState& input_state;
    RuntimeBodyRecoveryAction& recovery; // 0x00133F50's views.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE names; null for 0.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<NativeActorFactoryInstance&(u32 address)> actor_view; // A scene actor's native views.
    // 0x0013AF80 over a scene actor: an after-image fading over `duration`, `tint` its parameters +0x340.
    std::function<void(u32 address, float duration, const float* tint)> actor_fade;
    const RuntimeDialogueInputDevice* input = nullptr; // The tick's second argument (actor +0x960 or zero).
};
void bandaged_state_object_sizes(ObjectSizes& output); // Xbox 0x0008A2E0
bool initialize_runtime_bandaged_state(BandagedState& state, std::span<const u8> parameters,
    RuntimeBandagedStateContext& context); // Xbox 0x0008A360
void destroy_runtime_bandaged_state(BandagedState& state, RuntimeBandagedStateContext& context); // Xbox 0x0008A5A0
void tick_runtime_bandaged_state(BandagedState& state, RuntimeBandagedStateContext& context); // Xbox 0x0008A5F0
void message_runtime_bandaged_state(BandagedState& state, std::span<const u32> message,
    RuntimeBandagedStateContext& context); // Xbox 0x0008A8E0
void set_runtime_bandaged_phase(u32 phase, BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008A950, EAX state
// The body hides behind the mummified avatar (0x0008AE30), its +0x309..+0x30C counts and +0x310 raised.
void hide_runtime_bandaged_body(BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008AAD0, EAX state
// Undoes 0x0008AAD0 and the +0x30 hold, and deletes both avatars.
void release_runtime_bandaged_state(BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008AB60, EDI state
// The bandage wrap avatar at the body, playing the bind animation.
void spawn_runtime_bandage_wrap(BandagedState& state, RuntimeBandagedStateContext& context); // Xbox 0x0008AC30
// The wrap unravelling off the body (type 0x14).
void spawn_runtime_bandage_unwrap(const BandagedState& state, RuntimeBandagedStateContext& context); // Xbox 0x0008ADC0
void spawn_runtime_mummified_avatar(BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008AE30, EBX state
// The dust effect stream at `actor` (EAX), bound to it.
void start_runtime_bandage_dust(u32 actor_address, const BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008AF60, EAX actor
// The bandage bits effect stream at `actor` (EAX), raised by its model height.
void start_runtime_bandage_bits(u32 actor_address, const BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008B050, EAX actor
// The bandages tear: the body is down, its timer ran out, or its struggle is full.
bool runtime_bandaged_escape_due(BandagedState& state,
    RuntimeBandagedStateContext& context); // Xbox 0x0008B140, EAX state, ECX body
// The type 0x14 parameters (0x340 bytes): the avatar at `position` / `euler`, the model and animation, and the
// animation's start `position`, `speed` (its sign the direction, its size the duration) and the fade time.
void build_runtime_anim_delete_parameters(std::span<u8, 0x340> parameters, const Vec3& position, const Vec3& euler,
    float scale, const char* model, const char* animation, float speed, float start, float fade,
    const char* default_name); // Xbox 0x0006CBB0, EAX parameters, EDI position, ECX euler, EBX model
// Type 0x60 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_bandaged_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeBandagedStateContext&(u32 body_address)> context_of);
}
