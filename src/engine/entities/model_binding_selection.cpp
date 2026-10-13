#include "model_binding_selection.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
const u8* next_effect_shape(const u8* record)
{
    const u8 stride = record[1];
    return stride != 0 ? record + stride : nullptr;
}
bool matches_effect_shape(const u8* record, u32 mask)
{
    u16 joint;
    std::memcpy(&joint, record + 8, 2);
    return (mask & (1u << (record[2] & 31))) != 0 && joint != 0xFFFF && record[0] == 0;
}
}

// Xbox 0x00051C60
bool advance_runtime_model_binding(const u8* block, const u8*& current, i16 kind)
{
    u32 count;
    std::memcpy(&count, block, 4);
    const u8* record = current == nullptr ? block + 4 : current + 0x34;
    const u8* end = block + 4 + size_t(count) * 0x34;
    while (reinterpret_cast<uintptr_t>(record) < reinterpret_cast<uintptr_t>(end))
    {
        u16 value;
        std::memcpy(&value, record, 2);
        if (std::bit_cast<i16>(value) == kind)
        {
            current = record;
            return true;
        }
        record += 0x34;
    }
    return false;
}

// Xbox 0x00040930
bool advance_actor_model_binding(const Actor& actor, const u8*& current, i16 kind)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 2)
                {
                    const u32 offset = read_u32(cpu, table + index * 8 + 4);
                    if (offset == 0)
                    {
                        return false;
                    }
                    return advance_runtime_model_binding(cpu.data() + offset, current, kind);
                }
                ++index;
            }
            while (index < count);
        }
    }
    return false;
}

// Xbox 0x000424C0
const u8* actor_physical_shape_block(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 4)
                {
                    const u32 offset = read_u32(cpu, table + index * 8 + 4);
                    return offset != 0 ? cpu.data() + offset : nullptr;
                }
                ++index;
            }
            while (index < count);
        }
    }
    return nullptr;
}

// Xbox 0x000EC750
const u8* select_actor_effect_shape(const Actor& actor, const u8* previous, u32 mask)
{
    if (mask == 0)
    {
        return nullptr;
    }
    const u8* block = actor_physical_shape_block(actor);
    i32 count;
    std::memcpy(&count, block, 4);
    const u8* first = block + 8;
    const u8* record = first;
    i32 index = 0;
    if (previous != nullptr)
    {
        if (count > 0)
        {
            do
            {
                if (record == previous)
                {
                    break;
                }
                record = next_effect_shape(record);
                ++index;
            }
            while (index < count);
        }
        record = next_effect_shape(record);
        ++index;
    }
    const i32 wrap_count = index >= count ? count : index;
    while (index < count)
    {
        if (matches_effect_shape(record, mask))
        {
            return record;
        }
        record = next_effect_shape(record);
        ++index;
    }
    index = 0;
    record = first;
    if (wrap_count > 0)
    {
        do
        {
            if (matches_effect_shape(record, mask))
            {
                return record;
            }
            record = next_effect_shape(record);
            ++index;
        }
        while (index < wrap_count);
    }
    return nullptr;
}
}
