#pragma once
#include "gbtg/entity.h"
#include <array>
#include <functional>
#include <span>
#include <vector>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
struct DialogueOuterD3DBoundaries;
// The first background's mirror polygons (its model's runtime header +0x18) and the state the mirror pass
// leaves for the reflected bins. Words inside the mirror block are offsets in the model's CPU block.
struct RuntimeSceneMirrors
{
    u32 count = 0; // DAT_00511090
    std::span<const u8> model; // The DAT_00511B20 background's +0x204 model block.
    u32 data = 0; // DAT_00511094, 0 without mirrors.
    std::vector<std::array<float, 6>> boxes; // DAT_00511098: per mirror, minimum then maximum.
    std::array<float, 42> frustum{}; // DAT_005110B8: the reflected view's planes from +0x48.
    u32 sphere_test = 0; // DAT_00511160
    u8 plane = 0; // DAT_00511164, the first plane the sphere test tries.
    std::array<float, 4> plane_equation{}; // DAT_00511168..DAT_00511174
    Vec3 camera{}; // DAT_00511178, the reflected camera.
    std::array<float, 16> projection_transpose{}; // DAT_00506F98
    // 0x0011F690's stack view * reflection, which 0x00156330 binds as the frame camera for the reflected bins.
    std::array<float, 16> reflected_view{};
};
// Scene startup: reads the background's mirror block and the bounds of each mirror's triangles.
void setup_runtime_scene_mirrors(std::span<const u8> background_model,
    RuntimeSceneMirrors& mirrors); // Xbox 0x0011F450
// 1 when the position is on or in front of the plane through the block's first three vertices.
u32 runtime_mirror_side(const Vec3& position, const RuntimeSceneMirrors& mirrors); // Xbox 0x0011F310
// The planes of a symmetric view from `eye` toward `target` that just holds a sphere of `radius` there.
void build_runtime_mirror_frustum(const Vec3& eye, const Vec3& target, float radius,
    std::array<float, 42>& frustum); // Xbox 0x0013E520
void build_runtime_view_frustum(std::array<float, 42>& frustum, const float* view, const float* projection,
    const Vec3& eye, const Vec3& target, float behind); // Xbox 0x0013E070
struct RuntimeSceneMirrorPass
{
    RuntimeSceneMirrors& mirrors;
    const u32& enabled; // DAT_005110B4.
    RuntimeDisplaySetupContext& display;
    DialogueOuterD3DBoundaries& outer;
    u32& cull_mode; // DAT_0032E3D0, set through 0x001565E0.
    std::function<void(i32)> draw_bins; // 0x00120E80, ESI the mirror flag.
};
void draw_runtime_scene_mirror(RuntimeSceneMirrorPass& context); // Xbox 0x0011F690
}
