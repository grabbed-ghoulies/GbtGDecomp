#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_attachment_requests.h"
#include "../frontend/dialogue_input.h"

#include <unordered_map>

namespace gbtg
{
struct RuntimeBodyCollisionState;

// entityStrategyHunchback (factory type 0x45, getter 0x000B3A90, vtable 0x00505E88) over the base strategy (type 8):
// the hunchback. Idle (0x401) he steps out, taunts a few times and runs about; a target within the block's +4 sends him
// evading (0x3FE), from which a target past +8 brings him back, and one within +0 makes him block (0x40C) or, on an
// attack roll, charge his special attack (0x3EF: prepare, the armed charge with its hand shapes, a timed recovery,
// the finish). A landed hit makes him dizzy (0x409) until his body recovers.
struct HunchbackStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_00428770
    std::array<char, 0x80> charge_effect{}; // +0x3C, parameters +0x18
    float taunt_delay = 0.f; // +0xBC, seconds to the next taunt
    std::array<u8, 4> unk_C0{}; // +0xC0, never read or written by translated code
    u32 idle_phase = 0; // +0xC4, 1 stepping out, 2 taunting, 3 running about
    i32 taunts = 0; // +0xC8, taunts made
    i32 taunt_limit = 0; // +0xCC, from the block's +0x14..+0x18
    u32 dizzy_step = 0; // +0xD0, 1 falling, 2 down, 3 pushed away
    u32 special_phase = 0; // +0xD4, 1 prepare, 3 armed, 4 charging, 5 recovering, 6 finish
    float special_time = 0.f; // +0xD8
    u32 saved_334 = 0; // +0xDC, body +0x334 while armed
    u32 armed = 0; // +0xE0, the attack row and hand shapes are on (0x000B39D0 undoes them)
    u32 effect = 0; // +0xE4, the charge effect (0x0003DF40)
    u32 interrupted = 0; // +0xE8, message 100 during the special attack
};
static_assert(sizeof(HunchbackStrategyState) == 0xEC);
struct RuntimeHunchbackStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views;
    // DAT_00428770: +0 / +4 / +8 the block, evade and give-up reaches, +0xC..+0x10 the taunt delay, +0x14..+0x18 the
    // taunt count (integers), +0x1C..+0x28 message 0x6F's health bands, +0x2C..+0x30 the armed time, +0x34..+0x38
    // the charge time, +0x3C the charge's body +0x334, +0x40..+0x44 the recovery time.
    std::span<const u8> parameters;
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    std::span<const u8>& attack_resource; // Body +0x5C.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeInputAxisWriteDevice& axes; // Actor +0x960 as 0x00108FC0 writes it.
    std::span<u8> keyboard; // DAT_00507C98.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40; the object's address, or 0.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    // 0x0003BE20 over an actor's +0x8C callout records with the given creation options.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callouts;
    const char* goal_evade; // "evade", 0x003217D8.
    const char* goal_into_far; // "intofar", 0x003216DC.
    const char* goal_random_run; // "randomrun", 0x003216D0.
    const char* goal_taunt; // "tauntnoturn", 0x00321664.
    const char* goal_into_block; // "intoblock", 0x0031D26C.
    const char* goal_prepare; // "specialattackprepare", 0x00321688.
    const char* goal_prepare_armed; // "specialattackprepare2", 0x00321670.
    const char* goal_special; // "specialattack", 0x003216B4.
    const char* goal_finish; // "specialattackfinish", 0x003216A0.
};
void hunchback_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B3A70
bool initialize_runtime_hunchback_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    Actor& actor); // Xbox 0x000B3AE0
void update_runtime_hunchback_strategy(RuntimeHunchbackStrategy& context); // Xbox 0x000B3B40, vtable +0x10
void dispatch_runtime_hunchback_message(std::span<const u32> message,
    RuntimeHunchbackStrategy& context); // Xbox 0x000B3BF0, vtable +0xC
void enter_runtime_hunchback_state(u16 state, u16 previous,
    RuntimeHunchbackStrategy& context); // Xbox 0x000B3DD0, vtable +0x14
void leave_runtime_hunchback_state(u16 state, RuntimeHunchbackStrategy& context); // Xbox 0x000B3E70, vtable +0x18
// The special attack's next phase and its entry.
void set_runtime_hunchback_special_phase(u32 phase,
    RuntimeHunchbackStrategy& context); // Xbox 0x000B3780, EAX strategy, ECX phase
// The armed charge undone: hand shapes, attack row, holds, latches, the effect and body +0x334.
void disarm_runtime_hunchback(RuntimeHunchbackStrategy& context); // Xbox 0x000B39D0, EDI strategy
// The idle state's next phase and its entry.
void set_runtime_hunchback_idle_phase(u32 phase,
    RuntimeHunchbackStrategy& context); // Xbox 0x000B2F20, ECX strategy, EAX phase
// A hit during the armed or charging phase starts the recovery.
void recover_runtime_hunchback(RuntimeHunchbackStrategy& context); // Xbox 0x000B3700, ECX strategy
// Message 0x6F's stun time by health band: the block's +0x28 below a quarter, +0x24 below half, +0x20 below
// three quarters, else +0x1C.
float runtime_hunchback_stun_time(RuntimeHunchbackStrategy& context); // Xbox 0x000B3F10, EAX actor, EDX strategy
}
