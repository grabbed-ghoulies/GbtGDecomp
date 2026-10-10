#pragma once
#include "body_resources.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeBodyBitStorage : EntityBitBlock
{
    u32 bit_count;
    u32 byte_count;
};
RuntimeBodyBitStorage* allocate_runtime_body_bits(u32 count); // Xbox 0x00066140
struct RuntimeBodyAnimationGroup
{
    u16 length;
    u16 count;
    const u8** values;
};
struct RuntimeBodyAnimationSetup
{
    std::span<const u8>& resource; // Body +0x5C.
    const std::unordered_map<const u8*, const u8*>& sequences; // Native views of relocated resource +0x10, keyed by CPU identity.
    RuntimeBodyAnimationGroup*& groups; // Body +0x64.
    u8& group_count; // Body +0x68.
    RuntimeBodyBitStorage*& bits; // Body +0x60.
    std::span<RuntimeBodyResourceCacheEntry> cache; // DAT_004DD554.
    i32& cache_count; // DAT_004DD550.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
RuntimeBodyAnimationGroup* retain_runtime_body_animation_groups(const u8* resource,
    u32& count, std::span<RuntimeBodyResourceCacheEntry> cache,
    i32 cache_count); // Xbox 0x0001B9E0
GhoulySpawnPool* accumulate_runtime_body_sequence_length(GhoulySpawnPool* pool,
    u32 length); // Xbox 0x0001C400
u32 runtime_body_animation_group_bytes(const GhoulySpawnPool& pool); // Xbox 0x0001C490
void initialize_runtime_body_animation_group_headers(RuntimeBodyAnimationGroup* groups,
    const GhoulySpawnPool& pool); // Xbox 0x0001C4C0
void build_runtime_body_animation_groups(RuntimeBodyAnimationSetup& context); // Xbox 0x0001C220
u32 maximum_runtime_body_animation_count(const RuntimeBodyAnimationSetup& context); // Xbox 0x0001C1C0
void acquire_runtime_body_animation(std::span<const u8> parameters,
    RuntimeBodyAnimationSetup& context); // Xbox 0x0001CB60
void initialize_runtime_body_animation_settings(std::span<u8> body,
    std::span<const u8> parameters, std::span<const u8> actor,
    std::span<const u8> class_rows, i32 class_count,
    RuntimeBodyAnimationSetup& context); // Xbox 0x0001D360
u32 count_runtime_body_model_records(const u8* data, u16 kind); // Xbox 0x00051CA0
u32 count_runtime_body_model_kind(const RuntimeBodyModelNodes* model,
    u16 kind); // Xbox 0x00040980
}
