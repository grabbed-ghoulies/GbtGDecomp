#pragma once
#include <array>
#include "body_action_selection.h"
#include "body_death_effects.h"
#include "body_state_actions.h"
#include "body_state_fields.h"
#include "generate_ghouly_state.h"
#include "player_goal_cleanup.h"
#include "gbtg/object_factory.h"
#include <functional>

namespace gbtg
{
// entityStateSpawnGhouly (factory type 0x85, getter 0x000A1A10, vtable 0x00505B80) over the body state (type 9): the
// body holds still (input counters, +0x30, live holds and latches raised) and plays its spawn animation. Each time
// the body's bit 2 is set it takes the next ghouly from its box (actor +0xBA0) or spawn record (+0xB98) and makes it
// at the model's joint 0 (or the actor), placed in front of the body (0x000A1FD0). When the animation ends it plays
// it again while ghoulies remain, else the body's death finishes (0x00134AE0).
namespace spawn_ghouly_state_parameter
{
constexpr RetailField<u32, 0xC> animation{}; // The spawn animation's name (an XBE address)
constexpr u32 kSize = 0x10;
static_assert(retail_fits(kSize, animation));
} // namespace spawn_ghouly_state_parameter
struct SpawnGhoulyState
{
    BodyStateBase base{}; // +0
    u8 shape_mode = 0; // +0x18, 0x0001FEB0's previous mode, restored on destroy
    std::array<u8, 3> unk_19{}; // +0x19, padding
    u32 animation = 0; // +0x1C, the 0x0003AAA0 record; its native pointer is kept by state
};
static_assert(sizeof(SpawnGhoulyState) == 0x20);
struct RuntimeSpawnGhoulyStateContext
{
    RuntimeGenerateGhoulyScene& scene; // 0x000F40A0's spawns and the kept objects.
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<u8> actor_bytes;
    const EntityBitBlock& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions; // 0x0003AAA0 pairs and 0x0001AFA0.
    const RuntimeActorClock& clock;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<RuntimeBodyStateContext()> state; // 0x0001FEB0's views.
    GhoulySpawnRecord* spawn_record; // Actor +0xB98.
    std::span<ExecutableResourcePool> resources; // 0x00107720's templates.
    ObjectFactoryRegistry& factory;
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    u32& random; // DAT_0054DD7C.
    float occupancy_initial;
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses.
    std::function<void(u32 actor, std::span<const u32>)> message; // Actor vtable +0xC.
    RuntimeBodyDeathEffects& death; // 0x00134AE0.
    u32& scene_flags; // DAT_0050EA9C.
    u32& restart_requested; // DAT_00430D0C.
};
bool initialize_runtime_spawn_ghouly_state(SpawnGhoulyState& state, std::span<const u8> parameters,
    RuntimeSpawnGhoulyStateContext& context); // Xbox 0x000A1A70
void destroy_runtime_spawn_ghouly_state(SpawnGhoulyState& state,
    RuntimeSpawnGhoulyStateContext& context); // Xbox 0x000A1B60
void tick_runtime_spawn_ghouly_state(SpawnGhoulyState& state,
    RuntimeSpawnGhoulyStateContext& context); // Xbox 0x000A1BE0
// Type 0x85 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_spawn_ghouly_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSpawnGhoulyStateContext&(u32 body_address)> context_of);
}
