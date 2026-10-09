#pragma once
#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include <array>

namespace gbtg
{
// Native pointer fields map the Xbox pointers; unnamed storage is deliberately
// left as returned by the heap, matching the partial retail initialization.
struct RuntimeCutsceneInstance
{
    Vec3 position; // +0
    Vec3 scale; // +C
    Vec3 euler; // +18
    const u8* events; // +24
    i32 camera; // +28
    float time; // +2C
    float previous_time; // +30
    float duration; // +34
    u32 untouched_38;
    const u8* resource; // +3C
    i32 actor_count; // +40
    u8* actor_rows; // +44, retail rows have stride 0x30
    std::array<u8, 0x108> event_state; // +48..+14F
    std::array<u32, 6> pending; // +150..+164
    u32 flags; // +168
    float blend_time; // +16C
    float blend_duration; // +170
    Vec3 blend_position; // +174
    Quat blend_rotation; // +180
};
RuntimeCutsceneInstance* allocate_cutscene_instance(const u8* resource,
    i32 actor_count, float duration, u32 flags,
    void* (*allocate)(i32) = engine_heap_allocate); // Xbox 0x0004D560
FactoryObject create_cutscene_actor(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> pools, const char* parameters,
    const char* model, const Vec3& position, i32 secondary); // Xbox 0x0004DFA0
}
