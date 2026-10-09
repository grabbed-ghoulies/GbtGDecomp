#pragma once
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
struct CutsceneCameraEventAsset
{
    std::span<const u8> resource; // DAT_004E1C50, native pointer mapping.
    float duration; // DAT_004E1C4C.
};
void load_cutscene_camera_event_asset(const char* name,
    CutsceneCameraEventAsset& state, AnimationResourceRegistry& resources,
    ResourceErrorState& error); // Xbox 0x0004F0B0
i32 select_cutscene_random_index(u32& random, i32 upper, i32 lower); // Xbox 0x0001BB60
}
