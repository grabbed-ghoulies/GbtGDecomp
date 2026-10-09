#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
struct GhoulySpawnPool;
struct BackgroundShapeEvent
{
    const u8* source; // +0
    u32 flags; // +4
};
struct BackgroundRegion
{
    const u8* source = nullptr; // +0
    Vec3 first{}; // +4
    Vec3 second{}; // +0x10
    std::array<u8, 8> retained{}; // +0x1C
    float distance = 0.f; // +0x24
    u32 retained_28 = 0;
};

void actor_model_bounds_low(const Actor& actor, Vec3& output); // Xbox 0x0003DBF0
void actor_model_bounds_high(const Actor& actor, Vec3& output); // Xbox 0x0003DCA0
void initialize_background_volume(const Vec3& low, const Vec3& high); // Xbox 0x00118AE0
void append_background_region(std::span<BackgroundRegion> regions, i32& count,
    const u8* source); // Xbox 0x001198A0
u32 collect_background_shape_event(const u8* event,
    GhoulySpawnPool*& pool); // Xbox 0x00043DD0
}
