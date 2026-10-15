#include "gbtg/bundle.h"
#include "gbtg/anim.h"
#include "gbtg/language.h"
#include "gbtg/resource_error.h"
#include "gbtg/engine_internal.h"
#include "gbtg/font.h"
#include "gbtg/gfx.h"
#include "gbtg/hash.h"
#include "gbtg/loctext.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "gbtg/texture.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>


#include "frontend_types.h"
#include "frontend.h"
#include "frontend_contents.h"
#include "frontend_keyboard.h"
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


void draw_contents(const Game& g);


// Contents widget, mode-table index 3. 0x000E02D0 sets the scene object's +0x94
// to 3 when the chosen slot's status is 2, 3, or 4, and that index's draw is
// 0x000D9640. 0x000D8B10 walks three records of 0x104 from 0x00428D04 and the
// layout table at 0x00306994 (stride 0x1C): centers (130, 200), (320, 200),
// (510, 200), rest norm 0.16, hot norm 0.25. The background at +0xC88 is the
// highlighted row's texture, centered, scaled up to the frame. 0x000D9D10
// lerps only the highlighted plate toward the hot norm. The save name sits at
// screen center + (0, -185) = (320, 55), scale 1.8. The highlighted row's
// loctext is the wide string at slot+4, placed at (320, 278), scale 1.4.
// The chapter line is the buffer at +0xA78, anchor (320, 330), scale 0.75.
// 0x000D8CD0 copies "Progress not set" from 0x00324CB4 into that buffer.
// 0x000DA040 overwrites it only after 0x00010DB10 copies a script id out of the
// save list. 0x0004B4D0 matches aid_script_ghoulies_chapter, the chapter number
// is the dword at [index*4 + 0x002FE698], and 0x00011EC70 supplies the scene
// number (a 0 is replaced with 1). Those two counts are formatted with the
// wide "%s %d" at 0x00324C50 onto gameslotcontents_chapter and
// gameslotcontents_scene, then joined with the wide "%s%s : %s" at 0x00324C3C
// and the scene name from 0x00011ED80. No save list means no id is copied, so
// no chapter number is written and the buffer stays the init literal.
#include "frontend_progress.inl"


// Xbox 0x0011EBC0. The chapter suffix identifies the length of the prefix
// ending in "_scene"; atol reads the scene number immediately after it.
int script_scene_number(std::string_view script)
{
    const int chapter = script_chapter_index(script);
    if (chapter == 0)
    {
        return 0;
    }
    constexpr std::string_view suffixes[] = {"void", "1", "2a", "2b", "2c", "3a", "3b", "3c", "4", "5"};
    const std::string prefix = "aid_script_ghoulies_chapter" + std::string(suffixes[chapter]) + "_scene";
    return int(std::atol(std::string(script.substr(prefix.size())).c_str()));
}


// Xbox 0x0011EC70.
int script_chapter_scene(std::string_view script)
{
    const int chapter = script_chapter_index(script);
    int scene = script_scene_number(script);
    switch (chapter)
    {
    case 2:
    case 3:
    case 4:
        for (int i = 2; i < chapter; ++i)
        {
            scene += kChapterSceneCounts[i];
        }
        break;
    case 5:
    case 6:
    case 7:
        for (int i = 5; i < chapter; ++i)
        {
            scene += kChapterSceneCounts[i];
        }
        break;
    }
    return scene;
}


// Xbox 0x0004B530.
int script_chapter_first_scene(int chapter)
{
    int scene = 0;
    for (int i = 1; i < chapter; ++i)
    {
        scene += kChapterSceneCounts[i];
    }
    return scene;
}


// Xbox 0x0011ED30.
int script_scene_index(std::string_view script)
{
    const int scene = script_scene_number(script);
    const int chapter = script_chapter_index(script);
    if (chapter == 0)
    {
        return -1;
    }
    if (scene != 0)
    {
        return script_chapter_first_scene(chapter) - 1 + scene;
    }
    return script_chapter_first_scene(chapter);
}


// Xbox 0x0011ED80: the ordinal maps through two separate tables before
// 0x000FF120 resolves the scenenames text key.
std::u16string script_scene_name(const Game& g, int scene)
{
    static std::map<std::string, std::u16string> strings;
    static bool loaded = false;
    if (!loaded)
    {
        const auto path = g.aids.game_dir / "loctext" / language_folder(g_language) /
                          aid_hash_hex(aid_hash("aid_loctext_ghoulies_script_scenenames"));
        const LocTable parsed = parse_loctext(read_file(path), "scenenames");
        for (const auto& [key, text] : parsed.keys_to_utf8)
        {
            strings.emplace(key, utf8_to_u16(text));
        }
        loaded = true;
    }
    const auto found = strings.find("scenenames__" + std::string(kSceneNameKeys[kSceneNameIndices[scene]]));
    return found == strings.end() ? std::u16string{} : found->second;
}


// Xbox 0x000DA040: retain the initialized buffer when either label lookup
// fails. The host save-list lookup exposes the selected blob's +0x102.
std::u16string contents_progress_line(const Game& g)
{
    constexpr char16_t kLiteral[] = u"Progress not set";
    const std::u16string chapter_label = gameselect_string(g, "gameslotcontents_chapter");
    if (chapter_label.empty())
    {
        return kLiteral;
    }
    const std::u16string scene_label = gameselect_string(g, "gameslotcontents_scene");
    if (scene_label.empty())
    {
        return kLiteral;
    }
    const char* script = save_script(g);
    if (script == nullptr)
    {
        return kLiteral;
    }
    int scene = script_chapter_scene(script);
    if (scene == 0)
    {
        scene = 1;
    }
    const int chapter = slot_chapter(g, g.slot);
    return chapter_label + u" " + utf8_to_u16(std::to_string(chapter)) +
           scene_label + u" " + utf8_to_u16(std::to_string(scene)) + u" : " +
           script_scene_name(g, script_scene_index(script));
}


void draw_contents(const Game& g)
{
    struct Plate
    {
        const char* texture;
        const char* label;
        float center_x;
    };
    constexpr Plate kPlates[3] = {
        {"gameselect_house", "gameslotcontents_playgame", 130.f},
        {"gameselect_zombieclapframe", "gameslotcontents_features", 320.f},
        {"gameselect_skellyscratchheadframe", "gameslotcontents_challenges", 510.f},
    };
    const int hot = g.contents_row >= 0 && g.contents_row < 3 ? g.contents_row : 0;
    const Plate& shown = kPlates[hot];
    // Xbox 0x000D9640 / 0x000DA1E0: the backdrop is a texture, not a movie.
    draw_menu_backdrop(g, g.contents_visual);
    for (int i = 0; i < 3; ++i)
    {
        if (i == hot)
        {
            continue;
        }
        const float w = 0.16f * kFrameW;
        const float h = 0.16f * kFrameH;
        if (const Game::UiImage* image = find_ui(g, kPlates[i].texture))
        {
            draw_plate_background(kPlates[i].center_x, 200.f, w + 12.f, h + 12.f, 0x80069764u);
            draw_sprite(image, kPlates[i].center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
        }
    }
    {
        const float fraction = g.contents_visual.fraction;
        const float w = ((0.25f - 0.16f) * fraction + 0.16f) * kFrameW;
        const float h = ((0.25f - 0.16f) * fraction + 0.16f) * kFrameH;
        if (const Game::UiImage* image = find_ui(g, shown.texture))
        {
            const float padding = (20.f - 12.f) * fraction + 12.f;
            draw_plate_background(shown.center_x, 200.f, w + padding, h + padding,
                                  menu_color(0xC43EF7B6u, 0x80069764u, fraction));
            draw_sprite(image, shown.center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
        }
        // Xbox 0x000D9640: +0x2B0 >= 7 copies the selected movie; +0x2B4
        // latches the first successful copy. Other rows draw their static textures.
        movie_note_visible(kContentsMovies[hot]);
        if (7 <= g.contents_movie_frames[hot])
        {
            const bool copied = movie_copy_frame(kContentsMovies[hot], g.contents_movie_images[hot]);
            if (!g.contents_movie_copied[hot] && copied)
            {
                g.contents_movie_copied[hot] = true;
            }
        }
        if (movie_is_open(kContentsMovies[hot]))
        {
            if (g.contents_movie_copied[hot])
            {
                // DAT_0032B284 = 7, DAT_0032B250 = 0.05. The Xbox
                // conversion truncates the clamped byte before submission.
                const float alpha = float(u8(std::clamp(float(g.contents_movie_frames[hot] - 7) *
                                                       0.05f * 255.f, 0.f, 255.f))) / 255.f;
                gfx_sprite_submit(g.contents_movie_images[hot], shown.center_x - w * 0.5f,
                                  200.f - h * 0.5f, w, h, 0.f, 0.f, 1.f, 1.f,
                                  {1.f, 1.f, 1.f, alpha});
            }
            if (g.contents_movie_frames[hot] < 100)
            {
                ++g.contents_movie_frames[hot];
            }
        }
    }

    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    const std::u16string label = gameselect_string(g, shown.label);
    const std::u16string chapter = contents_progress_line(g);
    auto& text_pool = g.contents_text;
    int& last_row = text_pool.selection;
    GooLine& name_line = text_pool.name;
    GooLine& label_line = text_pool.title;
    GooLine& chapter_line = text_pool.chapter;
    auto& departing_labels = text_pool.departing_names;
    auto& departing_chapters = text_pool.departing_chapters;
    u32& label_generation = text_pool.generation;
    if (label_generation != g.contents_visual.generation)
    {
        if (!label_line.text.empty())
        {
            end_goo(label_line);
            departing_labels.push_back(label_line);
        }
        if (!chapter_line.text.empty())
        {
            end_goo(chapter_line);
            departing_chapters.push_back(chapter_line);
        }
        label_line = GooLine{};
        chapter_line = GooLine{};
        label_generation = g.contents_visual.generation;
    }
    if (name_line.slot != g.slot || name_line.text != name)
    {
        arm_goo(name_line, name, g.time, g.slot);
    }
    if (g.contents_visual.fraction >= 1.f && (last_row != hot || label_line.text != label))
    {
        arm_goo(label_line, label, g.time, hot);
    }
    if (g.contents_visual.fraction >= 1.f && hot == 0 && chapter_line.text != chapter)
    {
        arm_goo(chapter_line, chapter, g.time, -1);
    }
    last_row = hot;
    draw_goo_line(font, name_line, 320.f, 55.f, 1.8f, g, true);
    for (const GooLine& line : departing_labels)
    {
        draw_goo_line(font, line, 320.f, 278.f, 1.4f, g, true);
    }
    for (const GooLine& line : departing_chapters)
    {
        draw_goo_line(font, line, 320.f, 330.f, 0.75f, g, true);
    }
    std::erase_if(departing_labels, [](const GooLine& line)
    {
        return line.removed;
    });
    std::erase_if(departing_chapters, [](const GooLine& line)
    {
        return line.removed;
    });
    draw_goo_line(font, label_line, 320.f, 278.f, 1.4f, g, true);
    draw_goo_line(font, chapter_line, 320.f, 330.f, 0.75f, g, true);

    // 0x000DAA37 calls 0x000142180 with the prompt set for the highlighted row.
    // 0x0001428F0 walks that set in record order, stride 0x48.
    // Play (0x003069F0): select, move, back, info.
    // +0xF24 == 3 would select 0x00306D00 instead, which has no select entry.
    // 0x000DA040 leaves +0xF24 at 2 when 0x00010DC60 finds no save record.
    // Features (0x00307010) and Challenges (0x00307320): select, move, back.
    const char* prompt_keys[4] = {
        "gameslotcontents_selectprompt",
        "gameslotcontents_moveprompt",
        "gameslotcontents_backprompt",
        nullptr,
    };
    int prompt_count = 3;
    if (hot == 0)
    {
        prompt_keys[3] = "gameslotcontents_infoprompt";
        prompt_count = 4;
    }
    auto& prompts = text_pool.prompts;
    float column = 0.f;
    const char* all_keys[] = {
        "gameslotcontents_selectprompt", "gameslotcontents_moveprompt",
        "gameslotcontents_backprompt", "gameslotcontents_infoprompt",
    };
    for (const char* key : all_keys)
    {
        column = std::max(column, text_width(font, gameselect_string(g, key), 0.76f));
    }
    // 0x001428F0 returns maximum text width + 32 * icon scale + 20.
    column += 32.f * 0.85f + 20.f;
    for (int i = 0; i < prompt_count; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
    }
    const int codes[] = {0, 7, 1, 3};
    // 0x000DA580 uses the outer safe rectangle, scale .85 / .76.
    draw_menu_prompts(g, font, prompts, codes, prompt_count,
                      64.f, 428.f, 0.85f, 0.76f, 17.6f, -13.6f, column);
}

} // namespace frontend_detail

} // namespace gbtg
