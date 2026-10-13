#include "background_sectors.h"
#include "gbtg/anim.h"
#include "../audio/listener_settings.h"

#include <bit>
#include <cmath>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x00052660
void visit_runtime_sector_colors(std::span<u8> grid, const RuntimeSectorColorVisitor& visitor)
{
    for (u32 column = 0; column < read_u16(grid, 0x24); ++column)
    {
        const float x = float(column);
        for (u32 row = 0; row < read_u16(grid, 0x28); ++row)
        {
            const u32 index = u32(read_u16(grid, 0x24)) * row + column;
            const Vec3 position{x * read_f32(grid, 0x18) + read_f32(grid, 0), 0.f,
                float(row) * read_f32(grid, 0x20) + read_f32(grid, 8)};
            visitor(grid, position, grid[index * 3 + 0x2C], grid[index * 3 + 0x2D], grid[index * 3 + 0x2E]);
        }
    }
}

// Xbox 0x000FEAC0
void find_runtime_sector_settings(const char* name, RuntimeBackgroundSectors& state)
{
    for (i32 index = 0; index < state.settings_count; ++index)
    {
        const u32 offset = u32(index) * 0x8C;
        if (resource_names_match(reinterpret_cast<const char*>(state.settings.data() + offset), name))
        {
            state.saturation = read_f32(state.settings, offset + 0x80);
            state.low = read_f32(state.settings, offset + 0x84);
            state.high = read_f32(state.settings, offset + 0x88);
            return;
        }
    }
    state.saturation = 0.3f; // DAT_0032B268.
    state.low = 0.3f;
    state.high = 1.2f; // DAT_0032B24C.
}

// Xbox 0x000FEB70
void accumulate_runtime_sector_brightness(u8 first, u8 second, u8 third, RuntimeBackgroundSectors& state)
{
    constexpr float scale = std::bit_cast<float>(0x3B808081u); // DAT_0032B154.
    const float a = float(first) * scale;
    const float b = float(second) * scale;
    const float c = float(third) * scale;
    float maximum = a >= b ? a : b;
    maximum = maximum >= c ? maximum : c;
    if (state.minimum > maximum)
    {
        state.minimum = maximum;
    }
    if (maximum > state.maximum)
    {
        state.maximum = maximum;
    }
}

// Xbox 0x000FEBE0
void remap_runtime_sector_color(u8& first, u8& second, u8& third, RuntimeBackgroundSectors& state)
{
    constexpr float scale = std::bit_cast<float>(0x3B808081u); // DAT_0032B154.
    const float a = float(first) * scale;
    const float b = float(second) * scale;
    const float c = float(third) * scale;
    float minimum = b > a ? a : b;
    minimum = c > minimum ? minimum : c;
    float maximum = a >= b ? a : b;
    maximum = maximum >= c ? maximum : c;
    const float delta = maximum - minimum;
    float saturation;
    float hue;
    if (maximum != 0.f)
    {
        saturation = delta / maximum;
        if (a == maximum)
        {
            hue = (b - c) / delta;
        }
        else if (b == maximum)
        {
            hue = (c - a) / delta + 2.f;
        }
        else
        {
            hue = (a - b) / delta + 4.f;
        }
        hue *= 60.f;
        if (hue < 0.f)
        {
            hue += 360.f;
        }
    }
    else
    {
        hue = -1.f;
        saturation = 0.f;
    }
    saturation = state.saturation * saturation;
    float value;
    if (state.maximum != state.minimum)
    {
        const float fraction = (maximum - state.minimum) / (state.maximum - state.minimum);
        const float candidate = fraction * (state.high - state.low) + state.low;
        if (state.high > state.low)
        {
            if (candidate > state.high)
            {
                value = state.high;
            }
            else if (state.low > candidate)
            {
                value = state.low;
            }
            else
            {
                value = candidate;
            }
        }
        else
        {
            if (state.high > candidate)
            {
                value = state.high;
            }
            else if (candidate > state.low)
            {
                value = state.low;
            }
            else
            {
                value = candidate;
            }
        }
    }
    else
    {
        value = state.high;
    }
    if (_mm_comigt_ss(_mm_set_ss(1.f), _mm_set_ss(value)) == 0) // COMISS / JA.
    {
        value = 1.f;
    }
    float out_a;
    float out_b;
    float out_c;
    if (saturation == 0.f)
    {
        out_a = value;
        out_b = value;
        out_c = value;
    }
    else
    {
        hue *= std::bit_cast<float>(0x3C888889u); // DAT_0032B150.
        i32 sector = _mm_cvttss_si32(_mm_set_ss(hue));
        if (float(sector) > hue)
        {
            sector = std::bit_cast<i32>(u32(sector) - 1);
        }
        const float fraction = hue - float(sector);
        const float q = (1.f - fraction * saturation) * value;
        const float p = (1.f - saturation) * value;
        const float t = (1.f - (1.f - fraction) * saturation) * value;
        switch (u32(sector))
        {
        case 0:
            out_a = value;
            out_b = t;
            out_c = p;
            break;
        case 1:
            out_a = q;
            out_b = value;
            out_c = p;
            break;
        case 2:
            out_a = p;
            out_b = value;
            out_c = t;
            break;
        case 3:
            out_a = p;
            out_b = q;
            out_c = value;
            break;
        case 4:
            out_a = t;
            out_b = p;
            out_c = value;
            break;
        default:
            out_a = value;
            out_b = p;
            out_c = q;
            break;
        }
    }
    first = u8(truncate_runtime_float(static_cast<long double>(out_a) * 255.L));
    second = u8(truncate_runtime_float(static_cast<long double>(out_b) * 255.L));
    third = u8(truncate_runtime_float(static_cast<long double>(out_c) * 255.L));
}

// Xbox 0x000FEFD0
void register_runtime_background_sector(std::span<u8> grid, const char* name, RuntimeBackgroundSectors& state)
{
    state.grids[state.count] = grid;
    find_runtime_sector_settings(name, state);
    state.minimum = 999999.f; // DAT_0032B3F8.
    state.maximum = 0.f;
    visit_runtime_sector_colors(grid, [&](std::span<u8>, const Vec3&, u8& first, u8& second, u8& third)
    {
        accumulate_runtime_sector_brightness(first, second, third, state);
    });
    visit_runtime_sector_colors(grid, [&](std::span<u8>, const Vec3&, u8& first, u8& second, u8& third)
    {
        remap_runtime_sector_color(first, second, third, state);
    });
    ++state.count;
}

// Xbox 0x000FEF60
void load_runtime_sector_settings(RuntimeBackgroundSectors& state, AnimationResourceRegistry& registry,
    std::span<const RuntimeBodyLoadedResourceSize> loaded, ResourceErrorState& error)
{
    state.count = 0;
    for (auto& grid : state.grids)
    {
        grid = {};
    }
    const auto table = acquire_animation_resource(registry, "aid_misc_ghoulies_background_lightvolumetable");
    if (table.data() == nullptr)
    {
        resource_error(error, 2);
    }
    state.settings = table;
    const u32 size = runtime_body_resource_size(table.data(), registry.records, loaded);
    const u32 high = u32((u64(size) * 0xD41D41D5u) >> 32);
    state.settings_count = i32((((size - high) >> 1) + high) >> 7); // size / 0x8C.
}
}
