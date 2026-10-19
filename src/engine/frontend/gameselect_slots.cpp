#include "gameselect_slots.h"
#include "dialogue_draw_context.h"
#include "dialogue_draw_outer.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_parameters.h"
#include "dialogue_widget_state.h"
#include "frontend_sounds.h"
#include "frontend_state.h"
#include "hud_layout.h"
#include "../audio/listener_settings.h"
#include "../entities/actor_render_registration.h"
#include "../scene/executable_resources.h"
#include "gbtg/entity.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"
#include "../scene/scene_types.h"
#include "../saves/save_summary.h"
#include "../saves/save_games.h"
#include "../video/movie_startup.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

namespace field = slot_select_field;
namespace plate_field = slot_select_field::plate;
namespace sprite = gameselect_sprite;
namespace text_line = gameselect_text_line;

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

SlotSelectWidget& slots(GameSelectWidget& widget)
{
    return static_cast<SlotSelectWidget&>(widget);
}

// Plate `index`'s bytes (+0x214 + index * 0x304).
std::span<u8> plate_bytes(SlotSelectWidget& w, size_t index)
{
    return record(w.bytes, field::plates, index);
}

std::span<const u8> plate_bytes(const SlotSelectWidget& w, size_t index)
{
    return record(w.bytes, field::plates, index);
}

// The selected plate (+0xB2C).
i32 chosen_slot(const SlotSelectWidget& w)
{
    return get(w.bytes, field::slot);
}

std::span<u8> chosen_plate(SlotSelectWidget& w)
{
    return plate_bytes(w, size_t(chosen_slot(w)));
}

i32 plate_status(const SlotSelectWidget& w, size_t index)
{
    return get(plate_bytes(w, index), plate_field::status);
}

// Status 2, 3 or 4: the slot holds a save.
bool status_has_save(i32 status)
{
    return status == 2 || status == 4 || status == 3;
}

// A countdown field ticked toward 0 by the frame step and clamped there (0x000DF330 / 0x000DF3E0 / 0x000DE7B0).
template <typename F>
void count_down(std::span<u8> bytes, F timer, float step)
{
    float value = get(bytes, timer);
    if (0.f < value)
    {
        value -= step;
        set(bytes, timer, value);
        if (value <= 0.f)
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

const char* exe_string(const GameSelectControlServices& s, u32 address)
{
    return s.image.string(address);
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

// The texture header 0x0005CA60 and 0x0005CE30 bind: +4 offset, +0x12 frame, stride 0x14.
bool texture_has_frame(std::span<const u8> texture)
{
    return texture.data() + read_u32(texture, 4) + size_t(texture[0x12]) * 0x14 != nullptr;
}

// The frame step: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

u8 truncate_byte(long double value)
{
    return u8(truncate_runtime_float(value)); // 0x001673D8, AL
}

// A color word with its alpha byte (+3) replaced.
u32 with_alpha(u32 color, u8 alpha)
{
    return (color & 0x00FFFFFFu) | (u32(alpha) << 24);
}

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
    release_animation_resource(s.resources, texture); // 0x00035C00, EBX the resource
    texture = {};
}

// One sprite through 0x0005CDC0 / 0x0005CA60, then the override hold DAT_00509B5C is released.
void draw_sprite(i32 frame, const float* uv_top, std::span<const u8> texture, const float* scale,
    const float* uv_bottom, u32 color, const float* position, GameSelectControlServices& s)
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

// A plate's sprite record drawn through draw_sprite: its scale, color and position, no UVs.
void draw_plate_sprite(i32 frame, std::span<const u8> texture, std::span<u8> sprite_bytes,
    GameSelectControlServices& s)
{
    draw_sprite(frame, nullptr, texture, field_data(sprite_bytes, sprite::scale), nullptr,
        get(sprite_bytes, sprite::color), field_data(sprite_bytes, sprite::position), s);
}

// 0x000DDF20's plate: the box panel centered on the plate, then the plate sprite.
void draw_plate(SlotSelectWidget& w, size_t index, GameSelectControlServices& s)
{
    const auto p = plate_bytes(w, index);
    const auto plate_sprite = record(p, plate_field::plate_sprite);
    const i32 box_h = i32(get(p, plate_field::box_height));
    const i32 box_w = i32(get(p, plate_field::box_width));
    const float x = float(i32(get(plate_sprite, sprite::x)) - box_w / 2);
    const float y = float(i32(get(plate_sprite, sprite::y)) - box_h / 2);
    draw_runtime_dialogue_panel(x, y, float(box_w), float(box_h), exe_real(s, 0x00428F98),
        get(p, plate_field::box_color), s.outer); // 0x00061360
    draw_plate_sprite(0, w.plates[index].plate, plate_sprite, s);
}

// Xbox 0x000DF080, EAX widget: the plate textures.
void release_plate_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.plate.data() != nullptr)
        {
            release_texture(plate.plate, s);
        }
    }
}

// Xbox 0x000DF0B0, EAX widget: the disc textures.
void release_disc_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.disc.data() != nullptr)
        {
            release_texture(plate.disc, s);
        }
    }
}

// Xbox 0x000DF110, EAX widget: the broken-disc textures.
void release_broken_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.broken.data() != nullptr)
        {
            release_texture(plate.broken, s);
        }
    }
}

// Xbox 0x000DF0E0, EAX widget: the no-write textures.
void release_nowrite_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.nowrite.data() != nullptr)
        {
            release_texture(plate.nowrite, s);
        }
    }
}

// Xbox 0x000DF580, EAX widget: the plates' colors back to rest, the selected box highlighted.
void reset_plate_colors(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 white = exe_word(s, 0x00314F48);
    const u32 rest = exe_word(s, 0x00308B1C);
    for (size_t index = 0; index < 3; ++index)
    {
        const auto plate = plate_bytes(w, index);
        set(record(plate, plate_field::plate_sprite), sprite::color, white);
        set(plate, plate_field::box_color, rest);
    }
    set(chosen_plate(w), plate_field::box_color, exe_word(s, 0x00308B18));
}

// Xbox 0x000DF3E0, ESI widget: the plates fade in over DAT_00429014 seconds.
void fade_in_plates(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 rest = exe_word(s, 0x00308B1C);
    const u32 white = exe_word(s, 0x00314F48);
    count_down(w.bytes, field::plate_fade_timer, frame_step(s));
    const float duration = exe_real(s, 0x00429014);
    float fraction;
    if (duration == 0.f)
    {
        fraction = exe_real(s, 0x0032B0F8);
    }
    else
    {
        fraction = (get(w.bytes, field::plate_fade_timer) - duration) / (0.f - duration);
        const float one = exe_real(s, 0x0032B0F8);
        if (fraction > one)
        {
            fraction = one;
        }
        else if (0.f > fraction)
        {
            fraction = 0.f;
        }
    }
    const u32 plate = with_alpha(white, truncate_byte(long double(fraction) * exe_real(s, 0x0032B158)));
    const u32 box = with_alpha(rest, truncate_byte(long double(fraction) * exe_real(s, 0x0032B368)));
    for (size_t index = 0; index < 3; ++index)
    {
        set(record(plate_bytes(w, index), plate_field::plate_sprite), sprite::color, plate);
        set(plate_bytes(w, index), plate_field::box_color, box);
    }
    const u32 selected = exe_word(s, 0x00308B18);
    const auto chosen = chosen_plate(w);
    set(chosen, plate_field::box_color, selected);
    field_byte(chosen, plate_field::box_color, 3) = truncate_byte(long double(fraction) * exe_real(s, 0x0032B378));
    double limit;
    std::memcpy(&limit, s.image.memory(0x0032B370, 8).data(), 8);
    if (!(double(fraction) < limit))
    {
        set(w.bytes, field::plates_fading, 0);
        for (size_t index = 0; index < 3; ++index)
        {
            set(record(plate_bytes(w, index), plate_field::plate_sprite), sprite::color, white);
            set(plate_bytes(w, index), plate_field::box_color, exe_word(s, 0x00308B1C));
        }
        set(chosen, plate_field::box_color, selected);
    }
}

// Xbox 0x000DE990, ESI widget: the cursor bat, made once.
bool create_slot_bat(SlotSelectWidget& w, GameSelectControlServices& s)
{
    if (w.bat == nullptr)
    {
        const std::array<float, 2> position{0.f, 0.f};
        w.bat = s.create_bat(position, exe_real(s, 0x00428F90), exe_real(s, 0x00428F94)); // 0x0013B580
        if (w.bat == nullptr)
        {
            return false;
        }
    }
    return true;
}

// lstrcpyW
void copy_wide(char16_t* output, const char16_t* input)
{
    while ((*output++ = *input++) != 0)
    {
    }
}

// Slot `slot`'s save name (+0xB9C + slot * 0x100).
const char16_t* slot_name(const SlotSelectWidget& w, i32 slot)
{
    return reinterpret_cast<const char16_t*>(record(w.bytes, field::slot_names, size_t(slot)).data());
}

char16_t* slot_name(SlotSelectWidget& w, i32 slot)
{
    return reinterpret_cast<char16_t*>(record(w.bytes, field::slot_names, size_t(slot)).data());
}

void put_control_word(SceneControlInstance& control, size_t offset, u32 value)
{
    std::memcpy(control.state.data() + offset, &value, 4);
}

// 0x000DF9A0's occupied-slot tail: the selected save becomes the current game (DAT_00506C90).
void select_profile_game(GameSelectControlServices& s)
{
    copy_wide(s.profile.name.data(), s.frontend.selected_name.data());
    s.profile.scene_flags->bytes[0] |= 2;
    s.profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(s); // 0x000F2F00
    load_runtime_save_progress(s.frontend.selected_name.data(), s); // 0x000F3020, EAX DAT_00429CC4
}

SaveGameContext save_context(GameSelectControlServices& s)
{
    return {s.files, s.profile, s.scene_order_held};
}

// Xbox 0x0004B4D0, EAX the script: its chapter (1..9 from the names at 0x004232F8), or 0.
i32 script_chapter(const char* script, GameSelectControlServices& s)
{
    if (std::strncmp("aid_script_ghoulies_chapter", script, 0x1B) != 0)
    {
        return 0;
    }
    for (i32 chapter = 1; chapter < 10; ++chapter)
    {
        const char* name = exe_string(s, exe_word(s, 0x004232F4 + u32(chapter) * 4));
        if (std::strncmp(name, script + 0x1B, std::strlen(name)) == 0)
        {
            return chapter;
        }
    }
    return 0;
}

// The 0x0010DB10 / 0x0004B4D0 / DAT_002FE698 row of the selected slot's save: its chapter's plate row.
i32 slot_chapter_row(const char16_t* name, GameSelectControlServices& s)
{
    char script[0x80]{};
    save_game_script(name, script); // 0x0010DB10, EBX the name
    const i32 chapter = script_chapter(script, s); // 0x0004B4D0
    return i32(exe_word(s, 0x002FE698 + u32(chapter) * 4));
}

// Xbox 0x000DF950, EBX the found slot: the slot whose save name matches.
bool find_slot_by_name(const char16_t* name, i32& slot, GameSelectControlServices& s)
{
    auto& w = *s.frontend.slot_widget;
    slot = 0;
    for (i32 index = 0; index < 3; ++index)
    {
        const char16_t* candidate = slot_name(w, index);
        if (std::u16string_view(candidate) == std::u16string_view(name)) // lstrcmpW
        {
            slot = index;
            return true;
        }
    }
    return false;
}

// Xbox 0x000DF330, ECX widget: the background cross-fade fraction from the bat's flight time.
void advance_background_fade(SlotSelectWidget& w, GameSelectControlServices& s)
{
    count_down(w.bytes, field::background_fade_timer, frame_step(s));
    const float flight = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
    const float one = exe_real(s, 0x0032B0F8);
    if (flight != 0.f)
    {
        const float fraction = (get(w.bytes, field::background_fade_timer) - flight) / (0.f - flight);
        if (one < fraction)
        {
            set(w.bytes, field::background_fade, one);
            return;
        }
        if (fraction < 0.f)
        {
            set(w.bytes, field::background_fade, 0.f);
            return;
        }
        set(w.bytes, field::background_fade, fraction);
        return;
    }
    set(w.bytes, field::background_fade, one);
}

// Xbox 0x00101730: each byte of `rest` toward `selected` by the fraction, clamped to 0..255 and truncated.
u32 blend_runtime_color(u32 selected, u32 rest, float fraction, GameSelectControlServices& s)
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

// Xbox 0x000DE540, ESI widget: the selected plate grows from its rest box as the backgrounds cross-fade.
void raise_selected_plate(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 slot = u32(chosen_slot(w));
    const auto p = plate_bytes(w, slot);
    const auto plate_sprite = record(p, plate_field::plate_sprite);
    advance_background_fade(w, s); // 0x000DF330, ECX the widget
    if (w.old_background.data() != w.background.data())
    {
        const u8 alpha = truncate_byte(long double(get(w.bytes, field::background_fade)) * exe_real(s, 0x0032B158));
        field_byte(record(w.bytes, field::background), sprite::color, 3) = alpha;
        field_byte(record(w.bytes, field::old_background), sprite::color, 3) = u8(0xFF - alpha);
    }
    const float fraction = get(w.bytes, field::background_fade);
    set(p, plate_field::box_color, blend_runtime_color(exe_word(s, 0x00308B18), exe_word(s, 0x00308B1C), fraction, s));
    const u32 row = 0x0042902C + slot * 0x124;
    const float width = screen_width(s);
    const float plate_width = texture_width(w.plates[slot].plate);
    const float rest_x = (exe_real(s, row + 0x10) * width) / plate_width;
    const float raised_x = (exe_real(s, row + 8) * width) / plate_width;
    set(plate_sprite, sprite::scale_x, (raised_x - rest_x) * fraction + rest_x);
    const float height = screen_height(s);
    const float plate_height = texture_height(w.plates[slot].plate);
    const float raised_y = (height * exe_real(s, row + 0xC)) / plate_height;
    const float rest_y = (height * exe_real(s, row + 0x14)) / plate_height;
    const float one = exe_real(s, 0x0032B0F8);
    set(plate_sprite, sprite::scale_y, (raised_y - rest_y) * fraction + rest_y);
    const float grow_x = (exe_real(s, row + 8) / exe_real(s, row + 0x10) - one) * fraction + one;
    const float grow_y = (exe_real(s, row + 0xC) / exe_real(s, row + 0x14) - one) * fraction + one;
    if (w.plates[slot].nowrite.data() != nullptr)
    {
        const auto nowrite = record(p, plate_field::nowrite_sprite);
        set(nowrite, sprite::scale_x, exe_real(s, 0x00428FEC) * grow_x);
        set(nowrite, sprite::scale_y, exe_real(s, 0x00428FF0) * grow_y);
    }
    if (w.plates[slot].broken.data() != nullptr)
    {
        const auto broken = record(p, plate_field::broken_sprite);
        set(broken, sprite::scale_x, exe_real(s, 0x00428FD8) * grow_x);
        set(broken, sprite::scale_y, exe_real(s, 0x00428FDC) * grow_y);
    }
    const float scale_x = exe_real(s, 0x0032B6B0);
    const float box_x = exe_real(s, row + 0x10) * scale_x + exe_real(s, 0x00428FBC);
    set(p, plate_field::box_width,
        ((exe_real(s, row + 8) * scale_x + exe_real(s, 0x00428FC0)) - box_x) * fraction + box_x);
    const float scale_y = exe_real(s, 0x0032B6AC);
    const float box_y = exe_real(s, row + 0x14) * scale_y + exe_real(s, 0x00428FBC);
    set(p, plate_field::box_height,
        ((exe_real(s, row + 0xC) * scale_y + exe_real(s, 0x00428FC0)) - box_y) * fraction + box_y);
    if (!(get(w.bytes, field::background_fade) < one))
    {
        set_runtime_slot_select_state(w, 5, s);
    }
}

// Xbox 0x000DE7B0, ESI widget: the selected plate zooms to fill the screen over DAT_00429008 seconds.
void zoom_selected_plate(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 slot = u32(chosen_slot(w));
    const auto p = plate_bytes(w, slot);
    const auto plate_sprite = record(p, plate_field::plate_sprite);
    const u32 row = 0x0042902C + slot * 0x124;
    const float rest_box_x = exe_real(s, row + 8) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC);
    const float rest_box_y = exe_real(s, row + 0xC) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC);
    count_down(w.bytes, field::zoom_timer, frame_step(s));
    const float one = exe_real(s, 0x0032B0F8);
    const float duration = exe_real(s, 0x00429008);
    float fraction = one;
    if (duration != 0.f)
    {
        fraction = (get(w.bytes, field::zoom_timer) - duration) / (0.f - duration);
        if (fraction > one)
        {
            fraction = one;
        }
        else if (0.f > fraction)
        {
            fraction = 0.f;
        }
    }
    const bool done = !(fraction < one);
    const float margin_start = exe_real(s, 0x00428FC0);
    const float margin = (exe_real(s, 0x00428FC4) - margin_start) * fraction + margin_start;
    set(p, plate_field::box_width, ((screen_width(s) + margin) - rest_box_x) * fraction + rest_box_x);
    set(p, plate_field::box_height, ((screen_height(s) + margin) - rest_box_y) * fraction + rest_box_y);
    const float x = exe_real(s, row);
    set(plate_sprite, sprite::x, (s.hud.center_x - x) * fraction + x);
    const float y = exe_real(s, row + 4);
    set(plate_sprite, sprite::y, (s.hud.center_y - y) * fraction + y);
    const float scale_x = exe_real(s, row + 8);
    set(plate_sprite, sprite::scale_x, (one - scale_x) * fraction + scale_x);
    const float scale_y = exe_real(s, row + 0xC);
    set(plate_sprite, sprite::scale_y, (one - scale_y) * fraction + scale_y);
    if (done)
    {
        set_runtime_slot_select_state(w, 8, s);
    }
}

// Xbox 0x000DE9E0, EDI widget: the selected slot's movie and the bat's flight to it.
bool select_slot_movie(SlotSelectWidget& w, GameSelectControlServices& s)
{
    if (get(w.bytes, field::flown_row) == get(w.bytes, field::cursor_row)
        && get(w.bytes, field::flown_slot) == u32(get(w.bytes, field::slot)))
    {
        return true;
    }
    const u32 slot = u32(chosen_slot(w));
    const u32 row = 0x0042902C + slot * 0x124;
    set(w.bytes, field::bat_target_x, exe_real(s, row));
    set(w.bytes, field::bat_target_y, exe_real(s, row + 0x18) + exe_real(s, row + 4));
    if (w.movie != nullptr)
    {
        --w.movie->play_count;
    }
    const i32 status = plate_status(w, slot);
    RuntimeMovieState* movie;
    if (status_has_save(status))
    {
        const i32 row = slot_chapter_row(slot_name(w, i32(slot)), s);
        movie = s.frontend.movies.at(size_t(2 + row)); // DAT_005065A0 + row * 4
    }
    else
    {
        movie = s.frontend.movies[slot];
    }
    w.movie = movie;
    ++movie->play_count;
    set(w.bytes, field::movie_held, 1);
    set(w.bytes, field::movie_frames, 0);
    if (w.bat != nullptr)
    {
        const ButtonBatPoint target{get(w.bytes, field::bat_target_x), get(w.bytes, field::bat_target_y)};
        s.fly_bat(*w.bat, target, exe_real(s, 0x00428F90)); // 0x0006DA00, EBX +0xB20, ESI the bat
        set(w.bytes, field::flown_slot, u32(get(w.bytes, field::slot)));
        set(w.bytes, field::flown_row, get(w.bytes, field::cursor_row));
        return true;
    }
    return false;
}

// Xbox 0x000DF140: the background of the selected slot, the old one kept for the cross-fade.
void select_slot_background(SlotSelectWidget& w, GameSelectControlServices& s)
{
    if (w.old_background.data() != nullptr)
    {
        release_texture(w.old_background, s);
    }
    const auto background = record(w.bytes, field::background);
    const auto old_background = record(w.bytes, field::old_background);
    set(old_background, sprite::color, get(background, sprite::color));
    w.old_background = w.background;
    set(old_background, sprite::scale_x, get(background, sprite::scale_x));
    set(old_background, sprite::scale_y, get(background, sprite::scale_y));
    set(old_background, sprite::x, get(background, sprite::x));
    set(old_background, sprite::y, get(background, sprite::y));
    w.background = {};
    const i32 status = plate_status(w, size_t(chosen_slot(w)));
    std::span<const u8> texture;
    if (status_has_save(status))
    {
        const i32 row = slot_chapter_row(slot_name(w, chosen_slot(w)), s);
        texture = acquire_texture(exe_string(s, 0x00429290 + u32(row) * 0x10C), s);
    }
    else
    {
        char name[0x80]{};
        std::memcpy(name, "aid_texture_ghoulies_gameselect_emptyslotbackground", 53);
        texture = acquire_texture(name, s);
    }
    w.background = texture;
    if (texture.data() != nullptr && texture_has_frame(texture) && runtime_texture_format_drawable(texture))
    {
        set(background, sprite::scale_x, screen_width(s) / texture_width(texture));
        set(background, sprite::scale_y, screen_height(s) / texture_height(texture));
        if (w.bat != nullptr)
        {
            set(w.bytes, field::background_fade_timer, s.bat_screen(*w.bat).flight_duration); // Bat +0x93C
            set(w.bytes, field::background_fade, 0.f);
        }
        field_byte(background, sprite::color, 3) = 0;
        field_byte(old_background, sprite::color, 3) = 0xFF;
    }
}

// Xbox 0x000DF790: forget the saves that no longer exist, except the selected one.
void drop_missing_saves(GameSelectControlServices& s)
{
    auto& w = *s.frontend.slot_widget;
    for (i32 index = 0; index < 3; ++index)
    {
        char16_t* name = slot_name(w, index);
        if (chosen_slot(w) != index && name[0] != 0)
        {
            if (save_game_loading(name) == 0) // 0x0010DBA0, EBX the name
            {
                SaveGameContext saves = save_context(s);
                unload_save_game(name, saves); // 0x0010D9B0, EAX the name
                *name = 0;
            }
        }
    }
}

// Xbox 0x000DF880: each loaded save's name into its own slot, else into the first free one.
void fill_slots_from_saves(GameSelectControlServices& s)
{
    auto& w = *s.frontend.slot_widget;
    std::array<char16_t, 0x80> name{};
    save_first_slot_name(name.data()); // 0x0010D710
    i32 filled = 0;
    while (name[0] != 0 && filled < 3)
    {
        i32 found = 0;
        if (!find_slot_by_name(name.data(), found, s)) // 0x000DF950
        {
            const u8 slot = save_game_slot(name.data()); // 0x0010DB60
            char16_t* target = slot_name(w, slot);
            if (target[0] == 0)
            {
                std::copy(name.begin(), std::find(name.begin(), name.end(), u'\0') + 1, target); // lstrcpyW
                save_next_slot_name(name.data()); // 0x0010D7A0
                continue;
            }
            filled = 0;
            while (filled < 3 && slot_name(w, filled)[0] != 0)
            {
                ++filled;
            }
            if (filled < 3)
            {
                char16_t* free = slot_name(w, filled);
                std::copy(name.begin(), std::find(name.begin(), name.end(), u'\0') + 1, free);
            }
        }
        save_next_slot_name(name.data()); // 0x0010D7A0
    }
}

// Xbox 0x000DF7E0: the slots' save names, kept from DAT_00429DC4 where the save still checks out.
void load_slot_names(GameSelectControlServices& s)
{
    auto& w = *s.frontend.slot_widget;
    for (size_t index = 0; index < 3; ++index)
    {
        std::array<char16_t, 0x80> name{};
        const auto& kept = s.frontend.slot_names[index];
        std::copy(kept.begin(), std::find(kept.begin(), kept.end(), u'\0'), name.begin()); // lstrcpyW
        if (name[0] != 0 && save_game_name_exists(name.data())) // 0x0010F530
        {
            char16_t* target = slot_name(w, i32(index));
            std::copy(name.begin(), std::find(name.begin(), name.end(), u'\0') + 1, target); // lstrcpyW
        }
    }
    fill_slots_from_saves(s); // 0x000DF880
    for (size_t index = 0; index < 3; ++index)
    {
        const char16_t* name = slot_name(w, i32(index));
        auto& kept = s.frontend.slot_names[index];
        kept.fill(0);
        std::copy(name, name + std::u16string_view(name).size(), kept.begin()); // lstrcpyW
    }
}

// 0x0010D910, EBX the name, with the callbacks 0x000E0640 and 0x000DF6B0: load a save that is not loaded yet.
void load_save_slot(const char16_t* name, GameSelectControlServices& s)
{
    SaveGameContext saves = save_context(s);
    load_save_game(name,
        [&s](const void*)
        {
            // Xbox 0x000E0640: one load fewer outstanding.
            auto& bytes = s.frontend.slot_widget->bytes;
            set(bytes, field::loads_pending, get(bytes, field::loads_pending) - 1);
        },
        [](const void*)
        {
            // Xbox 0x000DF6B0: apperror slotselection_corruptgamemessage on the app error screen.
            throw Error("0x00032990 (the app error screen, apperror slotselection_corruptgamemessage) is not "
                        "translated");
        },
        saves); // 0x0010D910, EBX the name
}

// Xbox 0x000DF5D0: start the save enumeration.
void begin_save_scan(GameSelectControlServices& s)
{
    std::array<char16_t, 0x80> name{};
    save_first_name(name.data()); // 0x0010D810
    auto& w = *s.frontend.slot_widget;
    if (name[0] != 0)
    {
        if (!save_slot_loaded(name.data())) // 0x0010F620, EBX the name
        {
            set(w.bytes, field::loads_pending, get(w.bytes, field::loads_pending) + 1);
            load_save_slot(name.data(), s);
        }
    }
    set(w.bytes, field::scan_steps, get(w.bytes, field::scan_steps) + 1);
}

// Xbox 0x000DF650: the next save-scan step.
void continue_save_scan(GameSelectControlServices& s)
{
    std::array<char16_t, 0x80> name{};
    save_next_name(name.data()); // 0x0010D8A0, EBX the name
    auto& w = *s.frontend.slot_widget;
    if (name[0] != 0 && !save_slot_loaded(name.data()))
    {
        set(w.bytes, field::loads_pending, get(w.bytes, field::loads_pending) + 1);
        load_save_slot(name.data(), s);
    }
    set(w.bytes, field::scan_steps, get(w.bytes, field::scan_steps) + 1);
}

// The plate texture of a slot replaced, keeping its drawn size (0x000DEB20's scale update).
void replace_plate_texture(SlotSelectWidget& w, size_t index, const char* name, GameSelectControlServices& s)
{
    auto& plate = w.plates[index];
    const auto plate_sprite = record(plate_bytes(w, index), plate_field::plate_sprite);
    const float old_width = texture_width(plate.plate);
    const float old_height = texture_height(plate.plate);
    const float scale_x = get(plate_sprite, sprite::scale_x);
    const float scale_y = get(plate_sprite, sprite::scale_y);
    if (plate.plate.data() != nullptr)
    {
        release_texture(plate.plate, s);
    }
    plate.plate = acquire_texture(name, s);
    set(plate_sprite, sprite::scale_x, (scale_x * old_width) / texture_width(plate.plate));
    set(plate_sprite, sprite::scale_y, (scale_y * old_height) / texture_height(plate.plate));
}

void copy_label(SlotSelectWidget& w, size_t index, const char16_t* text)
{
    char16_t* label = field_data(record(plate_bytes(w, index), plate_field::label), text_line::text);
    std::u16string_view copied(text);
    std::copy(copied.begin(), copied.end(), label);
    label[copied.size()] = 0;
}

void copy_movie_name(SlotSelectWidget& w, size_t index, const char* name)
{
    std::strcpy(field_data(plate_bytes(w, index), plate_field::movie_name), name);
}

// An overlay sprite for the plate (the no-write and broken discs): its texture, scale, position offset from the
// plate sprite's, and color.
template <typename Sprite>
void set_plate_overlay(SlotSelectWidget& w, size_t index, std::span<const u8>& texture, const char* name,
    Sprite overlay, u32 size_row, u32 offset_row, u32 alpha_row, GameSelectControlServices& s)
{
    texture = acquire_animation_resource(s.resources, name); // 0x00035E90
    if (texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700(2)
    }
    const auto p = plate_bytes(w, index);
    const auto plate_sprite = record(p, plate_field::plate_sprite);
    const auto sprite_bytes = record(p, overlay);
    set(sprite_bytes, sprite::scale_x, exe_real(s, size_row));
    set(sprite_bytes, sprite::scale_y, exe_real(s, size_row + 4));
    set(sprite_bytes, sprite::x, get(plate_sprite, sprite::x) + exe_real(s, offset_row));
    set(sprite_bytes, sprite::y, get(plate_sprite, sprite::y) + exe_real(s, offset_row + 4));
    set(sprite_bytes, sprite::color, exe_word(s, 0x00314F48));
    field_byte(sprite_bytes, sprite::color, 3) = s.image.memory(alpha_row, 1)[0];
}

// Xbox 0x000DEB20, EAX widget: each slot's status, name, plate and movie from its save.
void describe_slots(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (size_t index = 0; index < 3; ++index)
    {
        const char16_t* name = slot_name(w, i32(index));
        const auto p = plate_bytes(w, index);
        i32 status;
        if (name[0] == 0)
        {
            status = 1;
        }
        else if (save_game_loading(name) != 0) // 0x0010DBA0
        {
            status = 4;
        }
        else if (save_game_writing(name) != 0) // 0x0010DBE0
        {
            status = 3;
        }
        else
        {
            status = 2;
        }
        set(p, plate_field::status, status);
        const u32 row = 0x00429048 + u32(index) * 0x124;
        switch (status)
        {
        case 1:
        {
            // The empty-slot name, plate and movie.
            const char16_t* text = s.lookup_text("aid_loctext_ghoulies_savedgame", "supportsavedgame",
                "emptyslotname");
            copy_label(w, index, text == nullptr ? u"ERROR" : text);
            replace_plate_texture(w, index, exe_string(s, row), s);
            copy_movie_name(w, index, exe_string(s, row + 0x80));
            set(p, plate_field::movie_uv, {exe_real(s, row + 0x100), exe_real(s, row + 0x104)});
            break;
        }
        case 2:
        case 4:
        {
            // The save's chapter plate and movie (rows of 0x10C from 0x00429290), and its name.
            const u32 chapter = 0x00429290 + u32(slot_chapter_row(name, s)) * 0x10C;
            replace_plate_texture(w, index, exe_string(s, chapter), s);
            copy_movie_name(w, index, exe_string(s, chapter + 0x80));
            set(p, plate_field::movie_uv, {exe_real(s, chapter + 0x100), exe_real(s, chapter + 0x104)});
            copy_label(w, index, name);
            if (status == 4)
            {
                set_plate_overlay(w, index, w.plates[index].nowrite, "aid_texture_ghoulies_gameselect_disc_nowrite",
                    plate_field::nowrite_sprite, 0x00428FEC, 0x00428FF4, 0x00428FFC, s);
            }
            break;
        }
        case 3:
        {
            // A damaged save: its name, plate, movie and the broken disc.
            const char16_t* text = s.lookup_text("aid_loctext_ghoulies_savedgame", "supportsavedgame",
                "damagedgamename");
            copy_label(w, index, text == nullptr ? u"ERROR" : text);
            replace_plate_texture(w, index, exe_string(s, 0x0042939C), s);
            copy_movie_name(w, index, exe_string(s, 0x0042941C));
            set(p, plate_field::movie_uv, {exe_real(s, 0x0042949C), exe_real(s, 0x004294A0)});
            set_plate_overlay(w, index, w.plates[index].broken, "aid_texture_ghoulies_gameselect_disc_broken",
                plate_field::broken_sprite, 0x00428FD8, 0x00428FE0, 0x00428FE8, s);
            break;
        }
        default:
            break;
        }
    }
}
}

// Xbox 0x000DD730
GameSelectWidgetHandle create_runtime_slot_select(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<SlotSelectWidget>(); // 0x000F72D0(0xEAC)
}

// Xbox 0x000DD740
void open_runtime_slot_select(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = slots(widget);
    char background[0x80]{};
    std::memcpy(background, "aid_texture_ghoulies_gameselect_emptyslotbackground", 53);
    s.frontend.slot_widget = &w; // DAT_005065B8
    open_runtime_gameselect_movies(s);
    w.owner = &control;
    w.owner_state = owner;
    set(w.bytes, field::state, 0);
    set(w.bytes, field::loads_pending, 0);
    set(w.bytes, field::scan_steps, 0);
    set(w.bytes, field::fade_out_timer, 0.f);
    w.prompt_pool = nullptr;
    w.plate_pool = nullptr;
    const auto prompt = record(w.bytes, field::prompt);
    set(prompt, text_line::record, 0);
    set(prompt, text_line::x, 0.f);
    set(prompt, text_line::y, 0.f);
    field_byte(prompt, text_line::text, 0) = 0;
    field_byte(prompt, text_line::text, 1) = 0;
    s.frontend.word_0042a0c8 = 0;
    s.frontend.word_0042a0c4 = 0;
    s.next_row = 1;
    s.selected_game[0] = 0; // lstrcpyW(DAT_00429BC4, 0x0030C9D0), an empty string.
    clear_runtime_profile_block(s.profile.progress.bytes);
    clear_runtime_profile_block(s.profile.unlocks.bytes);
    w.movie = nullptr;
    set(w.bytes, field::movie_frames, 0);
    set(w.bytes, field::movie_held, 0);
    set(w.bytes, field::zoom_timer, 0.f);
    set(w.bytes, field::bat_target_x, 0.f);
    set(w.bytes, field::bat_target_y, 0.f);
    set(w.bytes, field::cursor_row, exe_word(s, 0x00428F9C));
    set(w.bytes, field::slot, i32(exe_word(s, 0x00428FA0)));
    set(w.bytes, field::flown_row, 0xBADBADBAu);
    set(w.bytes, field::flown_slot, 0xBADBADBAu);
    w.bat = nullptr;
    for (size_t index = 0; index < 3; ++index)
    {
        const auto name = record(w.bytes, field::slot_names, index);
        name[0] = 0;
        name[1] = 0;
    }
    const u32 white = exe_word(s, 0x00314F48);
    const auto background_sprite = record(w.bytes, field::background);
    const auto old_background_sprite = record(w.bytes, field::old_background);
    w.background = acquire_texture(background, s);
    set(background_sprite, sprite::color, white);
    set(background_sprite, sprite::x, s.hud.center_x);
    set(background_sprite, sprite::y, s.hud.center_y);
    set(background_sprite, sprite::scale_y, screen_height(s) / texture_height(w.background));
    set(background_sprite, sprite::scale_x, screen_width(s) / texture_width(w.background));
    w.old_background = {};
    set(old_background_sprite, sprite::color, white);
    set(old_background_sprite, sprite::x, s.hud.center_x);
    set(old_background_sprite, sprite::y, s.hud.center_y);
    set(old_background_sprite, sprite::scale_x, get(background_sprite, sprite::scale_x));
    set(old_background_sprite, sprite::scale_y, get(background_sprite, sprite::scale_y));
    set(w.bytes, field::background_fade_timer, 0.f);
    set(w.bytes, field::background_fade, 0.f);
    for (size_t index = 0; index < 3; ++index)
    {
        const auto p = plate_bytes(w, index);
        const auto plate_sprite = record(p, plate_field::plate_sprite);
        const auto disc_sprite = record(p, plate_field::disc_sprite);
        const auto nowrite_sprite = record(p, plate_field::nowrite_sprite);
        const auto broken_sprite = record(p, plate_field::broken_sprite);
        const u32 row = 0x0042902C + u32(index) * 0x124; // Plate table, stride 0x124: x, y, ..., texture at +0x1C.
        const u32 movie = 0x0042941C + u32(index) * 0x10C; // Movie names, stride 0x10C.
        auto& plate = w.plates[index];
        set(p, plate_field::status, 0);
        const char16_t error[]{u'e', u'r', u'r', u'o', u'r', 0};
        const auto label = record(p, plate_field::label);
        std::memcpy(field_data(label, text_line::text), error, sizeof(error));
        set(label, text_line::x, s.hud.center_x + exe_real(s, 0x00428FA8));
        set(label, text_line::y, s.hud.center_y + exe_real(s, 0x00428FAC));
        plate.plate = acquire_texture(exe_string(s, row + 0x1C), s);
        set(plate_sprite, sprite::x, exe_real(s, row));
        set(plate_sprite, sprite::y, exe_real(s, row + 4));
        set(plate_sprite, sprite::scale_x, (screen_width(s) * exe_real(s, row + 0x10)) / texture_width(plate.plate));
        set(plate_sprite, sprite::scale_y,
            (screen_height(s) * exe_real(s, row + 0x14)) / texture_height(plate.plate));
        set(plate_sprite, sprite::color, white);
        plate.disc = {};
        plate.disc = acquire_animation_resource(s.resources, "aid_texture_ghoulies_gameselect_disc_discspin");
        if (plate.disc.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        set(disc_sprite, sprite::x, exe_real(s, row) + exe_real(s, 0x00428FD0));
        set(disc_sprite, sprite::y, exe_real(s, row + 4) + exe_real(s, 0x00428FD4));
        set(disc_sprite, sprite::scale_x, exe_real(s, 0x00428FC8));
        set(disc_sprite, sprite::scale_y, exe_real(s, 0x00428FCC));
        set(disc_sprite, sprite::color, white);
        set(nowrite_sprite, sprite::color, white);
        set(nowrite_sprite, sprite::x, 0.f);
        set(nowrite_sprite, sprite::y, 0.f);
        set(nowrite_sprite, sprite::scale_x, 0.f);
        set(nowrite_sprite, sprite::scale_y, 0.f);
        plate.nowrite = {};
        set(broken_sprite, sprite::color, white);
        set(broken_sprite, sprite::x, 0.f);
        set(broken_sprite, sprite::y, 0.f);
        set(broken_sprite, sprite::scale_x, 0.f);
        set(broken_sprite, sprite::scale_y, 0.f);
        plate.broken = {};
        copy_movie_name(w, index, exe_string(s, movie));
        set(p, plate_field::movie_uv, {exe_real(s, movie + 0x80), exe_real(s, movie + 0x84)});
        set(p, plate_field::box_color, exe_word(s, 0x00308B1C));
        set(p, plate_field::box_width,
            exe_real(s, row + 0x10) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC));
        set(p, plate_field::box_height,
            exe_real(s, row + 0x14) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC));
    }
    set(w.bytes, field::saved_plate_flag_0, s.plate_flags[0]);
    set(w.bytes, field::saved_plate_flag_1, s.plate_flags[1]);
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    w.prompts = nullptr;
    set_runtime_slot_select_state(w, 4, s);
    allocate_dialogue_text_pool(w.plate_pool, 0, 1, 0x80, s.text_pools);
    w.plate_pool->origin = {exe_real(s, 0x0042900C), exe_real(s, 0x00429010), std::bit_cast<float>(3u)};
}

// Xbox 0x000DDC90
void close_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = slots(widget);
    release_plate_textures(w, s);
    release_disc_textures(w, s);
    release_broken_textures(w, s);
    release_nowrite_textures(w, s);
    if (w.bat != nullptr)
    {
        activate_actor_bind_tree(*w.bat); // 0x00112CC0, ESI the bat: message 0x80 retires it.
        w.bat = nullptr;
    }
    const u32 cleared = exe_word(s, 0x00314F30);
    if (w.background.data() != nullptr)
    {
        release_texture(w.background, s);
        set(record(w.bytes, field::background), sprite::color, cleared);
    }
    if (w.old_background.data() != nullptr)
    {
        release_texture(w.old_background, s);
        set(record(w.bytes, field::old_background), sprite::color, cleared);
    }
    s.plate_flags[0] = get(w.bytes, field::saved_plate_flag_0);
    s.plate_flags[1] = get(w.bytes, field::saved_plate_flag_1);
    for (auto& movie : s.frontend.movies)
    {
        if (movie != nullptr)
        {
            release_runtime_movie(*movie, s.movies); // 0x001072B0
            movie = nullptr;
        }
    }
    if (w.plate_pool != nullptr)
    {
        release_dialogue_text_pool(w.plate_pool, *s.text_pools); // 0x0005F460
        w.plate_pool = nullptr;
    }
    if (w.prompt_pool != nullptr)
    {
        release_dialogue_text_pool(w.prompt_pool, *s.text_pools);
        w.prompt_pool = nullptr;
    }
    if (w.prompts != nullptr)
    {
        destroy_dialogue_widget(w.prompts, s.resources, *s.text_pools); // 0x00142070
        w.prompts = nullptr;
    }
}

// Xbox 0x000DDD80
void tick_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = slots(widget);
    const u32 state = u32(get(w.bytes, field::state)); // unsigned: the jump table bound
    if (state <= 8 && w.prompts != nullptr)
    {
        // State 5 updates the prompts with the front-end device (DAT_00429BBC); the others without one (the jump
        // table at 0x000DDEF0 with its index bytes at 0x000DDEF8).
        const bool device = state == 5;
        s.update_prompts(*w.prompts, device); // 0x00142460, EDI the widget
    }
    switch (get(w.bytes, field::state))
    {
    case 1:
        if (get(w.bytes, field::loads_pending) == 0)
        {
            if (get(w.bytes, field::scan_steps) < 3)
            {
                continue_save_scan(s);
                return;
            }
            set_runtime_slot_select_state(w, 2, s);
        }
        return;
    case 2:
    {
        if (s.timer_expired(field_data(w.bytes, field::fade_out_timer)) != 0) // 0x0012B2A0, EBX +0xB90
        {
            set_runtime_slot_select_state(w, 3, s);
            return;
        }
        for (size_t index = 0; index < 3; ++index)
        {
            const float duration = exe_real(s, 0x00429028);
            float alpha;
            if (duration == 0.f)
            {
                alpha = 0.f;
            }
            else
            {
                const float full = exe_real(s, 0x0032B158);
                const float value = full - ((get(w.bytes, field::fade_out_timer) - duration) / (0.f - duration)) * full;
                if (0.f > value)
                {
                    alpha = 0.f;
                }
                else if (value > full)
                {
                    alpha = full;
                }
                else
                {
                    alpha = value;
                }
            }
            field_byte(record(plate_bytes(w, index), plate_field::disc_sprite), sprite::color, 3) =
                truncate_byte(alpha);
        }
        return;
    }
    case 4:
        if (get(w.bytes, field::plates_fading) == 0)
        {
            if (!runtime_gameselect_book_busy(w.owner_state, *w.owner, s)) // 0x00083C40, ECX the owner
            {
                set_runtime_slot_select_state(w, 1, s);
            }
        }
        else
        {
            fade_in_plates(w, s);
        }
        for (size_t index = 0; index < 3; ++index)
        {
            // The disc fades with its plate.
            const auto p = plate_bytes(w, index);
            field_byte(record(p, plate_field::disc_sprite), sprite::color, 3) =
                field_byte(record(p, plate_field::plate_sprite), sprite::color, 3);
        }
        return;
    case 6:
        raise_selected_plate(w, s);
        return;
    case 7:
        zoom_selected_plate(w, s);
        return;
    default:
        return;
    }
}

// Xbox 0x000DDF20
void draw_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = slots(widget);
    const i32 chosen = chosen_slot(w);
    if ((s.flags & 0x40000u) != 0)
    {
        return;
    }
    if (w.movie != nullptr)
    {
        if ((s.flags & 0x80u) != 0)
        {
            if (get(w.bytes, field::movie_held) != 0)
            {
                --w.movie->play_count;
                set(w.bytes, field::movie_held, 0);
            }
        }
        else if (get(w.bytes, field::movie_held) == 0)
        {
            ++w.movie->play_count;
            set(w.bytes, field::movie_held, 1);
        }
    }
    const std::span<u8> frame = w.owner->texture;
    if (w.movie != nullptr && get(w.bytes, field::movie_frames) >= i32(exe_word(s, 0x00429000)))
    {
        // The movie boundary locks a texture header by its native address.
        const u32 header = s.textures.address_of(frame.data()) + read_u32(frame, 4) + u32(frame[0x12]) * 0x14;
        copy_runtime_movie_frame(*w.movie, reinterpret_cast<void*>(std::uintptr_t(header)),
            s.movies); // 0x00107360, ESI the movie, ECX the texture header
    }
    const auto background = record(w.bytes, field::background);
    if (w.background.data() != nullptr && texture_has_frame(w.background)
        && runtime_texture_format_drawable(w.background) && field_byte(background, sprite::color, 3) != 0)
    {
        draw_runtime_dialogue_sprite(w.background.data(), field_data(background, sprite::position),
            field_data(background, sprite::scale), 0.f, get(background, sprite::color), 0, s.draw); // 0x0005CE30
    }
    const auto old_background = record(w.bytes, field::old_background);
    if (w.old_background.data() != nullptr && texture_has_frame(w.old_background)
        && runtime_texture_format_drawable(w.old_background) && field_byte(old_background, sprite::color, 3) != 0)
    {
        draw_runtime_dialogue_sprite(w.old_background.data(), field_data(old_background, sprite::position),
            field_data(old_background, sprite::scale), 0.f, get(old_background, sprite::color), 0, s.draw);
    }
    // The unselected plates, then the selected one on top.
    for (i32 index = 0; index < 3; ++index)
    {
        if (index != chosen_slot(w) && w.plates[size_t(index)].plate.data() != nullptr)
        {
            draw_plate(w, size_t(index), s);
        }
    }
    if (w.plates[size_t(chosen_slot(w))].plate.data() != nullptr)
    {
        draw_plate(w, size_t(chosen), s);
    }
    const i32 state = get(w.bytes, field::state);
    if (0 < state && (state <= 2 || state == 4))
    {
        for (size_t index = 0; index < 3; ++index)
        {
            if (w.plates[index].disc.data() != nullptr)
            {
                draw_plate_sprite(-1, w.plates[index].disc,
                    record(plate_bytes(w, index), plate_field::disc_sprite), s);
            }
        }
    }
    if (w.movie != nullptr && w.movie->decoder != nullptr)
    {
        const i32 shown = get(w.bytes, field::movie_frames);
        const i32 delay = i32(exe_word(s, 0x00429000));
        if (shown > delay)
        {
            const float start = float(delay);
            const float end = float(i32(exe_word(s, 0x00429004)) + delay);
            float alpha;
            if (end == start)
            {
                alpha = exe_real(s, 0x0032B158);
            }
            else
            {
                const float full = exe_real(s, 0x0032B158);
                alpha = ((float(shown) - start) / (end - start)) * full;
                if (alpha > full)
                {
                    alpha = full;
                }
                else if (0.f > alpha)
                {
                    alpha = 0.f;
                }
            }
            const u32 color = with_alpha(exe_word(s, 0x00314F48), truncate_byte(alpha));
            const float uv_top[2]{exe_real(s, 0x0032B1F4), exe_real(s, 0x0032B1B0)};
            // The movie frame over the plate the frame was chosen for (`chosen`, read on entry), scaled by the
            // selected plate's (+0xB2C now).
            const auto q = plate_bytes(w, size_t(chosen));
            const auto r = record(chosen_plate(w), plate_field::plate_sprite);
            const auto plate = w.plates[size_t(chosen)].plate;
            const float scale[2]{(texture_width(plate) * get(r, sprite::scale_x)) / texture_width(frame),
                (texture_height(plate) * get(r, sprite::scale_y)) / texture_height(frame)};
            draw_sprite(0, uv_top, frame, scale, field_data(q, plate_field::movie_uv), color,
                field_data(record(q, plate_field::plate_sprite), sprite::position), s);
        }
        set(w.bytes, field::movie_frames, get(w.bytes, field::movie_frames) + 1);
    }
    for (size_t index = 0; index < 3; ++index)
    {
        const auto p = plate_bytes(w, index);
        if (w.plates[index].broken.data() != nullptr)
        {
            draw_plate_sprite(0, w.plates[index].broken, record(p, plate_field::broken_sprite), s);
        }
        if (w.plates[index].nowrite.data() != nullptr)
        {
            draw_plate_sprite(0, w.plates[index].nowrite, record(p, plate_field::nowrite_sprite), s);
        }
    }
    if (get(w.bytes, field::plates_fading) == 0 && w.bat != nullptr)
    {
        const float scale = exe_real(s, 0x00428F90);
        if (w.bat->scale_200 != scale)
        {
            w.bat->scale_200 = scale;
            w.bat->xform_valid = 0;
        }
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        s.draw_actor(*w.bat, view, 3); // Vtable +0x14
    }
    if (w.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*w.prompts, 0, s.outer); // 0x00142720
    }
}

// Xbox 0x000DF9A0
void set_runtime_slot_select_state(SlotSelectWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    const i32 previous = get(w.bytes, field::state);
    if (previous == 3)
    {
        if (get(w.bytes, field::plates_fading) != 0)
        {
            set(w.bytes, field::plates_fading, 0);
            reset_plate_colors(w, s);
        }
        create_slot_bat(w, s);
        char resource[0x80]{};
        std::memcpy(resource, "aid_loctext_ghoulies_gameselect_gameselecttext", 47); // 0x0020A574
        const float edge = exe_real(s, 0x0032B1F4);
        const std::array<float, 2> anchor{s.hud.inner_left + edge, s.hud.inner_bottom - edge};
        const float icon = exe_real(s, 0x00429020);
        const float text = exe_real(s, 0x00429024);
        const float spacing = exe_real(s, 0x00429018);
        const std::array<float, 2> row_step{icon * 0.f, 0.f - icon * spacing};
        const float half = spacing * exe_real(s, 0x0032B0F4) * icon;
        const std::array<float, 2> origin{half + exe_real(s, 0x0042901C), 0.f - half};
        std::array<char16_t, 0x200> scratch{};
        float widest = 0.f;
        const float width = measure_dialogue_widget_width(resource, s.image.memory(0x00308068, 0x310).data(), icon,
            text, "gameselect", scratch.data(), s.widgets); // 0x001428F0, ECX the prompt rows
        if (width > exe_real(s, 0x0032B100))
        {
            widest = width;
        }
        for (const u32 rows : {0x00308378u, 0x00308688u})
        {
            const float next = measure_dialogue_widget_width(resource, s.image.memory(rows, 0x310).data(), icon,
                text, "gameselect", scratch.data(), s.widgets);
            if (next > widest)
            {
                widest = next;
            }
        }
        const std::array<float, 2> column{widest, 0.f};
        create_resource_dialogue_widget(w.prompts, 1, nullptr, anchor, row_step, origin, &column, icon, text,
            "gameselect", resource, 0x400C0, s.widget_allocation); // 0x00141DA0, ESI +0xEA8
        auto& prompts = *w.prompts;
        const float fade = exe_real(s, 0x0032B1AC);
        prompts.duration = fade;
        if (prompts.progress > fade)
        {
            prompts.progress = fade;
        }
        prompts.pool->end_time = fade;
        prompts.pool->origin = {exe_real(s, 0x0042900C), exe_real(s, 0x00429010), std::bit_cast<float>(3u)};
        allocate_dialogue_text_pool(w.prompt_pool, 0, 1, 0x40080, s.text_pools); // 0x0005F390, ESI +0x210
        w.prompt_pool->origin = {exe_real(s, 0x0042900C), exe_real(s, 0x00429010), std::bit_cast<float>(3u)};
        const auto prompt = record(w.bytes, field::prompt);
        set(prompt, text_line::x, s.hud.center_x);
        set(prompt, text_line::y, s.hud.center_y);
        set(prompt, text_line::x, get(prompt, text_line::x) + exe_real(s, 0x00428FB4));
        set(prompt, text_line::y, exe_real(s, 0x00428FB8) + get(prompt, text_line::y));
        if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1), the read check boundary.
        {
            lookup_borrowed_localtext(s.text, "gameselect", exe_string(s, 0x00324F60),
                field_data(prompt, text_line::text), s.localtext); // 0x000FF120, EBX +4, EDI the key
        }
        set(prompt, text_line::record, create_dialogue_text_record(*w.prompt_pool,
            field_data(prompt, text_line::text), get(prompt, text_line::x), get(prompt, text_line::y),
            exe_real(s, 0x00428FB0), exe_real(s, 0x00428FB0), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random)); // 0x0005F5C0, EAX 1
    }
    else if (previous == 6)
    {
        const auto label = record(chosen_plate(w), plate_field::label);
        const u32 id = create_dialogue_text_record(*w.plate_pool, field_data(label, text_line::text),
            get(label, text_line::x), get(label, text_line::y), exe_real(s, 0x00428FA4),
            exe_real(s, 0x00428FA4), 1.f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id,
            s.random); // 0x0005F5C0, EAX 1
        set(label, text_line::record, id);
    }
    else if (previous == 7)
    {
        if (w.movie != nullptr)
        {
            --w.movie->play_count;
        }
        copy_wide(s.frontend.selected_name.data(), slot_name(w, chosen_slot(w)));
        if (s.frontend.selected_name[0] != 0)
        {
            select_profile_game(s);
        }
        drop_missing_saves(s); // 0x000DF790
        SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170, EDX 0x9A
        put_control_word(*control, 0x94, 3);
        put_control_word(*control, 0x124, 2);
        control->gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
    }
    set(w.bytes, field::state, state);
    switch (state)
    {
    case 1:
        begin_save_scan(s);
        start_pause_ditty(s.ditty, s.streams); // 0x000E5B40
        return;
    case 2:
        set(w.bytes, field::fade_out_timer, exe_real(s, 0x00429028));
        return;
    case 3:
    {
        load_slot_names(s);
        std::array<char16_t, 0x80> name{};
        const auto& selected = s.frontend.selected_name;
        std::copy(selected.begin(), std::find(selected.begin(), selected.end(), u'\0'), name.begin());
        if (name[0] == 0)
        {
            std::copy(s.selected_game, s.selected_game + std::u16string_view(s.selected_game).size(), name.begin());
        }
        i32 found = 0;
        if (name[0] != 0 && find_slot_by_name(name.data(), found, s))
        {
            set(w.bytes, field::slot, found);
        }
        describe_slots(w, s);
        if (name[0] == 0)
        {
            // No save named: the first slot holding one.
            i32 index = 0;
            for (; index < 3; ++index)
            {
                if (status_has_save(plate_status(w, size_t(index))))
                {
                    break;
                }
            }
            if (index < 3)
            {
                set(w.bytes, field::slot, index);
            }
        }
        set_runtime_slot_select_state(w, 6, s);
        return;
    }
    case 4:
        set(w.bytes, field::plate_fade_timer, exe_real(s, 0x00429014));
        set(w.bytes, field::plates_fading, 1);
        return;
    case 6:
    {
        // Every plate back to its rest size and color.
        for (size_t index = 0; index < 3; ++index)
        {
            const auto p = plate_bytes(w, index);
            const auto plate_sprite = record(p, plate_field::plate_sprite);
            const u32 row = 0x0042902C + u32(index) * 0x124;
            const auto plate = w.plates[index].plate;
            set(plate_sprite, sprite::scale_x, (screen_width(s) * exe_real(s, row + 0x10)) / texture_width(plate));
            set(plate_sprite, sprite::scale_y, (screen_height(s) * exe_real(s, row + 0x14)) / texture_height(plate));
            set(p, plate_field::box_width,
                exe_real(s, row + 0x10) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC));
            set(p, plate_field::box_height,
                exe_real(s, row + 0x14) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC));
            set(p, plate_field::box_color, exe_word(s, 0x00308B1C));
            if (w.plates[index].nowrite.data() != nullptr)
            {
                const auto nowrite = record(p, plate_field::nowrite_sprite);
                set(nowrite, sprite::scale_x, exe_real(s, 0x00428FEC));
                set(nowrite, sprite::scale_y, exe_real(s, 0x00428FF0));
            }
            if (w.plates[index].broken.data() != nullptr)
            {
                const auto broken = record(p, plate_field::broken_sprite);
                set(broken, sprite::scale_x, exe_real(s, 0x00428FD8));
                set(broken, sprite::scale_y, exe_real(s, 0x00428FDC));
            }
        }
        select_slot_movie(w, s);
        select_slot_background(w, s);
        for (size_t index = 0; index < 3; ++index)
        {
            const auto label = record(plate_bytes(w, index), plate_field::label);
            end_dialogue_text_record(*w.plate_pool, get(label, text_line::record), s.fonts, s.fallback_texture,
                s.random); // 0x0005F7E0, ECX the record id
        }
        if (w.prompts != nullptr)
        {
            auto& prompts = *w.prompts;
            const i32 status = plate_status(w, size_t(chosen_slot(w)));
            if (!status_has_save(status))
            {
                copy_dialogue_widget_parameters(prompts.requested_parameters,
                    s.image.memory(0x00308068, 0x310).data()); // 0x00142DF0, ECX +0x638
                prompts.requested_set = prompts.requested_parameters.data();
                set_dialogue_widget_mode(prompts, 3, s.widgets); // 0x00142C20, EAX 3
                return;
            }
            const u32 rows = status == 3 ? 0x00308688u : 0x00308378u;
            set_dialogue_widget_prompt(prompts, s.image.memory(rows, 0x310).data(), s.widgets); // 0x00142180
        }
        return;
    }
    case 7:
        set(w.bytes, field::zoom_timer, exe_real(s, 0x00429008));
        end_dialogue_text_record(*w.plate_pool, get(record(chosen_plate(w), plate_field::label), text_line::record),
            s.fonts, s.fallback_texture, s.random);
        return;
    case 8:
        if (w.prompts != nullptr)
        {
            w.prompts->requested_set = nullptr;
            set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20, EAX 3
        }
        if (w.movie != nullptr)
        {
            --w.movie->play_count;
        }
        copy_wide(s.frontend.selected_name.data(), slot_name(w, chosen_slot(w)));
        drop_missing_saves(s); // 0x000DF790
        if (s.frontend.selected_name[0] != 0)
        {
            select_profile_game(s);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000E01B0
void open_runtime_gameselect_movies(GameSelectControlServices& services)
{
    auto& s = services;
    for (size_t index = 0; index < 3; ++index)
    {
        s.frontend.movies[index] = allocate_runtime_movie(exe_string(s, 0x004290C8 + u32(index) * 0x124), 0x14,
            s.movies);
    }
    for (size_t index = 0; index < 5; ++index)
    {
        s.frontend.movies[3 + index] = allocate_runtime_movie(exe_string(s, 0x0042941C + u32(index) * 0x10C), 0x14,
            s.movies);
    }
}

// Xbox 0x000E0210
void move_runtime_slot_cursor(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.slot_widget;
    if (w.bat == nullptr)
    {
        return;
    }
    const i32 flight = s.bat_screen(*w.bat).state; // Bat +0x934
    if (flight != 1 && flight != 3)
    {
        return;
    }
    const u32 row = get(w.bytes, field::cursor_row);
    i32 slot = chosen_slot(w);
    std::array<float, 2> direction{};
    s.frontend_direction(direction); // 0x001429E0, ESI DAT_00429BBC
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return;
    }
    // 0x001080B0, ESI the direction: an axis inside (DAT_0032B274, DAT_0032B1AC) is dropped; the larger axis
    // gives up (1), down (2), left (4) or right (8).
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
    u16 code = 0;
    if (direction[0] != 0.f || direction[1] != 0.f)
    {
        const float vertical = std::fabs(direction[1]); // 0x00152BB0
        const float horizontal = std::fabs(direction[0]);
        if (horizontal < vertical)
        {
            code = direction[1] > exe_real(s, 0x0032B100) ? 1 : 2;
        }
        else
        {
            code = direction[0] > exe_real(s, 0x0032B100) ? 8 : 4;
        }
    }
    if (code == 4)
    {
        slot = slot == 0 ? 2 : slot - 1;
    }
    else if (code == 8)
    {
        slot = slot == 2 ? 0 : slot + 1;
    }
    else
    {
        return;
    }
    set(w.bytes, field::cursor_row, row);
    set(w.bytes, field::slot, slot);
    set_runtime_slot_select_state(w, 6, s);
}

// Xbox 0x000E02D0
void select_runtime_slot(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.slot_widget;
    for (size_t index = 0; index < 3; ++index)
    {
        copy_wide(s.frontend.slot_names[index].data(), slot_name(w, i32(index)));
    }
    s.frontend.word_0042a0c4 = chosen_slot(w);
    s.frontend.word_0042a0c8 = 1;
    const i32 status = plate_status(w, size_t(chosen_slot(w)));
    if (status_has_save(status))
    {
        // The bat flies off toward the screen edge farther from it.
        const float width = screen_width(s);
        const float height = screen_height(s);
        const float half = exe_real(s, 0x0032B0F4);
        ButtonBatPoint target{};
        target.x = width - get(w.bytes, field::bat_target_x) > width * half ? width + exe_real(s, 0x0032B1E0)
                                                                            : exe_real(s, 0x0032B304);
        target.y = height - get(w.bytes, field::bat_target_y) > height * half ? height + exe_real(s, 0x0032B1E0)
                                                                              : exe_real(s, 0x0032B304);
        if (w.bat != nullptr)
        {
            s.fly_bat(*w.bat, target, 1.f); // 0x0006DA00, EBX the target
        }
        SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
        put_control_word(*control, 0x94, 3);
        put_control_word(*control, 0x124, 2);
        control->gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
        set_runtime_slot_select_state(w, 8, s); // 0x000DF9A0(DAT_005065B8, 8)
        return;
    }
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
    put_control_word(*control, 0x94, 4);
    put_control_word(*control, 0x124, 1);
    if (control->gameselect->book != nullptr)
    {
        release_transition_book(control->gameselect->book, s.books); // 0x00048860
        control->gameselect->book = nullptr;
    }
    set_runtime_slot_select_state(w, 8, s);
}

// Xbox 0x000E04F0
void delete_runtime_slot(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = *s.frontend.slot_widget;
    for (size_t index = 0; index < 3; ++index)
    {
        copy_wide(s.frontend.slot_names[index].data(), slot_name(w, i32(index)));
    }
    copy_wide(s.frontend.selected_name.data(), slot_name(w, chosen_slot(w)));
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
    put_control_word(*control, 0x94, 2);
    put_control_word(*control, 0x124, 1);
    if (control->gameselect->book != nullptr)
    {
        release_transition_book(control->gameselect->book, s.books); // 0x00048860
        control->gameselect->book = nullptr;
    }
    set_runtime_slot_select_state(w, 8, s);
}

// Xbox 0x000E05D0
void leave_runtime_slot_select(GameSelectControlServices& services)
{
    auto& s = services;
    char name[0x80]{};
    std::memcpy(name, "aid_script_ghoulies_gameselect_backtoattract", 45); // 0x00324F30
    std::strcpy(s.transition_override, name); // DAT_0050E458
    // 0x0011EAD0: the profile is cleared and the first scene of the held scene order is requested.
    clear_runtime_profile_block({s.profile.scene_flags->bytes, size_t(s.profile.scene_flags->size)});
    s.profile.name[0] = 0;
    s.profile.times = {};
    s.request_script(reinterpret_cast<const char*>(s.scene_order_held.data()), 1); // 0x0011EB30, EAX *DAT_00511B64
    auto& w = *s.frontend.slot_widget;
    w.prompts->requested_set = nullptr;
    set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20, EAX 3
}
}
