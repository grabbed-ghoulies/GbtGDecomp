#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "../scripts/script_table_lookup.h"

namespace gbtg
{
// entityStrategyMummy (factory type 0x4C, getter 0x000BBC20, vtable 0x00505F4C) over the base strategy (type 8):
// scene 9's mummies. A mummy keeps a ring 2..6 reaches out from its target (0x000BBAB0), re-picked every 1.5..4 s;
// once provoked (+0x38) and queued on the target it attacks ("attack", 0x3ED), whacks with its ankh ("ankh_whack",
// 0x3EE, while the body holds something at +0x48) or binds the target ("bondage_bandage", 0x3EF), picked by the
// target's action table and the actor's aggression (0x000BC580). Too far off (80) it falls back to the ring.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace mummy_strategy_field
{
constexpr RetailField<u32, 0x38> provoked{}; // Hostile at construction, message 1, or a hit (0x6E)
constexpr RetailField<u32, 0x3C> unk_3C{}; // Message 0x68's ankh attack; a landed hit then staggers (0x3F2)
constexpr RetailField<i32, 0x40> side{}; // 1 or -1, drawn by 0x000BB9F0
constexpr RetailField<float, 0x44> ring_delay{}; // Seconds to the next ring point, 1.5 + 2.5 * random
constexpr RetailField<u32, 0x48> held{}; // Message 0x5F's actor; 0x7C clears it (0x00140A40)
constexpr u32 kSize = 0x4C;
static_assert(retail_fits(kSize, held));
} // namespace mummy_strategy_field
struct RuntimeMummyStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    // A target actor's body's action table (body +0x73C), by the body's bytes.
    std::function<const RuntimeScriptLookupTable*(std::span<const u8> body)> actions_of;
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_ankh_getout; // "ankh_getout", 0x00321550.
    const char* goal_ankh_whack; // "ankh_whack", 0x00321544.
    const char* goal_bandage; // "bondage_bandage", 0x00321534.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_scared; // "scared", 0x00321234.
};
void mummy_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000BBC00
bool initialize_runtime_mummy_strategy(std::span<u8> strategy, std::span<const u8> actor); // Xbox 0x000BBC70
// A mind request 0x2E to a clear point between 1 and 2 (else 6) of the mummy's reaches beyond the target's radius.
void approach_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context); // Xbox 0x000BBAB0, EAX strategy, ECX target
// Without a target the mummy waits (0x416); otherwise the ring delay runs down and a new ring point is requested.
void update_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context); // Xbox 0x000BB8E0, ESI strategy, ECX target, EDX mind
// A new ring delay and side, then a ring point.
void start_runtime_mummy_ring(const RuntimeMindTarget* target, RuntimeMummyStrategy& context); // Xbox 0x000BB9F0, EAX strategy
bool runtime_mummy_engages(const RuntimeMindTarget* target, RuntimeMummyStrategy& context); // Xbox 0x000BC560, EAX strategy, EDI target
void choose_runtime_mummy_attack(RuntimeMummyStrategy& context); // Xbox 0x000BC580, EAX strategy
void update_runtime_mummy_strategy(RuntimeMummyStrategy& context); // Xbox 0x000BBCB0, vtable +0x10
void dispatch_runtime_mummy_message(std::span<const u32> message,
    RuntimeMummyStrategy& context); // Xbox 0x000BBFA0, vtable +0xC
void enter_runtime_mummy_state(u16 state, u16 previous, RuntimeMummyStrategy& context); // Xbox 0x000BC350, vtable +0x14
void leave_runtime_mummy_state(u16 state, RuntimeMummyStrategy& context); // Xbox 0x000BC520, vtable +0x18
}
