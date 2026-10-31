#pragma once
#include "gbtg/entity.h"
#include "entity_types.h"
#include "actor_clock.h"
#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// Gate and storage for the queued intro notice (DAT_00545B38/DAT_00545B44).
struct RuntimeIntroNotices
{
    const u32& runtime_flags; // DAT_0050EA9C.
    const u8& scene_flags; // **(DAT_00506C74+8).
    u32& intro_pending; // DAT_00545B38.
    std::span<char> intro_name; // DAT_00545B44.
    const entity_detail::ListHead& cases; // DAT_00511B3C; nodes at actor +0x93C.
    const std::function<const EntityBitBlock*(const entity_detail::ListHead*)>& case_body_bits;
};
u32 runtime_intro_notice_ready(const RuntimeIntroNotices& context); // Xbox 0x0013FF70
u32 queue_runtime_intro_notice(const char* name, RuntimeIntroNotices& context); // Xbox 0x0013FF00, ESI name
// The death curse's intro, once (story bit 0x10): "intros_deathcurse" queued when a notice may show.
u32 queue_runtime_death_curse_intro(u8& story_byte, RuntimeIntroNotices& context); // Xbox 0x0013FEB0
// The queued intro notice's state: 1 queued, 2 the scene paused and waiting a second, 3 the notice showing.
extern u32 g_545b38; // DAT_00545B38
extern u32 g_545b3c; // DAT_00545B3C, the notice holds the scene paused.
extern float g_545b40; // DAT_00545B40, the wait before the notice shows.
extern std::array<char, 0x80> g_545b44; // DAT_00545B44, the notice's dialogue key.
extern std::array<char, 0x80> g_545bc4; // DAT_00545BC4, a second key shown after it, when set.
struct RuntimeDialogueActivationContext;
struct RuntimeIntroNoticeTick
{
    i32& pause_requests; // DAT_0050E794.
    const RuntimeActorClock& clock; // DAT_0050E768 and the decay table, then the step.
    const u32& prompt_owner; // DAT_004EE1EC, the dialogue's mode.
    RuntimeDialogueActivationContext& dialogue; // 0x000582A0.
    std::function<void(u8, i32)> pause; // 0x00117400.
};
// A queued notice pauses the scene (once nothing holds the actors and decay runs) and waits a second; then its key and
// the second key, when set, are shown (state 3), and once shown the notice is finished (0x0013FFD0).
void tick_runtime_intro_notice(RuntimeIntroNoticeTick& context); // Xbox 0x0013FD60
// Once no dialogue page holds the prompt, the notice's pause is released and its state and keys cleared.
void finish_runtime_intro_notice(RuntimeIntroNoticeTick& context); // Xbox 0x0013FFD0
}
