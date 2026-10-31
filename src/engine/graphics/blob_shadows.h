#pragma once
#include "gbtg/types.h"
#include <array>
#include <cstdint>
#include <functional>

namespace gbtg
{
struct RuntimeScenePointLightTargets;
// One of the scene's 0x20 blob shadows (DAT_0050EC10, 0x18 bytes each): a soft shadow the draw 0x00119AD0 puts under
// a point.
struct RuntimeBlobShadow
{
    Vec3 position; // +0.
    float radius = 0.f; // +0xC.
    float strength = 0.f; // +0x10.
    u32 active = 0; // +0x14 (DAT_0050EC24).
};
static_assert(sizeof(RuntimeBlobShadow) == 0x18);
struct RuntimeBlobShadows
{
    std::array<RuntimeBlobShadow, 0x20>& entries; // DAT_0050EC10.
    i32& count; // DAT_0050EF10.
    i32& next_free; // DAT_0050EF14.
    const u32& enabled; // DAT_0050EF18.
};
// ECX position, XMM0 radius, the stack strength: the slot taken, or -1 when shadows are off or all are in use.
i32 register_runtime_blob_shadow(const Vec3& position, float radius, float strength,
    RuntimeBlobShadows& shadows); // Xbox 0x00119A60
void advance_runtime_blob_shadow_free(RuntimeBlobShadows& shadows); // Xbox 0x00119950
struct RuntimeScenePointLightPass;
// With shadows on (DAT_0050EF18, the grid), some registered and DAT_0050EF1C clear: the light texture cleared to
// black with the frame modulated by the copy (DAT_0050486C 1, DAT_00504870 0), each active shadow drawn into it in
// white at its radius and strength, then the texture laid over the frame.
void draw_runtime_blob_shadows(const RuntimeBlobShadows& shadows, u32 suppress,
    RuntimeScenePointLightPass& pass); // Xbox 0x00119AD0
// The scene teardown's reset: every shadow off, the count, free slot and enable cleared, and the point-light pass's
// light and depth targets (DAT_00504850 .. DAT_0050485C) let go.
void reset_runtime_blob_shadows(std::array<RuntimeBlobShadow, 0x20>& entries, i32& count, i32& next_free,
    u32& enabled, RuntimeScenePointLightTargets& targets,
    const std::function<void(std::uintptr_t)>& release); // Xbox 0x001199F0
}
