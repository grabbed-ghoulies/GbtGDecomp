#include "../camera/camera.h"
#include "../audio/listener_settings.h"
#include "gbtg/bundle.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/dialogue.h"
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
#include "frontend_captions.h"
#include "frontend_contents.h"
#include "frontend_keyboard.h"
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


// One row of the slot table at 0x0042902C, stride 0x124.
// Init copies the center into the widget at +0x428 / +0x42C.
// 0x000DF9A0 case 6 writes the sprite scale at +0x434 / +0x438:
//   screen * rest_norm / texture_size
// and the hit box at +0x510 / +0x514: rest_norm * {640,480} + DAT_00428FBC (12).
// 0x000DE540 lerps the selected slot from that rest scale to the 0.25 hot scale.
// 0x0005CA60 then centers a quad of texture_size * scale on +0x428. The box is
// only the argument to 0x00061360.
struct SlotPlate
{
    const char* texture;
    float center_x;
    float center_y;
    float rest_x;
    float rest_y;
    float hot_x;
    float hot_y;
};


constexpr SlotPlate kSlots[3] = {
    {"gameselect_oneframe", 130.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_twoframe", 320.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_threeframe", 510.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
};


// Xbox 0x0004B4D0. Suffixes at 0x004232F4 map through 0x002FE698.
int script_chapter_index(std::string_view script)
{
    constexpr std::string_view prefix = "aid_script_ghoulies_chapter";
    constexpr std::string_view suffixes[] = {"void", "1", "2a", "2b", "2c", "3a", "3b", "3c", "4", "5"};
    if (script.compare(0, 0x1b, prefix) == 0)
    {
        for (int i = 1; i < 10; ++i)
        {
            if (script.substr(0x1b).starts_with(suffixes[i]))
            {
                return i;
            }
        }
    }
    return 0;
}


int slot_chapter(const Game& g, int index)
{
    const char* script = save_script_for_slot(g, index);
    if (script == nullptr)
    {
        throw Error("game select: occupied slot has no translated save script");
    }
    constexpr int chapters[] = {-1, 1, 2, 2, 2, 3, 3, 3, 4, 5};
    return chapters[script_chapter_index(script)];
}


// Xbox 0x000DE9E0: the selected slot owns the movie playback reference.
const char* slot_movie(const Game& g, int index)
{
    // 0x000E01B0 fills DAT_00506598 with the empty-slot movies and DAT_005065A4 with chapters.
    const int status = g.slot_status[index];
    if (status == 2 || status == 4 || status == 3)
    {
        static const char* chapters[] = {
            "aid_movie_ghoulies_gameselect_chapter1",
            "aid_movie_ghoulies_gameselect_chapter2",
            "aid_movie_ghoulies_gameselect_chapter3",
            "aid_movie_ghoulies_gameselect_chapter4",
            "aid_movie_ghoulies_gameselect_chapter5",
        };
        const int chapter = slot_chapter(g, index);
        if (chapter < 1 || chapter > 5)
        {
            throw Error("game select: save script has no chapter movie");
        }
        return chapters[chapter - 1];
    }
    static const char* kEmpty[3] = {
        "aid_movie_ghoulies_gameselect_emptyslots_one",
        "aid_movie_ghoulies_gameselect_emptyslots_two",
        "aid_movie_ghoulies_gameselect_emptyslots_three",
    };
    return kEmpty[index];
}


// Xbox 0x00101730: interpolate each packed color byte, clamp, then truncate.
u32 menu_color(u32 selected, u32 rest, float fraction)
{
    const float inverse = 1.f - fraction;
    const auto channel = [inverse, fraction](u32 first, u32 second)
    {
        float value = static_cast<float>(second) * inverse + static_cast<float>(first) * fraction;
        if (value < 0.f)
        {
            value = 0.f;
        }
        else if (255.f < value)
        {
            value = 255.f;
        }
        return static_cast<u8>(truncate_runtime_float(value));
    };
    const u32 red = channel(selected & 0xFF, rest & 0xFF);
    const u32 green = channel((selected >> 8) & 0xFF, (rest >> 8) & 0xFF);
    const u32 blue = channel((selected >> 16) & 0xFF, (rest >> 16) & 0xFF);
    const u32 alpha = channel(selected >> 24, rest >> 24);
    return red | (green << 8) | (blue << 16) | (alpha << 24);
}


std::string slot_backdrop(const Game& g)
{
    const int status = g.slot_status[g.slot];
    return status == 2 || status == 4 || status == 3
               ? "gameselect_chapter" + std::to_string(slot_chapter(g, g.slot))
               : "gameselect_emptyslotbackground";
}


constexpr const char* kContentsTextures[3] = {
    "gameselect_house", "gameselect_zombieclapframe", "gameselect_skellyscratchheadframe",
};


// Xbox 0x000DF140 / 0x000DA1E0: retain the previous background and start
// the countdown from button-bat +0x93C. The menu constructors override it.
void begin_menu_zoom(Game::MenuVisual& visual, const std::string& backdrop)
{
    visual.old_backdrop = visual.backdrop;
    visual.backdrop = backdrop;
    visual.remaining = visual.duration;
    visual.fraction = 0.f;
    visual.alpha = 0;
    visual.old_alpha = 255;
    ++visual.generation;
}


// Xbox 0x000DF330 / 0x000DA300, followed by the background-alpha part of
// 0x000DE540 / 0x000D9D10. Plate values are read from fraction when drawn.
void tick_menu_zoom(Game::MenuVisual& visual, float dt)
{
    if (0.f < visual.remaining)
    {
        visual.remaining -= dt;
        if (visual.remaining <= 0.f)
        {
            visual.remaining = 0.f;
        }
    }
    float fraction = 1.f;
    if (visual.duration != 0.f)
    {
        fraction = (visual.remaining - visual.duration) / (0.f - visual.duration);
        if (1.f < fraction)
        {
            fraction = 1.f;
        }
        else if (fraction < 0.f)
        {
            fraction = 0.f;
        }
    }
    visual.fraction = fraction;
    if (visual.old_backdrop != visual.backdrop)
    {
        visual.alpha = u8(fraction * 255.f);
        visual.old_alpha = 255 - visual.alpha;
    }
}


// Xbox 0x000DDF20 / 0x000D9640: current backdrop precedes the old backdrop.
void draw_menu_backdrop(const Game& g, const Game::MenuVisual& visual)
{
    if (const Game::UiImage* image = find_ui(g, visual.backdrop))
    {
        gfx_sprite_submit(image->image, 0.f, 0.f, kFrameW, kFrameH, 0.f, 0.f, 1.f, 1.f,
                          {1.f, 1.f, 1.f, float(visual.alpha) / 255.f});
    }
    if (const Game::UiImage* image = find_ui(g, visual.old_backdrop))
    {
        gfx_sprite_submit(image->image, 0.f, 0.f, kFrameW, kFrameH, 0.f, 0.f, 1.f, 1.f,
                          {1.f, 1.f, 1.f, float(visual.old_alpha) / 255.f});
    }
}


// Xbox 0x000DDF20: plate pass, before the selected movie overlay.
void draw_plate(const Game& g, int index, bool selected)
{
    const SlotPlate& slot = kSlots[index];
    // 0x000DEB20. Empty uses the slot's oneframe/twoframe/threeframe.
    // A named slot (status 4) uses the chapter table at 0x0042939C. A new game is chapter 1.
    std::string texture = slot.texture;
    switch (g.slot_status[index])
    {
    case 2:
    case 4:
        texture = "gameselect_chapter" + std::to_string(slot_chapter(g, index));
        break;
    case 3:
        texture = "gameselect_chapter1";
        break;
    }
    const float fraction = selected ? g.slot_visual.fraction : 0.f;
    const float nx = (slot.hot_x - slot.rest_x) * fraction + slot.rest_x;
    const float ny = (slot.hot_y - slot.rest_y) * fraction + slot.rest_y;
    // 0x0005CA60. On-screen size is texture pixels * scale, and scale is
    // (norm * screen) / texture pixels, so the quad is norm * {640,480}
    // centered on the table center.
    const float w = nx * kFrameW;
    const float h = ny * kFrameH;
    const Game::UiImage* image = find_ui(g, texture);
    if (image != nullptr)
    {
        const float padding = (20.f - 12.f) * fraction + 12.f;
        draw_plate_background(slot.center_x, slot.center_y, w + padding, h + padding,
                              menu_color(0xC43EF7B6u, 0x80069764u, fraction));
        draw_sprite(image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f, w, h);
    }
}


// Xbox 0x000DDF20: +0xB84 is tested against DAT_00429000 (10), then
// faded over DAT_00429004 frames. Static plates remain underneath the movie.
void draw_slot_movie(const Game& g)
{
    if (g.slot_movie_index != g.slot)
    {
        if (g.slot_movie_index != -1)
        {
            movie_release_visible(g.slot_movie_name.c_str());
        }
        g.slot_movie_index = g.slot;
        g.slot_movie_name = slot_movie(g, g.slot);
        g.slot_movie_frames = 0;
        movie_note_visible(slot_movie(g, g.slot));
    }
    const char* movie = slot_movie(g, g.slot);
    if (10 <= g.slot_movie_frames)
    {
        movie_copy_frame(movie, g.slot_movie_image);
    }
    if (movie_is_open(movie))
    {
        if (10 < g.slot_movie_frames)
        {
            const SlotPlate& slot = kSlots[g.slot];
            const float w = ((slot.hot_x - slot.rest_x) * g.slot_visual.fraction + slot.rest_x) * kFrameW;
            const float h = ((slot.hot_y - slot.rest_y) * g.slot_visual.fraction + slot.rest_y) * kFrameH;
            const float alpha = float(u8(std::clamp(float(g.slot_movie_frames - 10) / 20.f * 255.f,
                                                   0.f, 255.f))) / 255.f;
            gfx_sprite_submit(g.slot_movie_image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f, w, h,
                              0.f, 0.f, 1.f, 1.f, {1.f, 1.f, 1.f, alpha});
        }
        ++g.slot_movie_frames;
    }
}


// Xbox 0x000DEB20 initializes the overlays. 0x000DE540 scales them with
// the selected plate, and 0x000DDF20 draws them after the movie overlay.
void draw_slot_status(const Game& g, int index)
{
    const char* texture = nullptr;
    if (g.slot_status[index] == 4)
    {
        texture = "gameselect_disc_nowrite";
    }
    if (g.slot_status[index] == 3)
    {
        texture = "gameselect_disc_broken";
    }
    if (texture != nullptr)
    {
        if (const Game::UiImage* image = find_ui(g, texture))
        {
            const SlotPlate& slot = kSlots[index];
            const float fraction = index == g.slot ? g.slot_visual.fraction : 0.f;
            const float scale_x = 0.7f * ((slot.hot_x / slot.rest_x - 1.f) * fraction + 1.f);
            const float scale_y = 0.7f * ((slot.hot_y / slot.rest_y - 1.f) * fraction + 1.f);
            const float w = float(image->image.width) * scale_x;
            const float h = float(image->image.height) * scale_y;
            gfx_sprite_submit(image->image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f,
                              w, h, 0.f, 0.f, 1.f, 1.f, {1.f, 1.f, 1.f, 127.f / 255.f});
        }
    }
}


// Xbox 0x001421C0 / 0x00142720 / 0x00142AF0: mode 1 alternates
// columns, with icon centres and a separate offset for the text widgets.
void draw_menu_prompts(const Game& g, const GloopyFont& font,
                       const GooLine* prompts, const int* codes, int count,
                       float anchor_x, float anchor_y, float icon_scale,
                       float text_scale, float text_offset_x, float text_offset_y,
                       float column)
{
    for (int i = 0; i < count; ++i)
    {
        const float x = anchor_x + float(i % 2) * column;
        const float y = anchor_y - float(i / 2) * 32.f * icon_scale;
        if (const RgbaImage* icon = joypad_icon(g, codes[i]))
        {
            const float width = float(icon->width) * icon_scale;
            const float height = float(icon->height) * icon_scale;
            gfx_sprite_submit(*icon, x - width * 0.5f, y - height * 0.5f, width, height);
        }
        draw_goo_line(font, prompts[i], x + text_offset_x, y + text_offset_y,
                      text_scale, g, false);
    }
}


// 0x000DF9A0 while the widget mode is 6 places the selected slot's wide name
// through 0x0005F5C0. The anchor is the per-slot point init wrote at +0x418:
// screen center plus DAT_00428FA8 (0) and DAT_00428FAC (-185). Scale is
// DAT_00428FA4 (1.8). param_8, so the line is centered on that point.
// The title is the same call with DAT_00428FB0 (1) at center + (0, 50).
// The prompt bar's y is DAT_0050974C - 16. On 640x480, 0x000FC190 stores
// that edge at 431.5, and the three prompt strings use DAT_00429024 (0.75).
void draw_slot_select_copy(const Game& g)
{
    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    else
    {
        name = savedgame_string(g, "emptyslotname");
    }

    if (g.slot_status[g.slot] == 3)
    {
        name = savedgame_string(g, "damagedgamename");
    }

    // 0x000DF9A0 builds these widgets again on the way into mode 6, including
    // each time the slot changes. The title and the prompt bar stay put.
    auto& text_pool = g.slot_text;
    const bool entered = text_pool.generation == ~u32(0);
    GooLine& slot_name = text_pool.name;
    GooLine& title = text_pool.title;
    auto& prompts = text_pool.prompts;
    u32& name_generation = text_pool.generation;
    auto& departing_names = text_pool.departing_names;
    if (name_generation != g.slot_visual.generation)
    {
        if (!slot_name.text.empty())
        {
            end_goo(slot_name);
            departing_names.push_back(slot_name);
        }
        slot_name = GooLine{};
        name_generation = g.slot_visual.generation;
    }
    if (g.slot_visual.fraction >= 1.f && (entered || slot_name.slot != g.slot || slot_name.text != name))
    {
        arm_goo(slot_name, name, g.time, g.slot);
    }
    const std::u16string title_text = gameselect_string(g, "gameslotselection_instructions");
    if (entered || title.text != title_text)
    {
        arm_goo(title, title_text, g.time, -1);
    }
    const char* prompt_keys[3] = {
        "gameslotselection_selectprompt",
        "gameslotselection_moveprompt",
        "gameslotselection_backtoattract",
    };
    for (int i = 0; i < 3; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (entered || prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
    }

    for (const GooLine& line : departing_names)
    {
        draw_goo_line(font, line, 320.f, 55.f, 1.8f, g, true);
    }
    std::erase_if(departing_names, [](const GooLine& line)
    {
        return line.removed;
    });
    draw_goo_line(font, slot_name, 320.f, 55.f, 1.8f, g, true);
    draw_goo_line(font, title, 320.f, 290.f, 1.f, g, true);

    float column = 0.f;
    for (const char* key : prompt_keys)
    {
        column = std::max(column, text_width(font, gameselect_string(g, key), 0.75f));
    }
    column = std::max(column, text_width(font,
        gameselect_string(g, "gameslotselection_deleteprompt"), 0.75f));
    column += 52.f;
    const int codes[] = {0, 7, 1};
    draw_menu_prompts(g, font, prompts, codes, 3,
                      76.5f, 415.5f, 1.f, 0.75f, 20.f, -16.f, column);
}

} // namespace frontend_detail


void gameselect_tick(Game& g, const InputState& in)
{
    const int previous_mode = g.frontend_mode;
    const int previous_row = g.contents_row;
    const int previous_slot = g.slot;
    bool cursor_created = false;
    if (tick_menu_book(g) && g.menu_book_target != 0)
    {
        g.frontend_mode = g.menu_book_target;
        g.menu_book_target = 0;
        g.naming = false;
        if (g.frontend_mode == 3)
        {
            g.contents_row = 0;
            g.contents_cursor.reset();
            movie_open_contents();
            for (int i = 0; i < 3; ++i)
            {
                g.contents_movie_frames[i] = 0;
                g.contents_movie_copied[i] = false;
            }
        }
    }
    if (g.frontend_mode == 1 || g.frontend_mode == 3)
    {
        auto& cursor = g.frontend_mode == 1 ? g.slot_cursor : g.contents_cursor;
        if (cursor == nullptr)
        {
            cursor = std::make_shared<MenuCursor>(g);
            cursor->move(g.frontend_mode == 1 ? g.slot : g.contents_row);
            cursor_created = true;
        }
        cursor->tick(scene_sim_step(g.clock.dt));
    }
    if (g.frontend_mode == 1)
    {
        tick_menu_zoom(g.slot_visual, scene_sim_step(g.clock.dt));
    }
    if (g.frontend_mode == 3)
    {
        tick_menu_zoom(g.contents_visual, scene_sim_step(g.clock.dt));
    }
    // Dialog A. 0x000584D0's widget takes the accept edge and steps {page}.
    if (in.accept && !g.slot_accept)
    {
        caption_accept(g);
    }
    if (g.frontend_mode == 0)
    {
        g.naming = false;
        // 0x00080C10 calls 0x00080CB0 while the banner dword at +0x294 is 0.
        // 0x001431B0 accepts a new A or Start, not both. 0x0011EB30 then requests
        // the gameselect script copied from 0x00322B8C.
        if (!g.attract_banner_latched && attract_confirm_pressed())
        {
            // 0x00322B8C. 0x0011EB30 queues it. The next 0x0011E4C0 that enters state 4 loads it.
            script_request(g, "aid_script_ghoulies_gameselect_gameselect");
            // 0x00080CB0 then calls 0x0013BAF0: the cue at 0x00314D68 is
            // GBARON_STARTBUTTONLAUGH (selection scale DAT_0032B100 is zero).
            // Keep this one-shot alive across the outgoing cutscene cleanup.
            pcm_play_owned_cue(g, "XACT_SOUNDBANK_GBARON_STARTBUTTONLAUGH");
            g.attract_banner_latched = true;
        }
    }
    else if (g.frontend_mode == 4)
    {
        // 0x00014C10 then 0x00015090. A and Start (codes 1 and 2) activate the
        // key. B (codes 3 and 4) calls 0x000DD550, which leaves for slot select.
        // X (code 5) cycles the page. Triggers (codes 9 and 10) move the caret.
        // D-pad codes 11-14 move the highlight.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
            g.naming = false;
        }
        else
        {
            if (in.left && !g.slot_left)
            {
                kb_move(g, -1, 0);
            }
            if (in.right && !g.slot_right)
            {
                kb_move(g, 1, 0);
            }
            if (in.up && !g.slot_up)
            {
                kb_move(g, 0, -1);
            }
            if (in.down && !g.slot_down)
            {
                kb_move(g, 0, 1);
            }
            if (in.caret_left && !g.slot_caret_left && g.kb_caret > 0)
            {
                --g.kb_caret;
            }
            if (in.caret_right && !g.slot_caret_right && g.kb_caret < g.kb_len)
            {
                ++g.kb_caret;
            }
            if (in.page && !g.slot_page)
            {
                if (g.kb_page == 0)
                {
                    g.kb_page = 1;
                }
                else if (g.kb_page == 1)
                {
                    g.kb_page = 2;
                }
                else
                {
                    g.kb_page = 0;
                }
                kb_clamp_col(g);
            }
            if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start))
            {
                kb_activate_with_audio(g);
            }
        }
    }
    else if (g.frontend_mode == 1 && !menu_book_busy())
    {
        // 0x000E0210: direction 8 (right) increments, direction 4 (left) decrements. Both end in state 6.
        if (in.right && !g.slot_right && (g.slot_cursor->bat.state == 1 || g.slot_cursor->bat.state == 3))
        {
            g.slot = (g.slot + 1) % 3;
        }
        if (in.left && !g.slot_left && (g.slot_cursor->bat.state == 1 || g.slot_cursor->bat.state == 3))
        {
            g.slot = (g.slot + 2) % 3;
        }
        // Xbox 0x00142460 / 0x00141E20: prompt face-button acceptance sound.
        if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start) ||
            (in.back && !g.slot_back))
        {
            pcm_play_owned_cue(g, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON");
        }
        // 0x000E02D0: empty slot sets mode 4. Status 2, 3, or 4 sets mode 3 (slot contents).
        if (in.accept && !g.slot_accept)
        {
            if (g.slot_status[g.slot] == 0)
            {
                kb_open(g);
            }
            else
            {
                // Xbox 0x000E02D0: queue widget 3 and book clip 0.
                // 0x00083E20 installs it only after the first page threshold.
                g.menu_book_target = 3;
                g.slot_cursor->depart(g.slot);
                begin_menu_book(g, 0);
            }
        }
        // 0x000E05D0 copies aid_script_ghoulies_gameselect_backtoattract from 0x00324F30
        // into 0x0050E458, then 0x0011EAD0 requests that name.
        if (in.back && !g.slot_back)
        {
            g.naming = false;
            script_request(g, "aid_script_ghoulies_gameselect_backtoattract");
        }
    }
    else if (g.frontend_mode == 3 && !menu_book_busy())
    {
        // 0x000D9450 ticks the prompt widget. Move is 0x000DB450: direction 8
        // increments the row, direction 4 decrements it, then 0x000DA580 mode 6
        // swaps the prompt set through 0x000142180. 0x000D9D10, once the plate
        // zoom reaches 1, pushes mode 2 (Play), 5 (Features), or 4 (Challenges).
        // Those bodies place the row label and, for mode 2, the chapter buffer.
        // None of them request a script.
        if (in.right && !g.slot_right && (g.contents_cursor->bat.state == 1 || g.contents_cursor->bat.state == 3))
        {
            g.contents_row = (g.contents_row + 1) % 3;
        }
        if (in.left && !g.slot_left && (g.contents_cursor->bat.state == 1 || g.contents_cursor->bat.state == 3))
        {
            g.contents_row = (g.contents_row + 2) % 3;
        }
        // Xbox 0x00142460 / 0x00141E20: prompt face-button acceptance sound.
        if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start) ||
            (in.back && !g.slot_back))
        {
            pcm_play_owned_cue(g, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON");
        }
        // Play is 0x000DB570: it copies the slot name, calls 0x00083B10 with 1,
        // then 0x000DA580 with mode 7. Stream 2 is idle, so 0x00083B10 arms the
        // play state and the scene-control tick runs 0x00083C70, which
        // 0x0011EB30-requests the script 0x0010DB10 copied from the save.
        // Features 0x000DB6D0 and Challenges 0x000DB670 are separate callbacks.
        // Back is 0x000DB510, which returns to slot select.
        // Select record code 0 uses button word 0x110: A (0x0100) or Start (0x0010).
        // 0x001083F0 accepts either edge.
        if (((in.accept && !g.slot_accept) || (in.start && !g.slot_start)) && g.contents_row == 0)
        {
            const char* script = save_script(g);
            if (script != nullptr)
            {
                g.frontend_mode = 7;
                std::cout << "play " << script << "\n";
                script_request(g, script);
            }
        }
        if (in.back && !g.slot_back)
        {
            // Xbox 0x000DB510: queue widget 1 and book clip 1 (3 to 2).
            g.menu_book_target = 1;
            begin_menu_book(g, 1);
        }
    }
    if (g.frontend_mode == 1 && (previous_mode != 1 || previous_slot != g.slot))
    {
        if (previous_mode != 1)
        {
            g.slot_text = Game::MenuTextPool{};
        }
        begin_menu_zoom(g.slot_visual, slot_backdrop(g));
        if (g.slot_cursor != nullptr && !cursor_created)
        {
            g.slot_cursor->move(g.slot);
        }
    }
    if (g.frontend_mode == 3 && (previous_mode != 3 || previous_row != g.contents_row))
    {
        if (previous_mode != 3)
        {
            g.contents_text = Game::MenuTextPool{};
        }
        begin_menu_zoom(g.contents_visual, kContentsTextures[g.contents_row]);
        if (g.contents_cursor == nullptr)
        {
            g.contents_cursor = std::make_shared<MenuCursor>(g);
        }
        if (!cursor_created)
        {
            g.contents_cursor->move(g.contents_row);
        }
    }
    // Xbox 0x000DB450 / 0x000DACE0: release and reset the departing row
    // before 0x000D9F60 starts the newly selected movie.
    if (previous_mode == 3 && (g.frontend_mode != 3 || previous_row != g.contents_row))
    {
        movie_release_visible(kContentsMovies[previous_row]);
        g.contents_movie_frames[previous_row] = 0;
        g.contents_movie_copied[previous_row] = false;
    }
    // Xbox 0x000DF9A0: leaving the slot widget releases its +0xB80 movie.
    if (previous_mode == 1 && g.frontend_mode != 1 && g.slot_movie_index != -1)
    {
        movie_release_visible(g.slot_movie_name.c_str());
        g.slot_movie_index = -1;
    }
    g.slot_down = in.down;
    g.slot_up = in.up;
    g.slot_left = in.left;
    g.slot_right = in.right;
    g.slot_accept = in.accept;
    g.slot_back = in.back;
    g.slot_start = in.start;
    g.slot_page = in.page;
    g.slot_caret_left = in.caret_left;
    g.slot_caret_right = in.caret_right;
    tick_naming_background(g);
}

} // namespace gbtg
