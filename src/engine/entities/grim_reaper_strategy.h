#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_attachment_requests.h"
#include "actor_attachment_events.h"
#include "../graphics/blob_shadows.h"

namespace gbtg
{
struct AudioBankStreams;
struct RuntimeBodyCollisionState;
struct RuntimeBodyAttachmentLists;
struct RuntimeActorSoundQueues;

// entityStrategyGrimReaper (factory type 0x3F, getter 0x000AD3B0, vtable 0x00505DE0) over the base strategy (type 8):
// the Grim Reaper an event's failed time limit sends (0x000CF7B0). He chases his target (state 0x401) ever faster,
// pointing and laughing now and then; near enough he swings (0x40C), his hands' contact shapes live for the swing's
// second half, and a target that dies to him has its life sucked out (0x3F2). A hit slows him for a while; a landed one
// knocks him down (0x409, "knockeddown"). He plays the reaper stream while he lives and takes a blob shadow.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x48 the swing's callout 0x2E tokens.
namespace grim_reaper_strategy_field
{
constexpr RetailField<u32, 0x38> parameters{}; // DAT_00302B7C, the block his tunables come from
constexpr RetailField<float, 0x3C> turn_speed{}; // the chase's +0x71C / +0x720; grows by the block's +0x10 to +0x14
constexpr RetailField<float, 0x40> speed{}; // the chase pace (0.5 at first); grows by the block's +8 up to 1
constexpr RetailField<float, 0x44> cue{}; // seconds to the next point or laugh, from the block's +0x20..+0x24
constexpr RetailField<std::array<u8, 0x24>, 0x48> events{}; // callout 0x2E's tokens (0x0013B8B0)
constexpr RetailField<float, 0x6C> fly{}; // seconds the body's fly bit (+0x15C byte 7 bit 3) stays set
constexpr RetailField<float, 0x70> pace_time{}; // seconds to the next speed-up, from the block's +0x2C
constexpr RetailField<float, 0x74> slow{}; // a hit's slow-down left
constexpr RetailField<float, 0x78> slow_length{}; // and its whole length
constexpr RetailField<u32, 0x7C> slowed{};
constexpr RetailField<float, 0x80> held_speed{}; // the speed a hit slowed
constexpr RetailField<u32, 0x84> grabbing{}; // the swing's contact shapes are on (0x000AD0D0 .. 0x000AD200)
constexpr RetailField<u32, 0x88> swing{}; // 0 off, 1 winding up, 2 grabbing
constexpr RetailField<float, 0x8C> wind_up{}; // the block's +0x34
constexpr RetailField<u32, 0x90> fed{}; // his target died to him (message 0x31)
constexpr RetailField<u32, 0x94> appearing{}; // set until his first chase request finishes
constexpr RetailField<i32, 0x98> shadow{}; // the blob shadow (0x00119A60), or -1
constexpr u32 kSize = 0x9C;
static_assert(retail_fits(kSize, shadow));
} // namespace grim_reaper_strategy_field
// DAT_00512170: the powerup table's DizzyReaper row (DAT_00512140 kind 2) +0 active, which 0x00123E70 sets; set,
// the reaper turns dizzy (0x3EA). A native scene points it at its row.
extern const u32* g_grim_reaper_dizzy;
struct RuntimeGrimReaperStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views; // Other actors by address (message 0x31's body +0x270).
    // DAT_00302B7C: +0 his reach, +8 the speed-up, +0x10 / +0x14 the turn speed's step and limit, +0x20..+0x24 the cue
    // time's range, +0x28 / +0x2C the fly and pace times, +0x30 a hit's slow-down per unit of speed, +0x34 the wind-up.
    const std::span<const u8>& parameters;
    u32 parameters_address; // 0x00302B7C, kept at +0x38.
    RuntimeActorSoundQueues& sounds;
    AudioBankStreams* const& streams; // DAT_004DF370; the scene binds it.
    RuntimeBlobShadows* const& shadows; // DAT_0050EC10; the scene binds it.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    std::span<const u8>& attack_resource; // Body +0x5C.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    std::function<u32(const void*)> address_of; // Retail address of a native object or record.
    const RuntimeActorResourceReference& resource; // Actor +0x8C, the callouts' record.
    ActorAttachmentEventContext& callouts;
    RuntimeBodyAttachmentLists& lists;
    std::function<void(u32, u8)> model_nodes; // 0x000F96F0 on the +0x204 and +0x310 models (stack slot, AL value).
    const char* goal_dizzy; // "dizzy", 0x00320980.
    const char* goal_knocked_down; // "knockeddown", 0x003217A4.
};
void grim_reaper_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AD390
bool initialize_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context); // Xbox 0x000AD400
void destroy_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context); // Xbox 0x000AD570
void update_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context); // Xbox 0x000AD600, vtable +0x10
void dispatch_runtime_grim_reaper_message(std::span<const u32> message,
    RuntimeGrimReaperStrategy& context); // Xbox 0x000AD7D0, vtable +0xC
void enter_runtime_grim_reaper_state(u16 state, u16 previous,
    RuntimeGrimReaperStrategy& context); // Xbox 0x000ADA70, vtable +0x14
void leave_runtime_grim_reaper_state(u16 state, u16 next,
    RuntimeGrimReaperStrategy& context); // Xbox 0x000ADB10, vtable +0x18
}
