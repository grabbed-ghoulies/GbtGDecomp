#pragma once

#include "ghouly_spawn_record.h"

namespace gbtg
{
struct AnimationResourceRegistry;
struct ResourceErrorState;
// The five string slots keep their retail offsets and sizes.
struct GhoulySpawnDefinition
{
    char name[0x80];            // +0
    char variant[0x80];         // +0x80
    float weight;              // +0x100
    char first_override[0x80];  // +0x104
    char second_override[0x80]; // +0x184
    char alternate[0x80];       // +0x204
    u32 flags;                 // +0x284
};

GhoulySpawnRecord* load_ghoulybox(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state); // Xbox 0x000F39A0

GhoulySpawnPool* grow_ghouly_spawn_pool(GhoulySpawnPool* pool); // Xbox 0x000662A0
void append_ghouly_spawn_definition(GhoulySpawnRecord& record, const char* name,
    const char* variant, float weight, const char* first_override,
    const char* second_override, u32 flags); // Xbox 0x000F3AB0
void append_ghouly_spawn_variant(GhoulySpawnRecord& record, const char* variant); // Xbox 0x000F3B60
void append_ghouly_spawn_alternate(GhoulySpawnRecord& record, const char* alternate); // Xbox 0x000F3BD0
void decode_ghoulybox(GhoulySpawnRecord& record, std::span<const u8> resource,
    u32& random_state); // Xbox 0x000517B0
}
