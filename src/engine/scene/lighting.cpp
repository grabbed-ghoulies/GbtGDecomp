#include "gbtg/entity.h"
#include "gbtg/model_render.h"
#include "../scripts/script_vm.h"
#include "../camera/display_view.h"
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
#include "../audio/listener_settings.h"
#include "../entities/bounded_channels.h"
#include "../graphics/color_blend.h"
#include "../graphics/light_volume_records.h"
#include "background_ambient.h"

namespace gbtg
{
RuntimeSceneLightDirection g_scene_light_direction{.camera = []() -> const float*
    {
        return entity_detail::g_display_view->rotation_transform.data(); // DAT_005077A8 +0xD8
    }};

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
static_assert(kLightVolumeRecords == 8 + 8 + 9);
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
i32 g_light_live = 0;             // DAT_00509b78
void* g_light_slots[kLightVolumeSlots]{}; // DAT_00509b7c

void* g_light_grid = nullptr; // DAT_004d9658
u32 g_light_grid_bytes = 0;   // DAT_004d965c
u8 g_light_grid_flag = 0;     // DAT_004d9660

using LightAllocFn = void* (*)(i32);
using LightFreeFn = void (*)(void*);
LightAllocFn g_light_alloc = nullptr; // DAT_0054d94c. Thunk at 0x000F9C10.
LightFreeFn g_light_free = nullptr;   // DAT_0054d954
} // namespace

i32 g_light_paused = 0;                         // DAT_00509b70
i32 g_light_running = 0;                        // DAT_00509b74
Vec3 g_light_offset{};                          // DAT_0050d764
i32 g_light_filled = 0;                         // DAT_0050d71c. Compared with 0x19.
i32 g_light_filled_gate = 0;                    // DAT_0050d720
u8 g_light_order[kLightVolumeRecords]{};        // DAT_0050d724
RuntimeCandleFlameSpot g_light_candle_spot{};   // DAT_0050d740
std::array<u8, 4> g_light_volume_color{};       // DAT_0050d760

namespace
{

void* g_light_table_slots[4]{}; // DAT_0050d770
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
            rec.caster.model = nullptr;
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
    g_light_candle_spot.active = 0;
    g_light_volume_color = {0, 0, 0, 0x50};
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
    // DAT_00509B20..DAT_00509B2C, the ambient tint.
    g_scene_ambient_tint_0 = 1.f;
    g_scene_ambient_tint_1 = 1.f;
    g_scene_ambient_tint_2 = 1.f;
    g_scene_ambient_tint_3 = 1.f;
    g_scene_light_intensity = 1.f; // DAT_00509B4C
    g_scene_light_direction.pitch = std::bit_cast<float>(0x3eb2b8c3u); // DAT_00509B50, pool DAT_0032B160
    g_scene_light_direction.yaw = std::bit_cast<float>(0x3f32b8c3u); // DAT_00509B54, pool DAT_0032B15C
    g_scene_light_direction.camera_relative = 1; // DAT_00509B58
    g_light_paused = 0;
    g_light_running = 1;
}

namespace
{
// The next unit fraction of DAT_0054DD7C.
float next_light_unit(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
}

// (sin(the clock's place in the period, as a turn) + 1) * 0.5.
float light_swing(float time, float period)
{
    const long double place = runtime_float_remainder(time, period); // 0x00152BE0, DAT_004DEA60
    const long double sine = retail_sin(float(place / period * std::bit_cast<float>(0x40C90FDBu))); // DAT_0032B298
    return float((sine + 1.0L) * 0.5L); // DAT_0032B0F8, DAT_0032B0F4
}

// Xbox 0x000FDB20, AL slot
void flicker_runtime_light_color(u8* record, u32 frame, u32& random)
{
    LightRecord& light = light_record(record);
    // 0x001673D8, DAT_004DEA58
    if ((u32(truncate_runtime_float(light.colour_period)) & frame) != 0)
    {
        return;
    }
    const float fraction = next_light_unit(random);
    const u32 color = menu_color(light.colour_low,
        light.colour_high, fraction); // 0x00101730
    light.colour = color;
    light.flags |= 1;
}

// Xbox 0x000FDBA0
void swing_runtime_light_color(u8* record, float time)
{
    LightRecord& light = light_record(record);
    const float fraction = light_swing(time, light.colour_period);
    const u32 color = menu_color(light.colour_low,
        light.colour_high, fraction); // 0x00101730
    light.colour = color;
    light.flags |= 1;
}

// Xbox 0x000FDC20
void swing_runtime_light_range(u8* record, float time)
{
    LightRecord& light = light_record(record);
    const float fraction = light_swing(time, light.range_period);
    const float low = light.range_low;
    const float range = (light.range_high - low) * fraction + low;
    light.range = range;
    light.flags |= 1;
}
}

// Xbox 0x000FDDA0
void animate_runtime_light_records(RuntimeShaderRegistrationState& lights, u32 frame, float time, u32& random)
{
    for (size_t slot = 0; slot < lights.records.size(); ++slot) // Slots 0..0x27.
    {
        u8* record = lights.records[slot];
        if (record == nullptr)
        {
            continue;
        }
        LightRecord& light = light_record(record);
        if ((light.flags & 0x40) != 0)
        {
            flicker_runtime_light_color(record, frame, random); // 0x000FDB20, AL the slot
        }
        if (std::int8_t(light.flags) < 0
            && (u32(truncate_runtime_float(light.range_period)) & frame)
                == 0) // 0x001673D8, DAT_004DEA58
        {
            const float fraction = next_light_unit(random);
            const float low = light.range_low;
            const float range = (light.range_high - low) * fraction + low;
            light.range = range;
            light.flags |= 1;
        }
        if ((light.swing_flags & 2) != 0)
        {
            swing_runtime_light_color(record, time); // 0x000FDBA0
        }
        if ((light_record(lights.records[slot]).swing_flags & 4) != 0)
        {
            swing_runtime_light_range(lights.records[slot], time); // 0x000FDC20
        }
    }
}

// Xbox 0x000FDED0
void release_runtime_light_records(RuntimeShaderRegistrationState& shaders)
{
    for (i32 index = 0; index < 0x28; ++index)
    {
        if (shaders.records[size_t(index)] != nullptr)
        {
            // HeapFree when +0x66 bit 0x10 is clear. The host's records point into their model's shader storage
            // (0x000FDF60), which owns them, so the heap boundary has nothing to free.
            shaders.records[size_t(index)] = nullptr;
            --shaders.count;
        }
    }
    shaders.count = 0;
}

// Xbox 0x000FD710
void reset_runtime_light_environment(RuntimeShaderRegistrationState& shaders, RuntimeSceneFog& fog)
{
    release_runtime_light_records(shaders); // 0x000FDED0
    g_light_filled = 0; // DAT_0050D71C
    g_light_candle_spot.active = 0; // DAT_0050D740
    // DAT_00509AFC..DAT_00509B0C has two host views: the scene fog record the cutscene events and 0x000FD5C0 use,
    // and this file's light-volume pass. Both are reset.
    fog.enabled = 0; // DAT_00509AFC
    fog.disabled = 0; // DAT_00509B00
    g_light_pass.active = 0;
    g_light_pass.gate = 0;
    fog.start = 1.f; // DAT_00509B08 = DAT_0032B0F8
    g_light_pass.scale = 1.f;
    const auto light_color = runtime_background_ambient().light_color; // DAT_00509B3C
    light_color[3] = 1.f; // DAT_00509B48 = DAT_0032B0F8
    fog.end = 1000.f; // DAT_00509B0C = DAT_0032B164
    g_light_pass.range = 1000.f;
    const u32 color = 0xFFC0C0C0u; // DAT_00509B04 = DAT_00314F94
    std::memcpy(fog.color.data(), &color, 4);
    g_light_pass.color = color;
    light_color[2] = 0.f; // DAT_00509B44
    light_color[1] = 0.f; // DAT_00509B40
    light_color[0] = 0.f; // DAT_00509B3C
}

// Xbox 0x000FEA90, EAX the limit
void set_light_filled_gate(i32 limit)
{
    if (limit < 0)
    {
        g_light_filled_gate = 0; // DAT_0050D720
        return;
    }
    g_light_filled_gate = limit;
    if (0x18 < limit)
    {
        g_light_filled_gate = 0x19;
    }
}

// Xbox 0x000FD780, ESI the record
void apply_runtime_scene_lighting(std::span<const u8> record, RuntimeSceneFog& fog,
    const std::function<void()>& configure_fog)
{
    constexpr float unit = std::bit_cast<float>(0x3B808081u); // DAT_0032B154, 1/255
    constexpr float degrees = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC, pi/180
    g_scene_ambient_tint_2 = float(record[2]) * unit;
    g_scene_ambient_tint_1 = float(record[1]) * unit;
    g_scene_ambient_tint_0 = float(record[0]) * unit;
    g_scene_ambient_tint_3 = 1.f; // DAT_0032B0F8
    g_scene_light_intensity = read_f32(record, 4);
    g_scene_light_direction.pitch = read_f32(record, 0xC) * degrees; // DAT_00509B50
    g_scene_light_direction.yaw = read_f32(record, 0x10) * degrees; // DAT_00509B54
    g_scene_light_direction.camera_relative = read_u32(record, 8); // DAT_00509B58
    // DAT_00509AFC .. DAT_00509B0C: the scene fog record and this file's light-volume pass are both its views.
    fog.enabled = i32(read_u32(record, 0x14));
    g_light_pass.active = fog.enabled;
    if (fog.enabled != 0)
    {
        configure_fog(); // 0x000FD5C0
    }
    fog.start = read_f32(record, 0x18);
    g_light_pass.scale = fog.start;
    fog.end = read_f32(record, 0x1C);
    g_light_pass.range = fog.end;
    const u32 color = read_u32(record, 0x20);
    std::memcpy(fog.color.data(), &color, 4);
    g_light_pass.color = color;
}

// Xbox 0x000FE160, EAX the direction
void set_runtime_light_offset(const Vec3* direction)
{
    constexpr float reach = 30.f; // DAT_0032B108
    const Vec3 toward = direction == nullptr ? Vec3{0.f, 1.f, -1.f} : *direction; // DAT_0032B0F8, DAT_0032B120
    g_light_offset = {toward.x * reach, toward.y * reach, toward.z * reach}; // DAT_0050D764
}

} // namespace gbtg
