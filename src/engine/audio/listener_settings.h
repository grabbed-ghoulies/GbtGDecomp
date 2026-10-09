#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <functional>

namespace gbtg
{
struct AudioListenerSettings
{
    std::array<u32, 12> parameters{}; // DAT_004D9798, packed I3DL2 fields
    u32 apply = 0; // DAT_004D97EC
    i32 preset = 0; // DAT_004E14F4
    std::function<i32(u32, i32)> master_volume; // IXACTEngine_SetMasterVolume
    std::function<i32(const std::array<u32, 12>&, u32)> listener; // IXACTEngine_SetI3dl2Listener
};
std::int64_t truncate_runtime_float(long double value); // Xbox 0x001673D8
i32 audio_volume_attenuation(i32 volume); // Xbox 0x0015B8F0
void set_audio_category_volume(AudioListenerSettings& state, u32 category,
    i32 volume); // Xbox 0x0015C1C0
void set_audio_listener(AudioListenerSettings& state,
    const std::array<u32, 12>& parameters); // Xbox 0x0015C1E0
void set_audio_reverb_preset(AudioListenerSettings& state, i32 preset,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x0003A1F0
void set_dialogue_audio_duck(AudioListenerSettings& state, i32 enabled,
    u32 scene_flags, u32 first_category, u32 second_category); // Xbox 0x00058B90
}
