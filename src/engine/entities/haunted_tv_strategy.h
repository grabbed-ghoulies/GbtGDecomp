#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_attachment_requests.h"
#include "body_resources.h"
#include "ghouly_spawn_scheduler.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
// entityStrategyHauntedTV (factory type 0x44, getter 0x000B1EC0, vtable 0x00505E6C) over the base strategy (type 8).
// The haunted TV sleeps, wakes, shows static and the face of the ghouly in its box (actor +0xBA0), then lets one out
// (state 0x3EF, "generateghouly") or, near a target, plays its attack timeline: a list of {on, seconds} rows that
// switch its attack (latches, contact shapes, the body's 0x65 attack row and the effect object the parameters name)
// on and off. Its screen is the model's shader_blinn22 colour0 parameter, given one of six textures per mode.
// Strategy bytes: +0x1A attacking, +0x1B generating, +0x1C pause count.
namespace haunted_tv_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> timeline{}; // the attack timeline's AID
constexpr RetailField<std::array<char, 0x80>, 0x98> effect{}; // the object the attack makes
constexpr RetailField<std::array<char, 0x80>, 0x118> screen_0{}; // six screen textures' AIDs, 0x80 apart
constexpr u32 kScreenStride = 0x80;
constexpr u32 kSize = 0x498;
static_assert(retail_fits(kSize, screen_0));
} // namespace haunted_tv_parameter
namespace haunted_tv_strategy_field
{
constexpr RetailField<float, 0x38> repeat{}; // counts down from 0.5 s (0x00042450) in states 0x3FE and 0x416
constexpr RetailField<u32, 0x3C> announced{}; // set by message 0x3B
constexpr RetailField<u32, 0x44> generated{}; // ghoulies let out (state 0x3EF entered with none yet)
constexpr RetailField<u32, 0x48> generating{}; // raised with generated, dropped as 0x3EF ends
constexpr RetailField<float, 0x4C> near_time{}; // seconds near the target in state 0x3EE
constexpr RetailField<float, 0x50> recovery{}; // 1 s after a knock-down (state 0x409); no attack while it runs
constexpr RetailField<u32, 0x54> screen{}; // 1 click, 2 / 3 static, 4 the ghouly, 5 off, 0 the first texture
constexpr RetailField<u32, 0x58> screen_parameter{}; // the shader parameter (0x000F8540), or 0
constexpr RetailField<u32, 0x5C> screen_texture_0{}; // six textures, 4 apart, by screen mode
constexpr RetailField<u32, 0x74> ghouly_texture{}; // the box's ghouly's face (0x000B1D30)
constexpr RetailField<u32, 0x78> playing{}; // the attack timeline runs
constexpr RetailField<u32, 0x7C> attacking{}; // the attack is on (0x000B17B0 .. 0x000B1940)
constexpr RetailField<u32, 0x80> pending{}; // the timeline waits for the attack gate (0x000B1A90)
constexpr RetailField<std::array<char, 0x80>, 0x84> effect{};
constexpr RetailField<u32, 0x104> effect_object{}; // the attack's object, or 0
constexpr RetailField<u32, 0x108> timeline{}; // the timeline's rows, or 0
constexpr RetailField<u32, 0x10C> timeline_end{};
constexpr RetailField<u32, 0x110> timeline_row{};
constexpr RetailField<float, 0x114> row_time{};
constexpr RetailField<u32, 0x118> held{}; // set: the timeline does not advance
constexpr u32 kSize = 0x11C;
static_assert(retail_fits(kSize, held));
} // namespace haunted_tv_strategy_field
struct RuntimeHauntedTvStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    std::span<ExecutableResourcePool> resources;
    const char* const* object_tags; // DAT_0042A850, from objTag_Null.
    RuntimeActorSoundQueues& sounds;
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    GhoulySpawnRecord*& spawn_record; // Actor +0xB98.
    u8& spawn_active; // The native alias of actor +0xBA0's first byte.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    std::span<const u8>& attack_resource; // Body +0x5C.
    std::span<const RuntimeBodyLoadedResourceSize> loaded_sizes; // 0x00035D90's sizes.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    std::function<u32(const void*)> address_of; // Retail address of a native object or resource.
    std::function<u8*(u32)> texture_bytes; // A texture's bytes by its native address.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40's sequence: the object's address, or 0.
    std::function<bool(u32)> release_frame_object; // 0x0003DE30 over this actor's +0x1D8 objects.
    std::function<GhoulySpawnContext&()> spawns;
    std::function<std::span<const u8>()> story_bits; // **(DAT_00506C6C + 8), read by 0x000F29A0.
    const char* goal_generate; // "generateghouly", 0x003216E4.
    const char* goal_knocked_down; // "knocked_down", 0x003217F8.
    const char* goal_sleep; // "sleep", 0x0031D2F8.
    const char* goal_wakeup; // "wakeup", 0x0031D47C.
    const char* goal_wait; // "wait", 0x0031CB4C.
};
void haunted_tv_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B1EA0
bool initialize_runtime_haunted_tv_strategy(std::span<const u8> parameters,
    RuntimeHauntedTvStrategy& context); // Xbox 0x000B1F10
void destroy_runtime_haunted_tv_strategy(RuntimeHauntedTvStrategy& context); // Xbox 0x000B1F40
void update_runtime_haunted_tv_strategy(RuntimeHauntedTvStrategy& context); // Xbox 0x000B1F60, vtable +0x10
void dispatch_runtime_haunted_tv_message(std::span<const u32> message,
    RuntimeHauntedTvStrategy& context); // Xbox 0x000B23F0, vtable +0xC
void enter_runtime_haunted_tv_state(u16 state, u16 previous,
    RuntimeHauntedTvStrategy& context); // Xbox 0x000B2650, vtable +0x14
void leave_runtime_haunted_tv_state(u16 state, u16 next,
    RuntimeHauntedTvStrategy& context); // Xbox 0x000B2900, vtable +0x18
}
