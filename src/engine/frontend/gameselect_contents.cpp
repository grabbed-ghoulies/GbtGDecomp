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
constexpr size_t kChapter = 0x2C4;
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

u32 word(const GameContentsWidget& w, size_t offset)
{
    return read_u32(w.bytes, offset);
}

i32 integer(const GameContentsWidget& w, size_t offset)
{
    return i32(word(w, offset));
}

void put_word(GameContentsWidget& w, size_t offset, u32 value)
{
    std::memcpy(w.bytes.data() + offset, &value, 4);
}

float real(const GameContentsWidget& w, size_t offset)
{
    return std::bit_cast<float>(word(w, offset));
}

void put_real(GameContentsWidget& w, size_t offset, float value)
{
    put_word(w, offset, std::bit_cast<u32>(value));
}

float* reals(GameContentsWidget& w, size_t offset)
{
    return reinterpret_cast<float*>(w.bytes.data() + offset);
}

char16_t* wide(GameContentsWidget& w, size_t offset)
{
    return reinterpret_cast<char16_t*>(w.bytes.data() + offset);
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

size_t chapter(const GameContentsWidget& w)
{
    return size_t(integer(w, 0x85C));
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
        put_word(w, 0xF24, save_game_word(w.name, 0x188) != 0 ? 3 : 2); // 0x0010DC60
    }
    else
    {
        put_word(w, 0xF24, 1);
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
    std::copy(line.begin(), line.end(), wide(w, 0xA78));
    wide(w, 0xA78)[line.size()] = 0;
    return true;
}

// Xbox 0x000D9C20: each chapter's texture and movie name, and its localized name.
bool load_chapter_names(GameContentsWidget& w, GameSelectControlServices& s)
{
    for (size_t index = 0; index < 3; ++index)
    {
        auto& record = w.chapters[index];
        if (record.texture.data() != nullptr)
        {
            release_texture(record.texture, s);
        }
        const u32 row = kChapterTextures + u32(index) * 0x104;
        record.texture = acquire_texture(s.image.string(row), s);
        std::strcpy(reinterpret_cast<char*>(w.bytes.data() + index * kChapter + 0x22C), s.image.string(row + 0x80));
        put_word(w, index * kChapter + 0x210, exe_word(s, row - 4));
        if (s.text.data() == nullptr) // IsBadReadPtr(DAT_005462C4, 1)
        {
            return false;
        }
        // 0x000FF120, EDI the key (+0x210), EBX the chapter's name (+4).
        if (lookup_borrowed_localtext(s.text, "gameselect", s.image.string(word(w, index * kChapter + 0x210)),
                wide(w, index * kChapter + 4), s.localtext) == 0)
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
        put_word(w, index * kChapter + 0x220, white);
        put_word(w, index * kChapter + 0x2BC, rest);
    }
    put_word(w, chapter(w) * kChapter + 0x2BC, exe_word(s, 0x003069E8));
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
    const size_t p = chapter(w) * kChapter;
    auto* movie = w.chapters[chapter(w)].movie;
    if (movie != nullptr)
    {
        --movie->play_count;
        put_word(w, p + 0x2B8, word(w, p + 0x2B8) - 1);
        put_word(w, p + 0x2B0, 0);
        put_word(w, p + 0x2B4, 0);
    }
}

// Xbox 0x000D9F60, EDI the widget: the selected chapter's movie starts and the bat flies to it.
bool select_chapter_movie(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    if (word(w, 0x860) == u32(index))
    {
        return true;
    }
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    put_word(w, 0x854, exe_word(s, layout - 8));
    put_real(w, 0x858, exe_real(s, layout + 0x10) + exe_real(s, layout - 4));
    const size_t p = index * kChapter;
    if (integer(w, p + 0x2B8) < 1)
    {
        ++w.chapters[index].movie->play_count;
        put_word(w, p + 0x2B8, word(w, p + 0x2B8) + 1);
        put_word(w, p + 0x2B0, 0);
        put_word(w, p + 0x2B4, 0);
    }
    if (w.bat != nullptr)
    {
        const ButtonBatPoint target{real(w, 0x854), real(w, 0x858)};
        s.fly_bat(*w.bat, target, 2.f); // 0x0006DA00, EBX +0x854
        put_word(w, 0x860, u32(index));
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
    put_word(w, 0xCB0, word(w, 0xC98));
    put_word(w, 0xCB4, word(w, 0xC9C));
    put_word(w, 0xCA4, word(w, 0xC8C));
    put_word(w, 0xCA8, word(w, 0xC90));
    w.background = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    put_real(w, 0xC9C, screen_height(s) / texture_height(w.background));
    put_real(w, 0xC98, screen_width(s) / texture_width(w.background));
    put_real(w, 0xCB8, s.bat_screen(*w.bat).flight_duration); // Bat +0x93C
    put_real(w, 0xCBC, 0.f);
    w.bytes[0xC97] = 0;
    w.bytes[0xCAF] = 0xFF;
}

// Xbox 0x000DA300, ECX the widget: the cross-fade's fraction from the bat's flight.
void tick_chapter_fade(GameContentsWidget& w, GameSelectControlServices& s)
{
    const float step = frame_step(s);
    if (0.f < real(w, 0xCB8))
    {
        const float left = real(w, 0xCB8) - step;
        put_real(w, 0xCB8, left);
        if (left <= 0.f)
        {
            put_real(w, 0xCB8, 0.f);
        }
    }
    const float flight = s.bat_screen(*w.bat).flight_duration; // +0x93C
    float fraction = exe_real(s, 0x0032B0F8);
    if (flight != 0.f)
    {
        fraction = (real(w, 0xCB8) - flight) / (0.f - flight);
        if (exe_real(s, 0x0032B0F8) < fraction)
        {
            put_real(w, 0xCBC, exe_real(s, 0x0032B0F8));
            return;
        }
        if (fraction < 0.f)
        {
            put_real(w, 0xCBC, 0.f);
            return;
        }
    }
    put_real(w, 0xCBC, fraction);
}

void set_contents_state(GameContentsWidget& w, i32 state, GameSelectControlServices& s);

// Xbox 0x000D9D10, ESI the widget: the selected chapter grows into place, then its prompts open.
void tick_chapter_select(GameContentsWidget& w, GameSelectControlServices& s)
{
    const size_t index = chapter(w);
    const size_t p = index * kChapter;
    tick_chapter_fade(w, s); // 0x000DA300
    if (w.old_background.data() != w.background.data())
    {
        const u8 alpha = truncate_byte(real(w, 0xCBC) * exe_real(s, 0x0032B158)); // 0x001673D8
        w.bytes[0xC97] = alpha;
        w.bytes[0xCAF] = u8(0xFF - alpha);
    }
    const float fraction = real(w, 0xCBC);
    put_word(w, p + 0x2BC, blend_color(exe_word(s, 0x003069E8), exe_word(s, 0x003069EC), fraction, s)); // 0x00101730
    const u32 layout = kChapterLayout + u32(index) * 0x1C;
    const float width = texture_width(w.chapters[index].texture);
    const float rest_x = (exe_real(s, layout + 8) * screen_width(s)) / width; // DAT_003069A4
    put_real(w, p + 0x224, ((exe_real(s, layout) * screen_width(s)) / width - rest_x) * fraction + rest_x);
    const float height = texture_height(w.chapters[index].texture);
    const float rest_y = (screen_height(s) * exe_real(s, layout + 0xC)) / height; // DAT_003069A8
    put_real(w, p + 0x228, ((screen_height(s) * exe_real(s, layout + 4)) / height - rest_y) * fraction + rest_y);
    const float scale_x = exe_real(s, 0x0032B6B0);
    const float base = exe_real(s, 0x0032B470);
    const float grow = exe_real(s, 0x0032B234);
    const float from_x = exe_real(s, layout + 8) * scale_x + base;
    put_real(w, p + 0x2C0, ((exe_real(s, layout) * scale_x + grow) - from_x) * fraction + from_x);
    const float from_y = exe_real(s, layout + 0xC) * exe_real(s, 0x0032B6AC) + base;
    put_real(w, p + 0x2C4, ((exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + grow) - from_y) * fraction + from_y);
    if (exe_real(s, 0x0032B0F8) <= fraction)
    {
        if (index == 0)
        {
            set_contents_state(w, integer(w, 0xF24) == 3 ? 3 : 2, s);
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
    const float step = frame_step(s);
    if (0.f < real(w, 0xCC0))
    {
        const float left = real(w, 0xCC0) - step;
        put_real(w, 0xCC0, left);
        if (left <= 0.f)
        {
            put_real(w, 0xCC0, 0.f);
        }
    }
    const float scaled = (real(w, 0xCC0) - exe_real(s, 0x0032B110)) * exe_real(s, 0x0032B1C4);
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
        put_word(w, index * kChapter + 0x220, faded_white);
        put_word(w, index * kChapter + 0x2BC, faded_rest);
    }
    const u32 lit = exe_word(s, 0x003069E8);
    put_word(w, chapter(w) * kChapter + 0x2BC, lit);
    w.bytes[chapter(w) * kChapter + 0x2BF] = truncate_byte(fraction * exe_real(s, 0x0032B158));
    if (exe_real(s, 0x0032B370) <= fraction)
    {
        put_word(w, 0xCC4, 0);
        for (size_t index = 0; index < 3; ++index)
        {
            put_word(w, index * kChapter + 0x220, white);
            put_word(w, index * kChapter + 0x2BC, rest);
        }
        put_word(w, chapter(w) * kChapter + 0x2BC, lit);
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
    put_word(w, 0xCD8, exe_word(s, 0x00314F48));
    w.bytes[0xCDB] = 0x9B;
    put_real(w, 0xCD0, s.hud.center_x);
    put_real(w, 0xCD4, s.hud.center_y);
    const float height = float(s.metrics.fonts[0]->cell) * exe_real(s, 0x0032B3D0); // DAT_00504444 +0x10
    const float width = runtime_text_line_width(s.metrics, 0, wide(w, 0xCE8), exe_real(s, 0x0032B3D0)); // 0x00060710
    put_real(w, 0xCD4, width * exe_real(s, 0x0032B0F4) + real(w, 0xCD4) + exe_real(s, 0x0032B548));
    put_real(w, 0xEF8, height / texture_width(w.splat));
    put_real(w, 0xEFC, width / texture_height(w.splat));
    put_real(w, 0xEF8, real(w, 0xEF8) * exe_real(s, 0x0032B544));
    put_real(w, 0xEFC, real(w, 0xEFC) * exe_real(s, 0x0032B540));
    float time;
    if (integer(w, 0xCE4) == 3)
    {
        time = exe_real(s, 0x0032B1AC) - real(w, 0xCC8);
    }
    else
    {
        put_real(w, 0xCDC, 0.f);
        put_real(w, 0xCE0, 0.f);
        time = exe_real(s, 0x0032B1AC);
    }
    put_real(w, 0xCC8, time);
    put_word(w, 0xCE4, 1);
}

// Xbox 0x000DAED0: the splat shrinks away.
void hide_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    float time = exe_real(s, 0x0032B1AC);
    if (integer(w, 0xCE4) == 1)
    {
        time = exe_real(s, 0x0032B1AC) - real(w, 0xCC8);
    }
    put_real(w, 0xCC8, time);
    put_word(w, 0xCE4, 3);
}

// Xbox 0x000DAF10: the splat's grow (1), hold (2) and shrink (3).
void tick_completed_splat(GameSelectControlServices& s)
{
    auto& w = *s.frontend.contents_widget;
    if (w.splat.data() == nullptr)
    {
        return;
    }
    const i32 phase = integer(w, 0xCE4);
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
        if (0.f < real(w, 0xCC8))
        {
            const float left = real(w, 0xCC8) - step;
            put_real(w, 0xCC8, left);
            if (left <= 0.f)
            {
                put_real(w, 0xCC8, 0.f);
                put_word(w, 0xCE4, 0);
                put_real(w, 0xCDC, 0.f);
                put_real(w, 0xCE0, 0.f);
                return;
            }
        }
        for (const auto& [target, offset] : {std::pair<size_t, size_t>{0xEF8, 0xCDC}, {0xEFC, 0xCE0}})
        {
            const float full = real(w, target);
            float value = (real(w, 0xCC8) - total) * rate * (0.f - full) + full;
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
            put_real(w, offset, value);
        }
        return;
    }
    if (phase != 1)
    {
        return;
    }
    if (0.f < real(w, 0xCC8))
    {
        const float left = real(w, 0xCC8) - step;
        put_real(w, 0xCC8, left);
        if (left <= 0.f)
        {
            put_real(w, 0xCC8, 0.f);
            put_word(w, 0xCE4, 2);
            put_word(w, 0xCDC, word(w, 0xEF8));
            put_word(w, 0xCE0, word(w, 0xEFC));
            return;
        }
    }
    for (const auto& [target, offset] : {std::pair<size_t, size_t>{0xEF8, 0xCDC}, {0xEFC, 0xCE0}})
    {
        const float full = real(w, target);
        float value = (real(w, 0xCC8) - total) * rate * full;
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
        put_real(w, offset, value);
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
        return s.image.memory(integer(w, 0xF24) == 3 ? 0x00306D00 : 0x003069F0, 0x310).data();
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
    const i32 previous = integer(w, 0);
    switch (previous)
    {
    case 1:
    {
        if (word(w, 0xCC4) != 0)
        {
            put_word(w, 0xCC4, 0);
            reset_chapter_colors(w, s); // 0x000DA530
            put_word(w, 0xA70, text_record(*w.title_pool, wide(w, 0x868), real(w, 0xA68), real(w, 0xA6C), 1.8f, s));
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
        end_dialogue_text_record(*w.progress_pool, word(w, 0xC80), s.fonts, s.fallback_texture, s.random); // 0x0005F7E0
        break;
    case 3:
        remove_dialogue_text_record(*w.completed_pool, word(w, 0xEF0)); // 0x0005FB10
        hide_completed_splat(s); // 0x000DAED0
        break;
    case 4:
    case 5:
        end_all_dialogue_text_records(*w.name_pool, s.fonts, s.fallback_texture, s.random);
        break;
    default:
        break;
    }
    put_word(w, 0, u32(state));
    const size_t p = chapter(w) * kChapter;
    switch (state)
    {
    case 2:
        put_word(w, 0xC80, text_record(*w.progress_pool, wide(w, 0xA78), real(w, 0xC78), real(w, 0xC7C), 0.75f, s));
        put_word(w, p + 0x20C, text_record(*w.name_pool, wide(w, p + 4), real(w, p + 0x204), real(w, p + 0x208),
            1.4f, s));
        return;
    case 3:
        put_word(w, 0xEF0, text_record(*w.completed_pool, wide(w, 0xCE8), real(w, 0xEE8), real(w, 0xEEC), 1.4f, s));
        show_completed_splat(s); // 0x000DAD50
        return;
    case 4:
    case 5:
        put_word(w, p + 0x20C, text_record(*w.name_pool, wide(w, p + 4), real(w, p + 0x204), real(w, p + 0x208),
            1.4f, s));
        return;
    case 6:
    {
        const float scale = exe_real(s, 0x0032B69C);
        for (size_t index = 0; index < 3; ++index)
        {
            const size_t q = index * kChapter;
            const auto& texture = w.chapters[index].texture;
            put_real(w, q + 0x224, (screen_width(s) * scale) / texture_width(texture));
            put_real(w, q + 0x228, (screen_height(s) * scale) / texture_height(texture));
            put_real(w, q + 0x2C0, exe_real(s, 0x0032B694));
            put_real(w, q + 0x2C4, exe_real(s, 0x0032B698));
            put_word(w, q + 0x2BC, exe_word(s, 0x003069EC));
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
        if (0 < integer(w, p + 0x2B8))
        {
            stop_chapter_movie(w); // 0x000DACE0
            put_word(*s.frontend.contents_widget, chapter(*s.frontend.contents_widget) * kChapter + 0x2B0, 100);
        }
        s.frontend.contents_chapter = integer(w, 0x85C); // DAT_005462C8
        return;
    case 8:
        end_dialogue_text_record(*w.title_pool, word(w, 0xA70), s.fonts, s.fallback_texture, s.random);
        end_dialogue_text_record(*w.name_pool, word(w, p + 0x20C), s.fonts, s.fallback_texture, s.random);
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
    put_word(w, 0, 0);
    put_word(w, 0xF24, 0);
    put_word(w, 0xCD8, white);
    for (const size_t offset : {0xCD0u, 0xCD4u, 0xCDCu, 0xCE0u, 0xCC8u, 0x854u, 0x858u})
    {
        put_real(w, offset, 0.f);
    }
    w.splat = {};
    put_word(w, 0xCE4, 0);
    put_word(w, 0x85C, u32(s.frontend.contents_chapter)); // DAT_005462C8
    put_word(w, 0x860, 0xBADBADBAu);
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
    const char16_t blank[] = u"blank";
    std::copy(std::begin(blank), std::end(blank), wide(w, 0x868));
    put_word(w, 0xA70, 0);
    put_real(w, 0xA68, s.hud.center_x);
    put_real(w, 0xA6C, s.hud.center_y - exe_real(s, 0x0032B6BC));
    allocate_dialogue_text_pool(w.progress_pool, 0, 1, 0xC0, s.text_pools); // +0xC84
    const char* unset = "Progress not set"; // 0x00324CB4
    for (size_t index = 0; index < 0x10; ++index)
    {
        wide(w, 0xA78)[index] = char16_t(i16(static_cast<signed char>(unset[index]))); // MOVSX
    }
    wide(w, 0xA78)[0x10] = 0;
    put_real(w, 0xC78, s.hud.center_x);
    put_real(w, 0xC7C, s.hud.center_y + exe_real(s, 0x0032B1E8));
    put_word(w, 0xC80, 0);
    allocate_dialogue_text_pool(w.name_pool, 0, 1, 0xC0, s.text_pools); // +0x850
    allocate_dialogue_text_pool(w.completed_pool, 0, 1, 0x400C0, s.text_pools); // +0xEF4
    if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1)
    {
        lookup_borrowed_localtext(s.text, "gameselect", "gameslotcontents_completed", wide(w, 0xCE8),
            s.localtext); // 0x000FF120, EDI 0x00324C98
    }
    put_real(w, 0xEE8, s.hud.center_x);
    put_real(w, 0xEEC, s.hud.center_y + exe_real(s, 0x0032B548));
    for (size_t index = 0; index < 3; ++index)
    {
        const size_t p = index * kChapter;
        const u32 layout = kChapterLayout + u32(index) * 0x1C + 8; // EDI, DAT_003069A4
        const char16_t error[] = u"error";
        std::copy(std::begin(error), std::end(error), wide(w, p + 4));
        put_real(w, p + 0x204, s.hud.center_x);
        put_real(w, p + 0x208, s.hud.center_y + exe_real(s, 0x0032B6B8));
        put_word(w, p + 0x20C, 0);
        put_word(w, p + 0x218, exe_word(s, layout - 0x10));
        put_word(w, p + 0x21C, exe_word(s, layout - 0xC));
        put_word(w, p + 0x224, exe_word(s, layout));
        put_word(w, p + 0x228, exe_word(s, layout + 4));
        const u32 row = kChapterTextures + u32(index) * 0x104;
        w.chapters[index].texture = acquire_texture(s.image.string(row), s);
        put_word(w, p + 0x220, white);
        const auto& texture = w.chapters[index].texture;
        put_real(w, p + 0x224, (screen_width(s) * exe_real(s, layout)) / texture_width(texture));
        put_real(w, p + 0x228, (screen_height(s) * exe_real(s, layout + 4)) / texture_height(texture));
        char* movie = reinterpret_cast<char*>(w.bytes.data() + p + 0x22C);
        std::strcpy(movie, s.image.string(row + 0x80));
        put_word(w, p + 0x2BC, exe_word(s, 0x003069EC));
        put_real(w, p + 0x2C0, exe_real(s, layout) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x0032B470));
        put_real(w, p + 0x2C4, exe_real(s, layout + 4) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x0032B470));
        put_word(w, p + 0x2B8, 0);
        w.chapters[index].movie = allocate_runtime_movie(movie, 0x14, s.movies); // 0x001071A0
        put_word(w, p + 0x2B0, 0);
        put_word(w, p + 0x2B4, 0);
    }
    put_word(w, 0xF0C, white);
    put_real(w, 0xF04, 0.f);
    put_real(w, 0xF08, 0.f);
    put_real(w, 0xF10, 0.f);
    put_real(w, 0xF14, 0.f);
    w.challenge = {};
    if (unplayed_challenges(s) != 0) // 0x0004A470
    {
        w.challenge = acquire_animation_resource(s.resources, "aid_texture_ghoulies_scoreboard_challenge_unplayed");
        if (w.challenge.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        put_word(w, 0xF04, word(w, 0x7A0));
        put_word(w, 0xF08, word(w, 0x7A4));
    }
    w.background = acquire_texture(s.image.string(kChapterTextures + u32(chapter(w)) * 0x104), s);
    put_word(w, 0xC94, white);
    put_real(w, 0xC8C, s.hud.center_x);
    put_real(w, 0xC90, s.hud.center_y);
    put_real(w, 0xC9C, screen_height(s) / texture_height(w.background));
    put_real(w, 0xC98, screen_width(s) / texture_width(w.background));
    put_real(w, 0xCA4, s.hud.center_x);
    put_real(w, 0xCA8, s.hud.center_y);
    put_word(w, 0xCB0, word(w, 0xC98));
    put_word(w, 0xCB4, word(w, 0xC9C));
    w.old_background = {};
    put_word(w, 0xCAC, white);
    put_real(w, 0xCB8, 0.f);
    put_real(w, 0xCBC, 0.f);
    put_real(w, 0xCC0, exe_real(s, 0x0032B110));
    put_word(w, 0xCC4, 1);
    w.prompts = nullptr;
    put_word(w, 0xF18, u32(s.plate_flags[0]));
    put_word(w, 0xF1C, u32(s.plate_flags[1]));
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    load_chapter_names(w, s); // 0x000D9C20
    const std::u16string_view name(w.name);
    std::copy(name.begin(), name.end(), wide(w, 0x868)); // lstrcpyW(+0x868, +0xF20)
    wide(w, 0x868)[name.size()] = 0;
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
        put_word(w, 0xC94, rest);
    }
    if (w.old_background.data() != nullptr)
    {
        release_texture(w.old_background, s);
        put_word(w, 0xCAC, rest);
    }
    s.plate_flags[0] = integer(w, 0xF18);
    s.plate_flags[1] = integer(w, 0xF1C);
    for (size_t index = 0; index < 3; ++index)
    {
        auto*& movie = w.chapters[index].movie;
        if (movie != nullptr)
        {
            const size_t p = index * kChapter;
            if (0 < integer(w, p + 0x2B8))
            {
                --movie->play_count;
                put_word(w, p + 0x2B8, word(w, p + 0x2B8) - 1);
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
    const i32 state = integer(w, 0);
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
    switch (integer(w, 0))
    {
    case 1:
        if (word(w, 0xCC4) != 0)
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
        const float scale_x = real(w, 0x7AC);
        const float offset = scale_x * exe_real(s, 0x0032B6B4) * exe_real(s, 0x0032B184);
        put_word(w, 0xF04, word(w, 0x7A0));
        put_word(w, 0xF08, word(w, 0x7A4));
        const auto& texture = w.chapters[2].texture; // +0x79C
        put_real(w, 0xF04, texture_width(texture) * scale_x * exe_real(s, 0x0032B0F4) + offset + real(w, 0xF04));
        const float height = texture_height(texture) * real(w, 0x7B0) * exe_real(s, 0x0032B2AC);
        put_real(w, 0xF10, height / texture_width(w.challenge));
        put_real(w, 0xF14, height / texture_height(w.challenge));
        put_word(w, 0xF0C, word(w, 0x7A8));
    }
}

// Xbox 0x000D9640
void draw_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = contents(widget);
    const i32 state = integer(w, 0);
    size_t selected = chapter(w);
    if (state == 2 || state == 3 || state == 5 || state == 4)
    {
        const size_t p = selected * kChapter;
        if (w.chapters[selected].movie != nullptr)
        {
            if ((s.flags & 0x80) != 0)
            {
                if (0 < integer(w, p + 0x2B8))
                {
                    stop_chapter_movie(w); // 0x000DACE0
                }
            }
            else if (integer(w, p + 0x2B8) < 1)
            {
                ++w.chapters[selected].movie->play_count;
                put_word(w, p + 0x2B8, word(w, p + 0x2B8) + 1);
            }
        }
    }
    std::span<const u8> shown;
    size_t p = selected * kChapter;
    if (state == 1 || integer(w, p + 0x2B0) < 7)
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
        if (word(w, p + 0x2B4) == 0 && copied)
        {
            put_word(w, p + 0x2B4, 1);
        }
    }
    if (w.background.data() != nullptr && w.bytes[0xC97] != 0)
    {
        draw_runtime_dialogue_sprite(w.background.data(), reals(w, 0xC8C), reals(w, 0xC98), 0.f, word(w, 0xC94), 0,
            s.draw); // 0x0005CE30
    }
    if (w.old_background.data() != nullptr && w.bytes[0xCAF] != 0)
    {
        draw_runtime_dialogue_sprite(w.old_background.data(), reals(w, 0xCA4), reals(w, 0xCB0), 0.f, word(w, 0xCAC), 0,
            s.draw);
    }
    auto& shared = *s.frontend.contents_widget; // DAT_005462CC
    if (shared.splat.data() != nullptr)
    {
        draw_sprite(reals(shared, 0xCD0), 0, nullptr, shared.splat, reals(shared, 0xCDC), nullptr, word(shared, 0xCD8),
            s);
    }
    const auto draw_chapter = [&](size_t index)
    {
        const size_t q = index * kChapter;
        const i32 height = i32(real(w, q + 0x2C4));
        const i32 width = i32(real(w, q + 0x2C0));
        const i32 y = i32(real(w, q + 0x21C)) - height / 2;
        const i32 x = i32(real(w, q + 0x218)) - width / 2;
        draw_runtime_dialogue_panel(float(x), float(y), float(width), float(height), 0.1f, word(w, q + 0x2BC),
            s.outer); // 0x00061360
        draw_sprite(reals(w, q + 0x218), 0, nullptr, w.chapters[index].texture, reals(w, q + 0x224), nullptr,
            word(w, q + 0x220), s);
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
    p = selected * kChapter;
    auto* movie = w.chapters[selected].movie;
    if (movie != nullptr && movie->flags != 0)
    {
        if (word(w, p + 0x2B4) != 0)
        {
            const float top[2]{exe_real(s, 0x0032B1F4), exe_real(s, 0x0032B1B0)};
            const float bottom[2]{exe_real(s, 0x0032B588), exe_real(s, 0x0032B584)};
            const float full = exe_real(s, 0x0032B158);
            float alpha = (float(integer(w, p + 0x2B0)) - exe_real(s, 0x0032B284)) * exe_real(s, 0x0032B250) * full;
            if (alpha > full)
            {
                alpha = full;
            }
            else if (0.f > alpha)
            {
                alpha = 0.f;
            }
            const u32 color = with_alpha(exe_word(s, 0x00314F48), truncate_byte(alpha)); // 0x001673D8
            const float scale[2]{(texture_width(w.chapters[selected].texture) * real(w, p + 0x224))
                    / texture_width(shown),
                (texture_height(w.chapters[selected].texture) * real(w, p + 0x228)) / texture_height(shown)};
            draw_sprite(reals(w, p + 0x218), 0, top, shown, scale, bottom, color, s);
        }
        if (integer(w, 0) != 1 && integer(w, p + 0x2B0) < 100)
        {
            put_word(w, p + 0x2B0, word(w, p + 0x2B0) + 1);
        }
    }
    if (w.challenge.data() != nullptr)
    {
        draw_sprite(reals(w, 0xF04), -1, nullptr, w.challenge, reals(w, 0xF10), nullptr, word(w, 0xF0C), s);
    }
    if (s.dialogue_page_hold() == 0 && word(w, 0xCC4) == 0 && w.bat != nullptr)
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
    i32 index = integer(w, 0x85C);
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
    put_word(w, 0x85C, u32(index));
    set_contents_state(w, 6, s); // 0x000DA580(6)
}

// Xbox 0x000DB510: back to slot select, the book turning back (clip 1).
void leave_runtime_game_contents(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.contents_widget;
    fly_bat_off_screen(w.bat, reals(w, 0x854), s); // 0x0013B9D0, EAX +0x854
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
    fly_bat_off_screen(w.bat, reals(w, 0x854), s); // 0x0013B9D0, EAX +0x854
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
