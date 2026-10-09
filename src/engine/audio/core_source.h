#pragma once

#include "core_memory.h"
#include <array>
#include <span>

namespace gbtg
{
struct AudioCoreMixBin
{
    u32 bin;
    i32 volume;
};
struct AudioCoreSource : AudioCoreHeader
{
    AudioCoreLink secondary; // +18
    void* attributes; // +20
    u32 mode; // +24
    void* handle; // +28
    i32 mix_count; // +2C
    std::array<AudioCoreMixBin, 8> mix; // +30
    std::array<u8, 8> mix_flags; // +70
    u32 reverb_enabled; // +78
};
struct AudioCoreSourceBoundaries
{
    std::function<i32(u32, void*&)> create; // IXACTEngine_CreateSoundSource
    std::function<void(void*, std::span<const AudioCoreMixBin>)> mix_bins;
    std::function<void(void*, std::span<const AudioCoreMixBin>)> mix_volumes;
    std::function<bool(const void*)> bad_pointer; // XAPI pointer validation at the heap boundary
    std::function<void(void*, u32&)> status; // IXACTSoundSource_GetStatus
    std::function<void(void*)> release; // IXACTSoundSource_Release
    std::function<void(const char*)> debug_string; // OutputDebugStringA
    std::function<void(void*, const Vec3&, u32)> position; // IXACTSoundSource_SetPosition
    std::function<void(void*, const Vec3&, u32)> velocity; // IXACTSoundSource_SetVelocity
    std::function<void(void*, const Vec3&, u32)> cone; // IXACTSoundSource_SetConeOrientation
    std::function<void(void*, std::span<const u32, 9>, u32)> reverb; // IXACTSoundSource_SetI3DL2Source
};
struct AudioCoreSources
{
    AudioCoreMemory memory;
    AudioCoreSourceBoundaries boundaries;
    AudioCoreLink secondary; // DAT_004D97FC
    AudioCoreLink sources; // DAT_004D9810
    AudioCoreLink* tail; // DAT_004D9818
    float unit_scale; // DAT_004D97F0
    u32 apply; // DAT_004D97EC
};
void add_audio_source_mix_bins(AudioCoreSource& source, const u32* bins,
    AudioCoreSourceBoundaries& boundaries); // Xbox 0x0015BE00
void remove_audio_source_mix_bins(AudioCoreSource& source, const u32* bins); // Xbox 0x0015C000
void set_audio_source_mix_volume(AudioCoreSource& source, u32 bin, i32 volume,
    AudioCoreSourceBoundaries& boundaries); // Xbox 0x0015C0C0
void enable_audio_source_reverb(AudioCoreSource& source, i32 enabled,
    AudioCoreSourceBoundaries& boundaries); // Xbox 0x0015C170
void create_audio_core_source(u32 mode, AudioCoreSource*& output,
    AudioCoreSources& context); // Xbox 0x0015C320
void audio_core_debug(AudioCoreSourceBoundaries& boundaries, const char* format, ...); // Xbox 0x0015B920
bool audio_core_source_valid(AudioCoreSource* source, AudioCoreSources& context); // Xbox 0x0015C790
bool audio_core_source_in_use(AudioCoreSource& source, AudioCoreSources& context); // Xbox 0x0015C990
void destroy_audio_core_source(AudioCoreSource*& source, AudioCoreSources& context); // Xbox 0x0015C9E0
void set_audio_core_source_position(AudioCoreSource& source, const Vec3& position,
    AudioCoreSources& context); // Xbox 0x0015C800
void set_audio_core_source_velocity(AudioCoreSource& source, const Vec3& velocity,
    AudioCoreSources& context); // Xbox 0x0015C890
void set_audio_core_source_cone(AudioCoreSource& source, const Vec3& direction,
    AudioCoreSources& context); // Xbox 0x0015C930
void set_audio_core_source_reverb(AudioCoreSource& source, std::span<const u32, 9> values,
    AudioCoreSources& context); // Xbox 0x0015C290
}
