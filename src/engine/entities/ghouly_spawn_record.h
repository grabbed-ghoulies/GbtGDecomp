#pragma once

#include "../scene/actor_loading.h"

namespace gbtg
{
// Native pointers replace the Xbox pool header's three address dwords.
struct GhoulySpawnPool
{
    u16 stride;
    u16 grow;
    u8* begin;
    u8* cursor;
    u8* limit;
};

// Field names retain the Xbox offsets. Fields not written by 0x000F3920
// remain uninitialized until the corresponding caller writes them.
struct GhoulySpawnRecord : ActorLoadRecord
{
    GhoulySpawnPool* pool_8;
    i32 remaining_c;
    u32 value_10;
    u32 value_14;
    u32 value_18;
    u32 value_1c;
    float timer_20;
    void* controller_24;
    i32 index_28;
    char attributes_30[0x80];
    u32 value_b0;
};

GhoulySpawnRecord* allocate_ghouly_spawn_record(); // Xbox 0x000F3920
void link_actor_load_record(ActorLoadRecord* head, ActorLoadRecord& record); // Xbox 0x000F3C30
void release_ghouly_spawn_record(GhoulySpawnRecord& record); // Xbox 0x000F3A00
}
