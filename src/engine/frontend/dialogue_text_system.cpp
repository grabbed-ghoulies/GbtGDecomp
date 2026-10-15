#include "dialogue_text_system.h"
#include "../entities/entity_heap.h"
#include "gbtg/resource_error.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
// DAT_002FF2E8 (English), DAT_002FF3E8 (Chinese), DAT_002FF4E8 (Japanese), DAT_002FF5E8 (Korean); stride 0x80.
constexpr const char* kEnglishFonts[2]{"aid_font_localisation_gloopy_english_gloopyfont",
    "aid_font_localisation_dialog_english_dialogfont"};
constexpr const char* kChineseFonts[2]{"aid_font_localisation_gloopy_chinese_gloopyfont",
    "aid_font_localisation_dialog_chinese_dialogfont"};
constexpr const char* kJapaneseFonts[2]{"aid_font_localisation_gloopy_japanese_gloopyfont",
    "aid_font_localisation_dialog_japanese_dialogfont"};
constexpr const char* kKoreanFonts[2]{"aid_font_localisation_gloopy_korean_gloopyfont",
    "aid_font_localisation_dialog_korean_dialogfont"};
}

// Xbox 0x0005F210
void count_runtime_dialogue_tables(RuntimeDialogueTextSystem& system)
{
    for (size_t table = 0; table < 2; ++table)
    {
        const u8* rows = system.table_sources[table];
        i32 count = 0;
        u16 word;
        std::memcpy(&word, rows, 2);
        while (word != 0)
        {
            ++count;
            std::memcpy(&word, rows + size_t(count) * 0xC, 2);
        }
        system.tables[table] = {count, rows};
    }
}

// Xbox 0x00060320
void initialize_runtime_dialogue_text(u32 language, RuntimeDialogueTextSystem& system,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    const auto nullchar = acquire_animation_resource(registry, "aid_texture_ghoulies_font_nullchar");
    if (nullchar.data() == nullptr)
    {
        resource_error(error, 2);
    }
    system.fallback_texture = nullchar;
    const i32 selected = std::bit_cast<i32>(language);
    system.paragraph_disabled = selected < 6 || 8 < selected ? 0 : 1;
    const char* const* names = selected == 6 ? kKoreanFonts : selected == 7 ? kChineseFonts
        : selected == 8 ? kJapaneseFonts : kEnglishFonts;
    for (size_t index = 0; index < 2; ++index)
    {
        std::span<const u8> font;
        if (std::memcmp(names[index], "aid_", 4) == 0)
        {
            font = acquire_animation_resource(registry, names[index]);
            if (font.data() == nullptr)
            {
                resource_error(error, 2);
            }
        }
        system.fonts[index] = font;
    }
    auto& draw = system.draw;
    draw.color = 0xFFFFFFFFu; // DAT_00314F48.
    draw.text_scale_x = 1.f; // DAT_0032B0F8.
    draw.text_scale_y = 1.f;
    draw.active = 0;
    draw.retained = 0;
    const auto selected_font = system.fonts.at(size_t(draw.font));
    draw.half_height = float(std::bit_cast<i32>(read_u32(selected_font, 0x10))) * 0.5f; // DAT_0032B0F4.
    draw.shadow_color = 0x80000000u;
    draw.half_width = float(std::bit_cast<i32>(read_u32(selected_font, 0xC))) * 0.5f;
    draw.angle = 0.f;
    draw.retained_f4 = 0;
    draw.shadow_x = 2.f; // DAT_0032B110.
    draw.shadow_y = 2.f;
    draw.wrap = 0.f;
    // The registry: header, then four pool-pointer rows (growth 4). Retail rows are 4 bytes; the host rows hold a
    // native pointer, so they are pointer-width.
    constexpr u16 row = sizeof(RuntimeDialogueTextPool*);
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + 4 * row)));
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->stride = row;
    pool->grow = 4;
    pool->limit = pool->begin + 4 * row;
    system.registry = pool;
    pool->cursor = pool->begin;
    system.registry_ready = 1;
    count_runtime_dialogue_tables(system);
}
}
