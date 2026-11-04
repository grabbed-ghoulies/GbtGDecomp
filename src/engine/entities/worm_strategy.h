#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include <functional>

namespace gbtg
{
// entityStrategyWorm (factory type 0x57, getter 0x000C4580, vtable 0x00506080; the factory table's row 0x56 carries
// the name) over the base strategy (type 8): the worm. Holding (0x401) it wanders ("far") until it claims a target
// queue, then attacks (0x3EF: "attackprepare" while its body's bit 0x13 is clear, then "attack"). Once bit 0x13 is up
// its timer (+0x138) runs down while it attacks or holds, and at zero it rests (0x400). Its effects: mode 1 (the
// object named at +0x38, with body bit block byte 0 bit 7) on its animation event 0x3A naming flag 0x13, mode 2 (the
// object named at +0xB8, with the body's latches raised) when it is hit. Its body (type 0x2B) reads bit 0x13.
// Strategy bytes: +0x12 / +0x18 states, +0x1A attacking, +0x1C pause count, +0x20 target queue link (+0x30 active).
struct WormStrategyState
{
    StrategyBase base{}; // +0
    std::array<char, 0x80> emerge_effect_name{}; // +0x38, parameters +0x18, mode 1's object
    std::array<char, 0x80> hit_effect_name{}; // +0xB8, parameters +0x98, mode 2's object
    float timer = 0.f; // +0x138, DAT_0032B184 (5) on construction; 0x000C4D60 runs it down
    u32 effect = 0; // +0x13C, the current mode's object; message 0x14 from it forgets it
    u32 effect_mode = 0; // +0x140, 0x000C4C70: 1 emerged, 2 hit, 0 none
};
static_assert(sizeof(WormStrategyState) == 0x144);
struct RuntimeWormStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<u32(const char*, u32)> create_effect; // 0x00107720 / 0x00107520 with the actor at +8, 0x000663E0.
    std::function<bool(Actor&, u32)> release_frame_object; // 0x0003DE30, ECX actor, ESI object.
    std::function<std::span<const u8>(u32)> event_at; // An animation event record (message 7 +0xC) by address.
    std::span<const char* const> flag_names; // 0x00338FB0, 0x41 actorBodyFlagId names.
    std::function<bool(u16)> select_request; // 0x0001D210 on the body's attachment requests, EAX key.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_attack_prepare; // "attackprepare", 0x0032128C.
};
void worm_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000C4560
bool initialize_runtime_worm_strategy(std::span<u8> strategy, std::span<const u8> parameters); // Xbox 0x000C45D0
void destroy_runtime_worm_strategy(RuntimeWormStrategy& context); // Xbox 0x000C4620
// The effect mode changes: the old mode's object is released and its mark undone, then the new mode's made.
void set_runtime_worm_effect_mode(u32 mode, RuntimeWormStrategy& context); // Xbox 0x000C4C70, EDI strategy
// The body's +0xDC timer follows the strategy's; the strategy's runs down and at zero the worm rests (0x400).
bool run_down_runtime_worm_timer(const RuntimeMindTarget* target,
    RuntimeWormStrategy& context); // Xbox 0x000C4D60, EAX target, ECX body, ESI strategy
void update_runtime_worm_strategy(RuntimeWormStrategy& context); // Xbox 0x000C4690, vtable +0x10
void dispatch_runtime_worm_message(std::span<const u32> message,
    RuntimeWormStrategy& context); // Xbox 0x000C4820, vtable +0xC
void enter_runtime_worm_state(u16 state, u16 previous, RuntimeWormStrategy& context); // Xbox 0x000C4B20, vtable +0x14
void leave_runtime_worm_state(u16 state, RuntimeWormStrategy& context); // Xbox 0x000C4C10, vtable +0x18
}
