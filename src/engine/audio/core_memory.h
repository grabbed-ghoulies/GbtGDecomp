#pragma once

#include "gbtg/types.h"
#include <functional>

namespace gbtg
{
struct AudioCoreLink
{
    AudioCoreLink* previous; // +0
    AudioCoreLink* next; // +4
};
struct AudioCoreHeader : AudioCoreLink
{
    u32 references; // +8
    u32 type; // +C
    char* name; // +10
};
struct AudioCoreMemory
{
    std::function<void*(size_t)> allocate; // DAT_004D9784, host heap
    std::function<void(void*)> release; // DAT_004D9788, host heap
    std::function<void(u32, u32, const char*, const char*, u32, u32)> debug; // DAT_004D978C
};
void append_audio_core_link(AudioCoreLink& head, AudioCoreLink& node); // Xbox 0x00159A30
void unlink_audio_core_link(AudioCoreLink& node); // Xbox 0x00159A90
void* allocate_audio_core_array(u32 count, u32 size, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory); // Xbox 0x0015BC50
AudioCoreHeader* allocate_audio_core_object(u32 xbox_size, size_t host_size,
    const char* name, u32 type, const char* caller, u32 line, u32 extra,
    AudioCoreMemory& memory); // Xbox 0x0015BD30
void release_audio_core_allocation(void* allocation, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory); // Xbox 0x0015B7B0
void release_audio_core_object(AudioCoreHeader*& object, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory); // Xbox 0x0015BAA0
}
