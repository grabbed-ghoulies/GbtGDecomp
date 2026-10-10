#pragma once

#include "gbtg/entity.h"

#include <bit>

namespace gbtg
{
namespace entity_detail
{
struct ShapeProbe;
struct LatchNode;
struct ListHead;
struct WorldLists;
struct SegmentProbe;
struct SegmentPool;
struct SegmentSlot;
struct SlideRecord;
}
using namespace entity_detail;

namespace entity_detail
{
inline constexpr float kLookEps = std::bit_cast<float>(0x3456BF95u);

// Object walked from DAT_00511B04. Retail stores the link at object+0x110 (prev)
// and +0x114 (next); the head value is the first link, and the walk follows +4.
// The push is vtable +0x34. The port keeps the object and its next pointer together.
struct ShapeProbe
{
    i32 (*push)(ShapeProbe& self, const Actor* actor, const Vec3* query, EntityCylinderBound& merged,
                EntityCylinderBound* shapes, i32 count, Vec3& out) = nullptr;
    ShapeProbe* next = nullptr;
    ShapeProbe* prev = nullptr; // retail object+0x110
};

// DAT_00511AA4. Nodes are walked through vtable +0xC when a latch mask leaves 0x3F.
struct LatchNode
{
    void (*handle_message)(LatchNode& self, const Message& msg) = nullptr;
    LatchNode* next = nullptr;
};

// Intrusive head. Retail prev is +0 and the first node is +4. Walks follow +4.
struct ListHead
{
    void* prev = nullptr;
    void* next = nullptr;
};

// DAT_00511AA0 through DAT_00511B58. cam_11c480's first call (0x001233C0) zeros
// this whole block. The eight bins at DAT_00511AA8 take the 0xC-byte records
// that start at object+0x124. Segment nodes sit at object+0x1e0, shape probes
// at object+0x110, and the later heads hang off +0x900, +0x918, +0x934, +0x93c,
// +0x944, +0x94c, and +0x908. DAT_00511B18 is the first node 0x00123020 walks.
struct WorldLists
{
    LatchNode* latch_prev = nullptr; // DAT_00511AA0
    LatchNode* latch = nullptr;      // DAT_00511AA4
    ListHead bins[8]{};              // DAT_00511AA8
    ListHead anchor{};               // DAT_00511AE8. Next is DAT_00511AEC.
    SegmentProbe* segment_prev = nullptr; // DAT_00511AF0
    SegmentProbe* segment = nullptr;      // DAT_00511AF4
    ListHead wide{};                      // DAT_00511AF8. Node is object+0x1e8.
    ShapeProbe* shape_prev = nullptr;     // DAT_00511B00
    ShapeProbe* shape = nullptr;          // DAT_00511B04
    ListHead view{};                      // DAT_00511B08. Node is object+0x900.
    ListHead actor_link{};                // DAT_00511B10. Node is object+0x918.
    void* release = nullptr;              // DAT_00511B18
    ListHead body_a{};                    // DAT_00511B1C. Node is object+0x934.
    ListHead body_b{};                    // DAT_00511B24. Node is object+0x934.
    ListHead body_c{};                    // DAT_00511B2C. Node is object+0x934.
    ListHead mark{};                      // DAT_00511B34
    ListHead case_head{};                 // DAT_00511B3C. Node is object+0x93c.
    ListHead alt{};                       // DAT_00511B44. Node is object+0x944.
    ListHead slot{};                      // DAT_00511B4C. Node is object+0x94c.
    ListHead child{};                     // DAT_00511B54. Node is object+0x908.
};

inline constexpr float kHitEps = std::bit_cast<float>(0x3727C5ACu);

// 0x0032B230, 1e-5
inline constexpr float kHitPad = std::bit_cast<float>(0x3C23D70Au);

// DAT_00511A18. 0x001226C0 clears this, then 0x00122C10 indexes the pair array
// with it. A hit stores the object at DAT_00511A1C + index*8 and increments it.
// The pool pointer sits in the next dword (DAT_00511A20 for index 0).
// Pairs run from DAT_00511A1C up to the list head at DAT_00511AF4.
inline constexpr i32 kSegmentSlots = (0x511AF4 - 0x511A1C) / 8;

// 0x10 header + 0x54 * 10

// Link walked from DAT_00511AF4. Retail stores the link at object+0x1e0 and the
// next pointer at +0x1e4. The query is vtable +0x20. The port keeps them together.
struct SegmentProbe
{
    i32 (*query)(SegmentProbe& self, Vec3& candidate, Vec3& kept, u32 param, i32 kind, u32 mask,
                 u8* record) = nullptr;
    SegmentProbe* next = nullptr;
    SegmentProbe* prev = nullptr; // object+0x1E0, set by 0x0003E810
    // Vtable +0x1C, called by 0x00122480. Kept last so the head sentinel's aliasing of
    // query/next is unchanged.
    i32 (*sweep)(SegmentProbe& self, Vec3& candidate, Vec3& kept, i32 kind, u32 mask,
                 u8* record) = nullptr;
    // Vtable +0x2C, called by 0x001229F0.
    i32 (*point)(SegmentProbe& self, Vec3& candidate, Vec3& kept, float radius, u32 mask,
                 u8* record) = nullptr;
    // Vtable +0x30, called by 0x00122B00 with the slot pool it may append to or grow.
    i32 (*contacts)(SegmentProbe& self, Vec3& origin, float radius, u32 mask,
                    SegmentPool& pool) = nullptr;
};

// Header of the 0x54-byte record pool. begin is the first record (block+0x10).
// 0x00072CB0 copies that record with 0x000FCD70 after a hit.
struct SegmentPool
{
    u16 stride = 0;
    u16 grow = 0;
    u8* begin = nullptr;
    u8* cursor = nullptr;
    u8* limit = nullptr;
    u8* block = nullptr;
    i32 block_size = 0;
    bool live = false;
};

struct SegmentSlot
{
    SegmentProbe* object = nullptr;
    SegmentPool pool{};
};

// 0x54-byte slide record. The normal 0x00072CB0 steps is at +0xC.
// The 0x28-byte match block 0x0010AE90 compares is at +0x28.
struct SlideRecord
{
    u8 prefix[0x0c]{};
    float x = 0.f; // +0xC
    float y = 0.f; // +0x10
    float z = 0.f; // +0x14
    u8 gap[0x10]{};
    EntityMatchBlock match{}; // +0x28
    u8 tail[4]{};
};

inline constexpr float kSlideFloor = 0.05f;

// 0x0032B250
inline constexpr i32 kSlideSlots = 5;
}

} // namespace gbtg
