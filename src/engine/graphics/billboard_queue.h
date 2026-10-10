#pragma once
#include "gbtg/types.h"
#include <array>

namespace gbtg
{
// One 0x28-byte entry of the camera-facing sprite queue at DAT_004EE330.
struct BillboardQueueEntry
{
    const u8* sprite; // +0, retail sprite resource.
    Vec3 position; // +4.
    std::array<float, 2> size; // +0x10.
    float rotation; // +0x18.
    u32 color; // +0x1C.
    u32 frame; // +0x20.
};
struct BillboardQueue
{
    std::array<BillboardQueueEntry, 0x800> entries{};
    i32 count = 0; // DAT_00504330.
};
extern BillboardQueue g_billboard_queue;
void queue_billboard(BillboardQueue& queue, const u8* sprite, u32 color, u32 frame, const Vec3& position,
    const float* size, float rotation); // Xbox 0x0005C1A0
}
