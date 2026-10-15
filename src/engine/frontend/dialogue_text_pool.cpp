#include "dialogue_text_pool.h"
#include "dialogue_draw_context.h"
#include "dialogue_text_metrics.h"
#include "../entities/ghouly_spawn_record.h"
#include "../entities/ghouly_box_decoder.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/entity.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000602E0
static u32 runtime_text_glyph_cycle_frame(const u8* texture, float duration, float remaining)
{
    const u32 frames = texture[0x11];
    const float rate = (1.f / duration) * static_cast<float>(frames);
    if (rate != 0.f)
    {
        return u32(static_cast<i32>(rate * remaining) % static_cast<i32>(frames)) & 0xFF;
    }
    return 0;
}

// Xbox 0x00060070
static u32 runtime_text_glyph_frame(i32 font, const char16_t* code, float remaining, u32 sticky, float duration,
    float lifetime, RuntimeDialogueDrawContext& context)
{
    const RuntimeTextFont& table = *context.metrics.fonts[font];
    const u8* texture = nullptr;
    const i32 value = *code;
    if (table.first <= value && value <= table.last)
    {
        texture = table.glyphs[size_t(value - table.first)].texture;
    }
    if (texture == nullptr)
    {
        texture = context.fallback_texture;
    }
    if (texture == nullptr)
    {
        return 0xFFFFFFFFu;
    }
    if (sticky == 0 && remaining < std::bit_cast<float>(0x3A83126Fu))
    {
        return 0;
    }
    const u32 frames = texture[0x11];
    if (lifetime != 0.f)
    {
        if (duration <= 0.f)
        {
            duration = static_cast<float>(frames) / static_cast<float>(texture[0x10]);
        }
        if (lifetime - duration <= remaining)
        {
            const float rate = (1.f / duration) * static_cast<float>(frames);
            if (rate == 0.f)
            {
                return 0;
            }
            return u32(static_cast<i32>((lifetime - remaining) * rate) % static_cast<i32>(frames)) & 0xFF;
        }
        if (remaining <= duration && sticky == 0)
        {
            return runtime_text_glyph_cycle_frame(texture, duration, remaining);
        }
    }
    return frames - 1;
}

// Xbox 0x0005E950
static void draw_runtime_pooled_dialogue_text(const char16_t* text, float& x, float& y, u32 centered, u32 frame,
    float speed, float height, float time, i32 shadow, const u32* frames, u32 seed_mode,
    RuntimeDialogueDrawContext& context)
{
    if (seed_mode != 0xFFFFFFFFu)
    {
        if (seed_mode != 0)
        {
            context.state.random = seed_mode;
        }
        else
        {
            context.state.random = static_cast<u32>(context.counter()) - context.counter_baseline;
        }
    }
    if (centered == 0)
    {
        draw_runtime_dialogue_text(text, x, y, frame, frames, speed, height, time, shadow, context);
        return;
    }
    draw_runtime_centered_dialogue_text(text, x, y, frame, frames, speed, height, time, seed_mode, shadow,
        context);
}

// Xbox 0x0005FC30
static u32 draw_runtime_dialogue_text_record(RuntimeDialogueTextPool& pool, i32 index,
    RuntimeDialogueDrawContext& context)
{
    auto* allocation = pool.allocation;
    std::span<u8> record{allocation->begin + index * allocation->stride, allocation->stride};
    const auto real = [&record](size_t offset)
    {
        return read_f32(record, offset);
    };
    const auto put_real = [&record](size_t offset, float value)
    {
        std::memcpy(record.data() + offset, &value, 4);
    };
    const auto put_word = [&record](size_t offset, u32 value)
    {
        std::memcpy(record.data() + offset, &value, 4);
    };
    const auto& clock = context.dialogue;
    const auto step = [&clock]()
    {
        return clock.freeze <= 0.f ? clock.dt * clock.scene_scale : 0.f;
    };
    u32 expired = 0;
    constexpr float threshold = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
    size_t length = 0;
    while (read_u16(record, 4 + length * 2) != 0)
    {
        const size_t offset = length * 4;
        if (threshold <= float(real(0x204 + offset) == 0.f))
        {
            const u32 glyph = runtime_text_glyph_frame(pool.font,
                reinterpret_cast<const char16_t*>(record.data() + 4 + length * 2), real(0x604 + offset),
                read_u32(record, 0x121C), pool.end_time, real(0x1214), context);
            put_word(0xE04 + offset, glyph);
            const float advance = step();
            if (0.f < real(0x604 + offset))
            {
                const float remaining = real(0x604 + offset) - advance;
                put_real(0x604 + offset, remaining);
                if (remaining <= 0.f)
                {
                    put_real(0x604 + offset, 0.f);
                }
            }
            put_real(0xA04 + offset, real(0xA04 + offset) + step());
        }
        else
        {
            const float advance = step();
            if (0.f < real(0x204 + offset))
            {
                const float delay = real(0x204 + offset) - advance;
                put_real(0x204 + offset, delay);
                if (delay <= 0.f)
                {
                    put_real(0x204 + offset, 0.f);
                }
            }
        }
        if (real(0x604 + offset) < threshold && read_u32(record, 0x121C) == 0)
        {
            ++expired;
            put_word(0xE04 + offset, 0);
        }
        ++length;
    }
    if (expired == length)
    {
        u8* destination = record.data();
        u8* last = allocation->cursor - allocation->stride;
        if (destination < last)
        {
            std::memmove(destination, destination + allocation->stride, size_t(last - destination));
        }
        allocation->cursor -= allocation->stride;
        pool.records = {allocation->begin, size_t(allocation->cursor - allocation->begin)};
        return 1;
    }
    if (read_u32(record, 0x1224) != 0)
    {
        throw Error("untranslated Xbox 0x00060160 / 0x00061360: pooled text panel");
    }
    float x = real(0x1204);
    float y = real(0x1208);
    context.metrics.scale_x = real(0x120C);
    context.metrics.scale_y = real(0x1210);
    const RuntimeTextFont& font = *context.metrics.fonts[context.state.font];
    context.state.half_height = static_cast<float>(font.drop) * context.metrics.scale_y * .5f;
    context.state.half_width = static_cast<float>(font.cell) * context.metrics.scale_x * .5f;
    draw_runtime_pooled_dialogue_text(reinterpret_cast<const char16_t*>(record.data() + 4), x, y,
        read_u32(record, 0x1218), 0xF, pool.origin[0], pool.origin[1], real(0x1220), i32(pool.enabled),
        reinterpret_cast<const u32*>(record.data() + 0xE04), std::bit_cast<u32>(pool.origin[2]), context);
    put_real(0x1220, real(0x1220) + step());
    return 0;
}

// Xbox 0x0005FB80
static void draw_runtime_dialogue_text_pool(RuntimeDialogueTextPool& pool,
    RuntimeDialogueDrawContext& context)
{
    reset_dialogue_font(pool.font, context);
    context.state.wrap = pool.wrap_width;
    context.state.shadow_x = pool.offsets[0];
    context.state.shadow_y = pool.offsets[1];
    auto* allocation = pool.allocation;
    u8* cursor = allocation->begin;
    if (pool.variant == 0)
    {
        if (cursor < allocation->cursor)
        {
            draw_runtime_dialogue_text_record(pool, 0, context);
        }
    }
    else
    {
        u8* end = allocation->cursor;
        i32 index = 0;
        while (cursor < end)
        {
            if (draw_runtime_dialogue_text_record(pool, index, context) != 0)
            {
                cursor -= 0x1238;
                end -= 0x1238;
                --index;
            }
            cursor += 0x1238;
            ++index;
        }
    }
    --context.override_count;
    context.state.active = 0;
    if (context.override_count == 0)
    {
        context.fog_disabled = 0;
    }
}

// Xbox 0x0005F330
void draw_runtime_dialogue_text_pools(GhoulySpawnPool& registry, u32 scene_flags,
    RuntimeDialogueDrawContext& context)
{
    const size_t count = size_t(registry.cursor - registry.begin) / registry.stride;
    for (size_t index = 0; index < count; ++index)
    {
        RuntimeDialogueTextPool* pool;
        std::memcpy(&pool, registry.begin + index * registry.stride, sizeof(pool));
        if (pool->allocation != nullptr && (pool->flags & scene_flags) == 0)
        {
            draw_runtime_dialogue_text_pool(*pool, context);
        }
    }
}
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

namespace gbtg
{
// Xbox 0x0005F970
u32 end_all_dialogue_text_records(RuntimeDialogueTextPool& pool, std::span<const RuntimeTextFont* const> fonts,
    const u8* fallback_texture, u32& random_state)
{
    auto* allocation = pool.allocation;
    u8* begin = allocation->begin;
    u8* end = allocation->cursor;
    if (end <= begin)
    {
        return 0;
    }
    u32 count = u32(end - begin - 1) / 0x1238 + 1;
    u8* text = begin + 4;
    const auto real = [](const u8* at)
    {
        float value;
        std::memcpy(&value, at, 4);
        return value;
    };
    do
    {
        u16 first;
        std::memcpy(&first, text, 2);
        if (first != 0)
        {
            u8* age = text + 0x600;
            u8* character = text;
            do
            {
                float time;
                if (!(pool.end_time > 0.f))
                {
                    const auto& font = *fonts[pool.font];
                    const u32 code = first; // The record's first character, as the dump reads it.
                    const u8* texture;
                    if (i32(code) < font.first || font.last < i32(code))
                    {
                        texture = fallback_texture;
                    }
                    else
                    {
                        const auto& glyph = font.glyphs[code - font.first];
                        texture = glyph.texture;
                        if (texture == nullptr || u32(glyph.marker) == 0xFFFFFFFFu)
                        {
                            texture = fallback_texture;
                        }
                    }
                    const float duration = texture == nullptr ? 0.f : float(texture[0x11]) / float(texture[0x10]);
                    random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
                    time = (std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 0.25f + duration;
                }
                else
                {
                    random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
                    time = (std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 0.25f
                        + pool.end_time;
                }
                const float maximum = real(age + 0x400);
                if (time > maximum)
                {
                    std::memcpy(age, &maximum, 4);
                }
                else if (real(age) >= time)
                {
                    std::memcpy(age, &time, 4);
                }
                character += 2;
                age += 4;
                u16 next;
                std::memcpy(&next, character, 2);
                if (next == 0)
                {
                    break;
                }
            }
            while (true);
        }
        std::memset(text + 0x1218, 0, 4);
        text += 0x1238;
        --count;
    }
    while (count != 0);
    return 1;
}

// Xbox 0x0005FB10
u32 remove_dialogue_text_record(RuntimeDialogueTextPool& pool, u32 id)
{
    auto* allocation = pool.allocation;
    u8* record = allocation->begin;
    u8* const end = allocation->cursor; // Read once: the walk continues to the old end after a removal.
    u32 index = 0;
    while (record < end)
    {
        u32 word;
        std::memcpy(&word, record, 4);
        if (word == id)
        {
            const u32 stride = allocation->stride;
            u8* destination = allocation->begin + stride * index;
            u8* last = allocation->cursor - stride;
            if (destination < last)
            {
                std::memmove(destination, destination + stride, size_t(last - destination));
            }
            allocation->cursor -= allocation->stride;
        }
        record += 0x1238;
        ++index;
    }
    pool.records = {allocation->begin, size_t(allocation->cursor - allocation->begin)};
    return 0;
}
}
