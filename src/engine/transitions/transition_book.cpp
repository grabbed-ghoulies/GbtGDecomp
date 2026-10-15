#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "transition_data.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#if !defined(_WIN32)
#include <cfenv>
#endif

#if defined(_WIN32)
#include <excpt.h>
#include <float.h>
#include <malloc.h>
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
    __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
    __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(void* time);
    __declspec(dllimport) unsigned long __stdcall GetTickCount();
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
                                                        const std::uintptr_t* arguments);
    __declspec(dllimport) int __stdcall CloseHandle(void* handle);
    __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
    __declspec(dllimport) unsigned long __stdcall GetLastError();
    __declspec(dllimport) int __stdcall ReadFile(void* handle, void* buffer, unsigned long bytes, unsigned long* read,
                                                 void* overlapped);
    __declspec(dllimport) int __stdcall WriteFile(void* handle, const void* buffer, unsigned long bytes,
                                                  unsigned long* written, void* overlapped);
    __declspec(dllimport) unsigned long __stdcall SetFilePointer(void* handle, long distance, long* high,
                                                                 unsigned long method);
    __declspec(dllimport) int __stdcall IsBadReadPtr(const void* lp, std::uintptr_t ucb);
    __declspec(dllimport) int __stdcall IsBadWritePtr(void* lp, std::uintptr_t ucb);
}
#endif


#include "../scene/scene_types.h"
#include "../camera/camera.h"
#include "../cutscenes/cutscene_runtime.h"
#include "scene_transitions.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_vm.h"
#include "transition_book.h"
#include "../audio/native_audio.h"

namespace gbtg
{
using namespace scene_detail;
struct MapScreenPoint
{
    float x = 0.f;
    float y = 0.f;
};


struct TransitionSprite
{
    std::vector<RgbaImage> frames;
    u8 rate = 0;
};


// Host types for the object allocated by Xbox 0x00048260. Field comments
// refer to the Xbox layout; resource bytes remain owned by AidStore.
struct TransitionBook
{
    i32 clip = 0; // +0
    i32 paused = 0; // +8
    i32 hold = 0; // +0xC
    i32 done = 0; // +0x10
    i32 map_enabled = 0; // +0x14
    i32 route = 0; // +0x18
    const BundleEntry* model = nullptr; // +0x20
    Game view; // cutscene instance, +0x24
    Bundle resident;
    float previous = 0.f; // cutscene+0x30
    i32 map_state = 0; // +0xCC
    i32 route_index = 0; // +0xD0
    float left = 0.f; // +0xD4
    float length = 0.f; // +0xD8
    i32 reverse = 0; // +0xDC
    i32 map_done = 0; // +0xE4
    float fade = 0.f; // +0xEC
    u32 map_data = 0; // +0xF0, offset in model CPU
    i32 countdown = 0; // +0xF4
    TransitionSprite running; // +0x100
    TransitionSprite running_back; // +0x104
    i32 sprite = 0; // +0x108
    MapScreenPoint last; // +0xF8
    float flip = 0.7f;
    std::string start_label; // +0x118
    std::string end_label; // +0x138
    i32 labels_initialized = 0; // +0x110
    std::vector<std::pair<std::string, MapScreenPoint>> labels;
    RgbaImage spot; // +0x10C
    float footstep = 0.f; // +0x164
    i16 chapter_voice = -1; // +0x168, the chapter-screen ditty's event voice.
    std::array<i32, 4> capture{}; // page +0x94
    i32 released_busy = 0; // transition+0x304C
    std::span<const u8> camera_events; // cutscene +0x24, loaded by 0x0004E020.
    std::array<u64, 3> sound_windows{}; // +0x48: event slot -> acquired voice.
};

std::unique_ptr<TransitionBook> g_transition_book;

std::unique_ptr<TransitionBook> g_menu_book;

std::array<RgbaImage, 2> g_book_pages;
 // DAT_005047D8 / DAT_005047E4.
constexpr std::array<int, 4> kBookPageTargets{0, 1, 1, 0};
 // 0x00423194.
Game* g_transition_game = nullptr;

i32 g_546338 = 0;
 // transition+0x10: clip
i32 g_54633c = 0;
 // transition+0x14: route

std::string transition_string(std::span<const u8> bytes, size_t offset)
{
    size_t end = offset;
    while (end < bytes.size() && bytes[end] != 0)
    {
        ++end;
    }
    if (end == bytes.size())
    {
        throw Error("unterminated transition metadata string");
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), end - offset);
}


char g_50e458[0x80]{}; // DAT_0050E458, a one-shot name that replaces the next lookup's.

// Xbox 0x00116220. The retail fallback is directory entry zero.
int transition_find_scene_index(std::string_view name)
{
    std::string override_name;
    if (std::memcmp(g_50e458, "aid_", 4) == 0)
    {
        override_name = g_50e458;
        const u32 spent = 0x5F646900u; // The leading "a" becomes NUL.
        std::memcpy(g_50e458, &spent, 4);
        name = override_name;
    }
    for (size_t i = 0; i < std::size(kSceneDirectory); ++i)
    {
        if (transition_aid_equal(kSceneDirectory[i].aid, name))
        {
            return int(i);
        }
    }
    return 0;
}


// Xbox 0x0012C2D0. EDI receives the clip, ESI the signed map route.
int transition_set_clip_route(std::string_view name, i32& clip, i32& route)
{
    const int directory = transition_find_scene_index(name);
    if (directory != -1)
    {
        const auto& entry = kSceneDirectory[directory];
        clip = kTransition31478[entry.transition].at_108;
        route = entry.route;
        return entry.transition;
    }
    clip = 0;
    route = 0;
    return 0;
}


// Xbox 0x0012C830.
void transition_select_clip(i32 phase)
{
    if (g_54632c == 0)
    {
        ++g_510cec;
        ++g_4dea24;
        g_flags_ea9c |= 0x100;
    }
    else if (g_54632c == 1)
    {
        g_510f80 = 0;
    }
    else if (g_54632c == 3)
    {
        g_510f7c = 0;
    }
    g_54632c = phase;
    if (phase == 0)
    {
        --g_510cec;
        --g_4dea24;
        g_flags_ea9c &= ~0x100u;
    }
}


const BundleEntry& transition_resource(Game& g, std::string_view name)
{
    const BundleEntry* entry = g.aids.load_aid(name);
    if (entry == nullptr)
    {
        throw Error("transition resource missing: " + std::string(name)); // Xbox 0x00032700
    }
    return *entry;
}


RgbaImage transition_texture(Game& g, std::string_view name)
{
    const auto& entry = transition_resource(g, name);
    TextureHeader header;
    if (!parse_texture_header(entry.cpu, header))
    {
        throw Error("transition texture header: " + std::string(name));
    }
    auto gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        gpu = gpu.subspan(chunk.offset, chunk.size);
    }
    return decode_xbox_texture(header, gpu);
}


// Xbox 0x00053550 / 0x0005CA60: a texture's byte +0x11 selects the frame
// count, and byte +0x10 is the frame rate on the global animation clock.
TransitionSprite transition_sprite(Game& g, std::string_view name)
{
    const auto& entry = transition_resource(g, name);
    TextureHeader header;
    if (!parse_texture_header(entry.cpu, header))
    {
        throw Error("transition sprite header: " + std::string(name));
    }
    auto gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        gpu = gpu.subspan(chunk.offset, chunk.size);
    }
    TransitionSprite sprite;
    sprite.rate = header.unknown;
    for (size_t frame = 0; frame < header.tile_count; ++frame)
    {
        const size_t offset = size_t(header.texture_offset) + frame * header.data_size;
        TextureHeader tile = header;
        tile.texture_offset = 0;
        sprite.frames.push_back(decode_xbox_texture(tile, gpu.subspan(offset, header.data_size)));
    }
    return sprite;
}


struct GoopTransition
{
    TransitionSprite texture; // Xbox transition +0x1028
    RgbaImage previous_scene; // DAT_005047DC
    bool captured = false; // transition +0x1030
    size_t frame = 0; // texture +0x13
};

GoopTransition g_goop_transition;

void transition_release_texture(); // Xbox 0x0012C1A0
 // Xbox 0x0012C1A0

// Xbox 0x0012C1D0. Load the entering texture and start the slime cue.
void transition_load_enter_texture(Game& g)
{
    transition_release_texture();
    if (g_546340 != nullptr)
    {
        g_goop_transition.texture = transition_sprite(g, g_546340);
    }
    else
    {
        g_goop_transition.texture = {};
    }
    ++g_50484c;
    g_goop_transition.captured = false;
    // 0x0012C1D0: a mode-1 event voice that stops itself, five units ahead of the camera.
    auto& voices = native_audio_runtime().voices;
    const i16 slime = allocate_audio_event_voice(1, voices);
    if (slime != -1)
    {
        voices.voices[size_t(slime)].stop = 1;
        position_audio_event_voice_ahead(slime, 5.f, voices); // DAT_0032B184
        prepare_audio_event_voice(slime, "XACT_SOUNDBANK_GMISC_SLIMEON", voices);
    }
}


// Xbox 0x0012C270. Replace the entering texture with the exit resource.
void transition_load_exit_texture(Game& g)
{
    for (const auto& frame : g_goop_transition.texture.frames)
    {
        gfx_release_sprite(frame);
    }
    g_goop_transition.texture = {};
    if (g_546340 == nullptr)
    {
        return;
    }
    g_goop_transition.texture = transition_sprite(g, g_546340);
}


// Xbox 0x0012C1A0. Drop the transition's resource ownership.
void transition_release_texture()
{
    for (const auto& frame : g_goop_transition.texture.frames)
    {
        gfx_release_sprite(frame);
    }
    gfx_release_sprite(g_goop_transition.previous_scene);
    g_goop_transition = {};
}


// Xbox 0x0012BDD0. The exit cue crosses 80 percent of the remaining time.
int transition_tick_exit_cue(Game& g)
{
    const float previous = std::bit_cast<float>(g_546330);
    float time = previous;
    if (0.f < time)
    {
        time -= g_sim_dt;
        g_546330 = std::bit_cast<u32>(time);
    }
    if (g_54632c == 1)
    {
        if (time <= 0.f)
        {
            g_510f80 = 0;
            g_54632c = 2;
        }
    }
    else if (g_54632c == 3)
    {
        if (time < 0.f)
        {
            transition_select_clip(0);
            return g_54632c;
        }
        const float threshold = std::bit_cast<float>(g_546334) * std::bit_cast<float>(0x3f4ccccdu);
        if (threshold < previous && time <= threshold)
        {
            // 0x0012BDD0.
            request_spatial_audio_event_voice("XACT_SOUNDBANK_GMISC_SLIMEOFF", 1, 0.f, native_audio_runtime().voices);
            return g_54632c;
        }
    }
    return g_54632c;
}


// Xbox 0x0012BE60 / 0x0005CA60. Select the explicit texture frame from
// transition time, rather than the texture's global animation clock.
// Xbox 0x0012BE60
void transition_select_texture_frame()
{
    auto& goop = g_goop_transition;
    const size_t count = goop.texture.frames.size();
    if (count == 0)
    {
        throw Error("goop transition texture has no frames");
    }
    float frame = 0.f;
    const float duration = std::bit_cast<float>(g_546334);
    const float time = std::bit_cast<float>(g_546330);
    GfxRect rect;
    if (g_54632c == 1)
    {
        if (!goop.captured)
        {
            if (!gfx_readback(goop.previous_scene))
            {
                throw Error("goop transition backbuffer capture failed");
            }
            ++goop.previous_scene.serial;
            goop.captured = true;
        }
        GfxRect capture_rect;
        const float width = float(goop.previous_scene.width);
        const float height = float(goop.previous_scene.height);
        const float viewport_width = std::min(width, height * (4.f / 3.f));
        const float viewport_height = std::min(height, width * (3.f / 4.f));
        capture_rect.u0 = (width - viewport_width) * 0.5f / width;
        capture_rect.u1 = 1.f - capture_rect.u0;
        capture_rect.v0 = (height - viewport_height) * 0.5f / height;
        capture_rect.v1 = 1.f - capture_rect.v0;
        gfx_draw_sprite(goop.previous_scene, capture_rect);
        const float last = float(count - 1);
        if (duration != 0.f)
        {
            frame = std::clamp(last - time / duration * last, 0.f, last);
        }
    }
    else if (g_54632c == 2)
    {
        frame = float(count - 1);
    }
    else if (g_54632c == 3)
    {
        frame = duration == 0.f ? float(count) : std::clamp(time / duration * float(count), 0.f, float(count));
        // Negative X and Y scale mirror the entering texture on departure.
        rect.u0 = rect.v0 = 1.f;
        rect.u1 = rect.v1 = 0.f;
    }
    const size_t index = static_cast<size_t>(frame);
    // 0x0005CA60 retains the prior frame when its explicit index equals count.
    if (index < count)
    {
        goop.frame = index;
    }
    gfx_draw_sprite(goop.texture.frames.at(goop.frame), rect);
}


// Xbox 0x00048110 / 0x0012B640 / 0x0012B370: the page copy flags use
// normalized cutscene time and the descriptor's two thresholds.
void transition_capture_page(TransitionBook& book)
{
    const auto& entry = kTransitionBooks[book.clip];
    const auto& q = book.view.cutscenes;
    const float normalized = q.time / q.items[0].duration;
    i32 page = -1;
    if (normalized <= entry.first)
    {
        page = entry.first_page;
    }
    else if (entry.second < normalized)
    {
        page = entry.second_page;
    }
    if (page != -1)
    {
        book.capture.at(size_t(page)) = 1;
    }
}


// Xbox 0x00048110 binds colour0 to one of two shared dynamic textures.
void bind_book_page(TransitionBook& book, size_t page, const RgbaImage& frame)
{
    const std::string shader = "shader_page" + std::to_string(page + 1);
    for (auto& prop : book.view.cutscene_props)
    {
        for (auto& submesh : prop.mesh.submeshes)
        {
            if (submesh.shader_name == shader)
            {
                i32 texture = submesh.texture;
                if (texture < 0)
                {
                    texture = i32(prop.mesh.textures.size());
                    prop.mesh.textures.push_back(frame);
                }
                else
                {
                    prop.mesh.textures.at(size_t(texture)) = frame;
                }
                submesh.texture = texture;
                if (submesh.colour0_stage >= 0 && submesh.colour0_stage < i32(submesh.stages.size()))
                {
                    submesh.stages[size_t(submesh.colour0_stage)] = texture;
                }
            }
        }
        gfx_release_mesh(prop.mesh);
    }
}


// Xbox 0x000481E0. The assembly passes table +0x11C in AL to 0x000F96F0.
void transition_apply_camera_pair(TransitionBook& book)
{
    const auto& entry = kTransitionBooks[book.clip];
    if (entry.material != -1)
    {
        for (auto& prop : book.view.cutscene_props)
        {
            if (prop.mesh.name == entry.model)
            {
                entity_store_pair(prop.mesh.node_flags, u8(entry.material), u8(entry.flag_11c));
            }
        }
    }
}


// Xbox 0x0004E020 / 0x0004C640. Missing optional event resources return null.
void load_book_camera_events(Game& g, TransitionBook& book)
{
    std::string name = kTransitionBooks[book.clip].aid;
    const size_t separator = name.find('_', 4);
    name = "aid_cutsceneevents" + name.substr(separator) + "_camera";
    if (const auto* events = g.aids.load_aid(name))
    {
        book.camera_events = events->cpu;
    }
}


// Xbox 0x0004D370, sound arms of 0x0004C850 (types 1/2, 0xC and 0x1E).
void dispatch_book_sound_events(Game& g, TransitionBook& book)
{
    size_t offset = 0;
    const auto& q = book.view.cutscenes;
    while (offset + 16 <= book.camera_events.size())
    {
        const auto remaining = book.camera_events.subspan(offset);
        const u16 type = read_u16(remaining, 0xC);
        if (type == 0)
        {
            break;
        }
        const u32 size = read_u32(remaining, 0);
        if (size < 16 || size > remaining.size())
        {
            throw Error("book cutscene event record out of range");
        }
        const auto record = remaining.first(size);
        u32 phase = 0;
        if (cutscene_event_phase(read_f32(record, 4), read_f32(record, 8), q.time, book.previous, phase))
        {
            switch (type)
            {
            case 1:
            case 2:
            case 0x1E:
                pcm_play_owned_cue(g, transition_string(record, 0x10).c_str());
                break;
            case 0xC:
            {
                u64& handle = book.sound_windows.at(read_u32(record, 0x10));
                if (phase == 0)
                {
                    handle = pcm_play_owned_cue(g, transition_string(record, 0x14).c_str());
                }
                else if (phase == 1 && handle != 0)
                {
                    pcm_stop_owned_cue(handle);
                    handle = 0;
                }
                break;
            }
            default:
                throw Error("untranslated book cutscene event type " + std::to_string(type));
            }
        }
        offset += size;
    }
}


// Active host voices in the three slots stopped by 0x0004C420 / 0x0004D720.
void release_book_sound_windows(TransitionBook& book)
{
    for (u64& handle : book.sound_windows)
    {
        if (handle != 0)
        {
            pcm_stop_owned_cue(handle);
            handle = 0;
        }
    }
}


// Xbox 0x00048260. 0x0004D680 instances the selected clip; 0x0004EA50
// resolves its named model. Mesh and MINA decoding use the shared parsers.
// Xbox 0x00048260
std::unique_ptr<TransitionBook> transition_load_book_clip(Game& g, i32 clip)
{
    auto book = std::make_unique<TransitionBook>();
    book->clip = clip;
    const auto& entry = kTransitionBooks[clip];
    const auto& resource = transition_resource(g, entry.aid);
    auto& q = book->view.cutscenes;
    q.items.push_back(parse_cutscene(resource.cpu, entry.aid));
    q.playing = true;
    q.loop = false;
    book->model = &transition_resource(g, entry.model);
    for (const auto& actor : q.items[0].actors)
    {
        book->resident.entries.push_back(transition_resource(g, actor.model));
    }
    book->view.scene_bundle = &book->resident;
    sync_cutscene_props(book->view);
    load_book_camera_events(g, *book);
    for (size_t page = 0; page < kBookPageTargets.size(); ++page)
    {
        const auto& frame = g_book_pages[size_t(kBookPageTargets[page])];
        if (!frame.bgra.empty())
        {
            bind_book_page(*book, page, frame);
        }
    }
    transition_apply_camera_pair(*book);
    std::cout << "transition book begin " << entry.aid << " duration=" << q.items[0].duration << "s\n";
    return book;
}


// Xbox 0x00046F40, map route branch. Type 0x16 is the model's map metadata.
void transition_load_map(Game& g, TransitionBook& book, i32 route)
{
    auto cpu = book.model->cpu;
    const u32 table = read_u32(cpu, 0);
    const u32 count = read_u32(cpu, 4);
    book.map_data = 0;
    for (u32 i = 0; i < count; ++i)
    {
        if (read_u32(cpu, table + i * 8) == 0x16)
        {
            book.map_data = read_u32(cpu, table + i * 8 + 4);
            break;
        }
    }
    book.map_state = 1;
    book.fade = 0.3f; // DAT_0032B268
    book.spot = transition_texture(g, "aid_texture_ghoulies_misc_mapspot");
    book.map_done = 0;
    book.countdown = 5;
    book.footstep = 0.3f;
    if (route == 0)
    {
        book.left = 0.f;
        book.length = 0.f;
        book.route_index = 0;
        book.running = transition_sprite(g, scene_end_flag_set() ? "aid_texture_ghoulies_misc_girlstand" :
                                                         "aid_texture_ghoulies_misc_boystand");
        book.reverse = 0;
    }
    else
    {
        const i32 index = (route < 0 ? -route : route) - 1;
        const u32 routes = read_u32(cpu, book.map_data + 4);
        const u32 spline = read_u32(cpu, routes + index * 4);
        const float duration = read_f32(cpu, spline + 0x48) * std::bit_cast<float>(0x3e124925u);
        book.route_index = index;
        book.left = duration;
        book.length = duration;
        book.running = transition_sprite(g, scene_end_flag_set() ? "aid_texture_ghoulies_misc_girlrunning" :
                                                         "aid_texture_ghoulies_misc_boyrunning");
        book.running_back = transition_sprite(g, scene_end_flag_set() ? "aid_texture_ghoulies_misc_girlrunningback" :
                                                              "aid_texture_ghoulies_misc_boyrunningback");
        book.reverse = route < 0;
        book.start_label = transition_string(cpu, spline + (route < 1 ? 0x28 : 8));
        book.end_label = transition_string(cpu, spline + (route < 1 ? 8 : 0x28));
        book.chapter_voice = request_spatial_audio_event_voice("XACT_SOUNDBANK_GSTREAMSDITTIES_CHAPTERSCREEN", 0,
            0.f, native_audio_runtime().voices); // 0x000471E3
        std::cout << "transition map route " << route << " duration=" << duration << "s\n";
    }
}


// Xbox 0x000473D0. Release map resources and its loading-message flag.
void transition_release_map(TransitionBook& book)
{
    book.map_state = 0;
    book.running = {};
    book.running_back = {};
    book.spot = {};
    book.labels.clear();
    g_flags_ea9c &= ~0x2000u;
}


// Xbox 0x000475B0. This transition has no menu dialog (+0x15C is zero).
int transition_initialize_map_phase(Game& g, TransitionBook& book)
{
    const float step = g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f;
    if (book.map_state != 1)
    {
        if (book.map_state == 2)
        {
            if (0.f < book.fade)
            {
                book.fade -= step;
                if (book.fade <= 0.f)
                {
                    book.fade = 0.f;
                    transition_release_map(book);
                }
            }
            book.map_done = 1;
        }
        return book.map_done;
    }
    if (book.length != 0.f && 0.f < book.footstep)
    {
        book.footstep -= step;
        if (book.footstep <= 0.f)
        {
            book.footstep = 0.3f;
            request_spatial_audio_event_voice("XACT_SOUNDBANK_GFOOTSTEPGRAVEL_MAPGRAVEL", 1, 0.f,
                native_audio_runtime().voices); // 0x000476A0
        }
    }
    if (book.countdown == 0)
    {
        g_flags_ea9c |= 0x2000;
    }
    else
    {
        --book.countdown;
    }
    if (book.left <= 0.f)
    {
        book.map_done = 1;
        return 1;
    }
    if (0.f < book.left)
    {
        book.left -= step;
        if (book.left <= 0.f)
        {
            book.left = 0.f;
            book.map_done = 1;
            return 1;
        }
    }
    book.map_done = 0;
    return 0;
}


// Xbox 0x000483E0. Pause at the opening threshold, run the map, then
// release the pause when the scene-load state switches the handler to exit.
// Xbox 0x000483E0
void transition_tick_book_clip(Game& g, TransitionBook& book)
{
    auto& q = book.view.cutscenes;
    if (!q.items.empty())
    {
        transition_initialize_map_phase(g, book);
        if (book.done == 0)
        {
            // Xbox 0x0004DA50: paused clocks retain their previous time.
            if (book.paused == 0)
            {
                book.previous = q.time;
                q.time += g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f;
                if (q.items[0].duration < q.time)
                {
                    q.time = q.items[0].duration;
                }
                if (q.time < 0.f)
                {
                    q.time = 0.f;
                }
                // 0x0004DA50 dispatches only with an unpaused clock and nonzero delta.
                if ((g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f) != 0.f)
                {
                    dispatch_book_sound_events(g, book);
                }
            }
            const int ended = q.items[0].duration <= q.time;
            if (book.hold == 0)
            {
                book.done = ended;
                return;
            }
            if (book.map_state == 0)
            {
                const float threshold = kTransitionBooks[book.clip].first;
                if (book.previous / q.items[0].duration < threshold &&
                    threshold <= q.time / q.items[0].duration)
                {
                    if (book.paused == 0)
                    {
                        book.paused = 1; // Xbox 0x0004E640(1)
                    }
                    if (book.map_enabled != 0)
                    {
                        transition_load_map(g, book, book.route);
                        return;
                    }
                    book.done = 1;
                }
            }
            else if (book.map_done != 0)
            {
                book.done = 1;
            }
        }
        else if (book.hold == 0 && book.paused != 0)
        {
            book.paused = 0; // Xbox 0x0004E640(0)
            book.done = 0;
        }
    }
}


// Xbox 0x0012B640. The map-book cases are 3..7.
void transition_initialize_map_book(Game& g)
{
    g_transition_book = transition_load_book_clip(g, g_546338);
    auto& book = *g_transition_book;
    switch (g_546338)
    {
    case 0:
    case 1:
    case 0xd:
    case 0xe:
        book.hold = 1;
        break;
    case 2:
        throw Error("chapter title transition dialog is not translated");
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        book.hold = 1;
        book.map_enabled = 1;
        book.route = g_54633c;
        break;
    default:
        break;
    }
    transition_capture_page(book);
}


// Xbox 0x0012B370. The optional chapter-title dialog is absent for maps.
int transition_initialize_chapter_title(Game& g)
{
    auto& book = *g_transition_book;
    if (g_54632c == 1)
    {
        transition_tick_book_clip(g, book);
        if (book.done != 0)
        {
            transition_select_clip(2);
            std::cout << "transition map complete; scene load released\n";
        }
    }
    else if (g_54632c == 3)
    {
        const auto& q = book.view.cutscenes;
        if (kTransitionBooks[book.clip].second < q.time / q.items[0].duration && book.released_busy == 0)
        {
            --g_50484c;
            book.released_busy = 1;
        }
        transition_capture_page(book);
        transition_tick_book_clip(g, book);
        if (book.done != 0)
        {
            transition_select_clip(0);
            std::cout << "transition book end " << q.items[0].name << "\n";
        }
    }
    return g_54632c;
}


// Xbox 0x0012B790 / 0x00047370.
void transition_release_book()
{
    auto& book = *g_transition_book;
    book.hold = 0;
    if (book.map_state == 1)
    {
        book.map_state = 2;
        if (book.chapter_voice != -1)
        {
            stop_audio_event_voice(book.chapter_voice, 0, native_audio_runtime().voices); // 0x00039690
            book.chapter_voice = -1;
        }
    }
    transition_capture_page(book);
    ++g_50484c;
    book.released_busy = 0;
}


// Xbox 0x0012B5A0. The allocation and meshes belong to this transition,
// independently of the scene teardown/load that runs while it is open.
// Xbox 0x0012B5A0
void transition_initialize_book()
{
    if (g_transition_book != nullptr)
    {
        transition_release_map(*g_transition_book);
        release_book_sound_windows(*g_transition_book);
        release_props(g_transition_book->view);
        g_transition_book.reset();
    }
}


Vec3 transition_rotate(const Quat& q, Vec3 v)
{
    const Vec3 t{2.f * (q.y * v.z - q.z * v.y), 2.f * (q.z * v.x - q.x * v.z),
                 2.f * (q.x * v.y - q.y * v.x)};
    return Vec3{v.x + q.w * t.x + q.y * t.z - q.z * t.y,
                v.y + q.w * t.y + q.z * t.x - q.x * t.z,
                v.z + q.w * t.z + q.x * t.y - q.y * t.x};
}


// Xbox 0x00031530. Each map point stores its signed matrix index at +0xC.
// The model's current matrix bank is generated by the same MINA pose as its
// draw. 0x0002FAC0 finishes with 0x0002F720, so the matrix bank already
// includes the inverse bind translation, as does skin_matrices.
Vec3 transition_map_point(const TransitionBook& book, u32 offset)
{
    const auto cpu = book.model->cpu;
    const i16 index = static_cast<i16>(read_u16(cpu, offset + 0xc));
    const auto& mesh = book.view.cutscene_props.at(0).mesh;
    const auto& q = book.view.cutscenes;
    const auto pose = sample_mina(q.items[0].actors.at(0).mina, q.time / q.items[0].duration);
    std::vector<float> matrices(mesh.joints.size() * 16);
    skin_matrices(mesh.joints, pose, matrices.data(), int(mesh.joints.size()));
    if (index < 0 || size_t(index) >= mesh.joints.size())
    {
        throw Error("map point matrix outside model bank");
    }
    const float* matrix = matrices.data() + size_t(index) * 16;
    const Vec3 p{read_f32(cpu, offset), read_f32(cpu, offset + 4), read_f32(cpu, offset + 8)};
    return Vec3{matrix[0] * p.x + matrix[4] * p.y + matrix[8] * p.z + matrix[12],
                matrix[1] * p.x + matrix[5] * p.y + matrix[9] * p.z + matrix[13],
                matrix[2] * p.x + matrix[6] * p.y + matrix[10] * p.z + matrix[14]};
}


// Xbox 0x00046BA0. Walk the route's point distances, interpolate the
// enclosing segment, and transform the result by the model instance.
// Xbox 0x00046BA0
void transition_route_point(const TransitionBook& book, float normalized, Vec3& out)
{
    if (0.f <= normalized)
    {
        if (1.f < normalized)
        {
            normalized = 1.f;
        }
    }
    else
    {
        normalized = 0.f;
    }
    const auto cpu = book.model->cpu;
    const u32 routes = read_u32(cpu, book.map_data + 4);
    const u32 route = read_u32(cpu, routes + book.route_index * 4);
    const u32 count = read_u32(cpu, route);
    const float distance = read_f32(cpu, route + 0x48) * normalized;
    if (1 < count)
    {
        const u32 points = read_u32(cpu, route + 0x4c);
        float previous = 0.f;
        for (u32 i = 1; i < count; ++i)
        {
            const u32 a = read_u32(cpu, points + (i - 1) * 4);
            const u32 b = read_u32(cpu, points + i * 4);
            const Vec3 p{read_f32(cpu, a), read_f32(cpu, a + 4), read_f32(cpu, a + 8)};
            const Vec3 next{read_f32(cpu, b), read_f32(cpu, b + 4), read_f32(cpu, b + 8)};
            const Vec3 delta{next.x - p.x, next.y - p.y, next.z - p.z};
            const float segment = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            const float end = previous + segment;
            if (distance <= end)
            {
                const float fraction = (distance - previous) / segment;
                const Vec3 world = transition_map_point(book, a);
                const Vec3 world_next = transition_map_point(book, b);
                out = Vec3{world.x + (world_next.x - world.x) * fraction,
                           world.y + (world_next.y - world.y) * fraction,
                           world.z + (world_next.z - world.z) * fraction};
                return;
            }
            previous = end;
        }
    }
}


// Xbox 0x00046E10: project the transformed map point through the active
// transition camera. The original near-plane comparison leaves output intact.
// Xbox 0x00046E10
void transition_project_map_point(const TransitionBook& book, Vec3 point, MapScreenPoint& out)
{
    const auto& q = book.view.cutscenes;
    Vec3 position;
    Quat orientation;
    float fov = 0.f;
    if (!sample_cutscene_camera(q.items[0], q.time, position, orientation, fov))
    {
        throw Error("transition camera missing");
    }
    const auto& actor = q.items[0].actors.at(0);
    const Vec3 origin{actor_curve(actor, 0, q.time, q.items[0].duration, 0.f),
                      actor_curve(actor, 1, q.time, q.items[0].duration, 0.f),
                      actor_curve(actor, 2, q.time, q.items[0].duration, 0.f)};
    const Vec3 scale{actor_curve(actor, 3, q.time, q.items[0].duration, 1.f),
                     actor_curve(actor, 4, q.time, q.items[0].duration, 1.f),
                     actor_curve(actor, 5, q.time, q.items[0].duration, 1.f)};
    point = Vec3{point.x * scale.x + origin.x, point.y * scale.y + origin.y, point.z * scale.z + origin.z};
    const Quat inverse{-orientation.x, -orientation.y, -orientation.z, orientation.w};
    const Vec3 view = transition_rotate(inverse, Vec3{point.x - position.x, point.y - position.y, point.z - position.z});
    if (view.z <= -0.00001f)
    {
        const float focal = 240.f / std::tan(fov * 0.5f);
        out.x = 320.f + view.x * focal / (1.1f * -view.z);
        out.y = 240.f - view.y * focal / -view.z;
    }
}


// Xbox 0x00047770, route line and running sprite. D3D line submission is
// represented by a screen mesh at the existing graphics boundary.
void draw_transition_route(TransitionBook& book)
{
    if (book.map_state == 0 || book.length == 0.f)
    {
        return;
    }
    const auto cpu = book.model->cpu;
    const u32 routes = read_u32(cpu, book.map_data + 4);
    const u32 route = read_u32(cpu, routes + book.route_index * 4);
    const u32 segments = read_u32(cpu, route + 4);
    const float progress = std::clamp((book.length - book.left) / book.length, 0.f, 1.f);
    Mesh line;
    line.name = "transition route";
    for (u32 i = 0; float(i) < float(segments) * progress; ++i)
    {
        const float a = book.reverse == 0 ? (float(i) + 0.5f) / float(segments) :
                                          (float(segments - i) - 0.5f) / float(segments);
        const float b = book.reverse == 0 ? (float(i + 1) + 0.5f) / float(segments) :
                                          (float(segments - i - 1) - 0.5f) / float(segments);
        Vec3 start{};
        Vec3 end{};
        transition_route_point(book, a, start);
        transition_route_point(book, b, end);
        MapScreenPoint p{};
        MapScreenPoint next{};
        transition_project_map_point(book, start, p);
        transition_project_map_point(book, end, next);
        const float dx = next.x - p.x;
        const float dy = next.y - p.y;
        const float magnitude = std::sqrt(dx * dx + dy * dy);
        const float nx = -dy / magnitude * 2.5f; // DAT_00422068: line width 5
        const float ny = dx / magnitude * 2.5f;
        const u32 base = u32(line.positions.size());
        line.positions.insert(line.positions.end(), {Vec3{p.x + nx, -p.y - ny, 0.f},
                                                    Vec3{next.x + nx, -next.y - ny, 0.f},
                                                    Vec3{next.x - nx, -next.y + ny, 0.f},
                                                    Vec3{p.x - nx, -p.y + ny, 0.f}});
        line.indices.insert(line.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    Submesh submesh;
    submesh.index_count = u32(line.indices.size());
    submesh.color = Vec3{239.f / 255.f, 242.f / 255.f, 134.f / 255.f};
    line.submeshes.push_back(submesh);
    gfx_draw_screen_mesh(line, 0.f, 0.f, 1.f);
    gfx_release_mesh(line);
    Vec3 point{};
    transition_route_point(book, book.reverse == 0 ? progress : 1.f - progress, point);
    MapScreenPoint screen{};
    transition_project_map_point(book, point, screen);
    if (progress < 1.f || book.sprite == 0)
    {
        book.flip = book.last.x <= screen.x ? 0.7f : -0.7f;
        const float y = screen.y - float(book.running.frames.at(0).height) * 0.35f;
        book.sprite = book.last.y <= y ? 1 : 2;
        book.last = MapScreenPoint{screen.x, y};
    }
    const TransitionSprite& sprite = book.sprite == 1 ? book.running : book.running_back;
    const RgbaImage& image = sprite.frames.at(size_t(book.view.time * sprite.rate) % sprite.frames.size());
    GfxRect rect;
    rect.x = (screen.x - float(image.width) * 0.35f) / 640.f;
    rect.y = (screen.y - float(image.height) * 0.7f) / 480.f;
    rect.w = float(image.width) * 0.7f / 640.f;
    rect.h = float(image.height) * 0.7f / 480.f;
    rect.u0 = book.flip < 0.f ? 1.f : 0.f;
    rect.u1 = book.flip < 0.f ? 0.f : 1.f;
    rect.color[3] = book.fade / 0.3f;
    gfx_draw_sprite(image, rect);
}


// Xbox 0x00046760. Label records contain XYZ, the matrix index at +0xC,
// and the localization key at +0xE. Only the selected route endpoints show.
// Xbox 0x00046760
void transition_draw_endpoint_label(TransitionBook& book)
{
    const auto cpu = book.model->cpu;
    const u32 count = read_u32(cpu, book.map_data + 8);
    const u32 list = read_u32(cpu, book.map_data + 0xc);
    for (u32 i = 0; i < count; ++i)
    {
        const u32 offset = read_u32(cpu, list + i * 4);
        const std::string name = transition_string(cpu, offset + 0xe);
        if ((!book.start_label.empty() && name == book.start_label) ||
            (!book.end_label.empty() && name == book.end_label))
        {
            MapScreenPoint screen{};
            transition_project_map_point(book, transition_map_point(book, offset), screen);
            book.labels.emplace_back(name, screen);
        }
    }
}


// Xbox 0x00046500. The front-garden descriptor has no unlock markers;
// retain the six pointer tests rather than adding markers to its map.
// Xbox 0x00046500
void transition_draw_unlock_markers(const TransitionBook& book)
{
    for (u32 at = 0x10; at < 0x28; at += 4)
    {
        if (read_u32(book.model->cpu, book.map_data + at) != 0)
        {
            throw Error("map unlock marker drawing is not translated");
        }
    }
}


// Xbox 0x00047770. Initialize labels after the map camera is installed,
// then draw spots, endpoint text, the route, and the animated running sprite.
// Xbox 0x00047770
void transition_draw_map(Game& g, TransitionBook& book)
{
    if (book.map_state == 0)
    {
        return;
    }
    if (book.labels_initialized == 0)
    {
        transition_draw_endpoint_label(book);
        transition_draw_unlock_markers(book);
        book.labels_initialized = 1;
    }
    for (const auto& [name, point] : book.labels)
    {
        GfxRect rect;
        rect.x = (point.x - float(book.spot.width) * 0.5f) / 640.f;
        rect.y = (point.y - float(book.spot.height) * 0.5f) / 480.f;
        rect.w = float(book.spot.width) / 640.f;
        rect.h = float(book.spot.height) / 480.f;
        rect.color[3] = book.fade / 0.3f;
        gfx_draw_sprite(book.spot, rect);
        draw_map_label(g, name, point.x, point.y, float(book.spot.height));
    }
    draw_transition_route(book);
}


// Xbox 0x0012B560 / 0x00048550. Copy the scene target into the selected
// page before switching camera and submitting the book and map overlay.
void draw_transition_book(Game& g, TransitionBook& book)
{
    book.view.time = g.time;
    for (size_t page = 0; page < book.capture.size(); ++page)
    {
        if (book.capture[page] != 0)
        {
            RgbaImage frame;
            if (gfx_readback_ui(frame))
            {
                const int target = kBookPageTargets[page];
                frame.serial = g_book_pages[size_t(target)].serial + 1;
                g_book_pages[size_t(target)] = frame;
                for (size_t shared_page = 0; shared_page < kBookPageTargets.size(); ++shared_page)
                {
                    if (kBookPageTargets[shared_page] == target)
                    {
                        bind_book_page(book, shared_page, frame);
                    }
                }
            }
            book.capture[page] = 0;
        }
    }
    const auto& q = book.view.cutscenes;
    Vec3 position;
    Quat orientation;
    float fov = 0.f;
    if (!sample_cutscene_camera(q.items[0], q.time, position, orientation, fov))
    {
        throw Error("transition camera missing");
    }
    gfx_clear_dialog_background();
    gfx_set_camera(position, orientation, fov);
    draw_cutscene_props(book.view);
    transition_draw_map(g, book);
}


// Xbox 0x000E02D0 / 0x000DB510: the menu owns its book independently
// of the scene-loading transition. Both callbacks pass flags 1, no map.
void begin_menu_book(Game& g, int clip)
{
    g_menu_book = transition_load_book_clip(g, clip);
    transition_capture_page(*g_menu_book);
}


// Xbox 0x00083C40: global scene transition or a live menu book blocks input.
bool menu_book_busy()
{
    return g_54632c != 0 || g_menu_book != nullptr;
}


// Xbox 0x00083E20 checks the first threshold before 0x00083D90 advances
// the book. Equality retains the old widget; completion releases next tick.
bool tick_menu_book(Game& g)
{
    if (g_menu_book == nullptr)
    {
        return false;
    }
    auto& book = *g_menu_book;
    const auto& q = book.view.cutscenes;
    const bool ready = kTransitionBooks[book.clip].first < q.time / q.items[0].duration;
    if (book.done != 0)
    {
        release_book_sound_windows(book);
        release_props(book.view);
        g_menu_book.reset();
        return ready;
    }
    const float previous_step = g_sim_dt;
    g_sim_dt = g.clock.dt;
    transition_tick_book_clip(g, book);
    g_sim_dt = previous_step;
    transition_capture_page(book);
    return ready;
}


// Xbox 0x0012B560 / menu 0x00048550.
void draw_scene_transition(Game& g)
{
    if (g_menu_book != nullptr)
    {
        draw_transition_book(g, *g_menu_book);
        return;
    }
    if (g_54632c != 0 && kTransition31478[g_546328].handler == 2)
    {
        transition_select_texture_frame();
        return;
    }
    if (g_54632c != 0 && g_transition_book != nullptr)
    {
        draw_transition_book(g, *g_transition_book);
    }
}


struct NamingBackground
{
    Game view;
    Bundle resident;
};

std::unique_ptr<NamingBackground> g_naming_background;


// Xbox 0x000DCEC0: cutscene instance +4, flags 6, initial time DAT_0042A0CC = 0.
void begin_naming_background(Game& g)
{
    if (g_naming_background != nullptr)
    {
        release_props(g_naming_background->view);
    }
    g_naming_background = std::make_unique<NamingBackground>();
    auto& background = *g_naming_background;
    const char* name = "aid_cutscene_ghoulies_dialogs_dialogpage1";
    const auto& resource = transition_resource(g, name);
    auto& queue = background.view.cutscenes;
    queue.items.push_back(parse_cutscene(resource.cpu, name));
    queue.playing = true;
    queue.loop = true;
    for (const auto& actor : queue.items[0].actors)
    {
        background.resident.entries.push_back(transition_resource(g, actor.model));
    }
    background.view.scene_bundle = &background.resident;
    sync_cutscene_props(background.view);
}


// Xbox 0x000DD0A0: rewind before advancement once the clip reaches its duration.
void tick_naming_background(Game& g)
{
    if (g.frontend_mode != 4)
    {
        if (g_naming_background != nullptr)
        {
            release_props(g_naming_background->view);
            g_naming_background.reset();
        }
        return;
    }
    auto& queue = g_naming_background->view.cutscenes;
    if (queue.items[0].duration <= queue.time)
    {
        queue.time = 0.f;
    }
    queue.time += scene_sim_step(g.clock.dt);
    if (queue.items[0].duration < queue.time)
    {
        queue.time = queue.items[0].duration;
    }
}


// Xbox 0x000DD170: draw the widget's cutscene before its keyboard.
void draw_naming_background(const Game& g)
{
    auto& view = g_naming_background->view;
    view.time = g.time;
    Vec3 position;
    Quat orientation;
    float fov = 0.f;
    if (!sample_cutscene_camera(view.cutscenes.items[0], view.cutscenes.time, position, orientation, fov))
    {
        throw Error("naming cutscene camera missing");
    }
    gfx_set_camera(position, orientation, fov);
    draw_cutscene_props(view);
}

} // namespace gbtg
