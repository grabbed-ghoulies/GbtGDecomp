#include "book_map.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "../audio/event_voices.h"
#include "../audio/listener_settings.h"
#include "../camera/display_view.h"
#include "../frontend/dialogue_draw_context.h"
#include "../frontend/dialogue_text_metrics.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/dialogue_widget_bat.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_state.h"
#include "../frontend/dialogue_widget_update.h"
#include "../frontend/input_activation.h"
#include "../scene/executable_resources.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"

#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr u32 kMarkers = 0x002FD408; // 6 records of 0x88: the texture aid, +0x80 the bit that shows it, +0x84 the bit that hides it.

[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated");
}

// The map menu's scene views; the menu needs the scene's widget and input contexts.
void require_menu(const TransitionBookServices& s)
{
    if (s.widget_allocation == nullptr || s.widget_bats == nullptr || s.widget_update == nullptr
        || s.widget_draw == nullptr || s.input == nullptr || s.input_release == nullptr
        || s.input_addresses == nullptr || s.input_views == nullptr || !s.frame || s.viewport_bottom == nullptr)
    {
        throw Error("the book's map menu has no scene widget and input views");
    }
}

float real(const TransitionBookServices& s, u32 address)
{
    return read_f32(s.image.memory(address, 4), 0);
}

float frame_step(const TransitionBookServices& s)
{
    if (s.freeze <= 0.f)
    {
        return s.dt * s.scene_scale;
    }
    return 0.f;
}

// DAT_00421ED8: the runner's sprite scale. 0x00047770 writes its x as the runner turns.
std::array<float, 2>& runner_scale(const TransitionBookServices& s)
{
    static std::array<float, 2> scale{};
    static bool loaded = false;
    if (!loaded)
    {
        scale = {real(s, 0x00421ED8), real(s, 0x00421EDC)};
        loaded = true;
    }
    return scale;
}

// The book model's CPU view; the map block's words are offsets in it.
std::span<const u8> model_cpu(const RuntimeTransitionBook& book)
{
    if (book.model->model == nullptr)
    {
        throw Error("the transition book's model actor has no model (+0x204)");
    }
    return book.model->model->cpu;
}

u32 map_word(const RuntimeTransitionBook& book, u32 offset)
{
    if (book.map_block == 0)
    {
        throw Error("the transition book's model has no map block (type 0x16)");
    }
    return read_u32(model_cpu(book), book.map_block + offset);
}

// The map block's route: +0 points, +4 segments, +8 the start's name, +0x28 the end's, +0x48 the length,
// +0x4C its point table.
u32 map_route_record(const RuntimeTransitionBook& book, i32 route)
{
    return read_u32(model_cpu(book), map_word(book, 4) + u32(route) * 4);
}

const char* map_string(const RuntimeTransitionBook& book, u32 offset)
{
    return reinterpret_cast<const char*>(model_cpu(book).data() + offset);
}

// 0x00031530 on the model's pose (+0x168, else +0x164) at the point's joint (+0xC).
const AnimationMatrix& map_point_matrix(const RuntimeTransitionBook& book, u32 point)
{
    const ModelPoseBuffer* pose = nullptr;
    if (book.model->model != nullptr && book.model->model->runtime != nullptr)
    {
        const RuntimeModelInstance& model = *book.model->model->runtime;
        pose = model.alternate;
        if (pose == nullptr)
        {
            pose = model.primary;
        }
    }
    if (pose == nullptr)
    {
        throw Error("the transition book's map point has no pose matrix (0x00031530 on a null pose)");
    }
    i16 joint = 0;
    std::memcpy(&joint, model_cpu(book).data() + point + 0xC, 2);
    return model_pose_matrix(*pose, joint);
}

std::array<float, 3> map_point(const RuntimeTransitionBook& book, u32 point)
{
    const auto cpu = model_cpu(book);
    return {read_f32(cpu, point), read_f32(cpu, point + 4), read_f32(cpu, point + 8)};
}

std::array<float, 3> transform_map_point(const AnimationMatrix& m, const std::array<float, 3>& p)
{
    return {m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
        m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]};
}

// The current view's projection of a point (DAT_005077A8): written only when the point is in front.
bool project_map_point(const std::array<float, 3>& p, std::array<float, 2>& output, const TransitionBookServices& s)
{
    const auto& view = *entity_detail::g_display_view;
    const auto& t = view.transform; // +0x68
    float z = t[8] * p[0] + t[9] * p[1] + t[10] * p[2] + t[11];
    if (z > -0.00001f) // DAT_0032B23C.
    {
        return false;
    }
    const float half = 0.5f; // DAT_0032B0F4.
    z = view.focal_pixels / z; // +0x44
    output[0] = (float(view.fitted_width) * half
        - view.pixel_aspect * (t[0] * p[0] + t[1] * p[1] + t[2] * p[2] + t[3]) * z) + float(view.fitted_x);
    output[1] = (t[4] * p[0] + t[5] * p[1] + t[6] * p[2] + t[7]) * z + float(view.fitted_height) * half
        + float(view.fitted_y);
    return true;
}

// Xbox 0x00046BA0, EBX the route, XMM0 the place along it: the point that far along the route's length, between
// two of its posed points. Left unwritten past the last point.
void map_route_point(const RuntimeTransitionBook& book, i32 route, float along, std::array<float, 3>& output,
    const TransitionBookServices& s)
{
    if (along < 0.f)
    {
        along = 0.f;
    }
    else if (1.f < along) // DAT_0032B0F8.
    {
        along = 1.f; // DAT_0032B0F8.
    }
    const auto cpu = model_cpu(book);
    const u32 record = map_route_record(book, route);
    const u32 count = read_u32(cpu, record);
    const float target = read_f32(cpu, record + 0x48) * along;
    if (count <= 1)
    {
        return;
    }
    const u32 points = read_u32(cpu, record + 0x4C);
    u32 index = 1;
    float reached = 0.f;
    float length = 0.f;
    while (true)
    {
        const auto from = map_point(book, read_u32(cpu, points + (index - 1) * 4));
        const auto to = map_point(book, read_u32(cpu, points + index * 4));
        const float dx = from[0] - to[0];
        const float dy = from[1] - to[1];
        const float dz = from[2] - to[2];
        length = float(std::sqrt(double(dx) * dx + double(dy) * dy + double(dz) * dz));
        const float next = length + reached;
        if (target <= next)
        {
            break;
        }
        ++index;
        reached = next;
        if (count <= index)
        {
            return;
        }
    }
    const float blend = (target - reached) / length;
    const u32 first = read_u32(cpu, points + (index - 1) * 4);
    const auto a = transform_map_point(map_point_matrix(book, first), map_point(book, first));
    const u32 second = read_u32(cpu, points + index * 4);
    const auto b = transform_map_point(map_point_matrix(book, second), map_point(book, second));
    for (size_t axis = 0; axis < 3; ++axis)
    {
        output[axis] = (b[axis] - a[axis]) * blend + a[axis];
    }
}

// Xbox 0x00046E10, EDX the map block, ECX the model actor, EBX the route, XMM0 the place along it.
void project_map_route_point(const RuntimeTransitionBook& book, i32 route, float along, std::array<float, 2>& output,
    std::array<float, 3>& point, const TransitionBookServices& s)
{
    map_route_point(book, route, along, point, s); // 0x00046BA0
    project_map_point(point, output, s);
}

std::span<const u8> acquire_map_texture(const char* name, TransitionBookServices& s)
{
    const auto texture = acquire_animation_resource(s.resources, name); // 0x00035E90
    if (texture.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700
    }
    return texture;
}

bool girl(const TransitionBookServices& s)
{
    return (s.scene_flags & 8) != 0 || (s.flags & 0x200000) != 0;
}

bool story_bit(const TransitionBookServices& s, u32 bit)
{
    return (s.story_bits[bit >> 3] & (1u << (bit & 7))) != 0;
}

// The frame a texture's rate (+0x10) and count (+0x11) give at the clock's time.
u8 texture_frame(std::span<const u8> texture, const TransitionBookServices& s)
{
    const float rate = float(texture[0x10]);
    if (rate == 0.f) // DAT_0032B100.
    {
        return 0;
    }
    return u8(u32(truncate_runtime_float(rate * s.time)) % u32(texture[0x11])); // 0x001673D8
}

float texture_height(std::span<const u8> texture)
{
    return float(read_u16(texture, 0xA));
}

bool same_name(const char* name, const char* other)
{
    return name[0] != 0 && std::strcmp(name, other) == 0;
}

// Xbox 0x00046760, EDI the book: a label at each spot the route starts or ends at.
void place_book_map_labels(RuntimeTransitionBook& book, TransitionBookServices& s)
{
    const float half = 0.5f; // DAT_0032B0F4.
    const float lift = float(s.metrics.fonts[0]->cell) * 0.6f * half; // DAT_00504444 +0x10, DAT_0032B248.
    const auto cpu = model_cpu(book);
    for (i32 index = 0; index < i32(map_word(book, 8)); ++index)
    {
        const u32 spot = read_u32(cpu, map_word(book, 0xC) + u32(index) * 4);
        const char* name = map_string(book, spot + 0xE);
        const bool from = same_name(book.map_from.data(), name);
        const bool to = same_name(book.map_to.data(), name);
        if (!from && !to)
        {
            continue;
        }
        const auto point = transform_map_point(map_point_matrix(book, spot), map_point(book, spot));
        auto& label = book.map_labels.at(size_t(book.map_points));
        if (!project_map_point(point, label.position, s))
        {
            continue;
        }
        if (book.map_points == 0)
        {
            allocate_dialogue_text_pool(book.map_text, 0, 1, 0, s.text_pools); // 0x0005F390, ESI +0x114
        }
        request_namespaced_localtext("aid_loctext_ghoulies_mapnames", "mapname", name, label.text.data(),
            s.localtext); // 0x000FF0A0
        const float width = runtime_text_line_width(s.metrics, 0, label.text.data(), 0.6f); // 0x00060710, DAT_0032B248.
        const float spot_height = texture_height(book.map_spot);
        float x = 0.f;
        if (label.position[0] + width <= float(s.screen[0]) * 0.85f) // DAT_0032B17C.
        {
            x = spot_height + label.position[0];
        }
        else
        {
            x = (label.position[0] - width) - spot_height;
        }
        create_dialogue_text_record(*book.map_text, label.text.data(), x, label.position[1] - lift, 0.6f, 0.6f,
            1.f, 0, 0, 1, s.metrics, s.viewport_width, s.next_text_id,
            s.random); // 0x0005F5C0, EAX 1
        ++book.map_points;
    }
}

// Xbox 0x00046500: the map block's six marker points (+0x10..+0x24) placed, each with its texture.
void place_book_map_markers(RuntimeTransitionBook& book, TransitionBookServices& s)
{
    for (u32 index = 0; index < 6; ++index)
    {
        const u32 point = map_word(book, 0x10 + index * 4);
        if (point == 0)
        {
            continue;
        }
        const auto position = transform_map_point(map_point_matrix(book, point), map_point(book, point));
        auto& marker = book.map_markers[index];
        if (!project_map_point(position, marker.position, s))
        {
            continue;
        }
        const u32 name = kMarkers + index * 0x88;
        std::span<const u8> texture;
        if (read_u32(s.image.memory(name, 4), 0) == 0x5F646961) // "aid_"
        {
            texture = acquire_map_texture(s.image.string(name), s);
        }
        marker.texture = texture;
    }
}

void end_sprite_draw(TransitionBookServices& s)
{
    --s.draw.override_count; // DAT_00509B5C
    if (s.draw.override_count == 0)
    {
        s.draw.fog_disabled = 0; // DAT_00509B00
    }
}
}

// Xbox 0x00046F40
void begin_book_map(RuntimeTransitionBook& book, i32 route, TransitionBookServices& services)
{
    auto& s = services;
    const i32 index = (route < 0 ? -route : route) - 1;
    if (book.model->model == nullptr)
    {
        throw Error("the transition book's model actor has no model (+0x204)");
    }
    const auto cpu = model_cpu(book);
    u32 block = 0;
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    for (u32 entry = 0; entry < count; ++entry)
    {
        if (read_u32(cpu, table + entry * 8) == 0x16)
        {
            block = read_u32(cpu, table + entry * 8 + 4);
            break;
        }
    }
    book.map_block = block;
    book.map_state = 1;
    book.map_timer_ec = 0.3f; // DAT_0032B268.
    book.map_spot = acquire_map_texture("aid_texture_ghoulies_misc_mapspot", s);
    book.map_labelled = 0;
    book.map_points = 0;
    book.map_ready = 0;
    book.map_countdown = 5;
    book.map_footstep = 0.3f; // DAT_0032B268.
    if (route == 0)
    {
        book.map_timer_d4 = 0.f;
        book.map_timer_d8 = 0.f;
        book.map_route = 0;
        book.map_runner = acquire_map_texture(girl(s) ? "aid_texture_ghoulies_misc_girlstand"
                                                      : "aid_texture_ghoulies_misc_boystand", s);
        book.map_runner_back = {};
        book.map_reverse = 0;
    }
    else
    {
        const float time = read_f32(cpu, map_route_record(book, index) + 0x48) * 0.14285715f; // DAT_0032B650.
        book.map_route = index;
        book.map_timer_d4 = time;
        book.map_timer_d8 = time;
        book.map_runner = acquire_map_texture(girl(s) ? "aid_texture_ghoulies_misc_girlrunning"
                                                      : "aid_texture_ghoulies_misc_boyrunning", s);
        book.map_runner_back = acquire_map_texture(girl(s) ? "aid_texture_ghoulies_misc_girlrunningback"
                                                           : "aid_texture_ghoulies_misc_boyrunningback", s);
        book.map_reverse = route < 0 ? 1 : 0;
        std::array<float, 3> point{};
        project_map_route_point(book, book.map_route, 0.f, book.map_runner_position, point, s); // 0x00046E10
        book.map_runner_position[1] = book.map_runner_position[1]
            - texture_height(book.map_runner) * real(s, 0x00421EDC) * 0.5f; // DAT_0032B0F4.
        const u32 record = map_route_record(book, index);
        const char* start = map_string(book, record + 8);
        const char* end = map_string(book, record + 0x28);
        if (route < 1)
        {
            std::strcpy(book.map_to.data(), start);
            std::strcpy(book.map_from.data(), end);
        }
        else
        {
            std::strcpy(book.map_from.data(), start);
            std::strcpy(book.map_to.data(), end);
        }
        book.voice = request_spatial_audio_event_voice(s.image.string(0x00421F60), 0, 0.f, s.voices); // 0x00039510
    }
    if (book.map_menu != 0)
    {
        require_menu(s);
        std::array<char, 0x80> text{};
        std::strncpy(text.data(), s.image.string(0x003225E8), text.size() - 1); // aid_loctext_ghoulies_menu_menutext
        book.map_input = create_runtime_selected_input(*s.input_release); // 0x00107D60, ESI +0x15C
        // Native pointer mapping; the retail entry stores the allocated address.
        book.map_input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(book.map_input.data()));
        (*s.input_addresses)[book.map_input_address] = book.map_input;
        (*s.input_views)[book.map_input_address] = {book.map_input, {nullptr, nullptr, nullptr, nullptr}};
        push_runtime_input_entry(book.map_input_address, s.image.string(0x003225DC), *s.input); // 0x00109340, "Map Screen"
        // Anchor (DAT_005096E8 * 0.5, DAT_0050974C - 16), rows and origin (0, 0).
        create_resource_dialogue_widget(book.map_widget, 0, s.image.memory(0x002FD738, 0x310).data(),
            {float(s.screen[0]) * 0.5f, *s.viewport_bottom - 16.f}, {0.f, 0.f}, {0.f, 0.f}, nullptr, 1.5f, 0.75f,
            s.image.string(0x003225D0), text.data(), 0x80, *s.widget_allocation); // 0x00141DA0, "menutext"
        attach_dialogue_widget_bat(*book.map_widget, 2.f, 0, *s.widget_bats); // 0x00142860
        std::snprintf(book.map_widget->sound_name.data(), book.map_widget->sound_name.size(), "%s",
            s.image.string(0x00421FE0)); // +0x1034, XACT_SOUNDBANK_GGRIMREAPER_DEATHBELLFRONTEND
    }
}

// Xbox 0x00047370, ESI the book
void close_book_map(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    if (book.map_state != 1)
    {
        return;
    }
    book.map_state = 2;
    if (book.map_text != nullptr)
    {
        end_all_dialogue_text_records(*book.map_text, s.fonts, s.fallback_texture, s.random); // 0x0005F970
    }
    if (book.voice != -1)
    {
        stop_audio_event_voice(book.voice, 0, s.voices); // 0x00039690(0)
        book.voice = -1;
    }
}

// Xbox 0x000473D0, ESI the book
void release_book_map(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    book.map_state = 0;
    for (auto* texture : {&book.map_runner, &book.map_runner_back, &book.map_spot})
    {
        if (texture->data() != nullptr)
        {
            release_animation_resource(s.resources, *texture); // 0x00035C00
            *texture = {};
        }
    }
    for (auto& marker : book.map_markers)
    {
        if (marker.texture.data() != nullptr)
        {
            release_animation_resource(s.resources, marker.texture);
            marker.texture = {};
        }
    }
    if (book.map_text != nullptr)
    {
        release_dialogue_text_pool(book.map_text, *s.text_pools); // 0x0005F460
        book.map_text = nullptr;
    }
    s.flags &= 0xFFFFDFFFu;
    book.map_points = 0;
    if (book.map_widget != nullptr)
    {
        require_menu(s);
        destroy_dialogue_widget(book.map_widget, s.resources, *s.widget_allocation->pools); // 0x00142070
        book.map_widget = nullptr;
    }
    if (book.map_input.data() != nullptr)
    {
        require_menu(s);
        pop_runtime_input_entry(*s.input); // 0x001093C0
        switch (read_u32(book.map_input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(book.map_input); // 0x00107DD0
            break;
        case 4:
            release_runtime_composite_input(book.map_input, *s.input_release); // 0x00107960
            break;
        case 5:
            release_runtime_actor_input(book.map_input, *s.input_release); // The inlined 0x00107B60.
            break;
        default:
            break;
        }
        s.input_addresses->erase(book.map_input_address);
        s.input_views->erase(book.map_input_address);
        book.map_input = {};
        book.map_input_address = 0;
    }
    if (book.voice == -1)
    {
        return;
    }
    auto& voice = s.voices.voices[size_t(book.voice)]; // DAT_004E0CE8, stride 0x24
    if (voice.cue == nullptr)
    {
        voice.state = 10;
    }
    else
    {
        switch (voice.state)
        {
        case 2:
        case 3:
            voice.state = 4;
            break;
        case 5:
        case 6:
        case 7:
            voice.state = 9;
            stop_runtime_sound_cue(voice.cue->runtime, 0, s.voices.core.stopped_count, s.voices.core.sound); // 0x0015CE30
            break;
        default:
            break;
        }
    }
    book.voice = -1;
}

// Xbox 0x000475B0, EAX the book
u32 tick_book_map(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    if (book.map_state != 1)
    {
        if (book.map_state == 2)
        {
            const float step = frame_step(s);
            if (0.f < book.map_timer_ec)
            {
                book.map_timer_ec = book.map_timer_ec - step;
                if (book.map_timer_ec <= 0.f)
                {
                    book.map_timer_ec = 0.f;
                    release_book_map(book, s); // 0x000473D0
                }
            }
            book.map_ready = 1;
            return book.map_ready;
        }
        return book.map_ready;
    }
    u32 done = 1;
    if (book.map_timer_d8 != 0.f)
    {
        const float step = frame_step(s);
        if (0.f < book.map_footstep)
        {
            book.map_footstep = book.map_footstep - step;
            if (book.map_footstep <= 0.f)
            {
                book.map_footstep = 0.3f; // DAT_0032B268.
                request_spatial_audio_event_voice(s.image.string(0x00421EE0), 1, 0.f, s.voices); // 0x00039510
            }
        }
    }
    if (book.map_countdown == 0)
    {
        s.flags |= 0x2000; // DAT_0050EA9C
    }
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: the countdown counts retail ticks.
    else if (rare_replay_retail_tick())
#else
    else
#endif
    {
        --book.map_countdown;
    }
    if (book.map_input.data() != nullptr)
    {
        require_menu(s);
        refresh_runtime_input_device(book.map_input_address, s.frame(), *s.input, *s.input_release); // 0x00109470
        if (book.map_widget == nullptr)
        {
            throw Error("0x000475B0 updates the map menu through address zero");
        }
        s.widget_update->input.devices = s.input->entries;
        s.widget_update->input.selected = s.input->selected;
        s.widget_update->input.device_views = s.input_views;
        const RuntimeDialogueInputDevice device{book.map_input, {nullptr, nullptr, nullptr, nullptr}};
        done = update_dialogue_widget(*book.map_widget, &device, *s.widget_update); // 0x00142460, EAX +0x15C
    }
    if (book.map_timer_d4 <= 0.f)
    {
        if (done != 0)
        {
            book.map_ready = 1;
            return 1;
        }
    }
    else
    {
        const float step = frame_step(s);
        if (0.f < book.map_timer_d4)
        {
            book.map_timer_d4 = book.map_timer_d4 - step;
            if (book.map_timer_d4 <= 0.f)
            {
                book.map_timer_d4 = 0.f;
                if (done != 0)
                {
                    book.map_ready = 1;
                    return 1;
                }
            }
        }
    }
    book.map_ready = 0;
    return book.map_ready;
}

// Xbox 0x00047770, EAX the book
void draw_book_map(RuntimeTransitionBook& book, TransitionBookServices& services)
{
    auto& s = services;
    if (book.map_state == 0)
    {
        return;
    }
    if (book.map_labelled == 0)
    {
        place_book_map_labels(book, s); // 0x00046760
        place_book_map_markers(book, s); // 0x00046500
        book.map_labelled = 1;
    }
    const float one = 1.f; // DAT_0032B0F8.
    const u8 alpha = u8(truncate_runtime_float(
        (one - (0.3f - book.map_timer_ec) * 3.3333333f) * 255.f)); // 0x001673D8, DAT_0032B268, DAT_0032B3B4, DAT_0032B158.
    const u32 color = (u32(alpha) << 24) | (0xFFFFFFFFu & 0x00FFFFFFu); // DAT_00314F48.
    const std::array<float, 2> spot_scale{real(s, 0x00422060), real(s, 0x00422064)};
    begin_dialogue_sprite_draw(s.draw); // 0x0005CDC0
    for (i32 index = 0; index < book.map_points; ++index)
    {
        draw_dialogue_texture(-1, nullptr, const_cast<u8*>(book.map_spot.data()), spot_scale.data(), nullptr, 0.f,
            color, book.map_labels[size_t(index)].position.data(), s.draw); // 0x0005CA60
    }
    for (u32 index = 0; index < book.map_markers.size(); ++index)
    {
        auto& marker = book.map_markers[index];
        const u32 record = kMarkers + index * 0x88;
        if (marker.texture.data() == nullptr || !story_bit(s, read_u32(s.image.memory(record + 0x80, 4), 0))
            || story_bit(s, read_u32(s.image.memory(record + 0x84, 4), 0)))
        {
            continue;
        }
        const long double remainder = animation_remainder(s.time, 0.5f); // 0x00152BE0
        const float wave = float(retail_sin(float((remainder + remainder) * 6.2831855f))); // 0x00153320, DAT_0032B298.
        const float size = wave * 0.05f + 0.5f; // DAT_0032B250, DAT_0032B0F4.
        const std::array<float, 2> scale{size, size};
        const u8 frame = texture_frame(marker.texture, s);
        draw_dialogue_texture(frame, nullptr, const_cast<u8*>(marker.texture.data()), scale.data(), nullptr, 0.f,
            color, marker.position.data(), s.draw); // 0x0005CA60
    }
    end_sprite_draw(s);
    if (book.map_widget != nullptr)
    {
        require_menu(s);
        draw_runtime_dialogue_widget(*book.map_widget, 1, *s.widget_draw); // 0x00142720
        return;
    }
    begin_dialogue_sprite_draw(s.draw); // 0x0005CDC0
    std::array<float, 2> position{};
    std::span<const u8> runner;
    if (book.map_timer_d8 == 0.f)
    {
        if (book.map_points == 0)
        {
            end_sprite_draw(s);
            return;
        }
        runner = book.map_runner;
        position = {book.map_labels[0].position[0],
            book.map_labels[0].position[1] - texture_height(runner) * 0.125f}; // DAT_0032B5B8.
    }
    else
    {
        end_sprite_draw(s);
        auto& d3d = s.draw.d3d;
        reset_dialogue_sprite_state(s.draw); // 0x0005E110
        d3d.simple_state(0x40354, 0x207); // D3D 0x00171050
        s.draw.deferred[(0x181C54 - 0x181968) / 4] = 0x207;
        d3d.simple_state(0x4035C, 0);
        s.draw.deferred[(0x181C70 - 0x181968) / 4] = 0;
        d3d.depth(1); // D3D 0x001722A0
        s.draw.deferred[(0x181968 - 0x181968) / 4] |= 0x800;
        s.draw.deferred[(0x1819A0 - 0x181968) / 4] = 2;
        s.draw.deferred[(0x1819B0 - 0x181968) / 4] = 2;
        s.draw.deferred[(0x1819A8 - 0x181968) / 4] = 0;
        s.draw.deferred[(0x1819B8 - 0x181968) / 4] = 0;
        d3d.vertex_shader(0x44); // D3D 0x001739F0
        s.line_width(real(s, 0x00422068)); // D3D 0x00171570
        d3d.begin(2); // D3D 0x00172D10
        d3d.vertex_data_color(3, (u32(alpha) << 24) | 0xEFF286); // D3D 0x00172CC0
        const auto cpu = model_cpu(book);
        // The stack slots the loop and the runner share: a projection behind the view leaves them as they were.
        std::array<float, 3> near_point{};
        std::array<float, 3> far_point{};
        std::array<float, 2> near_screen{};
        std::array<float, 2> far_screen{};
        float slot_1c = 0.f;
        float slot_20 = 0.f;
        const float two_32 = 4294967300.f; // DAT_0032B144.
        const auto unsigned_real = [two_32](i32 value)
        {
            float result = float(value);
            if (value < 0)
            {
                result = result + two_32;
            }
            return result;
        };
        for (i32 index = 0;; ++index)
        {
            float ratio = (book.map_timer_d8 - book.map_timer_d4) / book.map_timer_d8;
            const i32 segments = i32(read_u32(cpu, map_route_record(book, book.map_route) + 4));
            if (one < ratio)
            {
                ratio = one;
            }
            else if (ratio < 0.f)
            {
                ratio = 0.f;
            }
            if (unsigned_real(segments) * ratio <= float(index))
            {
                break;
            }
            float along = 0.f;
            if (book.map_reverse != 0)
            {
                const float count = unsigned_real(segments);
                along = unsigned_real(segments - index) / count;
                slot_1c = along - one / (count + count);
            }
            else
            {
                const float count = unsigned_real(segments);
                along = float(index) / count;
                slot_1c = one / (count * 2.f) + along; // DAT_0032B110.
            }
            map_route_point(book, book.map_route, along, near_point, s); // 0x00046BA0
            project_map_point(near_point, near_screen, s);
            map_route_point(book, book.map_route, slot_1c, far_point, s);
            project_map_point(far_point, far_screen, s);
            d3d.vertex_data4f(0, near_screen[0], near_screen[1], 0.f, 1.f); // D3D 0x00172C60
            d3d.vertex_data4f(0, far_screen[0], far_screen[1], 0.f, 1.f);
        }
        d3d.end(); // D3D 0x00172D50
        begin_dialogue_sprite_draw(s.draw); // 0x0005CDC0
        float ratio = (book.map_timer_d8 - book.map_timer_d4) / book.map_timer_d8;
        float along = 0.f;
        if (book.map_reverse != 0)
        {
            if (one < ratio)
            {
                along = one - one;
            }
            else
            {
                along = one - (ratio < 0.f ? 0.f : ratio);
            }
        }
        else
        {
            along = one < ratio ? one : (ratio < 0.f ? 0.f : ratio);
        }
        map_route_point(book, book.map_route, along, far_point, s); // 0x00046BA0
        std::array<float, 2> screen{slot_1c, slot_20};
        project_map_point(far_point, screen, s);
        slot_1c = screen[0];
        slot_20 = screen[1] - texture_height(book.map_runner) * real(s, 0x00421EDC) * 0.5f; // DAT_0032B0F4.
        ratio = (book.map_timer_d8 - book.map_timer_d4) / book.map_timer_d8;
        if ((ratio <= one && (ratio < 0.f || ratio < one)) || book.map_runner_shown.data() == nullptr)
        {
            runner_scale(s)[0] = book.map_runner_position[0] > slot_1c ? -0.7f : 0.7f; // DAT_0032B2EC, DAT_0032B264.
            book.map_runner_shown = book.map_runner_position[1] > slot_20 ? book.map_runner_back : book.map_runner;
        }
        book.map_runner_position = {slot_1c, slot_20};
        runner = book.map_runner_shown;
        position = {slot_1c, slot_20};
    }
    const u8 frame = texture_frame(book.map_runner, s);
    draw_dialogue_texture(frame, nullptr, const_cast<u8*>(runner.data()), runner_scale(s).data(), nullptr, 0.f, color,
        position.data(), s.draw); // 0x0005CA60, DAT_00421ED8
    end_sprite_draw(s);
}
}
