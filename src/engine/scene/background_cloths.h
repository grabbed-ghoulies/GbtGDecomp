#pragma once
#include "background_morphs.h"
#include "../graphics/background_cloth.h"

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace gbtg
{
struct AudioEventVoices;
// The scene's background cloths, made at scene load (0x00114180) and released with the scene (0x001143A0).
struct RuntimeBackgroundCloths
{
    bool created = false; // The pools exist.
    std::vector<RuntimeBackgroundCloth*> list; // DAT_0050E390 (stride 4, growth 2).
    std::vector<std::array<float, 16>> planes; // DAT_0050E394 (stride 0x40, growth 2): plane, then four corners.
    RuntimeBackgroundMorphs morphs; // DAT_0050E388 (stride 0x2C, growth 2).
    u32 ramp = 0; // DAT_0050E418: the 256 x 1 grey steps the cloth draw samples.
};
struct RuntimeBackgroundClothScene
{
    RuntimeBackgroundCloths& cloths;
    RuntimeBackgroundMorphs*& morphs; // DAT_0050E388 as the scene controls see it.
    u32& ramp_texture; // DAT_005043B8.
    RuntimeBackgroundClothContext& cloth;
    std::function<u32(u32, u32, u32, u32, u32, u32)> create_texture; // D3DDevice_CreateTexture2 (0x00177840).
    std::function<u8*(u32 texture)> lock_texture; // D3DTexture_LockRect (0x00177950): level 0's texels.
    std::function<void(u32)> release_texture; // D3DResource_Release.
    std::function<u32()> background_model; // **(DAT_00511B20 - 0x730): the first background's model, or 0.
    std::function<std::span<const float, 42>()> frustum; // DAT_005077A8 +0x138.
    std::function<u8&()> frustum_hint; // DAT_005077A8 +0x1E0.
    AudioEventVoices& voices; // 0x00039580's voices.
    const float* room_low; // DAT_0050EA04..DAT_0050EA0C.
    const float* room_high; // DAT_0050EA10..DAT_0050EA18.
};
// The cloth list, plane list and curtain pairs, and the ramp texture.
void create_runtime_background_cloth_pools(RuntimeBackgroundClothScene& scene); // Xbox 0x00114180
void release_runtime_background_morphs(RuntimeBackgroundClothScene& scene); // Xbox 0x001134A0
void release_runtime_background_cloths(RuntimeBackgroundClothScene& scene); // Xbox 0x001143A0
// The first background's part table: cloths (kinds 1, 2, 3, 5, 6, 7, 8) and collision planes (kind 4), then each
// plane a cloth's sphere touches is added to that cloth, facing it.
void build_runtime_background_cloths(RuntimeBackgroundClothScene& scene); // Xbox 0x001153A0
// Lists the cloth, steps it 0.1 s and measures it.
void register_runtime_background_cloth(RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothScene& scene); // Xbox 0x00115A40, ESI cloth
// Each cloth on screen (or always stepped) steps; an awake one is measured and may rustle; then the curtain pairs.
void tick_runtime_background_cloths(float step, RuntimeBackgroundClothScene& scene); // Xbox 0x00115BB0
}
