#pragma once

#include "gbtg/model_runtime.h"

namespace gbtg
{

struct RuntimeModelRenderState
{
    u16 texture_key = 0; // +0x46, hashed into the material texture key.
    u16 directional_light_count = 0; // +0x4A
    std::array<float, 3> material_vector{}; // +0x54
    std::array<u32, 20> directional_lights{}; // +0xF8, four 0x14-byte records
    std::function<void(u32, u32, u32, void*)> draw; // +0x50
    u32 material_values = 0; // +0x70
    u32 material = 0; // +0x74
    std::array<u8, 4> color{}; // +0x184
    u32 node_descriptor = 0; // +0x1F4
    RuntimeModelInstance* instance = nullptr; // +0x1F8, host view beginning at model +0x294
    ModelPoseBuffer* skeleton_pose = nullptr; // +0x60, pointer-width view.
    u32 skeleton_matrices = 0; // +0x68, node +0x24.
    u32 skeleton_joint_count = 0; // +0x6C, node +0x20.
    std::array<float, 4> base_color{}; // DAT_00437270
    std::array<float, 4> ambient_color{}; // DAT_00437280
    float fog_reciprocal = 0; // DAT_00437238
    float fog_scaled_end = 0; // DAT_0043723C
    u8 fog_enabled = 0; // DAT_00437248
    u32 fog_color = 0; // DAT_0043724C
    const float* frustum = nullptr; // +0x64, native six-plane source.
    u32 background_key = 0xFFFFFFFFu; // +0x18C. Only 0x00155A10 writes it (-1); 0x001580E0 compares it.
};
struct RuntimeRenderLight
{
    RuntimeRenderLight* previous = nullptr;
    RuntimeRenderLight* next = nullptr;
    u32 type = 0; // +8
    std::array<float, 4> color{}; // +0xC
    Vec3 direction{}; // +0x1C
    Vec3 position{}; // +0x28
    float range = 0; // +0x34
    float range_reciprocal = 0; // +0x38
    u32 reserved = 0; // +0x3C
};
struct RuntimeModelRenderFrame
{
    RuntimeShaderMatrix world{}; // +0x44, original frame stride 0x310
};
struct RuntimeModelRenderContext
{
    RuntimeModelRenderState state; // DAT_004370A0
    std::span<RuntimeModelRenderFrame> frames; // DAT_004392B0
    i32 frame_index = 0; // DAT_0043F4B4
    u32 draw_parameter = 0; // DAT_005079E8
    RuntimeShaderMatrix world_scratch{}; // DAT_00507958
    RuntimeModelInstance* active_model = nullptr; // DAT_005079EC
    u32 rendering = 0; // DAT_005079E4
    RuntimeRenderLight lights; // DAT_00443998 sentinel
    u32 lighting_flags = 0; // DAT_00445120
    u8 lights_in_world_space = 0; // DAT_004439A0
    u32 changed_render_states = 0; // DAT_00181968
    u32 fog_render_state = 0; // D3DRS_FogEnable
};
struct RuntimeSceneFog
{
    i32 enabled = 0; // DAT_00509AFC
    i32 disabled = 0; // DAT_00509B00
    std::array<u8, 4> color{}; // DAT_00509B04
    float start = 0; // DAT_00509B08
    float end = 0; // DAT_00509B0C
};
struct RuntimeSceneLightDirection
{
    Vec3 scratch{}; // DAT_00509B10
    float pitch = 0; // DAT_00509B50
    float yaw = 0; // DAT_00509B54
    u32 camera_relative = 0; // DAT_00509B58
    u32 override_mode = 0; // DAT_00509B5C
    float override_pitch_degrees = 0; // DAT_0042A584
    float override_yaw_degrees = 0; // DAT_0042A588
    // DAT_005077A8 +0xD8: the current display view's rotation, read when the direction is computed.
    std::function<const float*()> camera;
};
// DAT_00509B10..DAT_00509B5C: one copy for the game. 0x000FD620 sets the angles and camera-relative mode at boot.
extern RuntimeSceneLightDirection g_scene_light_direction;
void configure_runtime_scene_fog(RuntimeModelRenderContext& context,
    const RuntimeSceneFog& fog); // Xbox 0x000FD5C0
Vec3& runtime_scene_light_direction(RuntimeSceneLightDirection& state,
    const std::function<void(Vec3&, const Vec3&)>& normalize); // Xbox 0x000FD9C0
RuntimeModelRenderState& runtime_render_state(RuntimeModelRenderContext& context); // Xbox 0x00156DD0
RuntimeModelRenderFrame& runtime_render_frame(RuntimeModelRenderContext& context); // Xbox 0x00156DE0
void set_runtime_render_material(RuntimeModelRenderContext& context,
    u32 values, u32 material); // Xbox 0x00156300
void set_runtime_render_instance(RuntimeModelRenderContext& context,
    RuntimeModelInstance* instance); // Xbox 0x001565C0
void set_runtime_render_world(RuntimeModelRenderContext& context,
    const RuntimeShaderMatrix* matrix); // Xbox 0x00156370
void set_runtime_render_color(RuntimeModelRenderContext& context,
    u8 red, u8 green, u8 blue, u8 alpha); // Xbox 0x001564A0
void initialize_runtime_light(RuntimeRenderLight& light); // Xbox 0x001599E0
RuntimeRenderLight* next_runtime_light(const RuntimeRenderLight& light); // Xbox 0x00159AC0
void append_runtime_light(RuntimeRenderLight& list, RuntimeRenderLight& light); // Xbox 0x00159A30
void insert_runtime_light_before(RuntimeRenderLight& before, RuntimeRenderLight& light); // Xbox 0x00159A60
void refresh_runtime_lighting_flags(RuntimeModelRenderContext& context); // Xbox 0x00158D20
void add_runtime_light(RuntimeModelRenderContext& context, RuntimeRenderLight& light); // Xbox 0x00158D80
void clear_runtime_lights(RuntimeModelRenderContext& context); // Xbox 0x001593A0
void set_runtime_light_type(RuntimeRenderLight& light, u32 type); // Xbox 0x00159320
void set_runtime_light_color(RuntimeRenderLight& light, float red, float green, float blue, float alpha); // Xbox 0x001592A0
void set_runtime_light_direction(RuntimeRenderLight& light, const Vec3& direction); // Xbox 0x001592E0
void set_runtime_light_position(RuntimeRenderLight& light, const Vec3& position); // Xbox 0x00159300
void set_runtime_light_range(RuntimeRenderLight& light, float start, float end); // Xbox 0x00159330
void set_runtime_lighting_enabled(RuntimeModelRenderContext& context, u8 enabled); // Xbox 0x00159360
void set_runtime_lights_world_space(RuntimeModelRenderContext& context, u8 enabled); // Xbox 0x001593C0
void set_runtime_render_base_color(RuntimeModelRenderContext& context, const std::array<float, 4>& color); // Xbox 0x00156F30
void set_runtime_render_ambient(RuntimeModelRenderContext& context, const std::array<float, 4>& color); // Xbox 0x00156F70
void set_runtime_render_fog_enabled(RuntimeModelRenderContext& context, u8 enabled); // Xbox 0x00156EA0
void set_runtime_render_fog(RuntimeModelRenderContext& context, float start, float end,
    u8 red, u8 green, u8 blue, u8 alpha); // Xbox 0x00156ED0
void apply_runtime_model_materials(const RuntimeRelocationImage& image,
    u32 model_cpu, RuntimeModelRenderContext& context); // Xbox 0x000F94F0
void runtime_render_empty(); // Xbox 0x001628B0
void draw_runtime_model_tree(const RuntimeRelocationImage& image, u32 node,
    u32 first, u32 second, void* third, RuntimeModelRenderContext& context); // Xbox 0x00156240
void draw_runtime_model_instance(RuntimeModelInstance& model, const RuntimeRelocationImage& image,
    u32 model_cpu, const AnimationMatrix& world, u32 parameter,
    RuntimeModelRenderContext& context, const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x000F9120

} // namespace gbtg
