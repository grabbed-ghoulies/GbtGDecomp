#include "gameselect_contents.h"
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

namespace field = game_contents_field;
namespace chapter_field = game_contents_field::chapter;
namespace sprite = gameselect_sprite;
namespace text_line = gameselect_text_line;

GameContentsWidget& contents(GameSelectWidget& widget)
{
    return static_cast<GameContentsWidget&>(widget);
}

// Chapter panel `index`'s bytes (+4 + index * 0x2C4).
std::span<u8> chapter_bytes(GameContentsWidget& w, size_t index)
{
    return record(w.bytes, field::chapters, index);
}

std::span<u8> chapter_sprite(GameContentsWidget& w, size_t index)
{
    return record(chapter_bytes(w, index), chapter_field::sprite);
}

// A countdown field ticked toward 0 by the frame step and clamped there (0x000DA300 / 0x000DA3B0).
template <typename F>
void count_down(std::span<u8> bytes, F timer, float step)
{
    if (0.f < get(bytes, timer))
    {
        const float left = get(bytes, timer) - step;
        set(bytes, timer, left);
        if (left <= 0.f)
        {
            set(bytes, timer, 0.f);
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
    return size_t(get(w.bytes, field::chapter_index));
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
u32 line_record(RuntimeDialogueTextPool& pool, std::span<u8> line, float scale, GameSelectControlServices& s)
{
    return text_record(pool, field_data(line, text_line::text), get(line, text_line::x), get(line, text_line::y),
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

// Xbox 0x001080B0, ESI the direction: an axis inside (DAT_0032B274, DAT_0032B1AC) is dropped; the larger axis
// gives up (1), down (2), left (4) or right (8).
u16 direction_code(std::array<float, 2> direction, GameSelectControlServices& s)
{
    const float dead = exe_real(s, 0x0032B1AC);
    const float low = exe_real(s, 0x0032B274);
    if (dead > direction[0] && direction[0] > low)
    {
        direction[0] = 0.f;
    }
    if (dead > direction[1] && direction[1] > low)
    {
        direction[1] = 0.f;
    }
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return 0;
    }
    if (std::fabs(direction[0]) < std::fabs(direction[1])) // 0x00152BB0
    {
        return direction[1] > exe_real(s, 0x0032B100) ? 1 : 2;
    }
    return direction[0] > exe_real(s, 0x0032B100) ? 8 : 4;
}

// Xbox 0x0013B9D0, ECX the bat, EAX its point: the bat flies off the nearer screen edge.
void fly_bat_off_screen(Actor* bat, const float* point, GameSelectControlServices& s)
{
    const float width = screen_width(s);
    const float height = screen_height(s);
    const float half = exe_real(s, 0x0032B0F4);
    ButtonBatPoint target{};
    target.x = width - point[0] > width * half ? width + exe_real(s, 0x0032B1E0) : exe_real(s, 0x0032B304);
    target.y = height - point[1] > height * half ? height + exe_real(s, 0x0032B1E0) : exe_real(s, 0x0032B304);
    if (bat != nullptr)
    {
        s.fly_bat(*bat, target, 1.f); // 0x0006DA00
    }
}

const RuntimeDialogueInputDevice& frontend_device(GameSelectControlServices& s)
{
    return s.input_view(s.frontend_input); // DAT_00429BBC
}

// Xbox 0x0004B4D0, EAX the script: its chapter (1..9 from the names at 0x004232F8), or 0.
i32 script_chapter(const char* script, GameSelectControlServices& s)
{
    if (std::strncmp("aid_script_ghoulies_chapter", script, 0x1B) != 0)
    {
        return 0;
    }
    for (i32 index = 1; index < 10; ++index)
    {
        const char* name = s.image.string(exe_word(s, 0x004232F4 + u32(index) * 4));
        if (std::strncmp(name, script + 0x1B, std::strlen(name)) == 0)
        {
            return index;
        }
    }
    return 0;
}

// Xbox 0x0004B530, EDX the chapter: the scenes of the chapters before it (DAT_002FE670 counts).
i32 scenes_before(i32 index, GameSelectControlServices& s)
{
    i32 total = 0;
    for (i32 count = 1; count < index; ++count)
    {
        total += i32(exe_word(s, 0x002FE670 + u32(count) * 4));
    }
    return total;
}

// Xbox 0x0011EBC0, EBX the script: the scene number after "aid_script_ghoulies_chapter<name>_scene".
i32 script_scene(const char* script, GameSelectControlServices& s)
{
    const i32 index = script_chapter(script, s); // 0x0004B4D0
    if (index == 0)
    {
        return 0;
    }
    std::string prefix = s.image.string(exe_word(s, 0x00430CFC)); // "aid_script_ghoulies_chapter"
    prefix += s.image.string(exe_word(s, 0x004232F4 + u32(index) * 4));
    prefix += s.image.string(exe_word(s, 0x00430D00)); // "_scene"
    return i32(std::atol(script + prefix.size()));
}

// Xbox 0x0011EC70, EAX the script: the scene's number within its book (chapters 2..4 and 5..7 count on).
i32 book_scene(const char* script, GameSelectControlServices& s)
{
    const i32 index = script_chapter(script, s); // 0x0004B4D0
    i32 scene = script_scene(script, s); // 0x0011EBC0
    i32 first;
    switch (index)
    {
    case 2:
    case 3:
    case 4:
        first = 2;
        break;
    case 5:
    case 6:
    case 7:
        first = 5;
        break;
    default:
        return scene;
    }
    i32 total = 0;
    for (i32 count = first; count < index; ++count)
    {
        total += i32(exe_word(s, 0x002FE670 + u32(count) * 4));
    }
    return scene + total;
}

// Xbox 0x0011ED30, EAX the script: the scene's ordinal over all chapters, or -1.
i32 scene_ordinal(const char* script, GameSelectControlServices& s)
{
    const i32 scene = script_scene(script, s); // 0x0011EBC0
    const i32 index = script_chapter(script, s); // 0x0004B4D0
    if (index == 0)
    {
        return -1;
    }
    if (scene != 0)
    {
        return scenes_before(index, s) - 1 + scene;
    }
    return scenes_before(index, s); // 0x0004B530
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

// Xbox 0x000DA040: the game's start mode (+0xF24) and its progress line at +0xA78.
bool describe_progress(GameContentsWidget& w, GameSelectControlServices& s)
{
    if (save_game_word(w.name, 0x184) == 0) // 0x0010DC20
    {
        set(w.bytes, field::start_mode, save_game_word(w.name, 0x188) != 0 ? 3 : 2); // 0x0010DC60
    }
    else
    {
        set(w.bytes, field::start_mode, 1);
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
    const i32 index = script_chapter(script, s); // 0x0004B4D0
    std::u16string chapter_line(chapter_text.data());
    chapter_line += u" ";
    chapter_line += std::u16string(std::to_wstring(i32(exe_word(s, 0x002FE698 + u32(index) * 4))).begin(),
        std::to_wstring(i32(exe_word(s, 0x002FE698 + u32(index) * 4))).end()); // wsprintfW("%s %d")
    i32 scene = book_scene(script, s); // 0x0011EC70
    if (scene == 0)
    {
        scene = 1;
    }
    std::u16string scene_line(scene_text.data());
    scene_line += u" ";
    const std::wstring number = std::to_wstring(scene);
    scene_line += std::u16string(number.begin(), number.end());
    std::array<char16_t, 0x800> name{};
    const std::u16string place = scene_name(scene_ordinal(script, s), name, s); // 0x0011ED30, 0x0011ED80, lstrcpyW
    const std::u16string line = chapter_line + scene_line + u" : " + place; // wsprintfW("%s%s : %s")
    char16_t* text = field_data(record(w.bytes, field::progress), text_line::text);
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
        const auto panel = chapter_bytes(w, index);
        if (assets.texture.data() != nullptr)
        {
            release_texture(assets.texture, s);
        }
        const u32 row = kChapterTextures + u32(index) * 0x104;
        assets.texture = acquire_texture(s.image.string(row), s);
        std::strcpy(field_data(panel, chapter_field::movie_name), s.image.string(row + 0x80));
        set(panel, chapter_field::name_key, exe_word(s, row - 4));
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return false;
        }
        // 0x000FF120, EDI the key (+0x210), EBX the chapter's name (+4).
        if (lookup_borrowed_localtext(s.text, "gameselect", s.image.string(get(panel, chapter_field::name_key)),
                field_data(record(panel, chapter_field::name), text_line::text), s.localtext) == 0)
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
        if (record.texture.data() != nullptr)
        {
            release_texture(record.texture, s);
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
        set(chapter_sprite(w, index), sprite::color, white);
        set(chapter_bytes(w, index), chapter_field::box_color, rest);
    }
    set(chapter_bytes(w, chapter(w)), chapter_field::box_color, exe_word(s, 0x003069E8));
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
    const auto panel = chapter_bytes(w, chapter(w));
    auto* movie = w.chapters[chapter(w)].movie;
    if (movie != nullptr)
    {
        --movie->play_count;
        set(panel, chapter_field::movie_held, get(panel, chapter_field::movie_held) - 1);
        set(panel, chapter_field::movie_frames, 0);
        set(panel, chapter_field::movie_frame_ready, 0);
    }
}

// Xbox 0x000D9F60, EDI the widget: the selected chapter's movie starts and the bat flies to it.
bool select_chapter_movie(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    if (get(w.bytes, field::flown_chapter) == u32(index))
    {
        return true;
    }
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    set(w.bytes, field::bat_target_x, exe_real(s, layout - 8));
    set(w.bytes, field::bat_target_y, exe_real(s, layout + 0x10) + exe_real(s, layout - 4));
    const auto panel = chapter_bytes(w, index);
    if (get(panel, chapter_field::movie_held) < 1)
    {
        ++w.chapters[index].movie->play_count;
        set(panel, chapter_field::movie_held, get(panel, chapter_field::movie_held) + 1);
        set(panel, chapter_field::movie_frames, 0);
        set(panel, chapter_field::movie_frame_ready, 0);
    }
    if (w.bat != nullptr)
    {
        const ButtonBatPoint target{get(w.bytes, field::bat_target_x), get(w.bytes, field::bat_target_y)};
        s.fly_bat(*w.bat, target, 2.f); // 0x0006DA00, EBX +0x854
        set(w.bytes, field::flown_chapter, u32(index));
        return true;
    }
    return false;
}

// Xbox 0x000DA1E0, ESI the widget: the selected chapter's background, the old one kept for the cross-fade.
void select_chapter_background(GameContentsWidget& w, GameSelectControlServices& s)
{
    if (w.old_background.data() != nullptr)
    {
        release_texture(w.old_background, s);
    }
    w.old_background = w.background;
    w.background = {};
    const auto background = record(w.bytes, field::background);
    const auto old_background = record(w.bytes, field::old_background);
    set(old_background, sprite::scale_x, get(background, sprite::scale_x));
    set(old_background, sprite::scale_y, get(background, sprite::scale_y));
    set(old_background, sprite::x, get(background, sprite::x));
    set(old_background, sprite::y, get(background, sprite::y));
    w.background = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    set(background, sprite::scale_y, screen_height(s) / texture_height(w.background));
    set(background, sprite::scale_x, screen_width(s) / texture_width(w.background));
    set(w.bytes, field::background_fade_timer, s.bat_screen(*w.bat).flight_duration); // Bat +0x93C
    set(w.bytes, field::background_fade, 0.f);
    field_byte(background, sprite::color, 3) = 0;
    field_byte(old_background, sprite::color, 3) = 0xFF;
}

// Xbox 0x000DA300, ECX the widget: the cross-fade's fraction from the bat's flight.
void tick_chapter_fade(GameContentsWidget& w, GameSelectControlServices& s)
{
    count_down(w.bytes, field::background_fade_timer, frame_step(s));
    const float flight = s.bat_screen(*w.bat).flight_duration; // +0x93C
    float fraction = exe_real(s, 0x0032B0F8);
    if (flight != 0.f)
    {
        fraction = (get(w.bytes, field::background_fade_timer) - flight) / (0.f - flight);
        if (exe_real(s, 0x0032B0F8) < fraction)
        {
            set(w.bytes, field::background_fade, exe_real(s, 0x0032B0F8));
            return;
        }
        if (fraction < 0.f)
        {
            set(w.bytes, field::background_fade, 0.f);
            return;
        }
    }
    set(w.bytes, field::background_fade, fraction);
}

void set_contents_state(GameContentsWidget& w, i32 state, GameSelectControlServices& s);

// Xbox 0x000D9D10, ESI the widget: the selected chapter grows into place, then its prompts open.
void tick_chapter_select(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    const auto panel = chapter_bytes(w, index);
    const auto panel_sprite = record(panel, chapter_field::sprite);
    tick_chapter_fade(w, s); // 0x000DA300
    if (w.old_background.data() != w.background.data())
    {
        const u8 alpha = truncate_byte(get(w.bytes, field::background_fade) * exe_real(s, 0x0032B158)); // 0x001673D8
        field_byte(record(w.bytes, field::background), sprite::color, 3) = alpha;
        field_byte(record(w.bytes, field::old_background), sprite::color, 3) = u8(0xFF - alpha);
    }
    const float fraction = get(w.bytes, field::background_fade);
    set(panel, chapter_field::box_color,
        blend_color(exe_word(s, 0x003069E8), exe_word(s, 0x003069EC), fraction, s)); // 0x00101730
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    const float width = texture_width(w.chapters[index].texture);
    const float rest_x = (exe_real(s, layout + 8) * screen_width(s)) / width; // DAT_003069A4
    set(panel_sprite, sprite::scale_x, ((exe_real(s, layout) * screen_width(s)) / width - rest_x) * fraction + rest_x);
    const float height = texture_height(w.chapters[index].texture);
    const float rest_y = (screen_height(s) * exe_real(s, layout + 0xC)) / height; // DAT_003069A8
    set(panel_sprite, sprite::scale_y,
        ((screen_height(s) * exe_real(s, layout + 4)) / height - rest_y) * fraction + rest_y);
    const float scale_x = exe_real(s, 0x0032B6B0);
    const float base = exe_real(s, 0x0032B470);
    const float grow = exe_real(s, 0x0032B234);
    const float from_x = exe_real(s, layout + 8) * scale_x + base;
    set(panel, chapter_field::box_width, ((exe_real(s, layout) * scale_x + grow) - from_x) * fraction + from_x);
    const float from_y = exe_real(s, layout + 0xC) * exe_real(s, 0x0032B6AC) + base;
    set(panel, chapter_field::box_height,
        ((exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + grow) - from_y) * fraction + from_y);
    if (exe_real(s, 0x0032B0F8) <= fraction)
    {
        if (index == 0)
        {
            set_contents_state(w, get(w.bytes, field::start_mode) == 3 ? 3 : 2, s);
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
    count_down(w.bytes, field::panel_fade_timer, frame_step(s));
    const float scaled = (get(w.bytes, field::panel_fade_timer) - exe_real(s, 0x0032B110)) * exe_real(s, 0x0032B1C4);
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
        set(chapter_sprite(w, index), sprite::color, faded_white);
        set(chapter_bytes(w, index), chapter_field::box_color, faded_rest);
    }
    const u32 lit = exe_word(s, 0x003069E8);
    set(chapter_bytes(w, chapter(w)), chapter_field::box_color, lit);
    field_byte(chapter_bytes(w, chapter(w)), chapter_field::box_color, 3) =
        truncate_byte(fraction * exe_real(s, 0x0032B158));
    if (exe_real(s, 0x0032B370) <= fraction)
    {
        set(w.bytes, field::panels_fading, 0);
        for (size_t index = 0; index < 3; ++index)
        {
            set(chapter_sprite(w, index), sprite::color, white);
            set(chapter_bytes(w, index), chapter_field::box_color, rest);
        }
        set(chapter_bytes(w, chapter(w)), chapter_field::box_color, lit);
    }
}

// Xbox 0x000DAD50: the completed splat over the completed line.
void show_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget; // DAT_005462CC
    if (w.splat.data() == nullptr)
    {
        w.splat = acquire_animation_resource(s.resources, "aid_texture_ghoulies_gameselect_completedsplat");
        if (w.splat.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
    }
    const auto splat = record(w.bytes, field::splat);
    set(splat, sprite::color, exe_word(s, 0x00314F48));
    field_byte(splat, sprite::color, 3) = 0x9B;
    set(splat, sprite::x, s.hud.center_x);
    set(splat, sprite::y, s.hud.center_y);
    const float height = float(s.metrics.fonts[0]->cell) * exe_real(s, 0x0032B3D0); // DAT_00504444 +0x10
    const float width = runtime_text_line_width(s.metrics, 0,
        field_data(record(w.bytes, field::completed), text_line::text), exe_real(s, 0x0032B3D0)); // 0x00060710
    set(splat, sprite::y, width * exe_real(s, 0x0032B0F4) + get(splat, sprite::y) + exe_real(s, 0x0032B548));
    set(w.bytes, field::splat_full_scale_x, height / texture_width(w.splat));
    set(w.bytes, field::splat_full_scale_y, width / texture_height(w.splat));
    set(w.bytes, field::splat_full_scale_x, get(w.bytes, field::splat_full_scale_x) * exe_real(s, 0x0032B544));
    set(w.bytes, field::splat_full_scale_y, get(w.bytes, field::splat_full_scale_y) * exe_real(s, 0x0032B540));
    float time;
    if (get(w.bytes, field::splat_phase) == 3)
    {
        time = exe_real(s, 0x0032B1AC) - get(w.bytes, field::splat_timer);
    }
    else
    {
        set(splat, sprite::scale_x, 0.f);
        set(splat, sprite::scale_y, 0.f);
        time = exe_real(s, 0x0032B1AC);
    }
    set(w.bytes, field::splat_timer, time);
    set(w.bytes, field::splat_phase, 1);
}

// Xbox 0x000DAED0: the splat shrinks away.
void hide_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    float time = exe_real(s, 0x0032B1AC);
    if (get(w.bytes, field::splat_phase) == 1)
    {
        time = exe_real(s, 0x0032B1AC) - get(w.bytes, field::splat_timer);
    }
    set(w.bytes, field::splat_timer, time);
    set(w.bytes, field::splat_phase, 3);
}

// Xbox 0x000DAF10: the splat's grow (1), hold (2) and shrink (3).
void tick_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    if (w.splat.data() == nullptr)
    {
        return;
    }
    const i32 phase = get(w.bytes, field::splat_phase);
    const auto splat = record(w.bytes, field::splat);
    float* scale = field_data(splat, sprite::scale);
    const float* full_scale = field_data(w.bytes, field::splat_full_scale);
    if (phase == 0)
    {
        release_texture(w.splat, s);
        return;
    }
    const float step = frame_step(s);
    const float rate = exe_real(s, 0x0032B37C);
    const float total = exe_real(s, 0x0032B1AC);
    if (phase == 3)
    {
        if (0.f < get(w.bytes, field::splat_timer))
        {
            const float left = get(w.bytes, field::splat_timer) - step;
            set(w.bytes, field::splat_timer, left);
            if (left <= 0.f)
            {
                set(w.bytes, field::splat_timer, 0.f);
                set(w.bytes, field::splat_phase, 0);
                set(splat, sprite::scale_x, 0.f);
                set(splat, sprite::scale_y, 0.f);
                return;
            }
        }
        // The splat's scale shrinks from its full scale, each axis clamped between 0 and the full value.
        for (size_t axis = 0; axis < 2; ++axis)
        {
            const float full = full_scale[axis];
            float value = (get(w.bytes, field::splat_timer) - total) * rate * (0.f - full) + full;
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
    if (0.f < get(w.bytes, field::splat_timer))
    {
        const float left = get(w.bytes, field::splat_timer) - step;
        set(w.bytes, field::splat_timer, left);
        if (left <= 0.f)
        {
            set(w.bytes, field::splat_timer, 0.f);
            set(w.bytes, field::splat_phase, 2);
            set(splat, sprite::scale_x, get(w.bytes, field::splat_full_scale_x));
            set(splat, sprite::scale_y, get(w.bytes, field::splat_full_scale_y));
            return;
        }
    }
    // The splat's scale grows toward its full scale, each axis clamped between 0 and the full value.
    for (size_t axis = 0; axis < 2; ++axis)
    {
        const float full = full_scale[axis];
        float value = (get(w.bytes, field::splat_timer) - total) * rate * full;
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
    auto& b = sequence.bytes;
    const auto put = [&b](size_t offset, u32 value)
    {
        std::memcpy(b.data() + offset, &value, 4);
    };
    put(0x58, 0xFFFFFFFFu);
    put(0x64, 0xFFFFFFFFu);
    put(0x5C, 0);
    put(0x60, 0);
    for (size_t offset = 8; offset <= 0x54; offset += 4)
    {
        put(offset, 0);
    }
    put(4, 0);
    put(0x68, 0);
    put(0x6C, 0);
    put(0, 1);
}

// Xbox 0x000DB1A0: the contents' secret sequence (held: black and white; pressed: B, Y, X, A, X, Y, B).
void build_button_sequence(GameContentsWidget& w)
{
    auto& b = w.sequence->bytes;
    const auto get = [&b](size_t offset)
    {
        return read_u32(b, offset);
    };
    const auto put = [&b](size_t offset, u32 value)
    {
        std::memcpy(b.data() + offset, &value, 4);
    };
    const auto hold = [&](u32 mask)
    {
        for (i32 index = 0; index <= i32(get(0x58)); ++index)
        {
            if ((get(8 + size_t(index) * 4) & mask) != 0)
            {
                return;
            }
        }
        const u32 next = get(0x64) + 1;
        put(0x64, next);
        put(0x5C + size_t(next) * 4, mask);
    };
    const auto press = [&](u32 mask)
    {
        for (size_t index = 0; index < 2; ++index)
        {
            if ((get(0x5C + index * 4) & mask) != 0)
            {
                return;
            }
        }
        const u32 next = get(0x58) + 1;
        put(0x58, next);
        put(8 + size_t(next) * 4, mask);
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
    if (get(0x58) != 0xFFFFFFFFu)
    {
        put(0x6C, 0);
        put(0x68, 0);
        put(4, 0);
        put(0, i32(get(0x64)) > -1 ? 2 : 3);
    }
}

// Xbox 0x00142F00, ESI the sequence: the held buttons, then each press and release in order; 4 when done.
void tick_button_sequence(ButtonSequence& sequence, const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& input)
{
    auto& b = sequence.bytes;
    const auto get = [&b](size_t offset)
    {
        return read_u32(b, offset);
    };
    const auto put = [&b](size_t offset, u32 value)
    {
        std::memcpy(b.data() + offset, &value, 4);
    };
    const auto restart = [&](u32 state)
    {
        put(0, state);
        put(4, 0);
        put(0x68, 0);
        put(0x6C, 0);
    };
    const u32 state = get(0);
    if (state == 2)
    {
        bool held = true;
        for (size_t index = 0; index < 2 && held; ++index)
        {
            const u32 mask = get(0x5C + index * 4);
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
    if (get(0x5C) != 0)
    {
        for (size_t index = 0; index < 2; ++index)
        {
            if (runtime_input_button_release(device, u16(get(0x5C + index * 4)), input) != 0) // 0x00108500
            {
                restart(2);
                break;
            }
        }
    }
    const u16 expected = u16(get(8 + size_t(get(4)) * 4));
    if (get(0x68) == 0)
    {
        if (runtime_input_button_edge(device, expected, input) != 0) // 0x001083F0
        {
            put(0x68, 1);
        }
        else if (runtime_input_button_edge(device, 0xFFFF, input) != 0)
        {
            restart(3);
        }
    }
    else if (get(0x6C) == 0)
    {
        if (runtime_input_button_release(device, expected, input) != 0)
        {
            put(0x6C, 1);
        }
        else if (runtime_input_button_edge(device, 0xFFFF, input) != 0)
        {
            restart(3);
        }
    }
    if (get(0x68) != 0 && get(0x6C) != 0)
    {
        const u32 next = get(4) + 1;
        put(0x68, 0);
        put(0x6C, 0);
        put(4, next);
        if (i32(get(0x58)) < i32(next))
        {
            put(0, 4);
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
        return s.image.memory(get(w.bytes, field::start_mode) == 3 ? 0x00306D00 : 0x003069F0, 0x310).data();
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
    const i32 previous = get(w.bytes, field::state);
    const auto title = record(w.bytes, field::title);
    const auto progress = record(w.bytes, field::progress);
    const auto completed = record(w.bytes, field::completed);
    switch (previous)
    {
    case 1:
    {
        if (get(w.bytes, field::panels_fading) != 0)
        {
            set(w.bytes, field::panels_fading, 0);
            reset_chapter_colors(w, s); // 0x000DA530
            set(title, text_line::record, line_record(*w.title_pool, title, 1.8f, s));
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
        end_dialogue_text_record(*w.progress_pool, get(progress, text_line::record), s.fonts, s.fallback_texture,
            s.random); // 0x0005F7E0
        break;
    case 3:
        remove_dialogue_text_record(*w.completed_pool, get(completed, text_line::record)); // 0x0005FB10
        hide_completed_splat(s); // 0x000DAED0
        break;
    case 4:
    case 5:
        end_all_dialogue_text_records(*w.name_pool, s.fonts, s.fallback_texture, s.random);
        break;
    default:
        break;
    }
    set(w.bytes, field::state, state);
    const auto panel = chapter_bytes(w, chapter(w));
    const auto name = record(panel, chapter_field::name);
    switch (state)
    {
    case 2:
        set(progress, text_line::record, line_record(*w.progress_pool, progress, 0.75f, s));
        set(name, text_line::record, line_record(*w.name_pool, name, 1.4f, s));
        return;
    case 3:
        set(completed, text_line::record, line_record(*w.completed_pool, completed, 1.4f, s));
        show_completed_splat(s); // 0x000DAD50
        return;
    case 4:
    case 5:
        set(name, text_line::record, line_record(*w.name_pool, name, 1.4f, s));
        return;
    case 6:
    {
        const float scale = exe_real(s, 0x0032B69C);
        // Every panel back to its rest size and color.
        for (size_t index = 0; index < 3; ++index)
        {
            const auto other = chapter_bytes(w, index);
            const auto other_sprite = record(other, chapter_field::sprite);
            const auto& texture = w.chapters[index].texture;
            set(other_sprite, sprite::scale_x, (screen_width(s) * scale) / texture_width(texture));
            set(other_sprite, sprite::scale_y, (screen_height(s) * scale) / texture_height(texture));
            set(other, chapter_field::box_width, exe_real(s, 0x0032B694));
            set(other, chapter_field::box_height, exe_real(s, 0x0032B698));
            set(other, chapter_field::box_color, exe_word(s, 0x003069EC));
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
        if (0 < get(panel, chapter_field::movie_held))
        {
            stop_chapter_movie(w); // 0x000DACE0
            auto& shared = *s.frontend.contents_widget; // DAT_005462CC
            set(chapter_bytes(shared, chapter(shared)), chapter_field::movie_frames, 100);
        }
        s.frontend.contents_chapter = get(w.bytes, field::chapter_index); // DAT_005462C8
        return;
    case 8:
        end_dialogue_text_record(*w.title_pool, get(title, text_line::record), s.fonts, s.fallback_texture, s.random);
        end_dialogue_text_record(*w.name_pool, get(name, text_line::record), s.fonts, s.fallback_texture, s.random);
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

void put_control(SceneControlInstance& control, size_t offset, u32 value)
{
    std::memcpy(control.state.data() + offset, &value, 4);
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
    w.owner_state = owner;
    w.name = s.frontend.selected_name.data(); // +0xF20, DAT_00429CC4
    const auto splat = record(w.bytes, field::splat);
    set(w.bytes, field::state, 0);
    set(w.bytes, field::start_mode, 0);
    set(splat, sprite::color, white);
    set(splat, sprite::x, 0.f);
    set(splat, sprite::y, 0.f);
    set(splat, sprite::scale_x, 0.f);
    set(splat, sprite::scale_y, 0.f);
    set(w.bytes, field::splat_timer, 0.f);
    set(w.bytes, field::bat_target_x, 0.f);
    set(w.bytes, field::bat_target_y, 0.f);
    w.splat = {};
    set(w.bytes, field::splat_phase, 0);
    set(w.bytes, field::chapter_index, s.frontend.contents_chapter); // DAT_005462C8
    set(w.bytes, field::flown_chapter, 0xBADBADBAu);
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
    const auto title = record(w.bytes, field::title);
    const auto progress = record(w.bytes, field::progress);
    const auto completed = record(w.bytes, field::completed);
    const char16_t blank[] = u"blank";
    std::copy(std::begin(blank), std::end(blank), field_data(title, text_line::text));
    set(title, text_line::record, 0);
    set(title, text_line::x, s.hud.center_x);
    set(title, text_line::y, s.hud.center_y - exe_real(s, 0x0032B6BC));
    allocate_dialogue_text_pool(w.progress_pool, 0, 1, 0xC0, s.text_pools); // +0xC84
    const char* unset = "Progress not set"; // 0x00324CB4
    char16_t* progress_text = field_data(progress, text_line::text);
    for (size_t index = 0; index < 0x10; ++index)
    {
        progress_text[index] = char16_t(i16(static_cast<signed char>(unset[index]))); // MOVSX
    }
    progress_text[0x10] = 0;
    set(progress, text_line::x, s.hud.center_x);
    set(progress, text_line::y, s.hud.center_y + exe_real(s, 0x0032B1E8));
    set(progress, text_line::record, 0);
    allocate_dialogue_text_pool(w.name_pool, 0, 1, 0xC0, s.text_pools); // +0x850
    allocate_dialogue_text_pool(w.completed_pool, 0, 1, 0x400C0, s.text_pools); // +0xEF4
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "gameslotcontents_completed",
            field_data(completed, text_line::text), s.localtext); // 0x000FF120, EDI 0x00324C98
    }
    set(completed, text_line::x, s.hud.center_x);
    set(completed, text_line::y, s.hud.center_y + exe_real(s, 0x0032B548));
    for (size_t index = 0; index < 3; ++index)
    {
        const auto panel = chapter_bytes(w, index);
        const auto name = record(panel, chapter_field::name);
        const auto panel_sprite = record(panel, chapter_field::sprite);
        const u32 layout = kChapterLayout + u32(index) * 0x1C + 8; // EDI, DAT_003069A4
        const char16_t error[] = u"error";
        std::copy(std::begin(error), std::end(error), field_data(name, text_line::text));
        set(name, text_line::x, s.hud.center_x);
        set(name, text_line::y, s.hud.center_y + exe_real(s, 0x0032B6B8));
        set(name, text_line::record, 0);
        set(panel_sprite, sprite::x, exe_real(s, layout - 0x10));
        set(panel_sprite, sprite::y, exe_real(s, layout - 0xC));
        set(panel_sprite, sprite::scale_x, exe_real(s, layout));
        set(panel_sprite, sprite::scale_y, exe_real(s, layout + 4));
        const u32 row = kChapterTextures + u32(index) * 0x104;
        w.chapters[index].texture = acquire_texture(s.image.string(row), s);
        set(panel_sprite, sprite::color, white);
        const auto& texture = w.chapters[index].texture;
        set(panel_sprite, sprite::scale_x, (screen_width(s) * exe_real(s, layout)) / texture_width(texture));
        set(panel_sprite, sprite::scale_y, (screen_height(s) * exe_real(s, layout + 4)) / texture_height(texture));
        char* movie = field_data(panel, chapter_field::movie_name);
        std::strcpy(movie, s.image.string(row + 0x80));
        set(panel, chapter_field::box_color, exe_word(s, 0x003069EC));
        set(panel, chapter_field::box_width,
            exe_real(s, layout) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x0032B470));
        set(panel, chapter_field::box_height,
            exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x0032B470));
        set(panel, chapter_field::movie_held, 0);
        w.chapters[index].movie = allocate_runtime_movie(movie, 0x14, s.movies); // 0x001071A0
        set(panel, chapter_field::movie_frames, 0);
        set(panel, chapter_field::movie_frame_ready, 0);
    }
    const auto challenge = record(w.bytes, field::challenge);
    set(challenge, sprite::color, white);
    set(challenge, sprite::x, 0.f);
    set(challenge, sprite::y, 0.f);
    set(challenge, sprite::scale_x, 0.f);
    set(challenge, sprite::scale_y, 0.f);
    w.challenge = {};
    if (unplayed_challenges(s) != 0) // 0x0004A470
    {
        w.challenge = acquire_animation_resource(s.resources, "aid_texture_ghoulies_scoreboard_challenge_unplayed");
        if (w.challenge.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        // The icon starts on the Features panel (chapter 2's sprite +0x7A0).
        set(challenge, sprite::x, get(chapter_sprite(w, 2), sprite::x));
        set(challenge, sprite::y, get(chapter_sprite(w, 2), sprite::y));
    }
    const auto background = record(w.bytes, field::background);
    const auto old_background = record(w.bytes, field::old_background);
    w.background = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    set(background, sprite::color, white);
    set(background, sprite::x, s.hud.center_x);
    set(background, sprite::y, s.hud.center_y);
    set(background, sprite::scale_y, screen_height(s) / texture_height(w.background));
    set(background, sprite::scale_x, screen_width(s) / texture_width(w.background));
    set(old_background, sprite::x, s.hud.center_x);
    set(old_background, sprite::y, s.hud.center_y);
    set(old_background, sprite::scale_x, get(background, sprite::scale_x));
    set(old_background, sprite::scale_y, get(background, sprite::scale_y));
    w.old_background = {};
    set(old_background, sprite::color, white);
    set(w.bytes, field::background_fade_timer, 0.f);
    set(w.bytes, field::background_fade, 0.f);
    set(w.bytes, field::panel_fade_timer, exe_real(s, 0x0032B110));
    set(w.bytes, field::panels_fading, 1);
    w.prompts = nullptr;
    set(w.bytes, field::saved_plate_flag_0, s.plate_flags[0]);
    set(w.bytes, field::saved_plate_flag_1, s.plate_flags[1]);
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    load_chapter_names(w, s); // 0x000D9C20
    const std::u16string_view name(w.name);
    char16_t* title_text = field_data(title, text_line::text);
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
    if (w.challenge.data() != nullptr)
    {
        release_texture(w.challenge, s);
    }
    auto& shared = *s.frontend.contents_widget; // DAT_005462CC
    if (shared.splat.data() != nullptr)
    {
        release_texture(shared.splat, s);
    }
    if (w.bat != nullptr)
    {
        activate_actor_bind_tree(*w.bat); // 0x00112CC0
        w.bat = nullptr;
    }
    const u32 rest = exe_word(s, 0x00314F30);
    if (w.background.data() != nullptr)
    {
        release_texture(w.background, s);
        set(record(w.bytes, field::background), sprite::color, rest);
    }
    if (w.old_background.data() != nullptr)
    {
        release_texture(w.old_background, s);
        set(record(w.bytes, field::old_background), sprite::color, rest);
    }
    s.plate_flags[0] = get(w.bytes, field::saved_plate_flag_0);
    s.plate_flags[1] = get(w.bytes, field::saved_plate_flag_1);
    for (size_t index = 0; index < 3; ++index)
    {
        auto*& movie = w.chapters[index].movie;
        if (movie != nullptr)
        {
            const auto panel = chapter_bytes(w, index);
            if (0 < get(panel, chapter_field::movie_held))
            {
                --movie->play_count;
                set(panel, chapter_field::movie_held, get(panel, chapter_field::movie_held) - 1);
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
    const i32 state = get(w.bytes, field::state);
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
                    if (read_u32(w.sequence->bytes, 0) == 4)
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
    switch (get(w.bytes, field::state))
    {
    case 1:
        if (get(w.bytes, field::panels_fading) != 0)
        {
            tick_contents_fade_in(w, s); // 0x000DA3B0
        }
        if (!runtime_gameselect_book_busy(w.owner_state, *w.owner, s)) // 0x00083C40
        {
            set_contents_state(w, 6, s);
        }
        break;
    case 6:
        tick_chapter_select(w, s); // 0x000D9D10
        break;
    case 7:
        if (!runtime_gameselect_book_busy(w.owner_state, *w.owner, s))
        {
            set_contents_state(w, 8, s);
        }
        break;
    default:
        break;
    }
    tick_completed_splat(s); // 0x000DAF10
    if (w.challenge.data() != nullptr)
    {
        // The icon follows the Features panel (chapter 2), at its right edge.
        const auto features = chapter_sprite(w, 2);
        const auto challenge = record(w.bytes, field::challenge);
        const float scale_x = get(features, sprite::scale_x);
        const float offset = scale_x * exe_real(s, 0x0032B6B4) * exe_real(s, 0x0032B184);
        set(challenge, sprite::x, get(features, sprite::x));
        set(challenge, sprite::y, get(features, sprite::y));
        const auto& texture = w.chapters[2].texture; // +0x79C
        set(challenge, sprite::x,
            texture_width(texture) * scale_x * exe_real(s, 0x0032B0F4) + offset + get(challenge, sprite::x));
        const float height = texture_height(texture) * get(features, sprite::scale_y) * exe_real(s, 0x0032B2AC);
        set(challenge, sprite::scale_x, height / texture_width(w.challenge));
        set(challenge, sprite::scale_y, height / texture_height(w.challenge));
        set(challenge, sprite::color, get(features, sprite::color));
    }
}

// Xbox 0x000D9640
void draw_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    const i32 state = get(w.bytes, field::state);
    size_t selected = chapter(w);
    if (state == 2 || state == 3 || state == 5 || state == 4)
    {
        const auto panel = chapter_bytes(w, selected);
        if (w.chapters[selected].movie != nullptr)
        {
            if ((s.flags & 0x80) != 0)
            {
                if (0 < get(panel, chapter_field::movie_held))
                {
                    stop_chapter_movie(w); // 0x000DACE0
                }
            }
            else if (get(panel, chapter_field::movie_held) < 1)
            {
                ++w.chapters[selected].movie->play_count;
                set(panel, chapter_field::movie_held, get(panel, chapter_field::movie_held) + 1);
            }
        }
    }
    std::span<const u8> shown;
    auto panel = chapter_bytes(w, selected);
    if (state == 1 || get(panel, chapter_field::movie_frames) < 7)
    {
        shown = w.chapters[selected].texture;
    }
    else if (w.chapters[selected].movie != nullptr)
    {
        shown = w.owner->texture; // The control's +0x84.
        // The movie boundary locks a texture header by its native address.
        const u32 header = s.textures.address_of(shown.data()) + read_u32(shown, 4) + u32(shown[0x12]) * 0x14;
        const bool copied = copy_runtime_movie_frame(*w.chapters[selected].movie,
            reinterpret_cast<void*>(std::uintptr_t(header)), s.movies); // 0x00107360, ECX the texture header
        if (get(panel, chapter_field::movie_frame_ready) == 0 && copied)
        {
            set(panel, chapter_field::movie_frame_ready, 1);
        }
    }
    const auto background = record(w.bytes, field::background);
    if (w.background.data() != nullptr && field_byte(background, sprite::color, 3) != 0)
    {
        draw_runtime_dialogue_sprite(w.background.data(), field_data(background, sprite::position),
            field_data(background, sprite::scale), 0.f, get(background, sprite::color), 0, s.draw); // 0x0005CE30
    }
    const auto old_background = record(w.bytes, field::old_background);
    if (w.old_background.data() != nullptr && field_byte(old_background, sprite::color, 3) != 0)
    {
        draw_runtime_dialogue_sprite(w.old_background.data(), field_data(old_background, sprite::position),
            field_data(old_background, sprite::scale), 0.f, get(old_background, sprite::color), 0, s.draw);
    }
    auto& shared = *s.frontend.contents_widget; // DAT_005462CC
    if (shared.splat.data() != nullptr)
    {
        const auto splat = record(shared.bytes, field::splat);
        draw_sprite(field_data(splat, sprite::position), 0, nullptr, shared.splat, field_data(splat, sprite::scale),
            nullptr, get(splat, sprite::color), s);
    }
    // A panel: the box centred on its sprite, then the sprite.
    const auto draw_chapter = [&](size_t index)
    {
        const auto box = chapter_bytes(w, index);
        const auto panel_sprite = record(box, chapter_field::sprite);
        const i32 height = i32(get(box, chapter_field::box_height));
        const i32 width = i32(get(box, chapter_field::box_width));
        const i32 y = i32(get(panel_sprite, sprite::y)) - height / 2;
        const i32 x = i32(get(panel_sprite, sprite::x)) - width / 2;
        draw_runtime_dialogue_panel(float(x), float(y), float(width), float(height), 0.1f,
            get(box, chapter_field::box_color), s.outer); // 0x00061360
        draw_sprite(field_data(panel_sprite, sprite::position), 0, nullptr, w.chapters[index].texture,
            field_data(panel_sprite, sprite::scale), nullptr, get(panel_sprite, sprite::color), s);
    };
    for (size_t index = 0; index < 3; ++index)
    {
        if (index != selected && w.chapters[index].texture.data() != nullptr)
        {
            draw_chapter(index);
        }
    }
    selected = chapter(w);
    if (w.chapters[selected].texture.data() != nullptr)
    {
        draw_chapter(selected);
    }
    panel = chapter_bytes(w, selected);
    const auto panel_sprite = record(panel, chapter_field::sprite);
    auto* movie = w.chapters[selected].movie;
    if (movie != nullptr && movie->flags != 0)
    {
        if (get(panel, chapter_field::movie_frame_ready) != 0)
        {
            const float top[2]{exe_real(s, 0x0032B1F4), exe_real(s, 0x0032B1B0)};
            const float bottom[2]{exe_real(s, 0x0032B588), exe_real(s, 0x0032B584)};
            const float full = exe_real(s, 0x0032B158);
            float alpha = (float(get(panel, chapter_field::movie_frames)) - exe_real(s, 0x0032B284))
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
            const float scale[2]{(texture_width(w.chapters[selected].texture) * get(panel_sprite, sprite::scale_x))
                    / texture_width(shown),
                (texture_height(w.chapters[selected].texture) * get(panel_sprite, sprite::scale_y))
                    / texture_height(shown)};
            draw_sprite(field_data(panel_sprite, sprite::position), 0, top, shown, scale, bottom, color, s);
        }
        if (get(w.bytes, field::state) != 1 && get(panel, chapter_field::movie_frames) < 100)
        {
            set(panel, chapter_field::movie_frames, get(panel, chapter_field::movie_frames) + 1);
        }
    }
    if (w.challenge.data() != nullptr)
    {
        const auto challenge = record(w.bytes, field::challenge);
        draw_sprite(field_data(challenge, sprite::position), -1, nullptr, w.challenge,
            field_data(challenge, sprite::scale), nullptr, get(challenge, sprite::color), s);
    }
    if (s.dialogue_page_hold() == 0 && get(w.bytes, field::panels_fading) == 0 && w.bat != nullptr)
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
    i32 index = get(w.bytes, field::chapter_index);
    std::array<float, 2> direction{};
    s.frontend_direction(direction); // 0x001429E0
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return;
    }
    const u16 pressed = direction_code(direction, s); // 0x001080B0
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
    set(w.bytes, field::chapter_index, index);
    set_contents_state(w, 6, s); // 0x000DA580(6)
}

// Xbox 0x000DB510: back to slot select, the book turning back (clip 1).
void leave_runtime_game_contents(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    fly_bat_off_screen(w.bat, field_data(w.bytes, field::bat_target), s); // 0x0013B9D0, EAX +0x854
    auto& control = gameselect_control();
    put_control(control, 0x94, 1);
    put_control(control, 0x124, 3);
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
    if (read_u32(control.state, 0x118) != 1)
    {
        put_control(control, 0x11C, 1);
        put_control(control, 0x118, 1);
        if (s.stream_busy(2) == 0) // 0x00039D50(2, 0, 0)
        {
            put_control(control, 0x128, 1);
        }
        else
        {
            put_control(control, 0x94, 0x12);
            put_control(control, 0x124, 1);
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
    put_control(control, 0x94, 0x13);
    put_control(control, 0x124, 1);
    release_control_book(control, s);
    set_contents_state(*s.frontend.contents_widget, 8, s);
}

// Xbox 0x000DB670: the challenges (row 6), the book turning forward.
void open_runtime_game_challenges(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    fly_bat_off_screen(w.bat, field_data(w.bytes, field::bat_target), s); // 0x0013B9D0, EAX +0x854
    auto& control = gameselect_control();
    put_control(control, 0x94, 6);
    put_control(control, 0x124, 2);
    control.gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
    set_contents_state(w, 7, s);
}

// Xbox 0x000DB6D0: the features (row 8).
void open_runtime_game_features(GameSelectControlServices& services)
{
    auto& s = services;
    auto& control = gameselect_control();
    put_control(control, 0x94, 8);
    put_control(control, 0x124, 1);
    release_control_book(control, s);
    set_contents_state(*s.frontend.contents_widget, 7, s);
}
}
