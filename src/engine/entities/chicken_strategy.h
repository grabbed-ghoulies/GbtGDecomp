#pragma once
#include <array>
#include "strategy_fields.h"
#include "spider_strategy.h"

namespace gbtg
{
// DAT_00302B5C, read through the strategy's +0x38. Nothing in the dumps writes it.
struct ChickenParameters
{
    float provoke_reach; // +0x0, A class 7 hit this close arms the transform.
    float unk_04; // +0x4
    float unk_08; // +0x8
    float unk_0C; // +0xC
    float unk_10; // +0x10
    float unk_14; // +0x14
};
static_assert(sizeof(ChickenParameters) == 0x18);

// entityStrategyChicken (factory type 0x3A, getter 0x000A9FE0, vtable 0x00505D38; the factory table's row 0x39
// carries the name) over the base strategy (type 8): a farmyard chicken. It wanders far ("far") until it is hit by
// itself or by an actor of class 7 within the parameters' reach; then, once body bit 0x29 is up, it transforms (state
// 0x3EF, "transform"). It keeps the skeleton's sizes (0x000A9FC0, 0x40 / 0x18), the empty destructor 0x001628B0 and
// the base leave 0x000BFA50.
struct ChickenStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_00302B5C: +0 the reach a class 7 hit arms the transform within
    u32 provoked = 0; // +0x3C, A hit armed the transform
};
static_assert(sizeof(ChickenStrategyState) == 0x40);
struct RuntimeChickenStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    std::span<const u8> parameters; // DAT_00302B5C, 0x18 bytes.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_transform; // "transform", 0x003217CC.
};
bool initialize_runtime_chicken_strategy(std::span<u8> strategy); // Xbox 0x000AA030
void update_runtime_chicken_strategy(RuntimeChickenStrategy& context); // Xbox 0x000AA050, vtable +0x10
void dispatch_runtime_chicken_message(std::span<const u32> message,
    RuntimeChickenStrategy& context); // Xbox 0x000AA0F0, vtable +0xC
void enter_runtime_chicken_state(u16 state, RuntimeChickenStrategy& context); // Xbox 0x000AA1D0, vtable +0x14
}
