#pragma once
#include "gbtg/types.h"
#include <array>

namespace gbtg
{
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
}
