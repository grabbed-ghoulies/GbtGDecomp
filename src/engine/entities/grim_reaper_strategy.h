#pragma once
#include <array>
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

// DAT_00302B7C, read through the strategy's +0x38. Nothing in the dumps writes it.
struct GrimReaperParameters
{
    float reach; // +0x0
    float unk_04; // +0x4
    float speed_step; // +0x8
    float unk_0C; // +0xC
    float turn_speed_step; // +0x10
    float turn_speed_limit; // +0x14
    float unk_18; // +0x18
    float unk_1C; // +0x1C
    float cue_time_low; // +0x20
    float cue_time_high; // +0x24
    float fly_time; // +0x28
    float pace_interval; // +0x2C
    float hit_slow; // +0x30
    float wind_up_time; // +0x34
};
static_assert(sizeof(GrimReaperParameters) == 0x38);

// entityStrategyGrimReaper (factory type 0x3F, getter 0x000AD3B0, vtable 0x00505DE0) over the base strategy (type 8):
// the Grim Reaper an event's failed time limit sends (0x000CF7B0). He chases his target (state 0x401) ever faster,
// pointing and laughing now and then; near enough he swings (0x40C), his hands' contact shapes live for the swing's
// second half, and a target that dies to him has its life sucked out (0x3F2). A hit slows him for a while; a landed one
// knocks him down (0x409, "knockeddown"). He plays the reaper stream while he lives and takes a blob shadow.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x48 the swing's callout 0x2E tokens.
struct GrimReaperStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_00302B7C, the block his tunables come from
    float turn_speed = 0.f; // +0x3C, the chase's +0x71C / +0x720; grows by the block's +0x10 to +0x14
    float speed = 0.f; // +0x40, the chase pace (0.5 at first); grows by the block's +8 up to 1
    float cue = 0.f; // +0x44, seconds to the next point or laugh, from the block's +0x20..+0x24
    std::array<u8, 0x24> events{}; // +0x48, callout 0x2E's tokens (0x0013B8B0)
    float fly = 0.f; // +0x6C, seconds the body's fly bit (+0x15C byte 7 bit 3) stays set
    float pace_time = 0.f; // +0x70, seconds to the next speed-up, from the block's +0x2C
    float slow = 0.f; // +0x74, a hit's slow-down left
    float slow_length = 0.f; // +0x78, and its whole length
    u32 slowed = 0; // +0x7C
    float held_speed = 0.f; // +0x80, the speed a hit slowed
    u32 grabbing = 0; // +0x84, the swing's contact shapes are on (0x000AD0D0 .. 0x000AD200)
    u32 swing = 0; // +0x88, 0 off, 1 winding up, 2 grabbing
    float wind_up = 0.f; // +0x8C, the block's +0x34
    u32 fed = 0; // +0x90, his target died to him (message 0x31)
    u32 appearing = 0; // +0x94, set until his first chase request finishes
    i32 shadow = 0; // +0x98, the blob shadow (0x00119A60), or -1
};
static_assert(sizeof(GrimReaperStrategyState) == 0x9C);
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
