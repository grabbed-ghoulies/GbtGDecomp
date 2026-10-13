#pragma once

#include "background_geometry.h"
#include "../entities/entity_types.h"
#include "../entities/model_sound_lifetime.h"

namespace gbtg
{
struct BackgroundAnimationAllocation
{
    u8* allocation; // Row +0x28, stride 0x1E0.
};
struct RuntimeBackgroundLifetime
{
    GhoulySpawnPool* events; // +0x9E4.
    u8* first_push_buffer; // +0x9D8.
    u8* second_push_buffer; // +0x9DC.
    i32 has_push_buffer; // +0x9E0.
    RuntimeModelSoundObject* auxiliary; // +0x940.
    u8* volumes; // +0x94C.
    u8* animation; // +0x950.
    entity_detail::ListHead link; // +0x934 / +0x938.
    i32 disables_clear; // +0x93C.
};
struct RuntimeBackgroundRelease
{
    // 0x00044620 reads the first background's model count and rows, regardless
    // of which background is being destroyed.
    u8 first_animation_count;
    std::span<BackgroundAnimationAllocation> first_animation_rows;
    i32& disable_framebuffer_clear; // DAT_004DEA50.
    RuntimeModelSoundRelease& model_sound;
    const std::function<void(u8*)>& heap_free;
};
void restore_background_shape_event_flags(GhoulySpawnPool*& pool,
    const std::function<void(u8*)>& heap_free); // Xbox 0x00043D50
void release_background_animation_allocations(u8 count,
    std::span<BackgroundAnimationAllocation> rows,
    const std::function<void(u8*)>& heap_free); // Xbox 0x00044620
void destroy_runtime_background(RuntimeBackgroundLifetime& background,
    RuntimeBackgroundRelease& services); // Xbox 0x000430C0
}
