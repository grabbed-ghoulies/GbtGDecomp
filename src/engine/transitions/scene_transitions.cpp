#include "scene_transition.h"
#include "scene_transitions.h"
#include "native_book.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#include "../runtime/rare_replay.h"
#endif
#include "book_map.h"
#include "../audio/event_voices.h"
#include "../audio/listener_settings.h"
#include "../camera/actor_camera_effects.h"
#include "../camera/camera.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/instance_construction.h"
#include "../frontend/dialogue_draw_context.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../frontend/dialogue_widget_bat.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_update.h"
#include "../frontend/input_activation.h"
#include "../runtime/name_lookup.h"
#include "../scene/executable_resources.h"
#include "../scene/scene.h"
#include "../scene/scene_types.h"
#include "../scripts/script_actions.h"
#include "../../gbtg/text/localtext_resources.h"
#include <cstdio>
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

#include <bit>
#include <cstring>
#include <limits>

namespace gbtg
{
using namespace scene_detail;

RuntimeSceneTransition g_scene_transition;
EntityCamNode g_transition_camera{};
SceneTransitionServices* g_transition_services = nullptr;

i32& g_546328 = g_scene_transition.row;
i32& g_54632c = g_scene_transition.phase;
i32 g_50484c = 0; // DAT_0050484C
i32 g_50e7a0 = 0; // DAT_0050E7A0
i32 g_510cec = 0; // DAT_00510CEC
i32 g_4dea24 = 0; // DAT_004DEA24
i32& g_510f80 = *reinterpret_cast<i32*>(g_510e58 + 0x128);
i32& g_510f7c = *reinterpret_cast<i32*>(g_510e58 + 0x124);
i32 g_507df4 = 0;
i32 g_510ce8 = 0;
i32 g_510e34 = 0;
// DAT_005493DC: .bss; the player's death (0x0008F4D0) sets it and 0x0012C2D0 tests it.
u32 g_5493dc = 0;
char g_50e458[0x80]{};
Game* g_transition_game = nullptr;

namespace
{
constexpr u32 kRows = 0x00431478; // Stride 0x110.
constexpr u32 kHandlers = 0x003129A8; // 4 records of 0x18: init, tick, draw, end, begin, exit.
constexpr u32 kDirectory = 0x0042AC80; // Stride 0x90: the scene name, its row (+0x80) and route (+0x84).
constexpr u32 kRet = 0x001628B0;

SceneTransitionServices& services()
{
    if (g_transition_services == nullptr)
    {
        throw Error("the scene transition has no services: the boot scene services are not built");
    }
    return *g_transition_services;
}

u32 image_word(u32 address)
{
    return read_u32(services().image.memory(address, 4), 0);
}

float image_real(u32 address)
{
    return std::bit_cast<float>(image_word(address));
}

u32 row_address(i32 row)
{
    return kRows + u32(row) * 0x110;
}

i32 row_handler(i32 row)
{
    return i32(image_word(row_address(row)));
}

const char* row_aid(i32 row, u32 offset)
{
    const u32 address = row_address(row) + offset;
    if (image_word(address) != kAidPrefixEd8)
    {
        return nullptr;
    }
    return services().image.string(address);
}

// The time counts down by DAT_005096C0.
void count_down(RuntimeSceneTransition& t, const SceneTransitionServices& s)
{
    if (0.f < t.time)
    {
        t.time = t.time - s.dt;
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // Rare Replay: a 1/60 s step can land a duration exactly on zero, which the handlers read as no timer
        // (0x0012BC80 waits for time <= 0 && time != 0) and never leave; retail's 1/30 s steps always pass below it.
        if (t.time == 0.f && rare_replay_enabled())
        {
            t.time = -std::numeric_limits<float>::min();
        }
#endif
    }
}

// The phase change every 0x0012C5D0-family function opens with.
void leave_phase(i32 phase)
{
    if (phase == 0)
    {
        g_510cec = g_510cec + 1;
        g_4dea24 = g_4dea24 + 1;
        g_flags_ea9c = g_flags_ea9c | 0x100u;
    }
    else if (phase == 1)
    {
        g_510f80 = 0;
    }
    else if (phase == 3)
    {
        g_510f7c = 0;
    }
}

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

// Xbox 0x0012C830, ECX the transition, EDX the phase.
void set_transition_phase(RuntimeSceneTransition& t, i32 phase)
{
    leave_phase(t.phase);
    t.phase = phase;
    if (phase == 0)
    {
        g_510cec = g_510cec - 1;
        g_4dea24 = g_4dea24 - 1;
        g_flags_ea9c = g_flags_ea9c & 0xFFFFFEFFu;
    }
}

// Xbox 0x00116220, EAX the scene: its row of the directory at 0x0042AC80, or 0. A pending override name
// (DAT_0050E458) is used instead, and cleared.
i32 find_scene_directory_row(const char* scene)
{
    char name[0x80]{};
    if (std::memcmp(g_50e458, "aid_", 4) == 0)
    {
        std::strcpy(name, g_50e458);
        g_50e458[0] = 0;
    }
    else
    {
        std::strcpy(name, scene);
    }
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // A plugin's scene has no row; its recipe names a disc scene to use the row of (docs/PLUGINS.md).
    if (const char* alias = plugin_scene_transition(name))
    {
        std::snprintf(name, sizeof(name), "%s", alias);
    }
#endif
    for (u32 index = 0; index * 0x90 < 0x5E80; ++index)
    {
        if (transition_aid_equal(name, services().image.string(kDirectory + index * 0x90))) // 0x000357D0
        {
            return i32(index);
        }
    }
    return 0;
}

// Xbox 0x0012C2D0, EAX the scene, EDI the clip, ESI the route: the scene's transition row.
i32 select_transition_row(const char* scene, i32& clip, i32& route)
{
    if (g_5493dc != 0)
    {
        clip = i32(image_word(row_address(2) + 0x108)); // DAT_004317A0
        g_5493dc = 0;
        route = 0;
        return 2;
    }
    const i32 entry = find_scene_directory_row(scene); // 0x00116220
    if (entry != -1)
    {
        const i32 row = i32(image_word(kDirectory + u32(entry) * 0x90 + 0x80));
        clip = i32(image_word(row_address(row) + 0x108));
        route = i32(image_word(kDirectory + u32(entry) * 0x90 + 0x84));
        return row;
    }
    clip = 0;
    route = 0;
    return 0;
}

// The first and second page thresholds of the book's clip row (0x0042217C, 0x00422180) against the book's
// cutscene time; the page index the row names (0x00422184, 0x00422188) is captured next draw.
float book_position(const RuntimeTransitionBook& book)
{
    return book.cutscene->time / book.cutscene->duration; // book +0x24: the cutscene's +0x2C over +0x34
}

void capture_book_page(RuntimeTransitionBook& book, const SceneTransitionServices& s)
{
    const float position = book_position(book);
    const float first = transition_book_threshold(book, 0x104, s.image);
    i32 page = -1;
    if (position < first || position == first)
    {
        page = transition_book_row_word(book, 0x10C, s.image);
    }
    else
    {
        const float second = transition_book_threshold(book, 0x108, s.image);
        if (second < position && position != second)
        {
            page = transition_book_row_word(book, 0x110, s.image);
        }
        else
        {
            return;
        }
    }
    if (page != -1)
    {
        book.pages[size_t(page)].capture = 1;
    }
}

// Handler 0, the fade.
// Xbox 0x0012BC80
i32 tick_fade(RuntimeSceneTransition& t)
{
    auto& s = services();
    count_down(t, s);
    if (t.phase == 1)
    {
        if (t.time <= 0.f && t.time != 0.f)
        {
            g_510f80 = 0;
            t.phase = 2;
        }
    }
    else if (t.phase == 3 && t.time <= 0.f && t.time != 0.f)
    {
        set_transition_phase(t, 0); // 0x0012C830
        return t.phase;
    }
    return t.phase;
}

// Xbox 0x0012BCE0
void begin_fade(RuntimeSceneTransition& t)
{
    const float duration = image_real(0x0032B150);
    leave_phase(t.phase);
    t.phase = 1;
    t.duration = duration;
    t.time = duration;
}

// Xbox 0x0012BD50
void exit_fade(RuntimeSceneTransition& t)
{
    const float duration = image_real(0x0032B0F4);
    leave_phase(t.phase);
    t.phase = 3;
    t.duration = duration;
    t.time = duration;
}

// Handler 1, the book.
// Xbox 0x0012B360
void initialize_book_handler(RuntimeSceneTransition& t)
{
    t.book = nullptr;
}

// 0x00142460 over the title prompts, with the title's input device (EAX +0x3048) or none (EAX 0).
u32 update_title_prompts(RuntimeSceneTransition& t, bool with_device)
{
    auto& s = services();
    s.widget_update.input.devices = s.input.entries;
    s.widget_update.input.selected = s.input.selected;
    s.widget_update.input.device_views = &s.input_views;
    if (!with_device || t.title_input.data() == nullptr)
    {
        return update_dialogue_widget(*t.title_widget, nullptr, s.widget_update);
    }
    const RuntimeDialogueInputDevice device{t.title_input, {nullptr, nullptr, nullptr, nullptr}};
    return update_dialogue_widget(*t.title_widget, &device, s.widget_update);
}

// Xbox 0x0012B810, EBX the transition
void show_chapter_title(RuntimeSceneTransition& t)
{
    auto& s = services();
    const float width = float(s.screen[0]);
    const float height = float(s.screen[1]);
    // The chapter number at 10% down and the name at 60%, centred, sticky (EAX 1).
    create_dialogue_text_record(*t.title_pool, t.chapter_number.data(), width * 0.5f, height * 0.1f, 1.2f, 1.2f,
        7.5f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0
    create_dialogue_text_record(*t.title_pool, t.chapter_name.data(), width * 0.5f, height * 0.6f, 0.9f, 0.9f,
        7.5f, 1, 0, 1, s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0
    if (t.title_input.data() == nullptr)
    {
        t.title_input = create_runtime_selected_input(s.input_release); // 0x00107D60
        // Native pointer mapping; the retail entry stores the allocated address.
        t.title_input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(t.title_input.data()));
        s.input_addresses[t.title_input_address] = t.title_input;
        s.input_views[t.title_input_address] = {t.title_input, {nullptr, nullptr, nullptr, nullptr}};
        push_runtime_input_entry(t.title_input_address, s.image.string(0x0032A93C), s.input); // 0x00109340
    }
    if (t.title_widget == nullptr)
    {
        // Anchor (DAT_005096E8 * 0.5, DAT_0050974C - 16), rows (0, -72), origin (40, -36).
        create_resource_dialogue_widget(t.title_widget, 0, s.image.memory(0x00312698, 0x310).data(),
            {width * 0.5f, s.viewport_bottom - 16.f}, {0.f, -72.f}, {40.f, -36.f}, nullptr, 1.5f, 0.75f,
            s.image.string(0x003225D0), s.image.string(0x003225E8), 0x400C0, s.widget_allocation); // 0x00141DA0
        attach_dialogue_widget_bat(*t.title_widget, 2.f, 0, s.widget_bats); // 0x00142860
        const char* sound = s.image.string(0x004312E8);
        std::snprintf(t.title_widget->sound_name.data(), t.title_widget->sound_name.size(), "%s", sound);
    }
    t.title_ready = 1;
}

// Xbox 0x0012B370
i32 tick_book_handler(RuntimeSceneTransition& t)
{
    auto& s = services();
    if (t.phase == 1)
    {
        tick_transition_book(*t.book, s.books); // 0x000483E0
        if (t.book->finished != 0)
        {
            if (t.titled != 0)
            {
                if (t.title_ready == 0)
                {
                    show_chapter_title(t); // 0x0012B810
                }
                refresh_runtime_input_device(t.title_input_address, s.frame(), s.input, s.input_release); // 0x00109470
                if (update_title_prompts(t, true) == 0) // 0x00142460, EAX +0x3048
                {
                    return t.phase;
                }
                if (t.titled != 0 && t.title_ready != 0)
                {
                    if (t.title_input.data() != nullptr)
                    {
                        pop_runtime_input_entry(s.input); // 0x001093C0
                        release_runtime_input_device(t.title_input, s.input_release); // 0x00108230
                        t.title_input = {};
                        t.title_input_address = 0;
                    }
                    if (t.title_widget != nullptr)
                    {
                        t.title_widget->requested_set = nullptr; // +0x950
                        set_dialogue_widget_mode(*t.title_widget, 3, s.widget_allocation.widget); // 0x00142C20
                    }
                    end_all_dialogue_text_records(*t.title_pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
                    t.titled = 0;
                }
            }
            set_transition_phase(t, 2); // 0x0012C830
        }
        return t.phase;
    }
    if (t.phase == 2)
    {
        if (t.title_widget != nullptr)
        {
            update_title_prompts(t, false); // 0x00142460, EAX 0
        }
        return t.phase;
    }
    if (t.phase != 3)
    {
        return t.phase;
    }
    if (t.title_widget != nullptr)
    {
        update_title_prompts(t, false); // 0x00142460, EAX 0
    }
    const float position = book_position(*t.book);
    const float second = transition_book_threshold(*t.book, 0x108, s.image);
    if (second <= position && position != second && t.title_released == 0)
    {
        g_50484c = g_50484c - 1;
        t.title_released = 1;
    }
    {
        const float first = transition_book_threshold(*t.book, 0x104, s.image);
        i32 page = -1;
        if (position < first || position == first)
        {
            page = transition_book_row_word(*t.book, 0x10C, s.image);
        }
        else if (second <= position && position != second)
        {
            page = transition_book_row_word(*t.book, 0x110, s.image);
        }
        if (page != -1)
        {
            t.book->pages[size_t(page)].capture = 1;
        }
    }
    tick_transition_book(*t.book, s.books); // 0x000483E0
    if (t.book->finished != 0)
    {
        set_transition_phase(t, 0); // 0x0012C830
    }
    return t.phase;
}

// Xbox 0x0012B560
void draw_book_handler(RuntimeSceneTransition& t)
{
    auto& s = services();
    if (t.book != nullptr)
    {
        draw_transition_book(*t.book, s.books); // 0x00048550
    }
    if (t.title_widget != nullptr)
    {
        draw_runtime_dialogue_widget(*t.title_widget, 1, s.widget_draw); // 0x00142720
    }
}

// Xbox 0x0012B5A0
void end_book_handler(RuntimeSceneTransition& t)
{
    auto& s = services();
    if (t.title_widget != nullptr)
    {
        destroy_dialogue_widget(t.title_widget, s.resources, *s.widget_allocation.pools); // 0x00142070
        t.title_widget = nullptr;
    }
    if (t.title_input.data() != nullptr)
    {
        switch (read_u32(t.title_input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(t.title_input); // 0x00107DD0
            break;
        case 4:
            release_runtime_composite_input(t.title_input, s.input_release); // 0x00107960
            break;
        case 5:
            release_runtime_actor_input(t.title_input, s.input_release); // 0x00107B60
            break;
        default:
            break;
        }
        t.title_input = {};
        t.title_input_address = 0;
    }
#if defined(GBTG_INSPECTOR_APP)
    // An inspector load can stop before create_transition_book returns an owner.
    if (t.book != nullptr)
    {
#endif
    release_transition_book(t.book, s.books); // 0x00048860
#if defined(GBTG_INSPECTOR_APP)
    }
#endif
    t.book = nullptr;
    if (t.title_pool != nullptr)
    {
        release_dialogue_text_pool(t.title_pool, *s.text_pools); // 0x0005F460
    }
}

// The chapter's heading under the key in aid_loctext_ghoulies_chapterheadings, in DAT_00507E78.
const char16_t* chapter_heading(const char* key, i32 chapter, LocalTextResourceContext& localtext)
{
    static std::array<char16_t, 0x800> scratch{}; // DAT_00507E78
    char name[0x100];
    std::snprintf(name, sizeof(name), "%d", chapter); // 0x001661F9, "%d" at 0x00322D20
    scratch[0] = 0;
    request_namespaced_localtext("aid_loctext_ghoulies_chapterheadings", key, name, scratch.data(),
        localtext); // 0x000FF0A0
    return scratch.data();
}

// Xbox 0x0004B550, EAX chapter
const char16_t* chapter_number_heading(i32 chapter, LocalTextResourceContext& localtext)
{
    return chapter_heading("chapternumber", chapter, localtext);
}

// Xbox 0x0004B5A0, EAX chapter
const char16_t* chapter_name_heading(i32 chapter, LocalTextResourceContext& localtext)
{
    return chapter_heading("chaptername", chapter, localtext);
}

// lstrcpyW (0x0014A961) into a transition string.
void copy_wide(std::array<char16_t, 0x800>& output, const char16_t* text)
{
    size_t index = 0;
    for (; text[index] != 0 && index + 1 < output.size(); ++index)
    {
        output[index] = text[index];
    }
    output[index] = 0;
}

// Xbox 0x0012B640
void begin_book_handler(RuntimeSceneTransition& t)
{
    auto& s = services();
    t.book = create_transition_book(t.clip, 0, t.aid, s.books); // 0x00048260
    auto& book = *t.book;
    switch (t.clip)
    {
    case 0:
    case 1:
    case 0xD:
    case 0xE:
        t.titled = 0;
        book.holding = 1;
        break;
    case 2:
        t.titled = 1;
        t.title_ready = 0;
        book.holding = 1;
        copy_wide(t.chapter_number, chapter_number_heading(t.route, s.localtext)); // 0x0004B550
        copy_wide(t.chapter_name, chapter_name_heading(t.route, s.localtext)); // 0x0004B5A0
        allocate_dialogue_text_pool(t.title_pool, 0, 1, 0x80, s.text_pools); // 0x0005F390, ESI +0x103C
        capture_book_page(book, s);
        return;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        t.titled = 0;
        book.holding = 1;
        book.route = 1;
        book.route_argument = 0;
        book.map_from[0] = 0; // +0x118
        book.map_to[0] = 0; // +0x138
        book.route_argument = t.route;
        break;
    default:
        capture_book_page(book, s);
        return;
    }
    t.chapter_name[0] = 0;
    t.chapter_number[0] = 0;
    t.title_pool = nullptr;
    capture_book_page(book, s);
}

// Xbox 0x0012B790
void exit_book_handler(RuntimeSceneTransition& t)
{
    auto& s = services();
    auto& book = *t.book;
    book.holding = 0;
    close_book_map(book, s.books); // 0x00047370, ESI the book
    capture_book_page(book, s);
    g_50484c = g_50484c + 1;
    t.title_released = 0;
}

// Handler 2, the slime.
// Xbox 0x0012BDC0
void initialize_slime(RuntimeSceneTransition& t)
{
    t.slime = {};
}

// Xbox 0x0012BDD0
i32 tick_slime(RuntimeSceneTransition& t)
{
    auto& s = services();
    const float previous = t.time;
    count_down(t, s);
    if (t.phase == 1)
    {
        if (t.time <= 0.f)
        {
            g_510f80 = 0;
            t.phase = 2;
        }
    }
    else if (t.phase == 3)
    {
        if (t.time <= 0.f && t.time != 0.f)
        {
            set_transition_phase(t, 0); // 0x0012C830
            return t.phase;
        }
        const float off = t.duration * image_real(0x0032B244);
        if (off < previous && t.time <= off)
        {
            request_spatial_audio_event_voice(services().image.string(0x00431370), 1, 0.f, s.voices); // 0x00039510
            return t.phase;
        }
    }
    return t.phase;
}

// The slime frame 0x0012BE60 truncates (0x001673D8): the time's share of the frames, clamped.
float slime_frame(float value, float limit)
{
    if (0.f < limit)
    {
        if (value > limit)
        {
            return limit;
        }
        if (0.f > value)
        {
            return 0.f;
        }
        return value;
    }
    if (limit > value)
    {
        return limit;
    }
    if (value > 0.f)
    {
        return 0.f;
    }
    return value;
}

// Xbox 0x0012BE60
void draw_slime(RuntimeSceneTransition& t)
{
    auto& s = services();
    const auto& view = *entity_detail::g_display_view; // DAT_005077A8
    const float view_height = float(view.fitted_height); // +0x64
    const float view_width = float(view.fitted_width); // +0x60
    const float screen_width = float(i32(s.screen[0]));
    const float half = image_real(0x0032B0F4);
    const float one = image_real(0x0032B0F8);
    std::array<float, 2> position{screen_width * half - one, view_height * half - one};
    std::array<float, 2> scale{};
    u8 frame = 0;
    const auto frames = [&t]()
    {
        return float(i32(t.slime[0x11]));
    };
    const auto width = [&t]()
    {
        return float(i32(read_u16(t.slime, 8)));
    };
    const auto height = [&t]()
    {
        return float(i32(read_u16(t.slime, 10)));
    };
    if (t.phase == 1)
    {
        if (t.slime_captured == 0)
        {
            s.copy_back_buffer(s.address_of(s.page.data())); // DAT_005047DC's surface level 0
            t.slime_captured = 1;
        }
        std::array<float, 2> page_scale{screen_width / view_width, image_real(0x004313F4)}; // DAT_004313F0
        begin_dialogue_sprite_draw(s.draw); // 0x0005CDC0
        draw_dialogue_texture(0, nullptr, s.page.data(), page_scale.data(), nullptr, 0.f, image_word(0x00314F48),
            position.data(), s.draw); // 0x0005CA60, DAT_005047D8
        --s.draw.override_count;
        if (s.draw.override_count == 0)
        {
            s.draw.fog_disabled = 0;
        }
        const float last = frames() - one;
        float value = 0.f;
        if (t.duration != 0.f)
        {
            value = slime_frame(t.time / t.duration * (0.f - last) + last, last);
        }
        frame = u8(truncate_runtime_float(value));
        scale = {view_width / width(), view_height / height()};
    }
    else if (t.phase == 2)
    {
        frame = u8(t.slime[0x11] - 1);
        scale = {view_width / width(), view_height / height()};
    }
    else if (t.phase == 3)
    {
        const float count = frames();
        float value = count;
        if (t.duration != 0.f)
        {
            value = slime_frame(t.time / t.duration * count, count);
        }
        frame = u8(truncate_runtime_float(value));
        scale = {0.f - view_width / width(), image_real(0x0032B120) / height() * view_height};
    }
    else
    {
        untranslated("0x0012BE60 outside phases 1 to 3 (its frame is an uninitialised stack byte)");
    }
    begin_dialogue_sprite_draw(s.draw); // 0x0005CDC0
    draw_dialogue_texture(frame, nullptr, const_cast<u8*>(t.slime.data()), scale.data(), nullptr, 0.f, t.slime_color,
        position.data(), s.draw); // 0x0005CA60
    --s.draw.override_count;
    if (s.draw.override_count == 0)
    {
        s.draw.fog_disabled = 0;
    }
}

// Xbox 0x0012C1A0
void end_slime(RuntimeSceneTransition& t)
{
    auto& s = services();
    release_animation_resource(s.resources, t.slime); // 0x00035C00
    unload_animation_resource(s.resources, t.slime); // 0x00035C60
    t.slime = {};
}

std::span<const u8> acquire_slime(const char* aid)
{
    auto& s = services();
    const auto texture = acquire_animation_resource(s.resources, aid); // 0x00035E90
    if (texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700
    }
    return texture;
}

// Xbox 0x0012C1D0
void begin_slime(RuntimeSceneTransition& t)
{
    auto& s = services();
    t.slime_color = u32(t.clip);
    t.slime = t.aid != nullptr ? acquire_slime(t.aid) : std::span<const u8>{};
    g_50484c = g_50484c + 1;
    t.slime_captured = 0;
    const i16 voice = allocate_audio_event_voice(1, s.voices); // 0x00039830, EBX 1
    if (voice != -1)
    {
        s.voices.voices[size_t(voice)].stop = 1; // DAT_004E0CEC
        position_audio_event_voice_ahead(voice, image_real(0x0032B184), s.voices); // 0x00039B30
        prepare_audio_event_voice(voice, s.image.string(0x004313F8), s.voices); // 0x00039A20
    }
}

// Xbox 0x0012C270
void exit_slime(RuntimeSceneTransition& t)
{
    auto& s = services();
    t.slime_color = u32(t.clip);
    if (t.slime.data() != nullptr)
    {
        release_animation_resource(s.resources, t.slime); // 0x00035C00
    }
    if (t.aid == nullptr)
    {
        t.slime = {};
        return;
    }
    t.slime = acquire_slime(t.aid);
}

// Handler 3 (row 16), which no scene's directory row names.
[[noreturn]] void handler_three(u32 slot)
{
    char detail[96];
    std::snprintf(detail, sizeof(detail), "transition handler 3's slot 0x%08X (row 16)", slot);
    untranslated(detail);
}

// The handler record's slot (0x003129A8 + handler * 0x18 + slot * 4), as the indirect call reaches it.
i32 call_handler(i32 handler, u32 slot)
{
    auto& t = g_scene_transition;
    const u32 address = image_word(kHandlers + u32(handler) * 0x18 + slot * 4);
    switch (address)
    {
    case kRet:
        return 0;
    case 0x0012BC80:
        return tick_fade(t);
    case 0x0012BCE0:
        begin_fade(t);
        return 0;
    case 0x0012BD50:
        exit_fade(t);
        return 0;
    case 0x0012B360:
        initialize_book_handler(t);
        return 0;
    case 0x0012B370:
        return tick_book_handler(t);
    case 0x0012B560:
        draw_book_handler(t);
        return 0;
    case 0x0012B5A0:
        end_book_handler(t);
        return 0;
    case 0x0012B640:
        begin_book_handler(t);
        return 0;
    case 0x0012B790:
        exit_book_handler(t);
        return 0;
    case 0x0012BDC0:
        initialize_slime(t);
        return 0;
    case 0x0012BDD0:
        return tick_slime(t);
    case 0x0012BE60:
        draw_slime(t);
        return 0;
    case 0x0012C1A0:
        end_slime(t);
        return 0;
    case 0x0012C1D0:
        begin_slime(t);
        return 0;
    case 0x0012C270:
        exit_slime(t);
        return 0;
    case 0x0012BA10:
    case 0x0012BC70:
        handler_three(address);
    default:
        untranslated("transition handler slot");
    }
}
}

// Xbox 0x0011C3F0's loop: each handler record's +0 slot over the transition.
void initialize_scene_transition()
{
    for (i32 handler = 0; handler < 4; ++handler)
    {
        call_handler(handler, 0);
    }
}

// Xbox 0x0012C340
void tick_scene_transition()
{
    auto& t = g_scene_transition;
    auto& s = services();
    if (0 < t.phase && t.phase < 4 && image_word(row_address(t.row) + 0x10C) != 0 && t.loading_pool != nullptr)
    {
        const auto* allocation = t.loading_pool->allocation;
        if (allocation->limit <= allocation->cursor)
        {
            create_dialogue_text_record(*t.loading_pool, t.loading_text.data(),
                float(i32(s.screen[0])) * image_real(0x0032B0F4), float(i32(s.screen[1])) * image_real(0x0032B244),
                1.5f, 1.5f, 1.5f, 1, 0, 0, s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F580
            create_dialogue_text_record(*t.loading_pool, u"spam", -100.f, -100.f, 0.1f, 0.1f, 0.5f, 0, 0, 0,
                s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F550
        }
    }
    if (t.phase == 1)
    {
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // Rare Replay: the delay counts retail ticks.
        if (t.flag_delay != 0 && rare_replay_retail_tick())
#else
        if (t.flag_delay != 0)
#endif
        {
            t.flag_delay = t.flag_delay - 1;
            if (t.flag_delay == 0)
            {
                g_flags_ea9c = g_flags_ea9c | 0x2000u;
            }
        }
        if (g_50484c < 1 && 0 < t.loading_frames)
        {
            g_50484c = g_50484c + 1;
        }
        else
        {
            t.loading_frames = t.loading_frames + 1;
        }
        call_handler(row_handler(t.row), 1);
    }
    else if (t.phase == 3)
    {
        call_handler(row_handler(t.row), 1);
    }
}

// Xbox 0x0012C490
void draw_scene_transition_view()
{
    auto& t = g_scene_transition;
    auto& s = services();
    if (t.phase != 0 && s.dialogue_mode() == 0)
    {
        setup_explicit_camera_display(&g_transition_camera, 0, s.display); // 0x00048BC0, ESI DAT_0054937C
        call_handler(row_handler(t.row), 2);
    }
}

// Xbox 0x0012C4E0
void end_scene_transition()
{
    auto& t = g_scene_transition;
    auto& s = services();
    call_handler(row_handler(t.row), 3);
    camera_unlink_view(&g_transition_camera); // 0x00048C70, EAX DAT_0054937C
    if (t.phase == 0)
    {
        g_510cec = g_510cec + 1;
        g_4dea24 = g_4dea24 + 1;
    }
    else if (t.phase == 1)
    {
        g_510f80 = 0;
    }
    else if (t.phase == 3)
    {
        g_510f7c = 0;
    }
    g_510cec = g_510cec - 1;
    g_4dea24 = g_4dea24 - 1;
    g_flags_ea9c = g_flags_ea9c & 0xFFFFFEFFu;
    t.phase = 0;
    if (t.loading_pool != nullptr)
    {
        release_dialogue_text_pool(t.loading_pool, *s.text_pools); // 0x0005F460
        g_flags_ea9c = g_flags_ea9c & 0xFFFFDFFFu;
        t.loading_pool = nullptr;
    }
    g_4dea24 = g_4dea24 - 1;
}

// Xbox 0x0012C5D0
void begin_scene_transition(const char* scene)
{
    auto& t = g_scene_transition;
    auto& s = services();
    if (t.phase != 0)
    {
        end_scene_transition(); // 0x0012C4E0
    }
    leave_phase(t.phase);
    t.phase = 1;
    t.row = select_transition_row(scene, t.clip, t.route); // 0x0012C2D0, EAX DAT_00510ED8
    t.time = image_real(row_address(t.row) + 0x104);
    t.loading_frames = 0;
    t.flag_delay = 0xF;
    t.aid = row_aid(t.row, 4);
    t.duration = t.time;
    initialize_camera_view(g_transition_camera, s.image.string(0x0032A954), 0); // 0x000496C0, CL 0
    link_camera_view(0, g_transition_camera); // The inline 0x00048C20 link and 0x00048CF0 when slot 0 has none.
    release_actor_camera_effects(); // 0x00048F10
    allocate_dialogue_text_pool(t.loading_pool, 0, 1, 0x80, s.text_pools); // 0x0005F390, ESI DAT_0054634C
    request_namespaced_localtext("aid_loctext_ghoulies_gameselect_gameselecttext", "gameselect", "loading",
        t.loading_text.data(), s.localtext); // 0x000FF0A0 into DAT_00546350
    call_handler(row_handler(t.row), 4);
    g_4dea24 = g_4dea24 + 1;
}

// Xbox 0x0012C770
void leave_scene_transition()
{
    auto& t = g_scene_transition;
    leave_phase(t.phase);
    t.time = image_real(row_address(t.row) + 0x104);
    t.phase = 3;
    if (0 < g_50484c)
    {
        g_50484c = g_50484c - 1;
    }
    t.aid = row_aid(t.row, 0x84);
    t.duration = t.time;
    call_handler(row_handler(t.row), 5);
}

u32 g_5462e8 = 0;

// Xbox 0x001162D0, EAX the scene record
void select_scene_directory_row(const char* scene)
{
    g_5462e8 = kDirectory + u32(find_scene_directory_row(scene)) * 0x90; // 0x00116220
}

// Xbox 0x001162B0: the directory row's book clip (+0x88), or the first row's (DAT_0042AD08).
i32 scene_directory_book_clip()
{
    if (g_5462e8 != 0)
    {
        return i32(image_word(g_5462e8 + 0x88));
    }
    return i32(image_word(0x0042AD08));
}
}
