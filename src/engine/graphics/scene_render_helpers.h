#pragma once
#include "gbtg/model_render.h"

namespace gbtg
{
void copy_runtime_directional_lights(const u32* input, u16 count,
    RuntimeModelRenderContext& renderer); // Xbox 0x001564F0
bool equal_runtime_packed_colors(u32 first, u32 second); // Xbox 0x00101930
void set_runtime_scene_color(float red, float green, float blue, float alpha,
    std::array<float, 4>& retained, RuntimeModelRenderContext& renderer); // Xbox 0x00159210
void copy_runtime_primary_environment_matrix(const u32* input,
    std::array<u32, 16>& projection); // Xbox 0x0015B300
void copy_runtime_secondary_environment_matrix(const u32* input,
    std::array<u32, 16>& view); // Xbox 0x0015B320
}
