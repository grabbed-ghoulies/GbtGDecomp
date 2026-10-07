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
#include "frontend_attract.h"
#include "frontend_captions.h"
#include "frontend_contents.h"
#include "frontend_keyboard.h"
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

float scene_sim_step(float dt);

namespace frontend_detail
{




std::u16string gameselect_string(const Game& g, std::string_view key);

std::u16string default_new_game_name(const Game& g);

float text_width(const GloopyFont& font, std::u16string_view text, float scale);

int keyboard_glyph_frame(const Game& g, int page, int row, int col, const std::u16string& label, size_t index);

int keyboard_name_frame(const Game& g, const std::u16string& name, size_t index);

void draw_plate_background(float center_x, float center_y, float width, float height, u32 color);


RgbaImage image_from_entry(const BundleEntry& entry)
{
    TextureHeader hdr;
    if (entry.cpu.size() < 28 || !parse_texture_header(entry.cpu, hdr))
    {
        throw Error("texture header");
    }
    std::span<const u8> gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        if (size_t(chunk.offset) + chunk.size <= entry.gpu.size())
        {
            gpu = entry.gpu.subspan(chunk.offset, chunk.size);
        }
    }
    return decode_xbox_texture(hdr, gpu);
}


const Game::UiImage* find_ui(const Game& g, std::string_view key)
{
    for (const auto& image : g.ui)
    {
        if (image.name.find(key) != std::string::npos)
        {
            return &image;
        }
    }
    return nullptr;
}


bool scene_control_is(const Game& g, std::string_view suffix)
{
    // The scene script names one objparams. The type table at 0x0030DB50
    // registers that object: scenecontrol_gameselect is entitySceneControlGameSelect.
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()) == suffix)
        {
            return true;
        }
    }
    return false;
}


bool scene_control_starts(const Game& g, std::string_view suffix)
{
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()).starts_with(suffix))
        {
            return true;
        }
    }
    return false;
}


// Xbox 0x0005CA60
// Submits the quad 0x0005E380 pushes. x,y is the top-left in the 640x480 HUD.
void draw_sprite(const Game::UiImage* image, float x, float y, float w, float h)
{
    if (image != nullptr)
    {
        gfx_sprite_submit(image->image, x, y, w, h);
    }
}

} // namespace frontend_detail


void load_ui(Game& g)
{
    g.ui.clear();
    // 0x000DD740 resolves these aids with 0x00035E90. They are files in the
    // scene bundle the gameselect scene control just opened, not a name test.
    if (!g.scene_bundle || !scene_control_is(g, "gameselect"))
    {
        return;
    }
    for (const auto* entry : entries_of_type(*g.scene_bundle, ResourceType::Texture))
    {
        try
        {
            Game::UiImage image;
            image.name = entry->name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  ui fail " << entry->name << ": " << ex.what() << "\n";
        }
    }
    for (const char* name : kJoypadIconNames)
    {
        if (const BundleEntry* entry = g.aids.load_aid(name))
        {
            Game::UiImage image;
            image.name = name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
    }
    std::cout << "  ui textures " << g.ui.size() << "\n";
}


// 0x000E5B40, from 0x000DF9A0 case 1 when the slot widget opens. EAX is this
// cue and the id on the stack is 1. 0x00038910 acquires it; 0x000382C0 then
// binds a channel and 0x00038DA0 sets that voice playing. The cue-list row is
// Gstreamsditties / pause: device 3 (Gdvd8), wave 0.
constexpr const char* kSlotOpen = "XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE";


void begin_frontend(Game& g)
{
    movie_reset();
    // entitySceneControlAttract (registrar 0x00080970). The banner is that object,
    // built because the script names aid_objparams_ghoulies_scenecontrol_attract*.
    if (scene_control_starts(g, "attract"))
    {
        pcm_stop_cue(kSlotOpen);
        g.frontend_mode = 0;
        g.naming = false;
        // A new attract script builds a new banner, so +0x294 starts clear.
        g.attract_banner_latched = false;
        std::cout << "title attract\n";
        return;
    }
    // entitySceneControlGameSelect init 0x00083660. It calls 0x000E5AE0, which
    // installs modes[DAT_00429BC0] when stream id 2 is idle (0x00039D50 returns 0).
    // Boot 0x000E5A80 stored 1, and 0x000DD740 stores 1 again: slot select.
    // A scene with no frontend scene-control leaves the widget down.
    if (!scene_control_is(g, "gameselect"))
    {
        pcm_stop_cue(kSlotOpen);
        g.frontend_mode = -1;
        g.naming = false;
        return;
    }
    g.frontend_pending = 1;
    g.frontend_mode = g.frontend_pending;
    g.naming = false;
    g.slot_movie_index = -1;
    g.slot_movie_frames = 0;
    g.slot_visual = Game::MenuVisual{};
    g.contents_visual = Game::MenuVisual{};
    g.slot_text = Game::MenuTextPool{};
    g.contents_text = Game::MenuTextPool{};
    g.slot_cursor.reset();
    g.contents_cursor.reset();
    g.menu_book_target = 0;
    begin_menu_zoom(g.slot_visual, slot_backdrop(g));
    // 0x000DD774 calls 0x000E01B0 while the slot widget is installed.
    movie_open_gameselect();
    pcm_play_cue(g, kSlotOpen, true);
    std::cout << "frontend mode 1 slot select\n";
}


void draw_ui(const Game& g)
{
    gfx_sprite_clear();
    if (g.frontend_mode < 0)
    {
        draw_move_captions(g);
        return;
    }
    if (g.frontend_mode == 0)
    {
        draw_start_banner(g);
        draw_move_captions(g);
        return;
    }
    if (g.frontend_mode == 3 || g.frontend_mode == 7)
    {
        // Mode 7 is the widget 0x000DA580 stores from Play. Its draw is 0x000D5300.
        // Until that body is translated, the contents plates stay up for the
        // frame before 0x0011EB30's load replaces the scene.
        draw_contents(g);
        if (g.contents_cursor != nullptr)
        {
            g.contents_cursor->draw();
        }
        return;
    }
    // Naming is mode-table index 4. Its draw is 0x000DD170, which draws the
    // keyboard object and not the three slot plates.
    if (g.frontend_mode == 4)
    {
        draw_naming_background(g);
        draw_keyboard(g);
        return;
    }
    // Xbox 0x000DF140: occupied statuses use the chapter texture as backdrop.
    draw_menu_backdrop(g, g.slot_visual);
    for (int i = 0; i < 3; ++i)
    {
        if (i != g.slot)
        {
            draw_plate(g, i, false);
        }
    }
    draw_plate(g, g.slot, true);
    draw_slot_movie(g);
    for (int i = 0; i < 3; ++i)
    {
        draw_slot_status(g, i);
    }
    if (g.frontend_mode == 1)
    {
        draw_slot_select_copy(g);
    }
    draw_keyboard(g);
    if (g.slot_cursor != nullptr)
    {
        g.slot_cursor->draw();
    }
}


// Xbox 0x00046760: resolve mapname text and place it beside its map spot.
// 0x00060710 measures at scale 0.6; 0x0005F5C0 submits the string.
void draw_map_label(const Game& g, std::string_view key, float x, float y, float spot_height)
{
    static std::map<std::string, std::u16string> labels;
    if (labels.empty())
    {
        const auto path = g.aids.game_dir / "loctext" / "english" /
                          aid_hash_hex(aid_hash("aid_loctext_ghoulies_mapnames"));
        const LocTable table = parse_loctext(read_file(path), "mapnames");
        for (const auto& [name, text] : table.keys_to_utf8)
        {
            labels.emplace(name, utf8_to_u16(text));
        }
    }
    const auto found = labels.find("mapname__" + std::string(key));
    const std::u16string text = found == labels.end() ? u"ERROR" : found->second;
    const GloopyFont& font = gloopy_font(g);
    const float width = text_width(font, text, 0.6f);
    const float pen_x = x + width <= 640.f * 0.85f ? x + spot_height : x - width - spot_height;
    const float pen_y = y - float(font.drop()) * 0.6f * 0.5f;
    draw_glyphs(font, text, pen_x, pen_y, 0.6f, font.rest_frame());
}

} // namespace gbtg
