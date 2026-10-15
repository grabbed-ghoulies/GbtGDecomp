#pragma once
#include "gbtg/types.h"
#include <array>

namespace gbtg
{
struct RuntimeDialogueDrawContext;
struct DialogueOuterD3DBoundaries;
struct RuntimeRelocationImage;
}

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
    float distance; // +0x24, squared distance from the view (0x0005C390).
};
struct BillboardQueue
{
    std::array<BillboardQueueEntry, 0x800> entries{};
    i32 count = 0; // DAT_00504330.
    std::array<BillboardQueueEntry*, 0x800> order{}; // DAT_00502330, sorted far to near.
    std::array<float, 16> view{}; // DAT_00504338, the view transform the flush reads back.
};
struct BillboardFlushContext
{
    BillboardQueue& queue;
    RuntimeDialogueDrawContext& draw;
    DialogueOuterD3DBoundaries& outer;
    const RuntimeRelocationImage& image; // Boot shader rows (DAT_00427368 / DAT_00427374, DAT_00425EEC).
};
extern BillboardQueue g_billboard_queue;
void queue_billboard(BillboardQueue& queue, const u8* sprite, u32 color, u32 frame, const Vec3& position,
    const float* size, float rotation); // Xbox 0x0005C1A0
// Each queued billboard's squared distance from the view, then the order list sorted far to near.
void sort_billboards(BillboardQueue& queue, const Vec3& view); // Xbox 0x0005C390
void begin_billboard_draw(BillboardFlushContext& context); // Xbox 0x0005C410
// c0x60..c0x63: the rotation about the view axis and the view-space position (ESI).
void set_billboard_transform(const Vec3& position, float rotation, BillboardFlushContext& context); // Xbox 0x0005C580
// Binds the sprite frame (EAX, ECX the sprite) and its color; EBX receives its +8 extent, EDI its +0xA.
void bind_billboard_sprite(const u8* sprite, i32 frame, u32 color, float& width, float& height,
    BillboardFlushContext& context); // Xbox 0x0005C7F0
// One quad of local offsets (EAX the half size) with texel UVs: U to the sprite's +8 extent, V to its +0xA.
void push_billboard_quad(const float* size, float u_extent, float v_extent,
    BillboardFlushContext& context); // Xbox 0x0005C8C0
// Draws and empties the billboard queue for the current view.
void flush_billboards(BillboardFlushContext& context); // Xbox 0x0005C250
}
