#pragma once
#include "body_health.h"
#include "player_state.h"
#include "ghouly_spawn_record.h"

namespace gbtg
{
struct RuntimeBodyResourceHash
{
    u32 key;
    u32 first;
    u32 second;
};
struct RuntimeBodyLoadedResourceSize
{
    const u8* cpu; // Loaded resource +8.
    u32 secondary; // Raw loaded resource +0xC.
    const u8* storage; // Loaded resource +0x10; its first dword contributes when non-null.
    u32 tail; // Raw loaded resource +0x14.
};
u32 runtime_body_resource_size(const u8* resource,
    std::span<const AnimationResourceRecord> records,
    std::span<const RuntimeBodyLoadedResourceSize> loaded); // Xbox 0x00035D90
void acquire_runtime_body_resource_hashes(std::span<const u8> parameters,
    std::span<std::span<const u8>, 10> resources, std::span<u32, 10> counts,
    RuntimeBodyResourceHash*& hashes, u32& count, AnimationResourceRegistry& registry,
    std::span<const RuntimeBodyLoadedResourceSize> loaded, ResourceErrorState& error); // Xbox 0x0001E7C0
struct RuntimeBodyResourceCacheEntry
{
    u32 references;
    u32 first;
    const u8* second; // Retail +8, animation resource identity.
    void* resource;
};
void initialize_runtime_body_friction(std::span<u8> body, std::span<const u8> parameters,
    std::span<u8> actor, std::span<const u8> curve,
    std::span<const u8> tags); // Xbox 0x0001FA50
void acquire_runtime_body_attribute_table(std::span<u8> body,
    std::span<const u8> parameters, std::span<const u8>& resource,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000242E0
void initialize_runtime_body_model_channels(std::span<u8> body,
    const u8* model); // Xbox 0x0001ED70
void release_runtime_body_resource_hashes(std::span<std::span<const u8>, 10> resources,
    RuntimeBodyResourceHash*& hashes, u32& count,
    AnimationResourceRegistry& registry); // Xbox 0x0001E9F0
void reload_runtime_body_actions(std::span<u8> body, const char* name,
    FactoryObject& first, FactoryObject& second, std::span<const u8>& resource,
    ObjectFactoryRegistry& factory, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x00027740
bool release_runtime_body_cached_resource(void* resource,
    std::span<RuntimeBodyResourceCacheEntry> entries, i32& count); // Xbox 0x0001B920
// Retail 0x10-byte override row; the actor's +0x1F0 collision filter is Actor::model_flags.
struct RuntimeBodyCollisionOverride
{
    Actor* actor;
    u32 saved; // +4, the filter word before suspension.
    u32 retained; // +8, float age advanced by 0x0013ED20.
    u32 active; // +0xC, nonzero while the filter is suspended.
};
struct RuntimeBodyCollisionRecord
{
    std::array<u8, 0x1C> bytes; // +8 previous centre, +0x17 flags, +0x18 radius scale.
    const u8* shape; // +0x1C.
    RuntimeBodyCollisionRecord* next = nullptr; // Retail +4.
};
struct RuntimeBodyCollisionState
{
    u32 count;
    RuntimeBodyCollisionRecord* records;
    GhoulySpawnPool* overrides; // +8, RuntimeBodyCollisionOverride rows.
    u32 word_c;
    RuntimeBodyCollisionRecord* active; // +0x10, first record linked through record +4.
    u32 walking; // +0x14, set while 0x0013F7B0 runs.
    GhoulySpawnPool* removed; // +0x18, Actor* rows dropped from the overrides after a walk.
    u32 forced; // +0x1C, walks even when the frame step is zero.
};
RuntimeBodyCollisionState* create_runtime_body_collision_state(
    const RuntimeBodyModelNodes* model); // Xbox 0x0013F150
void restore_runtime_body_collision_overrides(GhoulySpawnPool& pool); // Xbox 0x0013EE80
void destroy_runtime_body_collision_state(RuntimeBodyCollisionState* state); // Xbox 0x0013F240
}
