#pragma once

#include "gbtg/types.h"
#include "../entities/body_resources.h"
#include <functional>
#include <span>

namespace gbtg
{
struct RuntimeBackgroundSectors
{
    std::span<std::span<u8>> grids; // DAT_0050D770.
    i32& count; // DAT_0050D780.
    float& minimum; // DAT_0050D784.
    float& maximum; // DAT_0050D788.
    float& saturation; // DAT_0050D78C.
    float& low; // DAT_0050D790.
    float& high; // DAT_0050D794.
    std::span<const u8> settings; // DAT_0050D798, stride 0x8C.
    i32 settings_count; // DAT_0050D79C.
};
using RuntimeSectorColorVisitor = std::function<void(std::span<u8>, const Vec3&, u8&, u8&, u8&)>;
void visit_runtime_sector_colors(std::span<u8> grid,
    const RuntimeSectorColorVisitor& visitor); // Xbox 0x00052660
void find_runtime_sector_settings(const char* name, RuntimeBackgroundSectors& state); // Xbox 0x000FEAC0
void accumulate_runtime_sector_brightness(u8 first, u8 second, u8 third,
    RuntimeBackgroundSectors& state); // Xbox 0x000FEB70
void remap_runtime_sector_color(u8& first, u8& second, u8& third,
    RuntimeBackgroundSectors& state); // Xbox 0x000FEBE0
void register_runtime_background_sector(std::span<u8> grid, const char* name,
    RuntimeBackgroundSectors& state); // Xbox 0x000FEFD0
// Clears the sector grids and loads aid_misc_ghoulies_background_lightvolumetable (rows of 0x8C).
void load_runtime_sector_settings(RuntimeBackgroundSectors& state, AnimationResourceRegistry& registry,
    std::span<const RuntimeBodyLoadedResourceSize> loaded, ResourceErrorState& error); // Xbox 0x000FEF60
}
