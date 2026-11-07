#pragma once
#include "player_attachment.h"
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include <array>

namespace gbtg
{
// entityStrategyPirate (factory type 0x4E, getter 0x000BE0E0, vtable 0x00505F84; its name string follows the
// "entityStrategyPlayer" that row 0x4E names) over the base strategy (type 8): the pirate. He closes in (0x3EC,
// 0x40D) and attacks (0x3ED, 0x3EE's knockdown with a sword, 0x3EF's special) or fetches a chest to throw (0x405).
// Idle, he drinks (0x3FA); a hit while the body's bit 0x1A is up makes him angry (the parameters' +0x18 effect, all
// six body latches) and he throws at once. Hits from others, each within two seconds of the last, count in +0xD4;
// five or more force the knockdown with a sword.
struct PirateStrategyState
{
    StrategyBase base{}; // +0
    u32 chest = 0; // +0x38, the chest to fetch (0x00129C20, flags 0x1F), or the held one (0x7D)
    u32 fetching = 0; // +0x3C, state 0x405 found the chest free to take
    u32 wants_chest = 0; // +0x40, set by the constructor and message 0x3B; 0x405 clears it
    u32 drink_open = 0; // +0x44, state 0x3FA: the body's bit 0x1A, so a hit makes him angry
    float drink_delay = 0.f; // +0x48, seconds to the next drink, 5 + 5 * random
    std::array<char, 0x80> angry_effect{}; // +0x4C, parameters +0x18
    u32 angry_object = 0; // +0xCC, the angry effect (0x0003DF40); message 0x14 forgets it
    float hit_window = 0.f; // +0xD0, two seconds after each hit counted in +0xD4
    u32 hits = 0; // +0xD4, hits within the window; five or more force the knockdown
};
static_assert(sizeof(PirateStrategyState) == 0xD8);
struct RuntimePirateStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    RuntimePlayerAttachment& attachment; // Body +0x4C and +0x14 for 0x0001B6E0.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40 under scene ownership; object address or 0.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    std::function<u32(const Actor&)> actor_address; // Retail address of an actor.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_knockdown; // "attack_knockdown", 0x00321460.
    const char* goal_knockdown_confused; // "attack_knockdown_confused", 0x00321444.
    const char* goal_angry; // "angry", 0x003211EC.
    const char* goal_special; // "attack_special", 0x00321738.
    const char* goal_special_no_sword; // "attack_specialnosword", 0x0032142C.
    const char* goal_throw_chests; // "throw_multiplechests", 0x003213F4.
    const char* goal_throw_chest; // "throw_singlechest", 0x003213E0.
    const char* goal_drink; // "drink", 0x0031CC0C.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_get_weapon; // "getweapon", 0x00321420.
    const char* goal_get_weapon_confused; // "getweapon_confused", 0x0032140C.
};
void pirate_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000BE0C0
bool initialize_runtime_pirate_strategy(std::span<u8> strategy, std::span<const u8> parameters); // Xbox 0x000BE130
bool runtime_pirate_holds_sword(RuntimePirateStrategy& context); // Xbox 0x000BEB50, EAX strategy
// The weapon at strategy +0x38 is free to take: unparented, live (+0x93C), not held (+0xAD0) and settled (+0xAC4).
bool runtime_strategy_weapon_free(std::span<const u8> strategy,
    RuntimeStrategyActorViews& views); // Xbox 0x000BEB80, EAX strategy
bool runtime_pirate_chest_free(RuntimePirateStrategy& context); // 0x000BEB80 over the chest
void choose_runtime_pirate_attack(RuntimePirateStrategy& context); // Xbox 0x000BEBD0, EAX strategy
bool runtime_pirate_reacts(RuntimePirateStrategy& context); // Xbox 0x000BECF0, EAX strategy
void count_runtime_pirate_hit(std::span<const u32> message, RuntimePirateStrategy& context); // Xbox 0x000BED10
void update_runtime_pirate_strategy(RuntimePirateStrategy& context); // Xbox 0x000BE160, vtable +0x10
void dispatch_runtime_pirate_message(std::span<const u32> message,
    RuntimePirateStrategy& context); // Xbox 0x000BE520, vtable +0xC
void enter_runtime_pirate_state(u16 state, u16 previous, RuntimePirateStrategy& context); // Xbox 0x000BE820, vtable +0x14
void leave_runtime_pirate_state(u16 state, RuntimePirateStrategy& context); // Xbox 0x000BEA60, vtable +0x18
}
