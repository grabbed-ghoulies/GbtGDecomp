#pragma once
#include <array>
#include "actor_fields.h"
#include "trigger_actor.h"
#include "actor_clock.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct CutsceneRuntimeContext;
struct AudioEventVoices;

// entityAvatarGhoulySetupCutscene, factory type 0x25 (getter 0x0006B110, vtable 0x00504B08) over the actor (type 2):
// a ghoulybox whose spawns wait for a cutscene's spawn events (the snooker room's walk-ins). It keeps the actor's
// draw, visibility and +0x38 slots.
// Bytes: +0x934 the spawn tag (parameters +0x338), +0x938 parameters +0x3BC (the cutscene holds its last frame),
// +0x93C parameters +0x3C0 (the cutscene's camera mode), +0x940 the anchor actor (message 0x7D), +0x944 the ghoulybox
// load, +0x948 the cutscene, +0x94C its camera view (0x60 bytes), +0x9AC the "spawnId%d" camera name, +0x9D0 the
// cutscene has ended, +0x9D4 the cutscene runs, +0x9D8 spawns the cutscene's events have released.
// The setup's own parameters (0x3C4 bytes in all) and instance fields (0x9DC bytes in all, 0x0006B0F0), after the
// actor's. +0x944 the ghoulybox load, +0x948 the cutscene and +0x94C its camera view (0x60 bytes) are the
// RuntimeGhoulyCutsceneSetup views.
namespace ghouly_cutscene_parameter
{
constexpr RetailField<u32, 0x334> controller_key{}; // set: the setup's ghouly controller is registered under it
constexpr RetailField<u16, 0x338> spawn_tag{};
constexpr RetailField<std::array<char, 0x80>, 0x33A> cutscene{}; // the cutscene 0x0006B150 loads (0x0004D680)
constexpr RetailField<u32, 0x3BC> hold_last_frame{};
constexpr RetailField<u32, 0x3C0> camera_mode{};
constexpr u32 kSize = 0x3C4;
static_assert(retail_fits(kSize, camera_mode));
} // namespace ghouly_cutscene_parameter
struct GhoulyCutsceneState
{
    ActorBase base{}; // +0
    u16 spawn_tag = 0; // +0x934, the tag the cutscene's spawns get (0x0006B390)
    std::array<u8, 2> unk_936{}; // +0x936, padding
    u32 hold_last_frame = 0; // +0x938, set: the cutscene holds its last frame
    // +0x93C: 0 no camera, 2 always bind the cutscene's view; otherwise bind it until the cutscene ends.
    u32 camera_mode = 0;
    u32 anchor = 0; // +0x940, the anchor actor message 0x7D looks up by actor +0x1FE
    std::array<u8, 0x68> unk_944{}; // +0x944, never read or written by translated code
    std::array<char, 0x24> camera_name{}; // +0x9AC, "spawnId%d" (0x00323E08)
    u32 ended = 0; // +0x9D0, the cutscene has ended
    u32 running = 0; // +0x9D4, the cutscene runs
    u32 pending_spawns = 0; // +0x9D8, spawns the cutscene's events (event 0x13) have released
};
static_assert(sizeof(GhoulyCutsceneState) == 0x9DC);
struct RuntimeGhoulyCutsceneSetup
{
    std::span<u8> bytes;
    Actor& actor;
    GhoulyController& controller;
    u32& random;
    float occupancy_initial;
    GhoulySpawnRecord*& load; // +0x944, native pointer view.
    RuntimeCutsceneInstance*& cutscene; // +0x948, native pointer view.
    EntityCamNode& camera; // +0x94C, native view.
    std::function<CutsceneRuntimeContext&()> cutscenes; // The scene's cutscene runtime (0x0004D680).
    AudioEventVoices& voices; // DAT_004E0CE8, for 0x0004E640.
    std::function<u32()> self_address; // This actor's retail address.
    std::function<void(u32, std::span<u32>)> message; // An actor's actual vtable +0xC by address.
    const RuntimeActorClock& clock;
};
struct RuntimeGhoulyCutsceneSetupMessages
{
    RuntimeGhoulyCutsceneSetup& setup;
    ActorBaseContext& base;
    RuntimeMarkerObjects& objects;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<Actor&(FactoryObject&)> actor_of;
    std::function<FactoryObject*(FactoryObject)> keep; // Native scene ownership, not a creation callee.
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
};
void ghouly_cutscene_setup_object_sizes(ObjectSizes& sizes); // Xbox 0x0006B0F0
bool initialize_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context,
    std::span<const u8> parameters); // Xbox 0x0006B150
void destroy_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context); // Xbox 0x0006B350
i32 query_runtime_ghouly_cutscene_pose(RuntimeGhoulyCutsceneSetup& context, i32 select, Vec3& position,
    Vec3& euler); // Xbox 0x0006B9C0, EAX position
bool query_runtime_ghouly_cutscene_spawn_pose(RuntimeGhoulyCutsceneSetup& context, Vec3& source_position,
    Vec3& source_euler, Vec3& position, Vec3& euler); // Xbox 0x0006B390, EAX actor, EBX/ESI the sources
void release_runtime_ghouly_cutscene_spawn(std::span<u8> bytes); // Xbox 0x0006BA60, cutscene event 0x13
void dispatch_runtime_ghouly_cutscene_setup_message(RuntimeGhoulyCutsceneSetupMessages& context,
    std::span<u32> message); // Xbox 0x0006B4C0
void update_runtime_ghouly_cutscene_setup(RuntimeGhoulyCutsceneSetup& context); // Xbox 0x0006B8B0 after 0x000420D0
}
