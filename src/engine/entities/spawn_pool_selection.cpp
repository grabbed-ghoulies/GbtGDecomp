#include "spawn_pool_selection.h"
#include "actor_transform.h"

#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
float random_fraction(u32& state)
{
    state = state * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}

i32 random_index(u32& state, i32 count)
{
    return _mm_cvtt_ss2si(_mm_set_ss(random_fraction(state) * float(count)));
}

i32 select_index(SpawnPoolSelection& selection, i32 count, u32& state)
{
    i32 index;
    if (selection.mode == 1)
    {
        index = random_index(state, count);
    }
    else if (selection.mode == 4)
    {
        i32 attempts = 10;
        do
        {
            index = random_index(state, count);
            if (index != selection.previous)
            {
                break;
            }
            --attempts;
        }
        while (attempts != 0);
    }
    else
    {
        index = std::bit_cast<i32>(selection.sequence) % count;
    }
    ++selection.sequence;
    selection.previous = index;
    return index;
}
}

// Xbox 0x000F3840
u8* select_spawn_name(SpawnPoolSelection& selection, u32& random_state)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    const i32 index = select_index(selection, count, random_state);
    return selection.pool->begin + selection.pool->stride * index;
}

// Xbox 0x000F36C0
u8* select_spawn_attributes(SpawnPoolSelection& selection, u32& random_state)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    if (count < 1)
    {
        return nullptr;
    }
    const i32 index = select_index(selection, count, random_state);
    return selection.pool->begin + selection.pool->stride * index;
}

// Xbox 0x000F5280
float select_spawn_interval(SpawnPoolSelection& selection, u32& random_state)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    if (count == 0)
    {
        return 0.f;
    }
    auto* values = reinterpret_cast<float*>(selection.pool->begin);
    float result;
    if (selection.mode == 1)
    {
        result = (values[1] - values[0]) * random_fraction(random_state) + values[0];
    }
    else if (selection.mode == 3)
    {
        result = values[0];
        const float next = result + values[2];
        values[0] = next;
        if (values[2] > 0.f)
        {
            if (next >= values[1])
            {
                values[0] = values[1];
                values[2] = 0.f;
            }
        }
        else if (values[2] < 0.f)
        {
            if (values[1] >= next)
            {
                values[0] = values[1];
                values[2] = 0.f;
            }
        }
    }
    else
    {
        result = values[std::bit_cast<i32>(selection.sequence) % count];
    }
    ++selection.sequence;
    return result;
}

// Xbox 0x000F5610
i32 select_unused_spawn_controller(SpawnPoolSelection& selection, u32& random_state)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    auto* words = reinterpret_cast<u32*>(selection.pool->begin);
    if (count > 0)
    {
        i32 index = 0;
        while (index < count && words[index * 2 + 1] != 0)
        {
            ++index;
        }
        if (index == count)
        {
            for (i32 reset = 0; reset < count; ++reset)
            {
                words[reset * 2 + 1] = 0;
            }
        }
    }
    i32 index;
    do
    {
        index = random_index(random_state, count);
    }
    while (words[index * 2 + 1] != 0);
    return index;
}

// Xbox 0x000F50D0
i32 count_spawn_controller_attempts(std::span<const u8, 256> attempts)
{
    i32 count = 0;
    for (i32 group = 0; group < 64; ++group)
    {
        if (attempts[group * 4] != 0)
        {
            ++count;
        }
        if (attempts[group * 4 + 1] != 0)
        {
            ++count;
        }
        if (attempts[group * 4 + 2] != 0)
        {
            ++count;
        }
        if (attempts[group * 4 + 3] != 0)
        {
            ++count;
        }
    }
    return count;
}

// Xbox 0x000F5530
i32 select_nearest_spawn_controller(const SpawnPoolSelection& selection, const Vec3& position,
    const std::unordered_map<const GhoulyController*, Actor*>& actors)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    float distance = std::bit_cast<float>(0x4E6E6B28u);
    i32 selected = -1;
    for (i32 index = 0; index < count; ++index)
    {
        u32 key;
        std::memcpy(&key, selection.pool->begin + index * 8, 4);
        for (const auto& entry : g_ghouly_controllers)
        {
            if (entry.key == key)
            {
                if (entry.controller != nullptr)
                {
                    const Vec3 point = actor_world_position(*actors.at(entry.controller));
                    const float x = point.x - position.x;
                    const float y = point.y - position.y;
                    const float z = point.z - position.z;
                    const float candidate = x * x + z * z + y * y;
                    if (candidate < distance)
                    {
                        distance = candidate;
                        selected = index;
                    }
                }
                break;
            }
        }
    }
    return selected;
}

// Xbox 0x000F5410
u32 select_spawn_controller(SpawnPoolSelection& selection, const Vec3& position, u32& random_state,
    const std::unordered_map<const GhoulyController*, Actor*>& actors)
{
    const i32 count = i32(selection.pool->cursor - selection.pool->begin) / selection.pool->stride;
    i32 index;
    switch (selection.mode)
    {
    case 1:
        index = random_index(random_state, count);
        break;
    case 2:
        index = select_nearest_spawn_controller(selection, position, actors);
        break;
    case 4:
    {
        i32 attempts = 10;
        do
        {
            index = random_index(random_state, count);
            if (index != selection.previous)
            {
                break;
            }
            --attempts;
        }
        while (attempts != 0);
        break;
    }
    case 5:
        index = select_unused_spawn_controller(selection, random_state);
        break;
    default:
        index = std::bit_cast<i32>(selection.sequence) % count;
        break;
    }
    selection.previous = index;
    ++selection.sequence;
    auto* row = selection.pool->begin + selection.pool->stride * index;
    u32 used;
    std::memcpy(&used, row + 4, 4);
    ++used;
    std::memcpy(row + 4, &used, 4);
    u32 key;
    std::memcpy(&key, row, 4);
    return key;
}
}
