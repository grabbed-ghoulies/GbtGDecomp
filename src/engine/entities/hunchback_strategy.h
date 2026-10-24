#pragma once
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
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace hunchback_strategy_field
{
constexpr RetailField<u32, 0x38> parameters{}; // DAT_00428770
constexpr RetailField<std::array<char, 0x80>, 0x3C> charge_effect{}; // parameters +0x18
constexpr RetailField<float, 0xBC> taunt_delay{}; // seconds to the next taunt
constexpr RetailField<u32, 0xC4> idle_phase{}; // 1 stepping out, 2 taunting, 3 running about
constexpr RetailField<i32, 0xC8> taunts{}; // taunts made
constexpr RetailField<i32, 0xCC> taunt_limit{}; // from the block's +0x14..+0x18
constexpr RetailField<u32, 0xD0> dizzy_step{}; // 1 falling, 2 down, 3 pushed away
constexpr RetailField<u32, 0xD4> special_phase{}; // 1 prepare, 3 armed, 4 charging, 5 recovering, 6 finish
constexpr RetailField<float, 0xD8> special_time{};
constexpr RetailField<u32, 0xDC> saved_334{}; // body +0x334 while armed
constexpr RetailField<u32, 0xE0> armed{}; // the attack row and hand shapes are on (0x000B39D0 undoes them)
constexpr RetailField<u32, 0xE4> effect{}; // the charge effect (0x0003DF40)
constexpr RetailField<u32, 0xE8> interrupted{}; // message 100 during the special attack
constexpr u32 kSize = 0xEC;
static_assert(retail_fits(kSize, interrupted));
} // namespace hunchback_strategy_field
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
