#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_attachment_cleanup.h"

namespace gbtg
{
// entityStrategyUglyGirl (factory type 0x53, getter 0x000C1320, vtable 0x00506010) over the base strategy (type 8):
// scene 11's Super Scary ugly girl. Its message is the base strategy's 0x0002BC10. Entering state 0x3EC it starts
// the named attachment (+0x3C, an "aid_" name; its node mode 4) and keeps its token (+0x38), then loads "attack";
// the update reloads "attack" while 0x3EC has a target and no mind requests. State 0x401 loads "far" and goes back
// to 0x3EC once a target is selected.
namespace ugly_girl_strategy_parameter
{
constexpr RetailRecord<0x18, 0x80> attachment{};
constexpr u32 kSize = 0x98;
} // namespace ugly_girl_strategy_parameter
struct UglyGirlStrategyState
{
    StrategyBase base{}; // +0
    u32 attachment_token = 0; // +0x38, the started attachment's +0x18, or 0
    std::array<char, 0x80> attachment{}; // +0x3C, parameters +0x18
};
static_assert(sizeof(UglyGirlStrategyState) == 0xBC);
struct RuntimeUglyGirlStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    ActorAttachmentList& attachments; // Actor +0x1AC.
    RuntimeBodyAttachmentLists& lists; // 0x0003CFD0's lists.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_far; // "far", 0x00321824.
};
void ugly_girl_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000C1300
bool initialize_runtime_ugly_girl_strategy(std::span<u8> strategy,
    std::span<const u8> parameters); // Xbox 0x000C1370
void update_runtime_ugly_girl_strategy(RuntimeUglyGirlStrategy& context); // Xbox 0x000C1390, vtable +0x10
// The tail jump to the base strategy message 0x0002BC10.
void dispatch_runtime_ugly_girl_message(std::span<const u32> message,
    RuntimeUglyGirlStrategy& context); // Xbox 0x000C1420, vtable +0xC
void enter_runtime_ugly_girl_state(u16 state, u16 previous,
    RuntimeUglyGirlStrategy& context); // Xbox 0x000C1430, vtable +0x14
void leave_runtime_ugly_girl_state(u16 state, RuntimeUglyGirlStrategy& context); // Xbox 0x000C14E0, vtable +0x18
}
