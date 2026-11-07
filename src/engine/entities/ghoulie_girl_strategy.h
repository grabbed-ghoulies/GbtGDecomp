#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

#include <functional>

namespace gbtg
{
// entityStrategyGhoulieGirl (factory type 0x3D, getter 0x000AC160, vtable 0x00505DA8; the factory table's row 0x3C
// carries the name) over the base strategy (type 8): Amber as a ghoulie. Holding (0x401) she first wakes ("wake", with
// the object her parameters name made beside her and body latch 0x3F held) for the record's +4 seconds, then wanders
// ("randomwander"); a target she can queue on sends her knocking it down (0x3EE, "knockdown", repeated by chance while
// it has energy, each time a second longer), after which she waits ("wake", 0x40D) out the knockdown time before
// holding again. A hit on her that lands while she may react sends her back to holding.
struct GhoulieGirlStrategyState
{
    StrategyBase base{}; // +0
    u32 record = 0; // +0x38, 0x00302B68 (ghoulie_girl_record)
    u32 mode = 0; // +0x3C, 0 none, 1 waking (object made), 2 wandering (0x000ABE80)
    u32 woken = 0; // +0x40, the first wake has run out
    float timer = 0.f; // +0x44, the wake, then the wait after a knockdown
    float knockdown_time = 0.f; // +0x48, the record's +8, one more per repeated knockdown
    u32 object = 0; // +0x4C, the object made for the wake (0x00107520)
    std::array<char, 0x80> object_name{}; // +0x50, parameters +0x18
};
static_assert(sizeof(GhoulieGirlStrategyState) == 0xD0);
namespace ghoulie_girl_strategy_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> object_name{};
constexpr u32 kSize = 0x98;
static_assert(retail_fits(kSize, object_name));
} // namespace ghoulie_girl_strategy_parameter
struct RuntimeGhoulieGirlStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    std::function<u32(const char*, u32)> create_object; // 0x00107720 / 0x00107520 with the actor at +8.
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const char* goal_wake; // "wake", 0x0031D4CC.
    const char* goal_wander; // "randomwander", 0x003217B0.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
};
void ghoulie_girl_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AC140
bool initialize_runtime_ghoulie_girl_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeGhoulieGirlStrategy& context); // Xbox 0x000AC1B0
void dispatch_runtime_ghoulie_girl_message(std::span<const u32> message,
    RuntimeGhoulieGirlStrategy& context); // Xbox 0x000AC250, vtable +0xC
void update_runtime_ghoulie_girl_strategy(RuntimeGhoulieGirlStrategy& context); // Xbox 0x000AC200, vtable +0x10
void enter_runtime_ghoulie_girl_state(u16 state, RuntimeGhoulieGirlStrategy& context); // Xbox 0x000AC3E0, vtable +0x14
void leave_runtime_ghoulie_girl_state(u16 state, RuntimeGhoulieGirlStrategy& context); // Xbox 0x000AC460, vtable +0x18
}
