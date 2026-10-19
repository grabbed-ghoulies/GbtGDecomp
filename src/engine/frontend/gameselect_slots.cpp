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

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

SlotSelectWidget& slots(GameSelectWidget& widget)
{
    return static_cast<SlotSelectWidget&>(widget);
}

// The selected plate (+0xB2C).
i32 chosen_slot(const SlotSelectWidget& w)
{
    return w.slot;
}

SlotSelectWidget::Plate& chosen_plate(SlotSelectWidget& w)
{
    return w.plates[size_t(chosen_slot(w))];
}

i32 plate_status(const SlotSelectWidget& w, size_t index)
{
    return w.plates[index].status;
}

// Status 2, 3 or 4: the slot holds a save.
bool status_has_save(i32 status)
{
    return status == 2 || status == 4 || status == 3;
}

// A countdown ticked toward 0 by the frame step and clamped there (0x000DF330 / 0x000DF3E0 / 0x000DE7B0).
void count_down(float& timer, float step)
{
    float value = timer;
    if (0.f < value)
    {
        value -= step;
        timer = value;
        if (value <= 0.f)
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

// A plate's sprite drawn through draw_sprite: its texture, scale, color and position, no UVs.
void draw_plate_sprite(i32 frame, GameSelectSprite& sprite, GameSelectControlServices& s)
{
    draw_sprite(frame, nullptr, sprite.texture, sprite.scale.data(), nullptr, sprite.color, sprite.position.data(),
        s);
}

// 0x000DDF20's plate: the box panel centered on the plate, then the plate sprite.
void draw_plate(SlotSelectWidget& w, size_t index, GameSelectControlServices& s)
{
    auto& p = w.plates[index];
    auto& plate_sprite = p.plate_sprite;
    const i32 box_h = i32(p.box_height);
    const i32 box_w = i32(p.box_width);
    const float x = float(i32(plate_sprite.position[0]) - box_w / 2);
    const float y = float(i32(plate_sprite.position[1]) - box_h / 2);
    draw_runtime_dialogue_panel(x, y, float(box_w), float(box_h), exe_real(s, 0x00428F98),
        p.box_color, s.outer); // 0x00061360
    draw_plate_sprite(0, plate_sprite, s);
}

// Xbox 0x000DF080, EAX widget: the plate textures.
void release_plate_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.plate_sprite.texture.data() != nullptr)
        {
            release_texture(plate.plate_sprite.texture, s);
        }
    }
}

// Xbox 0x000DF0B0, EAX widget: the disc textures.
void release_disc_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.disc_sprite.texture.data() != nullptr)
        {
            release_texture(plate.disc_sprite.texture, s);
        }
    }
}

// Xbox 0x000DF110, EAX widget: the broken-disc textures.
void release_broken_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.broken_sprite.texture.data() != nullptr)
        {
            release_texture(plate.broken_sprite.texture, s);
        }
    }
}

// Xbox 0x000DF0E0, EAX widget: the no-write textures.
void release_nowrite_textures(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (auto& plate : w.plates)
    {
        if (plate.nowrite_sprite.texture.data() != nullptr)
        {
            release_texture(plate.nowrite_sprite.texture, s);
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
        auto& plate = w.plates[index];
        plate.plate_sprite.color = white;
        plate.box_color = rest;
    }
    chosen_plate(w).box_color = exe_word(s, 0x00308B18);
}

// Xbox 0x000DF3E0, ESI widget: the plates fade in over DAT_00429014 seconds.
void fade_in_plates(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 rest = exe_word(s, 0x00308B1C);
    const u32 white = exe_word(s, 0x00314F48);
    count_down(w.plate_fade_timer, frame_step(s));
    const float duration = exe_real(s, 0x00429014);
    float fraction;
    if (duration == 0.f)
    {
        fraction = exe_real(s, 0x0032B0F8);
    }
    else
    {
        fraction = (w.plate_fade_timer - duration) / (0.f - duration);
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
        w.plates[index].plate_sprite.color = plate;
        w.plates[index].box_color = box;
    }
    const u32 selected = exe_word(s, 0x00308B18);
    auto& chosen = chosen_plate(w);
    chosen.box_color = selected;
    set_color_alpha(chosen.box_color, truncate_byte(long double(fraction) * exe_real(s, 0x0032B378)));
    double limit;
    std::memcpy(&limit, s.image.memory(0x0032B370, 8).data(), 8);
    if (!(double(fraction) < limit))
    {
        w.plates_fading = 0;
        for (size_t index = 0; index < 3; ++index)
        {
            w.plates[index].plate_sprite.color = white;
            w.plates[index].box_color = exe_word(s, 0x00308B1C);
        }
        chosen.box_color = selected;
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
    return w.slot_names[size_t(slot)].data();
}

char16_t* slot_name(SlotSelectWidget& w, i32 slot)
{
    return w.slot_names[size_t(slot)].data();
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
    count_down(w.background_fade_timer, frame_step(s));
    const float flight = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
    const float one = exe_real(s, 0x0032B0F8);
    if (flight != 0.f)
    {
        const float fraction = (w.background_fade_timer - flight) / (0.f - flight);
        if (one < fraction)
        {
            w.background_fade = one;
            return;
        }
        if (fraction < 0.f)
        {
            w.background_fade = 0.f;
            return;
        }
        w.background_fade = fraction;
        return;
    }
    w.background_fade = one;
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
    auto& p = w.plates[slot];
    auto& plate_sprite = p.plate_sprite;
    advance_background_fade(w, s); // 0x000DF330, ECX the widget
    if (w.old_background.texture.data() != w.background.texture.data())
    {
        const u8 alpha = truncate_byte(long double(w.background_fade) * exe_real(s, 0x0032B158));
        set_color_alpha(w.background.color, alpha);
        set_color_alpha(w.old_background.color, u8(0xFF - alpha));
    }
    const float fraction = w.background_fade;
    p.box_color = blend_runtime_color(exe_word(s, 0x00308B18), exe_word(s, 0x00308B1C), fraction, s);
    const u32 row = 0x0042902C + slot * 0x124;
    const float width = screen_width(s);
    const float plate_width = texture_width(w.plates[slot].plate_sprite.texture);
    const float rest_x = (exe_real(s, row + 0x10) * width) / plate_width;
    const float raised_x = (exe_real(s, row + 8) * width) / plate_width;
    plate_sprite.scale[0] = (raised_x - rest_x) * fraction + rest_x;
    const float height = screen_height(s);
    const float plate_height = texture_height(w.plates[slot].plate_sprite.texture);
    const float raised_y = (height * exe_real(s, row + 0xC)) / plate_height;
    const float rest_y = (height * exe_real(s, row + 0x14)) / plate_height;
    const float one = exe_real(s, 0x0032B0F8);
    plate_sprite.scale[1] = (raised_y - rest_y) * fraction + rest_y;
    const float grow_x = (exe_real(s, row + 8) / exe_real(s, row + 0x10) - one) * fraction + one;
    const float grow_y = (exe_real(s, row + 0xC) / exe_real(s, row + 0x14) - one) * fraction + one;
    if (w.plates[slot].nowrite_sprite.texture.data() != nullptr)
    {
        auto& nowrite = p.nowrite_sprite;
        nowrite.scale[0] = exe_real(s, 0x00428FEC) * grow_x;
        nowrite.scale[1] = exe_real(s, 0x00428FF0) * grow_y;
    }
    if (w.plates[slot].broken_sprite.texture.data() != nullptr)
    {
        auto& broken = p.broken_sprite;
        broken.scale[0] = exe_real(s, 0x00428FD8) * grow_x;
        broken.scale[1] = exe_real(s, 0x00428FDC) * grow_y;
    }
    const float scale_x = exe_real(s, 0x0032B6B0);
    const float box_x = exe_real(s, row + 0x10) * scale_x + exe_real(s, 0x00428FBC);
    p.box_width = ((exe_real(s, row + 8) * scale_x + exe_real(s, 0x00428FC0)) - box_x) * fraction + box_x;
    const float scale_y = exe_real(s, 0x0032B6AC);
    const float box_y = exe_real(s, row + 0x14) * scale_y + exe_real(s, 0x00428FBC);
    p.box_height = ((exe_real(s, row + 0xC) * scale_y + exe_real(s, 0x00428FC0)) - box_y) * fraction + box_y;
    if (!(w.background_fade < one))
    {
        set_runtime_slot_select_state(w, 5, s);
    }
}

// Xbox 0x000DE7B0, ESI widget: the selected plate zooms to fill the screen over DAT_00429008 seconds.
void zoom_selected_plate(SlotSelectWidget& w, GameSelectControlServices& s)
{
    const u32 slot = u32(chosen_slot(w));
    auto& p = w.plates[slot];
    auto& plate_sprite = p.plate_sprite;
    const u32 row = 0x0042902C + slot * 0x124;
    const float rest_box_x = exe_real(s, row + 8) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC);
    const float rest_box_y = exe_real(s, row + 0xC) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC);
    count_down(w.zoom_timer, frame_step(s));
    const float one = exe_real(s, 0x0032B0F8);
    const float duration = exe_real(s, 0x00429008);
    float fraction = one;
    if (duration != 0.f)
    {
        fraction = (w.zoom_timer - duration) / (0.f - duration);
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
    p.box_width = ((screen_width(s) + margin) - rest_box_x) * fraction + rest_box_x;
    p.box_height = ((screen_height(s) + margin) - rest_box_y) * fraction + rest_box_y;
    const float x = exe_real(s, row);
    plate_sprite.position[0] = (s.hud.center_x - x) * fraction + x;
    const float y = exe_real(s, row + 4);
    plate_sprite.position[1] = (s.hud.center_y - y) * fraction + y;
    const float scale_x = exe_real(s, row + 8);
    plate_sprite.scale[0] = (one - scale_x) * fraction + scale_x;
    const float scale_y = exe_real(s, row + 0xC);
    plate_sprite.scale[1] = (one - scale_y) * fraction + scale_y;
    if (done)
    {
        set_runtime_slot_select_state(w, 8, s);
    }
}

// Xbox 0x000DE9E0, EDI widget: the selected slot's movie and the bat's flight to it.
bool select_slot_movie(SlotSelectWidget& w, GameSelectControlServices& s)
{
    if (w.flown_row == w.cursor_row
        && w.flown_slot == u32(w.slot))
    {
        return true;
    }
    const u32 slot = u32(chosen_slot(w));
    const u32 row = 0x0042902C + slot * 0x124;
    w.bat_target[0] = exe_real(s, row);
    w.bat_target[1] = exe_real(s, row + 0x18) + exe_real(s, row + 4);
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
    w.movie_held = 1;
    w.movie_frames = 0;
    if (w.bat != nullptr)
    {
        const ButtonBatPoint target{w.bat_target[0], w.bat_target[1]};
        s.fly_bat(*w.bat, target, exe_real(s, 0x00428F90)); // 0x0006DA00, EBX +0xB20, ESI the bat
        w.flown_slot = u32(w.slot);
        w.flown_row = w.cursor_row;
        return true;
    }
    return false;
}

// Xbox 0x000DF140: the background of the selected slot, the old one kept for the cross-fade.
void select_slot_background(SlotSelectWidget& w, GameSelectControlServices& s)
{
    if (w.old_background.texture.data() != nullptr)
    {
        release_texture(w.old_background.texture, s);
    }
    auto& background = w.background;
    auto& old_background = w.old_background;
    old_background.color = background.color;
    w.old_background.texture = w.background.texture;
    old_background.scale[0] = background.scale[0];
    old_background.scale[1] = background.scale[1];
    old_background.position[0] = background.position[0];
    old_background.position[1] = background.position[1];
    w.background.texture = {};
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
    w.background.texture = texture;
    if (texture.data() != nullptr && texture_has_frame(texture) && runtime_texture_format_drawable(texture))
    {
        background.scale[0] = screen_width(s) / texture_width(texture);
        background.scale[1] = screen_height(s) / texture_height(texture);
        if (w.bat != nullptr)
        {
            w.background_fade_timer = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
            w.background_fade = 0.f;
        }
        set_color_alpha(background.color, 0);
        set_color_alpha(old_background.color, 0xFF);
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
            auto& w = *s.frontend.slot_widget;
            w.loads_pending = w.loads_pending - 1;
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
            w.loads_pending = w.loads_pending + 1;
            load_save_slot(name.data(), s);
        }
    }
    w.scan_steps = w.scan_steps + 1;
}

// Xbox 0x000DF650: the next save-scan step.
void continue_save_scan(GameSelectControlServices& s)
{
    std::array<char16_t, 0x80> name{};
    save_next_name(name.data()); // 0x0010D8A0, EBX the name
    auto& w = *s.frontend.slot_widget;
    if (name[0] != 0 && !save_slot_loaded(name.data()))
    {
        w.loads_pending = w.loads_pending + 1;
        load_save_slot(name.data(), s);
    }
    w.scan_steps = w.scan_steps + 1;
}

// The plate texture of a slot replaced, keeping its drawn size (0x000DEB20's scale update).
void replace_plate_texture(SlotSelectWidget& w, size_t index, const char* name, GameSelectControlServices& s)
{
    auto& plate_sprite = w.plates[index].plate_sprite;
    const float old_width = texture_width(plate_sprite.texture);
    const float old_height = texture_height(plate_sprite.texture);
    const float scale_x = plate_sprite.scale[0];
    const float scale_y = plate_sprite.scale[1];
    if (plate_sprite.texture.data() != nullptr)
    {
        release_texture(plate_sprite.texture, s);
    }
    plate_sprite.texture = acquire_texture(name, s);
    plate_sprite.scale[0] = (scale_x * old_width) / texture_width(plate_sprite.texture);
    plate_sprite.scale[1] = (scale_y * old_height) / texture_height(plate_sprite.texture);
}

void copy_label(SlotSelectWidget& w, size_t index, const char16_t* text)
{
    char16_t* label = w.plates[index].label.text.data();
    std::u16string_view copied(text);
    std::copy(copied.begin(), copied.end(), label);
    label[copied.size()] = 0;
}

void copy_movie_name(SlotSelectWidget& w, size_t index, const char* name)
{
    std::strcpy(w.plates[index].movie_name.data(), name);
}

// An overlay sprite for the plate (the no-write and broken discs): its texture, scale, position offset from the
// plate sprite's, and color.
void set_plate_overlay(SlotSelectWidget::Plate& plate, GameSelectSprite& overlay, const char* name, u32 size_row,
    u32 offset_row, u32 alpha_row, GameSelectControlServices& s)
{
    overlay.texture = acquire_animation_resource(s.resources, name); // 0x00035E90
    if (overlay.texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700(2)
    }
    overlay.scale[0] = exe_real(s, size_row);
    overlay.scale[1] = exe_real(s, size_row + 4);
    overlay.position[0] = plate.plate_sprite.position[0] + exe_real(s, offset_row);
    overlay.position[1] = plate.plate_sprite.position[1] + exe_real(s, offset_row + 4);
    overlay.color = exe_word(s, 0x00314F48);
    set_color_alpha(overlay.color, s.image.memory(alpha_row, 1)[0]);
}

// Xbox 0x000DEB20, EAX widget: each slot's status, name, plate and movie from its save.
void describe_slots(SlotSelectWidget& w, GameSelectControlServices& s)
{
    for (size_t index = 0; index < 3; ++index)
    {
        const char16_t* name = slot_name(w, i32(index));
        auto& p = w.plates[index];
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
        p.status = status;
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
            p.movie_uv = {exe_real(s, row + 0x100), exe_real(s, row + 0x104)};
            break;
        }
        case 2:
        case 4:
        {
            // The save's chapter plate and movie (rows of 0x10C from 0x00429290), and its name.
            const u32 chapter = 0x00429290 + u32(slot_chapter_row(name, s)) * 0x10C;
            replace_plate_texture(w, index, exe_string(s, chapter), s);
            copy_movie_name(w, index, exe_string(s, chapter + 0x80));
            p.movie_uv = {exe_real(s, chapter + 0x100), exe_real(s, chapter + 0x104)};
            copy_label(w, index, name);
            if (status == 4)
            {
                set_plate_overlay(p, p.nowrite_sprite, "aid_texture_ghoulies_gameselect_disc_nowrite", 0x00428FEC,
                    0x00428FF4, 0x00428FFC, s);
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
            p.movie_uv = {exe_real(s, 0x0042949C), exe_real(s, 0x004294A0)};
            set_plate_overlay(p, p.broken_sprite, "aid_texture_ghoulies_gameselect_disc_broken", 0x00428FD8,
                0x00428FE0, 0x00428FE8, s);
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
    (void)owner; // the control's bytes; the widget reaches the control through +0xEA4
    w.state = 0;
    w.loads_pending = 0;
    w.scan_steps = 0;
    w.fade_out_timer = 0.f;
    w.prompt_pool = nullptr;
    w.plate_pool = nullptr;
    auto& prompt = w.prompt;
    prompt.record = 0;
    prompt.x = 0.f;
    prompt.y = 0.f;
    prompt.text[0] = 0;
    s.frontend.word_0042a0c8 = 0;
    s.frontend.word_0042a0c4 = 0;
    s.next_row = 1;
    s.selected_game[0] = 0; // lstrcpyW(DAT_00429BC4, 0x0030C9D0), an empty string.
    clear_runtime_profile_block(s.profile.progress.bytes);
    clear_runtime_profile_block(s.profile.unlocks.bytes);
    w.movie = nullptr;
    w.movie_frames = 0;
    w.movie_held = 0;
    w.zoom_timer = 0.f;
    w.bat_target[0] = 0.f;
    w.bat_target[1] = 0.f;
    w.cursor_row = exe_word(s, 0x00428F9C);
    w.slot = i32(exe_word(s, 0x00428FA0));
    w.flown_row = 0xBADBADBAu;
    w.flown_slot = 0xBADBADBAu;
    w.bat = nullptr;
    for (size_t index = 0; index < 3; ++index)
    {
        auto& name = w.slot_names[index];
        name[0] = 0;
    }
    const u32 white = exe_word(s, 0x00314F48);
    auto& background_sprite = w.background;
    auto& old_background_sprite = w.old_background;
    w.background.texture = acquire_texture(background, s);
    background_sprite.color = white;
    background_sprite.position[0] = s.hud.center_x;
    background_sprite.position[1] = s.hud.center_y;
    background_sprite.scale[1] = screen_height(s) / texture_height(w.background.texture);
    background_sprite.scale[0] = screen_width(s) / texture_width(w.background.texture);
    w.old_background.texture = {};
    old_background_sprite.color = white;
    old_background_sprite.position[0] = s.hud.center_x;
    old_background_sprite.position[1] = s.hud.center_y;
    old_background_sprite.scale[0] = background_sprite.scale[0];
    old_background_sprite.scale[1] = background_sprite.scale[1];
    w.background_fade_timer = 0.f;
    w.background_fade = 0.f;
    for (size_t index = 0; index < 3; ++index)
    {
        auto& p = w.plates[index];
        auto& plate_sprite = p.plate_sprite;
        auto& disc_sprite = p.disc_sprite;
        auto& nowrite_sprite = p.nowrite_sprite;
        auto& broken_sprite = p.broken_sprite;
        const u32 row = 0x0042902C + u32(index) * 0x124; // Plate table, stride 0x124: x, y, ..., texture at +0x1C.
        const u32 movie = 0x0042941C + u32(index) * 0x10C; // Movie names, stride 0x10C.
        auto& plate = w.plates[index];
        p.status = 0;
        const char16_t error[]{u'e', u'r', u'r', u'o', u'r', 0};
        auto& label = p.label;
        std::memcpy(label.text.data(), error, sizeof(error));
        label.x = s.hud.center_x + exe_real(s, 0x00428FA8);
        label.y = s.hud.center_y + exe_real(s, 0x00428FAC);
        plate.plate_sprite.texture = acquire_texture(exe_string(s, row + 0x1C), s);
        plate_sprite.position[0] = exe_real(s, row);
        plate_sprite.position[1] = exe_real(s, row + 4);
        plate_sprite.scale[0] = (screen_width(s) * exe_real(s, row + 0x10)) / texture_width(plate.plate_sprite.texture);
        plate_sprite.scale[1] =
            (screen_height(s) * exe_real(s, row + 0x14)) / texture_height(plate.plate_sprite.texture);
        plate_sprite.color = white;
        plate.disc_sprite.texture = {};
        plate.disc_sprite.texture =
            acquire_animation_resource(s.resources, "aid_texture_ghoulies_gameselect_disc_discspin");
        if (plate.disc_sprite.texture.data() == nullptr)
        {
            resource_error(s.error, 2);
        }
        disc_sprite.position[0] = exe_real(s, row) + exe_real(s, 0x00428FD0);
        disc_sprite.position[1] = exe_real(s, row + 4) + exe_real(s, 0x00428FD4);
        disc_sprite.scale[0] = exe_real(s, 0x00428FC8);
        disc_sprite.scale[1] = exe_real(s, 0x00428FCC);
        disc_sprite.color = white;
        nowrite_sprite.color = white;
        nowrite_sprite.position[0] = 0.f;
        nowrite_sprite.position[1] = 0.f;
        nowrite_sprite.scale[0] = 0.f;
        nowrite_sprite.scale[1] = 0.f;
        plate.nowrite_sprite.texture = {};
        broken_sprite.color = white;
        broken_sprite.position[0] = 0.f;
        broken_sprite.position[1] = 0.f;
        broken_sprite.scale[0] = 0.f;
        broken_sprite.scale[1] = 0.f;
        plate.broken_sprite.texture = {};
        copy_movie_name(w, index, exe_string(s, movie));
        p.movie_uv = {exe_real(s, movie + 0x80), exe_real(s, movie + 0x84)};
        p.box_color = exe_word(s, 0x00308B1C);
        p.box_width = exe_real(s, row + 0x10) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC);
        p.box_height = exe_real(s, row + 0x14) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC);
    }
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
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
    if (w.background.texture.data() != nullptr)
    {
        release_texture(w.background.texture, s);
        w.background.color = cleared;
    }
    if (w.old_background.texture.data() != nullptr)
    {
        release_texture(w.old_background.texture, s);
        w.old_background.color = cleared;
    }
    s.plate_flags[0] = w.saved_plate_flag_0;
    s.plate_flags[1] = w.saved_plate_flag_1;
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
    const u32 state = u32(w.state); // unsigned: the jump table bound
    if (state <= 8 && w.prompts != nullptr)
    {
        // State 5 updates the prompts with the front-end device (DAT_00429BBC); the others without one (the jump
        // table at 0x000DDEF0 with its index bytes at 0x000DDEF8).
        const bool device = state == 5;
        s.update_prompts(*w.prompts, device); // 0x00142460, EDI the widget
    }
    switch (w.state)
    {
    case 1:
        if (w.loads_pending == 0)
        {
            if (w.scan_steps < 3)
            {
                continue_save_scan(s);
                return;
            }
            set_runtime_slot_select_state(w, 2, s);
        }
        return;
    case 2:
    {
        if (s.timer_expired(&w.fade_out_timer) != 0) // 0x0012B2A0, EBX +0xB90
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
                const float value = full - ((w.fade_out_timer - duration) / (0.f - duration)) * full;
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
            set_color_alpha(w.plates[index].disc_sprite.color, truncate_byte(alpha));
        }
        return;
    }
    case 4:
        if (w.plates_fading == 0)
        {
            if (!runtime_gameselect_book_busy(*w.owner, s)) // 0x00083C40, ECX the owner
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
            auto& p = w.plates[index];
            set_color_alpha(p.disc_sprite.color, color_alpha(p.plate_sprite.color));
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
            if (w.movie_held != 0)
            {
                --w.movie->play_count;
                w.movie_held = 0;
            }
        }
        else if (w.movie_held == 0)
        {
            ++w.movie->play_count;
            w.movie_held = 1;
        }
    }
    const std::span<u8> frame = w.owner->texture;
    if (w.movie != nullptr && w.movie_frames >= i32(exe_word(s, 0x00429000)))
    {
        // The movie boundary locks a texture header by its native address.
        const u32 header = s.textures.address_of(frame.data()) + read_u32(frame, 4) + u32(frame[0x12]) * 0x14;
        copy_runtime_movie_frame(*w.movie, reinterpret_cast<void*>(std::uintptr_t(header)),
            s.movies); // 0x00107360, ESI the movie, ECX the texture header
    }
    auto& background = w.background;
    if (w.background.texture.data() != nullptr && texture_has_frame(w.background.texture)
        && runtime_texture_format_drawable(w.background.texture) && color_alpha(background.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.background.texture.data(), background.position.data(),
            background.scale.data(), 0.f, background.color, 0, s.draw); // 0x0005CE30
    }
    auto& old_background = w.old_background;
    if (w.old_background.texture.data() != nullptr && texture_has_frame(w.old_background.texture)
        && runtime_texture_format_drawable(w.old_background.texture) && color_alpha(old_background.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.old_background.texture.data(), old_background.position.data(),
            old_background.scale.data(), 0.f, old_background.color, 0, s.draw);
    }
    // The unselected plates, then the selected one on top.
    for (i32 index = 0; index < 3; ++index)
    {
        if (index != chosen_slot(w) && w.plates[size_t(index)].plate_sprite.texture.data() != nullptr)
        {
            draw_plate(w, size_t(index), s);
        }
    }
    if (w.plates[size_t(chosen_slot(w))].plate_sprite.texture.data() != nullptr)
    {
        draw_plate(w, size_t(chosen), s);
    }
    const i32 state = w.state;
    if (0 < state && (state <= 2 || state == 4))
    {
        for (size_t index = 0; index < 3; ++index)
        {
            if (w.plates[index].disc_sprite.texture.data() != nullptr)
            {
                draw_plate_sprite(-1, w.plates[index].disc_sprite, s);
            }
        }
    }
    if (w.movie != nullptr && w.movie->decoder != nullptr)
    {
        const i32 shown = w.movie_frames;
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
            auto& q = w.plates[size_t(chosen)];
            auto& r = chosen_plate(w).plate_sprite;
            const auto plate = w.plates[size_t(chosen)].plate_sprite.texture;
            const float scale[2]{(texture_width(plate) * r.scale[0]) / texture_width(frame),
                (texture_height(plate) * r.scale[1]) / texture_height(frame)};
            draw_sprite(0, uv_top, frame, scale, q.movie_uv.data(), color,
                q.plate_sprite.position.data(), s);
        }
        w.movie_frames = w.movie_frames + 1;
    }
    for (size_t index = 0; index < 3; ++index)
    {
        auto& p = w.plates[index];
        if (w.plates[index].broken_sprite.texture.data() != nullptr)
        {
            draw_plate_sprite(0, p.broken_sprite, s);
        }
        if (w.plates[index].nowrite_sprite.texture.data() != nullptr)
        {
            draw_plate_sprite(0, p.nowrite_sprite, s);
        }
    }
    if (w.plates_fading == 0 && w.bat != nullptr)
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
    const i32 previous = w.state;
    if (previous == 3)
    {
        if (w.plates_fading != 0)
        {
            w.plates_fading = 0;
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
        auto& prompt = w.prompt;
        prompt.x = s.hud.center_x;
        prompt.y = s.hud.center_y;
        prompt.x = prompt.x + exe_real(s, 0x00428FB4);
        prompt.y = exe_real(s, 0x00428FB8) + prompt.y;
        if (s.text.data() != nullptr) // IsBadReadPtr(DAT_005462C4, 1), the read check boundary.
        {
            lookup_borrowed_localtext(s.text, "gameselect", exe_string(s, 0x00324F60),
                prompt.text.data(), s.localtext); // 0x000FF120, EBX +4, EDI the key
        }
        prompt.record = create_dialogue_text_record(*w.prompt_pool,
            prompt.text.data(), prompt.x, prompt.y,
            exe_real(s, 0x00428FB0), exe_real(s, 0x00428FB0), 1.f, 1, 0, 1, s.metrics, s.viewport_width,
            s.next_text_id, s.random); // 0x0005F5C0, EAX 1
    }
    else if (previous == 6)
    {
        auto& label = chosen_plate(w).label;
        const u32 id = create_dialogue_text_record(*w.plate_pool, label.text.data(),
            label.x, label.y, exe_real(s, 0x00428FA4),
            exe_real(s, 0x00428FA4), 1.f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id,
            s.random); // 0x0005F5C0, EAX 1
        label.record = id;
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
        control->gameselect->requested_row = 3;
        control->gameselect->book_turn = 2;
        control->gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
    }
    w.state = state;
    switch (state)
    {
    case 1:
        begin_save_scan(s);
        start_pause_ditty(s.ditty, s.streams); // 0x000E5B40
        return;
    case 2:
        w.fade_out_timer = exe_real(s, 0x00429028);
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
            w.slot = found;
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
                w.slot = index;
            }
        }
        set_runtime_slot_select_state(w, 6, s);
        return;
    }
    case 4:
        w.plate_fade_timer = exe_real(s, 0x00429014);
        w.plates_fading = 1;
        return;
    case 6:
    {
        // Every plate back to its rest size and color.
        for (size_t index = 0; index < 3; ++index)
        {
            auto& p = w.plates[index];
            auto& plate_sprite = p.plate_sprite;
            const u32 row = 0x0042902C + u32(index) * 0x124;
            const auto plate = w.plates[index].plate_sprite.texture;
            plate_sprite.scale[0] = (screen_width(s) * exe_real(s, row + 0x10)) / texture_width(plate);
            plate_sprite.scale[1] = (screen_height(s) * exe_real(s, row + 0x14)) / texture_height(plate);
            p.box_width = exe_real(s, row + 0x10) * exe_real(s, 0x0032B6B0) + exe_real(s, 0x00428FBC);
            p.box_height = exe_real(s, row + 0x14) * exe_real(s, 0x0032B6AC) + exe_real(s, 0x00428FBC);
            p.box_color = exe_word(s, 0x00308B1C);
            if (w.plates[index].nowrite_sprite.texture.data() != nullptr)
            {
                auto& nowrite = p.nowrite_sprite;
                nowrite.scale[0] = exe_real(s, 0x00428FEC);
                nowrite.scale[1] = exe_real(s, 0x00428FF0);
            }
            if (w.plates[index].broken_sprite.texture.data() != nullptr)
            {
                auto& broken = p.broken_sprite;
                broken.scale[0] = exe_real(s, 0x00428FD8);
                broken.scale[1] = exe_real(s, 0x00428FDC);
            }
        }
        select_slot_movie(w, s);
        select_slot_background(w, s);
        for (size_t index = 0; index < 3; ++index)
        {
            auto& label = w.plates[index].label;
            end_dialogue_text_record(*w.plate_pool, label.record, s.fonts, s.fallback_texture,
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
        w.zoom_timer = exe_real(s, 0x00429008);
        end_dialogue_text_record(*w.plate_pool, chosen_plate(w).label.record,
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
    const u32 row = w.cursor_row;
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
    w.cursor_row = row;
    w.slot = slot;
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
        target.x = width - w.bat_target[0] > width * half ? width + exe_real(s, 0x0032B1E0)
                                                                            : exe_real(s, 0x0032B304);
        target.y = height - w.bat_target[1] > height * half ? height + exe_real(s, 0x0032B1E0)
                                                                              : exe_real(s, 0x0032B304);
        if (w.bat != nullptr)
        {
            s.fly_bat(*w.bat, target, 1.f); // 0x0006DA00, EBX the target
        }
        SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
        control->gameselect->requested_row = 3;
        control->gameselect->book_turn = 2;
        control->gameselect->book = create_transition_book(0, 1, nullptr, s.books); // 0x00048260(0, 1, 0)
        set_runtime_slot_select_state(w, 8, s); // 0x000DF9A0(DAT_005065B8, 8)
        return;
    }
    SceneControlInstance* control = find_runtime_scene_control(0x9A); // 0x00116170
    control->gameselect->requested_row = 4;
    control->gameselect->book_turn = 1;
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
    control->gameselect->requested_row = 2;
    control->gameselect->book_turn = 1;
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
