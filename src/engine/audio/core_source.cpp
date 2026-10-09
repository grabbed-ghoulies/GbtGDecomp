#include "core_source.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr std::array<u32, 28> flat_bins
{
    0, 1, 4, 5, 2, 10, 3, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 31
}; // DAT_00336200
constexpr std::array<u32, 28> spatial_bins
{
    6, 7, 8, 9, 2, 10, 3, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 31
}; // DAT_00336270
constexpr std::array<u32, 32> bin_order
{
    6, 8, 7, 9, 0, 1, 2, 4, 5, 10, 3, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
}; // DAT_003362E0
u32 mapped_bin(const AudioCoreSource& source, u32 bin)
{
    switch (source.mode)
    {
    case 1:
    case 3:
    case 4:
        return spatial_bins[bin];
    case 5:
    case 6:
    case 7:
    case 8:
        return flat_bins[bin];
    default:
        return 28;
    }
}
}

// Xbox 0x0015BE00
void add_audio_source_mix_bins(AudioCoreSource& source, const u32* bins,
    AudioCoreSourceBoundaries& boundaries)
{
    std::array<i32, 32> present{};
    std::array<u8, 32> flags;
    std::array<AudioCoreMixBin, 32> values;
    i32 count = 0;
    for (i32 index = 0; index < source.mix_count; ++index)
    {
        const u32 bin = source.mix[index].bin;
        values[bin] = source.mix[index];
        flags[bin] = source.mix_flags[index];
        present[bin] = 1;
        count = source.mix_count;
    }
    while (*bins != 28)
    {
        const u32 bin = mapped_bin(source, *bins);
        if (present[bin] == 0)
        {
            present[bin] = 1;
            ++count;
            values[bin] = AudioCoreMixBin{bin, 0};
            flags[bin] = 0x7F;
            ++source.mix_count;
        }
        ++bins;
    }
    i32 index = 0;
    for (u32 bin : bin_order)
    {
        if (present[bin] != 0)
        {
            source.mix[index] = values[bin];
            source.mix_flags[index] = flags[bin];
            ++index;
        }
    }
    source.mix_count = count;
    boundaries.mix_bins(source.handle, std::span(source.mix.data(), count));
}

// Xbox 0x0015C000
void remove_audio_source_mix_bins(AudioCoreSource& source, const u32* bins)
{
    while (*bins != 28)
    {
        const u32 bin = mapped_bin(source, *bins);
        bool found = false;
        for (i32 index = 0; index < source.mix_count; ++index)
        {
            if (found)
            {
                source.mix[index - 1] = source.mix[index];
            }
            else if (source.mix[index].bin == bin)
            {
                found = true;
            }
        }
        if (found)
        {
            --source.mix_count;
        }
        ++bins;
    }
}

// Xbox 0x0015C0C0
void set_audio_source_mix_volume(AudioCoreSource& source, u32 bin, i32 volume,
    AudioCoreSourceBoundaries& boundaries)
{
    const u32 mapped = mapped_bin(source, bin);
    for (i32 index = 0; index < source.mix_count; ++index)
    {
        if (source.mix[index].bin == mapped)
        {
            source.mix[index].volume = volume;
            boundaries.mix_volumes(source.handle, std::span(&source.mix[index], 1));
            return;
        }
    }
}

// Xbox 0x0015C170
void enable_audio_source_reverb(AudioCoreSource& source, i32 enabled,
    AudioCoreSourceBoundaries& boundaries)
{
    source.reverb_enabled = enabled != 0;
    constexpr u32 bins[]{5, 28};
    if (enabled != 0)
    {
        add_audio_source_mix_bins(source, bins, boundaries);
        set_audio_source_mix_volume(source, 5, 0, boundaries);
        return;
    }
    remove_audio_source_mix_bins(source, bins);
    set_audio_source_mix_volume(source, 5, -6400, boundaries);
}

// Xbox 0x0015C320
void create_audio_core_source(u32 mode, AudioCoreSource*& output,
    AudioCoreSources& context)
{
    auto* source = static_cast<AudioCoreSource*>(allocate_audio_core_object(
        0x7C, sizeof(AudioCoreSource), "soundSource", 3, "audioSoundSourceCreate", 0x41, 0,
        context.memory));
    switch (mode)
    {
    case 0:
        source->mode = 0;
        source->handle = nullptr;
        append_audio_core_link(context.secondary, source->secondary);
        source->attributes = allocate_audio_core_array(1, 4, "audioSoundSourceCreate", 0x59, 0, context.memory);
        break;
    case 1:
    case 3:
    case 4:
    {
        source->mode = mode;
        if (context.boundaries.create(2, source->handle) < 0)
        {
            AudioCoreHeader* header = source;
            release_audio_core_object(header, "audioSoundSourceCreate", 0x121, 0, context.memory);
            output = nullptr;
            return;
        }
        append_audio_core_link(context.secondary, source->secondary);
        source->attributes = allocate_audio_core_array(1, 0x50, "audioSoundSourceCreate", 299, 0, context.memory);
        constexpr u32 mode1[]{4, 6, 5, 0, 1, 2, 3, 28};
        constexpr u32 mode3[]{4, 0, 1, 2, 3, 28};
        constexpr u32 mode4[]{4, 6, 0, 1, 2, 3, 28};
        if (mode == 1)
        {
            add_audio_source_mix_bins(*source, mode1, context.boundaries);
            *reinterpret_cast<u32*>(static_cast<u8*>(source->attributes) + 0x4C) = 0;
        }
        else if (mode == 3)
        {
            add_audio_source_mix_bins(*source, mode3, context.boundaries);
        }
        else if (mode == 4)
        {
            add_audio_source_mix_bins(*source, mode4, context.boundaries);
        }
        enable_audio_source_reverb(*source, 1, context.boundaries);
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8:
    {
        source->mode = mode;
        if (context.boundaries.create(1, source->handle) < 0)
        {
            constexpr u32 lines[]{0x71, 0x99, 0xC3, 0xF0};
            AudioCoreHeader* header = source;
            release_audio_core_object(header, "audioSoundSourceCreate", lines[mode - 5], 0, context.memory);
            output = nullptr;
            return;
        }
        append_audio_core_link(context.secondary, source->secondary);
        constexpr u32 sizes[]{4, 8, 8, 0x10};
        constexpr u32 lines[]{0x7B, 0xA3, 0xCD, 0xFA};
        source->attributes = allocate_audio_core_array(1, sizes[mode - 5], "audioSoundSourceCreate", lines[mode - 5], 0, context.memory);
        if (mode != 5)
        {
            *static_cast<u8*>(source->attributes) = 0x40;
        }
        constexpr u32 stereo[]{0, 1, 28};
        constexpr u32 quad[]{0, 1, 2, 3, 28};
        constexpr u32 surround[]{0, 1, 4, 6, 2, 3, 28};
        add_audio_source_mix_bins(*source, mode == 8 ? surround : mode == 7 ? quad : stereo, context.boundaries);
        break;
    }
    default:
        break;
    }
    AudioCoreLink* tail = context.tail;
    if (context.sources.next == nullptr)
    {
        tail = &context.sources;
    }
    append_audio_core_link(*tail, *source);
    context.tail = source;
    source->references = 1;
    output = source;
}

// Xbox 0x0015B920
void audio_core_debug(AudioCoreSourceBoundaries& boundaries, const char* format, ...)
{
    char buffer[256];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    boundaries.debug_string(buffer);
}

// Xbox 0x0015C790
bool audio_core_source_valid(AudioCoreSource* source, AudioCoreSources& context)
{
    AudioCoreLink* current = context.sources.next;
    if (!context.boundaries.bad_pointer(source))
    {
        if (source->handle != nullptr)
        {
            if (context.boundaries.bad_pointer(source->handle))
            {
                audio_core_debug(context.boundaries, "audioSoundSourceCheckValid : XACTSoundSource : %p Invalid!\n", source->handle);
                return false;
            }
        }
        while (current != nullptr)
        {
            if (current == source)
            {
                return true;
            }
            current = current->next;
        }
    }
    else
    {
        audio_core_debug(context.boundaries, "audioSoundSourceCheckValid : %p Invalid!\n", source);
    }
    return false;
}

// Xbox 0x0015C990
bool audio_core_source_in_use(AudioCoreSource& source, AudioCoreSources& context)
{
    if (!audio_core_source_valid(&source, context))
    {
        audio_core_debug(context.boundaries, "audioSoundSourceDestroy : soundSource : %p is no longer valid!\n", &source);
        return true;
    }
    u32 status;
    context.boundaries.status(source.handle, status);
    return status == 1;
}

// Xbox 0x0015C9E0
void destroy_audio_core_source(AudioCoreSource*& source, AudioCoreSources& context)
{
    if (!audio_core_source_valid(source, context))
    {
        audio_core_debug(context.boundaries, "audioSoundSourceDestroy : soundSource : %p is no longer valid!\n", &source);
        return;
    }
    if (source->handle != nullptr)
    {
        if (audio_core_source_in_use(*source, context))
        {
            return;
        }
        context.boundaries.release(source->handle);
        source->handle = nullptr;
    }
    switch (source->mode)
    {
    case 0:
    case 1:
    case 5:
    case 6:
    case 7:
    case 8:
    {
        constexpr u32 lines[]{0x1B6, 0x1CF, 0, 0, 0, 0x1BB, 0x1C0, 0x1C5, 0x1CA};
        release_audio_core_allocation(source->attributes, "audioSoundSourceDestroy", lines[source->mode], 0, context.memory);
        source->attributes = nullptr;
        break;
    }
    default:
        break;
    }
    if (context.tail == source)
    {
        context.tail = source->previous;
    }
    unlink_audio_core_link(*source);
    unlink_audio_core_link(source->secondary);
    AudioCoreHeader* header = source;
    release_audio_core_object(header, "audioSoundSourceDestroy", 0x1E3, 0, context.memory);
    source = nullptr;
}

// Xbox 0x0015C800
void set_audio_core_source_position(AudioCoreSource& source, const Vec3& position,
    AudioCoreSources& context)
{
    if (!audio_core_source_valid(&source, context))
    {
        audio_core_debug(context.boundaries, "audioSoundSourceDestroy : soundSource : %p is no longer valid!\n", &source);
        return;
    }
    const Vec3 scaled{position.x * context.unit_scale, position.y * context.unit_scale, position.z * context.unit_scale};
    context.boundaries.position(source.handle, scaled, context.apply);
    std::memcpy(source.attributes, &position, 12);
}

// Xbox 0x0015C890
void set_audio_core_source_velocity(AudioCoreSource& source, const Vec3& velocity,
    AudioCoreSources& context)
{
    if (!audio_core_source_valid(&source, context))
    {
        audio_core_debug(context.boundaries, "audioSoundSourceDestroy : soundSource : %p is no longer valid!\n", &source);
        return;
    }
    const Vec3 scaled{velocity.x * context.unit_scale, velocity.y * context.unit_scale, velocity.z * context.unit_scale};
    context.boundaries.velocity(source.handle, scaled, context.apply);
    std::memcpy(static_cast<u8*>(source.attributes) + 0xC, &velocity, 12);
}

// Xbox 0x0015C930
void set_audio_core_source_cone(AudioCoreSource& source, const Vec3& direction,
    AudioCoreSources& context)
{
    if (!audio_core_source_valid(&source, context))
    {
        audio_core_debug(context.boundaries, "audioSoundSourceDestroy : soundSource : %p is no longer valid!\n", &source);
        return;
    }
    context.boundaries.cone(source.handle, direction, context.apply);
    std::memcpy(static_cast<u8*>(source.attributes) + 0x18, &direction, 12);
}

// Xbox 0x0015C290
void set_audio_core_source_reverb(AudioCoreSource& source, std::span<const u32, 9> values,
    AudioCoreSources& context)
{
    std::memcpy(static_cast<u8*>(source.attributes) + 0x28, values.data(), 36);
    context.boundaries.reverb(source.handle,
        std::span<const u32, 9>(reinterpret_cast<const u32*>(static_cast<u8*>(source.attributes) + 0x28), 9), context.apply);
    *(static_cast<u8*>(source.attributes) + 0x24) = 1;
}
}
