#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
struct SceneAmbientPoint
{
    Vec3 position; // DAT_00511B70, original stride 0x2C.
    u32 active; // +0x28, DAT_00511B98.
};
struct SceneAmbientContext
{
    std::span<const u8* const> grids; // DAT_0050D770.
    i32 grid_count; // DAT_0050D780.
    u32 override_count; // DAT_00509B5C.
    std::array<float, 4> override_color; // DAT_0042A560..56C.
    std::array<float, 3> scale; // DAT_00509B20..28.
    u32 points_enabled; // DAT_005120F0.
    std::span<const SceneAmbientPoint, 32> points;
};
u32 add_saturated_runtime_colors(u32 first, u32 second); // Xbox 0x001018A0
u32 sample_runtime_environment_grid(const u8* grid, const Vec3& position); // Xbox 0x000524A0
void sample_runtime_environment_color(const Vec3& position, float* output,
    const SceneAmbientContext& context); // Xbox 0x000FEE90
void sample_runtime_scene_ambient(const Vec3& position, float* output,
    const SceneAmbientContext& context); // Xbox 0x000FD850
void set_runtime_environment_world_space(u8 enabled, u8& retained); // Xbox 0x0015ACA0
}
