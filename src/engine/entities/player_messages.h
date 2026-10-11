#pragma once
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "actor_base_messages.h"
#include "ghouly_spawn_scheduler.h"
#include "../navigation/cell_registration.h"
#include "../scripts/script_actor_query.h"

namespace gbtg
{
// Receiver of a sender pointer carried at message +4 (actual vtable +0xC).
struct RuntimeMessageReceiver
{
    u32 address;
    std::function<void(std::span<const u32>)> message;
};
// Messages 0xE and 0xF share this layout; 0xF only reads the sender.
struct RuntimePlayerContactMessage
{
    u32 id;
    RuntimeMessageReceiver* sender; // +4.
    std::array<u32, 6> words; // +8..+0x1C.
    u16 extra; // +0x20.
};
struct RuntimeOwnedMessage
{
    u32 id;
    u32 owner; // +4, retail actor address.
    u32 present; // +8.
};

u32 fade_runtime_player_animation_slot(AnimationController* controller,
    const ActorAnimationMessage& message); // Xbox 0x00019150
void reset_runtime_player_attribute_timer(RuntimeBodyAttachedActor& actor, u8 value); // Xbox 0x00019660
u32 runtime_player_attribute_word_28(std::span<const u8> attributes); // Xbox 0x000196D0
void relay_runtime_player_contact(const RuntimePlayerContactMessage& message, u32 actor_address); // Xbox 0x000294F0
void acknowledge_runtime_player_contact(RuntimeBodyAttachedActor& actor,
    const RuntimePlayerContactMessage& message); // Xbox 0x00029590
void accept_runtime_player_spawn(std::span<u8> actor, GhoulySpawnMessage& message,
    RuntimePlayerSpawnController& spawn); // Xbox 0x00028700
void accept_runtime_player_variant(std::span<u8> actor, GhoulyVariantMessage& message,
    RuntimePlayerSpawnController& spawn); // Xbox 0x00028770
void accept_runtime_player_alternate_variant(std::span<u8> actor, GhoulyVariantMessage& message,
    RuntimePlayerSpawnController& spawn); // Xbox 0x000287C0

struct RuntimePlayerStrategyResume
{
    std::span<const u32, 2> references; // DAT_0050E764, DAT_0050E768.
    std::span<const u8> decay_enabled; // DAT_0030F144.
    const u32& decay_index; // DAT_0051248C.
};
u8 runtime_player_can_resume_strategy(std::span<const u8> actor,
    const RuntimePlayerStrategyResume& context); // Xbox 0x00028D50
u8 runtime_player_strategy_decay_enabled(std::span<const u8> actor,
    const RuntimePlayerStrategyResume& context); // Xbox 0x0002B7F0
void change_runtime_player_strategy_state(FactoryObject& strategy, u16 state,
    RuntimePlayerStrategySetup& context); // Xbox 0x0002B8D0
void drain_runtime_player_goals(RuntimePlayerGoalCleanup& goals); // Xbox 0x00028210
struct RuntimePlayerPause
{
    Actor& actor; // Native view of actor +0xCD8 and the mask span read by 0x00028360.
    FactoryObject& strategy; // Actor +0x954.
    RuntimePlayerStrategySetup& setup;
    const RuntimeScriptActorQueryContext& query;
    RuntimePlayerGoalCleanup& goals;
    const RuntimePlayerStrategyResume& resume;
};
u32 handle_runtime_player_pause_message(std::span<u8> actor, u32 id,
    RuntimePlayerPause& context); // Xbox 0x00027FB0

struct RuntimePlayerNavigationRegistration
{
    std::span<const u8> body; // Actor +0x95C, scale at +0x888.
    const Actor& body_owner; // Body +8, radius at +0x38.
    entity_detail::NavigationCellRegistration*& records; // Actor +0xCE8.
};
void register_runtime_player_navigation(std::span<const u8> actor,
    RuntimePlayerNavigationRegistration& context); // Xbox 0x00029490

struct RuntimePlayerActivation
{
    RuntimeBodyAttachedActor& self;
    Actor& actor; // Native world transform and Actor +0x93C/+0xCC0 aliases.
    // Native list nodes for actor +0x934/+0x93C/+0x944/+0x94C. Node +0x934 is linked into
    // DAT_00511B34 and must be the link of an ActorWorldMessageNode.
    std::span<entity_detail::ListHead, 4> links;
    entity_detail::WorldLists& world;
    std::function<u32(const void*)> address_of; // Retail address of a list head or node.
    const i32& excluded_count; // DAT_0050E228.
    const u16* excluded_tags; // DAT_0050E22C.
    const OccupancyRecord*& room; // Actor +0xCA0.
    const OccupancyRecord*& allowed; // Actor +0xCA4.
    RuntimePlayerGoalRing*& ring; // Actor +0x980.
    std::span<const u8> address_zero;
    RuntimePlayerScriptOverrides& overrides;
};
void activate_runtime_player(RuntimePlayerActivation& context); // Xbox 0x00029270

struct RuntimePlayerMessageContext
{
    RuntimeBodyAttachedActor& self;
    ActorBaseContext& base;
    AnimationController* animation; // Actor +0x50.
    std::span<const u8>& attributes; // Actor +0x964.
    EntityBitBlock*& body_bits; // Body +0x15C.
    RuntimePlayerSpawnController& spawn;
    RuntimePlayerActivation& activation;
    RuntimePlayerPause& pause;
    RuntimePlayerNavigationRegistration& navigation;
};
void handle_runtime_player_message(void* message, RuntimePlayerMessageContext& context); // Xbox 0x00028FC0
struct RuntimePlayerChildMessages
{
    FactoryObject& strategy; // Actor +0x954.
    FactoryObject& mind; // Actor +0x958.
    FactoryObject& body; // Actor +0x95C.
    std::function<void(FactoryObject&, void*)> message; // Actual child vtable +0xC.
};
void dispatch_runtime_player_message(void* message, RuntimePlayerMessageContext& context,
    RuntimePlayerChildMessages& children); // Xbox 0x00028F60
}
