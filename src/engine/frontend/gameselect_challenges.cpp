#include "gameselect_challenges.h"
#include "button_bat_menu.h"
#include "dialogue_activation.h"
#include "dialogue_draw_context.h"
#include "dialogue_draw_outer.h"
#include "dialogue_measurement.h"
#include "dialogue_text_pool.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "frontend_sounds.h"
#include "frontend_state.h"
#include "gameselect_progress.h"
#include "hud_layout.h"
#include "input_activation.h"
#include "scoreboard_draw.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../cutscenes/event_runtime.h"
#include "../entities/actor_render_registration.h"
#include "../graphics/color_blend.h"
#include "../runtime/name_lookup.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_games.h"
#include "../saves/save_summary.h"
#include "../scene/executable_resources.h"
#include "../scene/challenge_runtime.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"
#include "gbtg/entity.h"

#include <bit>
#include <cstring>

namespace gbtg
{
extern u16 g_challenge_game_name[0x40]; // DAT_004E19B4, scene.cpp.

RuntimeChallengeSelection g_challenge_selection;

namespace
{
constexpr u32 kChallengeRows = 0x00305CD8; // "challengeMenu_basic"
constexpr u32 kCarousel = 0x00305C48; // Five slots of {x, y, scale x, scale y, -}, stride 0x14.
constexpr u32 kAidTag = 0x5F646961u; // "aid_"
constexpr i32 kLast = 0x14; // Challenge 21, the one with its own scene order.

GameChallengesWidget& challenges(GameSelectWidget& widget)
{
    return static_cast<GameChallengesWidget&>(widget);
}

GameChallengesWidget& open_widget(GameSelectControlServices& s)
{
    return *s.frontend.challenges_widget; // DAT_00506574
}

u32 exe_word(const GameSelectControlServices& s, u32 address)
{
    return read_u32(s.image.memory(address, 4), 0);
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(exe_word(s, address));
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale; // DAT_005096C0 * DAT_004312C4
    }
    return 0.f;
}

// A countdown ticked toward 0 by the frame step and clamped there.
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

u8 truncate_byte(float value)
{
    return u8(i32(value)); // 0x001673D8
}

void copy_string(char* output, const char* input)
{
    while ((*output++ = *input++) != 0)
    {
    }
}

bool is_aid(const char* name)
{
    u32 tag;
    std::memcpy(&tag, name, 4);
    return tag == kAidTag;
}

// 0x00035E90 with its 'aid_' test and 0x00032700(2) when missing.
std::span<const u8> acquire_named(const char* name, GameSelectControlServices& s)
{
    if (!is_aid(name))
    {
        return {};
    }
    const auto resource = acquire_animation_resource(s.resources, name);
    if (resource.data() == nullptr)
    {
        resource_error(s.error, 2);
    }
    return resource;
}

// 0x00035E90 of a name the code does not test first, and 0x00032700(2) when missing.
std::span<const u8> acquire_required(const char* name, GameSelectControlServices& s)
{
    const auto resource = acquire_animation_resource(s.resources, name);
    if (resource.data() == nullptr)
    {
        resource_error(s.error, 2);
    }
    return resource;
}

void release(std::span<const u8>& resource, GameSelectControlServices& s)
{
    release_animation_resource(s.resources, resource); // 0x00035C00
    resource = {};
}

// A 0x80-byte name inside a resource; a name past its end reads as no name.
const char* name_at(std::span<const u8> resource, size_t offset)
{
    if (resource.data() == nullptr || resource.size() < offset + 4)
    {
        return nullptr;
    }
    return reinterpret_cast<const char*>(resource.data() + offset);
}

GameChallengeEntry& entry(GameChallengesWidget& w, i32 index)
{
    return (*w.entries)[size_t(index)];
}

float texture_width(std::span<const u8> texture)
{
    return float(i32(read_u16(texture, 8)));
}

float texture_height(std::span<const u8> texture)
{
    return float(i32(read_u16(texture, 10)));
}

// 0x0005CDC0, 0x0005CA60 (EAX the position, ECX frame 0, EDX no top texture coordinates), then the override hold
// DAT_00509B5C is released.
void draw_picture(std::span<const u8> texture, const float* position, const float* scale, u32 color,
    GameSelectControlServices& s)
{
    auto& draw = s.draw;
    begin_dialogue_sprite_draw(draw); // 0x0005CDC0
    draw_dialogue_texture(0, nullptr, const_cast<u8*>(texture.data()), scale, nullptr, 0.f, color, position,
        draw); // 0x0005CA60
    --draw.override_count;
    if (draw.override_count == 0)
    {
        draw.fog_disabled = 0;
    }
}

// The counter preset at 0x00428C10 ({scale, 0.5, 0.1, 10, white}) with its own scale and alpha.
std::array<u8, 0x14> counter_preset(GameSelectControlServices& s)
{
    std::array<u8, 0x14> preset{};
    std::memcpy(preset.data(), s.image.memory(0x00428C10, 0x14).data(), 0x14);
    return preset;
}

RuntimeScoreboardHud& scoreboard(GameSelectControlServices& s)
{
    if (s.scoreboard == nullptr)
    {
        throw Error("the challenges' counters: the scoreboard HUD (0x0012F000) is not bound");
    }
    return *s.scoreboard;
}

// Xbox 0x000D6EF0, ESI the widget: the cursor bat, made once.
bool create_challenges_bat(GameChallengesWidget& w, GameSelectControlServices& s)
{
    if (w.bat == nullptr)
    {
        w.bat = s.create_bat({0.f, 0.f}, 2.f, 0.4f); // 0x0013B580(2.0, 0.4, 0, 0)
        if (w.bat == nullptr)
        {
            return false;
        }
    }
    return true;
}

void fly_bat(GameChallengesWidget& w, GameSelectControlServices& s)
{
    const ButtonBatPoint target{w.bat_target[0], w.bat_target[1]};
    s.fly_bat(*w.bat, target, 2.f); // 0x0006DA00(2.0), ESI the bat, EBX +0x124
}

// Xbox 0x000D6F40, EAX the widget: the bat to the middle of the carousel.
bool aim_challenges_bat(GameChallengesWidget& w, GameSelectControlServices& s)
{
    w.bat_target[0] = exe_real(s, 0x0032B564);
    w.bat_target[1] = exe_real(s, 0x0032B138);
    if (w.bat != nullptr)
    {
        fly_bat(w, s);
        return true;
    }
    return false;
}

// Xbox 0x000D6F90, EDI the widget: every entry's picture released.
void release_entry_textures(GameChallengesWidget& w, GameSelectControlServices& s)
{
    for (i32 index = 0; index < w.count; ++index)
    {
        auto& challenge = entry(w, index);
        if (challenge.texture.data() != nullptr)
        {
            release(challenge.texture, s);
        }
    }
}

// Xbox 0x000D6FE0, ESI the widget: the pictures of the previous and the selected challenge, crossed over the bat's
// flight.
void select_challenge_pictures(GameChallengesWidget& w, GameSelectControlServices& s)
{
    auto& selection = g_challenge_selection;
    if (w.old_picture.texture.data() != nullptr)
    {
        release(w.old_picture.texture, s);
    }
    w.old_picture.texture = acquire_named(entry(w, selection.previous).texture_name.data(), s);
    if (w.picture.texture.data() != nullptr)
    {
        release(w.picture.texture, s);
    }
    w.picture.texture = acquire_named(entry(w, selection.selected).texture_name.data(), s);
    w.fade_timer = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
    w.fade = 0.f;
    set_color_alpha(w.picture.color, 0);
    set_color_alpha(w.old_picture.color, 0xFF);
}

// Xbox 0x000D6A30, ECX the slot, ESI the widget: one challenge of the carousel, its box, picture, trophy and number.
void draw_carousel_slot(GameChallengesWidget& w, i32 slot, GameSelectControlServices& s)
{
    auto& challenge = *w.shown[size_t(slot)];
    if (challenge.texture.data() == nullptr)
    {
        return;
    }
    const auto slot_real = [&s](i32 index, u32 field)
    {
        return exe_real(s, kCarousel + u32(index * 0x14) + field);
    };
    std::array<float, 2> position{slot_real(slot, 0), slot_real(slot, 4)};
    std::array<float, 2> scale{slot_real(slot, 8), slot_real(slot, 0xC)};
    const float one = exe_real(s, 0x0032B0F8);
    if (w.fade < one)
    {
        // Each slot slides in from its neighbour: the one to its right after a move right, to its left after a move
        // left. The end slot with no neighbour that way stays put.
        const bool left = g_challenge_selection.moved_left != 0; // DAT_00506580
        const i32 from = left ? slot - 1 : slot + 1;
        if (left ? 0 < slot : slot + 1 < 5)
        {
            position[0] = (position[0] - slot_real(from, 0)) * w.fade + slot_real(from, 0);
            position[1] = (position[1] - slot_real(from, 4)) * w.fade + slot_real(from, 4);
            scale[0] = (scale[0] - slot_real(from, 8)) * w.fade + slot_real(from, 8);
            const float height = slot_real(from, 0xC);
            scale[1] = (slot_real(slot, 0xC) - height) * w.fade + height;
        }
    }
    if (4 < w.state && slot == 2)
    {
        // Leaving with the chosen challenge: it grows into the middle.
        position[0] = w.leave * exe_real(s, 0x0032B100) + exe_real(s, 0x0032B564);
        position[1] = w.leave * exe_real(s, 0x0032B2F0) + exe_real(s, 0x0032B2FC);
        scale[0] = w.leave * exe_real(s, 0x0032B210) + exe_real(s, 0x0032B0F4);
        scale[1] = scale[0];
    }
    const float half = exe_real(s, 0x0032B0F4);
    const float border = scale[0] * half * exe_real(s, 0x0032B2F0);
    const float width = float(i32(s.screen[0])) * scale[0] * half + border; // DAT_005096E8
    const float height = float(i32(s.screen[1])) * scale[1] * half + border; // DAT_005096EA
    const float left = position[0] - width * half;
    const float top = position[1] - height * half;
    draw_runtime_dialogue_panel(float(i32(left)), float(i32(top)), float(i32(width)), float(i32(height)),
        exe_real(s, 0x00428C18), challenge.box_color, s.outer); // 0x00061360, radius 0.1
    draw_picture(challenge.texture, position.data(), scale.data(), challenge.color, s);
    if (challenge.unlocked != 0)
    {
        // The jump table at 0x000D6ED4: 0 unplayed, 3..6 bronze to platinum, 1 and 2 no icon.
        std::span<const u8> trophy;
        switch (challenge.trophy)
        {
        case 0:
            trophy = w.trophies[0];
            break;
        case 3:
            trophy = w.trophies[1];
            break;
        case 4:
            trophy = w.trophies[2];
            break;
        case 5:
            trophy = w.trophies[3];
            break;
        case 6:
            trophy = w.trophies[4];
            break;
        default:
            break;
        }
        if (trophy.data() != nullptr)
        {
            const float size = scale[0] * exe_real(s, 0x0032B260);
            const float at[2]{left + width, w.trophy_drop * size + top};
            const float trophy_scale[2]{size, size};
            draw_runtime_dialogue_sprite(trophy.data(), at, trophy_scale, 0.f, exe_word(s, 0x00314F48), -1,
                s.draw); // 0x0005CE30
        }
    }
    if ((s.flags & 0x40u) == 0) // DAT_0050EA9C
    {
        auto preset = counter_preset(s);
        float digit_scale = scale[0] * exe_real(s, 0x0032B4B0);
        if (exe_real(s, 0x0032B11C) < digit_scale)
        {
            digit_scale = exe_real(s, 0x0032B11C);
        }
        std::memcpy(preset.data(), &digit_scale, 4);
        if (w.intro != 0 && w.fade < one)
        {
            const float full = exe_real(s, 0x0032B158);
            float alpha = w.fade * full;
            if (full < alpha)
            {
                alpha = full;
            }
            else if (alpha < 0.f)
            {
                alpha = 0.f;
            }
            preset[0x13] = truncate_byte(alpha);
        }
        float at[2]{left + border, top + border};
        draw_runtime_hud_counter(challenge.number, at, preset, 1, scoreboard(s), *scoreboard(s).draw); // 0x0012F000
    }
}

// The intro's alphas (0x000D6410 state 1): every entry fades in, the selected one's box brighter.
void fade_in_entries(GameChallengesWidget& w, GameSelectControlServices& s)
{
    s.timer_expired(&w.intro_timer); // 0x0012B2A0, EAX +0x190
    const float one = exe_real(s, 0x0032B0F8);
    float fraction = (w.intro_timer - one) * exe_real(s, 0x0032B120);
    if (one < fraction)
    {
        fraction = one;
    }
    else if (fraction < 0.f)
    {
        fraction = 0.f;
    }
    u32 color = exe_word(s, 0x00314F48);
    set_color_alpha(color, truncate_byte(fraction * exe_real(s, 0x0032B158)));
    u32 box = exe_word(s, 0x00305FEC);
    set_color_alpha(box, truncate_byte(fraction * exe_real(s, 0x0032B368)));
    for (i32 index = 0; index < w.count; ++index)
    {
        entry(w, index).color = color;
        entry(w, index).box_color = box;
    }
    auto& selected = entry(w, g_challenge_selection.selected);
    selected.box_color = exe_word(s, 0x00305FE8);
    set_color_alpha(selected.box_color, truncate_byte(fraction * exe_real(s, 0x0032B378)));
}

// Challenge 21's script: its scene order stepped DAT_004E1740 times from the first (the inlined walk at
// 0x000D5B17), each step the name after the current one, back to the first after a missing one.
const char* challenge21_script(GameSelectControlServices& s)
{
    const i32 steps = g_game_progress.word_1740;
    const auto order = acquire_required("aid_aidlist_ghoulies_sceneorder_challenge21", s); // 0x003064F0
    const char* head = name_at(order, 0);
    i32 count = 0; // 0x000359D0, EAX the order: each name found again from the first, until one has no next.
    for (const char* name = head; name != nullptr;)
    {
        ++count;
        const char* next = nullptr;
        for (const char* at = head; at != nullptr;)
        {
            if (transition_aid_equal(name, at)) // 0x000357D0, EAX the name, EDX the entry
            {
                if (is_aid(at + 0x80))
                {
                    next = at + 0x80;
                }
                break;
            }
            const char* after = at + 0x80;
            at = is_aid(after) ? after : nullptr;
        }
        name = next;
    }
    const char* current = head;
    for (i32 step = 0; 0 < steps && step < count;)
    {
        ++step;
        if (head == nullptr)
        {
            current = nullptr;
        }
        else if (current == nullptr)
        {
            current = head;
        }
        else
        {
            const char* next = nullptr;
            for (const char* at = head; at != nullptr;)
            {
                if (transition_aid_equal(current, at)) // 0x000357D0, EAX the current, EDX the entry
                {
                    if (is_aid(at + 0x80))
                    {
                        next = at + 0x80;
                    }
                    break;
                }
                const char* after = at + 0x80;
                at = is_aid(after) ? after : nullptr;
            }
            current = next;
        }
        if (steps <= step)
        {
            break;
        }
    }
    if (current == nullptr)
    {
        throw Error("0x000D5880: challenge 21's scene order has no script at step " + std::to_string(steps));
    }
    return current; // The order is held, not released (0x000D5880 keeps it).
}

// The menu's text pages are looked up in the scenenames resource, into DAT_00507E78.
void scene_name_text(std::span<const u8> scenenames, const char* key, std::array<char16_t, 0x100>& text,
    GameSelectControlServices& s)
{
    lookup_borrowed_localtext(scenenames, "scenenames", key, text.data(), s.localtext); // 0x000FF120, EBX 0x00507E78
}

// Xbox 0x0004A3D0, EAX the thresholds: the chosen challenge's trophy scores (DAT_004E17DC).
void set_challenge_thresholds(const std::array<i32, 4>& thresholds)
{
    g_challenge_selection.thresholds = thresholds;
}

// Xbox 0x0004A3B0, EAX the key: the chosen challenge's scenenames key (DAT_004E17EC).
void set_challenge_key(const char* key)
{
    copy_string(g_challenge_selection.key.data(), key);
}

// Xbox 0x000D5860, EAX the index: the selection, the old one kept; 0 when it was already selected.
bool set_challenge_selection(i32 index)
{
    auto& selection = g_challenge_selection;
    const i32 old = selection.selected;
    if (index == old)
    {
        return false;
    }
    selection.selected = index;
    selection.previous = old;
    return true;
}

SceneControlInstance& gameselect_control()
{
    return *find_runtime_scene_control(0x9A); // 0x00116170, EDX 0x9A
}

// The control's next row with a new menu book.
void request_row(u32 row, u32 turn, i32 clip, GameSelectControlServices& s)
{
    auto& control = *gameselect_control().gameselect;
    control.requested_row = row;
    control.book_turn = turn;
    control.book = create_transition_book(clip, 1, nullptr, s.books); // 0x00048260(clip, 1, 0)
}
}

// Xbox 0x000D5850
GameSelectWidgetHandle create_runtime_game_challenges(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameChallengesWidget>(); // 0x000F72D0(0x1E0)
}

// Xbox 0x000D5880
void open_runtime_game_challenges_menu(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = challenges(widget);
    auto& selection = g_challenge_selection;
    (void)owner; // the control's bytes; the widget reaches the control through +0
    w.name[0] = 0;
    std::copy(s.frontend.selected_name.begin(), s.frontend.selected_name.end(), w.name.begin()); // lstrcpyW
    load_runtime_save_progress(w.name.data(), s); // 0x000F3020, EAX +8
    s.profile.scene_flags->bytes[0] |= 2;
    s.profile.scene_flags->bytes[0] |= 4;
    refresh_runtime_save_summary(s); // 0x000F2F00
    w.chosen = 0;
    w.state = 0;
    w.owner = &control;
    w.bat_target = {0.f, 0.f};
    s.frontend.challenges_widget = &w; // DAT_00506574
    w.bat = nullptr;
    allocate_dialogue_text_pool(w.pool, 0, 1, 0xC0, s.text_pools); // 0x0005F390, ESI +0x148
    w.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    auto scenenames = acquire_required("aid_loctext_ghoulies_script_scenenames", s); // 0x00305FF0
    const auto order = acquire_required("aid_aidlist_ghoulies_challenges_challengeorder", s); // 0x00306370
    w.entries = std::make_unique<std::array<GameChallengeEntry, 21>>(); // 0x000F72D0(0x5CDC)
    w.shown = {};
    i32 found = 0;
    const auto& books = s.profile.challenges; // DAT_00506C54
    for (i32 index = 0; index < books.size; ++index)
    {
        for (u8 bits = books.bytes[size_t(index)]; bits != 0; bits = u8(bits & (bits - 1)))
        {
            ++found;
        }
    }
    if (selection.selected == kLast && (s.profile.progress.bytes[4] & 0x20) == 0) // DAT_00506C6C
    {
        selection.previous = kLast;
        selection.selected = 0;
    }
    for (i32 index = 0; index < 0x15; ++index)
    {
        const char* name = name_at(order, size_t(index) * 0x80);
        std::span<const u8> params;
        if (name != nullptr && is_aid(name))
        {
            params = acquire_required(name, s);
        }
        if (params.data() == nullptr)
        {
            throw Error("0x000D5880: challenge " + std::to_string(index + 1) + " has no params in its order");
        }
        const char* text = reinterpret_cast<const char*>(params.data());
        auto& challenge = entry(w, index);
        challenge.index = index;
        if (index == kLast)
        {
            challenge.unlocked = 1;
        }
        else
        {
            challenge.unlocked = index * 5 + 5 <= found ? 1 : 0;
        }
        challenge.texture = {};
        challenge.color = exe_word(s, 0x00314F48);
        challenge.box_color = exe_word(s, 0x00305FEC);
        challenge.word_38 = 2;
        std::array<char16_t, 0x100> localized{};
        if (challenge.unlocked == 0)
        {
            copy_string(challenge.texture_name.data(), "aid_texture_ghoulies_gameselect_challenges_locked");
            challenge.script[0] = 0;
            scene_name_text(scenenames, "locked", localized, s); // EDI 0x00323DC4
            copy_string(challenge.key.data(), "challengelocked"); // 0x00324A8C
        }
        else
        {
            copy_string(challenge.texture_name.data(), text + 0xC0);
            if (index == kLast)
            {
                copy_string(challenge.script.data(), challenge21_script(s)); // +0x5B38
            }
            else
            {
                copy_string(challenge.script.data(), text + 0x40);
            }
            scene_name_text(scenenames, text, localized, s);
            copy_string(challenge.key.data(), text);
        }
        std::copy(localized.begin(), localized.end(), challenge.name.text.begin()); // lstrcpyW(+0x3C, DAT_00507E78)
        challenge.name.x = s.hud.center_x; // DAT_00509730
        challenge.name.y = s.hud.center_y; // DAT_00509734
        for (size_t threshold = 0; threshold < 4; ++threshold)
        {
            challenge.thresholds[threshold] = i32(read_u32(params, 0x140 + threshold * 4));
        }
        challenge.trophy = i32(read_u32(g_game_progress.words_194c, size_t(challenge.index) * 4)); // DAT_004E194C
        challenge.number = {};
        challenge.number.word_2c = exe_word(s, 0x00428C10);
        challenge.number.value = index + 1 < 0 ? 0 : index + 1;
        challenge.number.target = challenge.number.value;
        update_runtime_hud_counter_digits(challenge.number, s.image.memory(0x00428C10, 0x14)); // 0x0012F990
        release(params, s); // 0x00035C00, EBX the params
    }
    w.count = i32(u32(s.profile.progress.bytes[4] >> 5) & 1u | 0x14u);
    w.books = {};
    w.books.word_2c = exe_word(s, 0x00428C10);
    w.books.value = found < 0 ? 0 : found;
    w.books.target = w.books.value;
    update_runtime_hud_counter_digits(w.books, s.image.memory(0x00428C10, 0x14)); // 0x0012F990
    w.books_icon = acquire_required("aid_texture_ghoulies_scoreboard_pauseicons_bonusbooks", s); // 0x003063F0
    w.books_icon_half_width = texture_width(w.books_icon) * exe_real(s, 0x0032B0F4);
    w.picture.texture = acquire_named(entry(w, selection.selected).texture_name.data(), s);
    const u32 white = exe_word(s, 0x00314F48);
    const float middle_x = exe_real(s, 0x0032B564);
    const float middle_y = exe_real(s, 0x0032B680);
    w.picture.color = white;
    w.picture.position = {middle_x, middle_y};
    w.picture.scale[1] = float(i32(s.screen[1])) / texture_height(w.picture.texture);
    w.picture.scale[0] = float(i32(s.screen[0])) / texture_width(w.picture.texture);
    w.old_picture.texture = {};
    w.old_picture.color = white;
    w.old_picture.position = {middle_x, middle_y};
    w.old_picture.scale = w.picture.scale;
    const float one = exe_real(s, 0x0032B0F8);
    w.fade_timer = 0.f;
    w.fade = one;
    w.leave_timer = 0.f;
    w.leave = 0.f;
    w.intro_timer = one;
    w.intro = 1;
    w.saved_plate_flag_0 = s.plate_flags[0];
    w.saved_plate_flag_1 = s.plate_flags[1];
    s.plate_flags[0] = 1; // DAT_00510F7C
    s.plate_flags[1] = 1; // DAT_00510F80
    w.trophies[0] = acquire_required("aid_texture_ghoulies_scoreboard_challenge_unplayed", s); // 0x00306070
    w.trophies[1] = acquire_required("aid_texture_ghoulies_scoreboard_challenge_bronzetrophy", s); // 0x003060F0
    w.trophies[2] = acquire_required("aid_texture_ghoulies_scoreboard_challenge_silvertrophy", s); // 0x00306170
    w.trophies[3] = acquire_required("aid_texture_ghoulies_scoreboard_challenge_goldtrophy", s); // 0x003061F0
    w.trophies[4] = acquire_required("aid_texture_ghoulies_scoreboard_challenge_platinumtrophy", s); // 0x00306270
    w.trophy_drop = texture_height(w.trophies[1]) * exe_real(s, 0x0032B0F4);
    w.prompts = nullptr;
    select_runtime_challenge(w, selection.selected, 0, s); // 0x000D7640(0), ECX the widget
    set_runtime_game_challenges_state(w, 1, s); // 0x000D70E0(1)
    release(scenenames, s); // 0x00035C00; the challenge order is kept (retail never releases it)
    start_pause_ditty(s.ditty, s.streams); // The 0x000E5B40 inline, while DAT_00429BB8 is clear.
}

// Xbox 0x000D62A0
void close_runtime_game_challenges(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = challenges(widget);
    s.frontend.challenges_widget = nullptr; // DAT_00506574
    release_dialogue_text_pool(w.pool, *s.text_pools); // 0x0005F460
    w.pool = nullptr;
    release_entry_textures(w, s); // 0x000D6F90
    if (w.bat != nullptr)
    {
        activate_actor_bind_tree(*w.bat); // 0x00112CC0: message 0x80 retires it.
        w.bat = nullptr;
    }
    const u32 cleared = exe_word(s, 0x00314F30);
    if (w.picture.texture.data() != nullptr)
    {
        release(w.picture.texture, s);
        w.picture.color = cleared;
    }
    if (w.old_picture.texture.data() != nullptr)
    {
        release(w.old_picture.texture, s);
        w.old_picture.color = cleared;
    }
    release_entry_textures(w, s); // 0x000D6F90
    release(w.books_icon, s);
    for (auto& trophy : w.trophies)
    {
        release(trophy, s);
    }
    if (w.prompts != nullptr)
    {
        destroy_dialogue_widget(w.prompts, s.resources, *s.text_pools); // 0x00142070
        w.prompts = nullptr;
    }
    s.plate_flags[0] = w.saved_plate_flag_0;
    s.plate_flags[1] = w.saved_plate_flag_1;
    w.entries.reset(); // HeapFree on the block's heap (0x000F7100): the host heap boundary.
}

// Xbox 0x000D6410
void tick_runtime_game_challenges(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = challenges(widget);
    // The function starts with fabs(sin(fmod(DAT_004DEA60, 1) * 2 pi)) (0x00152BE0, 0x00153320, 0x00152BB0) and
    // drops the result.
    if (s.dialogue_page_hold() == 0 && w.prompts != nullptr && u32(w.state) <= 6) // DAT_004EE1EC
    {
        // The jump table at 0x000D67CC: state 2 passes the front-end device (DAT_00429BBC), the others none.
        s.update_prompts(*w.prompts, w.state == 2); // 0x00142460
    }
    switch (w.state)
    {
    case 1:
        if (w.intro != 0)
        {
            fade_in_entries(w, s);
        }
        if (w.intro_timer < exe_real(s, 0x0032B1A4) && !runtime_gameselect_book_busy(*w.owner, s)) // 0x00083C40
        {
            set_runtime_game_challenges_state(w, 2, s);
        }
        break;
    case 3:
    {
        auto& selected = entry(w, g_challenge_selection.selected);
        count_down(w.fade_timer, frame_step(s));
        const float flight = s.bat_screen(*w.bat).flight_duration; // Bat +0x93C
        const float one = exe_real(s, 0x0032B0F8);
        float fraction = one;
        if (flight != 0.f)
        {
            fraction = (w.fade_timer - flight) / (0.f - flight);
            if (one < fraction)
            {
                fraction = one;
            }
            else if (fraction < 0.f)
            {
                fraction = 0.f;
            }
        }
        w.fade = fraction;
        if (w.old_picture.texture.data() != w.picture.texture.data())
        {
            const u8 alpha = truncate_byte(fraction * exe_real(s, 0x0032B158));
            set_color_alpha(w.picture.color, alpha);
            set_color_alpha(w.old_picture.color, u8(0xFF - alpha));
        }
        selected.box_color = menu_color(exe_word(s, 0x00305FE8), exe_word(s, 0x00305FEC), fraction); // 0x00101730
        if (one <= w.fade)
        {
            set_runtime_game_challenges_state(w, 2, s);
        }
        break;
    }
    case 4:
        if (s.dialogue_page_hold() == 0)
        {
            set_runtime_game_challenges_state(w, 2, s);
        }
        break;
    case 5:
    {
        count_down(w.leave_timer, frame_step(s));
        const float one = exe_real(s, 0x0032B0F8);
        float leave = (w.leave_timer - exe_real(s, 0x0032B2AC)) * exe_real(s, 0x0032B6D0);
        if (one < leave)
        {
            leave = one;
        }
        else if (leave < 0.f)
        {
            leave = 0.f;
        }
        w.leave = leave;
        if (one <= leave)
        {
            set_runtime_game_challenges_state(w, 6, s);
        }
        break;
    }
    default:
        break;
    }
}

// Xbox 0x000D67F0
void draw_runtime_game_challenges(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = challenges(widget);
    if (w.picture.texture.data() != nullptr && color_alpha(w.picture.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.picture.texture.data(), w.picture.position.data(), w.picture.scale.data(), 0.f,
            w.picture.color, 0, s.draw); // 0x0005CE30
    }
    if (w.old_picture.texture.data() != nullptr && color_alpha(w.old_picture.color) != 0)
    {
        draw_runtime_dialogue_sprite(w.old_picture.texture.data(), w.old_picture.position.data(),
            w.old_picture.scale.data(), 0.f, w.old_picture.color, 0, s.draw); // 0x0005CE30
    }
    if (w.books_icon.data() != nullptr)
    {
        auto preset = counter_preset(s);
        const float one = exe_real(s, 0x0032B0F8);
        if (w.intro != 0 && w.fade < one)
        {
            const float full = exe_real(s, 0x0032B158);
            float alpha = w.fade * full;
            if (full < alpha)
            {
                alpha = full;
            }
            else if (alpha < 0.f)
            {
                alpha = 0.f;
            }
            preset[0x13] = truncate_byte(alpha);
        }
        u32 color;
        std::memcpy(&color, preset.data() + 0x10, 4);
        const float* position = reinterpret_cast<const float*>(s.image.memory(0x00305CC0, 8).data());
        const float* scale = reinterpret_cast<const float*>(s.image.memory(0x00305CC8, 8).data());
        draw_picture(w.books_icon, position, scale, color, s);
        float at[2]{w.books_icon_half_width + exe_real(s, 0x0032B590), exe_real(s, 0x0032B58C)};
        draw_runtime_hud_counter(w.books, at, preset, 1, scoreboard(s), *scoreboard(s).draw); // 0x0012F000
    }
    for (i32 slot = 0; slot < 5; ++slot)
    {
        if (slot != 2)
        {
            draw_carousel_slot(w, slot, s); // 0x000D6A30
        }
    }
    draw_carousel_slot(w, 2, s); // The selected one last, on top.
    if (s.dialogue_page_hold() == 0 && w.intro == 0 && w.bat != nullptr)
    {
        const float scale = exe_real(s, 0x0032B110);
        if (w.bat->scale_200 != scale)
        {
            w.bat->scale_200 = scale;
            w.bat->xform_valid = 0;
        }
        s.draw_actor(*w.bat, i32(s.outer.current_display_address - 0x507018u) / 0x1E4, 3); // Vtable +0x14
    }
    if (w.prompts != nullptr)
    {
        draw_runtime_dialogue_widget(*w.prompts, 1, s.outer); // 0x00142720(1)
    }
}

// Xbox 0x000D70E0
void set_runtime_game_challenges_state(GameChallengesWidget& w, i32 state, GameSelectControlServices& services)
{
    auto& s = services;
    auto& selection = g_challenge_selection;
    const auto text_record = [&s, &w](const char16_t* text, float x, float y)
    {
        const float scale = std::bit_cast<float>(0x3FACCCCDu); // 1.35
        return create_dialogue_text_record(*w.pool, text, x, y, scale, scale, 1.f, 1, 0, 1, s.metrics,
            s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
    };
    const auto record = [&text_record](GameSelectTextLine& line)
    {
        return text_record(line.text.data(), line.x, line.y);
    };
    if (w.state == 1)
    {
        if (w.intro != 0)
        {
            w.intro = 0;
            create_challenges_bat(w, s); // 0x000D6EF0
        }
        const float inset = exe_real(s, 0x0032B1F4);
        const std::array<float, 2> anchor{s.hud.inner_left + inset, s.hud.inner_bottom - inset}; // DAT_00509740
        const std::array<float, 2> row_step{0.f, exe_real(s, 0x0032B600)};
        const std::array<float, 2> origin{exe_real(s, 0x0032B234), exe_real(s, 0x0032B5FC)};
        const char* resource = s.image.string(0x00306470); // aid_loctext_ghoulies_gameselect_gameselecttext
        const u8* rows = s.image.memory(kChallengeRows, 0x310).data();
        std::array<char16_t, 0x200> scratch{};
        const float width = measure_dialogue_widget_width(resource, rows, 1.f, 0.75f, "gameselect", scratch.data(),
            s.widgets); // 0x001428F0, EAX the resource, ECX the rows
        const std::array<float, 2> column{width, 0.f};
        create_resource_dialogue_widget(w.prompts, 1, rows, anchor, row_step, origin, &column, 1.f, 0.75f,
            "gameselect", resource, 0x400C0, s.widget_allocation); // 0x00141DA0, ESI +0x1A0
        w.prompts->pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
        auto& selected = entry(w, selection.selected);
        selected.name.record = record(selected.name);
        text_record(w.name.data(), float(i32(s.screen[0])) * exe_real(s, 0x0032B0F4), 35.f); // +8, its id dropped
        aim_challenges_bat(w, s); // 0x000D6F40
    }
    else if (w.state == 3)
    {
        auto& selected = entry(w, selection.selected);
        selected.name.record = record(selected.name);
    }
    else if (w.state == 5)
    {
        request_row(3, 2, 0, s); // 0x00116170, 0x00048260(0, 1, 0)
    }
    w.state = state;
    switch (state)
    {
    case 3:
        for (i32 index = 0; index < w.count; ++index)
        {
            entry(w, index).box_color = exe_word(s, 0x00305FEC);
        }
        w.bat_target[0] = exe_real(s, 0x0032B564);
        w.bat_target[1] = exe_real(s, 0x0032B138);
        if (w.bat != nullptr)
        {
            fly_bat(w, s);
        }
        select_challenge_pictures(w, s); // 0x000D6FE0
        for (auto& challenge : *w.entries)
        {
            end_dialogue_text_record(*w.pool, challenge.name.record, s.fonts, s.fallback_texture,
                s.random); // 0x0005F7E0
        }
        break;
    case 4:
    {
        std::array<char, 0x100> key{};
        copy_string(key.data(), s.image.string(exe_word(s, 0x00428C0C))); // PTR "challengeintros_"
        std::strcat(key.data(), entry(w, selection.selected).key.data());
        if (s.cutscenes.dialogue == nullptr)
        {
            throw Error("the challenge's intro page: the scene dialogue (0x000582A0) is not bound");
        }
        enqueue_runtime_dialogue_key(key.data(), 0, *s.cutscenes.dialogue); // 0x000582A0(0)
        break;
    }
    case 5:
    {
        w.leave_timer = exe_real(s, 0x0032B2AC);
        w.leave = 0.f;
        end_all_dialogue_text_records(*w.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
        w.prompts->requested_set = nullptr; // +0x950
        set_dialogue_widget_mode(*w.prompts, 3, s.widgets); // 0x00142C20, EAX 3
        // The bat leaves by the nearer edges.
        if (exe_real(s, 0x0032B564) < exe_real(s, 0x0032B6B0) - w.bat_target[0])
        {
            w.bat_target[0] = exe_real(s, 0x0032B6D8);
        }
        else
        {
            w.bat_target[0] = exe_real(s, 0x0032B304);
        }
        if (exe_real(s, 0x0032B680) < exe_real(s, 0x0032B6AC) - w.bat_target[1])
        {
            w.bat_target[1] = exe_real(s, 0x0032B6D4);
        }
        else
        {
            w.bat_target[1] = exe_real(s, 0x0032B304);
        }
        if (w.bat != nullptr)
        {
            fly_bat(w, s);
        }
        break;
    }
    case 6:
    {
        auto& chosen = entry(w, selection.selected);
        if (w.chosen != 0 && chosen.unlocked != 0)
        {
            set_challenge_thresholds(chosen.thresholds); // 0x0004A3D0, EAX +0x448
            set_challenge_key(chosen.key.data()); // 0x0004A3B0, EAX +0x348
            set_challenge_selection(selection.selected); // 0x000D5860
            if (selection.selected == kLast)
            {
                hold_runtime_challenge_order(s.image.string(0x003064F0), s.profile.scene_flags->bytes[0], s.flags,
                    [&s](const char* order)
                    {
                        hold_runtime_scene_order(order, s); // 0x001234B0
                    }); // 0x00049B60
            }
            size_t at = 0;
            for (; at < 0x3F && w.name[at] != 0; ++at)
            {
                g_challenge_game_name[at] = w.name[at]; // lstrcpyW(DAT_004E19B4, +8)
            }
            g_challenge_game_name[at] = 0;
            queue_runtime_gameselect_script(*w.owner, entry(w, selection.selected).script.data(), 3,
                s); // 0x00083B90(3), ECX +0, EAX +0x2C8
        }
        break;
    }
    default:
        break;
    }
}

// Xbox 0x000D7640
void select_runtime_challenge(GameChallengesWidget& w, i32 index, i32 move, GameSelectControlServices& services)
{
    auto& s = services;
    auto& selection = g_challenge_selection;
    i32 first = index - 2;
    while (first < 0)
    {
        first += w.count;
    }
    if (index != selection.selected)
    {
        selection.previous = selection.selected;
        selection.selected = index;
    }
    set_challenge_thresholds(entry(w, index).thresholds); // DAT_004E17DC .. DAT_004E17E8
    release_entry_textures(w, s); // 0x000D6F90
    for (auto& shown : w.shown)
    {
        while (w.count <= first)
        {
            first -= w.count;
        }
        auto& challenge = entry(w, first);
        shown = &challenge;
        if (challenge.texture.data() == nullptr)
        {
            challenge.texture = acquire_named(challenge.texture_name.data(), s);
        }
        ++first;
    }
    if (move != 0)
    {
        set_runtime_game_challenges_state(w, 3, s);
    }
}

// Xbox 0x000D7770: A plays the selected challenge, once it is unlocked.
void play_runtime_challenge(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = open_widget(s);
    if (w.shown[2]->unlocked == 0)
    {
        return;
    }
    w.chosen = 1;
    set_runtime_game_challenges_state(w, 5, s);
    if (read_u32(s.frontend.input, 0) == 3) // *DAT_00429BBC
    {
        const i32 port = runtime_input_new_edge_port(0xFFFF, s.input); // 0x00107F50, AX 0xFFFF
        set_runtime_player_port(port, 0x10, s.port_masks); // 0x00109780(0x10), EAX the port
    }
}

// Xbox 0x000D77C0: the stick turns the carousel, while the bat is settled (state 1 or 3).
void move_runtime_challenge_cursor(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = open_widget(s);
    if (w.bat == nullptr)
    {
        return;
    }
    const i32 bat = s.bat_screen(*w.bat).state; // +0x934
    if (bat != 1 && bat != 3)
    {
        return;
    }
    std::array<float, 2> direction{};
    s.frontend_direction(direction); // 0x001429E0, ESI DAT_00429BBC
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return;
    }
    i32 index = g_challenge_selection.selected;
    const u16 pressed = gameselect_direction_code(direction, s); // 0x001080B0
    if (pressed == 4)
    {
        g_challenge_selection.moved_left = 1;
        --index;
    }
    else if (pressed == 8)
    {
        g_challenge_selection.moved_left = 0;
        ++index;
    }
    else
    {
        return;
    }
    while (w.count <= index)
    {
        index -= w.count;
    }
    while (index < 0)
    {
        index += w.count;
    }
    select_runtime_challenge(w, index, 1, s); // 0x000D7640(1)
}

// Xbox 0x000D7890: B goes back to the contents (row 3), the book turning back.
void leave_runtime_game_challenges(GameSelectControlServices& services)
{
    auto& s = services;
    request_row(3, 3, 1, s); // 0x00048260(1, 1, 0)
    set_runtime_game_challenges_state(open_widget(s), 6, s);
}

// Xbox 0x000D78E0: Y shows the selected challenge's intro page.
void show_runtime_challenge_intro(GameSelectControlServices& services)
{
    auto& s = services;
    set_runtime_game_challenges_state(open_widget(s), 4, s);
}

// Xbox 0x000D78F0: Black opens the bonus books (row 11), once the save is past the first scene.
void open_runtime_challenge_bonus_books(GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = open_widget(s);
    char script[0x80]{};
    save_game_script(w.name.data(), script); // 0x0010DB10, EBX +8
    if (0 < gameselect_scene_ordinal(script, s)) // 0x0011ED30
    {
        request_row(0xB, 2, 0, s); // 0x00048260(0, 1, 0)
        set_runtime_game_challenges_state(w, 6, s);
        return;
    }
    if (s.cutscenes.dialogue == nullptr)
    {
        throw Error("the challenges' bonus books page: the scene dialogue (0x000582E0) is not bound");
    }
    auto& dialogue = *s.cutscenes.dialogue;
    if (i32(dialogue.state.count) + 1 < 9) // DAT_004EE1E8
    {
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", "errormessages_notpastsceneone",
            dialogue.measurement.wide_scratch, dialogue.measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, 0, dialogue); // 0x000582E0
    }
}
}
