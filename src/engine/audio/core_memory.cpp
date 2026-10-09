#include "core_memory.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x00159A30
void append_audio_core_link(AudioCoreLink& head, AudioCoreLink& node)
{
    AudioCoreLink* tail = &head;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &node;
    node.previous = tail;
    node.next = nullptr;
}

// Xbox 0x00159A90
void unlink_audio_core_link(AudioCoreLink& node)
{
    AudioCoreLink* previous = node.previous;
    for (AudioCoreLink* ancestor = previous; ancestor != nullptr; ancestor = ancestor->previous)
    {
    }
    previous->next = node.next;
    if (node.next != nullptr)
    {
        node.next->previous = previous;
    }
}

// Xbox 0x0015BC50
void* allocate_audio_core_array(u32 count, u32 size, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory)
{
    const u32 bytes = count * size;
    if (memory.debug)
    {
        memory.debug(0, bytes, ".\\audiocore.c", caller, line, extra);
    }
    void* result = memory.allocate(bytes);
    std::memset(result, 0, bytes);
    return result;
}

// Xbox 0x0015BD30
AudioCoreHeader* allocate_audio_core_object(u32 xbox_size, size_t host_size,
    const char* name, u32 type, const char* caller, u32 line, u32 extra,
    AudioCoreMemory& memory)
{
    if (memory.debug)
    {
        memory.debug(0, xbox_size, ".\\audiocore.c", caller, line, extra);
    }
    auto* result = static_cast<AudioCoreHeader*>(memory.allocate(host_size));
    std::memset(result, 0, host_size);
    const u32 name_size = static_cast<u32>(std::strlen(name)) + 1;
    if (memory.debug)
    {
        memory.debug(0, name_size, ".\\audiocore.c", caller, line, extra);
    }
    result->name = static_cast<char*>(memory.allocate(name_size));
    std::memset(result->name, 0, name_size);
    std::strcpy(result->name, name);
    result->references = 1;
    result->type = type;
    return result;
}

// Xbox 0x0015B7B0
void release_audio_core_allocation(void* allocation, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory)
{
    if (memory.debug)
    {
        memory.debug(1, 0, ".\\audiocore.c", caller, line, extra);
    }
    memory.release(allocation);
}

// Xbox 0x0015BAA0
void release_audio_core_object(AudioCoreHeader*& object, const char* caller,
    u32 line, u32 extra, AudioCoreMemory& memory)
{
    AudioCoreHeader* current = object;
    char* name = current->name;
    --current->references;
    if (memory.debug)
    {
        memory.debug(1, 0, ".\\audiocore.c", caller, line, extra);
    }
    memory.release(name);
    current->name = nullptr;
    current = object;
    if (memory.debug)
    {
        memory.debug(1, 0, ".\\audiocore.c", caller, line, 0);
    }
    memory.release(current);
    object = nullptr;
}
}
