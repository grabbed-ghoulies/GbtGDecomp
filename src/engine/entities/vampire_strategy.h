#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include <functional>

namespace gbtg
{
// entityStrategyVampire (factory type 0x54, getter 0x000C2000, vtable 0x0050602C; the factory table's row 0x53
// carries the name) over the base strategy (type 8): the vampire in his coffin. Waiting in it (0x401) he springs out
// at a target in reach once the record's delay has run: to open the coffin ("special", 0x3EF) by the actor's attack
// roll, else to knock the target down ("knockdown", 0x3EE) or back ("knockback", 0x3ED). Hit from behind while he
// waits he is knocked back (0x406, "coffinlook"); message 0x1A shuts him in ("waitingforfollowup", 0x416), the body's
// bit 0x40 then set. His body (type 0x2C) reads that bit.
struct VampireStrategyState
{
    StrategyBase base{}; // +0
    u32 record = 0; // +0x38, 0x003034D0
    float delay = 0.f; // +0x3C, the record's +8 on entering 0x401; no attack while it runs
    u32 shut = 0; // +0x40, message 0x1A this frame; the update clears it
    u32 knocked_stage = 0; // +0x44, 0x406: 1 knocked, 2 walked off, 3 looking round
    u32 knockdown_stage = 0; // +0x48, 0x3EE: 1 knockdown, 2 the cheer or the close
    u32 knockback_stage = 0; // +0x4C, 0x3ED: 1 knockback, 2 the follow-up, 3 the close
    u32 struck = 0; // +0x50, message 0x6E from him (its +8) since the knockback began
    float open_time = 0.f; // +0x54, the record's +0xC on opening (0x3EF)
    u32 open_stage = 0; // +0x58, 0x3EF: 1 open, 2 grabbed, 3 closing
};
static_assert(sizeof(VampireStrategyState) == 0x5C);
struct RuntimeVampireStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    const char* goal_knockback; // "knockback", 0x003212F4.
    const char* goal_knockback_followup; // "knockbackfollowup", 0x003212E0.
    const char* goal_close_knockback; // "coffincloseknockback", 0x0032131C.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
    const char* goal_open_cheer; // "coffinopencheer", 0x00321300.
    const char* goal_close; // "coffinclose", 0x0031C598.
    const char* goal_special; // "special", 0x00321484.
    const char* goal_look; // "coffinlook", 0x00321310.
    const char* goal_wander; // "randomwander", 0x003217B0.
    const char* goal_waiting; // "waitingforfollowup", 0x003212C0.
    std::function<void()> knock_back; // 0x00020690 on his body (actor +0x95C).
};
void vampire_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000C1FE0
bool initialize_runtime_vampire_strategy(std::span<u8> strategy); // Xbox 0x000C2050
// The target is in reach once the delay has run: alive, not down, queued on, and held by no one but him.
bool runtime_vampire_reaches_target(RuntimeVampireStrategy& context); // Xbox 0x000C17C0, EAX strategy
// Out of the coffin by the actor's rolls: open it (0x3EF), knock down (0x3EE) or knock back (0x3ED).
void choose_runtime_vampire_attack(RuntimeVampireStrategy& context); // Xbox 0x000C1890, EAX strategy, EDX actor
// The body's hit direction (+0x1CC) lands from behind: no heading or hit, or a dot past the record's +0.
bool runtime_vampire_hit_from_behind(RuntimeVampireStrategy& context); // Xbox 0x000C1640, EDI strategy, ESI body +0x1CC
void update_runtime_vampire_strategy(RuntimeVampireStrategy& context); // Xbox 0x000C2070, vtable +0x10
void dispatch_runtime_vampire_message(std::span<const u32> message,
    RuntimeVampireStrategy& context); // Xbox 0x000C2140, vtable +0xC
void enter_runtime_vampire_state(u16 state, u16 previous,
    RuntimeVampireStrategy& context); // Xbox 0x000C2350, vtable +0x14
void leave_runtime_vampire_state(u16 state, RuntimeVampireStrategy& context); // Xbox 0x000C2420, vtable +0x18
}
