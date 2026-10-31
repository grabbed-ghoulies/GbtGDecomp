#pragma once
#include "background_cloth.h"
#include "scene_point_light_pass.h"
#include "scene_model_draw.h"
#include "../entities/candle_weapon.h"

#include <array>
#include <functional>
#include <vector>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
// The shadow projector's matrices and flag (DAT_00504880..DAT_00504950): a screen-to-texture matrix, a bias matrix
// and their product (the one 0x00064820 multiplies the light's view and projection by), and the alternate shader flag
// with its colour constant.
struct RuntimeShadowProjector
{
    std::array<float, 16> screen{}; // DAT_00504880
    std::array<float, 16> bias{}; // DAT_005048C0
    std::array<float, 16> product{}; // DAT_00504900
    u32 alternate = 0; // DAT_00504940, BSS that no translated code writes.
    std::array<float, 4> alternate_color{}; // DAT_00504944..DAT_00504950
};
// The six planes 0x00064030 builds (each a normal and a distance along it), laid out as the display view's frustum
// block (+0x48 on), so 0x00159BA0 tests boxes against it.
using RuntimeProjectorFrustum = std::array<float, 42>;
struct RuntimeBackgroundClothDraw
{
    RuntimeScenePointLightPass& pass; // D3D, pages, the light grid, the view, the back buffer size and image words.
    RuntimeDisplaySetupContext& display; // 0x000F6170, 0x000F6430.
    RuntimeShadowProjector& projector;
    const u32& ramp; // DAT_005043B8.
    float& water_phase; // DAT_005462B0, BSS: 0x0005D710's texture wobble, 0.01 more each draw.
    std::function<u8*(u32 address)> texture_record; // A texture record by retail address (0x00053550's ESI).
    std::function<u32()> background_model; // **(DAT_00511B20 - 0x730), or 0.
};
// One layer's cloths: the shadows of those that cast one and are in view, then the cloths themselves.
void draw_runtime_background_cloths(u32 layer, std::vector<RuntimeBackgroundCloth*>& cloths,
    RuntimeBackgroundClothDraw& context); // Xbox 0x00115A80
// The cloth pass's states, its vertex shader with the world-view-projection at c0x60 and c0x64..c0x67, and the ramp
// in stages 1 and 2.
void begin_runtime_background_cloth_pass(RuntimeBackgroundClothDraw& context); // Xbox 0x0005D250
// The cloth as strips of {position, colour, row, column}, textured with +0xA8 (nothing without it).
void draw_runtime_background_cloth(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothDraw& context); // Xbox 0x0005D470, EDI cloth
// The water's draw: 0x0005D470's strips with the texture rows and columns waved 0.02 by sin and cos of 20 times their
// share plus the phase, which advances 0.01 first.
void draw_runtime_background_water(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothDraw& context); // Xbox 0x0005D710, EDI cloth
// The cloth's silhouette, white on the page target, seen from 30 above its bounds' centre along -30 x its light.
void capture_runtime_background_cloth_shadow(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothDraw& context); // Xbox 0x0005DA50, EAX cloth
// The projector pass's states and the screen matrix for the back buffer's size.
void begin_runtime_background_cloth_shadow(RuntimeBackgroundClothDraw& context); // Xbox 0x0005DEB0
// The silhouette projected onto the light grid's cells (colour 0xB4000000).
void project_runtime_background_cloth_shadow(RuntimeBackgroundCloth& cloth, u32 grid,
    RuntimeBackgroundClothDraw& context); // Xbox 0x0005DFA0, EAX cloth
void end_runtime_background_cloth_shadow(RuntimeBackgroundClothDraw& context); // Xbox 0x0005DF30
// The light volumes' casters and target: the model draw, the cull state, the candle's flame spot and the volume texture
// boot 0x000620C0 makes (256 x 256, its surface the render target).
struct RuntimeLightVolumeCasters
{
    SceneModelDrawContext& models; // 0x000F94F0 and 0x000F9120's image, renderer and matrices.
    u32& cull_mode; // 0x001565E0's state.
    const RuntimeCandleFlameSpot& candle; // DAT_0050D740..DAT_0050D74C.
    u32 texture = 0; // DAT_00504844.
    DialogueSurfaceHandle surface = 0; // DAT_00504848.
    // A model's CPU header address; the host registers its node flags (model +0x294) for the draw.
    std::function<u32(RuntimeModelInstance&)> model_cpu;
};
// Volume `volume`'s casters drawn from its light (the candle's flame while it burns) into the volume texture: the
// record's reach grows to cover each extra caster, the view frames the reach and each caster is drawn (extra casters
// twice, kinds 1 and 2).
void capture_runtime_light_volume(u8 volume, RuntimeBackgroundClothDraw& context,
    RuntimeLightVolumeCasters& casters); // Xbox 0x000FE5A0
// 0x000FE960's loop body: the capture, the projector pass and the volume texture projected from the light onto the
// grid (with the parts when record `index`'s +0x1D8 byte is set), then the projector's states cleared.
void project_runtime_light_volume(i32 index, u8 volume, u32 grid, u32 color, RuntimeBackgroundClothDraw& context,
    RuntimeLightVolumeCasters& casters); // Xbox 0x000FE9B0..0x000FEA65
struct RuntimeSceneRefraction;
// The stage states for an untextured XYZ | DIFFUSE draw.
void begin_runtime_untextured_draw(RuntimeBackgroundClothDraw& context); // Xbox 0x0005E340
// With a refraction mesh and `disabled` (DAT_00510650) clear: the back buffer is copied to book page 0's texture,
// the mesh's triangles mark the stencil, and the quad draws the copy through the noise where it is marked.
void draw_runtime_refraction(RuntimeSceneRefraction& refraction, u32 disabled,
    RuntimeBackgroundClothDraw& context); // Xbox 0x0011BA50
// The projector pass's shaders, states and constants (c0x60 the frame's transform, c0x6A one, c0x73 zero), and the
// screen and bias matrices.
void begin_runtime_shadow_projector(RuntimeBackgroundClothDraw& context); // Xbox 0x000644F0
struct RuntimeActorSnapshot;
// The actor snapshots (DAT_005462F8 in link order) projected as decals on the grid DAT_005462EC, when it is set: each
// from its +8 eye onto its +0x14 point with its +0x20 texture, +0x24 radius and +0x28 colour (an unkept one's alpha
// scaled by its +0x2C fade), then the projector's states reset.
void draw_runtime_actor_snapshot_decals(u32 grid, const std::vector<std::unique_ptr<RuntimeActorSnapshot>>& snapshots,
    RuntimeBackgroundClothDraw& context); // Xbox 0x00116460
// The texture projected from `eye` toward `at` (EDX) over the grid's (ECX) cells around `at`, `reach` (EAX) cells out;
// the cells drawn.
i32 project_runtime_texture_on_grid(u32 grid, const Vec3& at, const Vec3& eye, u32 texture, float radius, u32 color,
    bool parts, i32 reach, RuntimeBackgroundClothDraw& context); // Xbox 0x00064820, ECX grid, EDX at, EAX reach
// The projector's frustum from its view (EAX) and projection.
void build_runtime_projector_frustum(RuntimeProjectorFrustum& frustum, const float* view, const float* projection,
    const Vec3& eye, const Vec3& at, float near_offset); // Xbox 0x00064030, ECX frustum, EAX view
void look_at_runtime_xmath_matrix(float* output, const Vec3& eye, const Vec3& at,
    const Vec3& up); // Xbox 0x0015233A
// The vector over its length, or itself when that is one, or zero when it is (nearly) zero.
void normalize_runtime_xmath_vector(Vec3& output, const Vec3& vector); // Xbox 0x00151CE0
void transform_runtime_xmath_vector4(float* output, const float* vector, const float* matrix); // Xbox 0x00151E1E
}
