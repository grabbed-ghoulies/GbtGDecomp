#pragma once
#include "../graphics/scene_ambient_sampling.h"

#include <array>
#include <span>

namespace gbtg
{
// The scene's point lights (DAT_00511B70, 32 records of 0x2C) and their bookkeeping.
struct RuntimeScenePointLights
{
    std::array<SceneAmbientPoint, 32>& points; // DAT_00511B70.
    u32& count; // DAT_005120F0, the records in use.
    i32& free; // DAT_005120F4, the first free record, -1 for none.
    u32& enabled; // DAT_005120F8, the background model's light word.
    u32& unk_fc; // DAT_005120FC, the background's type-0x11 block +0xC.
    u32& unk_100; // DAT_00512100, byte 1 of the background header's +0x10 block.
};
// Clears every record and reads the light words of the first background's model (DAT_00511B20's actor +0x204);
// `background_model` is that model's CPU header (offsets for pointers), empty without one.
void reset_runtime_scene_point_lights(std::span<const u8> background_model,
    RuntimeScenePointLights& lights); // Xbox 0x00123560
// From the current free record on, the first inactive one; -1 when all 32 are used.
void find_free_runtime_scene_point_light(RuntimeScenePointLights& lights); // Xbox 0x00123520
// A record at `position` with the given words, active; its index, or -1 while the lights are off or full.
i32 add_runtime_scene_point_light(const Vec3& position, u32 color, u32 second_color, float unk_14, float unk_18,
    float unk_1C, float unk_20, u32 unk_24, RuntimeScenePointLights& lights); // Xbox 0x00123720, ECX position
// The release the light owners inline (0x0006C540, 0x00066A80): the record goes inactive and becomes the free one
// when it lies before it.
void release_runtime_scene_point_light(i32 index, RuntimeScenePointLights& lights);
}
