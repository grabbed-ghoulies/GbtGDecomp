#pragma once
#include "intro_notices.h"
#include "player_attachment.h"
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_resources.h"
#include "gbtg/resource_error.h"
#include <array>

namespace gbtg
{
// entityStrategyBaron (factory type 0x37, getter 0x000A7800, vtable 0x00505D00; its name string follows the
// "entityStrategyBaron" that row 0x36 names) over the base strategy (type 8): the Baron, chapter 4's boss. Aloof
// (0x3EA) he waits for a target in front of him, then attacks with his cane (0x3EE) or fetches it first (0x402); a
// knock-down (0x409) or a target picking up his cane (0x411) leaves him recovering, and with the cane in a target's
// hands he chases it (0x3F4). Three timelines, one per energy tier (body +0x94 above 180, 161 to 180, or less),
// switch an effect (the parameters' +0x18 object, holding all six body latches while it lives) on and off.
namespace baron_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> effect{}; // the effect object's template
constexpr RetailField<std::array<char, 0x80>, 0x98> timeline_0{}; // three timelines' AIDs, 0x80 apart, by tier
constexpr u32 kTimelineStride = 0x80;
constexpr u32 kSize = 0x218;
static_assert(retail_fits(kSize, timeline_0) && 0x98 + 3 * kTimelineStride == kSize);
} // namespace baron_parameter
struct BaronStrategyState
{
    StrategyBase base{}; // +0
    u32 cane = 0; // +0x38, the cane actor (messages 0x5E and 0x7D), or 0
    u32 hit_target = 0; // +0x3C, the mind target was hit by him (message 0x6E); 0x3EE celebrates
    u32 angry = 0; // +0x40, set from 0x3EB near the target: 0x3EE starts the effect at once
    float aloof_time = 0.f; // +0x44, 2 s on entering 0x3EA; the target must stay in view for it
    float recovery = 0.f; // +0x48, 1 s after the first knock-down or cane notice; holds 0x409 / 0x411
    u32 knockdown_notice = 0; // +0x4C, "chapter4_baron_firstknockdown" queued (0x0013FF00)
    u32 cane_notice = 0; // +0x50, "chapter4_baron_canepickedup" queued
    u16 after_fetch = 0; // +0x54, the state 0x402 moves on to once he holds the cane
    std::array<u8, 2> unk_56{}; // +0x56, padding
    u32 effect_mode = 0; // +0x58, 0 off, 1 on, 2 by the tier's timeline
    float linger = 0.f; // +0x5C, 0.25 s the effect outlives a mode ending
    u32 effect = 0; // +0x60, the effect object (0x0003DF40), or 0
    std::array<char, 0x80> effect_name{}; // +0x64
    float row_time = 0.f; // +0xE4
    u32 tier = 0; // +0xE8
    u32 row = 0; // +0xEC, the timeline row: {u32 on, float seconds}
    std::array<u32, 6> timelines{}; // +0xF0, three {start, end} pairs, by tier
};
static_assert(sizeof(BaronStrategyState) == 0x108);
struct RuntimeBaronStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views;
    RuntimePlayerAttachment& attachment; // 0x0001B810's attachment.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    std::span<const RuntimeBodyLoadedResourceSize> loaded_sizes; // 0x00035D90's sizes.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<u32(const void*)> address_of; // Retail address of a native object or resource.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40 under scene ownership; object address or 0.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    std::function<RuntimeIntroNotices()> intro; // 0x0013FF00's gate and storage.
    const char* goal_fetch_cane; // "fetch_cane", 0x003211C8.
    const char* goal_aloof_prepare; // "aloof_prepare", 0x00321854.
    const char* goal_aloof; // "aloof", 0x00321864.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_dizzy; // "dizzy", 0x00320980.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_get_weapon; // "get_weapon", 0x00321818.
    const char* goal_knocked_down; // "knocked_down", 0x003217F8.
    const char* goal_celebrate; // "celebrate", 0x0031B548.
    const char* goal_worried; // "worried", 0x0031B538.
};
void baron_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000A77E0
// 0x000A7850 calls it with EAX the parameters and the strategy on the stack, then returns 1.
void initialize_runtime_baron_strategy(std::span<const u8> parameters, RuntimeBaronStrategy& context); // Xbox 0x000A73A0
void destroy_runtime_baron_strategy(RuntimeBaronStrategy& context); // Xbox 0x000A7440, EAX strategy (0x000A7870)
void tick_runtime_baron_effect(RuntimeBaronStrategy& context); // Xbox 0x000A7470, EAX strategy
void end_runtime_baron_effect(RuntimeBaronStrategy& context); // Xbox 0x000A75B0, EAX body, EDI strategy
void start_runtime_baron_effect(RuntimeBaronStrategy& context); // Xbox 0x000A76C0, EAX body, ECX strategy
// The tier's {0, speed, fetch speed, drop radii} row: 0x00302A60, 0x00302A74 or 0x00302A88.
std::span<const float, 5> runtime_baron_tier_row(std::span<const u8> body); // Xbox 0x000A8870, EAX body
u32 runtime_baron_tier(std::span<const u8> body); // Xbox 0x000A88A0, EAX body
// The target (EDX) holds his cane (+0x38), which is still live (+0x93C); a dead cane is forgotten.
bool runtime_baron_target_holds_cane(const RuntimeMindTarget& target,
    RuntimeBaronStrategy& context); // Xbox 0x000A8700, ECX strategy
// Holding something (body +0x48) he enters `state`; without, he fetches the cane (0x402) and enters it after.
void fetch_runtime_baron_cane_for(u16 state, RuntimeBaronStrategy& context); // Xbox 0x000A8750 (0x3EA), 0x000A87E0 (0x3EE)
void update_runtime_baron_strategy(RuntimeBaronStrategy& context); // Xbox 0x000A7880, vtable +0x10
void dispatch_runtime_baron_message(std::span<u32> message, RuntimeBaronStrategy& context); // Xbox 0x000A7D30, vtable +0xC
void enter_runtime_baron_state(u16 state, u16 previous, RuntimeBaronStrategy& context); // Xbox 0x000A80B0, vtable +0x14
void leave_runtime_baron_state(u16 state, RuntimeBaronStrategy& context); // Xbox 0x000A8590, vtable +0x18
// The target (EBX) is within `radius` and within `half_angle` of the actor's facing (EAX the strategy's body).
bool runtime_target_in_view(const Actor& actor, std::span<const u8> target, float half_angle,
    float radius); // Xbox 0x00134430
}
