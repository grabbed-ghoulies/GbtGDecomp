#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/language.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/transition_data.h"

#include <algorithm>
#include <array>
#include <span>
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
#include "cutscene_runtime.h"
#include "../scene/scene_loading.h"
#include "../scripts/script_samples.h"
#include "../scripts/script_vm.h"
#include "../scripts/script_actions.h"

namespace gbtg
{
using namespace scene_detail;


void release_props(Game& g)
{
    // The prop vector keeps its storage, so the next clip's meshes come back
    // at these addresses. The draw cache must not keep the previous clip.
    for (const Game::CutsceneProp& prop : g.cutscene_props)
    {
        gfx_release_mesh(prop.mesh);
    }
}

namespace
{

// DAT_004275DC, stride 4: the speech-bubble node shown for each language
// (DAT_0042A710, g_language).
constexpr std::array<u8, 5> kBubbleLanguageNode{0, 1, 3, 2, 4};

// 0x00080DA0 copies these from 0x00323EB0, 0x00323E74 and 0x00323E38.
constexpr std::array<const char*, 3> kAttractBubbles{
    "aid_model_ghoulies_cutscene_attractmodespeechbubble",
    "aid_model_ghoulies_cutscene_attractmodespeechbubbleaction",
    "aid_model_ghoulies_cutscene_attractmodespeechbubblebaddies",
};

// The model CPU blob ends in {type, value} records (+0 offset, +4 count).
// 0x000320F0 takes its node mask from the type-1 record.
u32 model_node_animation_mask(std::span<const u8> cpu)
{
    if (cpu.size() < 8)
    {
        return 0;
    }
    const u32 offset = read_u32(cpu, 0);
    const u32 count = read_u32(cpu, 4);
    for (u32 index = 0; index < count; ++index)
    {
        const size_t at = size_t(offset) + size_t(index) * 8;
        if (at + 8 > cpu.size())
        {
            break;
        }
        if (read_u32(cpu, at) == 1)
        {
            return read_u32(cpu, at + 4);
        }
    }
    return 0;
}

// Xbox 0x00080DA0. After 0x0004EDA0 instances an attract clip, the attract
// scene control finds each speech bubble through 0x0004EA50 and calls
// 0x000F96F0 with AL = 1 and the language's node on the stack.
void show_attract_bubble_language(Game& g)
{
    const u8 node = kBubbleLanguageNode[g_language < kBubbleLanguageNode.size() ? g_language : 0];
    for (const char* bubble : kAttractBubbles)
    {
        for (auto& prop : g.cutscene_props)
        {
            const Cutscene& cut = g.cutscenes.items[g.cutscenes.index];
            if (prop.actor < cut.actors.size() && cut.actors[prop.actor].model == bubble)
            {
                entity_store_pair(prop.mesh.node_flags, node, 1);
                break;
            }
        }
    }
}

// Xbox 0x00030C70 node-state tail. A clip with a state channel (MINA +0x20
// directory, entry +0xC0) passes channel 48 to 0x000320F0 every update.
// Cutscene actors play slot 0 with mix mode 1 (0x0004DC70), so the channel
// is sampled with the clamped frame.
void apply_cutscene_prop_node_states(Game::CutsceneProp& prop, const CutsceneActor& actor, float position)
{
    const std::span<const u8> mina(actor.mina);
    if (mina.size() < 0x28)
    {
        return;
    }
    const u32 directory = read_u32(mina, 0x20);
    if (directory == 0 || read_u32(mina, directory + 0xC0) == 0)
    {
        return;
    }
    MinaPlayback player;
    player.mina = mina;
    player.position = position;
    player.mode = 1;
    AnimationPose pose;
    sample_mina_auxiliary(player, pose.auxiliary);
    const std::array<ModelResourceBlock, 1> blocks{ModelResourceBlock{1, prop.node_mask}};
    ModelAnimationState model;
    model.resource_blocks = blocks;
    std::copy(prop.mesh.node_flags.begin(), prop.mesh.node_flags.end(), model.node_flags.begin());
    static ModelNodeFlagCache cache{}; // DAT_005078D8
    apply_model_node_animation(model, std::bit_cast<u32>(pose.auxiliary[48]), cache);
    std::copy_n(model.node_flags.begin(), prop.mesh.node_flags.size(), prop.mesh.node_flags.begin());
}

} // namespace

namespace scene_detail
{


void log_cutscene_begin(const Game& g)
{
    const auto& q = g.cutscenes;
    const Cutscene& c = q.items[q.index];
    std::cout << "cutscene begin [" << q.index << "/" << q.items.size() << "] " << c.name << " duration=" << c.duration
              << "s tracks=" << int(c.tracks) << " cameras=" << c.cameras.size() << " actors=" << c.actors.size()
              << " curves=" << c.camera.size() << "\n";
    Vec3 pos;
    Quat orient;
    float fov = 0.f;
    if (sample_cutscene_camera(c, q.time, pos, orient, fov))
    {
        std::cout << "  cam " << c.camera_name << " pos=" << pos.x << "," << pos.y << "," << pos.z << " quat=" << orient.x
                  << "," << orient.y << "," << orient.z << "," << orient.w << " fov=" << fov << "\n";
    }
}


void log_cutscene_end(const Game& g)
{
    const auto& q = g.cutscenes;
    std::cout << "cutscene end [" << q.index << "/" << q.items.size() << "] " << q.items[q.index].name << "\n";
}


void load_cutscene_reel(Game& g)
{
    g.cutscenes = {};
    // Attract entries are one pass of sceneorder. A later script (gameselect) may loop.
    g.cutscenes.loop = g.scene_name.find("attract") == std::string::npos;
    for (const auto& list_name : g.script_aidlists)
    {
        const BundleEntry* list = find_aid(g, list_name);
        if (!list || list->type != ResourceType::AidList)
        {
            continue;
        }
        const auto names = parse_aidlist(list->cpu);
        bool any_cutscene = false;
        for (const auto& n : names)
        {
            if (n.starts_with("aid_cutscene_"))
            {
                any_cutscene = true;
                break;
            }
        }
        if (!any_cutscene)
        {
            continue;
        }
        float total = 0.f;
        for (const auto& n : names)
        {
            const BundleEntry* e = find_aid(g, n);
            if (!e || e->type != ResourceType::Cutscene)
            {
                std::cout << "  cutscene missing " << n << "\n";
                continue;
            }
            Cutscene clip = parse_cutscene(e->cpu, e->name);
            total += clip.duration;
            g.cutscenes.items.push_back(std::move(clip));
        }
        std::cout << "cutscene reel " << list_name << " " << g.cutscenes.items.size() << " clips " << total << "s\n";
        for (size_t i = 0; i < g.cutscenes.items.size(); ++i)
        {
            const auto& c = g.cutscenes.items[i];
            std::cout << "  [" << i << "] " << c.duration << "s curves=" << c.camera.size() << " " << c.name << "\n";
        }
        break;
    }
    // A chapter clip can only start after the script reaches type 0x13.
    // Resource references include future clips; they are not start requests.
    if (g.cutscenes.items.empty())
    {
        const std::string pending = g_script_cutscene_pending;
        g_script_cutscene_pending.clear();
        const std::string* name = pending.empty() ? nullptr : &pending;
        if (name != nullptr)
        {
            const BundleEntry* entry = find_aid(g, *name);
            if (entry != nullptr && entry->type == ResourceType::Cutscene)
            {
                Cutscene clip = parse_cutscene(entry->cpu, entry->name);
                std::cout << "script cutscene " << clip.name << " " << clip.duration << "s\n";
                g.cutscenes.items.push_back(std::move(clip));
                g.cutscenes.loop = false;
            }
        }
    }
    else
    {
        g_script_cutscene_pending.clear();
    }
}


void sync_cutscene_props(Game& g)
{
    const auto& q = g.cutscenes;
    if (!q.playing || q.items.empty() || !g.scene_bundle)
    {
        release_props(g);
        g.cutscene_props.clear();
        g.cutscene_props_index = static_cast<size_t>(-1);
        return;
    }
    if (g.cutscene_props_index == q.index)
    {
        return;
    }
    g.cutscene_props_index = q.index;
    release_props(g);
    g.cutscene_props.clear();
    const Cutscene& cut = q.items[q.index];
    std::cout << "title actors " << cut.actors.size() << "\n";
    for (size_t i = 0; i < cut.actors.size(); ++i)
    {
        const BundleEntry* entry = find_aid(g, cut.actors[i].model);
        if (!entry || entry->type != ResourceType::Model)
        {
            std::cout << "  actor missing " << cut.actors[i].model << "\n";
            continue;
        }
        try
        {
            Game::CutsceneProp prop;
            prop.mesh = parse_model(*entry);
            prop.actor = i;
            prop.node_mask = model_node_animation_mask(entry->cpu);
            u16 max_bone = 0;
            for (u16 b : prop.mesh.bones)
            {
                max_bone = std::max(max_bone, b);
            }
            std::cout << "  " << cut.actors[i].model << " mina " << cut.actors[i].mina.size() << " joints "
                      << prop.mesh.joints.size() << " skinned " << prop.mesh.bones.size() << " maxbone " << max_bone
                      << "\n";
            g.cutscene_props.push_back(std::move(prop));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  actor fail " << cut.actors[i].model << ": " << ex.what() << "\n";
        }
    }
    if (g.scene_name.find("attract") != std::string::npos)
    {
        show_attract_bubble_language(g);
    }
}


float actor_curve(const CutsceneActor& actor, size_t index, float time, float duration, float fallback)
{
    if (index >= actor.curves.size() || actor.curves[index].keys.empty())
    {
        return fallback;
    }
    return sample_curve(actor.curves[index], time, duration);
}

} // namespace scene_detail


void cutscene_queue_start(Game& g)
{
    auto& q = g.cutscenes;
    if (!g_scene_script_started)
    {
        return;
    }
    if (q.items.empty())
    {
        return;
    }
    q.index = 0;
    q.time = 0.f;
    q.playing = true;
    log_cutscene_begin(g);
}


// Xbox 0x0004EB20. 0x0004EDA0 instances the clip named by a type 0x13 node
// through 0x0004D680 when the node runs. A request from the load walk stays
// pending: load_cutscene_reel resets the queue after that walk and takes it.
void cutscene_queue_begin_script_clip(Game& g)
{
    if (!g_scene_script_started || g_script_cutscene_pending.empty())
    {
        return;
    }
    const std::string name = g_script_cutscene_pending;
    g_script_cutscene_pending.clear();
    const BundleEntry* entry = find_aid(g, name);
    if (entry == nullptr || entry->type != ResourceType::Cutscene)
    {
        std::cout << "  cutscene missing " << name << "\n";
        return;
    }
    g.cutscenes = {};
    g.cutscenes.items.push_back(parse_cutscene(entry->cpu, entry->name));
    g.cutscenes.loop = false;
    std::cout << "script cutscene " << name << " " << g.cutscenes.items[0].duration << "s\n";
    cutscene_queue_start(g);
}


// Xbox 0x0004F0B0, called by 0x0004C850 event type 0x16.
// The asset belongs to the current cutscene's event stream, not its actor list.
void cutscene_camera_events(Game& g)
{
    auto& q = g.cutscenes;
    const auto& clip = q.items.at(q.index);
    if (q.camera_event_clip != clip.name || q.time < q.camera_event_time)
    {
        q.camera_event_clip = clip.name;
        q.camera_events.clear();
        q.camera_event_index = 0;
        std::string prefix = clip.name;
        prefix.replace(0, std::string("aid_cutscene_").size(), "aid_cutsceneevents_");
        if (g.scene_bundle)
        {
            for (const auto& entry : g.scene_bundle->entries)
            {
                if (entry.type != ResourceType::CutsceneEvents ||
                    (entry.name != prefix && entry.name.rfind(prefix + "_", 0) != 0))
                {
                    continue;
                }
                size_t offset = 0;
                while (offset + 16 <= entry.cpu.size())
                {
                    const u32 size = read_u32(entry.cpu, offset);
                    if (size < 16 || size > entry.cpu.size() - offset)
                    {
                        throw Error("cutscene camera event record out of range");
                    }
                    if ((read_u32(entry.cpu, offset + 12) & 0xffffu) == 0x16u)
                    {
                        const auto record = entry.cpu.subspan(offset, size);
                        const auto name_bytes = record.subspan(16);
                        const auto end = std::find(name_bytes.begin(), name_bytes.end(), u8{0});
                        const std::string name(reinterpret_cast<const char*>(name_bytes.data()),
                                               size_t(end - name_bytes.begin()));
                        q.camera_events.push_back({read_f32(record, 4), name});
                    }
                    offset += size;
                }
            }
        }
        std::stable_sort(q.camera_events.begin(), q.camera_events.end(),
                         [](const auto& a, const auto& b)
                         {
                             return a.time < b.time;
                         });
    }
    q.camera_event_time = q.time;
    while (q.camera_event_index < q.camera_events.size() &&
           q.camera_events[q.camera_event_index].time <= q.time)
    {
        const auto& event = q.camera_events[q.camera_event_index++];
        const auto* entry = find_aid(g, event.aid);
        if (entry == nullptr)
        {
            throw Error("missing cutscene camera asset: " + event.aid);
        }
        q.camera_wobble = parse_cutscene(entry->cpu, entry->name);
        q.camera_wobble_time = q.camera_wobble.duration;
    }
}


// Xbox 0x0004F110. 0x0004EBD0 calls this after 0x0004DA50, including
// frames when the story's +0x158 pause count prevents its clock advancing.
Vec3 cutscene_camera_wobble(CutsceneQueue& q, float dt)
{
    if (q.camera_wobble.name.empty())
    {
        return {};
    }
    q.camera_wobble_time += dt;
    if (q.camera_wobble.duration <= q.camera_wobble_time)
    {
        q.camera_wobble_time -= q.camera_wobble.duration;
    }
    Vec3 offset;
    Quat orientation;
    float fov;
    if (!sample_cutscene_camera(q.camera_wobble, q.camera_wobble_time, offset, orientation, fov))
    {
        throw Error("cutscene wobble asset has no camera channels");
    }
    return offset;
}


void cutscene_queue_tick(Game& g, float dt)
{
    // camera_apply 0x00025AE0: 0x0001EEC0 when camera+0xfc != 0, then 0x0001EC20
    // when camera+0x154 > 0, then 0x00025190 (host+0x1f8 anchors through
    // 0x0011BE40), then 0x0001FB40, then 0x00027960, then 0x0001DD00,
    // then 0x00025130.
    if (g_camera_channels.gate != 0)
    {
        camera_apply_default_blend(g_camera_channels);
    }
    if (!g_camera_channels.items.empty())
    {
        camera_apply_channels(g_camera_channels);
    }
    for (CamAnchor* node = g_cam_focus.anchors; node != nullptr; node = node->next)
    {
        camera_update_anchor(*node);
    }
    camera_tick_pulse(g_camera_pulse, dt);
    camera_tick_timers(g_camera_timers, g_camera_pulse, dt);
    camera_tick_wash(g_camera_wash, dt);
    camera_apply_pairs(g_camera_pairs);
    focus_bounds_tick();
    script_action_opcode_f();
    auto& q = g.cutscenes;
    if (!q.playing || q.items.empty())
    {
        movie_sync_cutscene(g);
        return;
    }
    q.time += attract_caption_dt(g, dt);
    int guard = 0;
    while (q.playing && q.items[q.index].duration > 0.f && q.time >= q.items[q.index].duration && guard < 64)
    {
        ++guard;
        q.time -= q.items[q.index].duration;
        log_cutscene_end(g);
        const size_t next = q.index + 1;
        if (next >= q.items.size())
        {
            if (!q.loop)
            {
                q.playing = false;
                q.time = q.items[q.index].duration;
                if (g.scene_name.find("attract") != std::string::npos)
                {
                    scene_order_advance(g);
                }
                movie_sync_cutscene(g);
                return;
            }
            std::cout << "cutscene reel loop\n";
            q.index = 0;
        }
        else
        {
            q.index = next;
        }
        log_cutscene_begin(g);
    }
    cutscene_camera_events(g);
    const Vec3 wobble = cutscene_camera_wobble(q, dt);
    if (g.camera && g.camera->actor)
    {
        Vec3 pos;
        Quat orient;
        float fov = 0.9f;
        if (sample_cutscene_camera(q.items[q.index], q.time, pos, orient, fov))
        {
            g.camera->actor->position = Vec3{pos.x + wobble.x, pos.y + wobble.y, pos.z + wobble.z};
            g.camera->actor->quat = orient;
            g.camera->actor->fov = fov;
        }
    }
    movie_sync_cutscene(g);
}


void draw_cutscene_props(Game& g)
{
    sync_cutscene_props(g);
    prepare_dialogue_texture(g);
    const auto& q = g.cutscenes;
    if (!q.playing || q.index >= q.items.size())
    {
        return;
    }
    const Cutscene& cut = q.items[q.index];
    for (auto& prop : g.cutscene_props)
    {
        if (prop.actor >= cut.actors.size())
        {
            continue;
        }
        const CutsceneActor& actor = cut.actors[prop.actor];
        const Vec3 origin{actor_curve(actor, 0, q.time, cut.duration, 0.f),
                          actor_curve(actor, 1, q.time, cut.duration, 0.f),
                          actor_curve(actor, 2, q.time, cut.duration, 0.f)};
        const Vec3 scale{actor_curve(actor, 3, q.time, cut.duration, 1.f),
                         actor_curve(actor, 4, q.time, cut.duration, 1.f),
                         actor_curve(actor, 5, q.time, cut.duration, 1.f)};
        apply_cutscene_prop_node_states(prop, actor, cut.duration > 0.f ? q.time / cut.duration : 0.f);
        float bones[128 * 16]{};
        if (!actor.mina.empty() && !prop.mesh.joints.empty() && !prop.mesh.bones.empty() && cut.duration > 0.f)
        {
            const float norm = q.time / cut.duration;
            const auto pose = sample_mina(actor.mina, norm);
            const int bone_n = int(std::min(pose.size(), std::min(prop.mesh.joints.size(), size_t(127))));
            if (bone_n > 0)
            {
                // Xbox 0x0002FAC0. The slot-to-joint bind is ndMtxArray (0x155E60),
                // applied when the mesh uploads the palette.
                skin_matrices(prop.mesh.joints, pose, bones, bone_n);
                gfx_set_bones(bones, bone_n);
            }
            else
            {
                gfx_set_bones(nullptr, 0);
            }
        }
        else
        {
            gfx_set_bones(nullptr, 0);
        }
        movie_apply(prop.mesh);
        gfx_draw_mesh(prop.mesh, origin, scale);
    }
    gfx_set_bones(nullptr, 0);
}

} // namespace gbtg
