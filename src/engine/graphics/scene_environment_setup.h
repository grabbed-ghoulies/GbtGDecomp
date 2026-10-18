#pragma once
#include "scene_light_upload.h"
#include "scene_ambient_sampling.h"

namespace gbtg
{
struct SceneEnvironmentSetupContext
{
    SceneLightUploadContext& upload;
    const RuntimeSceneFog& fog;
    const SceneAmbientContext& ambient;
    RuntimeSceneLightDirection& direction;
    const std::array<float, 4>& base_color; // DAT_00509B3C
    const std::array<float, 4>& override_base_color; // DAT_0042A570
    std::array<u32, 16>& primary; // DAT_004D9710
    std::array<u32, 16>& secondary; // DAT_004D9680
};
void configure_runtime_scene_environment(const Vec3& position, u8 enabled,
    SceneEnvironmentSetupContext& context); // Xbox 0x000FD2A0
struct SceneLightConstructionContext
{
    SceneEnvironmentSetupContext& environment;
    const std::array<float, 3>& scene_color; // DAT_00509B30..38
    std::array<float, 4>& retained_color; // DAT_0032E4E4..F0, aliases upload.retained_scene_color
    const float& directional_intensity; // DAT_00509B4C, 1.0 from scene load 0x0011CAC0
    float override_intensity; // DAT_0042A580
    RuntimeRenderLight& directional; // DAT_00509AB0
    // DAT_00509B7C: the 0x98-byte light records 0x000FDF60 registers (position +0x40, radius +0x70, color +0x78).
    const std::array<u8*, 40>& local_lights;
    std::span<RuntimeRenderLight, 2> temporary; // DAT_005097B8, stride 0x4C
};
void build_runtime_scene_lights(const Vec3& position,
    SceneLightConstructionContext& context); // Xbox 0x000FD3F0
}
