#pragma once
#include "../scripts/script_event_step_fields.h"
#include "../audio/bank_streams.h"
#include "../frontend/scoreboard_messages.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_games.h"
#include "ghouly_spawn_record.h"
#include "intro_notices.h"
#include "player_mind_events.h"
#include "player_mind_targets.h"
#include "../scripts/script_actor_query.h"
#include "../scene/challenge_runtime.h"
#include "../navigation/cell_registration.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
// One row of the powerup table (DAT_00512140, stride 0x18, indexed by powerup kind 0..0x13).
struct RuntimePowerupRow
{
    u32 active; // +0.
    float amount; // +4.
    u32 extra; // +8.
    u32 player; // +0xC, actor address.
    i32 id; // +0x10.
    u32 message; // +0x14, scoreboard message id.
};
struct RuntimePowerupTable
{
    std::span<RuntimePowerupRow, 20> rows; // DAT_00512140.
    RuntimeScoreboard*& board; // DAT_00545ABC.
    AnimationResourceRegistry& registry;
};
void cancel_runtime_powerup(u32 kind, RuntimePowerupTable& table); // Xbox 0x00124320, EAX kind
// Adds time to the running challenge's op 0x10 and 0x11 requirements (DAT_004E17D8, their +8 seconds).
void extend_runtime_challenge_timers(float amount,
    std::vector<RuntimeChallengeRecord>* records); // Xbox 0x0004A370, XMM0 amount
// The scripted event step (DAT_005064E0, stride 0x88) whose type word at +0 matches, or null.
EventStep* find_runtime_event_step(u32 type,
    const entity_detail::NavigationCellRegistration* steps); // Xbox 0x000D0650, EDX type
// What 0x000F24C0 reaches when a challenge is won.
struct RuntimeChallengeAward
{
    RuntimeProfile& profile; // DAT_00506C54 (the challenge bits), DAT_00506C90 (the save name).
    RuntimeIntroNotices& notices; // 0x0013FF70, DAT_00545B38, DAT_00545B44.
    RuntimeBonusCounters& counters; // 0x0012D7E0
    RuntimeScoreboardResources& scoreboard;
    SaveGameContext& saves; // 0x0010DA80
};
// Sets challenge bit `index`, queues the first-bonus-book (one won) or first-challenge (five won) notice, saves the
// game when one is named, and shows the count on the bonus board; returns the challenges won.
i32 award_runtime_challenge(u32 index, RuntimeChallengeAward& context); // Xbox 0x000F24C0, ECX index
// A DAT_00511B48 actor as powerup kind 0x13 reads it.
struct RuntimePowerupActor
{
    Actor& actor;
    std::span<u8> bytes; // +0x93C, +0x978, +0x98C, +0xCDC.
    std::span<const u8> strategy; // Actor +0x954; empty when null.
    std::span<u8> mind; // Actor +0x958; empty when null.
    const RuntimePlayerGoalRing* ring; // Actor +0x980.
};
// The DAT_00511B48 walk of powerup kind 0x13.
struct RuntimePowerupActors
{
    std::function<u32()> first; // DAT_00511B48 as its actor's address (node - 0x944), or 0.
    std::function<u32(u32 actor)> next; // Node +4 as its actor's address, or 0.
    std::function<RuntimePowerupActor(u32 actor)> actor_at;
    const RuntimeScriptActorQueryContext& query; // 0x00028360.
    std::function<RuntimeMindTarget&(u32)> target_at; // Mind +0x10's record for 0x00029C80.
    std::function<void(u32 actor, std::span<const u32> message)> message; // Actor vtable +0xC.
    std::span<const u32, 2> blocked; // DAT_0050E764.
    std::span<const u8> modes; // DAT_0030F144.
    const u32& mode; // DAT_0051248C.
    std::span<const u8> address_zero;
};
// 0x001349E0's scene views: the spawn's templates and factory, the kept object's address and its vtable +0xC.
struct RuntimeActorPlacementSpawn
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources; // 0x00107720's templates.
    const void* const& scene_records; // DAT_0051108C, 0x0011F2E0's spawn types.
    std::function<u32(FactoryObject)> keep; // The scene keeps the object; its retail address, 0 for none.
    std::function<void(u32, std::span<const u32>)> message; // Vtable +0xC of the object at an address.
    std::function<std::span<u8>(u32)> bytes_at; // The kept object's bytes, for +0x990 and +0x998.
};
// 0x001347C0 makes the named actor; when it is made and `tag` is set it keeps `word` at +0x990, copies `attributes`
// to +0x998 (0x000280D0) and hears {0x4D, tag, position, euler, last}. Its address, or 0.
u32 spawn_runtime_placed_actor(const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* second_override, const char* fourth_override, const char* fifth_override, u32 tag, u16 word,
    const Vec3& message_position, const Vec3& message_euler, const u8* attributes, u32 last, i16 scene_key,
    RuntimeActorPlacementSpawn& context); // Xbox 0x001349E0, ECX fifth, EDX fourth
// Powerup kind 0xC's views: the spawn, the actors' positions, the new player's body bits and DAT_00512320's list.
struct RuntimeMiniCooperSpawn
{
    RuntimeActorPlacementSpawn& spawn;
    std::function<const Actor&(u32)> actor_at; // The awarded player, by retail address.
    std::function<EntityBitBlock&(u32)> body_bits; // A player's body (+0x95C) +0x15C bits, by actor address.
    i32& count; // DAT_00512320.
    std::span<u32, 4> actors; // DAT_00512324.
    std::span<float, 4> times; // DAT_00512334.
};
// What 0x00123E70 reaches.
struct RuntimePowerupAwardContext
{
    RuntimePowerupTable& table;
    RuntimePowerupCaption& caption; // 0x00132540
    RuntimeScoreboardResources& scoreboard; // 0x00132450
    RuntimeChallengeAward& challenge; // 0x000F24C0
    AudioBankStreams& streams; // 0x00038880
    std::span<const u8> sounds; // 0x003121D0: rows of 0x80 from index -1 (0x00312250 is row 0).
    std::function<std::vector<Actor*>()> marked_actors{}; // DAT_00511B38's actors (node - 0x934) in list order.
    RuntimePowerupActors* active_actors = nullptr; // DAT_00511B48, for kind 0x13.
    std::function<bool()> complete_event{}; // 0x000D0870, the scripted event sequence's completion.
    RuntimeMiniCooperSpawn* minicooper = nullptr; // Kind 0xC.
};
// Awards powerup `kind` (a row of DAT_00512140): its amount, extra word, player and id, the caption and its
// scoreboard message, the kind's own effect, its sound (stream 0x12) and, for a player, broadcast 0x86.
void award_runtime_powerup(i32 id, u32 player, u32 kind, float amount, u32 extra,
    RuntimePowerupAwardContext& context); // Xbox 0x00123E70, EDX id
}
