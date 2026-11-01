#pragma once
#include "player_strategy_goals.h"
#include "intro_notices.h"
#include "player_state.h"
#include "actor_attachment_events.h"
#include "../audio/event_voices.h"
#include "../frontend/brew_meter.h"
#include "../frontend/dialogue_input.h"
#include <string_view>

namespace gbtg
{
// Player strategy (type 0x4F, vtable 0x00505FA0) methods over the base strategy.
struct RuntimePlayerStrategyFlow
{
    RuntimeBaseStrategyMessages& base;
    RuntimeActorPauseGroups& pause; // 0x00117400.
    u32& pause_count; // DAT_00510CEC.
    RuntimeBrewMeter& brew;
    RuntimePlayerHealthDisplay& health;
    RuntimeInputAxisWriteDevice& axis_device; // Actor +0x960 for the axis writers.
    const RuntimeDialogueInputDevice& input_device; // Actor +0x960 for the input reads.
    const RuntimeDialogueInputState& input_state;
    std::span<u8> keyboard; // DAT_00507C98.
    AudioEventVoices& voices;
    const RuntimeActorResourceReference& resource; // Actor +0x8C.
    ActorAttachmentEventContext& callout;
    u32& brews_taken; // DAT_00506C88.
    u32& brew_accepted; // DAT_00430D98.
    const i32& prompt_lock; // DAT_004E1C34.
    const u32& prompt_owner; // DAT_004EE1EC.
    const i32& difficulty; // DAT_005064D8.
    const i32& chapter_mode; // DAT_00430D0C.
    const u8& unlock_flags; // **(DAT_00506C70+8).
    const u8& scene_flags; // **(DAT_00506C74+8).
    std::string_view scene_script; // DAT_00510E58.
    const i32& scene_kills; // DAT_00506C8C, the scene's kills credited to tags 4, 9 and 0xE (0x0012ACF0).
    const i32& brew_kills_required; // DAT_00430D9C, 1 and only read: a brew needs one kill in the scene.
    u8& story_flags; // Byte +4 of **(DAT_00506C6C+8).
    u32& intro_pending; // DAT_00545B38.
    std::span<char> intro_name; // DAT_00545B44.
    const entity_detail::ListHead& cases; // DAT_00511B3C; nodes at actor +0x93C.
    std::function<const EntityBitBlock*(const entity_detail::ListHead*)> case_body_bits; // Node actor body +0x15C.
    const char* goal_idle; // "idle", 0x003213A4.
};

u32 runtime_brew_prompt_available(RuntimePlayerStrategyFlow& context); // Xbox 0x000BEDB0
void queue_runtime_scripted_goals(std::span<const u32> message,
    RuntimePlayerStrategyFlow& context); // Xbox 0x000BF4D0, EAX strategy, ECX message
u32 check_runtime_player_knockout(RuntimePlayerStrategyGoals& context); // Xbox 0x0013A490, EBX strategy
u32 runtime_difficulty_allows_rest(const i32& difficulty); // Xbox 0x000D0690
u32 runtime_player_idle_elapsed(RuntimePlayerStrategyFlow& context); // Xbox 0x000BF600, EAX body, EDI strategy
u32 runtime_player_wants_to_move(RuntimePlayerStrategyFlow& context); // Xbox 0x000BF6A0, EBX actor
void unlock_runtime_brew_intro(RuntimePlayerStrategyFlow& context); // Xbox 0x0013FF30
void dispatch_runtime_player_strategy_message(std::span<const u32> message,
    RuntimePlayerStrategyFlow& context); // Xbox 0x000BEE10
void update_runtime_player_strategy(RuntimePlayerStrategyFlow& context); // Xbox 0x000BEF40
void enter_runtime_player_strategy_state(u16 state, RuntimePlayerStrategyFlow& context); // Xbox 0x000BF2B0
}
