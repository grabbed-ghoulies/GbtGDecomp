#pragma once
#include "actor_fields.h"
#include "actor_base_messages.h"
#include "gbtg/object_factory.h"

namespace gbtg
{
// The area actor (0x95C bytes): where ghoulies may stand, and the last four cells it gave out.
struct AreaActorState
{
    ActorBytes base{}; // +0
    float unk_934 = 0.f; // +0x934, parameters +0x334
    float unk_938 = 0.f; // +0x938, parameters +0x338
    u32 unk_93C = 0; // +0x93C, parameters +0x33C
    u32 unk_940 = 0; // +0x940, parameters +0x340; the position's mode
    u32 unk_944 = 0; // +0x944, parameters +0x344
    std::array<u32, 4> recent_cells{}; // +0x948, -1 for none
    u32 recent_cursor = 0; // +0x958
};
static_assert(sizeof(AreaActorState) == 0x95C);

struct RuntimeAreaDialoguePool
{
    const u8* begin;
    const u8* end;
};
bool runtime_area_dialogue_active(const RuntimeAreaDialoguePool* pool); // Xbox 0x0005B830
void runtime_random_annulus_position(Vec3& output, const Vec3& origin,
    float maximum, float minimum, u32& random); // Xbox 0x00106670
void runtime_area_storage_sizes(ObjectSizes& sizes); // Xbox 0x0006DBB0
bool initialize_runtime_area_actor(FactoryObject& object, std::span<const u8> parameters); // Xbox 0x0006DC10
bool runtime_area_spawn_position(Vec3& output, const Vec3& origin, i32 floor,
    float minimum, float maximum, u32& random, float occupancy_initial); // Xbox 0x0006DF50
i32 select_runtime_area_position(std::span<u8> area, const Vec3& origin,
    Vec3& output, u32& random, float occupancy_initial); // Xbox 0x0006DCC0
struct RuntimeAreaActorContext
{
    std::span<u8> bytes;
    Actor& actor;
    ActorBaseContext& base;
    const i32& prompt_lock; // DAT_004E1C34.
    const RuntimeAreaDialoguePool*& dialogue; // DAT_004EE2B8.
    u32& random;
    float occupancy_initial;
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message;
    // Required pointer-width adapter into the translated 0x0003EF70.
};
i32 query_runtime_area_pose(RuntimeAreaActorContext& context, i32 select,
    Vec3& position, Vec3& euler); // Xbox 0x0006DDF0
void dispatch_runtime_area_message(RuntimeAreaActorContext& context,
    std::span<u32> message); // Xbox 0x0006DC80
}
