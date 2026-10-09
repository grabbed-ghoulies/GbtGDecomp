#pragma once

#include "ghouly_box_decoder.h"
#include "gbtg/anim.h"

namespace gbtg
{
struct GhoulySpawnMessage
{
    u32 id = 0xA7;
    char name[0x80]{};
    char variant[0x80]{};
    char attributes[0x80]{};
    char second_override[0x80]{};
    i32 result = 0;
};
struct GhoulyVariantMessage
{
    u32 id = 0xA9;
    char name[0x80]{};
    i32 result = 0;
};
struct GhoulySpawnContext
{
    u32& random_state; // DAT_0054DD7C
    u32& flags; // DAT_0050EA9C
    i32& maximum; // DAT_0050E568
    i32& minimum; // DAT_0050E56C
    i32& body_count; // DAT_00510578
    const u32* class_limits; // DAT_0050E570
    std::span<ExecutableResourcePool> resources;
};
i32 request_spawn_actor(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition,
    i32 force, GhoulySpawnContext& context); // Xbox 0x000F3E90
i32 request_spawn_variant(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition); // Xbox 0x000F3FC0
i32 request_spawn_alternate(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition); // Xbox 0x000F4030
i32 dispatch_next_ghouly_spawn(GhoulySpawnRecord& record, i32 force,
    GhoulySpawnContext& context); // Xbox 0x000F40A0
bool tick_ghouly_spawn_record(GhoulySpawnRecord& record, float delta,
    GhoulySpawnContext& context); // Xbox 0x000F3CF0
}
