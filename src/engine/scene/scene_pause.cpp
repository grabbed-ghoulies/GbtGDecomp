#include "scene_pause.h"
#include "background_geometry.h"
#include "executable_resources.h"
#include "scene.h"
#include "../audio/bank_streams.h"
#include "../audio/event_voices.h"
#include "../cutscenes/event_pause.h"
#include "../cutscenes/event_runtime.h"
#include "../cutscenes/instance_construction.h"
#include "../frontend/dialogue_activation.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_parameters.h"
#include "../frontend/dialogue_widget_state.h"
#include "../frontend/dialogue_widget_update.h"
#include "../frontend/hud_layout.h"
#include "../frontend/input_activation.h"
#include "../entities/ghouly_spawn_record.h"
#include "../runtime/crt_format.h"
#include "../saves/runtime_profile.h"
#include "../transitions/native_book.h"
#include "../transitions/scene_transitions.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
extern i32 g_429bc0; // DAT_00429BC0, scene.cpp.
extern u16 g_429bc4[0x80]; // DAT_00429BC4, scene.cpp.
extern u16 g_4e19b4[0x40]; // DAT_004E19B4, scene.cpp.

ScenePause g_scene_pause = []()
{
    ScenePause pause;
    const char16_t name[] = u"No Name Available"; // 0x0011EA50
    std::memcpy(pause.title.data(), name, sizeof(name));
    return pause;
}();

namespace
{
[[noreturn]] void untranslated(const std::string& what)
{
    throw Error(what + " is not translated");
}

const SceneExecutableResources& image(ScenePauseServices& s)
{
    return s.books.image;
}

float real(ScenePauseServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(image(s).memory(address, 4), 0));
}

u32 word(ScenePauseServices& s, u32 address)
{
    return read_u32(image(s).memory(address, 4), 0);
}

// A wide string in the executable's data.
std::u16string wide(ScenePauseServices& s, u32 address)
{
    std::u16string text;
    for (u32 at = address;; at += 2)
    {
        const auto unit = static_cast<char16_t>(read_u16(image(s).memory(at, 2), 0));
        if (unit == 0)
        {
            return text;
        }
        text.push_back(unit);
    }
}

RuntimeTransitionBook& book(const char* where)
{
    if (g_scene_pause.book == nullptr)
    {
        throw Error(std::string(where) + " reads the pause book (DAT_00510E24) through address zero");
    }
    return *g_scene_pause.book;
}

RuntimeDialogueWidget& widget(const char* where)
{
    if (g_scene_pause.widget == nullptr)
    {
        throw Error(std::string(where) + " reads the pause prompts (DAT_00510E30) through address zero");
    }
    return *g_scene_pause.widget;
}

float book_position(const RuntimeTransitionBook& book)
{
    return book.cutscene->time / book.cutscene->duration; // Book +0x24: the cutscene's +0x2C over +0x34.
}

void pause_book_events(RuntimeTransitionBook& book, i32 paused, ScenePauseServices& s)
{
    auto& cutscenes = s.books.cutscenes;
    CutsceneEventPauseContext pause{cutscenes.instances.at(book.cutscene).actors.data(), cutscenes.animations,
        cutscenes.audio.voices, cutscenes.resume_callbacks};
    pause_cutscene_events(*book.cutscene, paused, pause); // 0x0004E640, EDI book +0x24
}

RuntimeDialogueWidgetContext& widget_context(ScenePauseServices& s)
{
    return s.books.widget_allocation->widget;
}

// The pause's input device as the widget and the button tests read it, or none.
const RuntimeDialogueInputDevice* input_device(RuntimeDialogueInputDevice& device)
{
    if (g_scene_pause.input.data() == nullptr)
    {
        return nullptr;
    }
    device = RuntimeDialogueInputDevice{g_scene_pause.input, {nullptr, nullptr, nullptr, nullptr}};
    return &device;
}

u32 update_prompts(RuntimeDialogueWidget& prompts, const RuntimeDialogueInputDevice* device, ScenePauseServices& s)
{
    auto& update = *s.books.widget_update;
    update.input.devices = s.books.input->entries;
    update.input.selected = s.books.input->selected;
    update.input.device_views = s.books.input_views;
    return update_dialogue_widget(prompts, device, update); // 0x00142460, EAX the device, EDI the widget
}

// Xbox 0x0013BA70: one of the ghoul laughs (0x00314768 back in 0x80 steps) on a free voice.
i16 play_random_ghoul_laugh(ScenePauseServices& s)
{
    u32& random = s.books.random; // DAT_0054DD7C
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
    AudioEventVoices& voices = s.books.voices;
    const i16 voice = allocate_audio_event_voice(0, voices); // 0x00039830, EBX 0
    if (voice != -1)
    {
        const i32 index = i32((unit - 1.f) * real(s, 0x0032B240)); // DAT_0032B0F8
        voices.voices[size_t(voice)].stop = 1; // DAT_004E0CEC
        prepare_audio_event_voice(voice, image(s).string(0x00314768u - u32(index) * 0x80u), voices); // 0x00039A20
    }
    return voice;
}

// Phase 3's entry (0x0011D020 in 0x0011CFB0, phase 3 of 0x0011D490): the book holds open.
void hold_pause_book_open(ScenePauseServices& s)
{
    RuntimeTransitionBook& open = book("0x0011D490 phase 3");
    seek_transition_book(open, std::bit_cast<float>(0x3DFBE76Du), s.books); // 0x00048940
    if (open.paused == 0)
    {
        pause_book_events(open, 1, s);
        open.paused = 1;
    }
}

// Phase 6's entry (0x0011D490 phase 6, inlined in 0x0011DEE0): the book closes and the prompts and text go.
void close_pause_book(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    RuntimeTransitionBook& closing = book("0x0011D490 phase 6");
    seek_transition_book(closing, std::bit_cast<float>(0x3F50A3D7u), s.books); // 0x00048940
    if (closing.paused != 0)
    {
        pause_book_events(closing, 0, s);
        closing.paused = 0;
    }
    play_random_ghoul_laugh(s); // 0x0013BA70
    RuntimeDialogueWidget& prompts = widget("0x0011D490 phase 6");
    prompts.requested_set = nullptr; // +0x950
    set_dialogue_widget_mode(prompts, 3, widget_context(s)); // 0x00142C20
    for (const u32 id : {p.line_text, p.paused_text, p.third_text})
    {
        end_dialogue_text_record(*p.text, id, s.books.fonts, s.books.fallback_texture, s.books.random); // 0x0005F7E0
    }
}

void set_prompt_row(RuntimeDialogueWidget& prompts, u32 address, ScenePauseServices& s)
{
    copy_dialogue_widget_parameters(prompts.requested_parameters, image(s).memory(address, 0x310).data()); // 0x00142DF0
    prompts.requested_set = prompts.requested_parameters.data(); // +0x950 = +0x638
    set_dialogue_widget_mode(prompts, 3, widget_context(s)); // 0x00142C20
}

void leave_to_gameselect(i32 reason)
{
    char aid[0x80]{};
    std::strcpy(aid, "aid_script_ghoulies_gameselect_gameselect"); // 0x00322B8C
    g_429bc0 = reason;
    size_t index = 0;
    do
    {
        g_429bc4[index] = g_4e19b4[index]; // lstrcpyW (0x0014A961)
    }
    while (g_4e19b4[index++] != 0);
    request_scene_aid(aid, 1); // 0x0011EB30
}
}

// Xbox 0x0011EBA0
void request_scene_resume(ScenePauseServices& s)
{
    if (g_510cec == 0)
    {
        s.queued_state = g_510ce8 != 0 ? 1 : 6; // DAT_00510F60
    }
}

// Xbox 0x0011DD10, EAX the row
void select_scene_pause_prompt(i32 prompt, ScenePauseServices& s)
{
    g_scene_pause.prompt = prompt;
    RuntimeDialogueWidget& prompts = widget("0x0011DD10");
    if (prompt == -1)
    {
        prompts.requested_set = nullptr;
        set_dialogue_widget_mode(prompts, 3, widget_context(s)); // 0x00142C20
        return;
    }
    const u32 address = u32(prompt) * 0x310u + 0x0030F948u;
    if (address != 0)
    {
        set_prompt_row(prompts, address, s);
        return;
    }
    prompts.requested_set = nullptr;
    set_dialogue_widget_mode(prompts, 3, widget_context(s));
}

// Xbox 0x0011DD90: the prompt row for the game's state. Rows 0 .. 7 are by the event state (DAT_005064D8 3), the
// scene flags' bit 1 and the story's bit 0x23 without DAT_0050EA9C 0x4000; the front end's state 3 takes row 8.
void choose_scene_pause_prompt(ScenePauseServices& s)
{
    if (s.frontend_state == 3) // DAT_004E17C8
    {
        g_scene_pause.prompt = 8;
        set_prompt_row(widget("0x0011DD90"), 0x003111C8, s);
        return;
    }
    const bool flagged = (s.books.scene_flags & 2) != 0; // **(DAT_00506C74+8)
    const bool story = (s.books.story_bits[4] & 8) != 0 && (s.books.flags & 0x4000u) == 0; // *(DAT_00506C6C+8)+4
    i32 prompt;
    if (s.event_state == 3) // DAT_005064D8
    {
        if (flagged)
        {
            prompt = story ? 3 : 2;
        }
        else
        {
            prompt = story ? 7 : 6;
        }
    }
    else if (flagged)
    {
        prompt = story ? 1 : 0;
    }
    else
    {
        prompt = story ? 5 : 4;
    }
    select_scene_pause_prompt(prompt, s); // 0x0011DD10, EAX the row
}

// Xbox 0x0011DEA0
bool scene_pause_prompt_shown()
{
    return 0 <= g_scene_pause.prompt && g_scene_pause.prompt <= 8;
}

// Xbox 0x0011DEE0
void close_scene_pause(ScenePauseServices& s)
{
    g_scene_pause.choice = 1;
    if (g_scene_pause.phase != 6)
    {
        g_scene_pause.phase = 6;
        close_pause_book(s);
    }
}

// Xbox 0x0011ECF0, EAX the scene record: the scene's index in the game (the scenes of the chapters before it,
// DAT_002FE670, and its own number), or -1.
i32 scene_bonus_book_index(ScenePauseServices& s)
{
    const i32 scene = s.script_scene(s.scene); // 0x0011EBC0, EBX the record
    const i32 chapter = s.chapter(s.scene); // 0x0004B4D0
    if (chapter == 0 || scene == 0)
    {
        return -1;
    }
    i32 before = 0;
    for (i32 index = 1; index < chapter; ++index)
    {
        before += i32(word(s, 0x002FE670 + u32(index) * 4));
    }
    return before - 1 + scene;
}

// Xbox 0x0011D490, EAX the phase
void set_scene_pause_phase(i32 phase, ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    if (phase == p.phase)
    {
        return;
    }
    p.phase = phase;
    switch (phase)
    {
    case 1:
        request_scene_resume(s); // 0x0011EBA0
        b.flags &= 0xFFFFFFDFu;
        g_510ce8 = 0;
        if (p.book == nullptr)
        {
            g_510ce8 = 0;
            return;
        }
        release_transition_book(p.book, b); // 0x00048860
        p.book = nullptr;
        return;
    case 2:
    {
        const float x = s.hud.center_x; // DAT_00509730
        const float y = s.hud.center_y; // DAT_00509734
        ++g_4dea24;
        p.e38 = 0.f;
        g_510e34 = 1;
        p.input = create_runtime_selected_input(*b.input_release); // 0x00107D60, ESI DAT_00510CFC
        p.input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(p.input.data()));
        (*b.input_addresses)[p.input_address] = p.input;
        (*b.input_views)[p.input_address] = RuntimeDialogueInputDevice{p.input, {nullptr, nullptr, nullptr, nullptr}};
        push_runtime_input_entry(p.input_address, image(s).string(0x0032A54C), *b.input); // 0x00109340
        if (p.text == nullptr)
        {
            b.random = b.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
            p.seed = b.random;
            allocate_dialogue_text_pool(p.text, 0, 1, 0x802C0, b.text_pools); // 0x0005F390, ESI DAT_00510CF8
            p.text->origin = {real(s, 0x0032B20C), real(s, 0x0032B11C), std::bit_cast<float>(p.seed)};
        }
        if (p.widget == nullptr)
        {
            std::array<char, 0x80> resource{};
            std::strncpy(resource.data(), image(s).string(0x003225E8), resource.size() - 1); // menutext
            const float margin = real(s, 0x0032B1F4);
            create_resource_dialogue_widget(p.widget, 0, nullptr, {s.hud.inner_left + margin,
                s.hud.inner_bottom - margin}, {0.f, real(s, 0x0032B600)}, {real(s, 0x0032B234), real(s, 0x0032B5FC)},
                nullptr, 1.f, 0.75f, image(s).string(0x003225D0), resource.data(), 0xC00C0,
                *b.widget_allocation); // 0x00141DA0, ESI DAT_00510E30, "menutext"
            p.widget->pool->origin = {real(s, 0x0032B20C), real(s, 0x0032B11C), std::bit_cast<float>(p.seed)};
        }
        read_background_volume(p.volume_low, p.volume_high); // 0x00118CB0, ECX DAT_00510E0C, EAX DAT_00510E18
        const Vec3 low{p.volume_low.x, p.volume_low.y - real(s, 0x0032B64C), p.volume_low.z};
        initialize_background_volume(low, p.volume_high); // 0x00118AE0, ECX low, EAX DAT_00510E18
        b.flags |= 0x20u;
        g_510ce8 = 1;
        g_507df4 = 1;
        p.chapter = s.chapter(s.scene); // 0x0004B4D0, EAX DAT_00510E58
        std::array<char16_t, 0x100> line{};
        if (p.chapter == 0)
        {
            std::memcpy(line.data(), p.title.data(), sizeof(p.title)); // 0x0014A961
        }
        else
        {
            const std::span<const u8> resource = acquire_named_animation_resource(b.resources,
                image(s).string(0x00311E08), b.error); // 0x00035B60
            std::array<char16_t, 0x100> chapter_word{};
            std::array<char16_t, 0x100> scene_word{};
            lookup_borrowed_localtext(resource, image(s).string(0x0032A538), image(s).string(0x0032A544),
                chapter_word.data(), b.localtext); // 0x000FF120, "pausetext"
            lookup_borrowed_localtext(resource, image(s).string(0x0032A538), image(s).string(0x0032A530),
                scene_word.data(), b.localtext);
            release_animation_resource(b.resources, resource); // 0x00035C00, EBX the resource
            const i32 shown_chapter = i32(word(s, 0x002FE698 + u32(p.chapter) * 4));
            const i32 book_scene = s.book_scene(s.scene); // 0x0011EC70, EAX DAT_00510E58
            const std::u16string format = wide(s, 0x0032A50C); // "%s %d, %s %d: %s"
            xapi_format_wide_string(line.data(), format.c_str(), chapter_word.data(), shown_chapter,
                scene_word.data(), book_scene, p.title.data()); // 0x0014BC7D
        }
        play_random_ghoul_laugh(s); // 0x0013BA70
        p.book = create_transition_book(scene_directory_book_clip(), 0, nullptr, b); // 0x001162B0, 0x00048260
        request_namespaced_localtext(image(s).string(word(s, 0x00430C1C)), image(s).string(0x0032A538),
            image(s).string(0x0032A504), p.paused.data(), b.localtext); // 0x000FF0A0, "Paused" into DAT_00510AE8
        p.paused_text = create_dialogue_text_record(*p.text, p.paused.data(), x, y - real(s, 0x0032B170), 1.5f,
            1.5f, 3.f, 1, 1, 1, b.metrics, b.viewport_width, b.next_text_id, b.random); // 0x0005F5C0, EAX 1
        p.line_text = create_dialogue_text_record(*p.text, line.data(), x, y - real(s, 0x0032B6C4), 0.75f, 0.75f,
            3.f, 1, 1, 1, b.metrics, b.viewport_width, b.next_text_id, b.random);
        choose_scene_pause_prompt(s); // 0x0011DD90
        const i32 index = scene_bonus_book_index(s); // 0x0011ECF0, EAX DAT_00510E58
        if (index == -1)
        {
            return;
        }
        const std::span<const u8> challenges = s.challenge_bits(); // 0x000F2490, ECX the index
        if ((challenges[size_t(u32(index) >> 3)] & (1u << (u32(index) & 7u))) == 0)
        {
            return;
        }
        std::array<char, 0x80> icon{};
        std::strncpy(icon.data(), image(s).string(0x0032A4CC), icon.size() - 1); // ..._pauseicons_bonusbooks
        p.icon = acquire_named_animation_resource(b.resources, icon.data(), b.error); // 0x00035B60
        p.icon_position = {x, y + real(s, 0x0032B334)};
        p.icon_scale = {real(s, 0x0032B2AC), real(s, 0x0032B2AC)};
        return;
    }
    case 3:
        hold_pause_book_open(s);
        return;
    case 4:
        enqueue_runtime_dialogue_key(image(s).string(word(s, 0x00430C20)), 0, s.dialogue); // 0x000582A0
        return;
    case 5:
        if (s.event_state == 3)
        {
            untranslated("0x0011D490 phase 5's event check 0x000D0780");
        }
        if (s.frontend_state != 3)
        {
            return;
        }
        untranslated("0x0011D490 phase 5's front-end check 0x0004A270");
    case 6:
        close_pause_book(s);
        return;
    case 7:
    {
        p.prompt = -1;
        RuntimeDialogueWidget& prompts = widget("0x0011D490 phase 7");
        prompts.requested_set = nullptr;
        set_dialogue_widget_mode(prompts, 3, widget_context(s)); // 0x00142C20
        RuntimeTransitionBook& closing = book("0x0011D490 phase 7");
        seek_transition_book(closing, std::bit_cast<float>(0x3DFBE76Du), b); // 0x00048940
        if (closing.paused != 0)
        {
            pause_book_events(closing, 0, s);
            closing.paused = 0;
        }
        hold_transition_book_route(closing); // 0x00048960
        untranslated("0x0011D490 phase 7's map spot (0x001410F0 with no room argument)");
    }
    case 8:
        select_scene_pause_prompt(0xB, s); // 0x0011DD10, EAX 0xB
        b.flags |= 0x2200u;
        return;
    case 9:
        release_transition_book_hold(book("0x0011D490 phase 9"), b); // 0x00048920, EAX DAT_00510E24
        b.flags &= 0xFFFFDDFFu;
        choose_scene_pause_prompt(s); // 0x0011DD90
        return;
    default:
        return;
    }
}

// Xbox 0x0011CE10
void enter_scene_pause(ScenePauseServices& s)
{
    set_scene_pause_phase(2, s); // 0x0011D490, EAX 2
    acquire_tracked_audio_bank(image(s).string(0x0030F898), 0x10, s.banks); // 0x00038910, PAUSE
    s.audio_level(0.f); // 0x00038110
    ++s.dialogue.frontend_pause_count; // DAT_004E173C
}

// Xbox 0x0011CE40
void leave_scene_pause(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    if (p.phase != 1)
    {
        GhoulySpawnPool* records = p.text->allocation; // [DAT_00510CF8] +8 = +4
        records->cursor = records->begin;
        p.text->records = {records->begin, 0};
        reset_dialogue_widget_prompt(widget("0x0011CE40")); // 0x00142AA0, ESI DAT_00510E30
        set_scene_pause_phase(1, s); // 0x0011D490, EAX 1
    }
    initialize_background_volume(p.volume_low, p.volume_high); // 0x00118AE0, ECX DAT_00510E0C, EAX DAT_00510E18
    if (s.dialogue.state.mode != 0) // DAT_004EE1EC
    {
        set_runtime_dialogue_mode(0, s.dialogue); // 0x000584D0
    }
    --s.dialogue.frontend_pause_count; // DAT_004E173C
    g_507df4 = 0;
    if (p.book != nullptr)
    {
        release_transition_book(p.book, b); // 0x00048860
        p.book = nullptr;
    }
    if (g_510ce8 != 0)
    {
        b.flags &= 0xFFFFFFDFu;
        g_510ce8 = 0;
    }
    if (read_u32(p.input, 4) != 0)
    {
        pop_runtime_input_entry(*b.input); // 0x001093C0
    }
    switch (read_u32(p.input, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(p.input); // 0x00107DD0
        break;
    case 4:
        release_runtime_composite_input(p.input, *b.input_release); // 0x00107960
        break;
    case 5:
        release_runtime_actor_input(p.input, *b.input_release); // 0x00107B60
        break;
    default:
        break;
    }
    b.input_addresses->erase(p.input_address);
    b.input_views->erase(p.input_address);
    p.input = {};
    p.input_address = 0;
    if (p.icon.data() != nullptr)
    {
        release_animation_resource(b.resources, p.icon); // 0x00035C00
        p.icon = {};
    }
    if (g_510e34 != 0)
    {
        --g_4dea24;
        g_510e34 = 0;
    }
    // The pause ditty's bank (0x00038970 inlined).
    AudioTrackedBank& bank = *find_tracked_audio_bank(image(s).string(0x0030F898), s.banks); // 0x00038460
    --bank.references;
    if (bank.references < 1)
    {
        if (bank.id != 0x12)
        {
            AudioTrackedBank* resumed = select_resume_audio_bank(bank, s.banks); // 0x00038590
            if (resumed != nullptr)
            {
                resume_tracked_audio_bank(*resumed, s.banks); // 0x00038360
            }
        }
        detach_tracked_audio_bank(bank, s.banks); // 0x00038260
        bank.id = 0;
    }
    s.audio_level(real(s, 0x0032B0F8)); // 0x00038110
}

// Xbox 0x0011CFB0
void tick_scene_pause(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    if (1 < p.phase && p.phase < 10)
    {
        s.update_dialogue(); // 0x00057E60
    }
    if (p.input.data() != nullptr)
    {
        refresh_runtime_input_device(p.input_address, b.frame(), *b.input, *b.input_release); // 0x00109470
    }
    const i32 phase = p.phase;
    bool open = false; // 0x0011D017: phase 3 unless the phase is 3.
    switch (phase)
    {
    case 1:
        if (p.widget != nullptr && p.widget->current_set == nullptr) // +0x94C
        {
            destroy_dialogue_widget(p.widget, b.resources, *b.widget_allocation->pools); // 0x00142070
            p.widget = nullptr;
        }
        if (p.text != nullptr && p.text->allocation->cursor <= p.text->allocation->begin)
        {
            release_dialogue_text_pool(p.text, *b.text_pools); // 0x0005F460, EBX DAT_00510CF8
            p.text = nullptr;
        }
        break;
    case 2:
        open = p.book == nullptr || book_position(*p.book) > std::bit_cast<float>(0x3DFBE76Du); // DAT_0032B6CC
        break;
    case 4:
    case 5:
        open = s.dialogue.state.mode == 0; // DAT_004EE1EC
        break;
    case 6:
        if (p.book == nullptr || p.book->finished != 0)
        {
            switch (p.choice)
            {
            case 1:
                set_scene_pause_phase(1, s); // 0x0011D490, EAX 1
                break;
            case 2:
                s.stop_countdown(); // 0x000F2780
                if (g_runtime_profile.name[0] == 0) // DAT_00506C90
                {
                    clear_runtime_game(s); // 0x0011EAD0
                }
                else
                {
                    leave_to_gameselect(3);
                }
                break;
            case 3:
                s.restart_challenge(); // 0x0004A310
                s.stop_countdown(); // 0x000F2780
                break;
            case 4:
                s.stop_countdown(); // 0x000F2780
                leave_to_gameselect(6);
                break;
            case 5:
                leave_to_gameselect(10);
                break;
            case 6:
                leave_to_gameselect(0xB);
                break;
            case 7:
                s.stop_countdown(); // 0x000F2780
                leave_to_gameselect(9);
                break;
            default:
                break;
            }
        }
        break;
    case 7:
        if ((p.book == nullptr || p.book->finished != 0) && phase != 8)
        {
            p.phase = 8;
            select_scene_pause_prompt(0xB, s); // 0x0011DD10, EAX 0xB
            b.flags |= 0x2200u;
        }
        break;
    case 9:
        open = p.book == nullptr || book_position(*p.book) > std::bit_cast<float>(0x3F50A3D7u); // DAT_0032B634
        break;
    default:
        break;
    }
    if (open && phase != 3)
    {
        p.phase = 3;
        hold_pause_book_open(s);
    }
    RuntimeDialogueInputDevice device;
    switch (p.phase)
    {
    case 1:
    case 2:
    case 6:
    case 7:
    case 9:
        if (p.widget != nullptr)
        {
            update_prompts(*p.widget, nullptr, s); // 0x00142460, EAX 0
        }
        break;
    case 3:
        if (const RuntimeDialogueInputDevice* input = input_device(device))
        {
            RuntimeDialogueWidget& prompts = widget("0x0011CFB0 phase 3");
            if (update_prompts(prompts, input, s) == 0 && scene_pause_prompt_shown() // 0x0011DEA0
                && runtime_input_button_edge(*input, 0x10, b.widget_update->input) != 0) // 0x001083F0, Start
            {
                close_scene_pause(s); // 0x0011DEE0
            }
        }
        else
        {
            update_prompts(widget("0x0011CFB0 phase 3"), nullptr, s);
        }
        break;
    case 8:
        if (p.widget != nullptr)
        {
            update_prompts(*p.widget, input_device(device), s); // EAX DAT_00510CFC
        }
        break;
    default:
        break;
    }
    if (p.phase < 2 || (7 < p.phase && p.phase != 9) || p.book == nullptr)
    {
        return;
    }
    RuntimeTransitionBook& shown = *p.book;
    const float position = book_position(shown);
    i32 page = -1;
    bool mark = true;
    if (position <= transition_book_threshold(shown, 0x104, image(s))) // 0x0042217C
    {
        page = transition_book_row_word(shown, 0x10C, image(s)); // 0x00422184
    }
    else if (position <= transition_book_threshold(shown, 0x108, image(s))) // 0x00422180
    {
        mark = false;
    }
    else
    {
        page = transition_book_row_word(shown, 0x110, image(s)); // 0x00422188
    }
    if (mark && page != -1)
    {
        shown.pages.at(size_t(page)).capture = 1; // +0x94 + page * 0x10
    }
    tick_transition_book(shown, b); // 0x000483E0
}

// Xbox 0x0011DB50
void draw_scene_pause(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    switch (p.phase)
    {
    case 1:
    case 2:
    case 3:
    case 6:
    case 7:
    case 8:
    case 9:
        if (p.widget != nullptr)
        {
            draw_runtime_dialogue_widget(*p.widget, 1, *b.widget_draw); // 0x00142720
        }
        break;
    default:
        break;
    }
    if (p.icon.data() != nullptr)
    {
        const float full = real(s, 0x0032B2AC);
        std::array<float, 2> scale{};
        const float* scaling = nullptr;
        switch (p.phase)
        {
        case 2:
        {
            float level = real(s, 0x0032B0F8);
            if (p.book != nullptr)
            {
                const float grown = book_position(*p.book) * real(s, 0x0032B62C) * full;
                level = full;
                if (grown <= full)
                {
                    level = grown < 0.f ? 0.f : grown;
                }
            }
            scale = {level, level};
            scaling = scale.data();
            break;
        }
        case 3:
        case 7:
        case 9:
            scaling = p.icon_scale.data(); // DAT_00510E4C
            break;
        case 6:
        {
            float level = real(s, 0x0032B0F8);
            if (p.book != nullptr)
            {
                level = full - (book_position(*p.book) - real(s, 0x0032B634)) * real(s, 0x0032B630) * full;
                if (level < 0.f)
                {
                    level = 0.f;
                }
                else if (full < level)
                {
                    level = full;
                }
            }
            scale = {level, level};
            scaling = scale.data();
            break;
        }
        default:
            break;
        }
        if (scaling != nullptr)
        {
            draw_runtime_dialogue_sprite(p.icon.data(), p.icon_position.data(), scaling, 0.f, word(s, 0x00314F48),
                -1, b.draw); // 0x0005CE30
        }
    }
    if (p.book != nullptr)
    {
        draw_transition_book(*p.book, b); // 0x00048550
    }
    if (p.phase == 8 && p.widget != nullptr)
    {
        untranslated("0x0011DB50 phase 8's HUD viewport 0x000F6F70");
    }
}

// Xbox 0x0011CD90
void reset_scene_pause(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    if (p.text != nullptr)
    {
        release_dialogue_text_pool(p.text, *b.text_pools); // 0x0005F460
        p.text = nullptr;
    }
    if (p.widget != nullptr)
    {
        destroy_dialogue_widget(p.widget, b.resources, *b.widget_allocation->pools); // 0x00142070
        p.widget = nullptr;
    }
    if (p.book != nullptr)
    {
        release_transition_book(p.book, b); // 0x00048860
        p.book = nullptr;
    }
    p.phase = 1;
    if (g_510e34 != 0)
    {
        --g_4dea24;
        g_510e34 = 0;
    }
    if (p.icon.data() != nullptr)
    {
        release_animation_resource(b.resources, p.icon); // 0x00035C00
        p.icon = {};
    }
}

namespace
{
// 0x0011DFA0 and 0x0011E410: a confirmation line under the book (DAT_00510E08) and its prompt row.
void show_pause_confirm(const char* key, i32 prompt, u32 row, ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    auto& b = s.books;
    std::array<char16_t, 0x100> text{};
    request_namespaced_localtext(image(s).string(word(s, 0x00430C1C)), image(s).string(0x0032A538), key,
        text.data(), b.localtext); // 0x000FF0A0, "pausetext"
    p.third_text = create_dialogue_text_record(*p.text, text.data(), s.hud.center_x,
        s.hud.center_y + real(s, 0x0032B1E0), 0.75f, 0.75f, 3.f, 1, 0, 1, b.metrics, b.viewport_width,
        b.next_text_id, b.random); // 0x0005F5C0, EAX 1
    p.prompt = prompt;
    set_prompt_row(widget("the pause's confirmation"), row, s);
}
}

// Xbox 0x0011DFA0, pause_Quit: the quit warning for the scene flags and the game's state.
void confirm_scene_pause_quit(ScenePauseServices& s)
{
    const u8 flags = s.books.scene_flags; // **(DAT_00506C74+8)
    const char* key;
    if ((flags & 0x60) != 0)
    {
        key = "QuitReplayScene";
    }
    else if ((flags & 0x80) != 0)
    {
        key = "QuitReplayChapter";
    }
    else if ((flags & 8) == 0 && (s.books.flags & 0x200000u) == 0)
    {
        if ((s.books.flags & 0x4000u) != 0)
        {
            key = "QuitChallenge";
        }
        else
        {
            key = g_scene_pause.chapter == 9 ? "GameNotSavedLong" : "AreYouSure";
        }
    }
    else
    {
        key = g_scene_pause.chapter == 9 ? "QuitChallenge21Chap5" : "QuitChallenge21";
    }
    show_pause_confirm(key, 9, 0x003114D8, s); // pauseMenu_confirmQuit
}

// Xbox 0x0011E110, pause_Stats: the stats page through the scene dialogue (phase 4).
void show_scene_pause_stats(ScenePauseServices& s)
{
    if (g_scene_pause.phase != 4)
    {
        g_scene_pause.phase = 4;
        enqueue_runtime_dialogue_key(image(s).string(word(s, 0x00430C20)), 0, s.dialogue); // 0x000582A0
    }
}

// Xbox 0x0011E140, pause_Map: the book turns to the house map (phase 7).
void show_scene_pause_map(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    if (p.phase == 7)
    {
        return;
    }
    p.phase = 7;
    p.prompt = -1;
    RuntimeDialogueWidget& prompts = widget("0x0011E140");
    prompts.requested_set = nullptr;
    set_dialogue_widget_mode(prompts, 3, widget_context(s)); // 0x00142C20
    RuntimeTransitionBook& turning = book("0x0011E140");
    seek_transition_book(turning, std::bit_cast<float>(0x3DFBE76Du), s.books); // 0x00048940
    if (turning.paused != 0)
    {
        pause_book_events(turning, 0, s);
        turning.paused = 0;
    }
    hold_transition_book_route(turning); // 0x00048960
    untranslated("0x0011E140's map spot (0x001410F0 with no room argument)");
}

// Xbox 0x0011E150, pause_EventRules: the event's or the front end's rules (phase 5).
void show_scene_pause_rules(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    if (p.phase == 5)
    {
        return;
    }
    p.phase = 5;
    if (s.event_state == 3) // DAT_005064D8
    {
        untranslated("0x0011E150's event rules 0x000D0780");
    }
    if (s.frontend_state != 3) // DAT_004E17C8
    {
        p.phase = 5;
        return;
    }
    untranslated("0x0011E150's front-end rules 0x0004A270");
}

// Xbox 0x0011E1A0, pause_No: the confirmation line ends and the game's prompt row returns (0x0011DD90).
void cancel_scene_pause_confirm(ScenePauseServices& s)
{
    end_dialogue_text_record(*g_scene_pause.text, g_scene_pause.third_text, s.books.fonts, s.books.fallback_texture,
        s.books.random); // 0x0005F7E0, ECX DAT_00510E08
    choose_scene_pause_prompt(s); // The tail jump to 0x0011DD90.
}

// The quit and restart's close (0x0011E1C0 and 0x0011E320 inline it): phase 6, the book closing.
void close_pause_book_for_choice(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    if (p.phase != 6) // DAT_00510CF4
    {
        p.phase = 6;
        close_pause_book(s);
    }
}

// Xbox 0x0011E1C0, pause_Yes on the quit warning.
void quit_scene_from_pause(ScenePauseServices& s)
{
    auto& times = g_runtime_profile.times;
    if ((s.books.scene_flags & 2) != 0)
    {
        const std::array<i32, 2> counts = s.play_counts(); // DAT_00545970, DAT_00545974
        times[1] = times[1] + counts[0]; // DAT_00506C7C
        times[0] = times[0] + i32(s.scene_time); // DAT_00506C78, DAT_004312D0
        times[3] = times[3] + counts[1]; // DAT_00506C84
    }
    s.save_quit(); // 0x000F2CB0
    s.release_shakes(); // 0x000FAE40
    const u8 flags = s.books.scene_flags;
    if ((flags & 0x20) != 0)
    {
        g_scene_pause.choice = 5; // DAT_00510E28
    }
    else if ((flags & 0x40) != 0)
    {
        g_scene_pause.choice = 6;
    }
    else if ((flags & 0x80) != 0)
    {
        g_scene_pause.choice = 7;
    }
    else
    {
        g_scene_pause.choice = (s.frontend_state == 3 ? 1 : 0) * 2 + 2; // DAT_004E17C8
    }
    s.end_challenge_game(); // 0x00049C30
    close_pause_book_for_choice(s);
}

// Xbox 0x0011E320, pause_Yes on the restart warning.
void restart_scene_from_pause(ScenePauseServices& s)
{
    s.release_shakes(); // 0x000FAE40
    g_scene_pause.choice = 3; // DAT_00510E28
    close_pause_book_for_choice(s);
}

// Xbox 0x0011EAD0: the current game ends (its scene flags, name and times cleared) and the held order's first
// script is requested.
void clear_runtime_game(ScenePauseServices& s)
{
    auto& profile = g_runtime_profile;
    std::memset(profile.scene_flags->bytes, 0, size_t(profile.scene_flags->size)); // rep stos over +4 bytes
    profile.name[0] = 0; // DAT_00506C90, a dword store: the name's first two characters.
    profile.name[1] = 0;
    profile.times[0] = 0; // DAT_00506C78
    profile.times[1] = 0;
    profile.times[2] = 0;
    profile.times[3] = 0;
    profile.times[4] = 0;
    profile.times[5] = 0; // DAT_00506C8C
    request_scene_aid(s.held_order(), 1); // 0x0011EB30, EAX DAT_00511B64
}

// Xbox 0x0011E3E0, pause_MapReturn: the book leaves the map (phase 9) and the game's prompt row returns.
void leave_scene_pause_map(ScenePauseServices& s)
{
    auto& p = g_scene_pause;
    if (p.phase == 9)
    {
        return;
    }
    p.phase = 9;
    release_transition_book_hold(book("0x0011E3E0"), s.books); // 0x00048920
    s.books.flags &= 0xFFFFDDFFu;
    choose_scene_pause_prompt(s); // 0x0011DD90 inlined; DAT_0050EA9C 0x4000 is read before the clear, as above.
}

// Xbox 0x0011E410, pause_RestartChallenge: the restart warning.
void confirm_scene_pause_restart(ScenePauseServices& s)
{
    show_pause_confirm("RestartChallenge", 10, 0x003117E8, s); // pauseMenu_confirmRestart
}

// Xbox 0x0011DCF0, EAX the name or null
void name_scene(const char16_t* name)
{
    const char16_t* source = name != nullptr ? name : u"No Name Available";
    size_t index = 0;
    do
    {
        g_scene_pause.title[index] = source[index]; // lstrcpyW into DAT_00510D00
    }
    while (source[index++] != 0);
}
}
