#pragma once
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
namespace warlock_strategy_field
{
constexpr RetailField<u32, 0x38> record{}; // 0x003034F8
constexpr RetailField<u32, 0x3C> mode{}; // 1 .. 5 (0x000C3160), 0 outside holding
constexpr RetailField<float, 0x40> timer{}; // the mode's time left
constexpr RetailField<float, 0x44> notice_timer{}; // until callout 0x53
constexpr RetailField<float, 0x48> duration{}; // the mode's full time, for the fade and the stretch
constexpr RetailField<u32, 0x4C> model_flags{}; // actor +0x1F0 kept while out of the world
constexpr RetailField<i32, 0x50> model_mode{}; // actor +0x118 kept while out of the world
constexpr RetailField<u32, 0x54> shape_mode{}; // body +0x244
constexpr RetailField<u32, 0x58> hit{}; // a hit landed while holding (message 0x6E)
constexpr RetailField<u32, 0x5C> entering{}; // the first appearance: no hold, no message 0x26 / 0x27
constexpr RetailField<u32, 0x60> shown{}; // drawn (0x000C3440), hidden by 0x001364B0
constexpr RetailField<u32, 0x64> solid{}; // in the world's collision (0x001365C0), out by 0x00136540
constexpr RetailField<float, 0x68> height{}; // the actor's height when mode 1 began
constexpr RetailField<i32, 0x74> attacks{}; // attacks loaded since the attack began
constexpr RetailField<i32, 0x78> attack_limit{}; // the record's +0 .. +0xC by energy (0x000C3BE0)
constexpr RetailField<u32, 0x7C> entrance{}; // the entrance camera runs
// +0x80 .. +0xDF is the entrance camera's view (0x000496C0), held natively beside the strategy.
constexpr RetailField<Vec3, 0xE0> eye{}; // the display view's position when the entrance began
constexpr RetailField<float, 0xEC> entrance_timer{}; // the record's +0x50
constexpr RetailField<std::array<u8, 0x24>, 0xF0> first_events{}; // count +0, tokens after (0x0003CFD0)
constexpr RetailField<std::array<u8, 0x24>, 0x114> second_events{};
constexpr RetailField<std::array<char, 0x80>, 0x138> effect_name{}; // parameters +0x18
constexpr RetailField<u32, 0x1B8> effect{}; // the vanish's object (0x0003DF40)
constexpr RetailField<u32, 0x1BC> noticed{}; // callout 0x53 has run, or the notice is not wanted
constexpr u32 kSize = 0x1C0;
static_assert(retail_fits(kSize, noticed));
} // namespace warlock_strategy_field
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
