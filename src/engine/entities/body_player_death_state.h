#pragma once
#include "body_knockdown_state.h"
#include "../audio/bank_streams.h"
#include "../camera/camera_pulse.h"
#include "../frontend/dialogue_activation.h"

namespace gbtg
{
struct LiveScript;

// Factory type 0x69 (getter 0x0008EED0, vtable 0x00505800): the player's death. The body stops (counters +0x309..+0x30C
// and +0x30, shape mode 2), drops what it holds, plays the dying animation (+0x34) then the dead one (+0x38) and starts
// the scene's transition; a live player whose run allows it is offered a brew (state 4), every other way restarts the
// scene (0x0008F650, 0x00134AE0). +0xC and +0x14/+0x18 are the empty 0x000D4179 case and +0x1C is 0x0002B620.
// State +0xC is the next key and +0x10 the state clock (`body_state_field`).
namespace player_death_state_parameter
{
constexpr RetailField<u32, 0x0C> dying{}; // the animation's name, an XBE string, or 0 to start dead
constexpr RetailField<u32, 0x10> dead{}; // the animation's name, an XBE string
constexpr RetailField<u32, 0x14> death_word{};
constexpr u32 kSize = 0x18;
static_assert(retail_fits(kSize, death_word));
} // namespace player_death_state_parameter
namespace player_death_state_field
{
constexpr RetailField<u32, 0x18> phase{}; // 0 dying, 1 dead, 2 waiting, 3 done, 4 the brew offer
constexpr RetailField<float, 0x1C> wait_timer{};
constexpr RetailField<u8, 0x20> saved_shape_mode{};
constexpr RetailField<u32, 0x24> death_word{}; // the word message 0x34 carries to the actor
constexpr RetailField<u32, 0x28> contacts_raised{}; // body +0x715 / +0x717 are raised
constexpr RetailField<u32, 0x2C> fail_stream{}; // the fail stream plays
constexpr RetailField<u32, 0x30> saved_717{}; // body +0x717 at entry
// Whether the dying and dead animations were found; the records stay on the host.
constexpr RetailField<u32, 0x34> dying_found{};
constexpr RetailField<u32, 0x38> dead_found{};
constexpr u32 kSize = 0x3C;
static_assert(retail_fits(kSize, dead_found));
} // namespace player_death_state_field
struct RuntimePlayerDeathContext
{
    RuntimeKnockdownContext& knockdown; // Body, actor, attachment, shape, actions, death effects and flags.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::span<u8> mind; // Actor +0x958; empty when the actor has none.
    LiveScript* mind_events; // Mind +0x24's anchor; its next (mind +0x28) is the first event.
    RuntimeCameraPulse& pulse;
    AudioBankStreams& streams;
    RuntimeDialogueActivationContext& dialogue;
    i32& active_strategies; // DAT_00510CEC.
    i32& scene_counter; // DAT_004DEA24.
    i32& restart_pending; // DAT_00430D94.
    const i32& scene_kills; // DAT_00506C8C, the scene's kills credited to tags 4, 9 and 0xE (0x0012ACF0).
    const i32& brew_kills_required; // DAT_00430D9C, 1 and only read: a brew needs one kill in the scene.
    const i32& frontend_state; // DAT_004E17C8.
    const u32& prompt_owner; // DAT_004EE1EC.
    const i32& transition_phase; // DAT_0054632C.
    u32& transition_row_two; // DAT_005493DC.
    std::span<u8> story_bits; // *(DAT_00506C6C + 8).
    std::span<const u8> unlock_bits; // *(DAT_00506C70 + 8).
    const char* scene_name; // DAT_00510E58.
    std::function<void()> begin_transition; // 0x0012C5D0.
};

void player_death_state_object_sizes(ObjectSizes& output); // Xbox 0x0008EEB0
bool initialize_runtime_player_death_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePlayerDeathContext& context); // Xbox 0x0008EF30
void destroy_runtime_player_death_state(std::span<u8> state, RuntimePlayerDeathContext& context); // Xbox 0x0008F1A0
void enter_runtime_player_death_phase(std::span<u8> state, u32 phase,
    RuntimePlayerDeathContext& context); // Xbox 0x0008F4D0, EAX phase, EDI state
void restart_runtime_player_death(std::span<u8> state, RuntimePlayerDeathContext& context); // Xbox 0x0008F650
void start_runtime_player_fail_stream(std::span<u8> state, RuntimePlayerDeathContext& context); // Xbox 0x0008F6A0
// ECX timer, EAX the state: counts the timer down by the body's step; true on the step it reaches 0.
bool count_runtime_state_timer(float& timer, const Actor& actor, const RuntimeActorClock& clock); // Xbox 0x0002B630
void tick_runtime_player_death_state(std::span<u8> state, RuntimePlayerDeathContext& context); // Xbox 0x0008F290
}
