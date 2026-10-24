#pragma once
#include "spider_strategy.h"
#include "body_damage_tables.h"

namespace gbtg
{
// entityStrategyImpNinja (factory type 0x49, getter 0x000B7AB0, vtable 0x00505EF8; the factory table's row 0x4A
// carries the name) over the base strategy (type 8): the ninja imp. It wanders ("randommove" / "randomwalk") or
// meditates ("meditate", 0x412) while nothing is near, and once queued on its target psychs up (0x3ED), then kicks
// (0x3EE: a spin kick, a jump kick or a big jump kick) or gathers its inner force (0x3EF). The inner force's first
// ending makes it big: 1.7 times the size, the 0x1C attribute row and its health refilled (+0xBC).
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace imp_ninja_strategy_field
{
constexpr RetailField<u32, 0x38> parameters{}; // DAT_00303198: +0 reach, +8 move chance, +0xC meditate chance,
                                               // +0x10 / +0x14 the meditation's shortest and longest seconds
constexpr RetailField<u32, 0x3C> effect_name{}; // The inner force's effect template, copied from parameters +0x18
constexpr RetailField<u32, 0xBC> transformed{}; // The inner force has ended once and made the imp big
constexpr RetailField<u32, 0xC0> unk_C0{}; // Each tick: the actor is provoked, or its target is no player in view
constexpr RetailField<float, 0xC4> meditation{}; // Seconds left in state 0x412
constexpr RetailField<u32, 0xC8> idle_step{}; // State 0x401: 1 waits for body bit 0x30, 2 picks a move or attack
constexpr RetailField<u32, 0xCC> meditation_step{}; // State 0x412: 1 meditating, 2 getting out of it
constexpr RetailField<u32, 0xD0> effect{}; // The inner force's effect object
constexpr RetailField<u32, 0xD4> effect_made{}; // 0x000B7360 ran (+0xD0 may still be 0)
constexpr RetailField<u32, 0xD8> spin_kick{}; // 0x3EE takes "spinkick": a light target was kicked from 0x3ED
constexpr u32 kSize = 0xDC;
static_assert(retail_fits(kSize, spin_kick));
} // namespace imp_ninja_strategy_field
struct RuntimeImpNinjaStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    std::span<const u8> parameters; // DAT_00303198, 0x18 bytes.
    const RuntimeBodyDamageTables* const& damage_tables; // 0x00028DA0's tables, set once the scene has them.
    const u8* tag_rows; // DAT_00545AC4, stride 0x87.
    const char* const* tag_names; // DAT_0042A850, read by 0x00133A50.
    std::function<u32(const char*, u32)> create_effect; // 0x00107720 / 0x00107520 with the actor at +8.
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    std::function<void(std::span<const u32>)> actor_message; // The actor's own vtable +0xC.
    const char* goal_psycheup; // "psycheup", 0x0031CFB0.
    const char* goal_psycheup_traitor; // "psycheuptraitor", 0x003215BC.
    const char* goal_spin_kick; // "spinkick", 0x003215B0.
    const char* goal_jump_kick_big; // "jumpkickbig", 0x003215D8.
    const char* goal_jump_kick; // "jumpkick", 0x003215E4.
    const char* goal_inner_force; // "innerforce", 0x003215F0.
    const char* goal_inner_force_out; // "innerforceoutof", 0x0031D008.
    const char* goal_random_move; // "randommove", 0x0031CFA4.
    const char* goal_random_walk; // "randomwalk", 0x003216C4.
    const char* goal_meditate; // "meditate", 0x003215CC.
    const char* goal_meditate_out; // "meditateoutof", 0x0031CFBC.
};
void imp_ninja_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B7A90
bool initialize_runtime_imp_ninja_strategy(std::span<u8> strategy,
    std::span<const u8> parameters); // Xbox 0x000B7B00
// The target record (mind +0x10) lies within the parameters' reach (+0).
bool runtime_imp_ninja_target_near(RuntimeImpNinjaStrategy& context); // Xbox 0x000B7FE0, ECX strategy
void update_runtime_imp_ninja_strategy(RuntimeImpNinjaStrategy& context); // Xbox 0x000B7B40, vtable +0x10
void dispatch_runtime_imp_ninja_message(std::span<const u32> message,
    RuntimeImpNinjaStrategy& context); // Xbox 0x000B7C50, vtable +0xC
void enter_runtime_imp_ninja_state(u16 state, u16 previous,
    RuntimeImpNinjaStrategy& context); // Xbox 0x000B7E60, vtable +0x14
void leave_runtime_imp_ninja_state(u16 state, RuntimeImpNinjaStrategy& context); // Xbox 0x000B7F20, vtable +0x18
}
