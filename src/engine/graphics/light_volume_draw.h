#pragma once
#include "gbtg/types.h"
#include <functional>
#include <span>

namespace gbtg
{
struct RuntimeLightVolumeOrder
{
    i32& count; // DAT_0050D71C.
    const i32& limit; // DAT_0050D720.
    std::span<u8> order; // DAT_0050D724.
};
// Volumes with a nonzero +0x1D0 word come first, then the nearer (+0x1D4) of two.
int compare_runtime_light_volumes(const void* left, const void* right); // Xbox 0x000FE430
// Numbers the registered light volumes; more than the limit sorts them (comparator 0x000FE430) and keeps the
// first `limit`.
void order_runtime_light_volumes(RuntimeLightVolumeOrder& context); // Xbox 0x000FE480
struct RuntimeLightVolumeDraw
{
    RuntimeLightVolumeOrder& order;
    std::span<const u8> background_header; // DAT_00511B20 object +0x204 -> resource header, or empty.
    std::function<void(u8 volume)> draw_volume; // 0x000FE5A0, 0x000644F0 and 0x00064820 for one record.
    std::function<void()> bind_projection; // 0x000F6170.
};
// The scene's light volumes, drawn after the background passes when the background model has light data.
void draw_runtime_light_volumes(RuntimeLightVolumeDraw& context); // Xbox 0x000FE960
}
