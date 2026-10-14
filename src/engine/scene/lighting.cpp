#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>

#include "lighting.h"

namespace gbtg
{
namespace
{

// DAT_00509afc. 0x000FD5C0 enables the pass when active is set and gate is clear.
// colour bytes are B, G, R, A (0x000FD5E3 reads them in that order).
struct LightVolumePass
{
    i32 active = 0;    // +0
    i32 gate = 0;      // +4
    u32 color = 0;     // +8 DAT_00509b04
    float scale = 0.f; // +0xc DAT_00509b08
    float range = 0.f; // +0x10 DAT_00509b0c
};

static_assert(offsetof(LightVolumePass, color) == 8);
static_assert(offsetof(LightVolumePass, range) == 0x10);

// DAT_00509c20, stride 0x25c. 0x000FE1D0 clears +0 and +0x1e8 and writes the
// group word at +0x1f0. Twenty-five records end at DAT_0050d71c.
struct LightVolumeRecord
{
    u32 head = 0; // +0
    u8 gap_4[0x1e8 - 4]{};
    u32 tail = 0; // +0x1e8
    u8 gap_1ec[4]{};
    u16 kind = 0; // +0x1f0
    u8 gap_1f2[0x25c - 0x1f2]{};
};

static_assert(sizeof(LightVolumeRecord) == 0x25c);
static_assert(offsetof(LightVolumeRecord, tail) == 0x1e8);
static_assert(offsetof(LightVolumeRecord, kind) == 0x1f0);

// DAT_0030ca80, three groups, stride 8. count is the dword; kind is the word.
struct LightVolumeGroup
{
    u32 count;
    u16 kind;
};

constexpr LightVolumeGroup kLightVolumeGroups[] = {
    {8, 0x100},
    {8, 0x80},
    {9, 0x40},
};

constexpr i32 kLightVolumeSlots = 0x28;
constexpr i32 kLightVolumeRecords = 8 + 8 + 9;
static_assert(kLightVolumeRecords == 0x19);
static_assert(std::bit_cast<u32>(1.f) == 0x3f800000u);
static_assert(std::bit_cast<u32>(1000.f) == 0x447a0000u);

// 0x0015AB90. Both arguments are words. ((height + 1) * width * 3) << 4,
// then rounded up to 32. A 16x16 grid is 0x3300 bytes.
constexpr u32 light_grid_bytes(u32 width, u32 height)
{
    const u32 cells = ((height & 0xffffu) + 1u) * (width & 0xffffu);
    const u32 bytes = (cells * 3u) << 4;
    return (bytes + 0x1fu) & ~0x1fu;
}

static_assert(light_grid_bytes(0x10, 0x10) == 0x3300u);

LightVolumePass g_light_pass{};
float g_light_tint[4]{};          // DAT_00509b20
float g_light_intensity = 0.f;    // DAT_00509b4c
float g_light_angle_inner = 0.f;  // DAT_00509b50. Pool float DAT_0032b160.
float g_light_angle_outer = 0.f;  // DAT_00509b54. Pool float DAT_0032b15c.
i32 g_light_mark = 0;             // DAT_00509b58
i32 g_light_paused = 0;           // DAT_00509b70
i32 g_light_running = 0;          // DAT_00509b74
i32 g_light_live = 0;             // DAT_00509b78
void* g_light_slots[kLightVolumeSlots]{}; // DAT_00509b7c

void* g_light_grid = nullptr; // DAT_004d9658
u32 g_light_grid_bytes = 0;   // DAT_004d965c
u8 g_light_grid_flag = 0;     // DAT_004d9660

using LightAllocFn = void* (*)(i32);
using LightFreeFn = void (*)(void*);
LightAllocFn g_light_alloc = nullptr; // DAT_0054d94c. Thunk at 0x000F9C10.
LightFreeFn g_light_free = nullptr;   // DAT_0054d954

LightVolumeRecord g_light_records[kLightVolumeRecords]{};
} // namespace

i32 g_light_filled = 0;                         // DAT_0050d71c. Compared with 0x19.
i32 g_light_filled_gate = 0;                    // DAT_0050d720
u8 g_light_order[kLightVolumeRecords]{};        // DAT_0050d724

namespace
{
i32 g_light_mark_740 = 0;                       // DAT_0050d740
u8 g_light_flag_760 = 0;                        // DAT_0050d760
u8 g_light_flag_761 = 0;
u8 g_light_flag_762 = 0;
u8 g_light_flag_763 = 0;                        // DAT_0050d763

void* g_light_table_slots[4]{}; // DAT_0050d770
i32 g_light_table_count = 0;    // DAT_0050d780
void* g_light_table = nullptr;  // DAT_0050d798
i32 g_light_table_span = 0;     // DAT_0050d79c. Byte count from 0x00035D90 divided by 140.

void light_volume_release(void* block)
{
    // 0x000D8AC0. EDI is the block. 0x000F7100 selects the heap, 0x0014D360
    // frees it, and the previous heap index is restored.
    engine_heap_free(block);
}

void light_clear_slots()
{
    // 0x000FDCD0. Writes the forty pointers, then the count.
    g_light_live = 0;
    for (i32 i = 0; i < kLightVolumeSlots; ++i)
    {
        g_light_slots[i] = nullptr;
    }
}

void light_init_records()
{
    // 0x000FE1D0. Each group from DAT_0030ca80 fills that many records.
    i32 index = 0;
    for (const LightVolumeGroup& group : kLightVolumeGroups)
    {
        for (u32 n = 0; n < group.count; ++n)
        {
            LightVolumeRecord& rec = g_light_records[index];
            rec.head = 0;
            rec.tail = 0;
            rec.kind = group.kind;
            index += 1;
        }
    }
    for (i32 i = 0; i < kLightVolumeRecords; ++i)
    {
        g_light_order[i] = static_cast<u8>(i);
    }
    g_light_filled = 0;
    g_light_filled_gate = 0;
    g_light_mark_740 = 0;
    g_light_flag_760 = 0;
    g_light_flag_761 = 0;
    g_light_flag_762 = 0;
    g_light_flag_763 = 0x50;
}

void light_bind_table()
{
    // 0x000FEF60. Clears the four slots and the count, then looks up
    // aid_misc_ghoulies_background_lightvolumetable (DAT_0030ca98) through
    // 0x00035E90. The open directory at DAT_004df250 is not in this file, so
    // the table stays null. 0x00035D90 returns 0 for that miss and the span
    // is that count divided by 140. A null lookup calls 0x00032700 with 2.
    for (i32 i = 0; i < 4; ++i)
    {
        g_light_table_slots[i] = nullptr;
    }
    g_light_table_count = 0;
    g_light_table = nullptr;
    g_light_table_span = 0;
}

} // namespace

// Xbox 0x000FD620
void lighting_initialize_volume()
{
    // Xbox 0x000FD620
    // 0x000FDCD0 clears the slot table. 0x0015AB90 sizes a 16x16 grid and
    // 0x00177980 allocates it (a zero protect word becomes 0xBC800000, then
    // 0x000F7270). 0x0015AC60 stores the block, 0x0015AC80 stores the allocator
    // thunk 0x000F9C10 (engine_heap_allocate), and 0x0015AC90 stores the releaser 0x000D8AC0.
    // The pass globals are planted around 0x000FE1D0 and 0x000FEF60.
    light_clear_slots();
    const u32 bytes = light_grid_bytes(0x10, 0x10);
    void* block = std::malloc(bytes);
    g_light_grid = block;
    g_light_grid_bytes = bytes;
    g_light_grid_flag = 0;
    g_light_alloc = &engine_heap_allocate;
    g_light_free = &light_volume_release;
    g_light_pass.active = 0;
    g_light_pass.gate = 0;
    g_light_pass.color = 0xffc0c0c0u;
    g_light_pass.scale = 1.f;
    g_light_pass.range = 1000.f;
    light_init_records();
    light_bind_table();
    g_light_tint[0] = 1.f;
    g_light_tint[1] = 1.f;
    g_light_tint[2] = 1.f;
    g_light_tint[3] = 1.f;
    g_light_intensity = 1.f;
    g_light_angle_inner = std::bit_cast<float>(0x3eb2b8c3u);
    g_light_angle_outer = std::bit_cast<float>(0x3f32b8c3u);
    g_light_mark = 1;
    g_light_paused = 0;
    g_light_running = 1;
}


} // namespace gbtg
