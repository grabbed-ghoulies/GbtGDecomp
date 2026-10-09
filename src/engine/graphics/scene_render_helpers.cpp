#include "scene_render_helpers.h"
#include "../audio/listener_settings.h"

namespace gbtg
{
namespace
{
u32 reverse_color_bytes(u32 color)
{
    return ((color & 0xFF) << 24) | ((color & 0xFF00) << 8)
        | ((color >> 8) & 0xFF00) | (color >> 24);
}
}
// Xbox 0x00101930
bool equal_runtime_packed_colors(u32 first, u32 second)
{
    return reverse_color_bytes(first) == reverse_color_bytes(second);
}
// Xbox 0x001564F0
void copy_runtime_directional_lights(const u32* input, u16 count,
    RuntimeModelRenderContext& renderer)
{
    RuntimeModelRenderState& state = runtime_render_state(renderer);
    state.directional_light_count = count;
    u32* output = state.directional_lights.data();
    for (u32 remaining = static_cast<u32>(count) * 5; remaining != 0; --remaining)
    {
        *output = *input;
        ++input;
        ++output;
    }
}
// Xbox 0x00159210
void set_runtime_scene_color(float red, float green, float blue, float alpha,
    std::array<float, 4>& retained, RuntimeModelRenderContext& renderer)
{
    retained[0] = red;
    retained[1] = green;
    retained[2] = blue;
    retained[3] = alpha;
    const u8 a = static_cast<u8>(truncate_runtime_float(static_cast<long double>(alpha) * 255.f));
    const u8 b = static_cast<u8>(truncate_runtime_float(static_cast<long double>(blue) * 255.f));
    const u8 g = static_cast<u8>(truncate_runtime_float(static_cast<long double>(green) * 255.f));
    const u8 r = static_cast<u8>(truncate_runtime_float(static_cast<long double>(red) * 255.f));
    set_runtime_render_color(renderer, r, g, b, a);
}
// Xbox 0x0015B300
void copy_runtime_primary_environment_matrix(const u32* input, std::array<u32, 16>& projection)
{
    u32* output = projection.data();
    for (i32 remaining = 16; remaining != 0; --remaining)
    {
        *output = *input;
        ++input;
        ++output;
    }
}
// Xbox 0x0015B320
void copy_runtime_secondary_environment_matrix(const u32* input, std::array<u32, 16>& view)
{
    u32* output = view.data();
    for (i32 remaining = 16; remaining != 0; --remaining)
    {
        *output = *input;
        ++input;
        ++output;
    }
}
}
