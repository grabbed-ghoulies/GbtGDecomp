#pragma once
#include "player_strategy_goals.h"

namespace gbtg
{
// DAT_00511B48 actor list; each link is actor +0x944 with its next pointer at +4.
struct RuntimeActiveActorLink
{
    const RuntimeActiveActorLink* next; // +4.
    const RuntimeScriptMindQuery* body; // +0x18, actor +0x95C.
    RuntimePlayerGoalRing* const* goals; // +0x3C, actor +0x980.
};
struct RuntimeTutorialNotice
{
    const Actor* actor; // DAT_00506E04 + 8n.
    u32 id; // DAT_00506E08 + 8n.
};
// The four-entry first-sighting notice queue read by 0x000F4A00.
struct RuntimeTutorialNotices
{
    std::span<u8> seen; // *(DAT_00506C6C+8) bits; 0x000F4B90 sets a shown notice's bit.
    u32& pending; // DAT_00506E28.
    const Actor*& first; // DAT_00506DFC.
    u32& first_id; // DAT_00506E00.
    std::span<RuntimeTutorialNotice, 4> queued; // DAT_00506E04.
    i32& count; // DAT_00506E24.
    const Actor* const& player; // DAT_00511B40 minus 0x93C; null for the null and sentinel tokens.
};
// An actor's body (actor +0x95C) as the strategies read and hold it.
struct RuntimeStrategyBodyView
{
    u32 address;
    std::span<u8> bytes; // +0x50 holder actor address, +0x58, +0x15C bit block.
    const EntityBitBlock* bits; // +0x15C.
};
// Other actors as the 0x00139xxx reactions read them by retail address.
struct RuntimeStrategyActorViews
{
    std::function<Actor*(u32)> actor_at; // A scene actor by retail address.
    std::function<std::span<const u8>(u32)> actor_bytes_at; // Actor bytes by retail address; +0x8F8, +0x95C.
    std::function<RuntimeStrategyBodyView(const Actor&)> body_of; // Actor +0x95C.
    const RuntimeScriptActorQueryContext& query; // 0x00128DF0.
    const u32& story_flags; // DAT_0050EA98.
};
struct RuntimeStrategyTargetChecks
{
    RuntimePlayerStrategyGoals& goals;
    std::span<const u8>& attributes; // Actor +0x964.
    const i32& difficulty; // DAT_005064D8.
    const RuntimeActiveActorLink* const& active; // DAT_00511B48.
    RuntimeTutorialNotices& notices;
    float& occupancy_initial; // Retained caller occupancy scratch read by 0x0013AA50 flag 0x10.
};
RuntimeMindTarget* find_runtime_strategy_class_target(bool class_filter, float* distance,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00029AC0, EBX mind, EDX filter
RuntimeMindTarget* find_runtime_strategy_hostile_target(bool hostile_filter, float* distance,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00029BA0, EDX mind, EAX filter
bool select_runtime_strategy_target(RuntimeMindTarget*& output, u32 force,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00139AA0, EAX strategy
bool select_runtime_strategy_hostile_target(RuntimeMindTarget*& output, u32 force,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00139C70, EAX strategy, EDX force
Actor* runtime_mind_target_case_actor(const RuntimeMindTarget& target); // Xbox 0x00141450, EAX target
bool runtime_mind_target_case_unavailable(const RuntimeMindTarget& target); // Xbox 0x00141470, EAX target
// A player (+0xCDC 2, or +0x93C set) with +0xCE0 set, and the record's +0x14 clear.
bool runtime_strategy_target_watching_player(const RuntimeMindTarget& target); // Xbox 0x001414F0, EAX target
u32 runtime_actor_attack_limit(std::span<const u8> attributes); // Xbox 0x000196B0, EAX actor
bool roll_runtime_actor_attack(std::span<const u8> attributes, u32& random); // Xbox 0x000196F0, EAX actor
bool roll_runtime_actor_knockdown(std::span<const u8> attributes, u32& random); // Xbox 0x00019750, EAX actor
i32 count_runtime_energy_actors(const RuntimeActiveActorLink* first,
    std::span<const u8> address_zero); // Xbox 0x00129280
u32 runtime_strategy_abandons_target(const RuntimeMindTarget& target,
    RuntimeStrategyTargetChecks& context); // Xbox 0x00138410, EAX strategy, EDX target
bool runtime_message_names_mind_target(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at); // Xbox 0x001409C0, EDX message
bool runtime_message_hits_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at); // Xbox 0x00140980, ECX message, EDX actor
bool runtime_strategy_target_within(float range, const RuntimeMindTarget& target,
    const Actor& actor); // Xbox 0x0013A540, EBX target, ESI actor
void keep_nearer_tutorial_actor(RuntimeTutorialNotice& notice, const Actor& actor,
    const Actor* player); // Xbox 0x000F4E10, EBX notice, EDI actor
void queue_runtime_tutorial_notice(const Actor& actor, u32 id,
    RuntimeTutorialNotices& context); // Xbox 0x000F4870, EAX actor, EDX id
u32 notice_runtime_strategy_target(u32 id, RuntimeStrategyTargetChecks& context); // Xbox 0x0013A430, EAX strategy
u32 notice_runtime_strategy_hostile_target(u32 id,
    RuntimeStrategyTargetChecks& context); // Xbox 0x0013A4F0, ESI strategy
u32 resume_runtime_strategy_for_target(std::span<const u32> message,
    RuntimeStrategyTargetChecks& context); // Xbox 0x00138700, EAX strategy
u32 retreat_runtime_strategy_from_target(std::span<const u32> message,
    RuntimeStrategyTargetChecks& context); // Xbox 0x00138740, EAX strategy
u32 review_runtime_strategy_target(RuntimeStrategyTargetChecks& context); // Xbox 0x00138790, EAX strategy
u32 react_runtime_strategy_message(std::span<const u32> message,
    RuntimeStrategyTargetChecks& context); // Xbox 0x001384F0, EAX strategy, ECX message
u32 react_runtime_strategy_contact(std::span<const u32> message, float chance,
    RuntimeStrategyTargetChecks& context); // Xbox 0x001395C0, EAX strategy, EDX message, XMM1 chance
u32 react_runtime_strategy_cower(RuntimeStrategyTargetChecks& context); // Xbox 0x001398A0, EAX strategy
u32 react_runtime_strategy_startle(RuntimeStrategyTargetChecks& context); // Xbox 0x001399A0, EAX strategy
u32 retreat_runtime_strategy_from_reached_target(RuntimeStrategyTargetChecks& context); // Xbox 0x001383A0, EAX strategy
i32 runtime_mind_contact_count(const RuntimeMindEvents& events,
    u32 attacker); // Xbox 0x001372A0, EAX mind, ESI attacker
bool runtime_mind_watches_actor(const RuntimeMindEvents& events, u32 actor); // Xbox 0x001372F0, EAX mind, EDX actor
bool runtime_message_lands_on_actor(std::span<const u32> message, u32 actor); // Xbox 0x001408A0, EAX message, EDX actor
// The mind's target (+0x10) is the message's first actor (+4).
bool runtime_message_names_mind_target_actor(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at); // Xbox 0x001408C0, EAX actor, ECX message
// As 0x001408C0, and the message's second actor (+8) is `actor`.
bool runtime_message_from_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at); // Xbox 0x001408E0, ECX message, EDX actor
// The mind's target (+0x10) is the message's sender (+4) and the message landed (+0x10).
bool runtime_message_lands_from_mind_target(std::span<const u32> message, std::span<const u8> mind,
    const std::function<RuntimeMindTarget*(u32)>& target_at); // Xbox 0x00140910, EAX actor, ECX message
// The message names `actor` first (+4) and its second actor's target (+0x8F8, set) is the mind's target (+0x10).
bool runtime_message_source_targets_mind_target(std::span<const u32> message, u32 actor, std::span<const u8> mind,
    RuntimeStrategyActorViews& views); // Xbox 0x00140A00, EAX message, ECX actor
// The squared distance from the actor to the target record's point (+0x20).
float runtime_squared_target_distance(const Actor& actor, std::span<const u8> target); // Xbox 0x000415A0, ESI actor, EBX target
// The distance from the actor to the target record's point (+0x20).
long double runtime_target_distance(const Actor& actor, std::span<const u8> target); // Xbox 0x00041560, ESI actor, EBX target
// A running timer (above zero) counts down by the actor's step; true once it reaches zero, where it stops.
bool count_down_runtime_strategy_timer(float& timer,
    RuntimePlayerStrategyGoals& context); // Xbox 0x0002B9F0, EAX strategy, ECX timer
// 0x0002B9F0 held while a pause reference (DAT_0050E768) is open or the decay table's entry is off.
bool count_down_runtime_strategy_live_timer(float& timer,
    RuntimePlayerStrategyGoals& context); // Xbox 0x0002BA60, EAX strategy, EDX timer
// The message hits this actor (+4) and its attacker's target record (+8 -> +0x8F8) is free and eligible: the mind
// takes it as its target (+0x10) and the attacker as +0xC. `force` skips the strategy's pause count (+0x1C).
u32 adopt_runtime_strategy_message_attacker(std::span<const u32> message, u32 force,
    RuntimeStrategyTargetChecks& context, RuntimeStrategyActorViews& views); // Xbox 0x00139550, EAX strategy, EBX message, ECX force
// The inlined resume gate: strategy +0x1C clear, 0x0002B7F0 and an empty or idle goal ring (actor +0x980).
bool runtime_strategy_reactions_enabled(RuntimePlayerStrategyGoals& context);
float runtime_actor_body_gap(const Actor& first, const Actor& second,
    RuntimeStrategyActorViews& views); // Xbox 0x001363E0, EAX first, ECX second
u32 retaliate_runtime_strategy_contact(std::span<const u32> message, i32 minimum, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views); // Xbox 0x00139390
u32 runtime_strategy_target_hit_lands(std::span<const u32> message, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views); // Xbox 0x00139740, EAX strategy, EDI message
u32 react_runtime_strategy_watched_actor(std::span<const u32> message, RuntimeStrategyTargetChecks& context,
    RuntimeStrategyActorViews& views); // Xbox 0x001397B0, EAX strategy
RuntimePlayerMindRequest* request_runtime_strategy_position(const Vec3& position, std::span<const u8> settings,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00139FA0, EAX actor, EDI position, ESI settings
bool select_runtime_strategy_roam_position(Vec3& output, float heading, float range, float width,
    RuntimeStrategyTargetChecks& context); // Xbox 0x00138310, EAX actor, XMM0 width
RuntimePlayerMindRequest* request_runtime_strategy_roam(std::span<const u8> settings,
    RuntimeStrategyTargetChecks& context); // Xbox 0x0013A0A0, EAX settings, EBX actor
}
