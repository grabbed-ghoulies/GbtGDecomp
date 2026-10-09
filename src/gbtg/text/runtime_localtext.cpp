#include "gbtg/runtime_localtext.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000FBF90
bool text_key_present(const char* key)
{
    if (key != nullptr && *key != '\0')
    {
        return true;
    }
    return false;
}

// Xbox 0x000FF3C0
void sanitize_localtext_key(char* output, const char* key)
{
    const char* end = key;
    do
    {
        ++end;
    }
    while (end[-1] != '\0');
    i32 index = 0;
    const i32 count = i32(end - key);
    if (0 < count)
    {
        do
        {
            const char value = key[index];
            if (('/' < value && value < ':') || ('@' < value && value < '[') ||
                ('`' < value && value < '{') || value == '_' || value == '\0')
            {
                *output = value;
                ++output;
            }
            ++index;
        }
        while (index < count);
    }
}

// Xbox 0x00162910
u32 runtime_localtext_state_size()
{
    return 0x14;
}

// Xbox 0x00162920
void initialize_runtime_localtext(std::span<const u8> file, RuntimeLocalTextState& state)
{
    const u8* chunk = file.data() + read_u32(file, 0xC);
    state.chunk = chunk;
    state.entries = chunk + 8;
    state.strings = reinterpret_cast<const char16_t*>(chunk + 8 +
        (read_u32({chunk, 8}, 4) + 1) * 6u);
    state.disabled = 0;
    state.retained = 0;
}

// Xbox 0x00162A80
u32 runtime_localtext_hash(const char* key)
{
    u32 hash = 0;
    char value = *key;
    while (i16(value) != 0)
    {
        hash = hash * 0x10u + u32(u16(i16(value)));
        const u32 high = hash & 0xF000u;
        ++key;
        if (i16(high) != 0)
        {
            hash ^= (high >> 8) | high;
        }
        value = *key;
    }
    return hash;
}

// Xbox 0x00162A20
i32 compare_localtext_id(std::uintptr_t key, const u8* record)
{
    return i32(u16(key)) - i32(read_u16({record, 2}, 0));
}

// Xbox 0x00167F86
const u8* runtime_binary_search(std::uintptr_t key, const u8* base, u32 count,
    u32 stride, const std::function<i32(std::uintptr_t, const u8*)>& compare)
{
    auto first = reinterpret_cast<std::uintptr_t>(base);
    auto last = first + std::bit_cast<i32>((count - 1u) * stride);
    if (first <= last)
    {
        do
        {
            u32 half = count >> 1;
            if (half == 0)
            {
                if (count == 0)
                {
                    return nullptr;
                }
                return compare(key, reinterpret_cast<const u8*>(first)) != 0 ?
                    nullptr : reinterpret_cast<const u8*>(first);
            }
            u32 index = half;
            if ((count & 1) == 0)
            {
                index = half - 1;
            }
            const auto middle = first + index * stride;
            const i32 result = compare(key, reinterpret_cast<const u8*>(middle));
            if (result == 0)
            {
                return reinterpret_cast<const u8*>(middle);
            }
            if (result < 0)
            {
                last = middle - stride;
                if ((count & 1) == 0)
                {
                    --half;
                }
            }
            else
            {
                first = middle + stride;
            }
            count = half;
        }
        while (first <= last);
    }
    return nullptr;
}

// Xbox 0x00162A30
const char16_t* runtime_localtext_by_id(const RuntimeLocalTextState* state, u32 id)
{
    if (state != nullptr && state->disabled == 0)
    {
        const u8* record = runtime_binary_search(id, state->entries,
            read_u32({state->chunk, 8}, 4), 6, compare_localtext_id);
        if (record != nullptr)
        {
            return state->strings + read_u32({record, 6}, 2);
        }
    }
    return u""; // PTR_DAT_00336908 -> Xbox 0x0020A4F4, empty UTF-16 string
}

// Xbox 0x00162950
u32 find_runtime_localtext(std::span<const RuntimeLocalTextKey> keys,
    const RuntimeLocalTextState& state, const char* key, const char16_t*& output)
{
    u16 id;
    if (std::strncmp(key, "mapname__servantsdormitory", 0x1B) == 0)
    {
        id = 0xC49;
    }
    else
    {
        id = u16(runtime_localtext_hash(key));
    }
    i32 index = 0;
    if (u16(keys.size()) != 0)
    {
        do
        {
            if (id == keys[index].hash)
            {
                if (std::strcmp(key, keys[index].name) == 0)
                {
                    id = keys[index].id;
                    break;
                }
            }
            ++index;
        }
        while (index < i32(u16(keys.size())));
    }
    output = runtime_localtext_by_id(&state, id);
    if (*output != 0)
    {
        return id;
    }
    return 0xFFFFFFFFu;
}
}
