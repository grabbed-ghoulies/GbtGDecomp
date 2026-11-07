#pragma once
#include <array>
#include "actor_factory_owner.h"
#include "body_attachment_cleanup.h"
#include "body_state_actions.h"
#include "player_strategy_goals.h"
#include "strategy_fields.h"
#include "strategy_target_selection.h"

#include <functional>

namespace gbtg
{
struct RuntimeActorCreationOptions;
struct RuntimeActorPauseGroups;
namespace entity_detail
{
struct DisplayView;
}

// entityStrategyWarlock (factory type 0x56, getter 0x000C3CC0, vtable 0x00506064; the factory table's row 0x55
// carries the name) over the base strategy (type 8): a warlock that shows himself, waits, vanishes (stretched tall
// and thin, fading out) and reappears somewhere else in a record of mask 0x20, then attacks a target it can select
// ("attackprepare", "attack" up to a count set by its energy, "attackfinish"). A hit while it holds sends it away
// sooner. Its first appearance can run the "warlock entrance" camera while the scene is paused.
// Holding (0x401) runs the modes of 0x000C3160: 1 shown and waiting, 2 vanishing, 3 gone (moved), 4 reappearing and
// 5 waiting to attack (0x3EE).
// Strategy bytes: +0x1A attacking, +0x1C pause count (states 0x3FC, 0x3FF and 0x415 leave it one lower).
struct WarlockStrategyState
{
    StrategyBase base{}; // +0
    u32 record = 0; // +0x38, 0x003034F8
    u32 mode = 0; // +0x3C, 1 .. 5 (0x000C3160), 0 outside holding
    float timer = 0.f; // +0x40, the mode's time left
    float notice_timer = 0.f; // +0x44, until callout 0x53
    float duration = 0.f; // +0x48, the mode's full time, for the fade and the stretch
    u32 model_flags = 0; // +0x4C, actor +0x1F0 kept while out of the world
    i32 model_mode = 0; // +0x50, actor +0x118 kept while out of the world
    u32 shape_mode = 0; // +0x54, body +0x244
    u32 hit = 0; // +0x58, a hit landed while holding (message 0x6E)
    u32 entering = 0; // +0x5C, the first appearance: no hold, no message 0x26 / 0x27
    u32 shown = 0; // +0x60, drawn (0x000C3440), hidden by 0x001364B0
    u32 solid = 0; // +0x64, in the world's collision (0x001365C0), out by 0x00136540
    float height = 0.f; // +0x68, the actor's height when mode 1 began
    std::array<u8, 8> unk_6C{}; // +0x6C, never read or written by translated code
    i32 attacks = 0; // +0x74, attacks loaded since the attack began
    i32 attack_limit = 0; // +0x78, the record's +0 .. +0xC by energy (0x000C3BE0)
    u32 entrance = 0; // +0x7C, the entrance camera runs
    std::array<u8, 0x60> unk_80{}; // +0x80, the entrance camera's view (0x000496C0), held natively beside the strategy
    Vec3 eye{}; // +0xE0, the display view's position when the entrance began
    float entrance_timer = 0.f; // +0xEC, the record's +0x50
    std::array<u8, 0x24> first_events{}; // +0xF0, count +0, tokens after (0x0003CFD0)
    std::array<u8, 0x24> second_events{}; // +0x114
    std::array<char, 0x80> effect_name{}; // +0x138, parameters +0x18
    u32 effect = 0; // +0x1B8, the vanish's object (0x0003DF40)
    u32 noticed = 0; // +0x1BC, callout 0x53 has run, or the notice is not wanted
};
static_assert(sizeof(WarlockStrategyState) == 0x1C0);
namespace warlock_strategy_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> effect_name{};
constexpr u32 kSize = 0x98;
static_assert(retail_fits(kSize, effect_name));
} // namespace warlock_strategy_parameter
struct RuntimeWarlockStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views;
    NativeActorFactoryInstance& actor_view; // Render bins, model anchors, live attachments and the colour (+0x198).
    RuntimeBodyAttachmentLists& lists; // 0x0003CFD0 / 0x0013B810.
    std::function<RuntimeBodyStateContext()> body_state; // 0x0001FEB0's views of the body.
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40: the object's address, or 0.
    // 0x0003BE20 over an actor's +0x8C callout records with the given creation options.
    std::function<void(u32, u32, const RuntimeActorCreationOptions*)> actor_callouts;
    std::function<void(const Actor&, float)> scare_near; // 0x0013B660.
    std::function<std::span<const u8>()> story_bits; // **(DAT_00506C6C + 8).
    std::function<RuntimeActorPauseGroups&()> pause; // 0x00117400.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    entity_detail::DisplayView* const& view; // DAT_005077A8.
    const char* goal_reappear; // "reappear", 0x00321280.
    const char* goal_attack_prepare; // "attackprepare", 0x0032128C.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_attack_finish; // "attackfinish", 0x0032129C.
    EntityCamNode entrance_view{}; // Strategy +0x80, the native view.
};
void warlock_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000C3CA0
bool initialize_runtime_warlock_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeWarlockStrategy& context); // Xbox 0x000C3D10
void destroy_runtime_warlock_strategy(RuntimeWarlockStrategy& context); // Xbox 0x000C3E90, vtable +8
void dispatch_runtime_warlock_message(std::span<const u32> message,
    RuntimeWarlockStrategy& context); // Xbox 0x000C3EF0, vtable +0xC
void update_runtime_warlock_strategy(RuntimeWarlockStrategy& context); // Xbox 0x000C3EA0, vtable +0x10
void enter_runtime_warlock_state(u16 state, u16 previous,
    RuntimeWarlockStrategy& context); // Xbox 0x000C40B0, vtable +0x14
void leave_runtime_warlock_state(u16 state, RuntimeWarlockStrategy& context); // Xbox 0x000C40F0, vtable +0x18
}
