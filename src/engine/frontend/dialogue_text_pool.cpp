#include "dialogue_text_pool.h"
#include "dialogue_text_metrics.h"
#include "../entities/ghouly_spawn_record.h"
#include "../entities/ghouly_box_decoder.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/entity.h"
#include <bit>
#include <cstring>

namespace gbtg
{
void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

// Xbox 0x0005F390
void allocate_dialogue_text_pool(RuntimeDialogueTextPool*& output, i32 font,
    u32 variant, u32 flags, GhoulySpawnPool*& registry)
{
    output = static_cast<RuntimeDialogueTextPool*>(engine_heap_allocate(sizeof(RuntimeDialogueTextPool)));
    auto* allocation = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + 0x48E0));
    allocation->stride = 0x1238;
    allocation->grow = 4;
    allocation->begin = reinterpret_cast<u8*>(allocation + 1);
    allocation->cursor = allocation->begin;
    allocation->limit = allocation->begin + 0x48E0;
    output->allocation = allocation;
    output->records = {allocation->begin, 0};
    output->font = font;
    output->variant = variant;
    output->flags = flags;
    output->origin[0] = 0.f;
    output->origin[1] = 0.f;
    output->origin[2] = 0.f;
    output->wrap_width = 0.f;
    output->end_time = 0.5f;
    output->enabled = 1;
    output->offsets[0] = 0.f;
    output->offsets[1] = 0.f;
    if (registry->cursor == registry->limit)
    {
        registry = grow_ghouly_spawn_pool(registry);
    }
    u8* entry = registry->cursor;
    registry->cursor += registry->stride;
    std::memcpy(entry, &output, sizeof(output));
}

// Xbox 0x0005F460
void release_dialogue_text_pool(RuntimeDialogueTextPool* pool, GhoulySpawnPool& registry)
{
    auto* allocation = pool->allocation;
    if (allocation != nullptr)
    {
        const u32 count = u32((registry.cursor - registry.begin) / registry.stride);
        allocation->cursor = allocation->begin;
        u32 index = 0;
        if (count != 0)
        {
            u8* entry = registry.begin;
            do
            {
                RuntimeDialogueTextPool* current;
                std::memcpy(&current, entry, sizeof(current));
                if (current == pool)
                {
                    u8* destination = registry.begin + registry.stride * index;
                    if (destination < registry.cursor - registry.stride)
                    {
                        engine_copy_bytes(destination, destination + registry.stride,
                            u32(registry.cursor - destination - registry.stride));
                    }
                    registry.cursor -= registry.stride;
                    break;
                }
                ++index;
                entry += registry.stride;
            }
            while (index < count);
        }
        allocation = pool->allocation;
        if (allocation != nullptr)
        {
            engine_heap_free(allocation);
            pool->allocation = nullptr;
        }
    }
    engine_heap_free(pool);
}

// Xbox 0x0005F5C0
u32 create_dialogue_text_record(RuntimeDialogueTextPool& pool, const char16_t* text,
    float x, float y, float scale_x, float scale_y, float lifetime,
    i32 centered, i32 panel, i32 sticky, const RuntimeTextMetrics& metrics,
    float viewport_width, i32& next_id, u32& random_state)
{
    auto* allocation = pool.allocation;
    if (allocation->cursor == allocation->limit)
    {
        allocation = grow_ghouly_spawn_pool(allocation);
        pool.allocation = allocation;
    }
    u8* record = allocation->cursor;
    allocation->cursor += allocation->stride;
    pool.records = {allocation->begin, size_t(allocation->cursor - allocation->begin)};
    float prefix_scale;
    float prefix_x;
    float prefix_y;
    const char16_t* visible = parse_dialogue_text_transform(text, prefix_scale, prefix_x, prefix_y);
    copy_xapi_wide_text(reinterpret_cast<char16_t*>(record + 4), visible);
    if (next_id == -1)
    {
        next_id = 1;
    }
    const u32 id = u32(next_id);
    next_id = std::bit_cast<i32>(u32(next_id) + 1u);
    const auto word = [record](size_t offset, u32 value)
    {
        std::memcpy(record + offset, &value, 4);
    };
    const auto number = [record](size_t offset, float value)
    {
        std::memcpy(record + offset, &value, 4);
    };
    word(0, id);
    number(0x1204, prefix_x + x);
    number(0x1208, prefix_y + y);
    number(0x120C, prefix_scale * scale_x);
    number(0x1210, prefix_scale * scale_y);
    word(0x1218, u32(centered));
    word(0x121C, u32(sticky));
    number(0x1220, 0.f);
    word(0x1224, u32(panel));
    if (sticky != 0)
    {
        lifetime = 60.f;
    }
    number(0x1214, lifetime);
    const char16_t* cursor = reinterpret_cast<const char16_t*>(record + 4);
    i32 index = 0;
    do
    {
        if (*cursor == 0)
        {
            break;
        }
        number(0x604 + index * 4, lifetime);
        number(0xA04 + index * 4, 0.f);
        random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
        number(0x204 + index * 4,
            (std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 0.25f);
        number(0xE04 + index * 4, 0.f);
        ++index;
        ++cursor;
    }
    while (index < 0x100);
    if (panel != 0)
    {
        float dimensions[2];
        measure_runtime_text_block(metrics, pool.font, reinterpret_cast<const char16_t*>(record + 4),
            prefix_scale * scale_x, pool.wrap_width, viewport_width, dimensions);
        float left = prefix_x + x;
        if (centered != 0)
        {
            left -= dimensions[0] * 0.5f;
        }
        number(0x1228, left - 25.f);
        number(0x122C, (prefix_y + y) - 25.f);
        number(0x1230, dimensions[0] + 50.f);
        number(0x1234, dimensions[1] + 50.f);
    }
    return id;
}

// Xbox 0x0005F7E0
u32 end_dialogue_text_record(RuntimeDialogueTextPool& pool, u32 id,
    std::span<const RuntimeTextFont* const> fonts, const u8* fallback_texture,
    u32& random_state)
{
    size_t offset = 0;
    while (true)
    {
        if (pool.records.size() <= offset)
        {
            return 0;
        }
        if (read_u32(pool.records, offset) == id)
        {
            break;
        }
        offset += 0x1238;
    }
    size_t text = offset + 4;
    if (read_u16(pool.records, text) != 0)
    {
        size_t age = offset + 0x604;
        do
        {
            float time;
            if (!(pool.end_time > 0.f))
            {
                const auto& font = *fonts[pool.font];
                const u32 code = read_u16(pool.records, offset + 4);
                const u8* texture;
                if (i32(code) < font.first || font.last < i32(code))
                {
                    texture = fallback_texture;
                }
                else
                {
                    const auto& glyph = font.glyphs[code - font.first];
                    texture = glyph.texture;
                    // MOVZX followed by CMP -1: the retail marker comparison
                    // is against 0xFFFFFFFF, not against a 16-bit 0xFFFF.
                    if (texture == nullptr || u32(glyph.marker) == 0xFFFFFFFFu)
                    {
                        texture = fallback_texture;
                    }
                }
                const float duration = texture == nullptr ? 0.f :
                    float(texture[0x11]) / float(texture[0x10]);
                random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
                time = (std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 0.25f + duration;
            }
            else
            {
                random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
                time = (std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 0.25f + pool.end_time;
            }
            const float maximum = read_f32(pool.records, age + 0x400);
            if (time > maximum)
            {
                std::memcpy(pool.records.data() + age, &maximum, 4);
            }
            else
            {
                const float current = read_f32(pool.records, age);
                // COMISS current,time; JC skips both lesser and unordered.
                if (current >= time)
                {
                    std::memcpy(pool.records.data() + age, &time, 4);
                }
            }
            text += 2;
            age += 4;
        }
        while (read_u16(pool.records, text) != 0);
    }
    const u32 sticky = 0;
    std::memcpy(pool.records.data() + offset + 0x121C, &sticky, 4);
    return 1;
}
}
