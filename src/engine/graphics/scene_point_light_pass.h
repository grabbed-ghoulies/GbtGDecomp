#pragma once
#include "glow_pass.h"
#include "../scene/scene_light_grid.h"
#include "../scene/scene_point_lights.h"

#include <functional>

namespace gbtg
{
// The scene's point-light pass (0x001237E0): the light grid's cells and the background's parts are drawn into a
// light texture (book page 0, DAT_005047DC) over a depth texture the size of the back buffer, once to lay down their
// depth (0x00062B00) and once more for each light (0x00063770), then the frame is multiplied by the texture
// (0x00063320).
struct RuntimeScenePointLightTargets
{
    u32 light_texture = 0; // DAT_00504850, a texture record's address.
    DialogueSurfaceHandle light_surface = 0; // DAT_00504854.
    u32 depth_texture = 0; // DAT_00504858, made by 0x00062B00 (D3DFMT_LIN_D24S8).
    DialogueSurfaceHandle depth_surface = 0; // DAT_0050485C.
    DialogueSurfaceHandle saved_target = 0; // DAT_00504860.
    DialogueSurfaceHandle saved_depth = 0; // DAT_00504864.
    u32 copy_texture = 0; // DAT_00504868, the texture 0x00063320 copies the back buffer into (0x0013DF80).
    u32 copy_mode = 0; // DAT_0050486C: set, the frame is modulated by that copy.
    u32 mode = 0; // DAT_00504870: 1, the light texture with its own depth; 0, the back buffer's depth.
    u32 active = 0; // DAT_00504874.
    u32 drawn = 0; // DAT_00504878, the cells 0x00063770 drew.
};
// The camera light script type 0x64 sets (0x001236D0) and the clear colour it gives the light texture.
struct RuntimeSceneCameraLight
{
    const u32& enabled; // DAT_00512104.
    const u32& color; // DAT_00512114.
    const u32& second_color; // DAT_00512118.
    const float& unk_1C; // DAT_0051211C.
    const float& unk_20; // DAT_00512120.
    const float& unk_24; // DAT_00512124.
    const float& unk_28; // DAT_00512128.
    const u32& ambient; // DAT_00512134, the light texture's clear colour (bytes R, G, B, A).
};
struct RuntimeScenePointLightPass
{
    RuntimeDialogueOuterDrawContext& outer; // Draw states, D3D, render targets.
    RuntimeScenePointLightTargets& targets;
    RuntimeGlowTargets& pages; // DAT_005047DC / DAT_005047E0 and DAT_005047E8.
    RuntimeSceneLightGrid& grid;
    RuntimeScenePointLights& lights;
    RuntimeSceneCameraLight camera;
    const entity_detail::DisplayView* const& view; // DAT_005077A8.
    u32& fog_enable; // D3DRS_FogEnable (0x00181CE0).
    const u16& back_width; // DAT_005096E8.
    const u16& back_height; // DAT_005096EA.
    std::function<u32(u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)> create_texture; // 0x00177840
    std::function<void(u32 stream, u32 buffer, u32 stride)> stream; // D3DDevice_SetStreamSource (0x00173610).
    std::function<void(u32 primitive, u32 start, u32 count)> draw_vertices; // D3DDevice_DrawVertices (0x00172920).
    std::function<void(u32 stage, u32 texture)> texture; // D3DDevice_SetTexture (0x001749E0), a record's address.
    std::function<void(u32)> z_bias; // D3DDevice_SetRenderState_ZBias (0x00171660).
    std::function<void(u32)> multisample_antialias; // 0x00172600.
    std::function<void(u32)> multisample_target_mode; // 0x001725D0.
    std::function<u32(u32 address)> word; // Image words: the boot shader rows and pixel shader handles.
    std::function<const float*(u32 address)> floats; // A constant table in the image.
    std::function<const Vec3&(u32 part)> point; // A part's centre: the first row of its +4 points.
    std::function<u32(const u8* record)> address_of; // A texture record's address.
};
// The frame's point lights and the camera light, when the background has a light grid.
void draw_runtime_scene_point_lights(RuntimeScenePointLightPass& context); // Xbox 0x001237E0
// The light texture is cleared to `color` (bytes R, G, B, A) and the grid's cells and the parts are drawn into it.
void begin_runtime_scene_point_lights(u32 grid, u32 color, RuntimeScenePointLightPass& context); // Xbox 0x00062B00, EAX grid
// One light's cells and parts.
void add_runtime_scene_point_light(u32 grid, const Vec3& position, u32 color, u32 second_color, float unk_14,
    float unk_18, float unk_1C, float unk_20, u32 unk_24,
    RuntimeScenePointLightPass& context); // Xbox 0x00063770, EAX grid
// The light texture over the frame.
void finish_runtime_scene_point_lights(RuntimeScenePointLightPass& context); // Xbox 0x00063320
// At scene end: every record and light word cleared, the camera light (DAT_00512104) and the clear colour
// (DAT_00512134) zeroed, the light texture forgotten and the depth texture and its surface released (0x001759F0).
void reset_runtime_scene_point_light_pass(RuntimeScenePointLightPass& context, u32& camera_light, u32& ambient,
    const std::function<void(std::uintptr_t)>& release); // Xbox 0x00123640
}
