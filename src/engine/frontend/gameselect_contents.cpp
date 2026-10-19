#include "gameselect_contents.h"
#include "button_bat_menu.h"
#include "gameselect_progress.h"
#include "dialogue_draw_context.h"
#include "dialogue_draw_outer.h"
#include "dialogue_activation.h"
#include "dialogue_input.h"
#include "dialogue_measurement.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../cutscenes/event_runtime.h"
#include "dialogue_text_metrics.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "frontend_sounds.h"
#include "frontend_state.h"
#include "input_activation.h"
#include "hud_layout.h"
#include "../audio/event_voices.h"
#include "../entities/actor_render_registration.h"
#include "../saves/save_games.h"
#include "../scene/attract_control.h"
#include "../scene/executable_resources.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"
#include "../video/movie_startup.h"
#include "gbtg/entity.h"

#include <bit>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cwchar>

namespace gbtg
{
namespace
{
constexpr u32 kChapterTextures = 0x00428C84; // Stride 0x104: the texture, +0x80 the movie.
constexpr u32 kChapterLayout = 0x0030699C; // Stride 0x1C.

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

GameContentsWidget& contents(GameSelectWidget& widget)
{
    return static_cast<GameContentsWidget&>(widget);
}

// Chapter panel `index` (+4 + index * 0x2C4).
GameContentsWidget::Chapter& chapter_panel(GameContentsWidget& w, size_t index)
{
    return w.chapters[index];
}

GameSelectSprite& chapter_sprite(GameContentsWidget& w, size_t index)
{
    return chapter_panel(w, index).sprite;
}

// A countdown ticked toward 0 by the frame step and clamped there (0x000DA300 / 0x000DA3B0).
void count_down(float& timer, float step)
{
    if (0.f < timer)
    {
        const float left = timer - step;
        timer = left;
        if (left <= 0.f)
        {
            timer = 0.f;
        }
    }
}

u32 exe_word(const GameSelectControlServices& s, u32 address)
{
    return read_u32(s.image.memory(address, 4), 0);
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(exe_word(s, address));
}

float screen_width(const GameSelectControlServices& s)
{
    return float(i32(s.screen[0])); // DAT_005096E8
}

float screen_height(const GameSelectControlServices& s)
{
    return float(i32(s.screen[1])); // DAT_005096EA
}

float texture_width(std::span<const u8> texture)
{
    return float(i32(read_u16(texture, 8)));
}

float texture_height(std::span<const u8> texture)
{
    return float(i32(read_u16(texture, 10)));
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

u8 truncate_byte(float value)
{
    return u8(i32(value)); // 0x001673D8
}

u32 with_alpha(u32 color, u8 alpha)
{
    return (color & 0x00FFFFFFu) | (u32(alpha) << 24);
}

// The selected panel (+0x85C).
size_t chapter(const GameContentsWidget& w)
{
    return size_t(w.chapter_index);
}

// 0x00035E90 with its 'aid_' test and 0x00032700(2) when missing.
std::span<const u8> acquire_texture(const char* name, GameSelectControlServices& s)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return {};
    }
    const auto texture = acquire_animation_resource(s.resources, name);
    if (texture.data() == nullptr)
    {
        resource_error(s.error, 2);
    }
    return texture;
}

void release_texture(std::span<const u8>& texture, GameSelectControlServices& s)
{
    release_animation_resource(s.resources, texture); // 0x00035C00
    texture = {};
}

// 0x0005CDC0, 0x0005CA60 (EAX the position, ECX the frame, EDX the top texture coordinates), then the override
// hold DAT_00509B5C is released.
void draw_sprite(const float* position, i32 frame, const float* uv_top, std::span<const u8> texture,
    const float* scale, const float* uv_bottom, u32 color, GameSelectControlServices& s)
{
    auto& draw = s.draw;
    begin_dialogue_sprite_draw(draw); // 0x0005CDC0
    draw_dialogue_texture(frame, uv_top, const_cast<u8*>(texture.data()), scale, uv_bottom, 0.f, color, position,
        draw); // 0x0005CA60
    --draw.override_count;
    if (draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

u32 text_record(RuntimeDialogueTextPool& pool, const char16_t* text, float x, float y, float scale,
    GameSelectControlServices& s)
{
    return create_dialogue_text_record(pool, text, x, y, scale, scale, 1.f, 1, 0, 1, s.metrics, s.viewport_width,
        s.next_text_id, s.random); // 0x0005F5C0, EAX 1
}

// A text line's record: its text at its position.
u32 line_record(RuntimeDialogueTextPool& pool, GameSelectTextLine& line, float scale, GameSelectControlServices& s)
{
    return text_record(pool, line.text.data(), line.x, line.y,
        scale, s);
}

// Xbox 0x00101730: each byte of `rest` toward `selected` by the fraction, clamped to 0..255 and truncated.
u32 blend_color(u32 selected, u32 rest, float fraction, GameSelectControlServices& s)
{
    const float full = exe_real(s, 0x0032B158);
    const float remainder = exe_real(s, 0x0032B0F8) - fraction;
    u32 output = 0;
    for (u32 shift = 0; shift < 32; shift += 8)
    {
        float value = float(i32((rest >> shift) & 0xFF)) * remainder + float(i32((selected >> shift) & 0xFF)) * fraction;
        if (0.f > value)
        {
            value = 0.f;
        }
        else if (value > full)
        {
            value = full;
        }
        output |= u32(truncate_byte(value)) << shift;
    }
    return output;
}

const RuntimeDialogueInputDevice& frontend_device(GameSelectControlServices& s)
{
    return s.input_view(s.frontend_input); // DAT_00429BBC
}

// Xbox 0x0011ED80: the scene's localized name (aid_loctext_ghoulies_script_scenenames, "scenenames") in
// DAT_00507E78.
const char16_t* scene_name(i32 ordinal, std::array<char16_t, 0x800>& output, GameSelectControlServices& s)
{
    const char* resource = "aid_loctext_ghoulies_script_scenenames"; // 0x0032A5AC
    const std::span<const u8> text = acquire_texture(resource, s);
    const u32 key = exe_word(s, 0x00430C28 + exe_word(s, 0x00311E88 + u32(ordinal) * 4) * 4);
    output[0] = 0;
    lookup_borrowed_localtext(text, "scenenames", s.image.string(key), output.data(), s.localtext); // 0x000FF120
    std::span<const u8> held = text;
    release_animation_resource(s.resources, held); // 0x00035C00
    return output.data();
}

// wsprintfW("%s %d"): the text, a space and the number.
std::u16string text_and_number(const char16_t* text, i32 number)
{
    std::u16string output(text);
    output += u' ';
    const std::wstring digits = std::to_wstring(number);
    output.append(digits.begin(), digits.end());
    return output;
}

// Xbox 0x000DA040: the game's start mode (+0xF24) and its progress line at +0xA78.
bool describe_progress(GameContentsWidget& w, GameSelectControlServices& s)
{
    if (save_game_word(w.name, 0x184) == 0) // 0x0010DC20
    {
        w.start_mode = save_game_word(w.name, 0x188) != 0 ? 3 : 2; // 0x0010DC60
    }
    else
    {
        w.start_mode = 1;
    }
    if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        return false;
    }
    std::array<char16_t, 0x100> chapter_text{};
    std::array<char16_t, 0x100> scene_text{};
    if (lookup_borrowed_localtext(s.text, "gameselect", "gameslotcontents_chapter", chapter_text.data(),
            s.localtext) == 0) // 0x000FF120
    {
        return false;
    }
    if (lookup_borrowed_localtext(s.text, "gameselect", "gameslotcontents_scene", scene_text.data(), s.localtext) == 0)
    {
        return false;
    }
    char script[0x80]{};
    save_game_script(w.name, script); // 0x0010DB10
    const i32 index = gameselect_script_chapter(script, s); // 0x0004B4D0
    const std::u16string chapter_line =
        text_and_number(chapter_text.data(), i32(exe_word(s, 0x002FE698 + u32(index) * 4))); // wsprintfW("%s %d")
    i32 scene = gameselect_book_scene(script, s); // 0x0011EC70
    if (scene == 0)
    {
        scene = 1;
    }
    const std::u16string scene_line = text_and_number(scene_text.data(), scene); // wsprintfW("%s %d")
    std::array<char16_t, 0x800> name{};
    const std::u16string place = scene_name(gameselect_scene_ordinal(script, s), name, s); // 0x0011ED30, 0x0011ED80, lstrcpyW
    const std::u16string line = chapter_line + scene_line + u" : " + place; // wsprintfW("%s%s : %s")
    char16_t* text = w.progress.text.data();
    std::copy(line.begin(), line.end(), text);
    text[line.size()] = 0;
    return true;
}

// Xbox 0x000D9C20: each chapter's texture and movie name, and its localized name.
bool load_chapter_names(GameContentsWidget& w, GameSelectControlServices& s)
{
    for (size_t index = 0; index < 3; ++index)
    {
        auto& assets = w.chapters[index];
        auto& panel = chapter_panel(w, index);
        if (assets.sprite.texture.data() != nullptr)
        {
            release_texture(assets.sprite.texture, s);
        }
        const u32 row = kChapterTextures + u32(index) * 0x104;
        assets.sprite.texture = acquire_texture(s.image.string(row), s);
        std::strcpy(panel.movie_name.data(), s.image.string(row + 0x80));
        panel.name_key = exe_word(s, row - 4);
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return false;
        }
        // 0x000FF120, EDI the key (+0x210), EBX the chapter's name (+4).
        if (lookup_borrowed_localtext(s.text, "gameselect", s.image.string(panel.name_key),
                panel.name.text.data(), s.localtext) == 0)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x000DA1B0, EAX the widget
void release_chapter_textures(GameContentsWidget& w, GameSelectControlServices& s)
{
    for (auto& record : w.chapters)
    {
        if (record.sprite.texture.data() != nullptr)
        {
            release_texture(record.sprite.texture, s);
        }
    }
}

// Xbox 0x000DA530, EAX the widget: every chapter's colors back to rest, the selected one lit.
void reset_chapter_colors(GameContentsWidget& w, GameSelectControlServices& s)
{
    const u32 white = exe_word(s, 0x00314F48);
    const u32 rest = exe_word(s, 0x003069EC);
    for (size_t index = 0; index < 3; ++index)
    {
        chapter_sprite(w, index).color = white;
        chapter_panel(w, index).box_color = rest;
    }
    chapter_panel(w, chapter(w)).box_color = exe_word(s, 0x003069E8);
}

// Xbox 0x000D9F10, ESI the widget: the cursor bat, made once.
bool create_contents_bat(GameContentsWidget& w, GameSelectControlServices& s)
{
    if (w.bat == nullptr)
    {
        w.bat = s.create_bat({0.f, 0.f}, 2.f, 0.4f); // 0x0013B580(EAX {0, 0}, 2.0, 0.4)
        if (w.bat == nullptr)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x000DACE0: the selected chapter's movie stops being shown.
void stop_chapter_movie(GameContentsWidget& w)
{
    auto& panel = chapter_panel(w, chapter(w));
    auto* movie = w.chapters[chapter(w)].movie;
    if (movie != nullptr)
    {
        --movie->play_count;
        panel.movie_held = panel.movie_held - 1;
        panel.movie_frames = 0;
        panel.movie_frame_ready = 0;
    }
}

// Xbox 0x000D9F60, EDI the widget: the selected chapter's movie starts and the bat flies to it.
bool select_chapter_movie(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    if (w.flown_chapter == u32(index))
    {
        return true;
    }
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    w.bat_target[0] = exe_real(s, layout - 8);
    w.bat_target[1] = exe_real(s, layout + 0x10) + exe_real(s, layout - 4);
    auto& panel = chapter_panel(w, index);
    if (panel.movie_held < 1)
    {
        ++w.chapters[index].movie->play_count;
        panel.movie_held = panel.movie_held + 1;
        panel.movie_frames = 0;
        panel.movie_frame_ready = 0;
    }
    if (w.bat != nullptr)
    {
        const ButtonBatPoint target{w.bat_target[0], w.bat_target[1]};
        s.fly_bat(*w.bat, target, 2.f); // 0x0006DA00, EBX +0x854
        w.flown_chapter = u32(index);
        return true;
    }
    return false;
}

// Xbox 0x000DA1E0, ESI the widget: the selected chapter's background, the old one kept for the cross-fade.
void select_chapter_background(GameContentsWidget& w, GameSelectControlServices& s)
{
    if (w.old_background.texture.data() != nullptr)
    {
        release_texture(w.old_background.texture, s);
    }
    w.old_background.texture = w.background.texture;
    w.background.texture = {};
    auto& background = w.background;
    auto& old_background = w.old_background;
    old_background.scale[0] = background.scale[0];
    old_background.scale[1] = background.scale[1];
    old_background.position[0] = background.position[0];
    old_background.position[1] = background.position[1];
    w.background.texture = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    background.scale[1] = screen_height(s) / texture_height(w.background.texture);
    background.scale[0] = screen_width(s) / texture_width(w.background.texture);
    w.background_fade_timer = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
    w.background_fade = 0.f;
    set_color_alpha(background.color, 0);
    set_color_alpha(old_background.color, 0xFF);
}

// Xbox 0x000DA300, ECX the widget: the cross-fade's fraction from the bat's flight.
void tick_chapter_fade(GameContentsWidget& w, GameSelectControlServices& s)
{
    count_down(w.background_fade_timer, frame_step(s));
    const float flight = s.bat_screen(*w.bat).flight_duration; // +0x93C
    float fraction = exe_real(s, 0x0032B0F8);
    if (flight != 0.f)
    {
        fraction = (w.background_fade_timer - flight) / (0.f - flight);
        if (exe_real(s, 0x0032B0F8) < fraction)
        {
            w.background_fade = exe_real(s, 0x0032B0F8);
            return;
        }
        if (fraction < 0.f)
        {
            w.background_fade = 0.f;
            return;
        }
    }
    w.background_fade = fraction;
}

void set_contents_state(GameContentsWidget& w, i32 state, GameSelectControlServices& s);

// Xbox 0x000D9D10, ESI the widget: the selected chapter grows into place, then its prompts open.
void tick_chapter_select(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    auto& panel = chapter_panel(w, index);
    auto& panel_sprite = panel.sprite;
    tick_chapter_fade(w, s); // 0x000DA300
    if (w.old_background.texture.data() != w.background.texture.data())
    {
        const u8 alpha = truncate_byte(w.background_fade * exe_real(s, 0x0032B158)); // 0x001673D8
        set_color_alpha(w.background.color, alpha);
        set_color_alpha(w.old_background.color, u8(0xFF - alpha));
    }
    const float fraction = w.background_fade;
    panel.box_color = blend_color(exe_word(s, 0x003069E8), exe_word(s, 0x003069EC), fraction, s); // 0x00101730
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    const float width = texture_width(w.chapters[index].sprite.texture);
    const float rest_x = (exe_real(s, layout + 8) * screen_width(s)) / width; // DAT_003069A4
    panel_sprite.scale[0] = ((exe_real(s, layout) * screen_width(s)) / width - rest_x) * fraction + rest_x;
    const float height = texture_height(w.chapters[index].sprite.texture);
    const float rest_y = (screen_height(s) * exe_real(s, layout + 0xC)) / height; // DAT_003069A8
    panel_sprite.scale[1] = ((screen_height(s) * exe_real(s, layout + 4)) / height - rest_y) * fraction + rest_y;
    const float scale_x = exe_real(s, 0x0032B6B0);
    const float base = exe_real(s, 0x0032B470);
    const float grow = exe_real(s, 0x0032B234);
    const float from_x = exe_real(s, layout + 8) * scale_x + base;
    panel.box_width = ((exe_real(s, layout) * scale_x + grow) - from_x) * fraction + from_x;
    const float from_y = exe_real(s, layout + 0xC) * exe_real(s, 0x0032B6AC) + base;
    panel.box_height = ((exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + grow) - from_y) * fraction + from_y;
    if (exe_real(s, 0x0032B0F8) <= fraction)
    {
        if (index == 0)
        {
            set_contents_state(w, w.start_mode == 3 ? 3 : 2, s);
        }
        else if (index == 1)
        {
            set_contents_state(w, 5, s);
        }
        else if (index == 2)
        {
            set_contents_state(w, 4, s);
        }
    }
}

// Xbox 0x000DA3B0, ESI the widget: the opening fade of every chapter's colors.
void tick_contents_fade_in(GameContentsWidget& w, GameSelectControlServices& s)
{
    const u32 white = exe_word(s, 0x00314F48);
    const u32 rest = exe_word(s, 0x003069EC);
    count_down(w.panel_fade_timer, frame_step(s));
    const float scaled = (w.panel_fade_timer - exe_real(s, 0x0032B110)) * exe_real(s, 0x0032B1C4);
    float fraction;
    if (scaled <= exe_real(s, 0x0032B0F8))
    {
        fraction = 0.f <= scaled ? scaled : 0.f;
    }
    else
    {
        fraction = exe_real(s, 0x0032B0F8);
    }
    const u32 faded_white = with_alpha(white, truncate_byte(fraction * exe_real(s, 0x0032B158))); // 0x001673D8
    const u32 faded_rest = with_alpha(rest, truncate_byte(fraction * exe_real(s, 0x0032B158)));
    for (size_t index = 0; index < 3; ++index)
    {
        chapter_sprite(w, index).color = faded_white;
        chapter_panel(w, index).box_color = faded_rest;
    }
    const u32 lit = exe_word(s, 0x003069E8);
    chapter_panel(w, chapter(w)).box_color = lit;
    set_color_alpha(chapter_panel(w, chapter(w)).box_color, truncate_byte(fraction * exe_real(s, 0x0032B158)));
    if (exe_real(s, 0x0032B370) <= fraction)
    {
        w.panels_fading = 0;
        for (size_t index = 0; index < 3; ++index)
        {
            chapter_sprite(w, index).color = white;
            chapter_panel(w, index).box_color = rest;
        }
        chapter_panel(w, chapter(w)).box_color = lit;
    }
}

// Xbox 0x000DAD50: the completed splat over the completed line.
void show_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget; // DAT_005462CC
    if (w.splat.texture.data() == nullptr)
    {
        w.splat.texture = acquire_animation_resource(s.resources, "aid_texture_ghoulies_gameselect_completedsplat");
        if (w.splat.texture.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
    }
    auto& splat = w.splat;
    splat.color = exe_word(s, 0x00314F48);
    set_color_alpha(splat.color, 0x9B);
    splat.position[0] = s.hud.center_x;
    splat.position[1] = s.hud.center_y;
    const float height = float(s.metrics.fonts[0]->drop) * exe_real(s, 0x0032B3D0); // DAT_00504444 +0x10
    const float width = runtime_text_line_width(s.metrics, 0,
        w.completed.text.data(), exe_real(s, 0x0032B3D0)); // 0x00060710
    splat.position[1] = width * exe_real(s, 0x0032B0F4) + splat.position[1] + exe_real(s, 0x0032B548);
    w.splat_full_scale[0] = height / texture_width(w.splat.texture);
    w.splat_full_scale[1] = width / texture_height(w.splat.texture);
    w.splat_full_scale[0] = w.splat_full_scale[0] * exe_real(s, 0x0032B544);
    w.splat_full_scale[1] = w.splat_full_scale[1] * exe_real(s, 0x0032B540);
    float time;
    if (w.splat_phase == 3)
    {
        time = exe_real(s, 0x0032B1AC) - w.splat_timer;
    }
    else
    {
        splat.scale[0] = 0.f;
        splat.scale[1] = 0.f;
        time = exe_real(s, 0x0032B1AC);
    }
    w.splat_timer = time;
    w.splat_phase = 1;
}

// Xbox 0x000DAED0: the splat shrinks away.
void hide_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    float time = exe_real(s, 0x0032B1AC);
    if (w.splat_phase == 1)
    {
        time = exe_real(s, 0x0032B1AC) - w.splat_timer;
    }
    w.splat_timer = time;
    w.splat_phase = 3;
}

// Xbox 0x000DAF10: the splat's grow (1), hold (2) and shrink (3).
void tick_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    if (w.splat.texture.data() == nullptr)
    {
        return;
    }
    const i32 phase = w.splat_phase;
    auto& splat = w.splat;
    float* scale = splat.scale.data();
    const float* full_scale = w.splat_full_scale.data();
    if (phase == 0)
    {
        release_texture(w.splat.texture, s);
        return;
    }
    const float step = frame_step(s);
    const float rate = exe_real(s, 0x0032B37C);
    const float total = exe_real(s, 0x0032B1AC);
    if (phase == 3)
    {
        if (0.f < w.splat_timer)
        {
            const float left = w.splat_timer - step;
            w.splat_timer = left;
            if (left <= 0.f)
            {
                w.splat_timer = 0.f;
                w.splat_phase = 0;
                splat.scale[0] = 0.f;
                splat.scale[1] = 0.f;
                return;
            }
        }
        // The splat's scale shrinks from its full scale, each axis clamped between 0 and the full value.
        for (size_t axis = 0; axis < 2; ++axis)
        {
            const float full = full_scale[axis];
            float value = (w.splat_timer - total) * rate * (0.f - full) + full;
            if (0.f <= full)
            {
                if (value < 0.f)
                {
                    value = 0.f;
                }
                else if (!(value < full) && full != value)
                {
                    value = full;
                }
            }
            else if (0.f < value)
            {
                value = 0.f;
            }
            else if (!(full < value) && full != value)
            {
                value = full;
            }
            scale[axis] = value;
        }
        return;
    }
    if (phase != 1)
    {
        return;
    }
    if (0.f < w.splat_timer)
    {
        const float left = w.splat_timer - step;
        w.splat_timer = left;
        if (left <= 0.f)
        {
            w.splat_timer = 0.f;
            w.splat_phase = 2;
            splat.scale[0] = w.splat_full_scale[0];
            splat.scale[1] = w.splat_full_scale[1];
            return;
        }
    }
    // The splat's scale grows toward its full scale, each axis clamped between 0 and the full value.
    for (size_t axis = 0; axis < 2; ++axis)
    {
        const float full = full_scale[axis];
        float value = (w.splat_timer - total) * rate * full;
        if (full <= 0.f)
        {
            if (full <= value && !(value < 0.f) && value != 0.f)
            {
                value = 0.f;
            }
            else if (value < full)
            {
                value = full;
            }
        }
        else if (value <= full)
        {
            if (!(0.f < value) && value != 0.f)
            {
                value = 0.f;
            }
        }
        else
        {
            value = full;
        }
        scale[axis] = value;
    }
}

// Xbox 0x00142E50, EAX the sequence: empty, waiting (state 1).
void reset_button_sequence(ButtonSequence& sequence)
{
    auto& b = sequence;
    b.last_press = 0xFFFFFFFFu;
    b.last_held = 0xFFFFFFFFu;
    b.held = {0, 0};
    b.presses = {};
    b.step = 0;
    b.step_pressed = 0;
    b.step_released = 0;
    b.state = 1;
}

// Xbox 0x000DB1A0: the contents' secret sequence (held: black and white; pressed: B, Y, X, A, X, Y, B).
void build_button_sequence(GameContentsWidget& w)
{
    auto& b = *w.sequence;
    u32* presses = b.presses.data();
    u32* held = b.held.data();
    // A held button is added unless a press already uses it, and a press unless a held button uses it.
    const auto hold = [&](u32 mask)
    {
        for (i32 index = 0; index <= i32(b.last_press); ++index)
        {
            if ((presses[size_t(index)] & mask) != 0)
            {
                return;
            }
        }
        const u32 next = b.last_held + 1;
        b.last_held = next;
        held[next] = mask;
    };
    const auto press = [&](u32 mask)
    {
        for (size_t index = 0; index < 2; ++index)
        {
            if ((held[index] & mask) != 0)
            {
                return;
            }
        }
        const u32 next = b.last_press + 1;
        b.last_press = next;
        presses[next] = mask;
    };
    hold(0x4000);
    hold(0x8000);
    press(0x40);
    press(0x80);
    press(0x2000);
    press(0x1000);
    press(0x2000);
    press(0x80);
    press(0x40);
    if (b.last_press != 0xFFFFFFFFu)
    {
        b.step_released = 0;
        b.step_pressed = 0;
        b.step = 0;
        b.state = i32(b.last_held) > -1 ? 2 : 3;
    }
}

// Xbox 0x00142F00, ESI the sequence: the held buttons, then each press and release in order; 4 when done.
void tick_button_sequence(ButtonSequence& sequence, const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& input)
{
    auto& b = sequence;
    const u32* presses = b.presses.data();
    const u32* held_buttons = b.held.data();
    const auto restart = [&](u32 state)
    {
        b.state = state;
        b.step = 0;
        b.step_pressed = 0;
        b.step_released = 0;
    };
    const u32 state = b.state;
    if (state == 2)
    {
        bool held = true;
        for (size_t index = 0; index < 2 && held; ++index)
        {
            const u32 mask = held_buttons[index];
            if (mask != 0 && runtime_input_button_held(device, u16(mask), input) == 0) // 0x00108300
            {
                held = false;
            }
        }
        if (held)
        {
            restart(3);
        }
        return;
    }
    if (state != 3)
    {
        return;
    }
    if (held_buttons[0] != 0)
    {
        for (size_t index = 0; index < 2; ++index)
        {
            if (runtime_input_button_release(device, u16(held_buttons[index]), input) != 0) // 0x00108500
            {
                restart(2);
                break;
            }
        }
    }
    const u16 expected = u16(presses[b.step]);
    if (b.step_pressed == 0)
    {
        if (runtime_input_button_edge(device, expected, input) != 0) // 0x001083F0
        {
            b.step_pressed = 1;
        }
        else if (runtime_input_button_edge(device, 0xFFFF, input) != 0)
        {
            restart(3);
        }
    }
    else if (b.step_released == 0)
    {
        if (runtime_input_button_release(device, expected, input) != 0)
        {
            b.step_released = 1;
        }
        else if (runtime_input_button_edge(device, 0xFFFF, input) != 0)
        {
            restart(3);
        }
    }
    if (b.step_pressed != 0 && b.step_released != 0)
    {
        const u32 next = b.step + 1;
        b.step_pressed = 0;
        b.step_released = 0;
        b.step = next;
        if (i32(b.last_press) < i32(next))
        {
            b.state = 4;
        }
    }
}

// Xbox 0x000DB380: the sequence's unlock (progress bit 0x3D), its sound and a save.
void unlock_secret(GameSelectControlServices& s)
{
    if ((s.profile.progress.bytes[7] & 0x20) != 0)
    {
        return;
    }
    const i16 voice = allocate_audio_event_voice(0, s.voices); // 0x00039830
    if (voice != -1)
    {
        s.voices.voices[size_t(voice)].stop = 1;
        prepare_audio_event_voice(voice, "XACT_SOUNDBANK_GPICKUPDITTIES_KEYAPPEAR", s.voices); // 0x00039A20
    }
    s.profile.progress.bytes[7] |= 0x20;
    untranslated("0x000DB380's save of the unlock (0x000F2A50, 0x0010E4F0, 0x0010DA80)");
}

// Xbox 0x0004A470: the challenges not yet played (DAT_004E194C words of zero), plus the bonus one.
i32 unplayed_challenges(GameSelectControlServices& s)
{
    i32 bits = 0;
    for (const u8 byte : s.profile.challenges.bytes) // 0x00066180
    {
        bits += std::popcount(byte);
    }
    i32 count = 0;
    for (i32 index = 0; index < bits / 5; ++index)
    {
        if (read_u32(g_game_progress.words_194c, size_t(index) * 4) == 0)
        {
            ++count;
        }
    }
    if ((s.profile.progress.bytes[4] & 0x20) != 0 && read_u32(g_game_progress.words_194c, 0x50) == 0) // DAT_004E199C
    {
        ++count;
    }
    return count;
}

// The prompt rows of the selected chapter (0x000DA580's state 6).
const u8* chapter_prompts(const GameContentsWidget& w, GameSelectControlServices& s)
{
    switch (chapter(w))
    {
    case 0:
        return s.image.memory(w.start_mode == 3 ? 0x00306D00 : 0x003069F0, 0x310).data();
    case 1:
        return s.image.memory(0x00307010, 0x310).data();
    case 2:
        return s.image.memory(0x00307320, 0x310).data();
    default:
        return nullptr;
    }
}

// Xbox 0x000DA580, EAX the widget
void set_contents_state(GameContentsWidget& w, i32 state, GameSelectControlServices& s)
{
    const i32 previous = w.state;
    auto& title = w.title;
    auto& progress = w.progress;
    auto& completed = w.completed;
    switch (previous)
    {
    case 1:
    {
        if (w.panels_fading != 0)
        {
            w.panels_fading = 0;
            reset_chapter_colors(w, s); // 0x000DA530
            title.record = line_record(*w.title_pool, title, 1.8f, s);
        }
        create_contents_bat(w, s); // 0x000D9F10
        char resource[0x80]{};
        std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
        const float edge = exe_real(s, 0x0032B1F4);
        const std::array<float, 2> anchor{s.hud.left + edge, s.hud.bottom - edge};
        const std::array<float, 2> row_step{0.f, exe_real(s, 0x0032B6A8)};
        const std::array<float, 2> origin{exe_real(s, 0x0032B6A4), exe_real(s, 0x0032B6A0)};
        std::array<char16_t, 0x200> scratch{};
        float widest = 0.f;
        for (const u32 rows : {0x003069F0u, 0x00306D00u, 0x00307320u, 0x00307010u})
        {
            const float width = measure_dialogue_widget_width(resource, s.image.memory(rows, 0x310).data(), 0.85f,
                0.76f, "gameselect", scratch.data(), s.widgets); // 0x001428F0, ECX the rows
            if (rows == 0x003069F0u ? width > exe_real(s, 0x0032B100) : width > widest)
            {
                widest = width;
            }
        }
        const std::array<float, 2> column{widest, 0.f};
        create_resource_dialogue_widget(w.prompts, 1, nullptr, anchor, row_step, origin, &column, 0.85f, 0.76f,
            "gameselect", resource, 0x400C0, s.widget_allocation); // 0x00141DA0, ESI +0xF2C
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
        break;
    }
    case 2:
        end_all_dialogue_text_records(*w.name_pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
        end_dialogue_text_record(*w.progress_pool, progress.record, s.fonts, s.fallback_texture,
            s.random); // 0x0005F7E0
        break;
    case 3:
        remove_dialogue_text_record(*w.completed_pool, completed.record); // 0x0005FB10
        hide_completed_splat(s); // 0x000DAED0
        break;
    case 4:
    case 5:
        end_all_dialogue_text_records(*w.name_pool, s.fonts, s.fallback_texture, s.random);
        break;
    default:
        break;
    }
    w.state = state;
    auto& panel = chapter_panel(w, chapter(w));
    auto& name = panel.name;
    switch (state)
    {
    case 2:
        progress.record = line_record(*w.progress_pool, progress, 0.75f, s);
        name.record = line_record(*w.name_pool, name, 1.4f, s);
        return;
    case 3:
        completed.record = line_record(*w.completed_pool, completed, 1.4f, s);
        show_completed_splat(s); // 0x000DAD50
        return;
    case 4:
    case 5:
        name.record = line_record(*w.name_pool, name, 1.4f, s);
        return;
    case 6:
    {
        const float scale = exe_real(s, 0x0032B69C);
        // Every panel back to its rest size and color.
        for (size_t index = 0; index < 3; ++index)
        {
            auto& other = chapter_panel(w, index);
            auto& other_sprite = other.sprite;
            const auto& texture = w.chapters[index].sprite.texture;
            other_sprite.scale[0] = (screen_width(s) * scale) / texture_width(texture);
            other_sprite.scale[1] = (screen_height(s) * scale) / texture_height(texture);
            other.box_width = exe_real(s, 0x0032B694);
            other.box_height = exe_real(s, 0x0032B698);
            other.box_color = exe_word(s, 0x003069EC);
        }
        select_chapter_movie(w, s); // 0x000D9F60, EDI the widget
        select_chapter_background(w, s); // 0x000DA1E0, ESI the widget
        if (w.prompts != nullptr)
        {
            set_dialogue_widget_prompt(*w.prompts, chapter_prompts(w, s), s.widgets); // 0x00142180, EAX the rows
        }
        return;
    }
    case 7:
        if (0 < panel.movie_held)
        {
            stop_chapter_movie(w); // 0x000DACE0
            auto& shared = *s.frontend.contents_widget; // DAT_005462CC
            chapter_panel(shared, chapter(shared)).movie_frames = 100;
        }
        s.frontend.contents_chapter = w.chapter_index; // DAT_005462C8
        return;
    case 8:
        end_dialogue_text_record(*w.title_pool, title.record, s.fonts, s.fallback_texture, s.random);
        end_dialogue_text_record(*w.name_pool, name.record, s.fonts, s.fallback_texture, s.random);
        if (w.prompts != nullptr)
        {
            w.prompts->requested_set = nullptr; // +0x950
            set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20
        }
        return;
    default:
        return;
    }
}

SceneControlInstance& gameselect_control()
{
    return *find_runtime_scene_control(0x9A); // 0x00116170, EDX 0x9A
}

void release_control_book(SceneControlInstance& control, GameSelectControlServices& s)
{
    if (control.gameselect->book != nullptr)
    {
        release_transition_book(control.gameselect->book, s.books); // 0x00048860
        control.gameselect->book = nullptr;
    }
}
}

// Xbox 0x000D8AB0
GameSelectWidgetHandle create_runtime_game_contents(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameContentsWidget>(); // 0x000F72D0(0xF34)
}

// Xbox 0x000D8B10
void open_runtime_game_contents(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    const u32 white = exe_word(s, 0x00314F48);
    w.owner = &control;
    (void)owner; // the control's bytes; the widget reaches the control through +0xF28
    w.name = s.frontend.selected_name.data(); // +0xF20, DAT_00429CC4
    auto& splat = w.splat;
    w.state = 0;
    w.start_mode = 0;
    splat.color = white;
    splat.position[0] = 0.f;
    splat.position[1] = 0.f;
    splat.scale[0] = 0.f;
    splat.scale[1] = 0.f;
    w.splat_timer = 0.f;
    w.bat_target[0] = 0.f;
    w.bat_target[1] = 0.f;
    w.splat.texture = {};
    w.splat_phase = 0;
    w.chapter_index = s.frontend.contents_chapter; // DAT_005462C8
    w.flown_chapter = 0xBADBADBAu;
    w.bat = nullptr;
    w.sequence = nullptr;
    s.frontend.contents_widget = &w; // DAT_005462CC
    if ((s.profile.progress.bytes[7] & 0x20) == 0)
    {
        i32 played = 0;
        for (size_t index = 0; index < 0x15; ++index) // DAT_004E194C .. DAT_004E19A4
        {
            if (read_u32(g_game_progress.words_194c, index * 4) == 6)
            {
                ++played;
            }
        }
        if (played < 0x14)
        {
            w.sequence = new ButtonSequence; // 0x000F72D0(0x70)
            reset_button_sequence(*w.sequence); // 0x00142E50
        }
    }
    if (w.sequence != nullptr)
    {
        build_button_sequence(w); // 0x000DB1A0
    }
    allocate_dialogue_text_pool(w.title_pool, 0, 1, 0xC0, s.text_pools); // 0x0005F390, ESI +0xA74
    w.title_pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    auto& title = w.title;
    auto& progress = w.progress;
    auto& completed = w.completed;
    const char16_t blank[] = u"blank";
    std::copy(std::begin(blank), std::end(blank), title.text.data());
    title.record = 0;
    title.x = s.hud.center_x;
    title.y = s.hud.center_y - exe_real(s, 0x0032B6BC);
    allocate_dialogue_text_pool(w.progress_pool, 0, 1, 0xC0, s.text_pools); // +0xC84
    const char* unset = "Progress not set"; // 0x00324CB4
    char16_t* progress_text = progress.text.data();
    for (size_t index = 0; index < 0x10; ++index)
    {
        progress_text[index] = char16_t(i16(static_cast<signed char>(unset[index]))); // MOVSX
    }
    progress_text[0x10] = 0;
    progress.x = s.hud.center_x;
    progress.y = s.hud.center_y + exe_real(s, 0x0032B1E8);
    progress.record = 0;
    allocate_dialogue_text_pool(w.name_pool, 0, 1, 0xC0, s.text_pools); // +0x850
    allocate_dialogue_text_pool(w.completed_pool, 0, 1, 0x400C0, s.text_pools); // +0xEF4
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "gameslotcontents_completed",
            completed.text.data(), s.localtext); // 0x000FF120, EDI 0x00324C98
    }
    completed.x = s.hud.center_x;
    completed.y = s.hud.center_y + exe_real(s, 0x0032B548);
    for (size_t index = 0; index < 3; ++index)
    {
        auto& panel = chapter_panel(w, index);
        auto& name = panel.name;
        auto& panel_sprite = panel.sprite;
        const u32 layout = kChapterLayout + u32(index) * 0x1C + 8; // EDI, DAT_003069A4
        const char16_t error[] = u"error";
        std::copy(std::begin(error), std::end(error), name.text.data());
        name.x = s.hud.center_x;
        name.y = s.hud.center_y + exe_real(s, 0x0032B6B8);
        name.record = 0;
        panel_sprite.position[0] = exe_real(s, layout - 0x10);
        panel_sprite.position[1] = exe_real(s, layout - 0xC);
        panel_sprite.scale[0] = exe_real(s, layout);
        panel_sprite.scale[1] = exe_real(s, layout + 4);
        const u32 row = kChapterTextures + u32(index) * 0x104;
        w.chapters[index].sprite.texture = acquire_texture(s.image.string(row), s);
        panel_sprite.color = white;
        const auto& texture = w.chapters[index].sprite.texture;
        panel_sprite.scale[0] = (screen_width(s) * exe_real(s, layout)) / texture_width(texture);
        panel_sprite.scale[1] = (screen_height(s) * exe_real(s, layout + 4)) / texture_height(texture);
        char* movie = panel.movie_name.data();
        std::strcpy(movie, s.image.string(row + 0x80));
        panel.box_color = exe_word(s, 0x003069EC);
        panel.box_width = exe_real(s, layout) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x0032B470);
        panel.box_height = exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x0032B470);
        panel.movie_held = 0;
        w.chapters[index].movie = allocate_runtime_movie(movie, 0x14, s.movies); // 0x001071A0
        panel.movie_frames = 0;
        panel.movie_frame_ready = 0;
    }
    auto& challenge = w.challenge;
    challenge.color = white;
    challenge.position[0] = 0.f;
    challenge.position[1] = 0.f;
    challenge.scale[0] = 0.f;
    challenge.scale[1] = 0.f;
    w.challenge.texture = {};
    if (unplayed_challenges(s) != 0) // 0x0004A470
    {
        w.challenge.texture =
            acquire_animation_resource(s.resources, "aid_texture_ghoulies_scoreboard_challenge_unplayed");
        if (w.challenge.texture.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        // The icon starts on the Features panel (chapter 2's sprite +0x7A0).
        challenge.position[0] = chapter_sprite(w, 2).position[0];
        challenge.position[1] = chapter_sprite(w, 2).position[1];
    }
    auto& background = w.background;
    auto& old_background = w.old_background;
    w.background.texture = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    background.color = white;
    background.position[0] = s.hud.center_x;
    background.position[1] = s.hud.center_y;
    background.scale[1] = screen_height(s) / texture_height(w.background.texture);
    background.scale[0] = screen_width(s) / texture_width(w.background.texture);
    old_background.position[0] = s.hud.center_x;
    old_background.position[1] = s.hud.center_y;
    old_background.scale[0] = background.scale[0];
    old_background.scale[1] = background.scale[1];
    w.old_background.texture = {};
    old_background.color = white;
    w.background_fade_timer = 0.f;
    w.background_fade = 0.f;
    w.panel_fade_timer = exe_real(s, 0x0032B110);
    w.panels_fading = 1;
    w.prompts = nullptr;
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    load_chapter_names(w, s); // 0x000D9C20
    const std::u16string_view name(w.name);
    char16_t* title_text = title.text.data();
    std::copy(name.begin(), name.end(), title_text); // lstrcpyW(+0x868, +0xF20)
    title_text[name.size()] = 0;
    describe_progress(w, s); // 0x000DA040
    start_pause_ditty(s.ditty, s.streams); // The 0x000E5B40 inline, while DAT_00429BB8 is clear.
    set_contents_state(w, 1, s); // 0x000DA580(1)
}

// Xbox 0x000D9300
void close_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    if (w.sequence != nullptr)
    {
        delete w.sequence; // 0x00142EB0, EBX +0xF30
        w.sequence = nullptr;
    }
    for (auto** pool : {&w.title_pool, &w.name_pool, &w.progress_pool, &w.completed_pool})
    {
        release_dialogue_text_pool(*pool, *s.text_pools); // 0x0005F460
        *pool = nullptr;
    }
    release_chapter_textures(w, s); // 0x000DA1B0
    if (w.challenge.texture.data() != nullptr)
    {
        release_texture(w.challenge.texture, s);
    }
    auto& shared = *s.frontend.contents_widget; // DAT_005462CC
    if (shared.splat.texture.data() != nullptr)
    {
        release_texture(shared.splat.texture, s);
    }
    if (w.bat != nullptr)
    {
        activate_actor_bind_tree(*w.bat); // 0x00112CC0
        w.bat = nullptr;
    }
    const u32 rest = exe_word(s, 0x00314F30);
    if (w.background.texture.data() != nullptr)
    {
        release_texture(w.background.texture, s);
        w.background.color = rest;
    }
    if (w.old_background.texture.data() != nullptr)
    {
        release_texture(w.old_background.texture, s);
        w.old_background.color = rest;
    }
    s.plate_flags[0] = w.saved_plate_flag_0;
    s.plate_flags[1] = w.saved_plate_flag_1;
    for (size_t index = 0; index < 3; ++index)
    {
        auto*& movie = w.chapters[index].movie;
        if (movie != nullptr)
        {
            auto& panel = chapter_panel(w, index);
            if (0 < panel.movie_held)
            {
                --movie->play_count;
                panel.movie_held = panel.movie_held - 1;
            }
            release_runtime_movie(*movie, s.movies); // 0x001072B0
            movie = nullptr;
        }
    }
    if (w.prompts != nullptr)
    {
        destroy_dialogue_widget(w.prompts, s.resources, *s.text_pools); // 0x00142070
        w.prompts = nullptr;
    }
}

// Xbox 0x000D9450
void tick_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    const i32 state = w.state;
    if (w.prompts != nullptr && u32(state) <= 8)
    {
        switch (state)
        {
        case 2:
        case 3:
        case 4:
        case 5:
            if (s.dialogue_page_hold() == 0) // DAT_004EE1EC
            {
                if (w.sequence != nullptr)
                {
                    tick_button_sequence(*w.sequence, frontend_device(s), s.attract.input_state); // 0x00142F00
                    if (w.sequence->state == 4)
                    {
                        unlock_secret(s); // 0x000DB380
                        delete w.sequence; // 0x00142EB0
                        w.sequence = nullptr;
                    }
                }
                s.update_prompts(*w.prompts, true); // 0x00142460, EAX DAT_00429BBC
            }
            break;
        default:
            s.update_prompts(*w.prompts, false); // 0x00142460, EAX 0
            break;
        }
    }
    switch (w.state)
    {
    case 1:
        if (w.panels_fading != 0)
        {
            tick_contents_fade_in(w, s); // 0x000DA3B0
        }
        if (!runtime_gameselect_book_busy(*w.owner, s)) // 0x00083C40
        {
            set_contents_state(w, 6, s);
        }
        break;
    case 6:
        tick_chapter_select(w, s); // 0x000D9D10
        break;
    case 7:
        if (!runtime_gameselect_book_busy(*w.owner, s))
        {
            set_contents_state(w, 8, s);
        }
        break;
    default:
        break;
    }
    tick_completed_splat(s); // 0x000DAF10
    if (w.challenge.texture.data() != nullptr)
    {
        // The icon follows the Features panel (chapter 2), at its right edge.
        auto& features = chapter_sprite(w, 2);
        auto& challenge = w.challenge;
        const float scale_x = features.scale[0];
        const float offset = scale_x * exe_real(s, 0x0032B6B4) * exe_real(s, 0x0032B184);
        challenge.position[0] = features.position[0];
        challenge.position[1] = features.position[1];
        const auto& texture = w.chapters[2].sprite.texture; // +0x79C
        challenge.position[0] =
            texture_width(texture) * scale_x * exe_real(s, 0x0032B0F4) + offset + challenge.position[0];
        const float height = texture_height(texture) * features.scale[1] * exe_real(s, 0x0032B2AC);
        challenge.scale[0] = height / texture_width(w.challenge.texture);
        challenge.scale[1] = height / texture_height(w.challenge.texture);
        challenge.color = features.color;
    }
}

// Xbox 0x000D9640
void draw_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    const i32 state = w.state;
    size_t selected = chapter(w);
    if (state == 2 || state == 3 || state == 5 || state == 4)
    {
        auto& panel = chapter_panel(w, selected);
        if (w.chapters[selected].movie != nullptr)
        {
            if ((s.flags & 0x80) != 0)
            {
                if (0 < panel.movie_held)
                {
                    stop_chapter_movie(w); // 0x000DACE0
                }
            }
            else if (panel.movie_held < 1)
            {
                ++w.chapters[selected].movie->play_count;
                panel.movie_held = panel.movie_held + 1;
            }
        }
    }
    std::span<const u8> shown;
    auto& panel = chapter_panel(w, selected);
    if (state == 1 || panel.movie_frames < 7)
    {
        shown = w.chapters[selected].sprite.texture;
    }
    else if (w.chapters[selected].movie != nullptr)
    {
        shown = w.owner->texture; // The control's +0x84.
        // The movie boundary locks a texture header by its native address.
        const u32 header = s.textures.address_of(shown.data()) + read_u32(shown, 4) + u32(shown[0x12]) * 0x14;
        const bool copied = copy_runtime_movie_frame(*w.chapters[selected].movie,
            reinterpret_cast<void*>(std::uintptr_t(header)), s.movies); // 0x00107360, ECX the texture header
        if (panel.movie_frame_ready == 0 && copied)
        {
            panel.movie_frame_ready = 1;
        }
    }
    auto& background = w.background;
    if (w.background.texture.data() != nullptr && color_alpha(background.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.background.texture.data(), background.position.data(),
            background.scale.data(), 0.f, background.color, 0, s.draw); // 0x0005CE30
    }
    auto& old_background = w.old_background;
    if (w.old_background.texture.data() != nullptr && color_alpha(old_background.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.old_background.texture.data(), old_background.position.data(),
            old_background.scale.data(), 0.f, old_background.color, 0, s.draw);
    }
    auto& shared = *s.frontend.contents_widget; // DAT_005462CC
    if (shared.splat.texture.data() != nullptr)
    {
        auto& splat = shared.splat;
        draw_sprite(splat.position.data(), 0, nullptr, shared.splat.texture, splat.scale.data(),
            nullptr, splat.color, s);
    }
    // A panel: the box centred on its sprite, then the sprite.
    const auto draw_chapter = [&](size_t index)
    {
        auto& box = chapter_panel(w, index);
        auto& panel_sprite = box.sprite;
        const i32 height = i32(box.box_height);
        const i32 width = i32(box.box_width);
        const i32 y = i32(panel_sprite.position[1]) - height / 2;
        const i32 x = i32(panel_sprite.position[0]) - width / 2;
        draw_runtime_dialogue_panel(float(x), float(y), float(width), float(height), 0.1f,
            box.box_color, s.outer); // 0x00061360
        draw_sprite(panel_sprite.position.data(), 0, nullptr, w.chapters[index].sprite.texture,
            panel_sprite.scale.data(), nullptr, panel_sprite.color, s);
    };
    for (size_t index = 0; index < 3; ++index)
    {
        if (index != selected && w.chapters[index].sprite.texture.data() != nullptr)
        {
            draw_chapter(index);
        }
    }
    selected = chapter(w);
    if (w.chapters[selected].sprite.texture.data() != nullptr)
    {
        draw_chapter(selected);
    }
    auto& selected_panel = chapter_panel(w, selected);
    auto& panel_sprite = selected_panel.sprite;
    auto* movie = w.chapters[selected].movie;
    if (movie != nullptr && movie->flags != 0)
    {
        if (selected_panel.movie_frame_ready != 0)
        {
            const float top[2]{exe_real(s, 0x0032B1F4), exe_real(s, 0x0032B1B0)};
            const float bottom[2]{exe_real(s, 0x0032B588), exe_real(s, 0x0032B584)};
            const float full = exe_real(s, 0x0032B158);
            float alpha = (float(selected_panel.movie_frames) - exe_real(s, 0x0032B284))
                * exe_real(s, 0x0032B250) * full;
            if (alpha > full)
            {
                alpha = full;
            }
            else if (0.f > alpha)
            {
                alpha = 0.f;
            }
            const u32 color = with_alpha(exe_word(s, 0x00314F48), truncate_byte(alpha)); // 0x001673D8
            const float scale[2]{(texture_width(w.chapters[selected].sprite.texture) * panel_sprite.scale[0])
                    / texture_width(shown),
                (texture_height(w.chapters[selected].sprite.texture) * panel_sprite.scale[1])
                    / texture_height(shown)};
            draw_sprite(panel_sprite.position.data(), 0, top, shown, scale, bottom, color, s);
        }
        if (w.state != 1 && selected_panel.movie_frames < 100)
        {
            selected_panel.movie_frames = selected_panel.movie_frames + 1;
        }
    }
    if (w.challenge.texture.data() != nullptr)
    {
        auto& challenge = w.challenge;
        draw_sprite(challenge.position.data(), -1, nullptr, w.challenge.texture,
            challenge.scale.data(), nullptr, challenge.color, s);
    }
    if (s.dialogue_page_hold() == 0 && w.panels_fading == 0 && w.bat != nullptr)
    {
        const float scale = exe_real(s, 0x0032B110);
        if (w.bat->scale_200 != scale)
        {
            w.bat->scale_200 = scale;
            w.bat->xform_valid = 0;
        }
        s.draw_actor(*w.bat, i32(s.outer.current_display_address - 0x507018u) / 0x1E4, 3); // Vtable +0x14
    }
    if (w.prompts != nullptr && s.dialogue_page_hold() == 0)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720
    }
}

// Xbox 0x000DB450: Left or Right moves between the three chapters while the bat is idle.
void move_runtime_contents_cursor(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    if (w.bat == nullptr)
    {
        return;
    }
    const i32 bat = s.bat_screen(*w.bat).state; // +0x934
    if (bat != 1 && bat != 3)
    {
        return;
    }
    i32 index = w.chapter_index;
    std::array<float, 2> direction{};
    s.frontend_direction(direction); // 0x001429E0
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return;
    }
    const u16 pressed = gameselect_direction_code(direction, s); // 0x001080B0
    if (pressed == 4)
    {
        stop_chapter_movie(w); // 0x000DACE0
        index = index == 0 ? 2 : index - 1;
    }
    else
    {
        if (pressed != 8)
        {
            return;
        }
        stop_chapter_movie(w);
        index = index == 2 ? 0 : index + 1;
    }
    w.chapter_index = index;
    set_contents_state(w, 6, s); // 0x000DA580(6)
}

// Xbox 0x000DB510: back to slot select, the book turning back (clip 1).
void leave_runtime_game_contents(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    fly_runtime_bat_off_screen(w.bat, w.bat_target.data(), s); // 0x0013B9D0, EAX +0x854
    auto& control = gameselect_control();
    control.gameselect->requested_row = 1;
    control.gameselect->book_turn = 3;
    control.gameselect->book = create_transition_book(1, 1, nullptr, s.books); // 0x00048260(1, 1, 0)
    set_contents_state(w, 8, s); // 0x000DA580(8)
}

// Xbox 0x000DB570: play the game from where its save stands.
void play_runtime_game(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    s.next_row = 1; // DAT_00429BC0
    const std::u16string_view name(w.name);
    std::copy(name.begin(), name.end(), s.selected_game); // lstrcpyW(DAT_00429BC4, +0xF20)
    s.selected_game[name.size()] = 0;
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
    auto& control = gameselect_control();
    // 0x00083B10(1), EAX the control.
    if (control.gameselect->pending_start != 1)
    {
        control.gameselect->start_mode = 1;
        control.gameselect->pending_start = 1;
        if (s.stream_busy(2) == 0) // 0x00039D50(2, 0, 0)
        {
            control.gameselect->start_ready = 1;
        }
        else
        {
            control.gameselect->requested_row = 0x12;
            control.gameselect->book_turn = 1;
            release_control_book(control, s);
        }
    }
    set_contents_state(w, 7, s); // 0x000DA580(7)
}

// Xbox 0x000DB5E0: the stats dialog (aid_loctext_ghoulies_dialog_dialogs, "pause_stats").
void show_runtime_game_stats(GameSelectControlServices& services)
{
    auto& s = services;
    if (s.cutscenes.dialogue == nullptr)
    {
        throw Error("0x000DB5E0: the scene dialogue (0x000582E0) is not bound");
    }
    auto& dialogue = *s.cutscenes.dialogue;
    if (i32(dialogue.state.count) + 1 < 9) // DAT_004EE1E8
    {
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", "pause_stats",
            dialogue.measurement.wide_scratch, dialogue.measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, 0, dialogue); // 0x000582E0
    }
}

// Xbox 0x000DB620: row 19 (0x13).
void open_runtime_game_info(GameSelectControlServices& services)
{
    auto& s = services;
    auto& control = gameselect_control();
    control.gameselect->requested_row = 0x13;
    control.gameselect->book_turn = 1;
    release_control_book(control, s);
    set_contents_state(*s.frontend.contents_widget, 8, s);
}

// Xbox 0x000DB670: the challenges (row 6), the book turning forward.
void open_runtime_game_challenges(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    fly_runtime_bat_off_screen(w.bat, w.bat_target.data(), s); // 0x0013B9D0, EAX +0x854
    auto& control = gameselect_control();
    control.gameselect->requested_row = 6;
    control.gameselect->book_turn = 2;
    control.gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
    set_contents_state(w, 7, s);
}

// Xbox 0x000DB6D0: the features (row 8).
void open_runtime_game_features(GameSelectControlServices& services)
{
    auto& s = services;
    auto& control = gameselect_control();
    control.gameselect->requested_row = 8;
    control.gameselect->book_turn = 1;
    release_control_book(control, s);
    set_contents_state(*s.frontend.contents_widget, 7, s);
}
}
