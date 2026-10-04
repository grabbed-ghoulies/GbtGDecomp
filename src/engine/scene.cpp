#include "gbtg/anim.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/entity.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/model.hpp"
#include "gbtg/resource.hpp"

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

namespace gbtg
{

void release_models(Game& g)
{
    for (const Mesh& mesh : g.models)
    {
        gfx_release_mesh(mesh);
    }
}

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

const BundleEntry* find_aid(const Game& g, std::string_view name)
{
    if (g.scene_bundle)
    {
        if (const BundleEntry* e = find_entry(*g.scene_bundle, name))
        {
            return e;
        }
    }
    return find_entry(g.aids.common, name);
}

void log_script_refs(Game& g, const SceneScript& script)
{
    for (const auto& aid : script.aids)
    {
        if (const BundleEntry* e = find_aid(g, aid))
        {
            if (e->type == ResourceType::AidList)
            {
                auto names = parse_aidlist(e->cpu);
                g.script_aidlists.push_back(aid);
                std::cout << "  aidlist " << aid << " (" << names.size() << ")\n";
                for (size_t i = 0; i < names.size() && i < 12; ++i)
                {
                    std::cout << "    [" << i << "] " << names[i] << "\n";
                }
                if (names.size() > 12)
                {
                    std::cout << "    ...\n";
                }
                continue;
            }
            std::cout << "  " << resource_type_name(e->type) << " " << aid << "\n";
            continue;
        }
        const u32 hash = aid_hash(aid);
        if (g.aids.demand_exists(hash))
        {
            std::cout << "  demand " << aid_hash_hex(hash) << " " << aid << "\n";
        }
        else
        {
            std::cout << "  ref " << aid << "\n";
        }
    }
}

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
}

void load_scene_models(Game& g)
{
    release_models(g);
    g.models.clear();
    for (const auto& aid : g.scene_script.aids)
    {
        if (!aid.starts_with("aid_model_"))
        {
            continue;
        }
        const BundleEntry* entry = find_aid(g, aid);
        if (!entry || entry->type != ResourceType::Model)
        {
            std::cout << "  model missing " << aid << "\n";
            continue;
        }
        try
        {
            Mesh mesh = parse_model(*entry);
            Vec3 lo{1e9f, 1e9f, 1e9f};
            Vec3 hi{-1e9f, -1e9f, -1e9f};
            for (const Vec3& p : mesh.positions)
            {
                lo.x = std::min(lo.x, p.x);
                lo.y = std::min(lo.y, p.y);
                lo.z = std::min(lo.z, p.z);
                hi.x = std::max(hi.x, p.x);
                hi.y = std::max(hi.y, p.y);
                hi.z = std::max(hi.z, p.z);
            }
            std::cout << "  model " << mesh.name << " verts " << mesh.positions.size() << " idx " << mesh.indices.size()
                      << " tex " << mesh.textures.size() << " bounds " << lo.x << "," << lo.y << "," << lo.z << " .. " << hi.x
                      << "," << hi.y << "," << hi.z << "\n";
            int lit_sm = 0;
            int detail_sm = 0;
            int bare_sm = 0;
            for (const Submesh& sm : mesh.submeshes)
            {
                if (sm.lightmap >= 0)
                {
                    ++lit_sm;
                }
                if (sm.detail >= 0)
                {
                    ++detail_sm;
                }
                if (sm.texture < 0)
                {
                    ++bare_sm;
                }
            }
            std::cout << "    submeshes " << mesh.submeshes.size() << " lit " << lit_sm << " detail " << detail_sm
                      << " bare " << bare_sm << "\n";
            if (!mesh.positions.empty())
            {
                g.models.push_back(std::move(mesh));
            }
        }
        catch (const std::exception& ex)
        {
            std::cout << "  model fail " << aid << ": " << ex.what() << "\n";
        }
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
}

float actor_curve(const CutsceneActor& actor, size_t index, float time, float duration, float fallback)
{
    if (index >= actor.curves.size() || actor.curves[index].keys.empty())
    {
        return fallback;
    }
    return sample_curve(actor.curves[index], time, duration);
}

// Body-camera block at +0x94. camera_apply (0x00025AE0) ticks it once a frame.
// The object at camera+8 is the host read by 0x0001E770; those fields stay clear
// until that object exists, which keeps the wash idle.
struct CameraWash
{
    u16 sample = 0;      // +0x94
    u16 sample_max = 0;  // +0x96
    u8 state = 0;        // +0xA4
    u8 level = 0;        // +0xA5, 0..128
    u8 enabled = 0;      // +0xA6
    u8 latch = 0;        // +0xA7
    u8 blink = 0;        // +0xA8
    u8 blink2 = 0;       // +0xA9
    u8 blink2_on = 0;    // +0xAA
    float blink_time = 0.f;   // +0xAC
    float blink2_time = 0.f;  // +0xB0
    float hold = 0.f;         // +0xB4
    float fade = 0.f;         // +0xB8
    float wave = 0.f;         // +0xBC
    float wave_base = 0.f;    // +0xC0
    float wave_mag = 0.f;     // +0xC4
    float wave_phase = 0.f;   // +0xC8
    float wave_span = 0.f;    // +0xCC
    float wave_timer = 0.f;   // +0xD0
    float wave_rate = 0.f;    // +0xD4
    i32 host_block = 0;       // host+0x98C
    float host_push = 0.f;    // host+0x978
    u8 host_flag = 0;         // host+0x969
    bool host_link = false;   // host+0x964
    bool host_link_live = false; // [host+0x964]+0xB8
};

CameraWash g_camera_wash;

constexpr float kWashExitRatio = std::bit_cast<float>(u32{0x3eaa7efau}); // 0.333
constexpr float kWashPi = std::bit_cast<float>(u32{0x40490fdbu});

float wash_ratio(const CameraWash& c)
{
    if (c.sample_max == 0)
    {
        return 1.f;
    }
    return float(c.sample) / float(c.sample_max);
}

void wash_set_rate(CameraWash& c)
{
    // Rate helper: sample / sample_max picks 4, 3, 2, or 1.
    if (c.sample_max == 0)
    {
        c.wave_rate = 1.f;
        return;
    }
    const float ratio = float(c.sample) / float(c.sample_max);
    if (ratio < 0.25f)
    {
        c.wave_rate = 4.f;
    }
    else if (ratio < 0.5f)
    {
        c.wave_rate = 3.f;
    }
    else if (ratio < 0.75f)
    {
        c.wave_rate = 2.f;
    }
    else
    {
        c.wave_rate = 1.f;
    }
}

bool wash_host_active(const CameraWash& c)
{
    // Host check. The caller's other flag is independent of this result.
    if (c.host_block != 0)
    {
        return false;
    }
    if (!(c.host_push > 0.f || c.host_flag != 0))
    {
        return false;
    }
    if (!c.host_link || !c.host_link_live)
    {
        return true;
    }
    return false;
}

bool wash_countdown(float& t, float dt)
{
    // Step a timer down. True only on the frame it reaches 0.
    if (t <= 0.f)
    {
        return false;
    }
    t -= dt;
    if (t > 0.f)
    {
        return false;
    }
    t = 0.f;
    return true;
}

u8 wash_level_byte(float v)
{
    // Clamp to [0, 128], then truncate toward zero into a byte.
    if (v < 0.f)
    {
        v = 0.f;
    }
    else if (v > 128.f)
    {
        v = 128.f;
    }
    return static_cast<u8>(static_cast<int>(v));
}

void wash_blinks(CameraWash& c, float dt)
{
    if (c.sample == 0)
    {
        c.blink_time -= dt;
        if (c.blink_time <= 0.f)
        {
            c.blink_time += 0.1f;
            c.blink ^= 1;
        }
    }
    if (c.blink2_on != 0)
    {
        c.blink2_time -= dt;
        if (c.blink2_time <= 0.f)
        {
            c.blink2_time += 0.75f;
            c.blink2 ^= 1;
        }
    }
    else
    {
        c.blink2 = 0;
    }
}

void wash_wave(CameraWash& c, float dt)
{
    if (c.sample == 0)
    {
        c.wave_base = 2.625f;
        c.wave_mag = 0.f;
    }
    float timer = c.wave_timer - dt;
    c.wave_timer = timer;
    if (timer <= 0.f)
    {
        c.wave_timer = timer + (1.f / c.wave_rate);
        c.wave_phase = c.wave_span;
    }
    if (c.wave_phase > 0.f)
    {
        const float angle = (c.wave_phase / c.wave_span) * kWashPi;
        const float s = std::sin(angle);
        c.wave = c.wave_base + (c.wave_base * c.wave_mag * s);
    }
    else
    {
        c.wave = c.wave_base;
    }
    c.wave_phase -= dt;
    if (c.wave_phase < 0.f)
    {
        c.wave_phase = 0.f;
    }
}

void fn_1dd00(CameraWash& c, float dt)
{
    // Xbox 0x0001DD00
    if (c.enabled == 0)
    {
        return;
    }
    switch (c.state)
    {
    case 0:
    {
        bool start = false;
        if (c.latch == 0)
        {
            if (c.hold > 0.f || wash_ratio(c) < 0.25f)
            {
                start = true;
            }
        }
        if (wash_host_active(c) || start)
        {
            c.wave = 1.75f;
            c.wave_base = 1.75f;
            c.wave_mag = 0.5f;
            c.wave_span = 0.2f;
            c.wave_phase = 0.f;
            c.wave_timer = 0.f;
            c.state = 1;
            c.fade = 0.4f;
            c.level = 0;
            c.blink = 1;
            wash_set_rate(c);
        }
        break;
    }
    case 1:
    {
        wash_wave(c, dt);
        wash_blinks(c, dt);
        wash_set_rate(c);
        float scaled = c.fade * 2.5f;
        scaled *= 128.f;
        c.level = wash_level_byte(128.f - scaled);
        if (wash_countdown(c.fade, dt))
        {
            c.state = 2;
        }
        break;
    }
    case 2:
    {
        wash_wave(c, dt);
        const float ratio = wash_ratio(c);
        wash_countdown(c.hold, dt);
        wash_blinks(c, dt);
        wash_set_rate(c);
        c.level = 0x80;
        bool leave = c.latch != 0;
        if (c.hold == 0.f && ratio > kWashExitRatio)
        {
            leave = true;
        }
        if (!wash_host_active(c) && leave)
        {
            c.state = 3;
            c.fade = 0.5f;
        }
        break;
    }
    case 3:
    {
        wash_wave(c, dt);
        wash_set_rate(c);
        float scaled = c.fade * 2.f;
        scaled *= 128.f;
        c.level = wash_level_byte(scaled);
        if (wash_countdown(c.fade, dt))
        {
            c.state = 0;
            c.level = 0;
        }
        break;
    }
    default:
        break;
    }
}

// camera_apply (0x00025AE0) calls 0x00025130 after the wash. camera+0x2d0
// gates it. camera+0x2d1 selects the bit pair and camera+0x2d2 is the value
// passed in AL. Host is camera+8. 0x000F96F0 stores that pair at +0x294 on
// the objects at host+0x204 and host+0x310, and caches the value per index
// in DAT_005078D8. Both objects stay null until that host is filled in.
struct CamPairObject
{
    u8 pairs[64] = {}; // +0x294, two bits per index
};

struct CameraPairs
{
    u8 enabled = 0; // +0x2d0
    u8 index = 0;   // +0x2d1
    u8 value = 0;   // +0x2d2
    CamPairObject* at_204 = nullptr;
    CamPairObject* at_310 = nullptr;
};

CameraPairs g_camera_pairs;
u32 g_pair_cache[256] = {}; // DAT_005078D8

void fn_25130(CameraPairs& cam)
{
    // Xbox 0x00025130
    // 0x000F96F0 packs AL into the pair selected by the stack byte.
    if (cam.enabled == 0)
    {
        return;
    }
    CamPairObject* slots[2] = {cam.at_204, cam.at_310};
    for (CamPairObject* slot : slots)
    {
        if (slot == nullptr)
        {
            continue;
        }
        const int pair = int(cam.index) * 2;
        const int byte_index = pair >> 3;
        const int shift = pair & 7;
        if (g_pair_cache[cam.index] != cam.value)
        {
            g_pair_cache[cam.index] = cam.value;
        }
        const u8 mask = static_cast<u8>(3u << shift);
        const u8 packed = static_cast<u8>(cam.value << shift);
        u8 cur = slot->pairs[byte_index];
        cur = static_cast<u8>(cur & static_cast<u8>(~mask));
        cur = static_cast<u8>(cur | packed);
        slot->pairs[byte_index] = cur;
    }
}

// Body-camera channels at +0x150 (count +0x154). camera_apply (0x00025AE0)
// calls 0x0001EEC0 when gate (+0xfc) is set, then 0x0001EC20 when the count
// is greater than 0. Host is camera+8: +0x50 is the bind list walked by
// 0x00030B40, +0x204 is the slot table (float* at +0x1cc, indexed by channel id).
// +0xe8 and +0xf4 are the two blend floats written into slot 0 by 0x0001EEC0.
struct CamChannel
{
    i32 id = 0;        // +0
    float value = 0.f; // +4
};

struct CamBindRow
{
    bool live = false;      // +0 -> +0 -> +0x20 all present
    std::vector<i32> words; // dword array at +0x20, indexed by channel id
};

struct CameraChannels
{
    std::vector<CamChannel> items;
    float blend[2] = {}; // +0xe8 and +0xf4
    u8 mark[2] = {};     // +0xec and +0xf8
    i32 gate = 0;        // +0xfc
    bool binds_live = false;
    std::vector<CamBindRow> binds;
    bool slots_live = false;
    std::vector<float*> slots;
};

CameraChannels g_camera_channels;

bool channel_bound(const CameraChannels& cam, i32 id)
{
    // 0x00030B40. EAX is host+0x50. A row of stride 0x28 is bound when the
    // pointer chain reaches +0x20 and the dword at id*4 is nonzero.
    if (id < 0)
    {
        return false;
    }
    const size_t index = static_cast<size_t>(id);
    for (const CamBindRow& row : cam.binds)
    {
        if (!row.live)
        {
            continue;
        }
        if (index < row.words.size() && row.words[index] != 0)
        {
            return true;
        }
    }
    return false;
}

float* channel_slot(CameraChannels& cam, i32 id)
{
    if (!cam.slots_live || id < 0)
    {
        return nullptr;
    }
    const size_t index = static_cast<size_t>(id);
    if (index >= cam.slots.size())
    {
        return nullptr;
    }
    return cam.slots[index];
}

void fn_1ec20(CameraChannels& cam)
{
    // Xbox 0x0001EC20
    bool pull = false;
    if (!cam.items.empty() && cam.binds_live)
    {
        for (const CamChannel& ch : cam.items)
        {
            if (channel_bound(cam, ch.id))
            {
                pull = true;
                break;
            }
        }
    }
    for (CamChannel& ch : cam.items)
    {
        float* slot = channel_slot(cam, ch.id);
        if (pull)
        {
            ch.value = slot ? *slot : 0.f;
            continue;
        }
        if (ch.id == 0 && cam.gate != 0)
        {
            bool marked = false;
            for (u8 flag : cam.mark)
            {
                if (flag != 0)
                {
                    marked = true;
                    break;
                }
            }
            if (marked)
            {
                float cur = slot ? *slot : 0.f;
                if (cur <= ch.value)
                {
                    cur = ch.value;
                }
                if (slot)
                {
                    *slot = cur;
                }
                continue;
            }
        }
        if (slot)
        {
            *slot = ch.value;
        }
    }
}

void fn_1eec0(CameraChannels& cam)
{
    // Xbox 0x0001EEC0
    // EDI is the camera. Skip when the bind list is live and channel 0 is bound.
    // Otherwise write blend[0] (+0xe8) then blend[1] (+0xf4) through the channel-0
    // slot (slot table +0x1cc). Both stores use that same pointer, so blend[1] remains.
    if (cam.binds_live && channel_bound(cam, 0))
    {
        return;
    }
    if (float* slot = channel_slot(cam, 0))
    {
        *slot = cam.blend[0];
    }
    if (float* slot = channel_slot(cam, 0))
    {
        *slot = cam.blend[1];
    }
}

// camera_apply (0x00025AE0) calls 0x0001FB40 with ESI = camera, after the
// channel pass and before the wash. DAT_004312dc > 0 freezes the step
// (BanjoCam). Otherwise the step is DAT_005096C0 * DAT_004312C4 (dt * scale).
// Host is camera+8. Its +0x930 > 0 forces the decay rate to 0; otherwise the
// rate is host+0x92c. Those host fields stay 0 until that object is filled in.
float g_sim_freeze = 0.f; // DAT_004312dc
float g_sim_scale = 1.f;  // DAT_004312c4
float g_sim_dt = 0.f;      // DAT_005096c0

constexpr float kPulseBlend = std::bit_cast<float>(u32{0x3f7d70a4u}); // 0.99
constexpr i32 kPulseNoteId = 0x47;

struct CameraPulse
{
    i32 marker = 0;        // +0x164
    float primary = 0.f;   // +0x168
    float blend = 0.f;     // +0x16c
    float secondary = 0.f; // +0x174
    float latch = 0.f;     // +0x25c
    u8 counts[6] = {};     // +0x260 .. +0x265
    float host_rate = 0.f; // host+0x92c
    float host_gate = 0.f; // host+0x930
    void* host = nullptr;  // camera+8, payload of note 0x47
};

// DAT_00511AA4. The node sits at object+0x900; next is object+0x904.
// vtable+0xC receives (object, message). Note 0x47 passes {id, host}.
// 0x00122CA0 and 0x00124F30 pass a pointer to the message id.
struct PulseNode
{
    void (*handle)(PulseNode* self, i32 id, void* host) = nullptr;
    PulseNode* next = nullptr;
    u16 tag_6 = 0; // object+6
    u16 word_1fc = 0; // object+0x1FC
    void (*on_message)(PulseNode* self, const i32* msg) = nullptr; // vtable +0xC
};

CameraPulse g_camera_pulse;
PulseNode* g_pulse_nodes = nullptr;

float sim_step(float dt)
{
    if (g_sim_freeze > 0.f)
    {
        return 0.f;
    }
    return dt * g_sim_scale;
}

float pulse_rate(const CameraPulse& cam)
{
    if (cam.host_gate > 0.f)
    {
        return 0.f;
    }
    return cam.host_rate;
}

int pulse_mask(const u8 counts[6])
{
    // 0x0001F7A0. Bit i is set when byte +0x260+i is nonzero.
    int mask = 0;
    if (counts[0] != 0)
    {
        mask += 1;
    }
    if (counts[1] != 0)
    {
        mask += 2;
    }
    if (counts[2] != 0)
    {
        mask += 4;
    }
    if (counts[3] != 0)
    {
        mask += 8;
    }
    if (counts[4] != 0)
    {
        mask += 0x10;
    }
    if (counts[5] != 0)
    {
        mask += 0x20;
    }
    return mask;
}

void pulse_notify(CameraPulse& cam)
{
    PulseNode* node = g_pulse_nodes;
    while (node != nullptr)
    {
        PulseNode* next = node->next;
        if (node->handle != nullptr)
        {
            node->handle(node, kPulseNoteId, cam.host);
        }
        node = next;
    }
}

void fn_1f600(CameraPulse& cam, float dt)
{
    // Xbox 0x0001F600
    // Decay +0x25c. On the frame it hits 0, decrement the six latches.
    // If they were all live and one or more lands on 0, broadcast note 0x47.
    if (!(cam.latch > 0.f))
    {
        return;
    }
    cam.latch -= pulse_rate(cam) * sim_step(dt);
    if (cam.latch > 0.f)
    {
        return;
    }
    cam.latch = 0.f;
    const int before = pulse_mask(cam.counts);
    for (u8& count : cam.counts)
    {
        --count;
    }
    const int after = pulse_mask(cam.counts);
    if (before == 0x3f && after != before)
    {
        pulse_notify(cam);
    }
}

void fn_1fb40(CameraPulse& cam, float dt)
{
    // Xbox 0x0001FB40
    fn_1f600(cam, dt);
    if (cam.primary > 0.f)
    {
        cam.primary -= pulse_rate(cam) * sim_step(dt);
        if (cam.primary <= 0.f)
        {
            cam.primary = 0.f;
            cam.marker = 0;
            cam.blend = kPulseBlend;
        }
    }
    if (cam.secondary > 0.f)
    {
        cam.secondary -= pulse_rate(cam) * sim_step(dt);
        if (cam.secondary <= 0.f)
        {
            cam.secondary = 0.f;
        }
    }
}

// camera_apply (0x00025AE0) calls 0x00027960 with ESI = camera, after 0x0001FB40.
// The step is the same host rate as the pulse (camera+8: +0x930 gates, else +0x92c)
// times DAT_005096C0 * DAT_004312C4. +0x75c only clamps at 0. +0x758 runs while
// +0x760 is set and, on the frame it hits 0, calls that object's vtable+0xC with
// {5, 0}. +0x7e8 runs when +0x7e4 is set and +0x7e5 is clear. Hitting 0 sends
// that same note to +0x7f0 (0x00013B240), then 0x0001F2F0(0x3f) drops every
// pulse latch and may broadcast note 0x47, then clears +0x7e4.
struct TimerNote
{
    void (*on_note)(TimerNote* self, i32 id, i32 param) = nullptr; // vtable +0xC
};

struct CameraTimers
{
    float at_758 = 0.f;
    float at_75c = 0.f;
    TimerNote* at_760 = nullptr;
    u8 at_7e4 = 0;
    u8 at_7e5 = 0;
    float at_7e8 = 0.f;
    TimerNote* at_7f0 = nullptr;
};

CameraTimers g_camera_timers;

void note_five(TimerNote* obj)
{
    if (obj == nullptr || obj->on_note == nullptr)
    {
        return;
    }
    obj->on_note(obj, 5, 0);
}

void fn_27960(CameraTimers& cam, CameraPulse& pulse, float dt)
{
    // Xbox 0x00027960
    const float step = pulse_rate(pulse) * sim_step(dt);
    if (cam.at_75c > 0.f)
    {
        cam.at_75c -= step;
        if (cam.at_75c <= 0.f)
        {
            cam.at_75c = 0.f;
        }
    }
    if (cam.at_760 != nullptr && cam.at_758 > 0.f)
    {
        cam.at_758 -= step;
        if (cam.at_758 <= 0.f)
        {
            cam.at_758 = 0.f;
            note_five(cam.at_760);
        }
    }
    if (cam.at_7e4 != 0 && cam.at_7e5 == 0 && cam.at_7e8 > 0.f)
    {
        cam.at_7e8 -= step;
        if (cam.at_7e8 <= 0.f)
        {
            cam.at_7e8 = 0.f;
            note_five(cam.at_7f0);
            const int before = pulse_mask(pulse.counts);
            for (u8& count : pulse.counts)
            {
                --count;
            }
            const int after = pulse_mask(pulse.counts);
            if (before == 0x3f && after != before)
            {
                pulse_notify(pulse);
            }
            cam.at_7e4 = 0;
        }
    }
}


// Opcode 0xF of the script-action switch. 0x00020DC0 reaches this through
// 0x00021220 (ESI = the action object) and 0x00021450 (ESI = command+0x58).
// EAX is the gate from 0x00020DC0: the body runs only when that gate is 0.
// The command and the out-block are the stack arguments. Success writes 1 at out+4.

struct ScriptHost
{
    u16 mode = 0;           // +0xCDC
    void* link = nullptr;   // +0x93C
    u16 sample_id = 0;      // +0xCF4
    void* marker = nullptr; // +0xCE0, tested by 0x00028D20
};

struct ScriptCmd
{
    u8 opcode = 0;                   // +0
    u8 kind = 0;                     // +1, EBX for 0x000238E0
    u32 param_34 = 0;                // +0x34 -> target+0x26C
    const ScriptHost* ref = nullptr; // +0x38 -> target+0x270
    u32 param_3c = 0;                // +0x3C -> target+0x274
    u16 mark = 0;                    // +0x64, CX for 0x000238E0
};

struct ScriptOut
{
    i32 reserved = 0; // +0
    i32 handled = 0;  // +4
};

struct ScriptRow
{
    u16 key = 0;           // +0, sort key, stride 0x108
    u8 head[4] = {};     // +2, compared with 'aid_'
    u8 mid[4] = {};      // +0x82
    void* extra = nullptr; // +0x104
};

struct ScriptTable
{
    std::vector<ScriptRow> rows; // +0 count, +4 record base
};

struct ScriptTarget
{
    ScriptHost* host = nullptr; // +8
    u16 counter = 0;             // +0x94
    u8* block = nullptr;         // *(*(target+0x15C)+8); byte +2 bit 1 blocks
    ScriptTable* table = nullptr; // +0x73C
    void* suppress = nullptr;    // +0x750, kind 0x3F forces the sample to 0
    u32 at_26c = 0;
    const ScriptHost* at_270 = nullptr;
    u32 at_274 = 0;
    u32 at_278 = 0;
    u32 at_27c = 0;
    i32 at_280 = 0;
    u16 at_162 = 0;
    void* slot_314 = nullptr;
    void* slot_744 = nullptr;
    const ScriptRow* bound = nullptr; // +0x740
    u16 bound_key = 0;                // +0x748
    void* slot_74c = nullptr;
    u8 flag_6c = 0;
    void* slot_84 = nullptr;
    void (*release)(void* block) = nullptr;                         // 0x001075C0
    void (*on_note)(ScriptTarget& self, u32 code, u16 id) = nullptr; // host vtable +0xC
    void (*on_extra)(ScriptTarget& self, void* extra) = nullptr;    // 0x0001AF10
};

struct ScriptAction
{
    bool live = false;
    i32 gate = 0;
    ScriptTarget target;
    ScriptCmd cmd;
    ScriptOut out;
};

ScriptAction g_script_action;
i32 g_script_sample_gate = 0;          // DAT_00512200
const u8* g_script_sample_bank = nullptr; // DAT_00545AC4

u8 kind_pair(u8 kind, int odd)
{
    // Byte pair at 0x00217DE8 + kind*2. Odd selects the second byte.
    struct Pair
    {
        u8 kind;
        u8 lo;
        u8 hi;
    };
    static constexpr Pair k[] = {
        {0x1D, 64, 64}, {0x1F, 64, 64}, {0x21, 64, 64}, {0x25, 128, 63},
        {0x2B, 128, 63}, {0x33, 11, 42}, {0x34, 12, 43}, {0x35, 14, 45},
        {0x36, 21, 52}, {0x37, 23, 54}, {0x38, 22, 53}, {0x39, 25, 56},
        {0x3A, 26, 57}, {0x3B, 27, 58}, {0x3C, 28, 59}, {0x3D, 29, 60},
        {0x3E, 31, 62}, {0x3F, 31, 62}, {0x40, 32, 63}, {0x41, 33, 64},
        {0x42, 34, 65}, {0x43, 35, 66}, {0x44, 36, 67}, {0x45, 40, 71},
        {0x46, 11, 42}, {0x49, 10, 41}, {0x4A, 13, 44}, {0x4B, 10, 41},
        {0x4D, 30, 61}, {0x54, 18, 49}, {0x56, 37, 68}, {0x5C, 19, 50},
        {0x5E, 38, 69}, {0x64, 20, 51}, {0x66, 39, 70}, {0x68, 104, 0},
        {0x69, 105, 0}, {0x6A, 106, 0}, {0x6D, 128, 63}, {0x6F, 128, 63},
        {0x71, 128, 63}, {0x73, 128, 63}, {0x75, 128, 63}, {0x77, 128, 63},
        {0x79, 128, 63}, {0x7B, 128, 63}, {0x7D, 128, 63}, {0x7F, 128, 63},
        {0x81, 128, 63}, {0x83, 128, 63}, {0x84, 154, 153}, {0x85, 25, 62},
        {0x87, 128, 63}, {0x89, 128, 63}, {0x8B, 128, 63}, {0x8D, 128, 63},
        {0x8F, 128, 63}, {0x90, 27, 0}, {0x92, 42, 0}, {0x94, 40, 0},
        {0x98, 24, 0}, {0x9A, 4, 0}, {0x9C, 164, 2}, {0x9E, 144, 2},
        {0xA0, 28, 0}, {0xA2, 4, 0}, {0xA4, 24, 0}, {0xA6, 4, 0},
        {0xA8, 24, 0}, {0xAA, 4, 0}, {0xAC, 68, 0}, {0xAE, 4, 0},
        {0xB0, 84, 0}, {0xB2, 4, 0}, {0xB4, 24, 0}, {0xB6, 4, 0},
        {0xB8, 72, 0}, {0xBA, 4, 0}, {0xBC, 24, 0}, {0xBE, 4, 0},
        {0xC0, 24, 0}, {0xC2, 4, 0}, {0xC4, 28, 0}, {0xC6, 4, 0},
        {0xC8, 24, 0}, {0xCA, 4, 0}, {0xCC, 52, 0}, {0xCE, 16, 0},
        {0xD0, 64, 0}, {0xD2, 16, 0}, {0xD4, 48, 0}, {0xD6, 4, 0},
        {0xD8, 60, 0}, {0xDA, 4, 0}, {0xDC, 60, 0}, {0xDE, 4, 0},
        {0xE0, 24, 0}, {0xE2, 4, 0}, {0xE4, 36, 0}, {0xE6, 4, 0},
        {0xE8, 28, 0}, {0xEA, 4, 0}, {0xEC, 36, 0}, {0xEE, 4, 0},
        {0xF0, 24, 0}, {0xF2, 4, 0}, {0xF4, 24, 0}, {0xF6, 4, 0},
        {0xF8, 24, 0}, {0xFA, 4, 0}, {0xFC, 24, 0}, {0xFE, 4, 0},
    };
    for (const Pair& p : k)
    {
        if (p.kind == kind)
        {
            return odd != 0 ? p.hi : p.lo;
        }
    }
    return 0;
}

int trunc_sample(u8 index)
{
    // cvttss2si of the float at 0x00433CA4 + index*12. Missing entries are 0.
    struct Sample
    {
        u8 index;
        i32 value;
    };
    static constexpr Sample k[] = {
        {0x01, 200}, {0x02, 90}, {0x03, 90}, {0x04, 90}, {0x05, 90}, {0x06, 90},
        {0x07, 90}, {0x08, 90}, {0x09, 100}, {0x0A, 150}, {0x0B, 100}, {0x0C, 100},
        {0x0D, 150}, {0x0E, 100}, {0x0F, 2}, {0x10, 100}, {0x11, 10}, {0x12, 100},
        {0x13, 50}, {0x14, 2}, {0x15, 100}, {0x16, 100}, {0x17, 100}, {0x18, 5},
        {0x19, 100}, {0x1A, 100}, {0x1B, 100}, {0x1C, 100}, {0x1D, 100}, {0x1E, 150},
        {0x1F, 100}, {0x20, 100}, {0x21, 100}, {0x22, 100}, {0x23, 100}, {0x24, 100},
        {0x25, 100}, {0x26, 50}, {0x27, 2}, {0x28, 100}, {0x29, 150}, {0x2A, 100},
        {0x2B, 100}, {0x2C, 150}, {0x2D, 100}, {0x2E, 2}, {0x2F, 100}, {0x30, 10},
        {0x31, 100}, {0x32, 50}, {0x33, 2}, {0x34, 100}, {0x35, 100}, {0x36, 100},
        {0x37, 5}, {0x38, 100}, {0x39, 100}, {0x3A, 100}, {0x3B, 100}, {0x3C, 100},
        {0x3D, 150}, {0x3E, 100}, {0x3F, 100}, {0x40, 100}, {0x41, 100}, {0x42, 100},
        {0x43, 100}, {0x44, 100}, {0x45, 50}, {0x46, 2}, {0x47, 100}, {0x48, -1},
        {0x4A, -2}, {0x4B, 2}, {0x5B, (-2147483647 - 1)}, {0x5D, -5}, {0xF4, (-2147483647 - 1)}, {0xFD, (-2147483647 - 1)},
        {0xFE, 146},
    };
    for (const Sample& s : k)
    {
        if (s.index == index)
        {
            return s.value;
        }
    }
    return 0;
}

bool host_special(const ScriptHost& host)
{
    return host.mode == 2 || host.link != nullptr;
}

int sample_bank(u8 index, u16 id)
{
    // 0x001339C0 via 0x00133BD0 / 0x00028DA0. Byte at bank[id*0x87 + index - 0x48].
    // DAT_00545AC4 stays null until that bank is filled in.
    if (g_script_sample_bank == nullptr)
    {
        return 0;
    }
    (void)index;
    (void)id;
    return 0;
}

int sample_plain(const ScriptHost& host, u8 index)
{
    // 0x00028DA0. A zero id uses the float table; otherwise the bank.
    if (host.sample_id == 0)
    {
        return trunc_sample(index);
    }
    return sample_bank(index, host.sample_id);
}

bool host_marked(const ScriptHost& host)
{
    // 0x00028D20. Needs +0xCE0, then mode 2 or a live link.
    if (host.marker == nullptr)
    {
        return false;
    }
    return host_special(host);
}

const ScriptRow* find_row(const ScriptTable& table, u16 key)
{
    // 0x00027510. Signed binary search, unsigned key compare, stride 0x108.
    if (table.rows.empty())
    {
        return nullptr;
    }
    int lo = 0;
    int hi = static_cast<int>(table.rows.size()) - 1;
    while (lo <= hi)
    {
        const int mid = (lo + hi) >> 1;
        const u16 row_key = table.rows[static_cast<size_t>(mid)].key;
        if (row_key < key)
        {
            lo = mid + 1;
        }
        else if (row_key > key)
        {
            hi = mid - 1;
        }
        else
        {
            return &table.rows[static_cast<size_t>(mid)];
        }
    }
    return nullptr;
}

void release_slot(ScriptTarget& target, void*& slot)
{
    if (slot == nullptr)
    {
        return;
    }
    if (target.release != nullptr)
    {
        target.release(slot);
    }
    slot = nullptr;
}

void bind_row(ScriptTarget& target, const ScriptRow& row)
{
    // 0x000276A0. Drop the previous binds, keep this row, then 0x00027830 / 0x00027890
    // if the row text is an aid_ name. Those two allocate; the slots stay clear here.
    release_slot(target, target.slot_314);
    release_slot(target, target.slot_744);
    target.bound = nullptr;
    target.bound_key = 0;
    if (row.extra != nullptr && target.on_extra != nullptr)
    {
        target.on_extra(target, row.extra);
    }
    target.bound = &row;
    target.bound_key = row.key;
    if (target.slot_74c == nullptr)
    {
        target.flag_6c = 0;
        target.slot_84 = nullptr;
    }
    // 0x00027830 (row+0x82) and 0x00027890 (row+2) run when those bytes are the
    // dword 'aid_'. They allocate through 0x00107720 and store +0x314 / +0x744.
}

void apply_note(ScriptTarget& target, u16 id)
{
    // 0x00024B20. Notify host vtable+0xC with {0x61, id}, then bind that row.
    if (target.on_note != nullptr)
    {
        target.on_note(target, 0x61, id);
    }
    if (id == 0 || target.table == nullptr)
    {
        return;
    }
    if (const ScriptRow* row = find_row(*target.table, id))
    {
        bind_row(target, *row);
    }
}

int resolve_sample(const ScriptTarget& target, const ScriptCmd& cmd)
{
    // 0x000238E0. Kind 0x32 is a hard zero. Otherwise the host at target+8 picks
    // the bank path when it is special and the command mark is set.
    if (cmd.kind == 0x32 || target.host == nullptr)
    {
        return 0;
    }
    const ScriptHost& host = *target.host;
    const u8 pair_lo = kind_pair(cmd.kind, 0);
    const u8 pair_hi = kind_pair(cmd.kind, 1);
    if (host_special(host) && cmd.mark != 0)
    {
        return sample_bank(pair_hi, cmd.mark);
    }
    int got = sample_plain(host, pair_lo);
    int flagged = 0;
    if (cmd.ref != nullptr && host_marked(*cmd.ref) && g_script_sample_gate != 0)
    {
        flagged = 1;
    }
    if (got != 0 && flagged != 0)
    {
        return target.counter;
    }
    if (cmd.kind == 0x3F)
    {
        if (got != 0)
        {
            const int half = got / 2;
            got = half > 1 ? half : 1;
        }
        if (target.suppress != nullptr)
        {
            got = 0;
        }
        return got;
    }
    if (cmd.kind == 0x46 && !host_special(host))
    {
        return 0;
    }
    return got;
}

void fn_21c50(ScriptTarget& target, const ScriptCmd& cmd, ScriptOut& out, i32 gate)
{
    // Xbox 0x00021C50
    if (gate != 0)
    {
        return;
    }
    const int resolved = resolve_sample(target, cmd);
    if (resolved == 0)
    {
        return;
    }
    if (target.block == nullptr || (target.block[2] & 2) != 0)
    {
        return;
    }
    if (target.table == nullptr || find_row(*target.table, 0x81) == nullptr)
    {
        return;
    }
    target.at_274 = cmd.param_3c;
    target.at_26c = cmd.param_34;
    target.at_280 = resolved;
    target.at_278 = 0;
    target.at_27c = cmd.opcode;
    target.at_270 = cmd.ref;
    target.at_162 = 0;
    apply_note(target, 0x81);
    out.handled = 1;
}

void script_action_opcode_f()
{
    // 0x00020DC0 dispatches opcode 0xF into fn_21c50 once per action.
    // The slot stays clear until a command is filled in.
    if (!g_script_action.live)
    {
        return;
    }
    g_script_action.live = false;
    if (g_script_action.cmd.opcode != 0x0F)
    {
        return;
    }
    fn_21c50(g_script_action.target, g_script_action.cmd, g_script_action.out, g_script_action.gate);
}

// Focus cylinder built by 0x000426F0. camera_apply (0x00025AE0) reaches it
// through 0x00025C50 once the slot header at host+0x204 is live. Host is
// camera+8. +0x2ac is the actor matrix. +0x8fc is the scratch record list.
// DAT_0050DD80 is a BSS probe; the retail image leaves it at the origin, so an
// attached read is the matrix translation.
struct Mat34
{
    float m[12] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
};

struct CamShapeNode
{
    u8 kind = 0; // 0 sphere (local point +0x14), 1 cylinder (local point +0x18)
    u32 mask = 0; // +4, tested against the caller's mask
    u16 bone = 0xFFFF; // +8, 0xFFFF skips the skeleton
    Vec3 point{};
    float radius_x = 0.f; // cylinder node+0x10, added to local x
    float half_y = 0.f;   // cylinder node+0x14, added to local y
    CamShapeNode* next = nullptr;
};

struct CamSlotEntry
{
    i32 type = 0; // 4 shape list, 5 default scales, 9 bone byte
    CamShapeNode* shape = nullptr; // type 4, first node (blob+8)
    bool scale_live = false;       // type 5 pointer present
    float height_scale = 0.f;      // +0x20
    float radius_scale = 0.f;      // +0x24
    const u8* bone_byte = nullptr; // type 9, second dword; byte 0 is the id
};

struct CamSlotHeader
{
    std::vector<CamSlotEntry> entries;
};

struct CamBonePose
{
    u16 bone = 0xFFFF; // +8, bone id when this object is the radius source
    float along_x = 0.f; // +0x10, sphere radius offset along local x
    bool table_live = false; // +4
    std::vector<Mat34> bones; // +0x10, stride 0x30, indexed by the shape bone
};

struct CamAttach
{
    bool child_live = false; // +8
};

struct CamLink
{
    bool live = false;
};

// [slot+0x1bc]+0x24. Each record is 0x20 bytes; the translation sits at +0x10.
struct CamMarker
{
    Vec3 point{};
};

struct CamMarkerBank
{
    std::vector<CamMarker> items;
};

struct CamSlotObject
{
    CamSlotHeader* header = nullptr; // +0
    CamBonePose* at_164 = nullptr;
    CamBonePose* at_168 = nullptr;
    CamMarkerBank* markers = nullptr; // +0x1bc
};

struct CamBoundRec
{
    u8 kind = 0; // 1 sphere, 4 cylinder
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float radius = 0.f; // +0x10
    float height = 0.f; // +0x14
    float reach = 0.f;  // +0x1c, cylinder radius
};

struct CamBound
{
    u8 kind = 0; // 4
    u8 size = 0; // 0x20
    u8 flag = 0; // 1
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float pad0 = 0.f; // +0x10
    float height = 0.f; // +0x14
    float pad1 = 0.f; // +0x18
    float radius = 0.f; // +0x1c
};

struct CamAnchor;

// Locator record (stride 0x34, word 0 == 3). Mode 0 reads the bone at +2 and
// the position stored at +0x10 / +0x20 / +0x30.
struct CamLocator
{
    i16 bone = -1;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// camera+0x15c. +8 points at a byte block; +7 bit 0 keeps the stored follow cylinder.
struct CamGateBlock
{
    u8 bytes[8] = {};
};

struct CamGate
{
    CamGateBlock* at_8 = nullptr;
};

// host+0x8f8. Two cylinders (0x20 each) and the point 0x00025C50 posts through 0x0012B160.
struct CamFollow
{
    u8 mark = 0; // +0x19
    CamBound at_1c{}; // +0x1c
    CamBound at_3c{}; // +0x3c
    Vec3 at_5c{}; // +0x5c
};

// [[bind+0x58]+0]. +0x14 is the sample, +0x10c the other end, +0x1c the direction.
// +0x2a == 1 arms the end-of-travel note.
struct CamBlend
{
    float value = 0.f;    // +0x14
    float dir = 0.f;      // +0x1c
    u8 mark = 0;          // +0x2a
    float previous = 0.f; // +0x10c
};

struct CamBlendRef
{
    CamBlend* blend = nullptr;
};

// host+0x50. A null +0x58 faults on Xbox when the bind itself is live.
struct CamBind
{
    CamBlendRef* at_58 = nullptr;
};

// Bytes at picker+0x11 and picker+0x13.
struct CamPicker
{
    u8 count = 0;    // +0x11
    u8 selected = 0; // +0x13
};

// Node embedded at object+0x908. prev non-null means the object is already linked.
struct RetireNode
{
    RetireNode* prev = nullptr;
    RetireNode* next = nullptr;
};

struct CamFocus
{
    Vec3 position{}; // +0x08
    float extent_30 = 0.f; // +0x30
    float at_34 = 0.f; // +0x34, height factor of the follow cylinder
    float scale_38 = 0.f; // +0x38
    Vec3 at_3c{}; // +0x3c, copied to the follow packet +0x5c
    CamBind* bind = nullptr; // +0x50
    CamAttach* attach = nullptr; // +0x88
    float scale_200 = 0.f; // +0x200
    CamSlotObject* slots = nullptr; // +0x204
    CamPairObject* pair_204 = nullptr; // +0x204, byte pairs at +0x294
    CamPairObject* pair_310 = nullptr; // +0x310
    CamAnchor* anchors = nullptr; // +0x1f8, next link of the header at +0x1f4
    Vec3 ease{}; // +0x318
    Vec3 ease_to{}; // +0x324
    Mat34 world{}; // +0x2ac
    CamFollow* follow = nullptr; // +0x8f8
    RetireNode retire{}; // +0x908
    CamFocus* release_next = nullptr; // +0x91c
    CamFocus* release_child = nullptr; // +0x920
    float accum = 0.f; // +0x924
    i32 accum_n = 0; // +0x928
    float host_rate = 0.f; // +0x92c
    float host_gate = 0.f; // +0x930
    i32 pulse_on = 0; // +0x934
    i32 pulse_left = 0; // +0x938
    float pulse = 0.f; // +0x93c
    CamPicker* picker = nullptr; // +0x940
    u32 marker = 0; // +0x944
    u32 in_range = 0; // +0x948
    float marker_kick = 0.f; // +0x94c
    float range = 0.f; // +0x950
    CamLink* link = nullptr; // +0x918
    std::vector<CamBoundRec> recs; // +0x8fc
    u32 at_cec = 0; // +0xcec, low byte stored at packet +0x19
    u16 tag_6 = 0; // +6
    u16 word_1fc = 0; // +0x1FC, compared with the dword at node+0x48
    u16 word_cdc = 0; // +0xcdc, link+0x3a8, compared with 2
    Vec3 nudge_704{}; // camera+0x704, added to the stored follow position
    float mul_884 = 0.f; // camera+0x884
    float mul_888 = 0.f; // camera+0x888
    u32 mask_890 = 0; // camera+0x890, shape mask for the averaged bound
    CamGate* gate_15c = nullptr; // camera+0x15c
    u32 shape_mask = 0; // camera+0x88c, the mask 0x00025C50 passes with flag 0
    CamBound bound{};
    void (*on_note)(CamFocus* self, i32 id) = nullptr; // vtable +0xC
};

// Node allocated by 0x0011C0B0 (0x38 bytes) and linked at host+0x1f8.
// +0x14 selects the update. +0x18 is a locator pointer in mode 0 and a bone
// index in modes 1 and 2. +0x1c is the local point. +0x28 is the world point.
struct CamAnchor
{
    CamAnchor* next = nullptr; // +4
    CamFocus* host = nullptr;  // +0x10
    i32 mode = 0;              // +0x14
    CamLocator* locator = nullptr;
    u16 index = 0xFFFF; // +0x18
    Vec3 local{};       // +0x1c
    Vec3 out{};         // +0x28
};

CamFocus g_cam_focus;
Vec3 g_focus_probe{}; // DAT_0050DD80, DAT_0050DD84, DAT_0050DD88

Vec3 mul_mat(const Mat34& a, Vec3 p)
{
    return Vec3{p.x * a.m[0] + p.y * a.m[1] + p.z * a.m[2] + a.m[3],
                p.x * a.m[4] + p.y * a.m[5] + p.z * a.m[6] + a.m[7],
                p.x * a.m[8] + p.y * a.m[9] + p.z * a.m[10] + a.m[11]};
}

float fn_152b30(float value)
{
    // Xbox 0x00152B30
    // Square root. 0x00153450 seeds a reciprocal square root from
    // ((0x7F000000 - bits) >> 1) + 0x1FC00000, multiplies by 1/sqrt(2) when
    // bit 0x08000000 is clear, and takes two Newton steps. Two further steps
    // and a multiply by the input turn that into the root. A non-positive
    // input is returned unchanged.
    if (!(value > 0.f))
    {
        return value;
    }
    const u32 bits = std::bit_cast<u32>(value);
    float y = std::bit_cast<float>(((0x7F000000u - bits) >> 1) + 0x1FC00000u);
    if ((bits & 0x08000000u) == 0u)
    {
        y *= std::bit_cast<float>(u32{0x3F3504F3u});
    }
    const float half = value * 0.5f;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    y = (1.5f - half * y * y) * y;
    return y * value;
}

float length3(Vec3 d)
{
    return fn_152b30(d.x * d.x + d.y * d.y + d.z * d.z);
}

float dist_xz(float x, float z, float cx, float cz)
{
    const float dx = x - cx;
    const float dz = z - cz;
    return fn_152b30(dx * dx + dz * dz);
}

Vec3 focus_world_pos(CamFocus& host)
{
    // 0x000412A0. Attached and live: matrix times the probe. A dead attach is
    // released (0x000663E0) and the unattached path copies host+0x08.
    if (host.attach != nullptr)
    {
        const bool child = host.attach->child_live;
        const bool linked = host.link != nullptr && host.link->live;
        if (child || linked)
        {
            return mul_mat(host.world, g_focus_probe);
        }
        host.attach = nullptr;
    }
    return host.position;
}

const CamShapeNode* find_typed(const CamFocus& host, i32 type, const CamSlotEntry** entry_out)
{
    if (entry_out != nullptr)
    {
        *entry_out = nullptr;
    }
    if (host.slots == nullptr || host.slots->header == nullptr)
    {
        return nullptr;
    }
    for (const CamSlotEntry& entry : host.slots->header->entries)
    {
        if (entry.type != type)
        {
            continue;
        }
        if (entry_out != nullptr)
        {
            *entry_out = &entry;
        }
        return entry.shape;
    }
    return nullptr;
}

float default_extent(const CamFocus& host, bool height)
{
    // 0x0003DB30 reads type-5 +0x24. 0x0003DB90 reads type-5 +0x20.
    // Both scale by host+0x200 and host+0x38, then take the absolute value.
    const CamSlotEntry* entry = nullptr;
    find_typed(host, 5, &entry);
    if (entry == nullptr || !entry->scale_live)
    {
        return 0.f;
    }
    const float scale = height ? entry->height_scale : entry->radius_scale;
    return std::fabs(host.scale_200 * scale * host.scale_38);
}

const CamBonePose* focus_pose(const CamFocus& host)
{
    if (host.slots == nullptr)
    {
        return nullptr;
    }
    if (host.slots->at_168 != nullptr)
    {
        return host.slots->at_168;
    }
    return host.slots->at_164;
}

Vec3 xform_shape(u16 bone, const CamBonePose* pose, const Mat34& world, Vec3 local)
{
    // 0x00056790. Bone 0xFFFF copies the local point. Otherwise 0x000315F0
    // indexes the pose table (stride 0x30). The actor matrix always follows.
    Vec3 p = local;
    if (bone != 0xFFFF && pose != nullptr && pose->table_live)
    {
        const i32 index = static_cast<i16>(bone);
        if (index >= 0 && static_cast<size_t>(index) < pose->bones.size())
        {
            p = mul_mat(pose->bones[static_cast<size_t>(index)], p);
        }
    }
    return mul_mat(world, p);
}

void push_sphere(CamFocus& host, const CamShapeNode& node, const CamBonePose* pose)
{
    // 0x00056330. World position of node+0x14, radius is the world length of
    // the pose's +0x10 offset along local x. The radius point's bone id sits
    // on the pose; the lookup object is the position just written, so the
    // actor matrix is the transform that applies.
    const Vec3 center = xform_shape(node.bone, pose, host.world, node.point);
    Vec3 arm = node.point;
    arm.x += pose != nullptr ? pose->along_x : 0.f;
    const u16 arm_bone = pose != nullptr ? pose->bone : static_cast<u16>(0xFFFF);
    const Vec3 arm_w = xform_shape(arm_bone, nullptr, host.world, arm);
    CamBoundRec rec;
    rec.kind = 1;
    rec.x = center.x;
    rec.y = center.y;
    rec.z = center.z;
    rec.radius = length3(Vec3{center.x - arm_w.x, center.y - arm_w.y, center.z - arm_w.z});
    host.recs.push_back(rec);
}

void push_cylinder(CamFocus& host, const CamShapeNode& node, const CamBonePose* pose)
{
    // 0x00055AA0 / 0x00055B30. Point at node+0x18, radius from the local-x
    // offset at node+0x10, half-extent from the local-y offset at node+0x14.
    // The record stores the bottom and the full height.
    const Vec3 base = xform_shape(node.bone, pose, host.world, node.point);
    Vec3 along_x = node.point;
    along_x.x += node.radius_x;
    Vec3 along_y = node.point;
    along_y.y += node.half_y;
    const Vec3 wx = xform_shape(node.bone, pose, host.world, along_x);
    const Vec3 wy = xform_shape(node.bone, pose, host.world, along_y);
    const float reach = length3(Vec3{base.x - wx.x, base.y - wx.y, base.z - wx.z});
    const float hy = length3(Vec3{base.x - wy.x, base.y - wy.y, base.z - wy.z});
    CamBoundRec rec;
    rec.kind = 4;
    rec.x = base.x;
    rec.y = base.y - hy;
    rec.z = base.z;
    rec.height = hy - (-hy);
    rec.reach = reach;
    host.recs.push_back(rec);
}

void write_bound(CamBound& out, float x, float y, float z, float height, float radius)
{
    out.kind = 4;
    out.size = 0x20;
    out.flag = 1;
    out.x = x;
    out.y = y;
    out.z = z;
    out.pad0 = 0.f;
    out.height = height;
    out.pad1 = 0.f;
    out.radius = radius;
}

int fn_426f0(CamFocus& host, CamBound& out, u32 mask, i32 average)
{
    // Xbox 0x000426F0
    const Vec3 probed = focus_world_pos(host);
    const CamSlotEntry* shape_entry = nullptr;
    find_typed(host, 4, &shape_entry);
    if (shape_entry == nullptr || shape_entry->shape == nullptr)
    {
        write_bound(out, probed.x, probed.y, probed.z, default_extent(host, true), default_extent(host, false));
        return 0;
    }

    const CamBonePose* pose = focus_pose(host);
    host.recs.clear();
    for (const CamShapeNode* node = shape_entry->shape; node != nullptr; node = node->next)
    {
        if ((node->mask & mask) == 0)
        {
            continue;
        }
        if (node->kind == 0)
        {
            push_sphere(host, *node, pose);
        }
        else if (node->kind == 1)
        {
            push_cylinder(host, *node, pose);
        }
    }
    if (host.recs.empty())
    {
        return 0;
    }

    Vec3 center = probed;
    if (average != 0)
    {
        float sx = 0.f;
        float sy = 0.f;
        float sz = 0.f;
        for (const CamBoundRec& rec : host.recs)
        {
            sx += rec.x;
            sz += rec.z;
            if (rec.kind == 4)
            {
                sy += rec.y + rec.height;
            }
            else
            {
                sy += rec.y;
            }
        }
        const float inv = 1.f / float(host.recs.size());
        center = Vec3{sx * inv, sy * inv, sz * inv};
    }
    else
    {
        center = focus_world_pos(host);
    }

    float min_y = center.y;
    float max_y = center.y;
    float max_r = 0.f;
    for (const CamBoundRec& rec : host.recs)
    {
        if (rec.kind == 1)
        {
            const float horiz = dist_xz(rec.x, rec.z, center.x, center.z) + rec.radius;
            if (horiz > max_r)
            {
                max_r = horiz;
            }
            const float bottom = rec.y - rec.radius;
            if (bottom <= min_y)
            {
                min_y = bottom;
            }
            const float top = rec.radius + rec.y;
            if (max_y < top)
            {
                max_y = top;
            }
        }
        else if (rec.kind == 4)
        {
            const float horiz = dist_xz(rec.x, rec.z, center.x, center.z) + rec.reach;
            if (horiz > max_r)
            {
                max_r = horiz;
            }
            if (rec.y <= min_y)
            {
                min_y = rec.y;
            }
            const float top = rec.height + rec.y;
            if (max_y < top)
            {
                max_y = top;
            }
        }
    }
    write_bound(out, center.x, min_y, center.z, max_y - min_y, max_r);
    return int(host.recs.size());
}

Mat34 mul_affine(const Mat34& a, const Mat34& b)
{
    // 0x001041B0. 3x4 affine product, out = A * B.
    Mat34 o;
    for (int row = 0; row < 3; ++row)
    {
        const float ax = a.m[row * 4 + 0];
        const float ay = a.m[row * 4 + 1];
        const float az = a.m[row * 4 + 2];
        const float aw = a.m[row * 4 + 3];
        o.m[row * 4 + 0] = ax * b.m[0] + ay * b.m[4] + az * b.m[8];
        o.m[row * 4 + 1] = ax * b.m[1] + ay * b.m[5] + az * b.m[9];
        o.m[row * 4 + 2] = ax * b.m[2] + ay * b.m[6] + az * b.m[10];
        o.m[row * 4 + 3] = ax * b.m[3] + ay * b.m[7] + az * b.m[11] + aw;
    }
    return o;
}

const Mat34* pose_bone(const CamBonePose* pose, i32 index)
{
    // 0x00031530. +4 set: packed matrices at +0x10, stride 0x30.
    // +4 clear packs a column-major 4x4 (stride 0x40) into that same shape.
    // The port keeps the packed table, so a clear flag has no bone.
    if (pose == nullptr || !pose->table_live || index < 0)
    {
        return nullptr;
    }
    const size_t slot = static_cast<size_t>(index);
    if (slot >= pose->bones.size())
    {
        return nullptr;
    }
    return &pose->bones[slot];
}

bool anchor_bone_point(const CamFocus& host, u16 index, Vec3 probe, Vec3& out)
{
    // 0x0003AEB0. Marker translation (stride 0x20 at [slot+0x1bc]+0x24)
    // becomes T. Bone matrix is 0x00031530 on the same index. Result is
    // (world * bone * T) * probe. Probe is DAT_0050DD80. Writes when the
    // marker record is present.
    if (host.slots == nullptr || host.slots->markers == nullptr)
    {
        return false;
    }
    const std::vector<CamMarker>& items = host.slots->markers->items;
    if (static_cast<size_t>(index) >= items.size())
    {
        return false;
    }
    Mat34 bone{};
    if (const Mat34* found = pose_bone(focus_pose(host), static_cast<i32>(index)))
    {
        bone = *found;
    }
    Mat34 trans{};
    trans.m[3] = items[index].point.x;
    trans.m[7] = items[index].point.y;
    trans.m[11] = items[index].point.z;
    const Mat34 placed = mul_affine(mul_affine(host.world, bone), trans);
    out = mul_mat(placed, probe);
    return true;
}

void fn_11be40(CamAnchor& node)
{
    // Xbox 0x0011BE40
    // EDX is the anchor. Mode +0x14:
    //   0: locator position, optional bone, then the actor matrix (0x00051CE0)
    //   1: bone/marker probe into +0x28 (0x0003AEB0, EBX = DAT_0050DD80)
    //   2: index 0xFFFF maps +0x1c by the actor matrix (0x000415F0);
    //      any other index takes the mode-1 probe path
    if (node.host == nullptr)
    {
        return;
    }
    CamFocus& host = *node.host;
    if (node.mode == 0)
    {
        if (node.locator == nullptr)
        {
            return;
        }
        Vec3 p{node.locator->x, node.locator->y, node.locator->z};
        if (node.locator->bone >= 0)
        {
            if (const Mat34* bone = pose_bone(focus_pose(host), node.locator->bone))
            {
                p = mul_mat(*bone, p);
            }
        }
        node.out = mul_mat(host.world, p);
        return;
    }
    if (node.mode == 1)
    {
        anchor_bone_point(host, node.index, g_focus_probe, node.out);
        return;
    }
    if (node.mode == 2)
    {
        if (node.index == 0xFFFF)
        {
            node.out = mul_mat(host.world, node.local);
            return;
        }
        anchor_bone_point(host, node.index, g_focus_probe, node.out);
    }
}

// DAT_00509B74. Nonzero eases +0x318 toward +0x324; zero snaps.
i32 g_ease_live = 0;
// DAT_0050E768. Nonzero skips the +0x930 decay inside the base update.
i32 g_block_decay = 0;
// DAT_0050E764. 0x00117400 counts the other mask bit beside DAT_0050E768.
i32 g_count_e764 = 0;
// Byte at [DAT_0051248C]+0x30F144. Zero skips that same decay.
u8 g_decay_arm = 0;
// DAT_00511B40. Null, and the retail sentinel 0x93C, both leave the marker clear.
// The live byte is [[[actor+0x20]+0x15C]+8]+5; bit 4 is the marker.
struct ActorMark
{
    u8 byte = 0;
};
ActorMark* g_actor_mark = nullptr;
// DAT_005077A8. XYZ at [0], [1], [2]. A null pointer faults on Xbox.
const float* g_listener_pos = nullptr;
// DAT_0042742C, copied into the tint block while the host is inside its range.
u32 g_tint_src = 0x800000ffu;
// DAT_00504450 / 54 / 58, then DAT_0050446C / 70 / 74.
struct RangeTint
{
    u32 c0 = 0;
    u32 c1 = 0;
    u32 c2 = 0;
    float n0 = 0.f;
    float n1 = 0.f;
    i32 flag = 0;
};
RangeTint g_range_tint;
// DAT_004E1C5C. 0x000703A0 writes the flag then the world position.
struct AidHit
{
    u32 flags = 0;
    Vec3 pos{};
};
AidHit g_aid_hit;
// DAT_00511B38. Each node is an object+0x934 link; next is at +4.
struct RangeLink
{
    RangeLink* next = nullptr;
    CamFocus* host = nullptr;
    void (*on_note)(CamFocus* self, i32 id) = nullptr;
};
RangeLink* g_range_links = nullptr;
// DAT_00511B20. First node of the DAT_00511B1C list. The node is object+0x934
// and the next pointer is at +4. link-0x730 is the pair at object+0x204.
// link-0x624 is the pair at object+0x310.
struct BodyLink
{
    BodyLink* next = nullptr;
    CamPairObject* pair_204 = nullptr;
    CamPairObject* pair_310 = nullptr;
};
BodyLink* g_body_links = nullptr;
// DAT_00511B48. Next of the DAT_00511B44 head. The node is object+0x944 and
// the next pointer is at +4. object+6 is the actor class key. The byte at
// object+0x969 is node+0x25. The word at object+0x990 is node+0x4c. The
// pointer at object+0x958 is node+0x14. The pointer at object+0x95c is
// node+0x18. The word at that object's +0x94 is the count 0x00132680 reads.
struct AltSlot
{
    void* target = nullptr; // +8, vtable object
    void* flag_10 = nullptr; // +0x10
    u32 at_6c = 0; // +0x6c. Handler 15 type 0x8 stores the dword at node+0x48.
    void (*on_message)(void* self, const u32* msg) = nullptr; // vtable +0xC
};
// Object at node+0x18 (actor+0x95c).
struct AltAt18
{
    u16 word_94 = 0; // +0x94
};
struct AltLink
{
    AltLink* next = nullptr;
    Actor* actor = nullptr;
    u8 byte_969 = 0;
    u16 word_990 = 0;
    AltSlot* slot = nullptr;
    AltAt18* at_18 = nullptr; // node+0x18
    void (*on_message)(Actor* self, const u32* msg) = nullptr; // object vtable +0xC
};
AltLink* g_alt_links = nullptr;
// Head standing in for DAT_00511B54. 0x0003ED70 appends host+0x908 here.
RetireNode g_retire_head;

constexpr float kSixPi = std::bit_cast<float>(0x4196cbe4u);
constexpr float kSpan = std::bit_cast<float>(0x3f733333u); // 0.95
constexpr float kThird = std::bit_cast<float>(0x3eaaaaabu);
constexpr float kEaseStep = std::bit_cast<float>(0x3dcccccdu); // 0.1
constexpr float kKick = std::bit_cast<float>(0x3e4ccccdu); // 0.2

CamBlend* host_blend(const CamFocus& host)
{
    if (host.bind == nullptr || host.bind->at_58 == nullptr)
    {
        return nullptr;
    }
    return host.bind->at_58->blend;
}

int slot_bone(const CamSlotObject* slots)
{
    // 0x00052FC0 with a zero word. First type-9 entry, then the byte at its blob.
    // 0xFF and a missing entry become -1.
    if (slots == nullptr || slots->header == nullptr)
    {
        return -1;
    }
    for (const CamSlotEntry& entry : slots->header->entries)
    {
        if (entry.type != 9)
        {
            continue;
        }
        if (entry.bone_byte == nullptr)
        {
            return -1;
        }
        if (entry.bone_byte[0] == 0xFF)
        {
            return -1;
        }
        return int(entry.bone_byte[0]);
    }
    return -1;
}

float host_range(const CamFocus& host)
{
    // 0x00070410. Trace of the bone matrix times the mean of +0x30/+0x34/+0x38,
    // each scaled by +0x200 and by 1/3. A missing pose faults on Xbox.
    const CamBonePose* pose = nullptr;
    if (host.slots != nullptr)
    {
        pose = host.slots->at_168 != nullptr ? host.slots->at_168 : host.slots->at_164;
    }
    const Mat34* mat = pose_bone(pose, slot_bone(host.slots));
    if (mat == nullptr)
    {
        return 0.f;
    }
    const float s30 = host.extent_30 * host.scale_200;
    const float s34 = host.at_34 * host.scale_200;
    const float s38 = host.scale_38 * host.scale_200;
    const float trace = mat->m[10] + mat->m[5] + mat->m[0];
    return (trace * kThird) * ((s38 + s34 + s30) * kThird);
}

void host_base(CamFocus& host)
{
    // 0x000420D0. This override calls the base slot first.
    // 0x00030BA0 and 0x00030C70 run only when +0x50 is live, and 0x0003CD00
    // walks +0x1B8. Both stay idle while those pointers are null. The list at
    // +0x1D8 is the same. 0x00165A00 runs when slot+0x29C is live.
    for (CamAnchor* node = host.anchors; node != nullptr; node = node->next)
    {
        fn_11be40(*node);
    }
    if (g_ease_live != 0)
    {
        host.ease.x = host.ease.x + (host.ease_to.x - host.ease.x) * kEaseStep;
        host.ease.y = host.ease.y + (host.ease_to.y - host.ease.y) * kEaseStep;
        host.ease.z = host.ease.z + (host.ease_to.z - host.ease.z) * kEaseStep;
    }
    else
    {
        host.ease = host.ease_to;
    }
    const float step = sim_step(g_sim_dt);
    if (g_block_decay == 0 && g_decay_arm != 0 && host.host_gate > 0.f)
    {
        host.host_gate -= step;
        if (host.host_gate <= 0.f)
        {
            host.host_gate = 0.f;
        }
    }
    float rate = 0.f;
    if (!(host.host_gate > 0.f))
    {
        rate = host.host_rate;
    }
    host.accum += rate * step;
    host.accum_n += 1;
}

u8 index_byte(float scaled)
{
    // 0x001673D8 (__ftol). The caller keeps the low 8 bits.
    if (!std::isfinite(scaled))
    {
        return 0;
    }
    const float chopped = std::trunc(scaled);
    if (chopped >= 2147483647.f || chopped <= -2147483648.f)
    {
        return 0;
    }
    return static_cast<u8>(static_cast<i32>(chopped));
}

void pick_index(CamFocus& host, float sample)
{
    if (host.picker == nullptr)
    {
        return;
    }
    const float wave = std::fabs(std::sin(sample * kSixPi));
    const float scaled = wave * (float(host.picker->count) - 1.f);
    const u8 idx = index_byte(scaled);
    if (idx < host.picker->count)
    {
        host.picker->selected = idx;
    }
}

bool span_crossed(float hi, float lo)
{
    if (hi <= lo)
    {
        return (lo <= kSpan) || (hi > kSpan);
    }
    return (lo <= kSpan) && (hi > kSpan);
}

bool at_travel_end(const CamBlend& blend)
{
    if (blend.dir < 0.f)
    {
        return blend.value <= 0.f;
    }
    return blend.value >= 1.f;
}

void retire_host(CamFocus& host)
{
    // 0x0003ED70 links +0x908 onto DAT_00511B58 when that dword is still null,
    // then 0x0003CBB0 / 0x0003E850 / 0x0003C700. The link is what blocks a repeat.
    if (host.retire.prev != nullptr)
    {
        return;
    }
    RetireNode* tail = &g_retire_head;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &host.retire;
    host.retire.prev = tail;
    host.retire.next = nullptr;
}

void release_tree(CamFocus* node)
{
    // 0x00112D10. Child at +8, next at +4, object is the link minus 0x918.
    while (node != nullptr)
    {
        if (node->release_child != nullptr)
        {
            release_tree(node->release_child);
        }
        CamFocus* next = node->release_next;
        if (node->retire.prev == nullptr)
        {
            if (node->on_note != nullptr)
            {
                node->on_note(node, 0x80);
            }
            if (node->retire.prev == nullptr)
            {
                retire_host(*node);
            }
        }
        node = next;
    }
}

void release_host(CamFocus& host)
{
    // 0x00112CC0.
    if (host.release_child != nullptr)
    {
        release_tree(host.release_child);
    }
    if (host.retire.prev != nullptr)
    {
        return;
    }
    if (host.on_note != nullptr)
    {
        host.on_note(&host, 0x80);
    }
    if (host.retire.prev != nullptr)
    {
        return;
    }
    retire_host(host);
}

void post_cross(CamFocus& host)
{
    // 0x000703A0. World position lands in DAT_004E1C60 and the flag becomes 1.
    // 0x00053170 continues only when +0x954 starts with "aid_".
    g_aid_hit.flags = 0;
    g_aid_hit.pos = focus_world_pos(host);
    g_aid_hit.flags = 1;
}

void notify_range(const Vec3& pos, float radius)
{
    // 0x00122D80. Squared test is strict. Note id is 0x5B through vtable+0xC.
    const float limit = radius * radius;
    for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
    {
        if (link->host == nullptr)
        {
            continue;
        }
        const Vec3 at = focus_world_pos(*link->host);
        const float dx = at.x - pos.x;
        const float dy = at.y - pos.y;
        const float dz = at.z - pos.z;
        if (limit > dx * dx + dy * dy + dz * dz && link->on_note != nullptr)
        {
            link->on_note(link->host, 0x5b);
        }
    }
}

void fn_70006(CamFocus& host)
{
    // Xbox 0x00070006
    // Retail entry is 0x0006FF90 (vtable slot +0x10, over 0x000420D0). Ghidra
    // split the body at the fsub that scales the picker index. ESI is the host
    // passed on the stack. 0x000412A0 writes the world position used for the
    // range test against DAT_005077A8.
    host_base(host);
    // A live bind with a null +0x58 faults on Xbox before the picker.
    if (host.bind != nullptr && host.bind->at_58 == nullptr)
    {
        return;
    }

    float sample = 0.f;
    if (host.bind != nullptr && host.bind->at_58->blend != nullptr)
    {
        sample = host.bind->at_58->blend->value;
    }
    pick_index(host, sample);

    if (host.pulse_on != 0)
    {
        float rate = 0.f;
        if (!(host.host_gate > 0.f))
        {
            rate = host.host_rate;
        }
        host.pulse -= rate * sim_step(g_sim_dt);
        if (host.pulse <= 0.f)
        {
            host.pulse += 0.5f;
            host.pulse_left -= 1;
            if (host.pulse_left == 0)
            {
                host.pulse_on = 0;
            }
        }
    }

    const u32 previous = host.marker;
    u32 bit = 0;
    if (g_actor_mark != nullptr)
    {
        bit = (u32(g_actor_mark->byte) >> 4) & 1u;
    }
    host.marker = bit;
    if (bit != 0 && previous == 0)
    {
        host.marker_kick = kKick;
    }

    host.range = host_range(host);
    const Vec3 world = focus_world_pos(host);
    u32 inside = 0;
    if (g_listener_pos != nullptr)
    {
        const Vec3 d{world.x - g_listener_pos[0], world.y - g_listener_pos[1], world.z - g_listener_pos[2]};
        inside = length3(d) <= host.range ? 1u : 0u;
    }
    host.in_range = inside;
    if (inside != 0)
    {
        g_range_tint.c0 = g_tint_src;
        g_range_tint.c1 = g_tint_src;
        g_range_tint.c2 = g_tint_src;
        g_range_tint.n0 = -1.f;
        g_range_tint.n1 = -1.f;
        g_range_tint.flag = 1;
    }
    notify_range(world, host.range);

    CamBlend* blend = host_blend(host);
    if (host.bind != nullptr && host.bind->at_58 != nullptr && blend != nullptr && blend->value != blend->previous)
    {
        float hi = blend->value;
        float lo = blend->previous;
        if (blend->dir < 0.f)
        {
            hi = blend->previous;
            lo = blend->value;
        }
        if (span_crossed(hi, lo))
        {
            post_cross(host);
        }
    }

    if (host.bind == nullptr)
    {
        release_host(host);
        return;
    }
    if (host.bind->at_58 == nullptr)
    {
        return;
    }
    if (blend == nullptr || (blend->mark == 1 && at_travel_end(*blend)))
    {
        release_host(host);
    }
}

void fn_12b160(CamFollow& self, const CamBound& primary, const CamBound* secondary, const Vec3& point, u8 mark)
{
    // Xbox 0x0012B160
    // ESI is host+0x8f8. 0x0010AA60 copies 0x20 bytes to +0x1c from the first
    // stack argument, then to +0x3c from EBX. A null EBX repeats the first
    // source. EDI's three floats land at +0x5c. The stack byte lands at +0x19.
    self.at_1c = primary;
    if (secondary != nullptr)
    {
        self.at_3c = *secondary;
    }
    else
    {
        self.at_3c = primary;
    }
    self.at_5c = point;
    self.mark = mark;
}

bool follow_shift_set(const CamFocus& host)
{
    // camera+0x15c -> +8 -> byte +7. Bit 0 keeps the stored follow cylinder.
    if (host.gate_15c == nullptr || host.gate_15c->at_8 == nullptr)
    {
        return false;
    }
    return (host.gate_15c->at_8->bytes[7] & 1u) != 0;
}

void focus_bounds_tick()
{
    // 0x00025C50 calls fn_426f0 with camera+0x88c and flag 0 whenever the
    // slot header is present. Flag 0 keeps the actor position as the center.
    // When host+0x8f8 is set, the follow look-at packets go out through
    // fn_12b160 first. Bit 0 of the gate shifts the stored cylinder; otherwise
    // the primary cylinder is fn_426f0 with camera+0x890 and the average flag.
    if (g_cam_focus.slots == nullptr || g_cam_focus.slots->header == nullptr)
    {
        return;
    }
    if (g_cam_focus.follow != nullptr)
    {
        CamFollow& follow = *g_cam_focus.follow;
        const Vec3 world = focus_world_pos(g_cam_focus);
        CamBound actor{};
        write_bound(actor, world.x, world.y, world.z, g_cam_focus.at_34 * g_cam_focus.mul_884,
                    g_cam_focus.scale_38 * g_cam_focus.mul_888);
        CamBound primary{};
        if (follow_shift_set(g_cam_focus))
        {
            write_bound(primary, follow.at_1c.x + g_cam_focus.nudge_704.x, follow.at_1c.y + g_cam_focus.nudge_704.y,
                        follow.at_1c.z + g_cam_focus.nudge_704.z, follow.at_1c.height, follow.at_1c.radius);
        }
        else
        {
            fn_426f0(g_cam_focus, primary, g_cam_focus.mask_890, 1);
        }
        fn_12b160(follow, primary, &actor, g_cam_focus.at_3c, static_cast<u8>(g_cam_focus.at_cec));
    }
    fn_426f0(g_cam_focus, g_cam_focus.bound, g_cam_focus.shape_mask, 0);
}

u16* fn_1660e9(u16* dest, const u16* src)
{
    // Xbox 0x001660E9
    // libc wcscpy, cdecl. Each 16-bit unit is stored, then both pointers
    // advance, until the unit just stored is zero. The destination pointer
    // is the return value. The SEH wrapper at 0x0014A961 pushes the source
    // and destination, then pops both after this returns.
    u16* out = dest;
    u16 unit = 0;
    do
    {
        unit = *src;
        *out = unit;
        ++out;
        ++src;
    } while (unit != 0);
    return dest;
}

int fn_166ecb(const u16* text)
{
    // Xbox 0x00166ECB
    // libc wcslen, cdecl. Each 16-bit unit is read, then the pointer
    // advances, until the unit just read is zero. The walked span includes
    // that terminator, so the returned count is one less.
    const u16* cursor = text;
    u16 unit = 0;
    do
    {
        unit = *cursor;
        ++cursor;
    } while (unit != 0);
    return static_cast<int>(cursor - text) - 1;
}

// Xbox 0x00209788. C-locale ctype words for bytes 0..255. _pctype points here.
// The low byte holds _UPPER 0x01, _LOWER 0x02, _DIGIT 0x04, _SPACE 0x08,
// _PUNCT 0x10, _CONTROL 0x20, _BLANK 0x40, and _HEX 0x80. The high byte is
// clear in the image. Bytes 0x09..0x0D and 0x20 are the ones with _SPACE.
constexpr u16 kCrtCtype[256] = {
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0010, 0x0010, 0x0010, 0x0020,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// Xbox 0x00338180. _pctype. The image pointer is 0x00209788.
const u16* g_crt_pctype = kCrtCtype;

// Xbox 0x00338188. __mb_cur_max. The image dword is 1, so isspace takes the
// single-byte path. A signed compare greater than 1 uses _isctype.
int g_crt_mb_cur_max = 1;

// Xbox 0x0033818C. C-locale decimal point. The image byte is '.'. _winput
// widens it with mbtowc (0x0016E07A).
char g_crt_decimal_point = '.';

// Xbox 0x0020998A. C-locale _pwctype words for units 0..255. The low byte
// uses the same bits as _pctype. Units 0x80..0xFF are classified here; the
// narrow table leaves those words zero. 0x09..0x0D, 0x20, and 0xA0 have
// _SPACE (8).
constexpr u16 kWctype[256] = {
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0068, 0x0028, 0x0028, 0x0028, 0x0028, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0081, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0082, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0010, 0x0010, 0x0010, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0014, 0x0014, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0014, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0010, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0010, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
};

// Xbox 0x00338184. _pwctype. The image pointer is 0x0020998A.
const u16* g_crt_pwctype = kWctype;

int fn_166bf9(u16 c, u16 mask)
{
    // Xbox 0x00166BF9
    // iswctype. cdecl; the caller pops both word arguments. WEOF (0xFFFF)
    // and every unit from 0x100 up yield 0. Retail still masks that zero
    // with the caller's bits. Any other unit returns the _pwctype word
    // masked with those bits. _winput (0x0016A17C) passes mask 8 for
    // whitespace. The hex adjuster at 0x0016A101 passes mask 2 and
    // subtracts 0x27 when the result is nonzero.
    if (c == 0xffffu || c > 0xffu)
    {
        return 0;
    }
    return static_cast<int>(g_crt_pwctype[c] & mask);
}

int fn_16ae1f(int c, unsigned mask)
{
    // Xbox 0x0016AE1F
    // _isctype. cdecl; the caller pops both arguments. isspace (0x00168883)
    // and the scanf classifiers call here when __mb_cur_max is above 1.
    // (unsigned)(c + 1) above 0x100 returns 0, so the accepted values are
    // -1..255. The ushort at 0x00209786, immediately before _pctype, is 0,
    // so EOF still returns 0. Every other accepted value returns the ctype
    // word masked with the caller's bits.
    if (static_cast<unsigned>(c) + 1u > 0x100u)
    {
        return 0;
    }
    const u16 entry = (c < 0) ? static_cast<u16>(0) : g_crt_pctype[c];
    return static_cast<int>(entry & mask);
}

int fn_168883(u32 c)
{
    // Xbox 0x00168883
    // isspace. A value above 0xFF returns 0, which covers EOF passed as
    // 0xFFFFFFFF. The compare with __mb_cur_max is signed (jle). At 1 or
    // below, the low byte of _pctype[c] is masked with _SPACE (8). Above 1,
    // _isctype (0x0016AE1F) is called with that same mask: (unsigned)(c + 1)
    // above 0x100 returns 0, otherwise the full ctype word is masked with 8.
    // c is already in 0..255 on that path, so the range test does not fire.
    // The whitespace skip at 0x0016605A tests the return for nonzero. The
    // bit itself is 8 or 0.
    if (c > 0xffu)
    {
        return 0;
    }
    if (g_crt_mb_cur_max > 1)
    {
        return fn_16ae1f(static_cast<int>(c), 8u);
    }
    return static_cast<int>(static_cast<u8>(g_crt_pctype[c]) & 8u);
}

int fn_16b050(int c)
{
    // Xbox 0x0016B050
    // tolower. cdecl; the caller pops the character. The compare with
    // __mb_cur_max is signed (cmp 1 / jle). At 1 or below, the low byte of
    // _pctype[c] is masked with _UPPER (1). Above 1, _isctype (0x0016AE1F)
    // is called with that same mask. A nonzero result returns c + 0x20;
    // zero returns c. The image __mb_cur_max is 1, so the table path runs.
    // There is no range check: the address is _pctype + c * 2. _stricmp
    // (0x00166EE1) and _strnicmp (0x00167213) zero-extend each source byte.
    // __forcdecpt (0x0016B141) sign-extends one byte and compares the result
    // with 'e' (0x65). A value outside 0..255 would walk off the table; the
    // port returns it unchanged. EOF (-1) is that case, and the ushort just
    // before _pctype is 0, so retail also returns it unchanged.
    unsigned upper = 0;
    if (g_crt_mb_cur_max > 1)
    {
        upper = static_cast<unsigned>(fn_16ae1f(c, 1u));
    }
    else if (static_cast<unsigned>(c) < 256u)
    {
        upper = static_cast<unsigned>(static_cast<u8>(g_crt_pctype[c]) & 1u);
    }
    if (upper == 0)
    {
        return c;
    }
    return c + 0x20;
}

char* fn_16b308(double* value, char* buffer, int ndec, int caps)
{
    // Xbox 0x0016B308
    // _cftoe. __cfltcvt (0x0016B4EC) calls here for 'e' and 'E', passing the
    // double pointer, the output buffer, the precision, and the caps flag
    // (nonzero writes 'E'). The return is that buffer pointer.
    //
    // _fltout (0x0016F027) turns the double into a _strflt on the stack: the
    // sign dword is '-' or ' ' (0x20), the decpt dword is the exponent plus
    // one, and the mantissa pointer aims at a 24-byte digit buffer. _dtold
    // (0x0016EF6D) drops the sign of a zero, so -0.0 prints as a positive
    // zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits. A zero mantissa
    // is the single character '0' and decpt 0. An exponent of 0x7FFF selects
    // the image strings at 0x00209EEC..0x00209EFC: "1#INF", "1#IND",
    // "1#QNAN", or "1#SNAN", with decpt 1. The quiet bit distinguishes a
    // QNAN from an SNAN, and the negative quiet NaN with a zero payload is
    // "1#IND".
    //
    // _fptostr (0x0016EEEE) writes precision + 1 digits one byte in from a
    // slot that already accounts for a minus and, when precision is positive,
    // the decimal point. It plants a leading '0', copies digits (padding
    // '0' past the end of the mantissa), and rounds when the next character
    // is at least '5', carrying through '9'. A carry into that leading '0'
    // increments decpt and keeps the extra digit. Otherwise the guard byte
    // is shifted off. The stored mantissa pointer is left on the first
    // character; the zero test below still sees it.
    //
    // 0x0016B25F then lays the text out. This call passes 0 for its format
    // byte, so the %g shift is skipped. A minus is stored, a positive
    // precision pulls the first digit forward and writes the decimal point
    // from 0x0033818C ('.'), and "e+000" from 0x00209B9C is copied at
    // digit_end. A nonzero caps flag replaces 'e' with 'E'. A mantissa that
    // still starts with '0' leaves the exponent at "+000". Any other value
    // writes decpt - 1, three digits, with '-' over the '+' when the power
    // is negative.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    const char* man = mantissa;
    char* placed = buffer + (ndec > 0 ? 1 : 0) + (sign == '-' ? 1 : 0);
    placed[0] = '0';
    char* out = placed + 1;
    if (ndec + 1 > 0)
    {
        int remaining = ndec + 1;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (ndec + 1 >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (placed[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        char* src = placed + 1;
        char* dst = placed;
        while (true)
        {
            const char ch = *src;
            src += 1;
            *dst = ch;
            dst += 1;
            if (ch == '\0')
            {
                break;
            }
        }
    }

    char* text = buffer;
    if (sign == '-')
    {
        *text = '-';
        text += 1;
    }
    if (ndec > 0)
    {
        char* dot = text + 1;
        *text = *dot;
        *dot = '.';
        text = dot;
    }
    char* exp_at = text + ndec + 1;
    std::memcpy(exp_at, "e+000", 6);
    if (caps != 0)
    {
        *exp_at = 'E';
    }
    exp_at += 1;
    if (mantissa[0] != '0')
    {
        int power = decpt - 1;
        if (power < 0)
        {
            power = -power;
            *exp_at = '-';
        }
        exp_at += 1;
        if (power >= 100)
        {
            *exp_at = static_cast<char>(*exp_at + power / 100);
            power %= 100;
        }
        exp_at += 1;
        if (power >= 10)
        {
            *exp_at = static_cast<char>(*exp_at + power / 10);
            power %= 10;
        }
        exp_at[1] = static_cast<char>(exp_at[1] + power);
    }
    return buffer;
}

char* fn_16b40d(double* value, char* buffer, int ndec)
{
    // Xbox 0x0016B40D
    // _cftof. __cfltcvt (0x0016B4EC) calls here for 'f', passing the double
    // pointer, the output buffer, and the precision. The return is that
    // buffer pointer.
    //
    // _fltout (0x0016F027) fills a stack _strflt the same way _cftoe does:
    // sign '-' or ' ' (0x20), decpt = exponent + 1, mantissa in a 24-byte
    // buffer. _dtold (0x0016EF6D) drops the sign of a zero, so -0.0 prints
    // as a positive zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits.
    // A zero is the character '0' and decpt 0. An exponent of 0x7FFF selects
    // "1#INF", "1#IND", "1#QNAN", or "1#SNAN" with decpt 1.
    //
    // _fptostr (0x0016EEEE) is called with a count of decpt + precision and
    // a destination one past a slot reserved for a minus. There is no gap
    // for the decimal point. It plants a leading '0', copies that many
    // digits (padding '0' past the mantissa), and rounds when the next
    // character is at least '5'. A count below 0 writes an empty string and
    // does not round. A count of 0 still rounds from the first mantissa
    // digit. A carry into the guard increments decpt and keeps the extra
    // digit; otherwise the guard is shifted off.
    //
    // 0x0016B366 then lays the text out. EAX holds the _strflt; the stack
    // arguments are the buffer, the precision, and a format byte. This call
    // passes 0, so the %g truncation (a trailing '0' when decpt - 1 equals
    // the precision) is skipped. A minus is stored. A decpt of 0 or below
    // shifts the digits right and writes a leading '0'; a positive decpt
    // steps over that many digits. A positive precision shifts in the
    // decimal point from 0x0033818C ('.'). When decpt is still negative,
    // zeros are inserted after that point: -decpt of them when the
    // precision is at least that large, otherwise the precision itself.
    // 0x0016B23C is the shift, a memmove of the tail including the NUL.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    const char* man = mantissa;
    char* placed = buffer + (sign == '-' ? 1 : 0);
    const int digits = decpt + ndec;
    placed[0] = '0';
    char* out = placed + 1;
    if (digits > 0)
    {
        int remaining = digits;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (digits >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (placed[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        std::memmove(placed, placed + 1, std::strlen(placed + 1) + 1);
    }

    auto shift_right = [](char* text, int count)
    {
        if (count == 0)
        {
            return;
        }
        std::memmove(text + count, text, std::strlen(text) + 1);
    };

    char* text = buffer;
    if (sign == '-')
    {
        *text = '-';
        text += 1;
    }
    if (decpt <= 0)
    {
        shift_right(text, 1);
        *text = '0';
        text += 1;
    }
    else
    {
        text += decpt;
    }
    if (ndec > 0)
    {
        shift_right(text, 1);
        *text = '.';
        text += 1;
        if (decpt < 0)
        {
            int zeros = -decpt;
            if (ndec >= zeros)
            {
                ndec = zeros;
            }
            shift_right(text, ndec);
            for (int i = 0; i < ndec; i += 1)
            {
                text[i] = '0';
            }
        }
    }
    return buffer;
}

char* fn_16b45f(double* value, char* buffer, int ndec, int caps)
{
    // Xbox 0x0016B45F
    // _cftog. __cfltcvt (0x0016B4EC) calls here for a conversion other than
    // 'e', 'E', or 'f', which is the 'g' and 'G' path. The arguments are the
    // double pointer, the output buffer, the precision, and the caps flag
    // (nonzero writes 'E'). EAX on return is the buffer pointer left there
    // by the layout helper.
    //
    // _fltout (0x0016F027) fills a stack _strflt the same way _cftoe does:
    // sign '-' or ' ' (0x20), decpt = exponent + 1, mantissa in a 24-byte
    // buffer. _dtold (0x0016EF6D) drops the sign of a zero, so -0.0 prints
    // as a positive zero. $I10_OUTPUT (0x0016F945) is asked for 17 digits.
    // A zero is the character '0' and decpt 0. An exponent of 0x7FFF selects
    // "1#INF", "1#IND", "1#QNAN", or "1#SNAN" with decpt 1.
    //
    // decpt - 1 is saved, then _fptostr (0x0016EEEE) runs with a count of
    // the precision and a destination one past a slot reserved for a minus.
    // There is no extra digit and no gap for the decimal point. It plants a
    // leading '0', copies that many characters (padding '0' past the
    // mantissa), and rounds when the next character is at least '5'. A count
    // below 0 writes an empty string and does not round. A count of 0 still
    // rounds from the first mantissa character. A carry into the guard
    // increments decpt and keeps the extra character; otherwise the guard is
    // shifted off.
    //
    // The format choice uses that rounded magnitude. Below -4, or at least
    // the precision, 0x0016B25F lays the text out. EBX is the _strflt and the
    // stack arguments are the buffer, the precision, the caps flag, and a
    // format byte of 1. A positive precision shifts the digits right by one
    // (0x0016B23C) before the decimal point from 0x0033818C ('.') is written,
    // and "e+000" from 0x00209B9C is copied at that point plus the precision.
    // That slot covers the extra character a carry kept. A nonzero caps flag
    // replaces 'e' with 'E'. A mantissa that still starts with '0' leaves
    // the exponent at "+000". Any other value writes decpt - 1, three
    // digits, with '-' over the '+' when the power is negative.
    //
    // Otherwise 0x0016B366 lays the text out. EAX is the _strflt and the
    // stack arguments are the buffer, the precision, and a format byte of 1.
    // If the round incremented decpt, the last character of the digit string
    // is cleared first. A minus is stored. A decpt of 0 or below shifts the
    // digits right and writes a leading '0'; a positive decpt steps over
    // that many characters. A positive precision shifts in the decimal
    // point. When decpt is still negative the format byte inserts -decpt
    // zeros after that point. The helper's other format-byte store, a
    // trailing '0' when decpt - 1 equals the precision, sits on the compare
    // that already selected the scientific helper.

    char mantissa[24];
    int sign = ' ';
    int decpt = 0;
    std::uint64_t bits = 0;
    std::memcpy(&bits, value, sizeof(bits));
    const std::uint64_t frac = bits & 0x000fffffffffffffull;
    const unsigned exp_bits = static_cast<unsigned>((bits >> 52) & 0x7ffu);

    if (exp_bits == 0 && frac == 0)
    {
        mantissa[0] = '0';
        mantissa[1] = '\0';
    }
    else if (exp_bits == 0x7ffu)
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        decpt = 1;
        const char* token = "1#QNAN";
        if (frac == 0)
        {
            token = "1#INF";
        }
        else if ((frac & 0x0008000000000000ull) == 0)
        {
            token = "1#SNAN";
        }
        else if ((bits >> 63) != 0 && frac == 0x0008000000000000ull)
        {
            token = "1#IND";
        }
        int i = 0;
        while (token[i] != '\0')
        {
            mantissa[i] = token[i];
            i += 1;
        }
        mantissa[i] = '\0';
    }
    else
    {
        sign = ((bits >> 63) != 0) ? '-' : ' ';
        const std::uint64_t mag_bits = bits & 0x7fffffffffffffffull;
        const double mag = std::bit_cast<double>(mag_bits);
        char scratch[40];
        const auto converted = std::to_chars(scratch, scratch + sizeof(scratch), mag, std::chars_format::scientific, 16);
        const char* cursor = scratch;
        int count = 0;
        int power = 0;
        if (converted.ec == std::errc{})
        {
            if (*cursor == '+' || *cursor == '-')
            {
                cursor += 1;
            }
            if (*cursor >= '0' && *cursor <= '9')
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == '.')
            {
                cursor += 1;
            }
            while (*cursor >= '0' && *cursor <= '9' && count < 17)
            {
                mantissa[count] = *cursor;
                count += 1;
                cursor += 1;
            }
            if (*cursor == 'e' || *cursor == 'E')
            {
                power = std::atoi(cursor + 1);
            }
        }
        if (count == 0)
        {
            mantissa[0] = '0';
            count = 1;
            power = 0;
        }
        mantissa[count] = '\0';
        decpt = power + 1;
    }

    auto shift_right = [](char* text, int count)
    {
        if (count == 0)
        {
            return;
        }
        std::memmove(text + count, text, std::strlen(text) + 1);
    };

    const int magnitude = decpt - 1;
    char* digits = buffer + (sign == '-' ? 1 : 0);
    const char* man = mantissa;
    digits[0] = '0';
    char* out = digits + 1;
    if (ndec > 0)
    {
        int remaining = ndec;
        while (remaining != 0)
        {
            char digit = '0';
            if (*man != '\0')
            {
                digit = *man;
                man += 1;
            }
            *out = digit;
            out += 1;
            remaining -= 1;
        }
    }
    *out = '\0';
    if (ndec >= 0 && *man >= '5')
    {
        char* round = out;
        while (true)
        {
            round -= 1;
            if (*round == '9')
            {
                *round = '0';
                continue;
            }
            *round = static_cast<char>(*round + 1);
            break;
        }
    }
    if (digits[0] == '1')
    {
        decpt += 1;
    }
    else
    {
        std::memmove(digits, digits + 1, std::strlen(digits + 1) + 1);
    }

    const int rounded = decpt - 1;
    if (rounded < -4 || rounded >= ndec)
    {
        if (ndec > 0)
        {
            shift_right(digits, 1);
        }
        char* text = buffer;
        if (sign == '-')
        {
            *text = '-';
            text += 1;
        }
        if (ndec > 0)
        {
            char* dot = text + 1;
            *text = *dot;
            *dot = '.';
            text = dot;
        }
        char* exp_at = text + ndec;
        std::memcpy(exp_at, "e+000", 6);
        if (caps != 0)
        {
            *exp_at = 'E';
        }
        exp_at += 1;
        if (mantissa[0] != '0')
        {
            int power = decpt - 1;
            if (power < 0)
            {
                power = -power;
                *exp_at = '-';
            }
            exp_at += 1;
            if (power >= 100)
            {
                *exp_at = static_cast<char>(*exp_at + power / 100);
                power %= 100;
            }
            exp_at += 1;
            if (power >= 10)
            {
                *exp_at = static_cast<char>(*exp_at + power / 10);
                power %= 10;
            }
            exp_at[1] = static_cast<char>(exp_at[1] + power);
        }
    }
    else
    {
        if (magnitude < rounded)
        {
            char* end = digits;
            while (*end != '\0')
            {
                end += 1;
            }
            end[-1] = '\0';
        }
        char* text = buffer;
        if (sign == '-')
        {
            *text = '-';
            text += 1;
        }
        if (decpt <= 0)
        {
            shift_right(text, 1);
            *text = '0';
            text += 1;
        }
        else
        {
            text += decpt;
        }
        if (ndec > 0)
        {
            shift_right(text, 1);
            *text = '.';
            text += 1;
            if (decpt < 0)
            {
                const int zeros = -decpt;
                shift_right(text, zeros);
                for (int i = 0; i < zeros; i += 1)
                {
                    text[i] = '0';
                }
            }
        }
    }
    return buffer;
}

// Xbox 0x00337A70. __NLG_Destination. The image signature is the C++ EH
// magic 0x19930520. The next three dwords are zero until __NLG_Notify1
// writes them. Nothing in the image reads the record back.
struct NlgDestination
{
    u32 sig;
    u32 destination;
    u32 code;
    u32 frame;
};

static_assert(sizeof(NlgDestination) == 0x10);

NlgDestination g_nlg_destination = {0x19930520u, 0, 0, 0};

void fn_1670f6(u32 destination, const u32* frame)
{
    // Xbox 0x001670F6
    // __NLG_Notify1. stdcall, ret 4. _except_handler3 (0x00166DAC) and
    // __local_unwind2 (0x00167062) call here before transferring to a
    // scope handler. The stack cookie (1 when a filter accepts, 0x101
    // when a scopetable entry has no filter) is popped and not read.
    // Incoming EAX is the handler. This body does not build a frame, so
    // EBP is the caller's and the dword at [EBP+8] is that frame's first
    // argument. _except_handler3 sets EBP to the registration node + 0x10
    // before its own call and before __local_unwind2, which leaves EBP
    // alone. EBX and ECX are saved across the three stores. The entry
    // at 0x001670ED (__NLG_Notify) is the same stores with ECX already
    // holding the code; _CallSettingFrame (0x0016C1E0) uses that entry.
    g_nlg_destination.destination = destination;
    g_nlg_destination.code = frame[2];
    g_nlg_destination.frame = static_cast<u32>(reinterpret_cast<std::uintptr_t>(frame));
}

// Xbox 0x003381A0. The image points every slot at _fptrap (0x0016F08C),
// which exits with code 2. _fpmath overwrites the six dwords.
u32 g_cfltcvt_tab[6] = {
    0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu, 0x0016F08Cu,
};

void fn_1673c1(int set_precision)
{
    // Xbox 0x001673C1
    // _fpmath. _cinit (0x001516D4) calls the pointer at 0x00337A88, which
    // holds this address, before the __xi and __xc initializer walks.
    // 0x00167389 writes the six converter slots:
    //   [0] and [5] __cfltcvt   0x0016B4EC
    //   [1]         __cropzeros 0x0016B199
    //   [2]         __fassign   0x0016B1FE
    //   [3]         __forcdecpt 0x0016B141
    //   [4]         __positive  0x0016B1E4
    // A nonzero argument calls 0x00169774: __controlfp(0x10000, 0x30000),
    // mask _MCW_PC and value _PC_53 (53-bit mantissa). The compare reads the
    // dword above the return address. From _cinit that dword is the caller's
    // return address, so the precision change runs. Both paths execute fnclex.
    g_cfltcvt_tab[0] = 0x0016B4ECu;
    g_cfltcvt_tab[1] = 0x0016B199u;
    g_cfltcvt_tab[2] = 0x0016B1FEu;
    g_cfltcvt_tab[3] = 0x0016B141u;
    g_cfltcvt_tab[4] = 0x0016B1E4u;
    g_cfltcvt_tab[5] = 0x0016B4ECu;
    if (set_precision != 0)
    {
#if defined(_WIN32)
        unsigned int previous = 0;
        (void)_controlfp_s(&previous, _PC_53, _MCW_PC);
#else
        // Host IEEE doubles already round to a 53-bit mantissa.
#endif
    }
#if defined(_WIN32)
    (void)_clearfp();
#else
    std::feclearexcept(FE_ALL_EXCEPT);
#endif
}

// 0x00337AAC. __security_cookie. The image value is DEFAULT_SECURITY_COOKIE.
u32 g_security_cookie = 0xBB40E64Eu;

void fn_16d805()
{
    // Xbox 0x0016D805
    // __security_init_cookie. The pointer sits at 0x0032C814, the first slot
    // of the __xc_a..__xc_z span (null at 0x0032C810, null at 0x0032C834).
    // _cinit walks that span. Nothing calls this address directly.
    // A cookie other than 0 or 0xBB40E64E is left alone. Otherwise the mix is
    // the high and low dwords of GetSystemTimeAsFileTime (0x0014BC60),
    // GetCurrentThreadId (0x0014BC53), GetTickCount (0x0014B956), and both
    // dwords of QueryPerformanceCounter (0x0014C29C). A zero mix is stored
    // back as 0xBB40E64E.
    constexpr u32 kDefaultCookie = 0xBB40E64Eu;
    if (g_security_cookie != 0 && g_security_cookie != kDefaultCookie)
    {
        return;
    }

    u32 time_low = 0;
    u32 time_high = 0;
    u32 thread_id = 0;
    u32 tick = 0;
    u32 perf_low = 0;
    u32 perf_high = 0;

#if defined(_WIN32)
    struct FileTime
    {
        u32 low;
        u32 high;
    };
    struct PerfCount
    {
        u32 low;
        u32 high;
    };
    FileTime file_time{};
    PerfCount counter{};
    GetSystemTimeAsFileTime(&file_time);
    time_low = file_time.low;
    time_high = file_time.high;
    thread_id = static_cast<u32>(GetCurrentThreadId());
    tick = static_cast<u32>(GetTickCount());
    QueryPerformanceCounter(&counter);
    perf_low = counter.low;
    perf_high = counter.high;
#else
    using FileTicks = std::chrono::duration<std::uint64_t, std::ratio<1, 10000000>>;
    const std::uint64_t file_time =
        std::chrono::duration_cast<FileTicks>(std::chrono::system_clock::now().time_since_epoch()).count() +
        116444736000000000ull;
    time_low = static_cast<u32>(file_time);
    time_high = static_cast<u32>(file_time >> 32);
    thread_id = 1;
    tick = static_cast<u32>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count());
    const std::uint64_t perf = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    perf_low = static_cast<u32>(perf);
    perf_high = static_cast<u32>(perf >> 32);
#endif

    g_security_cookie = time_high ^ time_low ^ thread_id ^ tick ^ perf_high ^ perf_low;
    if (g_security_cookie == 0)
    {
        g_security_cookie = kDefaultCookie;
    }
}

void fn_16805f()
{
    // Xbox 0x0016805F
    // __report_gsfailure. __security_check_cookie (0x0016808F) compares ECX
    // with __security_cookie at 0x00337AAC. A mismatch jumps here from
    // 0x00168098. Nothing else calls this address. __SEH_prolog (0x00166D58)
    // is entered with local size 8 and the scope table at 0x00209180:
    // enclosing level -1, filter 0x0016807C, handler 0x00168080. The filter
    // returns 1 (EXCEPTION_EXECUTE_HANDLER). The handler reloads ESP from
    // [ebp-0x18] and falls through.
    //
    // The try calls __security_error_handler (0x0016D85F) with
    // _SECERR_BUFFER_OVERRUN (1) and a null data pointer. The image hook at
    // 0x004DD2D0 is zero, so that callee writes the buffer-overrun text and
    // then calls _exit (0x0016985C) with status 3. It does not return. An
    // exception is swallowed here, the try level is set to -1, and _exit(3)
    // runs anyway. _exit pushes 0 then 1 into doexit (0x00169798), which
    // does not return when the return-to-caller argument is 0. The int 3
    // after that call is the noreturn trap. Status 3 is what this frame
    // pushes. The host exit skips atexit handlers, matching that _exit.
    // The filter accepts every exception (return value 1). The report is
    // 0x0016D85F and is not this body; both paths end in the same exit.
#if defined(_WIN32)
    __try
    {
    }
    __except (1)
    {
    }
#else
    try
    {
    }
    catch (...)
    {
    }
#endif
    std::_Exit(3);
}

// Image _locktable at 0x00337AB0. Thirty-six entries, eight bytes each:
// critical-section pointer, then kind. Kind 1 is a static lock.
constexpr u32 kCrtLockKind[0x24] = {
    1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

struct CrtLockSlot
{
    std::recursive_mutex* cs = nullptr;
    u32 kind = 0;
};

// Xbox XTHREAD_NOTIFICATION is a LIST_ENTRY plus the routine. The list head
// at 0x0032C9E0 is an empty list (Flink and Blink point at the head). The
// node _mtinit registers lives at 0x004DD288; the routine dword is +8.
struct XThreadNotify
{
    XThreadNotify* flink = nullptr;
    XThreadNotify* blink = nullptr;
    u32 routine = 0;
};

// Per-thread CRT block. Retail calloc is 0x88 bytes. +0x00 is the thread id,
// +0x04 is the thread handle, +0x14 is _holdrand, +0x54 is _pxcptacttab.
// The terminate pointer is the dword at +0x68. _curexception is the dword
// at +0x78, _curcontext is +0x7C, and the frame-info chain is +0x84. Those
// image slots are 32-bit. The host pointers follow the 0x88-byte block.
struct CrtTidData
{
    u32 tid;
    u32 thandle;
    u8 rest[0x80];
    void* curexception;
    void* curcontext;
    void* frame_chain;
    void (*terminate_fn)();
};

static_assert(offsetof(CrtTidData, curexception) == 0x88);

struct CrtModuleTls
{
    // Xbox stores the _tiddata pointer at offset 0xC of the 0x90-byte TLS
    // block (TLS directory SizeOfZeroFill, AddressOfIndex 0x00434234).
    void* ptd = nullptr;
};

CrtLockSlot g_crt_locks[0x24];
std::unique_ptr<std::recursive_mutex> g_crt_lock_mu[0x24];

// 0x004DD2D4. BSS, so the image dword is 0. 0x0016D9F8 writes 0x0016D9E8
// here when the slot is still null. The target is stdcall and returns BOOL.
int (*g_crt_crit_init)(std::recursive_mutex* section, u32 spin_count) = nullptr;
XThreadNotify g_xthread_notify_head;
XThreadNotify g_crt_thread_notify;
std::recursive_mutex g_xthread_notify_cs;
CrtModuleTls g_crt_module_tls;
u32 g_crt_tls_index = 0;

// 0x0054DD64. BSS, so the image dword is 0. A thread whose TlsData is null
// keeps its _tiddata pointer here instead of in the module TLS block.
void* g_crt_ptd_nontls = nullptr;

// Host stand-in for the dword at KTHREAD+0x28 (TlsData), read through
// fs:[0x28]. The game thread has TLS, so this stays non-null.
void* g_kthread_tls_data = &g_crt_module_tls;

// 0x0054DD60. BSS, so the image value is 0. 1 is __SYSTEM_HEAP and 3 is
// __V6_HEAP. Nothing in the image stores this dword; the compares still read it.
int g_crt_active_heap = 0;

void fn_165f0e()
{
    // Xbox 0x00165F0E
    // Unwind funclet for type_info::~type_info (0x00165ED1). The state-0
    // entry at 0x002090F8 points here, and the destructor calls it on the
    // success path after setting the EH state to -1. The body pushes 0xE
    // (_TYPEINFO_LOCK) and calls _unlock (0x0016810F), then pops the cdecl
    // argument. _unlock leaves the critical section in _locktable[0xE]
    // (image 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    if (g_crt_locks[0xe].cs != nullptr)
    {
        g_crt_locks[0xe].cs->unlock();
    }
}

// _iob at 0x00337CF8. Twenty FILE records, 0x20 bytes each. _unlock_file
// treats a pointer as one of these when it is between that base and
// 0x00337F58, the address of the last record.
constexpr u32 kCrtIobCount = 20;

struct CrtFile
{
    u8 bytes[0x20];
};

static_assert(sizeof(CrtFile) == 0x20);

// A stream allocated past _iob is a _FILEX: the FILE bytes, then the
// critical section _unlock_file passes to RtlLeaveCriticalSection.
struct CrtFileX
{
    CrtFile file;
    std::recursive_mutex lock;
};

static_assert(offsetof(CrtFileX, lock) == 0x20);

CrtFile g_crt_iob[kCrtIobCount];

// 0x0054DD78. BSS, so the image value is 0. _nstream. 0x001688B3 raises a
// zero to 0x200 and a smaller value up to 0x14, then allocates that many
// FILE* slots. This body does not read the count; the port uses it so an
// index cannot walk off __piob.
u32 g_crt_nstream = 0;

// 0x0054DD74. BSS, so the image pointer is null. __piob. 0x001688B3 points
// the first twenty entries at _iob. The unlock body indexes it with the
// stream number left in ESI.
CrtFile** g_crt_piob = nullptr;

void fn_16619a(void* stream)
{
    // Xbox 0x0016619A
    // Shared unlock body for fclose (0x00166151). The scope table at
    // 0x00209100 points the __finally at 0x00166197, which loads the
    // FILE* from [ebp+8] into ESI and falls through to this address.
    // The success path at 0x00166190 calls here after _fclose_lk with
    // ESI already holding that pointer. The eight bytes push ESI, call
    // _unlock_file (0x001689C2), pop the cdecl argument, and return.
    //
    // _unlock_file compares the pointer with _iob and with 0x00337F58.
    // Inside that span the lock number is (byte delta >> 5) + 0x10
    // (_STREAM_LOCKS). _unlock (0x0016810F) then leaves the critical
    // section stored in _locktable[index] at 0x00337AB0. Outside the
    // span it calls RtlLeaveCriticalSection (kernel ordinal 294, thunk
    // 0x00202EA4) on the critical section at FILE+0x20. A null ESI
    // would take that path at address 0x20; the port returns instead
    // of faulting.
    if (stream == nullptr)
    {
        return;
    }

    const auto addr = reinterpret_cast<std::uintptr_t>(stream);
    const auto base = reinterpret_cast<std::uintptr_t>(&g_crt_iob[0]);
    const auto last = reinterpret_cast<std::uintptr_t>(&g_crt_iob[kCrtIobCount - 1]);
    if (addr >= base && addr <= last)
    {
        const u32 index = static_cast<u32>((addr - base) >> 5) + 0x10u;
        if (g_crt_locks[index].cs != nullptr)
        {
            g_crt_locks[index].cs->unlock();
        }
        return;
    }

    reinterpret_cast<CrtFileX*>(stream)->lock.unlock();
}

int crt_mtinitlocknum(int locknum);

void fn_1681c1(int locknum)
{
    // Xbox 0x001681C1
    // _lock. The slot is the 8-byte record at 0x00337AB0 + index*8. A null
    // critical-section pointer calls __mtinitlocknum (0x00168121). A zero
    // return calls _amsg_exit (0x00168B7A) with 0x11 (_RT_LOCK): the banner
    // at 0x00169894, the runtime message at 0x00169868, then the pointer at
    // 0x003380E0 (0x0016985C) which pushes 1,0 into 0x00169798. That path
    // does not come back. Otherwise RtlEnterCriticalSection (kernel ordinal
    // 277, thunk 0x00202EA0) takes the section stored in the slot. An index
    // outside the 36 slots walks off the table; the port returns.
    if (locknum < 0 || static_cast<u32>(locknum) >= 0x24u)
    {
        return;
    }

    if (g_crt_locks[locknum].cs == nullptr)
    {
        if (crt_mtinitlocknum(locknum) == 0)
        {
            std::exit(0xFF);
        }
    }
    g_crt_locks[locknum].cs->lock();
}

void fn_1681b8()
{
    // Xbox 0x001681B8
    // __finally of __mtinitlocknum (0x00168121). The scope table at
    // 0x00209190 points here. The nine bytes push 10 (_LOCKTAB_LOCK),
    // call _unlock (0x0016810F), pop the cdecl argument, and return.
    // _unlock leaves the critical section in _locktable[10] (image
    // 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // _local_unwind2 (0x00167062) runs this handler when
    // InitializeCriticalSection (0x0016D9F8) fails. The success path at
    // 0x001681AA calls it after the try level at [ebp-4] is set to -1.
    if (g_crt_locks[10].cs != nullptr)
    {
        g_crt_locks[10].cs->unlock();
    }
}

CrtTidData* fn_16ae40()
{
    // Xbox 0x0016AE40
    // _getptd. The IRQL byte at fs:[0x24] above 1 is KeBugCheck(10). The
    // host thread stays at passive level, so that check does not fire.
    // fs:[0x28] is the current KTHREAD. A zero dword at KTHREAD+0x28
    // (TlsData) selects the global pointer at 0x0054DD64. Otherwise the
    // slot is fs:[4][__tls_index] + 0xC: fs:[4] is the thread TLS array
    // and __tls_index is the dword at 0x00434234. This port keeps that
    // slot in g_crt_module_tls.ptd. The game thread has TLS, so it takes
    // that path.
    //
    // A null slot is calloc of the host block (retail 0x88 bytes,
    // 0x00168012). A failed
    // allocation is _amsg_exit (0x00168B7A) with 0x10 (_RT_THREAD) and
    // does not return. The new block is stored in the slot, +0x54
    // receives the static exception table 0x00338280, +0x14 receives 1
    // (_holdrand), +4 receives 0xFFFFFFFF, and +0 receives the thread id
    // from GetCurrentThreadId (0x0014BC53).
    void** slot = &g_crt_ptd_nontls;
    if (g_kthread_tls_data != nullptr)
    {
        slot = &g_crt_module_tls.ptd;
    }

    CrtTidData* ptd = static_cast<CrtTidData*>(*slot);
    if (ptd == nullptr)
    {
        ptd = static_cast<CrtTidData*>(std::calloc(1, sizeof(CrtTidData)));
        if (ptd == nullptr)
        {
            std::exit(0xFF);
        }
        *slot = ptd;
        auto* raw = reinterpret_cast<u8*>(ptd);
        *reinterpret_cast<u32*>(raw + 0x54) = 0x00338280u;
        *reinterpret_cast<u32*>(raw + 0x14) = 1u;
#if defined(_WIN32)
        const u32 tid = static_cast<u32>(GetCurrentThreadId());
#else
        const u32 tid = 1;
#endif
        ptd->thandle = 0xffffffffu;
        ptd->tid = tid;
    }
    return ptd;
}

[[noreturn]] void fn_16c177()
{
    // Xbox 0x0016C177
    // terminate. __SEH_prolog (0x00166D58) is entered with local size 8 and
    // the scope table at 0x00209C38: enclosing level -1, filter 0x0016C19C,
    // handler 0x0016C1A0. _getptd (0x0016AE40) returns _tiddata. A null
    // dword at +0x68 jumps to the bugcheck. Otherwise the try level at
    // [ebp-4] is set to 0, a second _getptd loads that slot, and the
    // pointer is called. The filter is xor eax,eax / inc eax / ret, so it
    // returns 1 and every exception is swallowed. The handler reloads ESP
    // from [ebp-0x18]. A normal return skips that reload with a short jump.
    // Both paths then set the try level to -1, push 0xC0000144, and call
    // KeBugCheck through the dword at 0x00203040 (kernel ordinal 0x5F).
    // The int 3 is the noreturn trap. The host pointer lives past the
    // 0x88-byte image, and abort matches a bugcheck that does not return.
    CrtTidData* ptd = fn_16ae40();
    if (ptd->terminate_fn != nullptr)
    {
#if defined(_WIN32)
        __try
        {
            ptd = fn_16ae40();
            ptd->terminate_fn();
        }
        __except (1)
        {
        }
#else
        try
        {
            ptd = fn_16ae40();
            ptd->terminate_fn();
        }
        catch (...)
        {
        }
#endif
    }
    std::abort();
}

// Catch frame pushed by _CreateFrameInfo (0x001678B8). Retail is two dwords:
// the exception object, then the previous chain head.
struct CrtFrameInfo
{
    void* object;
    CrtFrameInfo* next;
};

// ThrowInfo as CallCatchBlock reads it. The unwind function is the pointer
// at +4. Retail passes that pointer to the thiscall thunk at 0x001675FB.
struct CrtThrowLite
{
    u32 attributes;
    void (*dtor)(void* self);
};

// Exception record. Retail object is +0x18 and ThrowInfo is +0x1C. Host
// pointers follow the same field order with natural alignment.
struct CrtExceptRec
{
    u32 code;
    u32 flags;
    void* next;
    void* address;
    u32 nparams;
    u32 magic;
    void* object;
    CrtThrowLite* throw_info;
};

// EHRegistrationNode. state is the dword at +8. CallCatchBlock also reads
// and writes the dword immediately before the node.
struct CrtEhNode
{
    void* next;
    void* handler;
    i32 state;
};

// Unwind map slot. Retail is 8 bytes: toState, then the action.
struct CrtUnwindSlot
{
    i32 to_state;
    void (*action)(void* registration);
};

// Try block. Retail stride is 20 bytes: tryLow, tryHigh, catchHigh,
// nCatches, handler array.
struct CrtTrySlot
{
    i32 try_low;
    i32 try_high;
    i32 catch_high;
    i32 n_catches;
    void* handlers;
};

// FuncInfo fields CallCatchBlock uses. maxState is +4, the unwind map is
// +8, nTryBlocks is +0xC, and the try map is +0x10 in the image.
struct CrtFuncRec
{
    u32 magic;
    i32 max_state;
    const CrtUnwindSlot* unwind;
    i32 n_try_blocks;
    const CrtTrySlot* tries;
};

void crt_link_frame(CrtFrameInfo* frame, void* object)
{
    // _CreateFrameInfo (0x001678B8). The object is stored, the chain head
    // at _tiddata+0x84 becomes the next pointer, and that head becomes
    // this frame. Each link is a separate _getptd.
    frame->object = object;
    CrtTidData* ptd = fn_16ae40();
    frame->next = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    ptd = fn_16ae40();
    ptd->frame_chain = frame;
}

void crt_unlink_frame(CrtFrameInfo* frame)
{
    // _FindAndUnlinkFrame (0x00167901). A frame that is the chain head is
    // replaced by its next pointer. Otherwise the chain is walked until
    // the previous node's next pointer is this frame. A missing frame is
    // __inconsistency (0x0016C1B3).
    CrtTidData* ptd = fn_16ae40();
    if (ptd->frame_chain == frame)
    {
        ptd = fn_16ae40();
        ptd->frame_chain = frame->next;
        return;
    }

    ptd = fn_16ae40();
    auto* prev = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    while (prev != nullptr && prev->next != nullptr)
    {
        if (prev->next == frame)
        {
            prev->next = frame->next;
            return;
        }
        prev = prev->next;
    }
    std::terminate();
}

int crt_exception_object_to_destroy(void* object)
{
    // _IsExceptionObjectToBeDestroyed (0x001678E0). Walks the chain at
    // _tiddata+0x84. A frame whose object matches returns 0. The end of
    // the chain returns 1.
    CrtTidData* ptd = fn_16ae40();
    auto* frame = static_cast<CrtFrameInfo*>(ptd->frame_chain);
    while (frame != nullptr)
    {
        if (frame->object == object)
        {
            return 0;
        }
        frame = frame->next;
    }
    return 1;
}

int crt_dtor_throw_fails()
{
    // 0x001670CA. Returns 1 when fs:[0] is the registration __local_unwind2
    // (0x00167062) installs, whose handler is 0x00167040, and the trylevel
    // dword of the node saved on that frame equals the -2 cookie at
    // frame+8. The port does not install that registration, so the result
    // stays 0. ___DestructExceptionObject's filter (0x0016BA7E) returns
    // that byte: 0 continues the search, and 1 enters terminate.
    return 0;
}

void crt_destruct_except(CrtExceptRec* except)
{
    // Body of ___DestructExceptionObject (0x0016BA4A) for the call from
    // the CallCatchBlock finally. A null record or a null ThrowInfo
    // returns. A null unwind function at ThrowInfo+4 returns. Otherwise
    // the function runs on the exception object. The second argument is
    // 0x001670CA.
    if (except == nullptr || except->throw_info == nullptr || except->throw_info->dtor == nullptr)
    {
        return;
    }
    const int fail = crt_dtor_throw_fails();
    try
    {
        except->throw_info->dtor(except->object);
    }
    catch (...)
    {
        if (fail != 0)
        {
            fn_16c177();
        }
        throw;
    }
}

i32* crt_processing_throw(CrtTidData* ptd)
{
    return reinterpret_cast<i32*>(reinterpret_cast<u8*>(ptd) + 0x80);
}

void crt_unwind_rethrow(CrtEhNode* rn, CrtFuncRec* info)
{
    // Except handler at 0x0016BB76, then __FrameUnwindToState (0x0016B97C).
    // The state is the dword at registration+8. The first try whose
    // tryHigh is below that state and whose catchHigh covers it selects
    // unwindMap[tryHigh+1].toState. No match unwinds to the current
    // state. The dispatcher argument is null. _ProcessingThrow at
    // _tiddata+0x80 is incremented for the walk. A state below 0, a
    // missing unwind map, or a state past maxState is __inconsistency.
    // An action runs after the state dword is updated. An exception from
    // an action clears _ProcessingThrow and terminates (0x0016C177).
    // 0x0016BA2F then decrements a positive count.
    if (rn == nullptr || info == nullptr)
    {
        std::terminate();
    }

    i32 target = rn->state;
    if (info->n_try_blocks > 0)
    {
        if (info->tries == nullptr)
        {
            std::terminate();
        }
        for (i32 i = 0; i < info->n_try_blocks; i += 1)
        {
            const CrtTrySlot& entry = info->tries[i];
            if (target <= entry.try_high || target > entry.catch_high)
            {
                continue;
            }
            const i32 index = entry.try_high + 1;
            if (info->unwind == nullptr || index < 0 || index >= info->max_state)
            {
                std::terminate();
            }
            target = info->unwind[index].to_state;
            break;
        }
    }

    CrtTidData* ptd = fn_16ae40();
    ++(*crt_processing_throw(ptd));
    i32 state = rn->state;
    while (state != target)
    {
        if (state <= -1 || info->unwind == nullptr || state >= info->max_state)
        {
            std::terminate();
        }
        const CrtUnwindSlot& entry = info->unwind[state];
        const i32 next = entry.to_state;
        if (entry.action != nullptr)
        {
            rn->state = next;
            try
            {
                entry.action(rn);
            }
            catch (...)
            {
                *crt_processing_throw(fn_16ae40()) = 0;
                fn_16c177();
            }
        }
        state = next;
    }
    ptd = fn_16ae40();
    if (*crt_processing_throw(ptd) > 0)
    {
        ptd = fn_16ae40();
        --(*crt_processing_throw(ptd));
    }
    if (state != target)
    {
        std::terminate();
    }
    rn->state = state;
}

void crt_catch_cleanup(
    CrtExceptRec* except,
    CrtEhNode* rn,
    i32 saved_before,
    CrtFrameInfo* frame,
    void* saved_ex,
    void* saved_ctx,
    void* continuation)
{
    // Finally at 0x0016BBF1. Restores the dword before the registration
    // node, unlinks the frame, then writes the saved _curexception and
    // _curcontext back through two _getptd calls. A C++ exception
    // (0xE06D7363, three parameters, magic 0x19930520) with a non-null
    // continuation destroys its object when 0x001678E0 returns 1.
    // [ebp-0x48] is cleared on entry to CallCatchBlock and is not stored
    // again, so that compare does not skip the destroy.
    reinterpret_cast<i32*>(rn)[-1] = saved_before;
    crt_unlink_frame(frame);
    CrtTidData* ptd = fn_16ae40();
    ptd->curexception = saved_ex;
    ptd = fn_16ae40();
    ptd->curcontext = saved_ctx;
    if (except == nullptr)
    {
        return;
    }
    if (except->code != 0xE06D7363u || except->nparams != 3 || except->magic != 0x19930520u)
    {
        return;
    }
    if (continuation == nullptr)
    {
        return;
    }
    if (crt_exception_object_to_destroy(except->object) == 0)
    {
        return;
    }
    crt_destruct_except(except);
}

#if defined(_WIN32)
int crt_rethrow_filter(void* pointers)
{
    // Filter at 0x0016BB33. Returns 1 (EXCEPTION_EXECUTE_HANDLER) only for
    // a C++ exception whose ThrowInfo pointer is null. Every other record
    // returns 0 (EXCEPTION_CONTINUE_SEARCH). The image reads the record
    // from the exception pointers at [ebp-0x14]: code at +0, parameter
    // count at +0x10, magic at +0x14, ThrowInfo at +0x1C. The host record
    // uses the same fields; a 64-bit pointer moves the parameter count to
    // +0x18 and the information array to +0x20.
    if (pointers == nullptr)
    {
        return 0;
    }
    void* record = *static_cast<void**>(pointers);
    if (record == nullptr)
    {
        return 0;
    }
    auto* bytes = static_cast<u8*>(record);
    const u32 code = *reinterpret_cast<u32*>(bytes);
    constexpr u32 kParams = sizeof(void*) == 8 ? 0x18u : 0x10u;
    constexpr u32 kInfo = sizeof(void*) == 8 ? 0x20u : 0x14u;
    const u32 nparams = *reinterpret_cast<u32*>(bytes + kParams);
    const auto* info = reinterpret_cast<const std::uintptr_t*>(bytes + kInfo);
    if (code != 0xE06D7363u || nparams != 3 || info[0] != 0x19930520u || info[2] != 0)
    {
        return 0;
    }
    return 1;
}
#endif

void* fn_16baae(
    void* handler,
    void* except_record,
    void* registration,
    void* context,
    void* func_info,
    int catch_depth,
    u32 nlg_code)
{
    // Xbox 0x0016BAAE
    // CallCatchBlock. The funclet arrives in ECX. The stack arguments, in
    // order, are the exception record, the registration node, the context,
    // FuncInfo, the catch depth, and the NLG code. __SEH_prolog
    // (0x00166D58) is entered with local size 0x40 and the scope table at
    // 0x00209C10. Level 0 is the __finally at 0x0016BBEB. Level 1 is the
    // __except with filter 0x0016BB33 and handler 0x0016BB76.
    //
    // The dword before the registration node is saved. The thrown object
    // at exception+0x18 is pushed on the frame-info chain. _curexception
    // and _curcontext are saved and replaced for the funclet.
    // __CallCatchBlock2 (0x0016794D) calls _CallSettingFrame (0x0016C1E0)
    // with the funclet, the registration node, and the NLG code, and
    // returns the continuation. Catch depth is the argument before the
    // NLG code. The continuation slot is first written with the funclet,
    // so an exception that leaves the inner try still sees that pointer.
    // A rethrow accepted by the filter unwinds and stores a null
    // continuation. A null funclet is left uncalled.
    (void)catch_depth;
    (void)nlg_code;

    auto* except = static_cast<CrtExceptRec*>(except_record);
    auto* rn = static_cast<CrtEhNode*>(registration);
    auto* info = static_cast<CrtFuncRec*>(func_info);
    i32 saved_before = reinterpret_cast<i32*>(rn)[-1];
    CrtFrameInfo frame{};
    crt_link_frame(&frame, except->object);

    CrtTidData* ptd = fn_16ae40();
    void* saved_ex = ptd->curexception;
    ptd = fn_16ae40();
    void* saved_ctx = ptd->curcontext;
    ptd = fn_16ae40();
    ptd->curexception = except_record;
    ptd = fn_16ae40();
    ptd->curcontext = context;

    void* volatile continuation = handler;
#if defined(_WIN32)
    __try
    {
        __try
        {
            if (handler != nullptr)
            {
                continuation = reinterpret_cast<void* (*)()>(handler)();
            }
        }
        __except (crt_rethrow_filter(GetExceptionInformation()))
        {
            crt_unwind_rethrow(rn, info);
            continuation = nullptr;
        }
    }
    __finally
    {
        crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
    }
#else
    (void)info;
    try
    {
        if (handler != nullptr)
        {
            continuation = reinterpret_cast<void* (*)()>(handler)();
        }
    }
    catch (...)
    {
        crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
        throw;
    }
    crt_catch_cleanup(except, rn, saved_before, &frame, saved_ex, saved_ctx, continuation);
#endif
    return continuation;
}

int* fn_168af5()
{
    // Xbox 0x00168AF5
    // _errno. Nine bytes: call _getptd (0x0016AE40), add eax, 8, ret.
    // The dword at _tiddata+8 is _terrno. __mtinitlocknum stores ENOMEM
    // (0xC) through this pointer.
    CrtTidData* ptd = fn_16ae40();
    return reinterpret_cast<int*>(reinterpret_cast<u8*>(ptd) + 8);
}

u32* fn_168afe()
{
    // Xbox 0x00168AFE
    // __doserrno. Nine bytes: call _getptd (0x0016AE40), add eax, 0xC, ret.
    // The dword at _tiddata+0xC is _tdoserrno.
    CrtTidData* ptd = fn_16ae40();
    return reinterpret_cast<u32*>(reinterpret_cast<u8*>(ptd) + 0xc);
}

// One lowio record. Retail stride is 0x28: a 32-bit OS handle, osfile at +4,
// pipech at +5, a dword at +8, and the critical section at +0xC.
// _ioinit (0x0016D401) allocates 32 of them and stores the pointer in
// __pioinfo[0]. Handles start at -1, pipech is 10, and the dword at +8 is 0.
// The first three records are then marked FOPEN|FDEV|FTEXT (0xC1) for stdin,
// stdout, and stderr. The port keeps that order and stores a host handle, so
// the record is wider than 0x28. _lock_fhandle (0x0016DBAE) treats the dword
// at +8 as an init count: zero creates the section and increments it. Until
// then the pointer stays null. cs_owner keeps the section for the process.
struct CrtIoInfo
{
    std::intptr_t osfhnd = -1;
    u8 osfile = 0;
    u8 pipech = 10;
    u32 field8 = 0;
    std::recursive_mutex* cs = nullptr;
    std::unique_ptr<std::recursive_mutex> cs_owner;
};

constexpr u32 kCrtIoArrayCount = 64;
constexpr u32 kCrtIoPerArray = 32;

CrtIoInfo g_crt_ioinfo0[kCrtIoPerArray] = {
    {-1, 0xC1, 10, 0},
    {-1, 0xC1, 10, 0},
    {-1, 0xC1, 10, 0},
};

// 0x0054DC60. Sixty-four pointers. Only the first is filled by _ioinit.
CrtIoInfo* g_crt_pioinfo[kCrtIoArrayCount] = {g_crt_ioinfo0};

// 0x0054DC40. _nhandle. _ioinit stores 32.
u32 g_crt_nhandle = kCrtIoPerArray;

// Image pairs at 0x00337F78. __dosmaperr walks all 45. Win32 code 6 appears
// twice; the first hit (EBADF, 9) wins.
struct CrtDosMap
{
    u32 dos;
    int err;
};

constexpr CrtDosMap kCrtDoserrMap[45] = {
    {1, 22},    {2, 2},     {3, 2},     {4, 24},    {5, 13},    {6, 9},     {7, 12},   {8, 12},
    {9, 12},    {10, 7},    {11, 8},    {12, 22},   {13, 22},   {15, 2},    {16, 13},  {17, 18},
    {18, 2},    {33, 13},   {53, 2},    {65, 13},   {67, 2},    {80, 17},   {82, 13},  {83, 13},
    {87, 22},   {89, 11},   {108, 13},  {109, 32},  {112, 28},  {114, 9},   {6, 22},   {128, 10},
    {129, 10},  {130, 9},   {131, 22},  {132, 13},  {145, 41},  {158, 13},  {161, 2},  {164, 11},
    {167, 13},  {183, 17},  {206, 2},   {215, 11},  {1816, 12},
};

void crt_dosmaperr(u32 code)
{
    // __dosmaperr at 0x00168B07. _doserrno is stored first. A table hit
    // writes that errno. Codes 0x13 through 0x24 are EACCES (13). Codes
    // 0xBC through 0xCA are ENOEXEC (8). Every other code is EINVAL (22).
    *fn_168afe() = code;
    for (const CrtDosMap& entry : kCrtDoserrMap)
    {
        if (entry.dos == code)
        {
            *fn_168af5() = entry.err;
            return;
        }
    }
    if (code >= 0x13u && code <= 0x24u)
    {
        *fn_168af5() = 13;
        return;
    }
    if (code >= 0xBCu && code <= 0xCAu)
    {
        *fn_168af5() = 8;
        return;
    }
    *fn_168af5() = 22;
}

CrtIoInfo* crt_ioinfo_checked(u32 fh)
{
    if (fh >= g_crt_nhandle)
    {
        return nullptr;
    }
    const int block_index = static_cast<int>(fh) >> 5;
    if (block_index < 0 || static_cast<u32>(block_index) >= kCrtIoArrayCount)
    {
        return nullptr;
    }
    CrtIoInfo* block = g_crt_pioinfo[block_index];
    if (block == nullptr)
    {
        return nullptr;
    }
    return &block[fh & 0x1fu];
}

std::intptr_t crt_get_osfhandle(u32 fh)
{
    // __get_osfhandle at 0x0016DB6D. A descriptor outside _nhandle, or one
    // whose osfile lacks FOPEN, stores EBADF (9), clears _doserrno, and
    // returns -1. Otherwise the OS handle is returned, including -1.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 1u) == 0)
    {
        *fn_168af5() = 9;
        *fn_168afe() = 0;
        return -1;
    }
    return info->osfhnd;
}

int fn_16d466(u32 fh, int offset, u32 method)
{
    // Xbox 0x0016D466
    // _lseek_lk. cdecl. The caller pops the descriptor, the signed byte
    // offset, and the origin. FILE_BEGIN, FILE_CURRENT, and FILE_END are
    // the same values as SEEK_SET, SEEK_CUR, and SEEK_END. This body does
    // not lock the descriptor; the locked _lseek calls here with that lock
    // already held.
    //
    // __get_osfhandle (0x0016DB6D) supplies the OS handle. A handle of -1
    // stores EBADF (9) through _errno (0x00168AF5) and returns -1.
    // _doserrno stays as __get_osfhandle left it. An open slot whose stored
    // handle is already -1 takes this path too.
    //
    // Otherwise XAPILIB::SetFilePointer is called with a null high distance.
    // A return other than -1 stores a zero DOS code. A return of -1 stores
    // GetLastError (0x0014AFE4). A zero code, including a file position of
    // 0xFFFFFFFF with NO_ERROR, clears FEOFLAG (bit 1) and returns that
    // position. The osfile byte is __pioinfo[fd >> 5] at 0x0054DC60, slot
    // (fd & 0x1F), stride 0x28, offset +4, masked with 0xFD. A null
    // __pioinfo slot is left unwritten; retail would fault on that address.
    //
    // Any other DOS code goes through __dosmaperr (0x00168B07) and the
    // return is -1. errno is not cleared on success.
    const std::intptr_t handle = crt_get_osfhandle(fh);
    if (handle == -1)
    {
        *fn_168af5() = 9;
        return -1;
    }

    int position = -1;
    u32 doserr = 0;
#if defined(_WIN32)
    const unsigned long moved =
        SetFilePointer(reinterpret_cast<void*>(handle), static_cast<long>(offset), nullptr, method);
    position = static_cast<int>(moved);
    if (moved == 0xffffffffu)
    {
        doserr = GetLastError();
    }
#else
    (void)handle;
    (void)offset;
    (void)method;
    position = 0;
#endif

    if (doserr == 0)
    {
        CrtIoInfo* info = crt_ioinfo_checked(fh);
        if (info != nullptr)
        {
            info->osfile = static_cast<u8>(info->osfile & 0xfdu);
        }
    }
    else
    {
        crt_dosmaperr(doserr);
        position = -1;
    }
    return position;
}

void crt_free_osfhnd(u32 fh)
{
    // _free_osfhnd at 0x0016DB24. An open slot whose handle is not -1 stores
    // -1. Every other slot stores EBADF and clears _doserrno. The return
    // value is unused by _close_lk.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info != nullptr && (info->osfile & 1u) != 0 && info->osfhnd != -1)
    {
        info->osfhnd = -1;
        return;
    }
    *fn_168af5() = 9;
    *fn_168afe() = 0;
}

bool crt_close_os_handle(std::intptr_t handle, u32* doserr)
{
    // CloseHandle at 0x0014B785. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4) for __dosmaperr.
#if defined(_WIN32)
    if (CloseHandle(reinterpret_cast<void*>(handle)) != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)doserr;
    return true;
#endif
}

int fn_168b96(u32 fh)
{
    // Xbox 0x00168B96
    // _close_lk. __close (0x00168C17) calls here with the file lock already
    // held. cdecl; the caller pops the descriptor. Zero is success. Minus
    // one is an OS failure.
    //
    // A handle of -1 from __get_osfhandle skips CloseHandle. Descriptors 1
    // and 2 also skip it when stdout and stderr share one OS handle, so
    // closing one standard stream leaves that console handle open. Any
    // other handle is closed. A failed CloseHandle keeps GetLastError.
    //
    // _free_osfhnd runs either way, then the osfile byte is cleared. The
    // index is the arithmetic shift of the descriptor by 5, and the byte
    // sits at offset 4 of record (fd & 0x1F). A null __pioinfo slot is
    // left unwritten; retail would fault on that address.
    //
    // A zero DOS code returns 0. Any other code goes through __dosmaperr
    // and the return is -1.
    std::intptr_t handle = crt_get_osfhandle(fh);
    u32 doserr = 0;
    if (handle != -1)
    {
        bool skip_close = false;
        if (fh == 1u || fh == 2u)
        {
            const std::intptr_t stderr_handle = crt_get_osfhandle(2u);
            const std::intptr_t stdout_handle = crt_get_osfhandle(1u);
            if (stdout_handle == stderr_handle)
            {
                skip_close = true;
            }
        }
        if (!skip_close)
        {
            handle = crt_get_osfhandle(fh);
            if (crt_close_os_handle(handle, &doserr))
            {
                doserr = 0;
            }
        }
    }

    crt_free_osfhnd(fh);

    const int block_index = static_cast<int>(fh) >> 5;
    if (block_index >= 0 && static_cast<u32>(block_index) < kCrtIoArrayCount)
    {
        CrtIoInfo* block = g_crt_pioinfo[block_index];
        if (block != nullptr)
        {
            block[fh & 0x1fu].osfile = 0;
        }
    }

    if (doserr == 0)
    {
        return 0;
    }
    crt_dosmaperr(doserr);
    return -1;
}

void fn_168c8e(int fh)
{
    // Xbox 0x00168C8E
    // Shared unlock body for __close (0x00168C17). The scope table at
    // 0x00209258 has one entry: enclosing level -1, a null filter, and the
    // handler at 0x00168C8B. That handler loads the descriptor from [ebp+8]
    // into EBX and falls through to this address. The success path at
    // 0x00168C81 calls here after _close_lk (0x00168B96) with EBX already
    // holding that descriptor. The eight bytes push EBX, call
    // _unlock_fhandle (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    const int block_index = fh >> 5;
    if (block_index < 0 || static_cast<u32>(block_index) >= kCrtIoArrayCount)
    {
        return;
    }
    CrtIoInfo* block = g_crt_pioinfo[block_index];
    if (block == nullptr)
    {
        return;
    }
    std::recursive_mutex* lock = block[fh & 0x1f].cs;
    if (lock == nullptr)
    {
        return;
    }
    lock->unlock();
}

void fn_16d55f(int fh)
{
    // Xbox 0x0016D55F
    // Shared unlock tail for _lseek (0x0016D4D8). The scope table at
    // 0x00209C88 has one entry: enclosing level -1, a null filter, and the
    // handler at 0x0016D55C. That handler loads the descriptor from [ebp+8]
    // into EBX and falls through to this address. The success path at
    // 0x0016D552 calls here after _lseek_lk (0x0016D466) with EBX already
    // holding that descriptor. The eight bytes push EBX, call
    // _unlock_fhandle (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    fn_168c8e(fh);
}

bool crt_read_file(std::intptr_t handle, void* buffer, u32 bytes, u32* got, u32* doserr)
{
    // ReadFile at 0x0014B3A3. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4). The transferred count is reported either way.
#if defined(_WIN32)
    unsigned long n = 0;
    const int ok = ReadFile(reinterpret_cast<void*>(handle), buffer, bytes, &n, nullptr);
    *got = static_cast<u32>(n);
    if (ok != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)buffer;
    (void)bytes;
    (void)doserr;
    *got = 0;
    return true;
#endif
}

int fn_16d583(u32 fh, char* buf, u32 count)
{
    // Xbox 0x0016D583
    // _read_lk. cdecl. The caller pops the descriptor, the buffer, and the
    // byte count. This body does not lock the descriptor; the locked _read
    // calls here with that lock already held.
    //
    // A count of 0 returns 0. The record is __pioinfo[fd >> 5] at
    // 0x0054DC60, slot (fd & 0x1F), stride 0x28. osfile is at +4 and the
    // pipe lookahead is at +5. FEOFLAG (bit 1) returns 0 without reading.
    // A null __pioinfo slot is the same zero return; retail would fault on
    // that address.
    //
    // FPIPE or FDEV (0x48) with a lookahead other than LF (10) stores that
    // byte at the front of the buffer, decrements the count, sets the
    // lookahead back to LF, and reads the rest one byte later.
    //
    // XAPILIB::ReadFile uses the OS handle at record+0. A zero return takes
    // GetLastError. Code 5 stores EBADF (9) through _errno (0x00168AF5) and
    // 5 through __doserrno (0x00168AFE), then returns -1. Code 0x6D
    // (ERROR_BROKEN_PIPE) returns 0 and leaves errno alone. Any other code
    // goes through __dosmaperr (0x00168B07) and returns -1.
    //
    // A successful read that is not FTEXT (bit 0x80) returns the lookahead
    // byte plus the OS count. FTEXT then sets FCRLF (bit 2) when the OS
    // count is nonzero and the first buffer byte is LF, and clears it
    // otherwise. That test uses the original buffer, so a consumed
    // lookahead is what gets tested.
    //
    // The buffer is then scanned in place. CR LF becomes LF. A lone CR
    // stays CR. CTRL-Z (0x1A) stops the scan and is not copied. Unless
    // FDEV (bit 0x40) is set, CTRL-Z also sets FEOFLAG. A CR that is the
    // last buffered byte reads one more OS byte. A failed peek, or a peek
    // of zero bytes, stores the CR. On a pipe or device, LF replaces the
    // CR and any other byte is saved as the lookahead after the CR. On a
    // disk file, LF at the start of an otherwise empty result is stored
    // and consumed. Any other peek is pushed back with _lseek_lk
    // (0x0016D466) by -1 from the current position; LF is dropped with the
    // CR, and any other byte leaves the CR in the buffer.
    if (count == 0)
    {
        return 0;
    }

    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 2u) != 0)
    {
        return 0;
    }

    char* read_at = buf;
    u32 carried = 0;
    if ((info->osfile & 0x48u) != 0 && info->pipech != 10)
    {
        count -= 1u;
        *buf = static_cast<char>(info->pipech);
        read_at = buf + 1;
        carried = 1u;
        info->pipech = 10;
    }

    u32 os_read = 0;
    u32 doserr = 0;
    if (!crt_read_file(info->osfhnd, read_at, count, &os_read, &doserr))
    {
        if (doserr == 5u)
        {
            *fn_168af5() = 9;
            *fn_168afe() = 5u;
            return -1;
        }
        if (doserr == 0x6du)
        {
            return 0;
        }
        crt_dosmaperr(doserr);
        return -1;
    }

    const u32 total = carried + os_read;
    if ((info->osfile & 0x80u) == 0)
    {
        return static_cast<int>(total);
    }

    if (os_read != 0 && *buf == '\n')
    {
        info->osfile = static_cast<u8>(info->osfile | 4u);
    }
    else
    {
        info->osfile = static_cast<u8>(info->osfile & 0xfbu);
    }

    char* src = buf;
    char* dst = buf;
    char* end = buf + total;
    while (src < end)
    {
        const char ch = *src;
        if (ch == '\x1a')
        {
            if ((info->osfile & 0x40u) == 0)
            {
                info->osfile = static_cast<u8>(info->osfile | 2u);
            }
            break;
        }
        if (ch != '\r')
        {
            *dst = ch;
            dst += 1;
            src += 1;
            continue;
        }
        if (src < end - 1)
        {
            if (src[1] == '\n')
            {
                src += 2;
                *dst = '\n';
            }
            else
            {
                src += 1;
                *dst = '\r';
            }
            dst += 1;
            continue;
        }

        src += 1;
        char peek = 0;
        u32 peek_n = 0;
        u32 peek_err = 0;
        const bool peek_ok = crt_read_file(info->osfhnd, &peek, 1u, &peek_n, &peek_err);
        if ((!peek_ok && peek_err != 0) || peek_n == 0)
        {
            *dst = '\r';
            dst += 1;
            continue;
        }
        if ((info->osfile & 0x48u) != 0)
        {
            if (peek == '\n')
            {
                *dst = '\n';
            }
            else
            {
                *dst = '\r';
                info->pipech = static_cast<u8>(static_cast<unsigned char>(peek));
            }
            dst += 1;
            continue;
        }
        if (dst == buf && peek == '\n')
        {
            *dst = '\n';
            dst += 1;
            continue;
        }

        fn_16d466(fh, -1, 1u);
        if (peek != '\n')
        {
            *dst = '\r';
            dst += 1;
        }
    }
    return static_cast<int>(dst - buf);
}

void fn_16d7e1(int fh)
{
    // Xbox 0x0016D7E1
    // Shared unlock tail for _read (0x0016D75A). __SEH_prolog (0x00166D58)
    // is entered with local size 0x0C and the scope table at 0x00209C98:
    // one entry, enclosing level -1, a null filter, and the handler at
    // 0x0016D7DE. That handler loads the descriptor from [ebp+8] into EBX
    // and falls through to this address. The success path at 0x0016D7D4
    // calls here after _read_lk (0x0016D583) with EBX already holding that
    // descriptor. The eight bytes push EBX, call _unlock_fhandle
    // (0x0016DC4E), pop the cdecl argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    fn_168c8e(fh);
}

void fn_16dc45()
{
    // Xbox 0x0016DC45
    // Shared unlock tail of _lock_fhandle (0x0016DBAE). __SEH_prolog
    // (0x00166D58) is entered with local size 8 and the scope table at
    // 0x00209E98: one entry, enclosing level -1, a null filter, and the
    // handler at 0x0016DC42. That handler loads the descriptor from
    // [ebp+8] into EDI and falls through to this address. The success
    // path at 0x0016DC13 sets the try level at [ebp-4] to -1 and calls
    // here. EDI already holds the descriptor, and these nine bytes do
    // not read it: push 10 (_LOCKTAB_LOCK), call _unlock (0x0016810F),
    // pop the cdecl argument, and return.
    //
    // _unlock leaves the critical section in _locktable[10] (image
    // 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // _local_unwind2 (0x00167062) runs the handler when
    // InitializeCriticalSectionAndSpinCount (0x0016D9F8) returns 0.
    // _lock_fhandle then returns 0 and leaves the file-handle section
    // unentered. After this return on the success path, _lock_fhandle
    // enters the section at record+0xC through RtlEnterCriticalSection
    // (thunk 0x00202EA0). A section __mtinitlocks has not created is
    // left alone; retail would fault on the null pointer.
    if (g_crt_locks[10].cs != nullptr)
    {
        g_crt_locks[10].cs->unlock();
    }
}

bool crt_write_file(std::intptr_t handle, const void* buffer, u32 bytes, u32* wrote, u32* doserr)
{
    // WriteFile at 0x0014B490. Nonzero is success. Zero keeps GetLastError
    // (0x0014AFE4). The transferred count is stored either way; _write_lk
    // reads it only after a nonzero return.
#if defined(_WIN32)
    unsigned long n = 0;
    const int ok = WriteFile(reinterpret_cast<void*>(handle), buffer, bytes, &n, nullptr);
    *wrote = static_cast<u32>(n);
    if (ok != 0)
    {
        return true;
    }
    *doserr = static_cast<u32>(GetLastError());
    return false;
#else
    (void)handle;
    (void)buffer;
    (void)doserr;
    *wrote = bytes;
    return true;
#endif
}

int fn_16dc70(u32 fh, const char* buf, u32 count)
{
    // Xbox 0x0016DC70
    // _write_lk. cdecl. The caller pops the descriptor, the buffer, and the
    // byte count. This body does not lock the descriptor; the locked _write
    // calls here with that lock already held.
    //
    // A count of 0 returns 0. The record is __pioinfo[fd >> 5] at
    // 0x0054DC60, slot (fd & 0x1F), stride 0x28. The OS handle is at +0 and
    // osfile is at +4. A null __pioinfo slot is reported as EBADF; retail
    // would fault on that address.
    //
    // FAPPEND (bit 0x20) calls _lseeki64_lk (0x0016F270) with offset halves
    // 0 and 0 and FILE_END (2). The 64-bit result is ignored, including a
    // failure that has already stored errno. Both halves are zero, so
    // _lseek_lk (0x0016D466) leaves the pointer at the same place and clears
    // FEOFLAG on success.
    //
    // FTEXT (bit 0x80) copies the buffer through a 1028-byte stack slot.
    // Each LF is counted and preceded by CR. The copy stops once the
    // translated count reaches 0x400, so one last LF can land at 0x401.
    // Each chunk is WriteFile (0x0014B490). A zero return stores
    // GetLastError and stops. A short write, or a chunk that consumes the
    // rest of the input, stops as well. A full chunk with bytes remaining
    // translates another chunk.
    //
    // Any other osfile writes the caller's buffer in one WriteFile. Success
    // stores a zero DOS code and the transferred count. Failure stores
    // GetLastError and leaves the count at zero.
    //
    // A nonzero transferred total returns that total minus the LF count.
    // A zero total with DOS code 5 (ERROR_ACCESS_DENIED) stores EBADF (9)
    // through _errno (0x00168AF5) and 5 through __doserrno (0x00168AFE),
    // then returns -1. Any other DOS code goes through __dosmaperr
    // (0x00168B07) and returns -1. A zero total and a zero DOS code returns
    // 0 when FDEV (bit 0x40) is set and the first buffer byte is CTRL-Z
    // (0x1A). Otherwise errno becomes ENOSPC (0x1C), _doserrno becomes 0,
    // and the return is -1.
    if (count == 0)
    {
        return 0;
    }

    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr)
    {
        *fn_168af5() = 9;
        return -1;
    }

    if ((info->osfile & 0x20u) != 0)
    {
        fn_16d466(fh, 0, 2u);
    }

    int written_total = 0;
    int lfcount = 0;
    u32 doserr = 0;

    if ((info->osfile & 0x80u) == 0)
    {
        u32 wrote = 0;
        if (crt_write_file(info->osfhnd, buf, count, &wrote, &doserr))
        {
            doserr = 0;
            written_total = static_cast<int>(wrote);
        }
    }
    else
    {
        const char* src = buf;
        char lfbuf[1028];
        while (static_cast<u32>(src - buf) < count)
        {
            char* out = lfbuf;
            int filled = 0;
            u32 consumed = static_cast<u32>(src - buf);
            do
            {
                if (consumed >= count)
                {
                    break;
                }
                const char ch = *src;
                src += 1;
                consumed += 1u;
                if (ch == '\n')
                {
                    lfcount += 1;
                    *out = '\r';
                    out += 1;
                    filled += 1;
                }
                *out = ch;
                out += 1;
                filled += 1;
            } while (filled < 0x400);

            const int chunk = static_cast<int>(out - lfbuf);
            u32 wrote = 0;
            if (!crt_write_file(info->osfhnd, lfbuf, static_cast<u32>(chunk), &wrote, &doserr))
            {
                break;
            }
            written_total += static_cast<int>(wrote);
            doserr = 0;
            if (static_cast<int>(wrote) < chunk || static_cast<u32>(src - buf) >= count)
            {
                break;
            }
        }
    }

    if (written_total != 0)
    {
        return written_total - lfcount;
    }
    if (doserr != 0)
    {
        if (doserr == 5u)
        {
            *fn_168af5() = 9;
            *fn_168afe() = 5u;
            return -1;
        }
        crt_dosmaperr(doserr);
        return -1;
    }
    if ((info->osfile & 0x40u) != 0 && *buf == '\x1a')
    {
        return 0;
    }
    *fn_168af5() = 0x1c;
    *fn_168afe() = 0;
    return -1;
}

void fn_16de82(int fh)
{
    // Xbox 0x0016DE82
    // Shared unlock tail for _write (0x0016DDFB). __SEH_prolog (0x00166D58)
    // is entered with local size 0x0C and the scope table at 0x00209EA8:
    // one entry, enclosing level -1, a null filter, and the handler at
    // 0x0016DE7F. That handler loads the descriptor from [ebp+8] into EBX
    // and falls through to this address. After _lock_fhandle (0x0016DBAE)
    // the try level at [ebp-4] is 0. The join at 0x0016DE71, after
    // _write_lk (0x0016DC70) or the second FOPEN failure, sets that level
    // to -1 and calls here. EBX already holds the descriptor. The eight
    // bytes push EBX, call _unlock_fhandle (0x0016DC4E), pop the cdecl
    // argument, and return.
    //
    // _unlock_fhandle arithmetic-shifts the descriptor by 5 and reads
    // __pioinfo at 0x0054DC60. The record is (descriptor & 0x1F), stride
    // 0x28. RtlLeaveCriticalSection (kernel ordinal 294, thunk 0x00202EA4)
    // takes the critical section at record+0xC. A null __pioinfo slot, or a
    // section _lock_fhandle has not created, is left alone; retail would
    // fault on that address.
    fn_168c8e(fh);
}

int fn_16d9f8(std::recursive_mutex* section, u32 spin_count);

int fn_16dea6(u32 fh)
{
    // Xbox 0x0016DEA6
    // _commit. cdecl. __SEH_prolog (0x00166D58) is entered with local size
    // 0x0C and the scope table at 0x00209EB8: one entry, enclosing level -1,
    // a null filter, and the handler at 0x0016DF41. __SEH_epilog (0x00166D93)
    // returns the value left in EAX.
    //
    // A descriptor greater than or equal to _nhandle (0x0054DC40), or one
    // whose osfile byte lacks FOPEN (bit 0), stores EBADF (9) through
    // _errno (0x00168AF5) and returns -1. _doserrno is left alone. The
    // record is __pioinfo[fd >> 5] at 0x0054DC60, slot (fd & 0x1F), stride
    // 0x28, osfile at +4. A null __pioinfo slot is the same EBADF return;
    // retail would fault on that address.
    //
    // Otherwise _lock_fhandle (0x0016DBAE) is called and its return is
    // discarded. That callee creates the critical section at record+0xC
    // when the dword at +8 is still zero: it takes _LOCKTAB_LOCK (10),
    // calls InitializeCriticalSectionAndSpinCount (0x0016D9F8) with spin
    // 0xFA0, increments the dword, and _unlocks lock 10. A failed init
    // unwinds lock 10 and returns 0 without entering the section. A live
    // section is entered with RtlEnterCriticalSection (thunk 0x00202EA0).
    // The try level at [ebp-4] is then 0.
    //
    // FOPEN is tested again. If it is clear, errno becomes 9 and the result
    // at [ebp-0x1c] becomes -1. If it is set, __get_osfhandle (0x0016DB6D)
    // supplies the OS handle to FlushFileBuffers (0x0014B884, stdcall ret 4).
    // A nonzero return stores 0. A zero return stores GetLastError
    // (0x0014AFE4). A zero code leaves the result 0 and does not touch
    // errno. Any other code is stored through __doserrno (0x00168AFE), errno
    // becomes 9, and the result becomes -1. __dosmaperr is not used.
    //
    // The success path stores try level -1 and calls 0x0016DF44. The unwind
    // handler at 0x0016DF41 reloads the descriptor from [ebp+8] into EBX and
    // falls through to that address. Both push the descriptor and call
    // _unlock_fhandle (0x0016DC4E). A section this call did not create is
    // left alone.
    CrtIoInfo* info = crt_ioinfo_checked(fh);
    if (info == nullptr || (info->osfile & 1u) == 0)
    {
        *fn_168af5() = 9;
        return -1;
    }

    if (info->field8 == 0)
    {
        fn_1681c1(10);
        if (info->field8 == 0)
        {
            bool created = false;
            try
            {
                info->cs_owner = std::make_unique<std::recursive_mutex>();
                created = fn_16d9f8(info->cs_owner.get(), 0xFA0) != 0;
            }
            catch (...)
            {
                created = false;
            }
            if (!created)
            {
                info->cs_owner.reset();
                info->cs = nullptr;
                fn_1681b8();
            }
            else
            {
                info->cs = info->cs_owner.get();
                info->field8 += 1u;
                fn_1681b8();
                info->cs->lock();
            }
        }
        else if (info->cs != nullptr)
        {
            fn_1681b8();
            info->cs->lock();
        }
        else
        {
            fn_1681b8();
        }
    }
    else if (info->cs != nullptr)
    {
        info->cs->lock();
    }

    int result = -1;
    if ((info->osfile & 1u) != 0)
    {
        const std::intptr_t handle = crt_get_osfhandle(fh);
        u32 dos = 0;
#if defined(_WIN32)
        if (FlushFileBuffers(reinterpret_cast<void*>(handle)) == 0)
        {
            dos = static_cast<u32>(GetLastError());
        }
#else
        (void)handle;
#endif
        if (dos == 0)
        {
            result = 0;
        }
        else
        {
            *fn_168afe() = dos;
            *fn_168af5() = 9;
            result = -1;
        }
    }
    else
    {
        *fn_168af5() = 9;
        result = -1;
    }

    fn_168c8e(static_cast<int>(fh));
    return result;
}

u32 fn_16dfb5(u32 unit, void* stream)
{
    // Xbox 0x0016DFB5
    // _fputwc_nolock. cdecl. The only caller is write_char in _woutput
    // (0x001698B6): it pushes the FILE and the unit, then compares AX with
    // 0xFFFF. A match stores -1 in the character count. Anything else
    // increments that count. A string whose base is null never reaches here.
    //
    // The host FILE matches _woutput: ptr, cnt, base, flag, then the
    // descriptor. The low flag byte is tested for _IOSTRG (0x40). A string
    // stream skips the text check. Any other stream reads osfile. A
    // descriptor of -1 uses the dummy ioinfo at 0x00338250, whose image
    // bytes are osfhnd -1, osfile 0x80, and pipech 10. Any other descriptor
    // is __pioinfo[fd >> 5] at 0x0054DC60, slot (fd & 0x1F), stride 0x28,
    // osfile at +4. Bit 0x80 is FTEXT. A null __pioinfo slot is left with a
    // clear osfile; retail would fault on that address.
    //
    // FTEXT calls wctomb (0x0016DF8A) with an 8-byte buffer. That callee
    // reads the low 16 bits. A unit above 0xFF stores EILSEQ (0x2A) through
    // _errno (0x00168AF5) and returns -1; this function stores 0x2A again
    // and returns WEOF. A unit that fits is one byte. cnt drops by 1. A
    // negative cnt is _flsbuf (0x00168E46): a string stream, or a stream
    // that is not open for write, sets _IOERR (flag 0x20) and returns EOF.
    // The low half of that EOF is WEOF, which is what write_char tests.
    // The text path then returns the unit masked to 16 bits.
    //
    // A string stream, or a stream whose osfile lacks FTEXT, drops cnt by 2
    // and stores the low 16 bits at ptr. A negative cnt is _flswbuf
    // (0x0016F2F1), which sets _IOERR and returns WEOF for a string stream
    // and for a stream that is not open for write. The port has no file
    // flush, so a full buffer takes that same WEOF. A store that fits
    // returns the original unit.

    struct StreamFile
    {
        char* ptr;
        int cnt;
        char* base;
        int flag;
        int file;
        int charbuf;
        int bufsiz;
        char* tmpfname;
    };

    auto* file = static_cast<StreamFile*>(stream);
    if ((static_cast<unsigned char>(file->flag) & 0x40) == 0)
    {
        u8 osfile = 0;
        if (file->file == -1)
        {
            osfile = 0x80;
        }
        else
        {
            CrtIoInfo* info = crt_ioinfo_checked(static_cast<u32>(file->file));
            if (info != nullptr)
            {
                osfile = info->osfile;
            }
        }
        if ((osfile & 0x80) != 0)
        {
            const u16 wc = static_cast<u16>(unit);
            if (wc > 0xffu)
            {
                *fn_168af5() = 0x2a;
                return 0xffffu;
            }

            file->cnt -= 1;
            if (file->cnt < 0)
            {
                file->flag |= 0x20;
                return 0xffffu;
            }
            *file->ptr = static_cast<char>(wc);
            file->ptr += 1;
            return unit & 0xffffu;
        }
    }

    file->cnt -= 2;
    if (file->cnt < 0)
    {
        file->flag |= 0x20;
        return 0xffffu;
    }
    const auto wide = static_cast<std::uint16_t>(unit);
    std::memcpy(file->ptr, &wide, sizeof(wide));
    file->ptr += 2;
    return unit;
}

int fn_16e07a(char16_t* dest, const char* src, int count)
{
    // Xbox 0x0016E07A
    // mbtowc. cdecl; the caller pops the three arguments. This body is the
    // single-byte form. A null source or a zero count returns 0 and leaves
    // the destination alone. A zero byte stores 0 when the destination is
    // non-null and returns 0. Any other byte is zero-extended into that
    // word and returns 1. A null destination skips the store and still
    // returns 1 for a nonzero byte. The count is tested only against zero,
    // so a lead-byte caller that passes 2 still converts one byte. The
    // return is 0 or 1.
    //
    // _woutput (0x0016993D) passes __mb_cur_max. 0x00169C84 widens the %hc
    // byte, and 0x0016A068 walks narrow text one byte at a time. _winput
    // (0x0016A17C) at 0x0016A4E1 widens the decimal byte at 0x0033818C.
    if (src != nullptr && count != 0)
    {
        const unsigned char byte = static_cast<unsigned char>(*src);
        if (byte != 0)
        {
            if (dest != nullptr)
            {
                *dest = static_cast<char16_t>(byte);
            }
            return 1;
        }
        if (dest != nullptr)
        {
            *dest = 0;
        }
    }
    return 0;
}

void fn_168e08(int index)
{
    // Xbox 0x00168E08
    // Shared unlock body of flsall (0x00168D68). The scope table at
    // 0x00209268 has two entries. The inner one (enclosing level 0, a null
    // filter) is the handler at 0x00168E03: it clears EDI and reloads the
    // stream index from [ebp-0x20] into ESI, then falls through to this
    // address. The success path at 0x00168DFB calls here after the try
    // level at [ebp-4] is stored as 0, with ESI already holding that index.
    // The seventeen bytes load __piob (0x0054DD74), push __piob[index] and
    // the index, call _unlock_file2 (0x001689F1), pop both cdecl arguments,
    // and return.
    //
    // _unlock_file2 compares the index with 0x14 (_IOB_ENTRIES). The
    // compare is signed jge. Below that it adds 0x10 (_STREAM_LOCKS) and
    // calls _unlock (0x0016810F), which leaves the critical section in
    // _locktable (image 0x00337AB0). The kernel thunk is the dword at
    // 0x00202EA4. An index of 0x14 or more calls RtlLeaveCriticalSection
    // on the critical section at FILE+0x20. A null __piob, an index
    // outside the allocated table, or a null FILE on the FILEX path would
    // fault; the port returns. A stream lock __mtinit has not created is
    // left alone.
    if (g_crt_piob == nullptr || index < 0 || static_cast<u32>(index) >= g_crt_nstream)
    {
        return;
    }

    CrtFile* stream = g_crt_piob[index];
    if (index < 0x14)
    {
        const u32 locknum = static_cast<u32>(index) + 0x10u;
        if (g_crt_locks[locknum].cs != nullptr)
        {
            g_crt_locks[locknum].cs->unlock();
        }
        return;
    }

    if (stream == nullptr)
    {
        return;
    }
    reinterpret_cast<CrtFileX*>(stream)->lock.unlock();
}

void fn_168e34()
{
    // Xbox 0x00168E34
    // Outer __finally of flsall (0x00168D68). The scope table at
    // 0x00209268 has two entries. This one (enclosing level -1, a null
    // filter) is the handler. The inner entry (enclosing level 0, a null
    // filter) is the handler at 0x00168E03, which falls into fn_168e08.
    // flsall pushes 1 (_IOB_SCAN_LOCK) and calls _lock (0x001681C1)
    // before the __piob scan, stores try level 0 at [ebp-4], and on the
    // success path at 0x00168E19 sets that level to -1 and calls here.
    // _local_unwind2 (0x00167062) runs the same handler. The nine bytes
    // push 1, call _unlock (0x0016810F), pop the cdecl argument, and
    // return. _unlock leaves the critical section in _locktable[1]
    // (image 0x00337AB0). The kernel thunk is the dword at 0x00202EA4.
    // A section __mtinitlocks has not created is left alone; retail
    // would fault on the null pointer.
    if (g_crt_locks[1].cs != nullptr)
    {
        g_crt_locks[1].cs->unlock();
    }
}

int crt_mtinitlocknum(int locknum)
{
    // __mtinitlocknum at 0x00168121. Scope table 0x00209190, prolog
    // 0x00166D58. A live pointer returns 1. Otherwise 0x00167F1C allocates
    // 0x1C bytes. A null block stores ENOMEM (0xC) through 0x00168AF5
    // (per-thread data + 8) and returns 0. The host mutex stands in for
    // that block; a failed allocation is the same zero return.
    //
    // The try level at [ebp-4] is 0 while _lock holds _LOCKTAB_LOCK (10).
    // Kind 1 at that index is created by __mtinitlocks. When this index is
    // 10 and the pointer is still null, the section is stored and the
    // caller enters it. Taking _lock(10) from inside that init would call
    // back here before the pointer is stored. A section that appeared while
    // the table lock was held is freed (0x00167F2E) and the new one is kept.
    //
    // 0x0016D9F8 initializes the section with spin count 0xFA0. A zero
    // return frees the block, stores ENOMEM, and _local_unwind2 (0x00167062)
    // runs the finally at 0x001681B8, which is _unlock (0x0016810F) of lock
    // 10. The host constructor runs before that call. The finally still
    // leaves lock 10. Index 10 is stored without taking the table lock, so
    // a failed init on that index does not unlock. Success returns 1.
    if (g_crt_locks[locknum].cs != nullptr)
    {
        return 1;
    }

    std::unique_ptr<std::recursive_mutex> pcs;
    try
    {
        pcs = std::make_unique<std::recursive_mutex>();
    }
    catch (...)
    {
        *fn_168af5() = 12;
        return 0;
    }

    if (locknum == 10)
    {
        if (fn_16d9f8(pcs.get(), 0xFA0) == 0)
        {
            *fn_168af5() = 12;
            return 0;
        }
        g_crt_lock_mu[10] = std::move(pcs);
        g_crt_locks[10].cs = g_crt_lock_mu[10].get();
        return 1;
    }

    fn_1681c1(10);
    if (g_crt_locks[locknum].cs == nullptr)
    {
        if (fn_16d9f8(pcs.get(), 0xFA0) == 0)
        {
            pcs.reset();
            *fn_168af5() = 12;
            fn_1681b8();
            return 0;
        }
        g_crt_lock_mu[locknum] = std::move(pcs);
        g_crt_locks[locknum].cs = g_crt_lock_mu[locknum].get();
    }
    else
    {
        pcs.reset();
    }
    fn_1681b8();
    return 1;
}

void fn_167bde(void* ptr, u32 element_size, int constructed, void (*dtor)(void*), int success)
{
    // Xbox 0x00167BDE
    // Finally of __ehvec_ctor (0x00167B94). The scope table at 0x00209140
    // has one entry: enclosing level -1, a null filter, and this handler.
    // __SEH_prolog (0x00166D58) is entered with local size 0x10. This body
    // builds no frame, so EBP stays the constructor iterator's.
    // [ebp-0x20] is the success flag, [ebp-0x1c] is how many constructors
    // have returned, [ebp+8] is the element pointer, [ebp+0xc] is the
    // stride, and [ebp+0x18] is the destructor. The iterator stores 0 in
    // the flag and the count, sets the try level at [ebp-4] to 0, and
    // constructs [ebp+0x10] elements through the thiscall at [ebp+0x14].
    // After a constructor returns, the pointer advances by the stride and
    // the count increments. A throw leaves both as they were: the pointer
    // addresses the element that failed, and the count is the number
    // already built. The success path at 0x00167BC6 stores 1, sets the try
    // level to -1, and calls here. The compare is jne, so a nonzero flag
    // returns. A zero flag pushes the destructor, the count, the stride,
    // and the pointer, then calls __ArrayUnwind (0x00167BF6, stdcall,
    // ret 0x10). That callee decrements the count and stops once the
    // result is negative, subtracts the stride, and calls the destructor
    // with ECX on that element. A C++ exception (0xE06D7363) from a
    // destructor takes the filter at 0x00167C25 into terminate
    // (0x0016C177). Any other exception code returns 0 from the filter.
    if (success != 0)
    {
        return;
    }

    auto* cursor = static_cast<char*>(ptr);
    while (constructed > 0)
    {
        --constructed;
        cursor -= element_size;
        try
        {
            dtor(cursor);
        }
        catch (...)
        {
            fn_16c177();
        }
    }
}

void fn_167c9c(void* ptr, u32 element_size, int count, void (*dtor)(void*), int success)
{
    // Xbox 0x00167C9C
    // Finally of __ehvec_dtor (0x00167C54). The scope table at 0x00209160
    // has one entry: enclosing level -1, a null filter, and this handler.
    // __SEH_prolog (0x00166D58) is entered with local size 0x0C. This body
    // builds no frame, so EBP stays the destructor iterator's.
    // [ebp-0x1c] is the success flag, [ebp+8] is the element pointer,
    // [ebp+0xc] is the stride, [ebp+0x10] is the count, and [ebp+0x14] is
    // the destructor. The iterator stores 0 in the flag, multiplies the
    // stride by the count, and adds that product to the pointer so it sits
    // just past the last element. It sets the try level at [ebp-4] to 0,
    // then decrements the count. A negative result leaves the loop.
    // Otherwise it subtracts the stride and calls the destructor with ECX
    // on that element. A throw leaves the pointer on the element that
    // failed and the count already decremented for it. The success path at
    // 0x00167C84 stores 1, sets the try level to -1, and calls here. The
    // compare is jne, so a nonzero flag returns. A zero flag pushes the
    // destructor, the count, the stride, and the pointer, then calls
    // __ArrayUnwind (0x00167BF6, stdcall, ret 0x10). That callee decrements
    // the count and stops once the result is negative, subtracts the stride,
    // and calls the destructor with ECX on that element. A C++ exception
    // (0xE06D7363) from a destructor takes the filter at 0x00167C25 into
    // terminate (0x0016C177). Any other exception code returns 0 from the
    // filter.
    if (success != 0)
    {
        return;
    }

    auto* cursor = static_cast<char*>(ptr);
    while (count > 0)
    {
        --count;
        cursor -= element_size;
        try
        {
            dtor(cursor);
        }
        catch (...)
        {
            fn_16c177();
        }
    }
}

void* fn_167ec9(u32 size)
{
    // Xbox 0x00167EC9
    // _heap_alloc. The caller is _nh_malloc_base (0x00167EF0), which malloc
    // (0x00167F1C) calls with _newmode. A zero request becomes one byte.
    // __active_heap (0x0054DD60) is __SYSTEM_HEAP when it equals 1. Any other
    // value, including the BSS zero, rounds the count up to a multiple of
    // 16. The two pushes are the HeapAlloc size and flags. 0x0014D377 is the
    // six-byte load of _crtheap (0x00549520) and does not consume them.
    // 0x0014E872 is HeapAlloc(heap, 0, size) and returns the block in EAX
    // after its stdcall cleanup. The host allocator matches free (0x00167F2E).
    if (size == 0)
    {
        size = 1;
    }
    if (g_crt_active_heap != 1)
    {
        size = (size + 0xfu) & 0xfffffff0u;
    }
    return std::malloc(size);
}

void fn_167f2e(void* block)
{
    // Xbox 0x00167F2E
    // free. The printf helpers at 0x00168FED and 0x0016993D release a
    // temporary buffer here, and _fcloseall (0x0016DAB4) releases FILE
    // records past the first twenty. A null block returns before the heap
    // check. __SEH_prolog (0x00166D58) builds the frame from the scope table
    // at 0x00209170; __SEH_epilog (0x00166D93) tears it down.
    //
    // When __active_heap is __V6_HEAP (3), _lock (0x001681C1) takes
    // _HEAP_LOCK (4). A missing critical section is the __mtinitlocknum path
    // inside _lock; kind 1 matches the descriptor table. The __try stores 0
    // and returns the EH state to -1. The __finally at 0x00167F7B clears ESI
    // on the unwind entry, and both that entry and the direct call at
    // 0x00167F7D fall into _unlock (0x0016810F). The stored 0 fails the
    // compare, so the early-out is not taken.
    //
    // That fall-through and every other heap kind push the block and flags
    // 0, call 0x0014D377 (EAX becomes _crtheap at 0x00549520), push the
    // handle, and call the HeapFree wrapper at 0x0014D360. The host releases
    // the block with the process heap, as 0x000663E0 does.
    if (block == nullptr)
    {
        return;
    }

    if (g_crt_active_heap == 3)
    {
        fn_1681c1(4);
        g_crt_locks[4].cs->unlock();
    }

    std::free(block);
}

int fn_168fed(void* stream, const char* format, std::va_list argptr)
{
    // Xbox 0x00168FED
    // _output. The printf family builds a FILE and calls here. The return is
    // the number of characters accepted, or -1 after a failed write.
    //
    // Retail FILE is 0x20 bytes with 32-bit pointers: ptr, cnt, base, flag,
    // then the handle fields. The port keeps that order and stores host
    // pointers, so a destination buffer is addressable. A null format
    // returns 0. The walk is the table at 0x00209280. A character from 0x20
    // through 0x78 takes its class from the low nibble. Characters outside
    // that span are class 0.
    // The next state is the high nibble of the byte at class * 8 + state.
    // The states are literal, conversion, flag, width, dot, precision, size,
    // and type. The jump table for those states is at 0x00169754.
    //
    // Flag bits are the CRT set: '+' 0x1, ' ' 0x2, '-' 0x4, '0' 0x8, 'l' 0x10,
    // 'h' 0x20, signed 0x40, '#' 0x80, negative 0x100, forced octal 0x200,
    // 'w' 0x800, and 'I64' 0x8000. %I64 and %I32 consume the two digits. %I
    // immediately before d, i, o, u, x, or X is ignored. Any other character
    // after %I is written as a literal and the state returns to literal.
    //
    // Literals go through write_char (0x00168F5F). When the high byte of
    // _pctype[ch] (0x00338180) has bit 0x80, the next source byte is written
    // as well. The C locale table has that bit clear. write_multi_char
    // (0x00168F92) pads. write_string (0x00168FB6) copies. A stream whose
    // flag has 0x40 and whose base is null only advances the count. Any
    // other stream stores the byte at ptr while cnt stays non-negative.
    // A full buffer is _flsbuf (0x00168E46). A string stream, or a stream
    // that is not open for write, sets flag bit 0x20 and fails the count.
    // The port reports that same failure when a real file buffer is full.
    //
    // Integers are reduced by __aulldvrm (0x0016CFA0). %n stores the count.
    // %Z reads a counted record: the word at +0 is the byte length, and the
    // pointer at +4 is the buffer. A null record prints "(null)" from
    // 0x002092DC; the wide null string is the UTF-16 text at 0x002092E4.
    // Floats take a double and call the slots _fpmath fills at 0x003381A0
    // (__cfltcvt), 0x003381AC (__forcdecpt), and 0x003381A4 (__cropzeros).
    // While a slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. Precision above 0xA3 mallocs (0x00167F1C) precision + 0x15D
    // bytes and this function frees that block (0x00167F2E). Wide units
    // expand through wctomb (0x0016DF8A). With __mb_cur_max at 1 that stores
    // one byte, or returns -1 and sets errno to EILSEQ (42) when the unit
    // does not fit in a byte.
    //
    // The host va_list is the Xbox argument cursor. Each promoted integer or
    // pointer is one argument. A double or an I64 value is one 8-byte
    // argument. %p prints a host pointer in uppercase hex, at least eight
    // digits. On Xbox the pointer is 32 bits, so eight digits are the whole
    // value.

    struct OutputFile
    {
        char* ptr;
        int cnt;
        char* base;
        int flag;
        int file;
        int charbuf;
        int bufsiz;
        char* tmpfname;
    };

    // Image bytes at 0x00209280. Index 0 is the character ' '.
    static constexpr unsigned char kLookup[] = {
        0x06, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x03, 0x06, 0x00, 0x06, 0x02, 0x10,
        0x04, 0x45, 0x45, 0x45, 0x05, 0x05, 0x05, 0x05, 0x05, 0x35, 0x30, 0x00, 0x50, 0x00, 0x00, 0x00,
        0x00, 0x20, 0x28, 0x38, 0x50, 0x58, 0x07, 0x08, 0x00, 0x37, 0x30, 0x30, 0x57, 0x50, 0x07, 0x00,
        0x00, 0x20, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x60, 0x68, 0x60, 0x60, 0x60, 0x60, 0x00,
        0x00, 0x70, 0x70, 0x78, 0x78, 0x78, 0x78, 0x08, 0x07, 0x08, 0x00, 0x00, 0x07, 0x00, 0x08, 0x08,
        0x08, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x08,
    };
    static_assert(sizeof(kLookup) == 89);

    constexpr int kSign = 0x1;
    constexpr int kBlank = 0x2;
    constexpr int kLeft = 0x4;
    constexpr int kZero = 0x8;
    constexpr int kLong = 0x10;
    constexpr int kShort = 0x20;
    constexpr int kSigned = 0x40;
    constexpr int kAlt = 0x80;
    constexpr int kNeg = 0x100;
    constexpr int kOctal = 0x200;
    constexpr int kWide = 0x800;
    constexpr int kI64 = 0x8000;
    constexpr u32 kFptrap = 0x0016F08Cu;

    static constexpr char kNullText[] = "(null)";
    static constexpr char16_t kNullWide[] = {'(', 'n', 'u', 'l', 'l', ')', 0};

    auto* out = static_cast<OutputFile*>(stream);
    int charsout = 0;
    int state = 0;
    int flags = 0;
    int precision = -1;
    int width = 0;
    int prefix_len = 0;
    int text_len = 0;
    int no_output = 0;
    int wide_chars = 0;
    int capexp = 0;
    int hex_shift = 0;
    int radix = 10;
    char prefix[2] = {};
    const char* text = "";
    char* heap = nullptr;
    char stack_buf[0x200];
    char digits[513];
    char mb[8];
    u32 lo = 0;
    u32 hi = 0;

    auto write_char = [&](unsigned char byte)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += 1;
            return;
        }

        int wrote = static_cast<int>(byte);
        out->cnt -= 1;
        if (out->cnt >= 0)
        {
            *out->ptr = static_cast<char>(byte);
            out->ptr += 1;
        }
        else
        {
            // _flsbuf sets the error bit and returns EOF for a string stream
            // and for a stream that is not open for write. A buffered file
            // would flush here; that body lives at 0x00168E46.
            out->flag |= 0x20;
            wrote = -1;
        }
        if (wrote == -1)
        {
            charsout = -1;
        }
        else
        {
            charsout += 1;
        }
    };

    auto write_multi = [&](unsigned char byte, int count)
    {
        while (count > 0)
        {
            count -= 1;
            write_char(byte);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto write_string = [&](const char* bytes, int count)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += count;
            return;
        }
        while (count > 0)
        {
            const unsigned char byte = static_cast<unsigned char>(*bytes);
            count -= 1;
            bytes += 1;
            write_char(byte);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    const unsigned char* cursor = nullptr;
    auto write_literal = [&](unsigned char byte)
    {
        wide_chars = 0;
        if ((g_crt_pctype[byte] & 0x8000u) != 0)
        {
            write_char(byte);
            byte = *cursor;
            cursor += 1;
        }
        write_char(byte);
    };

    auto wctomb_byte = [&](char* dest, int wchar) -> int
    {
        if (g_crt_mb_cur_max <= 1)
        {
            if (wchar < 0 || wchar > 0xff)
            {
                errno = 42;
                return -1;
            }
            dest[0] = static_cast<char>(wchar);
            return 1;
        }
        errno = 42;
        return -1;
    };

    auto force_decimal = [](char* s)
    {
        char* p = s;
        while (*p != '\0')
        {
            if (*p == '.')
            {
                return;
            }
            if (*p == 'e' || *p == 'E')
            {
                break;
            }
            p += 1;
        }
        char* end = p;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= p)
        {
            *(end + 1) = *end;
            end -= 1;
        }
        *p = '.';
    };

    auto crop_zeros = [](char* s)
    {
        char* p = s;
        while (*p != '\0' && *p != '.')
        {
            p += 1;
        }
        if (*p == '\0')
        {
            return;
        }
        while (*p != '\0' && *p != 'e' && *p != 'E')
        {
            p += 1;
        }
        char* stop = p;
        if (p > s)
        {
            p -= 1;
        }
        while (p > s && *p == '0')
        {
            p -= 1;
        }
        if (*p == '.' && p > s)
        {
            p -= 1;
        }
        p += 1;
        while (*stop != '\0')
        {
            *p = *stop;
            p += 1;
            stop += 1;
        }
        *p = '\0';
    };

    auto pad_exponent = [](char* s, int buf_size)
    {
        char* mark = std::strchr(s, 'e');
        if (mark == nullptr)
        {
            mark = std::strchr(s, 'E');
        }
        if (mark == nullptr || (mark[1] != '+' && mark[1] != '-'))
        {
            return;
        }
        char* digits_at = mark + 2;
        int count = 0;
        while (digits_at[count] >= '0' && digits_at[count] <= '9')
        {
            count += 1;
        }
        if (count == 0 || count >= 3)
        {
            return;
        }
        const int need = 3 - count;
        const int used = static_cast<int>(std::strlen(s));
        if (used + need + 1 > buf_size)
        {
            return;
        }
        char* end = digits_at + count;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= digits_at)
        {
            *(end + need) = *end;
            end -= 1;
        }
        for (int i = 0; i < need; i += 1)
        {
            digits_at[i] = '0';
        }
    };

    auto format_float = [&](char* buffer, int buf_size, char fmt, double value)
    {
        if (g_cfltcvt_tab[0] == kFptrap)
        {
            std::exit(2);
        }
        if (!std::isfinite(value))
        {
            const char* token = "1.#QNAN";
            if (std::isnan(value))
            {
                token = std::signbit(value) ? "-1.#IND" : "1.#QNAN";
            }
            else
            {
                token = std::signbit(value) ? "-1.#INF" : "1.#INF";
            }
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%s", token);
            return;
        }

        const bool upper = capexp != 0;
        if (fmt == 'e')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision, value);
        }
        else if (fmt == 'f')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", precision, value);
        }
        else
        {
            bool scientific = false;
            int exponent = 0;
            if (value != 0.0)
            {
                char probe[64];
                std::snprintf(probe, sizeof(probe), "%.*e", precision - 1, value);
                const char* mark = std::strchr(probe, 'e');
                if (mark == nullptr)
                {
                    mark = std::strchr(probe, 'E');
                }
                if (mark != nullptr)
                {
                    exponent = std::atoi(mark + 1);
                }
                scientific = exponent < -4 || exponent >= precision;
            }
            if (scientific)
            {
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision - 1, value);
            }
            else
            {
                int frac = precision - exponent - 1;
                if (frac < 0)
                {
                    frac = 0;
                }
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", frac, value);
            }
        }
        pad_exponent(buffer, buf_size);
    };

    auto write_number = [&](bool as_i64)
    {
        if ((flags & kSigned) != 0 && static_cast<std::int32_t>(hi) < 0)
        {
            unsigned long long mag = (static_cast<unsigned long long>(hi) << 32) | lo;
            mag = 0ull - mag;
            lo = static_cast<u32>(mag);
            hi = static_cast<u32>(mag >> 32);
            flags |= kNeg;
        }
        if (!as_i64)
        {
            hi = 0;
        }
        if (precision < 0)
        {
            precision = 1;
        }
        else
        {
            flags &= ~kZero;
            if (precision > 0x200)
            {
                precision = 0x200;
            }
        }
        if (lo == 0 && hi == 0)
        {
            prefix_len = 0;
        }

        char* place = digits + 512;
        while (true)
        {
            const int prec = precision;
            precision -= 1;
            if (prec < 1 && lo == 0 && hi == 0)
            {
                break;
            }
            const unsigned long long value = (static_cast<unsigned long long>(hi) << 32) | lo;
            const unsigned long long quot = value / static_cast<unsigned>(radix);
            const unsigned rem = static_cast<unsigned>(value % static_cast<unsigned>(radix));
            lo = static_cast<u32>(quot);
            hi = static_cast<u32>(quot >> 32);
            int digit = static_cast<int>(rem) + 0x30;
            if (digit > 0x39)
            {
                digit += hex_shift;
            }
            *place = static_cast<char>(digit);
            place -= 1;
        }
        text_len = static_cast<int>((digits + 512) - place);
        text = place + 1;
        if ((flags & kOctal) != 0 && (text_len == 0 || text[0] != '0'))
        {
            *place = '0';
            text = place;
            text_len += 1;
        }
    };

    auto load_integer = [&]()
    {
        if ((flags & kI64) != 0)
        {
            unsigned long long bits = 0;
            if ((flags & kSigned) != 0)
            {
                bits = static_cast<unsigned long long>(va_arg(argptr, long long));
            }
            else
            {
                bits = va_arg(argptr, unsigned long long);
            }
            lo = static_cast<u32>(bits);
            hi = static_cast<u32>(bits >> 32);
            return;
        }
        if ((flags & kShort) != 0)
        {
            const int value = va_arg(argptr, int);
            if ((flags & kSigned) != 0)
            {
                const auto narrow = static_cast<std::int16_t>(value);
                lo = static_cast<u32>(static_cast<std::int32_t>(narrow));
                hi = narrow < 0 ? 0xffffffffu : 0u;
            }
            else
            {
                lo = static_cast<u32>(static_cast<std::uint16_t>(value));
                hi = 0;
            }
            return;
        }
        if ((flags & kSigned) != 0)
        {
            const int value = va_arg(argptr, int);
            lo = static_cast<u32>(value);
            hi = value < 0 ? 0xffffffffu : 0u;
            return;
        }
        lo = va_arg(argptr, unsigned int);
        hi = 0;
    };

    auto set_hex_prefix = [&]()
    {
        if ((flags & kAlt) == 0)
        {
            return;
        }
        prefix[0] = '0';
        prefix[1] = static_cast<char>(hex_shift + 'Q');
        prefix_len = 2;
    };

    auto emit = [&]()
    {
        if (no_output != 0)
        {
            return;
        }
        if ((flags & kSigned) != 0)
        {
            char sign = 0;
            if ((flags & kNeg) != 0)
            {
                sign = '-';
            }
            else if ((flags & kSign) != 0)
            {
                sign = '+';
            }
            else if ((flags & kBlank) != 0)
            {
                sign = ' ';
            }
            if (sign != 0)
            {
                prefix[0] = sign;
                prefix_len = 1;
            }
        }

        const int pad = width - prefix_len - text_len;
        if ((flags & (kLeft | kZero)) == 0)
        {
            write_multi(0x20, pad);
        }
        write_string(prefix, prefix_len);
        if ((flags & kZero) != 0 && (flags & kLeft) == 0)
        {
            write_multi(0x30, pad);
        }
        if (wide_chars == 0 || text_len < 1)
        {
            write_string(text, text_len);
        }
        else
        {
            const auto* units = reinterpret_cast<const char16_t*>(text);
            int left = text_len;
            while (left != 0)
            {
                left -= 1;
                const unsigned unit = static_cast<unsigned>(*units);
                units += 1;
                const int produced = wctomb_byte(mb, static_cast<int>(unit));
                if (produced < 1)
                {
                    break;
                }
                write_string(mb, produced);
            }
        }
        if ((flags & kLeft) != 0)
        {
            write_multi(0x20, pad);
        }
    };

    auto counted_length = [](const char* s) -> int
    {
        const char* start = s;
        while (*s != '\0')
        {
            s += 1;
        }
        return static_cast<int>(s - start);
    };

    if (format == nullptr || *format == '\0')
    {
        return charsout;
    }

    cursor = reinterpret_cast<const unsigned char*>(format);
    unsigned char ch = *cursor;
    for (;;)
    {
        cursor += 1;
        if (charsout < 0)
        {
            return charsout;
        }

        int cls = 0;
        if (ch >= 0x20 && ch <= 0x78)
        {
            cls = static_cast<signed char>(kLookup[ch - 0x20]) & 0x0f;
        }
        const int slot = cls * 8 + state;
        state = static_cast<signed char>(kLookup[slot]) >> 4;
        if (static_cast<unsigned>(state) <= 7u)
        {
            switch (state)
            {
            case 0:
                write_literal(ch);
                break;
            case 1:
                precision = -1;
                capexp = 0;
                no_output = 0;
                width = 0;
                prefix_len = 0;
                flags = 0;
                wide_chars = 0;
                break;
            case 2:
                if (ch == 0x20)
                {
                    flags |= kBlank;
                }
                else if (ch == '#')
                {
                    flags |= kAlt;
                }
                else if (ch == '+')
                {
                    flags |= kSign;
                }
                else if (ch == '-')
                {
                    flags |= kLeft;
                }
                else if (ch == '0')
                {
                    flags |= kZero;
                }
                break;
            case 3:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    if (value < 0)
                    {
                        flags |= kLeft;
                        width = static_cast<int>(0u - static_cast<unsigned>(value));
                    }
                    else
                    {
                        width = value;
                    }
                }
                else
                {
                    width = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(width) * 10u - 0x30u);
                }
                break;
            case 4:
                precision = 0;
                break;
            case 5:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    precision = value < 0 ? -1 : value;
                }
                else
                {
                    precision = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(precision) * 10u - 0x30u);
                }
                break;
            case 6:
                if (ch == 'I')
                {
                    const unsigned char n0 = *cursor;
                    unsigned char n1 = 0;
                    if (n0 != 0)
                    {
                        n1 = *(cursor + 1);
                    }
                    if (n0 == '6' && n1 == '4')
                    {
                        cursor += 2;
                        flags |= kI64;
                    }
                    else if (n0 == '3' && n1 == '2')
                    {
                        cursor += 2;
                        flags &= ~kI64;
                    }
                    else if (n0 != 'd' && n0 != 'i' && n0 != 'o' && n0 != 'u' && n0 != 'x' && n0 != 'X')
                    {
                        state = 0;
                        write_literal(ch);
                    }
                }
                else if (ch == 'h')
                {
                    flags |= kShort;
                }
                else if (ch == 'l')
                {
                    flags |= kLong;
                }
                else if (ch == 'w')
                {
                    flags |= kWide;
                }
                break;
            case 7:
                if (ch == 'E' || ch == 'G')
                {
                    capexp = 1;
                    ch = static_cast<unsigned char>(ch + 0x20);
                }
                if (ch == 'C' || ch == 'S')
                {
                    if ((flags & (kWide | kShort | kLong)) == 0)
                    {
                        flags |= kWide;
                    }
                }

                if (ch == 'd' || ch == 'i')
                {
                    flags |= kSigned;
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'u')
                {
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'o')
                {
                    radix = 8;
                    if ((flags & kAlt) != 0)
                    {
                        flags |= kOctal;
                    }
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'x' || ch == 'X' || ch == 'p')
                {
                    hex_shift = ch == 'x' ? 0x27 : 7;
                    radix = 16;
                    set_hex_prefix();
                    if (ch == 'p')
                    {
                        const auto bits = reinterpret_cast<std::uintptr_t>(va_arg(argptr, void*));
                        lo = static_cast<u32>(bits);
                        hi = static_cast<u32>(bits >> 32);
                        precision = 8;
                        write_number(true);
                    }
                    else
                    {
                        load_integer();
                        write_number((flags & kI64) != 0);
                    }
                    emit();
                }
                else if (ch == 'e' || ch == 'f' || ch == 'g')
                {
                    flags |= kSigned;
                    char* buffer = stack_buf;
                    int buf_size = 0x200;
                    if (precision < 0)
                    {
                        precision = 6;
                    }
                    else if (precision == 0)
                    {
                        if (ch == 'g')
                        {
                            precision = 1;
                        }
                    }
                    else
                    {
                        if (precision > 0x200)
                        {
                            precision = 0x200;
                        }
                        if (precision > 0xa3)
                        {
                            heap = static_cast<char*>(std::malloc(static_cast<std::size_t>(precision) + 0x15du));
                            if (heap != nullptr)
                            {
                                buffer = heap;
                                buf_size = precision + 0x15d;
                            }
                            else
                            {
                                precision = 0xa3;
                            }
                        }
                    }
                    text = buffer;
                    const double value = va_arg(argptr, double);
                    format_float(buffer, buf_size, static_cast<char>(ch), value);
                    if ((flags & kAlt) != 0 && precision == 0)
                    {
                        if (g_cfltcvt_tab[3] == kFptrap)
                        {
                            std::exit(2);
                        }
                        force_decimal(buffer);
                    }
                    if (ch == 'g' && (flags & kAlt) == 0)
                    {
                        if (g_cfltcvt_tab[1] == kFptrap)
                        {
                            std::exit(2);
                        }
                        crop_zeros(buffer);
                    }
                    if (buffer[0] == '-')
                    {
                        flags |= kNeg;
                        text = buffer + 1;
                    }
                    else
                    {
                        text = buffer;
                    }
                    text_len = counted_length(text);
                    emit();
                }
                else if (ch == 'c' || ch == 'C')
                {
                    if ((flags & (kWide | kLong)) != 0)
                    {
                        const int value = va_arg(argptr, int);
                        const int wchar = static_cast<std::int16_t>(value);
                        text_len = wctomb_byte(mb, wchar);
                        text = mb;
                        if (text_len < 0)
                        {
                            no_output = 1;
                        }
                    }
                    else
                    {
                        const int value = va_arg(argptr, int);
                        mb[0] = static_cast<char>(value);
                        text = mb;
                        text_len = 1;
                    }
                    emit();
                }
                else if (ch == 's' || ch == 'S')
                {
                    int limit = precision == -1 ? 0x7fffffff : precision;
                    if ((flags & (kWide | kLong)) != 0)
                    {
                        using WidePtr = char16_t*;
                        char16_t* units = va_arg(argptr, WidePtr);
                        if (units == nullptr)
                        {
                            units = const_cast<char16_t*>(kNullWide);
                        }
                        char16_t* start = units;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*units == 0)
                            {
                                break;
                            }
                            units += 1;
                        }
                        text = reinterpret_cast<const char*>(start);
                        text_len = static_cast<int>(units - start);
                        wide_chars = 1;
                    }
                    else
                    {
                        using CharPtr = char*;
                        char* str = va_arg(argptr, CharPtr);
                        if (str == nullptr)
                        {
                            str = const_cast<char*>(kNullText);
                        }
                        char* start = str;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*str == '\0')
                            {
                                break;
                            }
                            str += 1;
                        }
                        text = start;
                        text_len = static_cast<int>(str - start);
                    }
                    emit();
                }
                else if (ch == 'Z')
                {
                    const auto* block = static_cast<const char*>(va_arg(argptr, void*));
                    const char* buffer = nullptr;
                    if (block != nullptr)
                    {
                        std::memcpy(&buffer, block + 4, sizeof(buffer));
                    }
                    if (block == nullptr || buffer == nullptr)
                    {
                        text = kNullText;
                        text_len = counted_length(text);
                    }
                    else
                    {
                        std::int16_t raw = 0;
                        std::memcpy(&raw, block, sizeof(raw));
                        text = buffer;
                        text_len = raw;
                        if ((flags & kWide) != 0)
                        {
                            text_len /= 2;
                            wide_chars = 1;
                        }
                        else
                        {
                            wide_chars = 0;
                        }
                    }
                    emit();
                }
                else if (ch == 'n')
                {
                    void* dest = va_arg(argptr, void*);
                    if ((flags & kShort) != 0)
                    {
                        *static_cast<std::uint16_t*>(dest) = static_cast<std::uint16_t>(charsout);
                    }
                    else
                    {
                        *static_cast<int*>(dest) = charsout;
                    }
                    no_output = 1;
                }
                else
                {
                    emit();
                }
                if (heap != nullptr)
                {
                    fn_167f2e(heap);
                    heap = nullptr;
                }
                break;
            default:
                break;
            }
        }

        ch = *cursor;
        if (ch == 0)
        {
            break;
        }
    }
    return charsout;
}

int fn_16993d(void* stream, const char16_t* format, std::va_list argptr)
{
    // Xbox 0x0016993D
    // _woutput. The wide printf family builds a FILE and calls here.
    // 0x00166A03 and 0x00166FB1 are the swprintf / vswprintf bodies: flag
    // 0x42 (_IOWRT|_IOSTRG), cnt 0x7FFFFFFF, ptr and base on the caller's
    // buffer. 0x001671A5 is _snwprintf: cnt is the character count times
    // two, because each unit occupies two bytes. The return is the number
    // of wide characters accepted, or -1 after a failed write. A null or
    // empty format returns 0. Retail faults on a null format; the port
    // returns 0.
    //
    // The walk is the same table as _output (0x00168FED), at 0x00209280.
    // A unit from 0x20 through 0x78 takes its class from the low nibble of
    // the byte at 0x00209260 plus the unit. Every other unit is class 0.
    // The next state is the high nibble of the byte at class * 8 + state.
    // States are literal, conversion, flag, width, dot, precision, size,
    // and type. The jump table is at 0x0016A0E1.
    //
    // Flag bits match _output: '+' 0x1, ' ' 0x2, '-' 0x4, '0' 0x8, 'l' 0x10,
    // 'h' 0x20, signed 0x40, '#' 0x80, negative 0x100, forced octal 0x200,
    // 'w' 0x800, and 'I64' 0x8000. %C and %S set the short bit when no size
    // is set yet, so they are the narrow forms. %c and %s stay wide. %hc
    // and %hs are narrow. %I64 and %I32 consume the two digits. %I
    // immediately before d, i, o, u, x, or X is ignored. Any other unit
    // after %I is written as a literal and the state returns to literal.
    //
    // Literals go through write_char (0x001698B6). write_multi_char
    // (0x001698DB) pads. write_string (0x00169900) copies wide units. A
    // stream whose flag has 0x40 and whose base is null only advances the
    // count. Any other string stream calls _fputwc (0x0016DFB5), which
    // subtracts 2 from cnt and stores the unit. A negative cnt sets flag
    // bit 0x20 and returns 0xFFFF, and the count becomes -1. A stream that
    // is not a string takes the text-mode path: the default ioinfo at
    // 0x00338250 has FTEXT set, so the unit is expanded with wctomb
    // (0x0016DF8A) and each byte is stored with cnt dropping by 1. A real
    // file flush is _flsbuf (0x00168E46) or _flswbuf (0x0016F2F1). The port
    // reports that same failure when the buffer is full.
    //
    // Narrow text (integers, floats, %hs, %hc, a null %Z) is expanded with
    // mbtowc (0x0016E07A). With __mb_cur_max at 1 that turns one byte into
    // one unit and returns 1, or 0 for a zero byte. %Z reads a counted
    // record: the word at +0 is the byte length, and the pointer at +4 is
    // the buffer. A null record prints "(null)" from 0x002092DC. The wide
    // null string is the UTF-16 text at 0x002092E4. The 'w' flag halves the
    // counted length and leaves those units wide.
    //
    // Floats take a double and call the slots _fpmath fills at 0x003381A0
    // (__cfltcvt), 0x003381AC (__forcdecpt), and 0x003381A4 (__cropzeros).
    // While a slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. Precision above 0xA3 mallocs (0x00167F1C) precision + 0x15D
    // bytes and this function frees that block (0x00167F2E).
    //
    // The host va_list is the Xbox argument cursor. Each promoted integer
    // or pointer is one argument. A double or an I64 value is one 8-byte
    // argument. %p prints a host pointer in uppercase hex, at least eight
    // digits. On Xbox the pointer is 32 bits, so eight digits are the whole
    // value. %n stores the wide count.

    struct OutputFile
    {
        char* ptr;
        int cnt;
        char* base;
        int flag;
        int file;
        int charbuf;
        int bufsiz;
        char* tmpfname;
    };

    static constexpr unsigned char kLookup[] = {
        0x06, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x03, 0x06, 0x00, 0x06, 0x02, 0x10,
        0x04, 0x45, 0x45, 0x45, 0x05, 0x05, 0x05, 0x05, 0x05, 0x35, 0x30, 0x00, 0x50, 0x00, 0x00, 0x00,
        0x00, 0x20, 0x28, 0x38, 0x50, 0x58, 0x07, 0x08, 0x00, 0x37, 0x30, 0x30, 0x57, 0x50, 0x07, 0x00,
        0x00, 0x20, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x60, 0x68, 0x60, 0x60, 0x60, 0x60, 0x00,
        0x00, 0x70, 0x70, 0x78, 0x78, 0x78, 0x78, 0x08, 0x07, 0x08, 0x00, 0x00, 0x07, 0x00, 0x08, 0x08,
        0x08, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x08,
    };
    static_assert(sizeof(kLookup) == 89);

    constexpr int kSign = 0x1;
    constexpr int kBlank = 0x2;
    constexpr int kLeft = 0x4;
    constexpr int kZero = 0x8;
    constexpr int kLong = 0x10;
    constexpr int kShort = 0x20;
    constexpr int kSigned = 0x40;
    constexpr int kAlt = 0x80;
    constexpr int kNeg = 0x100;
    constexpr int kOctal = 0x200;
    constexpr int kWide = 0x800;
    constexpr int kI64 = 0x8000;
    constexpr u32 kFptrap = 0x0016F08Cu;

    static constexpr char kNullText[] = "(null)";
    static constexpr char16_t kNullWide[] = {'(', 'n', 'u', 'l', 'l', ')', 0};

    auto* out = static_cast<OutputFile*>(stream);
    int charsout = 0;
    int state = 0;
    int flags = 0;
    int precision = -1;
    int width = 0;
    int prefix_len = 0;
    int text_len = 0;
    int no_output = 0;
    int wide_text = 0;
    int capexp = 0;
    int hex_shift = 0;
    int radix = 10;
    char16_t prefix[2] = {};
    const char* narrow = "";
    const char16_t* wide = kNullWide;
    char* heap = nullptr;
    char stack_buf[0x200];
    char digits[513];
    char16_t wchar_buf[2] = {};
    u32 lo = 0;
    u32 hi = 0;

    auto wctomb_byte = [&](char* dest, int wchar) -> int
    {
        if (g_crt_mb_cur_max <= 1)
        {
            if (wchar < 0 || wchar > 0xff)
            {
                errno = 42;
                return -1;
            }
            dest[0] = static_cast<char>(wchar);
            return 1;
        }
        errno = 42;
        return -1;
    };

    auto write_wchar = [&](unsigned unit)
    {
        const auto code = static_cast<std::uint16_t>(unit);
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += 1;
            return;
        }

        if ((fn_16dfb5(code, out) & 0xffffu) == 0xffffu)
        {
            charsout = -1;
            return;
        }
        charsout += 1;
    };

    auto write_multi = [&](unsigned unit, int count)
    {
        while (count > 0)
        {
            count -= 1;
            write_wchar(unit);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto write_wchars = [&](const char16_t* units, int count)
    {
        if ((out->flag & 0x40) != 0 && out->base == nullptr)
        {
            charsout += count;
            return;
        }
        while (count > 0)
        {
            count -= 1;
            const unsigned unit = static_cast<unsigned>(*units);
            units += 1;
            write_wchar(unit);
            if (charsout == -1)
            {
                break;
            }
        }
    };

    auto force_decimal = [](char* s)
    {
        char* p = s;
        while (*p != '\0')
        {
            if (*p == '.')
            {
                return;
            }
            if (*p == 'e' || *p == 'E')
            {
                break;
            }
            p += 1;
        }
        char* end = p;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= p)
        {
            *(end + 1) = *end;
            end -= 1;
        }
        *p = '.';
    };

    auto crop_zeros = [](char* s)
    {
        char* p = s;
        while (*p != '\0' && *p != '.')
        {
            p += 1;
        }
        if (*p == '\0')
        {
            return;
        }
        while (*p != '\0' && *p != 'e' && *p != 'E')
        {
            p += 1;
        }
        char* stop = p;
        if (p > s)
        {
            p -= 1;
        }
        while (p > s && *p == '0')
        {
            p -= 1;
        }
        if (*p == '.' && p > s)
        {
            p -= 1;
        }
        p += 1;
        while (*stop != '\0')
        {
            *p = *stop;
            p += 1;
            stop += 1;
        }
        *p = '\0';
    };

    auto pad_exponent = [](char* s, int buf_size)
    {
        char* mark = std::strchr(s, 'e');
        if (mark == nullptr)
        {
            mark = std::strchr(s, 'E');
        }
        if (mark == nullptr || (mark[1] != '+' && mark[1] != '-'))
        {
            return;
        }
        char* digits_at = mark + 2;
        int count = 0;
        while (digits_at[count] >= '0' && digits_at[count] <= '9')
        {
            count += 1;
        }
        if (count == 0 || count >= 3)
        {
            return;
        }
        const int need = 3 - count;
        const int used = static_cast<int>(std::strlen(s));
        if (used + need + 1 > buf_size)
        {
            return;
        }
        char* end = digits_at + count;
        while (*end != '\0')
        {
            end += 1;
        }
        while (end >= digits_at)
        {
            *(end + need) = *end;
            end -= 1;
        }
        for (int i = 0; i < need; i += 1)
        {
            digits_at[i] = '0';
        }
    };

    auto format_float = [&](char* buffer, int buf_size, char fmt, double value)
    {
        if (g_cfltcvt_tab[0] == kFptrap)
        {
            std::exit(2);
        }
        if (!std::isfinite(value))
        {
            const char* token = "1.#QNAN";
            if (std::isnan(value))
            {
                token = std::signbit(value) ? "-1.#IND" : "1.#QNAN";
            }
            else
            {
                token = std::signbit(value) ? "-1.#INF" : "1.#INF";
            }
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%s", token);
            return;
        }

        const bool upper = capexp != 0;
        if (fmt == 'e')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision, value);
        }
        else if (fmt == 'f')
        {
            std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", precision, value);
        }
        else
        {
            bool scientific = false;
            int exponent = 0;
            if (value != 0.0)
            {
                char probe[64];
                std::snprintf(probe, sizeof(probe), "%.*e", precision - 1, value);
                const char* mark = std::strchr(probe, 'e');
                if (mark == nullptr)
                {
                    mark = std::strchr(probe, 'E');
                }
                if (mark != nullptr)
                {
                    exponent = std::atoi(mark + 1);
                }
                scientific = exponent < -4 || exponent >= precision;
            }
            if (scientific)
            {
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), upper ? "%.*E" : "%.*e", precision - 1, value);
            }
            else
            {
                int frac = precision - exponent - 1;
                if (frac < 0)
                {
                    frac = 0;
                }
                std::snprintf(buffer, static_cast<std::size_t>(buf_size), "%.*f", frac, value);
            }
        }
        pad_exponent(buffer, buf_size);
    };

    auto write_number = [&](bool as_i64)
    {
        if ((flags & kSigned) != 0 && static_cast<std::int32_t>(hi) < 0)
        {
            unsigned long long mag = (static_cast<unsigned long long>(hi) << 32) | lo;
            mag = 0ull - mag;
            lo = static_cast<u32>(mag);
            hi = static_cast<u32>(mag >> 32);
            flags |= kNeg;
        }
        if (!as_i64)
        {
            hi = 0;
        }
        if (precision < 0)
        {
            precision = 1;
        }
        else
        {
            flags &= ~kZero;
            if (precision > 0x200)
            {
                precision = 0x200;
            }
        }
        if (lo == 0 && hi == 0)
        {
            prefix_len = 0;
        }

        char* place = digits + 512;
        while (true)
        {
            const int prec = precision;
            precision -= 1;
            if (prec < 1 && lo == 0 && hi == 0)
            {
                break;
            }
            const unsigned long long value = (static_cast<unsigned long long>(hi) << 32) | lo;
            const unsigned long long quot = value / static_cast<unsigned>(radix);
            const unsigned rem = static_cast<unsigned>(value % static_cast<unsigned>(radix));
            lo = static_cast<u32>(quot);
            hi = static_cast<u32>(quot >> 32);
            int digit = static_cast<int>(rem) + 0x30;
            if (digit > 0x39)
            {
                digit += hex_shift;
            }
            *place = static_cast<char>(digit);
            place -= 1;
        }
        text_len = static_cast<int>((digits + 512) - place);
        narrow = place + 1;
        wide_text = 0;
        if ((flags & kOctal) != 0 && (text_len == 0 || narrow[0] != '0'))
        {
            *place = '0';
            narrow = place;
            text_len += 1;
        }
    };

    auto load_integer = [&]()
    {
        if ((flags & kI64) != 0)
        {
            unsigned long long bits = 0;
            if ((flags & kSigned) != 0)
            {
                bits = static_cast<unsigned long long>(va_arg(argptr, long long));
            }
            else
            {
                bits = va_arg(argptr, unsigned long long);
            }
            lo = static_cast<u32>(bits);
            hi = static_cast<u32>(bits >> 32);
            return;
        }
        if ((flags & kShort) != 0)
        {
            const int value = va_arg(argptr, int);
            if ((flags & kSigned) != 0)
            {
                const auto narrow_bits = static_cast<std::int16_t>(value);
                lo = static_cast<u32>(static_cast<std::int32_t>(narrow_bits));
                hi = narrow_bits < 0 ? 0xffffffffu : 0u;
            }
            else
            {
                lo = static_cast<u32>(static_cast<std::uint16_t>(value));
                hi = 0;
            }
            return;
        }
        if ((flags & kSigned) != 0)
        {
            const int value = va_arg(argptr, int);
            lo = static_cast<u32>(value);
            hi = value < 0 ? 0xffffffffu : 0u;
            return;
        }
        lo = va_arg(argptr, unsigned int);
        hi = 0;
    };

    auto set_hex_prefix = [&]()
    {
        if ((flags & kAlt) == 0)
        {
            return;
        }
        prefix[0] = static_cast<char16_t>(0x30);
        prefix[1] = static_cast<char16_t>(hex_shift + 0x51);
        prefix_len = 2;
    };

    auto emit = [&]()
    {
        if (no_output != 0)
        {
            return;
        }
        if ((flags & kSigned) != 0)
        {
            char16_t sign = 0;
            if ((flags & kNeg) != 0)
            {
                sign = static_cast<char16_t>(0x2d);
            }
            else if ((flags & kSign) != 0)
            {
                sign = static_cast<char16_t>(0x2b);
            }
            else if ((flags & kBlank) != 0)
            {
                sign = static_cast<char16_t>(0x20);
            }
            if (sign != 0)
            {
                prefix[0] = sign;
                prefix_len = 1;
            }
        }

        const int pad = width - prefix_len - text_len;
        if ((flags & (kLeft | kZero)) == 0)
        {
            write_multi(0x20, pad);
        }
        write_wchars(prefix, prefix_len);
        if ((flags & kZero) != 0 && (flags & kLeft) == 0)
        {
            write_multi(0x30, pad);
        }
        if (wide_text == 0 && text_len > 0)
        {
            const char* bytes = narrow;
            int left = text_len;
            while (left > 0)
            {
                left -= 1;
                char16_t unit = 0;
                const int produced = fn_16e07a(&unit, bytes, g_crt_mb_cur_max);
                if (produced < 1)
                {
                    break;
                }
                write_wchar(static_cast<unsigned>(unit));
                bytes += produced;
                if (charsout == -1)
                {
                    break;
                }
            }
        }
        else
        {
            write_wchars(wide, text_len);
        }
        if ((flags & kLeft) != 0)
        {
            write_multi(0x20, pad);
        }
    };

    auto counted_length = [](const char* s) -> int
    {
        const char* start = s;
        while (*s != '\0')
        {
            s += 1;
        }
        return static_cast<int>(s - start);
    };

    if (format == nullptr || *format == 0)
    {
        return charsout;
    }

    char16_t ch = *format;
    for (;;)
    {
        format += 1;
        if (charsout < 0)
        {
            return charsout;
        }

        int cls = 0;
        if (ch >= 0x20 && ch <= 0x78)
        {
            cls = static_cast<signed char>(kLookup[ch - 0x20]) & 0x0f;
        }
        const int slot = cls * 8 + state;
        state = static_cast<signed char>(kLookup[slot]) >> 4;
        if (static_cast<unsigned>(state) <= 7u)
        {
            switch (state)
            {
            case 0:
                wide_text = 1;
                write_wchar(static_cast<unsigned>(ch));
                break;
            case 1:
                precision = -1;
                capexp = 0;
                no_output = 0;
                width = 0;
                prefix_len = 0;
                flags = 0;
                wide_text = 0;
                break;
            case 2:
                if (ch == 0x20)
                {
                    flags |= kBlank;
                }
                else if (ch == '#')
                {
                    flags |= kAlt;
                }
                else if (ch == '+')
                {
                    flags |= kSign;
                }
                else if (ch == '-')
                {
                    flags |= kLeft;
                }
                else if (ch == '0')
                {
                    flags |= kZero;
                }
                break;
            case 3:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    if (value < 0)
                    {
                        flags |= kLeft;
                        width = static_cast<int>(0u - static_cast<unsigned>(value));
                    }
                    else
                    {
                        width = value;
                    }
                }
                else
                {
                    width = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(width) * 10u - 0x30u);
                }
                break;
            case 4:
                precision = 0;
                break;
            case 5:
                if (ch == '*')
                {
                    const int value = va_arg(argptr, int);
                    precision = value < 0 ? -1 : value;
                }
                else
                {
                    precision = static_cast<int>(static_cast<unsigned>(ch) + static_cast<unsigned>(precision) * 10u - 0x30u);
                }
                break;
            case 6:
                if (ch == 'I')
                {
                    const char16_t n0 = *format;
                    char16_t n1 = 0;
                    if (n0 != 0)
                    {
                        n1 = format[1];
                    }
                    if (n0 == '6' && n1 == '4')
                    {
                        format += 2;
                        flags |= kI64;
                    }
                    else if (n0 == '3' && n1 == '2')
                    {
                        format += 2;
                        flags &= ~kI64;
                    }
                    else if (n0 != 'd' && n0 != 'i' && n0 != 'o' && n0 != 'u' && n0 != 'x' && n0 != 'X')
                    {
                        state = 0;
                        wide_text = 1;
                        write_wchar(static_cast<unsigned>(ch));
                    }
                }
                else if (ch == 'h')
                {
                    flags |= kShort;
                }
                else if (ch == 'l')
                {
                    flags |= kLong;
                }
                else if (ch == 'w')
                {
                    flags |= kWide;
                }
                break;
            case 7:
                if (ch == 'E' || ch == 'G')
                {
                    capexp = 1;
                    ch = static_cast<char16_t>(ch + 0x20);
                }
                if (ch == 'C' || ch == 'S')
                {
                    if ((flags & (kWide | kShort | kLong)) == 0)
                    {
                        flags |= kShort;
                    }
                }

                if (ch == 'd' || ch == 'i')
                {
                    flags |= kSigned;
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'u')
                {
                    radix = 10;
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'o')
                {
                    radix = 8;
                    if ((flags & kAlt) != 0)
                    {
                        flags |= kOctal;
                    }
                    load_integer();
                    write_number((flags & kI64) != 0);
                    emit();
                }
                else if (ch == 'x' || ch == 'X' || ch == 'p')
                {
                    hex_shift = ch == 'x' ? 0x27 : 7;
                    radix = 16;
                    set_hex_prefix();
                    if (ch == 'p')
                    {
                        const auto bits = reinterpret_cast<std::uintptr_t>(va_arg(argptr, void*));
                        lo = static_cast<u32>(bits);
                        hi = static_cast<u32>(bits >> 32);
                        precision = 8;
                        write_number(true);
                    }
                    else
                    {
                        load_integer();
                        write_number((flags & kI64) != 0);
                    }
                    emit();
                }
                else if (ch == 'e' || ch == 'f' || ch == 'g')
                {
                    flags |= kSigned;
                    char* buffer = stack_buf;
                    int buf_size = 0x200;
                    if (precision < 0)
                    {
                        precision = 6;
                    }
                    else if (precision == 0)
                    {
                        if (ch == 'g')
                        {
                            precision = 1;
                        }
                    }
                    else
                    {
                        if (precision > 0x200)
                        {
                            precision = 0x200;
                        }
                        if (precision > 0xa3)
                        {
                            heap = static_cast<char*>(std::malloc(static_cast<std::size_t>(precision) + 0x15du));
                            if (heap != nullptr)
                            {
                                buffer = heap;
                                buf_size = precision + 0x15d;
                            }
                            else
                            {
                                precision = 0xa3;
                            }
                        }
                    }
                    narrow = buffer;
                    wide_text = 0;
                    const double value = va_arg(argptr, double);
                    format_float(buffer, buf_size, static_cast<char>(ch), value);
                    if ((flags & kAlt) != 0 && precision == 0)
                    {
                        if (g_cfltcvt_tab[3] == kFptrap)
                        {
                            std::exit(2);
                        }
                        force_decimal(buffer);
                    }
                    if (ch == 'g' && (flags & kAlt) == 0)
                    {
                        if (g_cfltcvt_tab[1] == kFptrap)
                        {
                            std::exit(2);
                        }
                        crop_zeros(buffer);
                    }
                    if (buffer[0] == '-')
                    {
                        flags |= kNeg;
                        narrow = buffer + 1;
                    }
                    else
                    {
                        narrow = buffer;
                    }
                    text_len = counted_length(narrow);
                    emit();
                }
                else if (ch == 'c' || ch == 'C')
                {
                    const int value = va_arg(argptr, int);
                    const unsigned unit = static_cast<unsigned>(value) & 0xffffu;
                    wide_text = 1;
                    wide = wchar_buf;
                    text_len = 1;
                    if ((flags & kShort) == 0)
                    {
                        wchar_buf[0] = static_cast<char16_t>(unit);
                    }
                    else
                    {
                        char bytes[2];
                        bytes[0] = static_cast<char>(unit);
                        bytes[1] = 0;
                        const int produced = fn_16e07a(wchar_buf, bytes, g_crt_mb_cur_max);
                        if (produced < 0)
                        {
                            no_output = 1;
                        }
                    }
                    emit();
                }
                else if (ch == 's' || ch == 'S')
                {
                    int limit = precision == -1 ? 0x7fffffff : precision;
                    if ((flags & kShort) == 0)
                    {
                        char16_t* units = va_arg(argptr, char16_t*);
                        if (units == nullptr)
                        {
                            units = const_cast<char16_t*>(kNullWide);
                        }
                        char16_t* start = units;
                        while (limit != 0)
                        {
                            limit -= 1;
                            if (*units == 0)
                            {
                                break;
                            }
                            units += 1;
                        }
                        wide = start;
                        text_len = static_cast<int>(units - start);
                        wide_text = 1;
                    }
                    else
                    {
                        char* str = va_arg(argptr, char*);
                        if (str == nullptr)
                        {
                            str = const_cast<char*>(kNullText);
                        }
                        narrow = str;
                        wide_text = 0;
                        text_len = 0;
                        if (limit > 0)
                        {
                            const unsigned char* cursor = reinterpret_cast<const unsigned char*>(str);
                            while (text_len < limit)
                            {
                                const unsigned char byte = *cursor;
                                if (byte == 0)
                                {
                                    break;
                                }
                                if ((g_crt_pctype[byte] & 0x8000u) != 0)
                                {
                                    cursor += 1;
                                }
                                cursor += 1;
                                text_len += 1;
                            }
                        }
                    }
                    emit();
                }
                else if (ch == 'Z')
                {
                    const auto* block = static_cast<const char*>(va_arg(argptr, void*));
                    const char* buffer = nullptr;
                    if (block != nullptr)
                    {
                        std::memcpy(&buffer, block + 4, sizeof(buffer));
                    }
                    if (block == nullptr || buffer == nullptr)
                    {
                        narrow = kNullText;
                        text_len = counted_length(narrow);
                        wide_text = 0;
                    }
                    else
                    {
                        std::int16_t raw = 0;
                        std::memcpy(&raw, block, sizeof(raw));
                        text_len = raw;
                        if ((flags & kWide) != 0)
                        {
                            text_len /= 2;
                            wide = reinterpret_cast<const char16_t*>(buffer);
                            wide_text = 1;
                        }
                        else
                        {
                            narrow = buffer;
                            wide_text = 0;
                        }
                    }
                    emit();
                }
                else if (ch == 'n')
                {
                    void* dest = va_arg(argptr, void*);
                    if ((flags & kShort) != 0)
                    {
                        *static_cast<std::uint16_t*>(dest) = static_cast<std::uint16_t>(charsout);
                    }
                    else
                    {
                        *static_cast<int*>(dest) = charsout;
                    }
                    no_output = 1;
                }
                else
                {
                    emit();
                }
                if (heap != nullptr)
                {
                    fn_167f2e(heap);
                    heap = nullptr;
                }
                break;
            default:
                break;
            }
        }

        ch = *format;
        if (ch == 0)
        {
            break;
        }
    }
    return charsout;
}

int fn_16a17c(void* stream, const char16_t* format, std::va_list argptr)
{
    // Xbox 0x0016A17C
    // _winput. swscanf (0x00166A72) builds a string FILE and calls here:
    // ptr and base point at the wchar buffer, cnt is wcslen (0x00166ECB)
    // times two, flag is 0x49 (_IOREAD|_IOMYBUF|_IOSTRG). The return is the
    // number of assigned fields, or -1 when the stream is at EOF and no
    // conversion and no suppressed field has matched. %n stores the wide
    // count and does not add to that total.
    //
    // The host FILE matches _woutput: ptr, cnt, base, flag, then the handle
    // fields. A string stream (flag 0x40) reads a wchar and subtracts 2 from
    // cnt. A negative cnt is WEOF; retail would call _filwbuf (0x0016F419).
    // Pushback is _ungetwc_nolock (0x0016E1A5) through the helper at
    // 0x0016A14E, which ignores WEOF. On a string stream the previous wchar
    // has to equal the pushed character. A byte stream with room reads one
    // byte. __mb_cur_max is 1 and the C ctype table has no lead bytes, so
    // that byte is the wchar. A full byte buffer returns WEOF instead of
    // calling _filbuf (0x00168A14).
    //
    // Whitespace in the format skips wchar whitespace via iswctype
    // (0x00166BF9) and _pwctype at 0x0020998A. Bytes at or above 0x100, and
    // WEOF, are not whitespace. 0x09..0x0D, 0x20, and 0xA0 are. %s stops on
    // 0x09..0x0D and 0x20 only. Digits and hex use _pctype (0x00338180), the
    // low byte when __mb_cur_max is at most 1, otherwise the whole word.
    //
    // %I64 selects the 64-bit store and %I32 is a 32-bit no-op. %I immediately
    // before d, i, o, x, or X is ignored. A bare %I is %i. %n with I64 stores
    // the zeroed 64-bit accumulator. Other integers store a short when the
    // size count is 0 ('h') and an int otherwise. %p forces that count back
    // to 1 and reads hex without a 0x prefix. %i and %x accept 0x. A leading
    // 0 on %i selects octal. %c, %s, and %[ write wchars when the size count
    // is positive. %C and %S with no size are bytes, expanded with wctomb
    // (0x0016DF8A): one byte, or -1 and errno EILSEQ (42) above 0xFF.
    //
    // Floats (%e, %f, %g and the capitals) build a wchar image and call
    // __fassign, the slot _fpmath writes at 0x003381A8 (g_cfltcvt_tab[2]).
    // While that slot is still _fptrap (0x0016F08C) the process exits with
    // code 2. A size count of 1 ('f', the default) stores a float. Any other
    // count stores a double. The decimal point is the C-locale '.' from
    // mbtowc (0x0016E07A) of the byte at 0x0033818C. A scanset bitmap is
    // 0x2000 bytes. Retail alloca-probes that (0x00166F70) and mallocs
    // (0x00167F1C) if the probe raises; this frees the block on the way out.
    // A ']' missing before the format terminator ends the scan.

    struct ScanFile
    {
        char* ptr;
        int cnt;
        char* base;
        int flag;
        int file;
        int charbuf;
        int bufsiz;
        char* tmpfname;
    };

    auto* file = static_cast<ScanFile*>(stream);
    const char16_t* fmt = format;
    int charcount = 0;
    int assigned = 0;
    unsigned char matched = 0;
    unsigned ch = 0;
    unsigned char* scan = nullptr;

    auto is_wspace = [&](unsigned unit) -> bool
    {
        return fn_166bf9(static_cast<u16>(unit), 8u) != 0;
    };

    auto narrow_class = [&](unsigned unit, unsigned mask) -> bool
    {
        if ((unit & 0xff00u) != 0)
        {
            return false;
        }
        const unsigned byte = unit & 0xffu;
        if (g_crt_mb_cur_max > 1)
        {
            return fn_16ae1f(static_cast<int>(byte), mask) != 0;
        }
        return (static_cast<unsigned>(static_cast<u8>(g_crt_pctype[byte])) & mask) != 0;
    };

    auto hex_adjust = [&](unsigned unit) -> unsigned
    {
        if ((unit & 0xff00u) == 0 && narrow_class(unit, 4u))
        {
            return unit;
        }
        if (fn_166bf9(static_cast<u16>(unit), 2u) != 0)
        {
            return unit - 0x27u;
        }
        return unit - 7u;
    };

    auto get_unit = [&]() -> unsigned
    {
        if ((file->flag & 0x40) != 0)
        {
            file->cnt -= 2;
            if (file->cnt < 0)
            {
                return 0xffffu;
            }
            const auto unit = *reinterpret_cast<char16_t*>(file->ptr);
            file->ptr += 2;
            return unit;
        }
        file->cnt -= 1;
        if (file->cnt < 0)
        {
            return 0xffffu;
        }
        const auto byte = static_cast<unsigned char>(*file->ptr);
        file->ptr += 1;
        return byte;
    };

    auto unget_unit = [&](unsigned unit)
    {
        if ((unit & 0xffffu) == 0xffffu)
        {
            return;
        }
        const int flags = file->flag;
        if ((flags & 1) == 0 && ((flags & 0x80) == 0 || (flags & 2) != 0))
        {
            return;
        }
        if (file->base == nullptr)
        {
            return;
        }
        if ((flags & 0x40) != 0)
        {
            if (file->ptr < file->base + 2)
            {
                if (file->cnt != 0 || file->bufsiz < 2)
                {
                    return;
                }
                file->ptr = file->base + 2;
            }
            file->ptr -= 2;
            if (*reinterpret_cast<char16_t*>(file->ptr) != static_cast<char16_t>(unit))
            {
                file->ptr += 2;
                return;
            }
            file->cnt += 2;
            file->flag = (file->flag & ~0x10) | 1;
            return;
        }
        if (file->ptr <= file->base)
        {
            return;
        }
        file->ptr -= 1;
        *file->ptr = static_cast<char>(unit);
        file->cnt += 1;
        file->flag = (file->flag & ~0x10) | 1;
    };

    auto skip_white = [&]() -> unsigned
    {
        unsigned next = 0;
        do
        {
            charcount += 1;
            next = get_unit();
        } while (is_wspace(next));
        return next;
    };

    auto wctomb_byte = [&](char* dest, unsigned unit) -> int
    {
        if (dest == nullptr)
        {
            return 0;
        }
        if (unit > 0xffu)
        {
            errno = 42;
            return -1;
        }
        *dest = static_cast<char>(unit);
        return 1;
    };

    auto assign_float = [&](int kind, void* dest, const char16_t* text)
    {
        if (g_cfltcvt_tab[2] == 0x0016F08Cu)
        {
            std::exit(2);
        }
        char narrow[360];
        int length = 0;
        while (text[length] != 0 && length < 358)
        {
            narrow[length] = static_cast<char>(text[length] & 0xff);
            length += 1;
        }
        narrow[length] = 0;
        if (kind == 0)
        {
            float value = 0.0f;
            std::from_chars(narrow, narrow + length, value);
            *static_cast<float*>(dest) = value;
        }
        else
        {
            double value = 0.0;
            std::from_chars(narrow, narrow + length, value);
            *static_cast<double*>(dest) = value;
        }
    };

    std::va_list args;
    va_copy(args, argptr);

    auto finish_field = [&]() -> bool
    {
        matched = static_cast<unsigned char>(matched + 1);
        fmt += 1;
        if ((ch & 0xffffu) == 0xffffu && (*fmt != '%' || fmt[1] != 'n'))
        {
            return false;
        }
        return true;
    };

    while (*fmt != 0)
    {
        if (is_wspace(*fmt))
        {
            charcount -= 1;
            ch = skip_white();
            unget_unit(ch);
            do
            {
                fmt += 1;
            } while (is_wspace(*fmt));
            continue;
        }

        if (*fmt != '%')
        {
            charcount += 1;
            ch = get_unit();
            const char16_t literal = *fmt;
            fmt += 1;
            if (literal != static_cast<char16_t>(ch))
            {
                unget_unit(ch);
                break;
            }
            if ((ch & 0xffffu) == 0xffffu && (*fmt != '%' || fmt[1] != 'n'))
            {
                break;
            }
            continue;
        }

        int width = 0;
        int widthset = 0;
        signed char suppress = 0;
        signed char size_count = 1;
        signed char wide_count = 0;
        int i64 = 0;
        bool spec_done = false;
        while (!spec_done)
        {
            fmt += 1;
            const unsigned unit = *fmt;
            if ((unit & 0xff00u) == 0 && narrow_class(unit, 4u))
            {
                widthset += 1;
                width = static_cast<int>(unit - 0x30u) + width * 10;
                continue;
            }
            if (unit == 'N' || unit == 'F')
            {
                continue;
            }
            if (unit == '*')
            {
                suppress = static_cast<signed char>(suppress + 1);
                continue;
            }
            if (unit == 'I')
            {
                const char16_t next = fmt[1];
                if (next == '6' && fmt[2] == '4')
                {
                    i64 += 1;
                    fmt += 2;
                    continue;
                }
                if (next == '3' && fmt[2] == '2')
                {
                    fmt += 2;
                    continue;
                }
                if (next == 'd' || next == 'i' || next == 'o' || next == 'x' || next == 'X')
                {
                    continue;
                }
                spec_done = true;
                continue;
            }
            if (unit == 'L')
            {
                size_count = static_cast<signed char>(size_count + 1);
                continue;
            }
            if (unit == 'h')
            {
                size_count = static_cast<signed char>(size_count - 1);
                wide_count = static_cast<signed char>(wide_count - 1);
                continue;
            }
            if (unit == 'l')
            {
                size_count = static_cast<signed char>(size_count + 1);
                wide_count = static_cast<signed char>(wide_count + 1);
                continue;
            }
            if (unit == 'w')
            {
                wide_count = static_cast<signed char>(wide_count + 1);
                continue;
            }
            spec_done = true;
        }

        if (wide_count == 0)
        {
            if (*fmt == 'S' || *fmt == 'C')
            {
                wide_count = -1;
            }
            else
            {
                wide_count = 1;
            }
        }

        const unsigned spec = static_cast<unsigned>(*fmt) | 0x20u;
        if (spec != 'n')
        {
            if (spec == 'c' || spec == '{')
            {
                charcount += 1;
                ch = get_unit();
            }
            else
            {
                ch = skip_white();
            }
        }
        if (widthset != 0 && width == 0)
        {
            unget_unit(ch);
            break;
        }

        const bool is_float = spec == 'e' || spec == 'f' || spec == 'g';
        const bool is_int = spec == 'd' || spec == 'i' || spec == 'o' || spec == 'u' || spec == 'x' || spec == 'p';
        if (spec == 'n')
        {
            if (suppress == 0)
            {
                void* dest = va_arg(args, void*);
                if (i64 != 0)
                {
                    *reinterpret_cast<std::uint64_t*>(dest) = 0;
                }
                else if (size_count != 0)
                {
                    *reinterpret_cast<int*>(dest) = charcount;
                }
                else
                {
                    *reinterpret_cast<u16*>(dest) = static_cast<u16>(charcount);
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (is_int)
        {
            int started = 0;
            bool done = false;
            signed char negative = 0;
            u32 bits = 0;
            std::uint64_t wide_bits = 0;
            const bool radix_prefix = spec == 'x' || spec == 'i';
            unsigned number_spec = spec;
            if (spec == 'p')
            {
                size_count = 1;
            }
            if (spec == 'i')
            {
                number_spec = 'd';
            }
            if (ch == '-' || ch == '+')
            {
                if (ch == '-')
                {
                    negative = 1;
                }
                width -= 1;
                if (width == 0 && widthset != 0)
                {
                    done = true;
                }
                else
                {
                    charcount += 1;
                    ch = get_unit();
                }
            }
            if (radix_prefix && ch == '0')
            {
                charcount += 1;
                ch = get_unit();
                if (ch == 'x' || ch == 'X')
                {
                    charcount += 1;
                    ch = get_unit();
                    if (widthset != 0)
                    {
                        width -= 2;
                        if (width < 1)
                        {
                            done = true;
                        }
                    }
                    number_spec = 'x';
                }
                else
                {
                    started = 1;
                    if (number_spec == 'x')
                    {
                        charcount -= 1;
                        unget_unit(ch);
                        ch = '0';
                    }
                    else
                    {
                        if (widthset != 0)
                        {
                            width -= 1;
                            if (width == 0)
                            {
                                done = true;
                            }
                        }
                        number_spec = 'o';
                    }
                }
            }
            while (!done)
            {
                const bool hex = number_spec == 'x' || number_spec == 'p';
                bool take = false;
                if (hex)
                {
                    if (narrow_class(ch, 0x80u))
                    {
                        if (i64 != 0)
                        {
                            wide_bits <<= 4;
                        }
                        else
                        {
                            bits <<= 4;
                        }
                        ch = hex_adjust(ch);
                        take = true;
                    }
                }
                else if (narrow_class(ch, 4u))
                {
                    if (number_spec == 'o')
                    {
                        if (ch < 0x38u)
                        {
                            if (i64 != 0)
                            {
                                wide_bits <<= 3;
                            }
                            else
                            {
                                bits <<= 3;
                            }
                            take = true;
                        }
                    }
                    else
                    {
                        if (i64 != 0)
                        {
                            wide_bits *= 10u;
                        }
                        else
                        {
                            bits *= 10u;
                        }
                        take = true;
                    }
                }
                if (!take)
                {
                    done = true;
                    charcount -= 1;
                    unget_unit(ch);
                    break;
                }
                started += 1;
                const unsigned digit = (ch & 0xffffu) - 0x30u;
                if (i64 != 0)
                {
                    wide_bits += digit;
                }
                else
                {
                    bits += digit;
                }
                if (widthset != 0)
                {
                    width -= 1;
                    if (width == 0)
                    {
                        done = true;
                        break;
                    }
                }
                charcount += 1;
                ch = get_unit();
            }
            if (number_spec == 0x46u)
            {
                started = 0;
            }
            if (started == 0)
            {
                break;
            }
            if (negative != 0)
            {
                if (i64 != 0)
                {
                    wide_bits = static_cast<std::uint64_t>(-static_cast<std::int64_t>(wide_bits));
                }
                else
                {
                    bits = static_cast<u32>(-static_cast<int>(bits));
                }
            }
            if (suppress == 0)
            {
                void* dest = va_arg(args, void*);
                assigned += 1;
                if (i64 != 0)
                {
                    *reinterpret_cast<std::uint64_t*>(dest) = wide_bits;
                }
                else if (size_count != 0)
                {
                    *reinterpret_cast<u32*>(dest) = bits;
                }
                else
                {
                    *reinterpret_cast<u16*>(dest) = static_cast<u16>(bits);
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (is_float)
        {
            int started = 0;
            int float_len = 0;
            char16_t float_buf[352];
            if (ch == '-' || ch == '+')
            {
                if (ch == '-')
                {
                    float_buf[float_len] = '-';
                    float_len += 1;
                }
                width -= 1;
                charcount += 1;
                ch = get_unit();
            }
            if (widthset == 0 || width > 0x15d)
            {
                width = 0x15d;
            }
            auto take_digits = [&]()
            {
                while ((ch & 0xff00u) == 0 && narrow_class(ch, 4u))
                {
                    const int before = width;
                    width -= 1;
                    if (before == 0)
                    {
                        break;
                    }
                    started += 1;
                    float_buf[float_len] = static_cast<char16_t>(ch);
                    float_len += 1;
                    charcount += 1;
                    ch = get_unit();
                }
            };
            take_digits();
            char16_t decimal = 0;
            fn_16e07a(&decimal, &g_crt_decimal_point, g_crt_mb_cur_max);
            if (ch == static_cast<unsigned>(decimal))
            {
                const int before = width;
                width -= 1;
                if (before != 0)
                {
                    charcount += 1;
                    ch = get_unit();
                    float_buf[float_len] = decimal;
                    float_len += 1;
                    take_digits();
                }
            }
            if (started != 0 && (ch == 'e' || ch == 'E'))
            {
                const int before = width;
                width -= 1;
                if (before != 0)
                {
                    float_buf[float_len] = 'e';
                    float_len += 1;
                    charcount += 1;
                    ch = get_unit();
                    if (ch == '-' || ch == '+')
                    {
                        const unsigned sign = ch;
                        const int sign_before = width;
                        width -= 1;
                        if (sign_before != 0)
                        {
                            if (sign == '-')
                            {
                                float_buf[float_len] = '-';
                                float_len += 1;
                            }
                            charcount += 1;
                            ch = get_unit();
                        }
                        else
                        {
                            width = 0;
                            if (sign == '-')
                            {
                                float_buf[float_len] = '-';
                                float_len += 1;
                            }
                        }
                    }
                    take_digits();
                }
            }
            charcount -= 1;
            unget_unit(ch);
            if (started == 0)
            {
                break;
            }
            if (suppress != 0)
            {
                if (!finish_field())
                {
                    break;
                }
                continue;
            }
            assigned += 1;
            float_buf[float_len] = 0;
            const int bytes = float_len * 2 + 2;
            void* block = std::malloc(static_cast<std::size_t>(bytes));
            if (block == nullptr)
            {
                break;
            }
            std::memcpy(block, float_buf, static_cast<std::size_t>(bytes));
            void* dest = va_arg(args, void*);
            assign_float(static_cast<int>(size_count) - 1, dest, static_cast<char16_t*>(block));
            fn_167f2e(block);
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (spec == 'c' || spec == 's' || spec == '{')
        {
            signed char wide_out = 0;
            unsigned char invert = 0;
            if (spec == 'c' && widthset == 0)
            {
                widthset = 1;
                width += 1;
            }
            if (wide_count > 0)
            {
                wide_out = 1;
            }
            if (spec == '{')
            {
                if (scan == nullptr)
                {
                    scan = static_cast<unsigned char*>(std::malloc(0x2000));
                    if (scan == nullptr)
                    {
                        break;
                    }
                }
                std::memset(scan, 0, 0x2000);
                const char16_t* cursor = fmt + 1;
                unsigned prev = 0;
                if (*cursor == '^')
                {
                    invert = 0xff;
                    cursor += 1;
                }
                if (*cursor == ']')
                {
                    prev = ']';
                    scan[0x5du >> 3] = static_cast<unsigned char>(scan[0x5du >> 3] | (1u << (0x5du & 7u)));
                    cursor += 1;
                }
                bool closed = false;
                while (*cursor != 0)
                {
                    const unsigned cur = *cursor;
                    if (cur == ']')
                    {
                        closed = true;
                        break;
                    }
                    cursor += 1;
                    if (cur == '-' && prev != 0 && *cursor != ']' && *cursor != 0)
                    {
                        const unsigned end = *cursor;
                        cursor += 1;
                        unsigned low = prev;
                        unsigned high = end;
                        if (low > high)
                        {
                            const unsigned swap = low;
                            low = high;
                            high = swap;
                        }
                        for (unsigned unit = low;; unit += 1)
                        {
                            scan[unit >> 3] = static_cast<unsigned char>(scan[unit >> 3] | (1u << (unit & 7u)));
                            if (unit == high)
                            {
                                break;
                            }
                        }
                        prev = 0;
                        continue;
                    }
                    prev = cur;
                    scan[cur >> 3] = static_cast<unsigned char>(scan[cur >> 3] | (1u << (cur & 7u)));
                }
                if (!closed)
                {
                    break;
                }
                fmt = cursor;
            }

            char* write = nullptr;
            char* mark = nullptr;
            if (suppress == 0)
            {
                write = static_cast<char*>(va_arg(args, void*));
            }
            mark = write;
            charcount -= 1;
            unget_unit(ch);
            while (true)
            {
                if (widthset != 0)
                {
                    const int before = width;
                    width -= 1;
                    if (before == 0)
                    {
                        break;
                    }
                }
                charcount += 1;
                ch = get_unit();
                if ((ch & 0xffffu) == 0xffffu)
                {
                    charcount -= 1;
                    unget_unit(ch);
                    break;
                }
                if (spec != 'c')
                {
                    if (spec == 's')
                    {
                        if ((ch >= 9u && ch <= 0x0du) || ch == 0x20u)
                        {
                            charcount -= 1;
                            unget_unit(ch);
                            break;
                        }
                    }
                    else
                    {
                        const unsigned bit = 1u << (ch & 7u);
                        const int table_byte = static_cast<signed char>(scan[ch >> 3]);
                        const int flipped = table_byte ^ static_cast<signed char>(invert);
                        if ((flipped & static_cast<int>(bit)) == 0)
                        {
                            charcount -= 1;
                            unget_unit(ch);
                            break;
                        }
                    }
                }
                if (suppress != 0)
                {
                    mark += 2;
                    continue;
                }
                if (wide_out == 0)
                {
                    const int produced = wctomb_byte(write, ch);
                    write += produced;
                }
                else
                {
                    *reinterpret_cast<char16_t*>(write) = static_cast<char16_t>(ch);
                    write += 2;
                }
            }
            if (write == mark)
            {
                break;
            }
            if (suppress == 0)
            {
                assigned += 1;
                if (spec != 'c')
                {
                    if (wide_out == 0)
                    {
                        *write = 0;
                    }
                    else
                    {
                        *reinterpret_cast<char16_t*>(write) = 0;
                    }
                }
            }
            if (!finish_field())
            {
                break;
            }
            continue;
        }

        if (*fmt != static_cast<char16_t>(ch))
        {
            unget_unit(ch);
            break;
        }
        matched = static_cast<unsigned char>(matched - 1);
        if (!finish_field())
        {
            break;
        }
    }

    if (scan != nullptr)
    {
        fn_167f2e(scan);
    }
    va_end(args);
    if ((ch & 0xffffu) == 0xffffu && assigned == 0 && matched == 0)
    {
        return -1;
    }
    return assigned;
}

int fn_16afdd()
{
    // Xbox 0x0016AFDD
    // _mtinit. Pointer at 0x0032C804, in the __xi_a..__xi_z span
    // (0x0032C800..0x0032C80C). Startup 0x0014FAC1 calls 0x0015172C, which
    // walks that span, then _cinit at 0x001516D4, then WinMain. The walker
    // ignores the return. This function returns 0 on every path that returns.
    //
    // __mtinitlocks (0x0016809D) walks the 36 descriptors. Kind 1 stores a
    // critical section from the pool at 0x004DD0D8 (0x1C bytes each) and
    // calls 0x0016D9F8 with spin count 0xFA0. Any other kind is left alone.
    // A failed init clears that descriptor's pointer and returns 0. Success
    // returns 1. The port uses a host recursive mutex in place of the Xbox
    // RTL_CRITICAL_SECTION. The spin count is passed through; the default
    // target ignores it. A zero return from that call clears the pointer.
    //
    // Failure calls 0x001680E6, which walks 0x00337AB0 up to 0x00337BD0 and
    // deletes a critical section only when the pointer is set and the kind
    // is not 1. Kind-1 sections created before the failure stay in place.
    //
    // Success calloc's 0x88 bytes (0x00168012) and stores the pointer at
    // fs:[4][__tls_index] + 0xC. fs:[4] is the thread TLS array. __tls_index
    // is the dword at 0x00434234, zero in the image until the loader writes
    // it. A null block writes 0x00338280 through address 0x54 and 1 through
    // address 0x14 (_pxcptacttab and _holdrand with a null base), then faults
    // on the thread-handle store. The port does not write low memory and
    // returns 0 instead of faulting.
    //
    // A live block gets GetCurrentThreadId (0x0014BC53, fs:[0x28] + 0x12C)
    // in the first dword and 0xFFFFFFFF in the second. 0x004DD290 becomes
    // 0x0016EEC4, and XRegisterThreadNotifyRoutine (0x0014BAC8) inserts
    // 0x004DD288 at the tail of the list headed by 0x0032C9E0 while holding
    // the critical section at 0x0032C9C4. 0x0016EEC4 is the notify routine:
    // a nonzero argument calls [0x00337A8C] when that pointer is set, and a
    // zero argument calls [0x00337A90] when set, then _freeptd(0) at
    // 0x0016AF8B. Both image pointers are 0x00167388, which is a ret.
    for (u32 i = 0; i < 0x24; ++i)
    {
        g_crt_locks[i].kind = kCrtLockKind[i];
    }

    int locks_ok = 1;
    for (u32 i = 0; i < 0x24; ++i)
    {
        if (g_crt_locks[i].kind != 1)
        {
            continue;
        }
        try
        {
            g_crt_lock_mu[i] = std::make_unique<std::recursive_mutex>();
            g_crt_locks[i].cs = g_crt_lock_mu[i].get();
        }
        catch (...)
        {
            g_crt_locks[i].cs = nullptr;
            locks_ok = 0;
            break;
        }
        if (fn_16d9f8(g_crt_locks[i].cs, 0xFA0) == 0)
        {
            g_crt_lock_mu[i].reset();
            g_crt_locks[i].cs = nullptr;
            locks_ok = 0;
            break;
        }
    }

    if (locks_ok == 0)
    {
        for (CrtLockSlot& slot : g_crt_locks)
        {
            if (slot.cs != nullptr && slot.kind != 1)
            {
                slot.cs = nullptr;
            }
        }
        return 0;
    }

    CrtTidData* ptd = static_cast<CrtTidData*>(std::calloc(1, sizeof(CrtTidData)));
    if (g_crt_tls_index == 0)
    {
        g_crt_module_tls.ptd = ptd;
    }
    if (ptd == nullptr)
    {
        return 0;
    }

#if defined(_WIN32)
    const u32 tid = static_cast<u32>(GetCurrentThreadId());
#else
    const u32 tid = 1;
#endif
    ptd->thandle |= 0xffffffffu;
    ptd->tid = tid;

    g_crt_thread_notify.routine = 0x0016EEC4u;
    {
        std::lock_guard<std::recursive_mutex> hold(g_xthread_notify_cs);
        if (g_xthread_notify_head.flink == nullptr)
        {
            g_xthread_notify_head.flink = &g_xthread_notify_head;
            g_xthread_notify_head.blink = &g_xthread_notify_head;
        }
        XThreadNotify* head = &g_xthread_notify_head;
        XThreadNotify* prev = head->blink;
        g_crt_thread_notify.flink = head;
        g_crt_thread_notify.blink = prev;
        prev->flink = &g_crt_thread_notify;
        head->blink = &g_crt_thread_notify;
    }
    return 0;
}

int fn_14aaed(const u16* src, int cch, u8* dst, int cb)
{
    // Xbox 0x0014AAED
    // WideCharToMultiByte for code page 65001. A cch of -1 counts through the
    // NUL (0x00166ECB) and includes it. Bytes that fit are stored. A nonzero
    // cb shorter than the encoded size returns 0; 0x0014B00C would record
    // last-error 0x7A.
    if (cch == -1)
    {
        cch = fn_166ecb(src) + 1;
    }

    int count = 0;
    while (cch > 0)
    {
        const u16 unit = *src;
        ++src;
        --cch;

        if (unit < 0x80)
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(unit);
                ++dst;
            }
            continue;
        }

        if (unit < 0x800)
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0xc0 | ((unit >> 6) & 0x1f));
                ++dst;
            }
        }
        else
        {
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0xe0 | ((unit >> 12) & 0x0f));
                ++dst;
            }
            ++count;
            if (count <= cb && dst != nullptr)
            {
                *dst = static_cast<u8>(0x80 | ((unit >> 6) & 0x3f));
                ++dst;
            }
        }

        ++count;
        if (count <= cb && dst != nullptr)
        {
            *dst = static_cast<u8>(0x80 | (unit & 0x3f));
            ++dst;
        }
    }

    if (cb != 0 && cb < count)
    {
        return 0;
    }
    return count;
}

// Win32 code stored by 0x0014B00C. 0x0014B03A maps the NTSTATUS, and
// 0x0014AFE4 reads this slot. XCreateSaveGame (0x0014C70F) treats 0xB7 as
// "the directory is already there".
u32 g_xapi_last_error = 0;

int fn_16d9e8(std::recursive_mutex* section, u32 spin_count)
{
    // Xbox 0x0016D9E8
    // stdcall, ret 8. Pushes the critical section and calls
    // RtlInitializeCriticalSection (kernel ordinal 291, thunk 0x00202FC0).
    // The spin count is left for that epilogue. EAX is 1 when the call
    // returns. The host mutex is already constructed. A null section is the
    // access fault that kernel call would raise. That status is not
    // 0xC0000017, so the caller returns 0 and leaves the last-error slot.
    (void)spin_count;
    if (section == nullptr)
    {
#if defined(_WIN32)
        RaiseException(0xC0000005u, 0, 0, nullptr);
#else
        throw static_cast<unsigned long>(0xC0000005u);
#endif
    }
    return 1;
}

int fn_16d9f8(std::recursive_mutex* section, u32 spin_count)
{
    // Xbox 0x0016D9F8
    // cdecl. The callers pop both arguments. __SEH_prolog (0x00166D58) is
    // entered with local size 0x10 and the scope table at 0x00209E78:
    // enclosing level -1, filter 0x0016DA2C, handler 0x0016DA3A.
    //
    // 0x004DD2D4 starts at 0. A null pointer is replaced with 0x0016D9E8.
    // The try level at [ebp-4] is then 0, and both arguments are passed to
    // that target. Its EAX is the success return. The filter returns 1, so
    // every exception runs the handler. The handler reloads ESP from
    // [ebp-0x18]. Exception code 0xC0000017 (STATUS_NO_MEMORY) stores 8
    // through 0x0014B00C. Every exception returns 0. __SEH_epilog
    // (0x00166D93) leaves EAX unchanged.
    if (g_crt_crit_init == nullptr)
    {
        g_crt_crit_init = fn_16d9e8;
    }

#if defined(_WIN32)
    __try
    {
        return g_crt_crit_init(section, spin_count);
    }
    __except (1)
    {
        if (GetExceptionCode() == 0xC0000017u)
        {
            g_xapi_last_error = 8;
        }
        return 0;
    }
#else
    try
    {
        return g_crt_crit_init(section, spin_count);
    }
    catch (unsigned long code)
    {
        if (code == 0xC0000017u)
        {
            g_xapi_last_error = 8;
        }
        return 0;
    }
    catch (...)
    {
        return 0;
    }
#endif
}

void fn_16e770(double* result, const char* text)
{
    // Xbox 0x0016E770
    // _atodbl. cdecl. __fassign (0x0016B1FE) calls this when its size flag
    // is nonzero, then copies the eight bytes into the caller's pointer.
    // The frame holds a 12-byte _LDBL12 and an end pointer. __strgtold12
    // (0x0016E7CA) is called with mult12, scale, decpt, and implicit_E all
    // zero. _ld12tod (0x0016E744) pushes the double descriptor at
    // 0x00338308 and _ld12cvt (0x0016E5EC) writes the IEEE double. Both
    // returns are discarded; __fassign overwrites EAX.
    //
    // Whitespace is 0x20, 0x09, 0x0A, and 0x0D. An optional sign comes
    // next, then digits, one decimal point (the byte at 0x0033818C, '.'),
    // and an optional e/E/d/D exponent. No significant digits store a
    // signed zero, so "-" and "-0" are negative zero. At most 25 digits
    // are kept. A 25th digit drops back to 24, and when digit 23 is at
    // least 5 that digit is bumped. The parsed exponent clamps at 5201.
    // Above 5200 the double is a signed infinity; below -5200 it is a
    // signed zero. __mb_cur_max above 1 classifies digits with _isctype
    // (0x0016AE1F) and mask 4; otherwise the low byte of _pctype.

    auto digit_byte = [](unsigned char c) -> bool
    {
        if (g_crt_mb_cur_max > 1)
        {
            return fn_16ae1f(static_cast<int>(c), 4u) != 0;
        }
        return (static_cast<unsigned>(static_cast<u8>(g_crt_pctype[c])) & 4u) != 0;
    };

    auto store = [&](double value)
    {
        std::memcpy(result, &value, sizeof(value));
    };

    const char* p = text;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
    {
        ++p;
    }

    bool negative = false;
    if (*p == '+' || *p == '-')
    {
        negative = *p == '-';
        ++p;
        const unsigned char next = static_cast<unsigned char>(*p);
        if (next != '.' && !digit_byte(next))
        {
            store(negative ? -0.0 : 0.0);
            return;
        }
    }

    unsigned char digits[25]{};
    int ndigits = 0;
    int exp_adj = 0;
    bool saw = false;

    auto bump = [&](int delta)
    {
        if (delta > 0)
        {
            if (exp_adj < 20000)
            {
                exp_adj += 1;
            }
        }
        else if (exp_adj > -20000)
        {
            exp_adj -= 1;
        }
    };

    while (digit_byte(static_cast<unsigned char>(*p)) && *p == '0')
    {
        saw = true;
        ++p;
    }
    while (digit_byte(static_cast<unsigned char>(*p)))
    {
        saw = true;
        if (ndigits < 25)
        {
            digits[ndigits] = static_cast<unsigned char>(*p - '0');
            ndigits += 1;
        }
        else
        {
            bump(1);
        }
        ++p;
    }

    if (*p == '.')
    {
        ++p;
        if (!saw && !digit_byte(static_cast<unsigned char>(*p)))
        {
            store(negative ? -0.0 : 0.0);
            return;
        }
        if (ndigits == 0)
        {
            while (*p == '0')
            {
                saw = true;
                bump(-1);
                ++p;
            }
        }
        while (digit_byte(static_cast<unsigned char>(*p)))
        {
            saw = true;
            if (ndigits < 25)
            {
                digits[ndigits] = static_cast<unsigned char>(*p - '0');
                ndigits += 1;
                bump(-1);
            }
            ++p;
        }
    }

    int exp_mag = 0;
    bool exp_neg = false;
    bool saw_exp = false;
    const unsigned char exp_ch = static_cast<unsigned char>(*p);
    if (saw && (exp_ch == 'e' || exp_ch == 'E' || exp_ch == 'd' || exp_ch == 'D'))
    {
        const char* exp_mark = p;
        ++p;
        if (*p == '+' || *p == '-')
        {
            exp_neg = *p == '-';
            ++p;
        }
        if (!digit_byte(static_cast<unsigned char>(*p)))
        {
            p = exp_mark;
        }
        else
        {
            saw_exp = true;
            while (*p == '0')
            {
                ++p;
            }
            while (digit_byte(static_cast<unsigned char>(*p)))
            {
                if (exp_mag <= 5200)
                {
                    exp_mag = exp_mag * 10 + (*p - '0');
                    if (exp_mag > 5200)
                    {
                        exp_mag = 5201;
                    }
                }
                ++p;
            }
        }
    }

    if (!saw || ndigits == 0)
    {
        store(negative ? -0.0 : 0.0);
        return;
    }

    if (ndigits > 24)
    {
        if (digits[23] >= 5)
        {
            digits[23] = static_cast<unsigned char>(digits[23] + 1);
        }
        ndigits = 24;
        bump(1);
    }
    while (ndigits > 0 && digits[ndigits - 1] == 0)
    {
        ndigits -= 1;
        bump(1);
    }
    if (ndigits == 0)
    {
        store(negative ? -0.0 : 0.0);
        return;
    }

    int carry = 0;
    for (int i = ndigits - 1; i >= 0; --i)
    {
        const int sum = digits[i] + carry;
        digits[i] = static_cast<unsigned char>(sum % 10);
        carry = sum / 10;
    }
    if (carry != 0)
    {
        exp_adj += ndigits;
        digits[0] = 1;
        ndigits = 1;
    }

    int exp10 = exp_adj;
    if (saw_exp)
    {
        exp10 += exp_neg ? -exp_mag : exp_mag;
    }

    double value = 0.0;
    if (exp10 > 5200)
    {
        value = std::bit_cast<double>(0x7ff0000000000000ull);
    }
    else if (exp10 < -5200)
    {
        value = 0.0;
    }
    else
    {
        char buf[64];
        int n = 0;
        for (int i = 0; i < ndigits; ++i)
        {
            buf[n] = static_cast<char>('0' + digits[i]);
            n += 1;
        }
        if (exp10 != 0)
        {
            buf[n] = 'e';
            n += 1;
            int power = exp10;
            if (power < 0)
            {
                buf[n] = '-';
                n += 1;
                power = -power;
            }
            char rev[8];
            int rn = 0;
            do
            {
                rev[rn] = static_cast<char>('0' + power % 10);
                rn += 1;
                power /= 10;
            } while (power != 0);
            while (rn != 0)
            {
                rn -= 1;
                buf[n] = rev[rn];
                n += 1;
            }
        }
        buf[n] = '\0';
        const int saved_errno = errno;
        value = std::strtod(buf, nullptr);
        errno = saved_errno;
    }
    if (negative)
    {
        value = -value;
    }
    store(value);
}

void fn_16e79d(float* result, const char* text)
{
    // Xbox 0x0016E79D
    // _atoflt. cdecl, same 45-byte frame as _atodbl (0x0016E770): a 12-byte
    // _LDBL12 and an end pointer. __strgtold12 (0x0016E7CA) is called with
    // mult12, scale, decpt, and implicit_E all zero. _ld12tof (0x0016E75A)
    // pushes the float descriptor at 0x00338320 and _ld12cvt (0x0016E5EC)
    // writes an IEEE float. Both returns are discarded.
    //
    // That descriptor is max exponent 128, min exponent -127, 24 mantissa
    // bits, pack shift 8, width 32, bias 127. An unbiased exponent above
    // 127 is a signed infinity. Below -151 the result is a signed zero.
    // Exponents from -151 through -127 are subnormals. The sign lives in
    // bit 15 of the word at offset 0xA of the _LDBL12.
    //
    // __fassign (0x0016B1FE) calls this when the size flag is zero. The
    // string is already on the stack; the result pointer is the flag slot,
    // and those four bytes are copied to the caller's pointer. A nonzero
    // flag calls _atodbl and copies eight bytes.
    //
    // Digit scanning, the 25-digit cutoff, and the 5200 exponent clamp are
    // __strgtold12, shared with _atodbl. The host narrows that double to
    // the 24-bit mantissa _ld12tof stores.
    double parsed = 0.0;
    fn_16e770(&parsed, text);
    const float value = static_cast<float>(parsed);
    std::memcpy(result, &value, sizeof(value));
}

void* fn_14aaba(u32 bytes, u32 lowest, u32 highest, u32 alignment, u32 protect)
{
    // Xbox 0x0014AABA
    // stdcall. The five arguments pass through to MmAllocateContiguousMemoryEx
    // (kernel ordinal 166, thunk 0x00202EA8). XMemAllocDefault (0x0014F90F)
    // calls this when the attribute alignment nibble is 1: size, lowest 0,
    // highest 0x00FFFFFF, alignment 0x1000, and a protect dword from
    // DAT_0032CA10 (0x02, 0x204, 0x04, 0x404). A null result stores last-error
    // 8 (ERROR_NOT_ENOUGH_MEMORY). A pointer leaves that slot unchanged.
    // The kernel returns the virtual address of physically contiguous pages
    // that fit in [lowest, highest]. Alignment below a page becomes a page.
    // The port allocates one aligned host block of the page-rounded size.
    // Protect stays a host default. Release the pointer with _aligned_free
    // (ordinal 171, MmFreeContiguousMemory).
    (void)protect;

    if (bytes == 0 || highest < lowest)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    u32 align = alignment;
    if (align < 0x1000)
    {
        align = 0x1000;
    }
    if ((align & (align - 1u)) != 0)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    const u64 span_end = highest;
    const u64 start = (static_cast<u64>(lowest) + align - 1u) & ~(static_cast<u64>(align) - 1u);
    const u64 rounded = (static_cast<u64>(bytes) + align - 1u) & ~(static_cast<u64>(align) - 1u);
    if (start > span_end || rounded > span_end - start + 1u)
    {
        g_xapi_last_error = 8;
        return nullptr;
    }

    void* block = nullptr;
#if defined(_WIN32)
    block = _aligned_malloc(static_cast<size_t>(rounded), align);
#else
    block = std::aligned_alloc(align, static_cast<size_t>(rounded));
#endif
    if (block == nullptr)
    {
        g_xapi_last_error = 8;
    }
    return block;
}

// Retail image at base 0x10000. Count is the dword at 0x0001011C. The header
// array pointer is the dword at 0x00010120. Each record is 0x38 bytes and the
// name is the C string at +0x14. Virtual address sits at +4 and virtual size
// at +8, which 0x0014AF9B and 0x0014AFDA read from the handle.
struct XbeSectionRecord
{
    u32 flags;
    u32 virtual_address;
    u32 virtual_size;
    u32 raw_address;
    u32 raw_size;
    const char* name;
};

const XbeSectionRecord g_xbe_sections[] = {
    {0x16, 0x11000, 0x160020, 0x1000, 0x160020, ".text"},
    {0x7, 0x171020, 0x135d8, 0x162000, 0xfd2c, "D3D"},
    {0x7, 0x184600, 0xe40, 0x172000, 0xe3c, "D3DX"},
    {0x7, 0x185440, 0x20598, 0x173000, 0x1f91c, "XGRPH"},
    {0x26, 0x1a59e0, 0xd3ac, 0x193000, 0xd3ac, "BINK"},
    {0x36, 0x1b2da0, 0x1175, 0x1a1000, 0x1174, "BINK32"},
    {0x36, 0x1b3f20, 0x1489, 0x1a3000, 0x1488, "BINK32A"},
    {0x36, 0x1b53c0, 0x123c, 0x1a5000, 0x123c, "BINK16"},
    {0x36, 0x1b6600, 0x14c0, 0x1a7000, 0x14c0, "BINK4444"},
    {0x36, 0x1b7ac0, 0x10b4, 0x1a9000, 0x10b4, "BINK5551"},
    {0x36, 0x1b8b80, 0xcac, 0x1ab000, 0xcac, "BINKYUY2"},
    {0x36, 0x1b9840, 0x9c, 0x1ac000, 0x9c, "BINKYUY2"},
    {0x36, 0x1b98e0, 0xb8, 0x1ad000, 0xb8, "BINKYUY2"},
    {0x36, 0x1b99a0, 0x88, 0x1ae000, 0x88, "BINKYUY2"},
    {0x36, 0x1b9a40, 0x130, 0x1af000, 0x130, "BINK16MX"},
    {0x16, 0x1b9b80, 0x558, 0x1b0000, 0x558, "BINK16X2"},
    {0x6, 0x1ba0e0, 0x1f8, 0x1b1000, 0x1f8, "BINK16M"},
    {0x6, 0x1ba2e0, 0x1bc, 0x1b2000, 0x1bc, "BINK32MX"},
    {0x6, 0x1ba4a0, 0x58c, 0x1b3000, 0x58c, "BINK32X2"},
    {0x6, 0x1baa40, 0x14c, 0x1b4000, 0x14c, "BINK32M"},
    {0x7, 0x1baba0, 0xac30, 0x1b5000, 0xabdc, "XACTENG"},
    {0x7, 0x1c57e0, 0x1e534, 0x1c0000, 0x1e2cc, "DSOUND"},
    {0x7, 0x1e3d20, 0x1907c, 0x1df000, 0x1907c, "WMADEC"},
    {0x7, 0x1fcda0, 0x60fc, 0x1f9000, 0x60fc, "XPP"},
    {0x6, 0x202ea0, 0x129958, 0x200000, 0x129948, ".rdata"},
    {0x7, 0x32c800, 0x222fc0, 0x32a000, 0x107a14, ".data"},
    {0x7, 0x54f7c0, 0xf5e0, 0x432000, 0xf5d4, "BINKDATA"},
    {0x26, 0x55eda0, 0x7180, 0x442000, 0x716c, "DOLBY"},
    {0x38, 0x565f20, 0x4800, 0x44d000, 0x4800, "$$XTIMAGE"},
    {0x38, 0x56a720, 0x4800, 0x452000, 0x4800, "$$XSIMAGE"},
    {0x3a, 0x56ef20, 0x28be, 0x44a000, 0x28be, "dirtydisk_bitmap"},
    {0x38, 0x5717e0, 0x7a0, 0x457000, 0x7a0, ".XTLID"},
};

void xapi_init_ansi(u16& length, const char*& buffer, const char* src)
{
    buffer = src;
    if (src == nullptr)
    {
        length = 0;
        return;
    }
    length = static_cast<u16>(std::strlen(src));
}

bool xapi_equal_ansi_ci(const char* left, u16 left_len, const char* right, u16 right_len)
{
    // RtlEqualString with CaseInSensitive set. Lengths must match. a-z folds
    // the way RtlUpperChar does; every other byte compares as itself.
    if (left_len != right_len)
    {
        return false;
    }
    for (u16 i = 0; i < left_len; ++i)
    {
        u8 a = static_cast<u8>(left[i]);
        u8 b = static_cast<u8>(right[i]);
        if (a >= 'a' && a <= 'z')
        {
            a = static_cast<u8>(a - 0x20);
        }
        if (b >= 'a' && b <= 'z')
        {
            b = static_cast<u8>(b - 0x20);
        }
        if (a != b)
        {
            return false;
        }
    }
    return true;
}

std::uintptr_t fn_14af34(const char* name)
{
    // Xbox 0x0014AF34
    // XGetSectionHandleA. stdcall. RtlInitAnsiString (thunk 0x00202EE8) on
    // the argument, then walk section headers. Each name is RtlInitAnsiString
    // from header+0x14 and RtlEqualString (thunk 0x00202EE4) with the case
    // flag set. The first hit returns that header. The end test is unsigned
    // below the header base plus count*0x38, so a miss falls out, stores
    // last-error 0x490 (ERROR_NOT_FOUND) via 0x0014B00C, and returns
    // 0xFFFFFFFF. A hit leaves last-error alone. 0x0003277C asks for
    // dirtydisk_bitmap. 0x00150ADD asks for $$XTINFO (absent), $$XTIMAGE,
    // and $$XSIMAGE.
    u16 want_len = 0;
    const char* want = nullptr;
    xapi_init_ansi(want_len, want, name);
    for (const XbeSectionRecord& section : g_xbe_sections)
    {
        u16 have_len = 0;
        const char* have = nullptr;
        xapi_init_ansi(have_len, have, section.name);
        if (xapi_equal_ansi_ci(want, want_len, have, have_len))
        {
            return reinterpret_cast<std::uintptr_t>(&section);
        }
    }
    g_xapi_last_error = 0x490;
    return static_cast<std::uintptr_t>(-1);
}

// DAT_0032CA50, indexed by the low byte of a UTF-16 unit. Bit 0x100000 is
// uppercase, 0x80000 lowercase, 0x40000 a digit, 0x20000 a symbol. The high
// bit marks everything else. '.' shares '-'s weight, 0x92 shares 0x91, and
// 0xF6 shares 0xF5.
u32 xapi_char_weight(u8 unit)
{
    if (unit == 0x2E)
    {
        return 0x0002002Du;
    }
    if (unit == 0x92)
    {
        return 0x80000091u;
    }
    if (unit == 0xF6)
    {
        return 0x000800F5u;
    }
    if (unit < 0x20 || (unit >= 0x7F && unit <= 0xA0))
    {
        return 0x80000000u | unit;
    }
    if (unit >= '0' && unit <= '9')
    {
        return 0x00040000u | unit;
    }
    if ((unit >= 'A' && unit <= 'Z') || (unit >= 0xC0 && unit <= 0xD6) || (unit >= 0xD8 && unit <= 0xDF))
    {
        return 0x00100000u | unit;
    }
    if ((unit >= 'a' && unit <= 'z') || (unit >= 0xE0 && unit <= 0xF5) || unit >= 0xF8)
    {
        return 0x00080000u | unit;
    }
    return 0x00020000u | unit;
}

int fn_14fb5b(u32 flags, const u16* left, int left_count, const u16* right, int right_count)
{
    // Xbox 0x0014FB5B
    // Wide compare used by lstrcmpW (0x0014A91C) with flags 0 and counts -1.
    // Bit 0 folds uppercase (weight bit 0x100000) to byte+0x20 with the
    // lowercase bit. Bit 2 drops symbol weights (0x20000), one side at a time.
    // Bit 0x1000 compares weight bit 0x10000 in the primary pass. With that
    // bit clear, those units are skipped and the difference is resolved from
    // DAT_0032CA20 when both sides end. A count of -1 walks to NUL. Running
    // out of an explicit count before NUL returns 3 if the left count hits 0
    // first and 1 if the right count does. A null pointer or a count below
    // -1 stores last-error 0x57 and returns 0. 1, 2, and 3 are less, equal,
    // and greater.
    if (left == nullptr || right == nullptr || left_count < -1 || right_count < -1)
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

    const bool ignore_case = (flags & 1u) != 0;
    const bool ignore_symbols = (flags & 4u) != 0;
    const bool word_sort = (flags & 0x1000u) == 0;
    // DAT_0032CA20. Index 0 is equal. 5 and 11 are greater; 7 and 10 are less.
    static constexpr int kTie[12] = {2, 0, 0, 0, 0, 3, 0, 1, 0, 0, 1, 3};
    u32 tie = 0;

    for (;;)
    {
        if (left_count == 0 || right_count == 0)
        {
            if (left_count == 0 && right_count == 0)
            {
                return kTie[tie];
            }
            return left_count == 0 ? 3 : 1;
        }

        if (*left == 0)
        {
            if (*right != 0)
            {
                return 1;
            }
            return kTie[tie];
        }
        if (*right == 0)
        {
            return 3;
        }

        const u8 left_byte = static_cast<u8>(*left);
        const u8 right_byte = static_cast<u8>(*right);
        u32 left_w = xapi_char_weight(left_byte);
        u32 right_w = xapi_char_weight(right_byte);

        if (word_sort && (left_w & 0x10000u) != 0)
        {
            if ((right_w & 0x10000u) == 0)
            {
                if (tie == 0)
                {
                    tie = 4;
                }
                tie |= 1u;
                ++left;
                --left_count;
                continue;
            }
            if (left_w < right_w)
            {
                tie = 7;
            }
            else if (right_w < left_w)
            {
                tie = 11;
            }
            ++left;
            ++right;
            --left_count;
            --right_count;
            continue;
        }
        if (word_sort && (right_w & 0x10000u) != 0)
        {
            if (tie == 0)
            {
                tie = 8;
            }
            tie |= 2u;
            ++right;
            --right_count;
            continue;
        }

        if (ignore_symbols)
        {
            if ((left_w & 0x20000u) != 0)
            {
                ++left;
                --left_count;
                continue;
            }
            if ((right_w & 0x20000u) != 0)
            {
                ++right;
                --right_count;
                continue;
            }
        }

        if (ignore_case)
        {
            if ((left_w & 0x100000u) != 0)
            {
                left_w = (static_cast<u32>(left_byte) + 0x20u) | 0x80000u;
            }
            if ((right_w & 0x100000u) != 0)
            {
                right_w = (static_cast<u32>(right_byte) + 0x20u) | 0x80000u;
            }
        }

        if (left_w < right_w)
        {
            return 1;
        }
        if (right_w < left_w)
        {
            return 3;
        }
        ++left;
        ++right;
        --left_count;
        --right_count;
    }
}

int fn_14fcf4(u32 flags, const u8* left, int left_count, const u8* right, int right_count)
{
    // Xbox 0x0014FCF4
    // Byte compare used by lstrcmpiA (0x0014AC6D) with flags 1 and counts -1.
    // Weights come from DAT_0032CA50. Bit 0 folds uppercase (weight bit
    // 0x100000) to byte+0x20 with the lowercase bit. Bit 2 drops symbol
    // weights (0x20000), one side at a time. Bit 0x1000 compares weight bit
    // 0x10000 in the primary pass. With that bit clear, those units are
    // skipped and the difference is resolved from DAT_0032CE50 when both
    // sides end. A count of -1 walks to NUL. Running out of an explicit count
    // before NUL returns 3 if the left count hits 0 first and 1 if the right
    // count does. A null pointer or a count below -1 stores last-error 0x57
    // and returns 0. 1, 2, and 3 are less, equal, and greater.
    if (left == nullptr || right == nullptr || left_count < -1 || right_count < -1)
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

    const bool ignore_case = (flags & 1u) != 0;
    const bool ignore_symbols = (flags & 4u) != 0;
    const bool word_sort = (flags & 0x1000u) == 0;
    // DAT_0032CE50. Index 0 is equal. 5 and 11 are greater; 7 and 10 are less.
    static constexpr int kTie[12] = {2, 0, 0, 0, 0, 3, 0, 1, 0, 0, 1, 3};
    u32 tie = 0;

    for (;;)
    {
        if (left_count == 0 || right_count == 0)
        {
            if (left_count == 0 && right_count == 0)
            {
                return kTie[tie];
            }
            return left_count == 0 ? 3 : 1;
        }

        const u8 left_byte = *left;
        const u8 right_byte = *right;
        if (left_byte == 0)
        {
            if (right_byte != 0)
            {
                return 1;
            }
            return kTie[tie];
        }
        if (right_byte == 0)
        {
            return 3;
        }

        u32 left_w = xapi_char_weight(left_byte);
        u32 right_w = xapi_char_weight(right_byte);

        if (word_sort && (left_w & 0x10000u) != 0)
        {
            if ((right_w & 0x10000u) == 0)
            {
                if (tie == 0)
                {
                    tie = 4;
                }
                tie |= 1u;
                ++left;
                --left_count;
                continue;
            }
            if (left_w < right_w)
            {
                tie = 7;
            }
            else if (right_w < left_w)
            {
                tie = 11;
            }
            ++left;
            ++right;
            --left_count;
            --right_count;
            continue;
        }
        if (word_sort && (right_w & 0x10000u) != 0)
        {
            if (tie == 0)
            {
                tie = 8;
            }
            tie |= 2u;
            ++right;
            --right_count;
            continue;
        }

        if (ignore_symbols)
        {
            if ((left_w & 0x20000u) != 0)
            {
                ++left;
                --left_count;
                continue;
            }
            if ((right_w & 0x20000u) != 0)
            {
                ++right;
                --right_count;
                continue;
            }
        }

        if (ignore_case)
        {
            if ((left_w & 0x100000u) != 0)
            {
                left_w = (static_cast<u32>(left_byte) + 0x20u) | 0x80000u;
            }
            if ((right_w & 0x100000u) != 0)
            {
                right_w = (static_cast<u32>(right_byte) + 0x20u) | 0x80000u;
            }
        }

        if (left_w < right_w)
        {
            return 1;
        }
        if (right_w < left_w)
        {
            return 3;
        }
        ++left;
        ++right;
        --left_count;
        --right_count;
    }
}

// Cursor left by fn_14b122. It already sits on the entry after the one
// copied into WIN32_FIND_DATA. FindNextFile passes a null name, and the
// kernel keeps the mask captured by the first query. "*.*" is match-all.
struct XapiFindCursor
{
    std::filesystem::directory_iterator cursor;
    std::string pattern;
    bool match_all = false;
};

bool xapi_find_match(const char* pattern, size_t pattern_len, const char* name, size_t name_len)
{
    auto fold = [](unsigned char c) -> unsigned char
    {
        if (c >= 'a' && c <= 'z')
        {
            return static_cast<unsigned char>(c - 0x20);
        }
        return c;
    };

    size_t pi = 0;
    size_t ni = 0;
    size_t star_p = static_cast<size_t>(-1);
    size_t star_n = 0;
    while (ni < name_len)
    {
        if (pi < pattern_len &&
            (pattern[pi] == '?' || fold(static_cast<unsigned char>(pattern[pi])) == fold(static_cast<unsigned char>(name[ni]))))
        {
            ++pi;
            ++ni;
            continue;
        }
        if (pi < pattern_len && pattern[pi] == '*')
        {
            star_p = pi;
            ++pi;
            star_n = ni;
            continue;
        }
        if (star_p != static_cast<size_t>(-1))
        {
            pi = star_p + 1;
            ++star_n;
            ni = star_n;
            continue;
        }
        return false;
    }
    while (pi < pattern_len && pattern[pi] == '*')
    {
        ++pi;
    }
    return pi == pattern_len;
}

std::string xapi_narrow_filename(const std::filesystem::path& path)
{
    const std::wstring wide = path.filename().wstring();
    std::string name;
    name.reserve(wide.size());
    for (wchar_t unit : wide)
    {
        if (static_cast<std::uint32_t>(unit) <= 0xFFu)
        {
            name.push_back(static_cast<char>(static_cast<unsigned char>(unit)));
        }
        else
        {
            name.push_back('?');
        }
    }
    return name;
}

u64 xapi_host_file_time(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto stamp = std::filesystem::last_write_time(path, ec);
    if (ec)
    {
        return 0;
    }
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(stamp);
    const auto unix_ticks = std::chrono::duration_cast<std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(
                                sys.time_since_epoch())
                                .count();
    const std::int64_t file_time = unix_ticks + 116444736000000000LL;
    if (file_time < 0)
    {
        return 0;
    }
    return static_cast<u64>(file_time);
}

void xapi_set_dir_open_error(const std::filesystem::path& opened, const std::error_code& ec)
{
    if (ec == std::errc::no_such_file_or_directory)
    {
        std::filesystem::path leaf = opened;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        std::error_code parent_ec;
        const auto parent = leaf.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
        return;
    }
    if (ec == std::errc::not_a_directory || ec.value() == 267)
    {
        g_xapi_last_error = 267;
        return;
    }
    if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
        return;
    }
    if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 0x57;
        return;
    }
    if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
        return;
    }
    g_xapi_last_error = 2;
}

void xapi_fill_find_data(u8* dst, const std::filesystem::directory_entry& entry, const std::string& name)
{
    // 0x0014B0B6. Attributes from FILE_DIRECTORY_INFORMATION+0x38 land at +0.
    // Creation, access, and write FILETIMEs follow; the port stores the host
    // last-write time in all three. EndOfFile high/low land at +0x1C/+0x20.
    // The ANSI name is copied to +0x2C and NUL-terminated. The first byte of
    // cAlternateFileName (+0x130) is cleared. Reserved slots at +0x24 and
    // +0x28 are left alone.
    u32 attr = 0x20;
    std::error_code ec;
    if (entry.is_directory(ec) && !ec)
    {
        attr = 0x10;
    }
    ec.clear();
    const auto perms = entry.status(ec).permissions();
    if (!ec && (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        attr |= 0x1u;
    }
    std::memcpy(dst + 0x00, &attr, 4);

    const u64 stamp = xapi_host_file_time(entry.path());
    const u32 stamp_lo = static_cast<u32>(stamp);
    const u32 stamp_hi = static_cast<u32>(stamp >> 32);
    std::memcpy(dst + 0x04, &stamp_lo, 4);
    std::memcpy(dst + 0x08, &stamp_hi, 4);
    std::memcpy(dst + 0x0C, &stamp_lo, 4);
    std::memcpy(dst + 0x10, &stamp_hi, 4);
    std::memcpy(dst + 0x14, &stamp_lo, 4);
    std::memcpy(dst + 0x18, &stamp_hi, 4);

    u32 size_lo = 0;
    u32 size_hi = 0;
    if ((attr & 0x10u) == 0)
    {
        ec.clear();
        const auto bytes = entry.file_size(ec);
        if (!ec)
        {
            size_lo = static_cast<u32>(bytes);
            size_hi = static_cast<u32>(bytes >> 32);
        }
    }
    std::memcpy(dst + 0x1C, &size_hi, 4);
    std::memcpy(dst + 0x20, &size_lo, 4);

    const size_t count = std::min(name.size(), size_t(259));
    if (count != 0)
    {
        std::memcpy(dst + 0x2C, name.data(), count);
    }
    dst[0x2C + count] = 0;
    dst[0x130] = 0;
}

std::uintptr_t fn_14b122(const char* path, void* find_data)
{
    // Xbox 0x0014B122
    // FindFirstFileA. RtlInitAnsiString, then walk back to the last backslash.
    // The head, including that slash, is the NtOpenFile object name under
    // ObDosDevicesDirectory (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE,
    // FILE_LIST_DIRECTORY|SYNCHRONIZE, share read|write, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // The tail is the NtQueryDirectoryFile pattern (FileDirectoryInformation,
    // class 1, 0x148 bytes). A tail of "*.*" is passed with length 0 so every
    // name matches. No backslash, or an empty tail, stores last-error 0x57
    // and returns 0xFFFFFFFF. The first hit is copied by 0x0014B0B6 and the
    // directory handle is returned. A failed query closes the handle. Other
    // failures go through XapiSetLastNTError: 2 when the leaf or the match is
    // missing, 3 when a parent is missing, 267 when the path is not a directory.
    if (path == nullptr)
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    const size_t length = std::strlen(path);
    size_t split = length;
    while (split != 0 && path[split - 1] != '\\')
    {
        --split;
    }
    const size_t tail = length - split;
    if (split == 0 || tail == 0)
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool match_all = tail == 3 && path[split] == '*' && path[split + 1] == '.' && path[split + 2] == '*';
    const std::string pattern(path + split, tail);
    const std::filesystem::path dir(std::string(path, split));

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        xapi_set_dir_open_error(dir, ec);
        return static_cast<std::uintptr_t>(-1);
    }

    const std::filesystem::directory_iterator end;
    while (it != end)
    {
        const std::string name = xapi_narrow_filename(it->path());
        if (match_all || xapi_find_match(pattern.data(), pattern.size(), name.data(), name.size()))
        {
            if (find_data != nullptr)
            {
                xapi_fill_find_data(static_cast<u8*>(find_data), *it, name);
            }
            std::error_code step_ec;
            it.increment(step_ec);
            if (step_ec)
            {
                it = std::filesystem::directory_iterator();
            }
            auto* state = new (std::nothrow) XapiFindCursor;
            if (state == nullptr)
            {
                g_xapi_last_error = 0x5AA;
                return static_cast<std::uintptr_t>(-1);
            }
            state->cursor = std::move(it);
            state->pattern = pattern;
            state->match_all = match_all;
            return reinterpret_cast<std::uintptr_t>(state);
        }

        it.increment(ec);
        if (ec)
        {
            xapi_set_dir_open_error(dir, ec);
            return static_cast<std::uintptr_t>(-1);
        }
    }

    g_xapi_last_error = 2;
    return static_cast<std::uintptr_t>(-1);
}

bool fn_14b22f(std::uintptr_t find_handle, void* find_data)
{
    // Xbox 0x0014B22F
    // FindNextFileA. NtQueryDirectoryFile on the handle from FindFirstFileA,
    // FileDirectoryInformation, class 1, 0x148 bytes, a null name, and
    // RestartScan false. The null name does not replace the mask from the
    // first query. A hit is copied by 0x0014B0B6. Failure stores the Win32
    // error through XapiSetLastNTError: 18 when the scan is exhausted, 6 for
    // a bad handle. The handle stays open.
    if (find_handle == 0 || find_handle == static_cast<std::uintptr_t>(-1))
    {
        g_xapi_last_error = 6;
        return false;
    }

    auto* state = reinterpret_cast<XapiFindCursor*>(find_handle);
    const std::filesystem::directory_iterator end;
    while (state->cursor != end)
    {
        const std::string name = xapi_narrow_filename(state->cursor->path());
        const bool hit = state->match_all ||
            xapi_find_match(state->pattern.data(), state->pattern.size(), name.data(), name.size());
        if (hit && find_data != nullptr)
        {
            xapi_fill_find_data(static_cast<u8*>(find_data), *state->cursor, name);
        }

        std::error_code ec;
        state->cursor.increment(ec);
        if (ec)
        {
            state->cursor = std::filesystem::directory_iterator();
        }
        if (hit)
        {
            return true;
        }
        if (ec)
        {
            g_xapi_last_error = 18;
            return false;
        }
    }

    g_xapi_last_error = 18;
    return false;
}

void fn_14c2d5(const u16* name, char* dest, int remaining)
{
    // Xbox 0x0014C2D5
    // XCreateSaveGame (0x0014C70F) appends this to the copied root path. Each
    // UTF-16 unit updates hash = (hash * 0x10000 + unit) mod (2^48 - 59) until
    // a zero unit. The 48-bit value is written as twelve uppercase hex digits,
    // low nibble at dest[11], then a NUL at dest[12]. 0x0014C2C1 adds 0x37 when
    // the nibble is greater than 9 and 0x30 otherwise. The third argument is
    // the caller's leftover path budget (0xF7 minus the root length) and is
    // left unread.
    (void)remaining;
    constexpr u64 kMod = 0xFFFFFFFFFFC5ull;
    u64 hash = 0;
    while (*name != 0)
    {
        hash = (hash * 0x10000ull + *name) % kMod;
        ++name;
    }

    for (int index = 11; index >= 0; --index)
    {
        const u32 nibble = static_cast<u32>(hash) & 0xFu;
        if (static_cast<i32>(nibble) > 9)
        {
            dest[index] = static_cast<char>(nibble + 0x37);
        }
        else
        {
            dest[index] = static_cast<char>(nibble + 0x30);
        }
        hash >>= 4;
    }
    dest[12] = '\0';
}

int fn_14c532(u8* find_data, const char* root, int root_len)
{
    // Xbox 0x0014C532
    // XFindFirstSaveGame (0x0014CA42) and XFindNextSaveGame (0x0014CB4C)
    // call this for each directory entry. Bit 0x10 of the WIN32_FIND_DATA
    // attributes at +0 selects directories. The root is copied to
    // szSaveGameDirectory at +0x140 and cFileName at +0x2C is appended at
    // the caller's root length. A joined length below 0x103 stores a
    // trailing backslash. A joined length below 0xF7 then appends
    // "\\SaveMeta.xbx" and 0x0014D08C opens it with GENERIC_READ,
    // FILE_SHARE_READ, OPEN_EXISTING, and FILE_ATTRIBUTE_NORMAL.
    // 0x0014C367 seeks to the start and requires a 2-byte read; a FEFF
    // word also passes. 0x0014C3AF reads the rest as UTF-16. The first
    // L"Name" wins only when the next wchar is '='; a hit inside
    // L"TitleName=" therefore supplies the value. Any other hit is
    // discarded through the next CR LF. The value is copied to the wide
    // name at +0x244, clipped to 0x7F wchars plus a NUL. The path is then
    // cut back to the trailing backslash. A miss returns 0 so the walk
    // continues.
    u32 attr = 0;
    std::memcpy(&attr, find_data, 4);
    if ((attr & 0x10u) == 0)
    {
        return 0;
    }

    const char* file_name = reinterpret_cast<const char*>(find_data + 0x2C);
    int name_len = 0;
    while (file_name[name_len] != '\0')
    {
        ++name_len;
    }

    char* dir = reinterpret_cast<char*>(find_data + 0x140);
    {
        int i = 0;
        char unit = 0;
        do
        {
            unit = root[i];
            dir[i] = unit;
            ++i;
        } while (unit != '\0');
    }
    {
        int i = 0;
        char unit = 0;
        do
        {
            unit = file_name[i];
            dir[root_len + i] = unit;
            ++i;
        } while (unit != '\0');
    }

    const int combined = name_len + root_len;
    if (combined < 0x103)
    {
        dir[combined] = '\\';
        dir[combined + 1] = '\0';
    }

    int result = 0;
    if (combined < 0xF7)
    {
        const char meta[] = "\\SaveMeta.xbx";
        int i = 0;
        char unit = 0;
        do
        {
            unit = meta[i];
            dir[combined + i] = unit;
            ++i;
        } while (unit != '\0');

        std::ifstream in(std::filesystem::path(dir), std::ios::binary);
        if (in)
        {
            unsigned char probe[2] = {};
            in.read(reinterpret_cast<char*>(probe), 2);
            const std::streamsize got = in.gcount();
            const u16 word = static_cast<u16>(probe[0] | (static_cast<unsigned>(probe[1]) << 8));
            if (got == 2 || word == 0xFEFFu)
            {
                in.clear();
                std::u16string text;
                unsigned char pair[2];
                while (in.read(reinterpret_cast<char*>(pair), 2))
                {
                    text.push_back(static_cast<char16_t>(pair[0] | (static_cast<unsigned>(pair[1]) << 8)));
                }

                u16* dest = reinterpret_cast<u16*>(find_data + 0x244);
                size_t line = 0;
                while (line < text.size())
                {
                    size_t crlf = line;
                    bool broke = false;
                    while (crlf + 1 < text.size())
                    {
                        if (text[crlf] == u'\r' && text[crlf + 1] == u'\n')
                        {
                            broke = true;
                            break;
                        }
                        ++crlf;
                    }
                    if (!broke)
                    {
                        break;
                    }

                    for (size_t at = line; at + 4 <= crlf; ++at)
                    {
                        if (text[at] != u'N' || text[at + 1] != u'a' || text[at + 2] != u'm' || text[at + 3] != u'e')
                        {
                            continue;
                        }
                        if (text[at + 4] == u'=')
                        {
                            const size_t value = at + 5;
                            size_t count = crlf - value;
                            if (count > 0x7F)
                            {
                                count = 0x7F;
                            }
                            for (size_t n = 0; n < count; ++n)
                            {
                                dest[n] = static_cast<u16>(text[value + n]);
                            }
                            dest[count] = 0;
                            result = 1;
                        }
                        break;
                    }
                    if (result != 0)
                    {
                        break;
                    }
                    line = crlf + 2;
                }
            }
        }
        dir[combined + 1] = '\0';
    }
    return result;
}

u32 fn_14c633(const char* path, const u16* expected)
{
    // Xbox 0x0014C633
    // XCreateSaveGame (0x0014C70F) calls this with the hashed directory plus
    // "\\SaveMeta.xbx" and the wide save name. 0x0014D08C opens that path
    // GENERIC_READ|SYNCHRONIZE, share none, OPEN_ALWAYS, FILE_ATTRIBUTE_SYSTEM.
    // A failed open returns GetLastError. 0x0014C367 seeks to the start and
    // requires a 2-byte read; a FEFF word also passes. 0x0014C3AF then reads
    // L"Name" the same way 0x0014C532 does: the first hit whose next wchar is
    // '=' wins, including one inside L"TitleName=", and any other hit is
    // dropped through the next CR LF. The value is clipped to 0x7F wchars.
    // 0x00166E99 compares it with the caller's name. A match returns 0. A
    // short read, a missing Name, or a different name returns 0x491
    // (ERROR_NO_MATCH) after CloseHandle. OPEN_ALWAYS leaves an empty file
    // behind when the parent exists and the meta file does not.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return g_xapi_last_error;
    }

    const std::filesystem::path fs_path(path);
    std::error_code ec;
    if (std::filesystem::is_directory(fs_path, ec))
    {
        const char last = path[std::strlen(path) - 1];
        g_xapi_last_error = last == '\\' ? 3u : 5u;
        return g_xapi_last_error;
    }
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        if (ec == std::errc::permission_denied)
        {
            g_xapi_last_error = 5;
        }
        else if (ec == std::errc::invalid_argument)
        {
            g_xapi_last_error = 123;
        }
        else if (ec.value() != 0)
        {
            g_xapi_last_error = static_cast<u32>(ec.value());
        }
        else
        {
            g_xapi_last_error = 3;
        }
        return g_xapi_last_error;
    }

    ec.clear();
    const bool present = std::filesystem::is_regular_file(fs_path, ec);
    if (!present)
    {
        if (ec && ec != std::errc::no_such_file_or_directory)
        {
            g_xapi_last_error = ec == std::errc::permission_denied ? 5u : 3u;
            return g_xapi_last_error;
        }
        const auto parent = fs_path.parent_path();
        std::error_code parent_ec;
        if (!parent.empty() && !std::filesystem::is_directory(parent, parent_ec))
        {
            g_xapi_last_error = 3;
            return g_xapi_last_error;
        }
        std::ofstream created(fs_path, std::ios::binary | std::ios::trunc);
        if (!created)
        {
            g_xapi_last_error = 5;
            return g_xapi_last_error;
        }
        g_xapi_last_error = 0;
    }
    else
    {
        g_xapi_last_error = 0xB7;
    }

    std::ifstream in(fs_path, std::ios::binary);
    if (!in)
    {
        g_xapi_last_error = 5;
        return g_xapi_last_error;
    }
    const std::string bytes(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});

    unsigned char probe[2] = {};
    if (!bytes.empty())
    {
        probe[0] = static_cast<unsigned char>(bytes[0]);
    }
    if (bytes.size() >= 2)
    {
        probe[1] = static_cast<unsigned char>(bytes[1]);
    }
    const u16 word = static_cast<u16>(probe[0] | (static_cast<unsigned>(probe[1]) << 8));
    if (bytes.size() < 2 && word != 0xFEFFu)
    {
        return 0x491;
    }

    std::u16string text;
    for (size_t i = 2; i + 1 < bytes.size(); i += 2)
    {
        text.push_back(static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
                                              (static_cast<unsigned>(static_cast<unsigned char>(bytes[i + 1])) << 8)));
    }

    u16 found[0x80];
    found[0] = 0;
    bool matched_key = false;
    size_t line = 0;
    while (line < text.size())
    {
        size_t crlf = line;
        bool broke = false;
        while (crlf + 1 < text.size())
        {
            if (text[crlf] == u'\r' && text[crlf + 1] == u'\n')
            {
                broke = true;
                break;
            }
            ++crlf;
        }
        if (!broke)
        {
            break;
        }

        for (size_t at = line; at + 4 <= crlf; ++at)
        {
            if (text[at] != u'N' || text[at + 1] != u'a' || text[at + 2] != u'm' || text[at + 3] != u'e')
            {
                continue;
            }
            if (text[at + 4] == u'=')
            {
                const size_t value = at + 5;
                size_t count = crlf - value;
                if (count > 0x7F)
                {
                    count = 0x7F;
                }
                for (size_t n = 0; n < count; ++n)
                {
                    found[n] = static_cast<u16>(text[value + n]);
                }
                found[count] = 0;
                matched_key = true;
            }
            break;
        }
        if (matched_key)
        {
            break;
        }
        line = crlf + 2;
    }

    if (!matched_key || expected == nullptr)
    {
        return 0x491;
    }

    const u16* left = found;
    const u16* right = expected;
    while (*left != 0 && *left == *right)
    {
        ++left;
        ++right;
    }
    if (*left != *right)
    {
        return 0x491;
    }
    return 0;
}

void fn_14c6b3(const char* path)
{
    // Xbox 0x0014C6B3
    // 0x0014D08C opens the path with GENERIC_READ|GENERIC_WRITE, share
    // read|write, a null security descriptor, OPEN_EXISTING, and
    // FILE_FLAG_BACKUP_SEMANTICS, so the handle may be a directory. A handle
    // other than -1 zeros 0x28 bytes of FILE_BASIC_INFORMATION, stores
    // KeQuerySystemTime at +0x10 (LastWriteTime), and calls NtSetInformationFile
    // with class 4. Creation, access, change, and FileAttributes stay 0, which
    // leaves those fields unchanged. CloseHandle follows. A failed open returns
    // immediately; 0x0014D08C has already stored the Win32 error.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return;
    }

    const std::filesystem::path fs_path(path);
    std::error_code ec;
    if (!std::filesystem::exists(fs_path, ec) || ec)
    {
        if (!ec)
        {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
        }
        xapi_set_dir_open_error(fs_path, ec);
        return;
    }

    std::filesystem::last_write_time(fs_path, std::filesystem::file_time_type::clock::now(), ec);
}

// XcSHA context, 0x74 bytes. count[0] is the high half of the byte length
// and count[1] the low half.
struct XapiShaContext
{
    u32 unknown[6];
    u32 state[5];
    u32 count[2];
    u8 buffer[64];
};

static_assert(sizeof(XapiShaContext) == 0x74);

struct XapiCalcSigContext
{
    u32 flags;
    const u8* key;
    XapiShaContext sha;
};

void xapi_sha_block(u32 state[5], const u8 block[64])
{
    u32 w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = (static_cast<u32>(block[i * 4]) << 24) | (static_cast<u32>(block[i * 4 + 1]) << 16) |
               (static_cast<u32>(block[i * 4 + 2]) << 8) | static_cast<u32>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i)
    {
        w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    for (int i = 0; i < 80; ++i)
    {
        u32 f = 0;
        u32 k = 0;
        if (i < 20)
        {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        const u32 temp = std::rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void xapi_sha_init(XapiShaContext& ctx)
{
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.state[0] = 0x67452301u;
    ctx.state[1] = 0xEFCDAB89u;
    ctx.state[2] = 0x98BADCFEu;
    ctx.state[3] = 0x10325476u;
    ctx.state[4] = 0xC3D2E1F0u;
}

void xapi_sha_update(XapiShaContext& ctx, const u8* data, u32 size)
{
    u32 filled = ctx.count[1] & 63u;
    const u32 low = ctx.count[1];
    ctx.count[1] = low + size;
    if (ctx.count[1] < size)
    {
        ctx.count[0] += 1;
    }
    ctx.count[0] += size >> 29;

    if (filled + size < 64u)
    {
        std::memcpy(ctx.buffer + filled, data, size);
        return;
    }

    while (filled + size >= 64u)
    {
        const u32 take = 64u - filled;
        std::memcpy(ctx.buffer + filled, data, take);
        data += take;
        size -= take;
        xapi_sha_block(ctx.state, ctx.buffer);
        filled = 0;
    }
    if (size != 0)
    {
        std::memcpy(ctx.buffer + filled, data, size);
    }
}

void xapi_sha_final(XapiShaContext& ctx, u8* out)
{
    const u64 bits = (static_cast<u64>(ctx.count[0]) << 32 | ctx.count[1]) << 3;
    u8 tail[72];
    u32 n = 0;
    tail[n++] = 0x80;
    const u32 mod = (ctx.count[1] + 1u) & 63u;
    const u32 zeros = (56u - mod) & 63u;
    for (u32 i = 0; i < zeros; ++i)
    {
        tail[n++] = 0;
    }
    for (int i = 0; i < 8; ++i)
    {
        tail[n++] = static_cast<u8>(bits >> ((7 - i) * 8));
    }
    xapi_sha_update(ctx, tail, n);
    for (int i = 0; i < 5; ++i)
    {
        out[i * 4] = static_cast<u8>(ctx.state[i] >> 24);
        out[i * 4 + 1] = static_cast<u8>(ctx.state[i] >> 16);
        out[i * 4 + 2] = static_cast<u8>(ctx.state[i] >> 8);
        out[i * 4 + 3] = static_cast<u8>(ctx.state[i]);
    }
}

// Retail certificate TitleID is 0x4D530053. AlternateTitleIDs are all zero.
// XboxSignatureKey is the 16-byte kernel export for this title. The sixteen
// XboxAlternateSignatureKeys follow it; a scan that finishes at index 16
// reads one key past that export.
constexpr u32 kCalcSigTitleId = 0x4D530053u;
constexpr u32 kCalcSigAlternateTitleIds[16] = {};
constexpr u8 kXboxSignatureKey[16] = {
    0x26, 0x18, 0xC5, 0x08, 0x88, 0x8E, 0x75, 0xB9, 0x36, 0x3E, 0x23, 0xDA, 0x8F, 0xE0, 0x2D, 0x37};
constexpr u8 kXboxAlternateSignatureKeys[17][16] = {
    {0xD9, 0x0D, 0xBF, 0xE7, 0x68, 0x73, 0x41, 0xBF, 0xBC, 0x9A, 0xD8, 0x89, 0xB6, 0x3E, 0xBF, 0x7B},
    {0x4C, 0x3F, 0x43, 0x3A, 0x19, 0x27, 0x8C, 0x19, 0x5E, 0x12, 0x08, 0x1E, 0xF7, 0x40, 0xC9, 0xE3},
    {0xA5, 0x77, 0xD7, 0xC9, 0x17, 0x18, 0xAC, 0x79, 0xEB, 0xBB, 0x22, 0xED, 0xF7, 0xEA, 0xFC, 0xB3},
    {0xBE, 0x2F, 0x42, 0x2A, 0x8A, 0xD9, 0x8A, 0xE0, 0x2B, 0xB5, 0x67, 0x0A, 0x87, 0xE4, 0x23, 0x0A},
    {0x65, 0x56, 0x61, 0x54, 0x63, 0x62, 0x23, 0x23, 0x7C, 0x42, 0x65, 0xC8, 0x43, 0x8C, 0x02, 0x61},
    {0xB3, 0xEB, 0xDC, 0xB3, 0x1A, 0x19, 0xEE, 0xD6, 0x86, 0xCE, 0x2C, 0x84, 0xF5, 0x03, 0x26, 0x50},
    {0xA7, 0xBB, 0x8B, 0x34, 0x72, 0xC0, 0x5C, 0x72, 0x61, 0x0B, 0x5B, 0x7B, 0x20, 0xCC, 0x20, 0x81},
    {0x43, 0xBE, 0xB4, 0x77, 0xF3, 0xC8, 0xEB, 0xE5, 0x0B, 0xCA, 0x04, 0x61, 0x93, 0xF7, 0x2A, 0x1C},
    {0x4A, 0x94, 0x4D, 0x0A, 0x4A, 0x65, 0x54, 0x38, 0xEE, 0x3C, 0x8E, 0x64, 0xD5, 0xFD, 0xA3, 0xBE},
    {0xEF, 0xA5, 0x62, 0xC5, 0x2D, 0x48, 0x63, 0xE2, 0x92, 0x05, 0xEE, 0x58, 0x78, 0xD1, 0xE6, 0x40},
    {0x13, 0x2E, 0x78, 0x97, 0x61, 0x9E, 0x07, 0x79, 0x92, 0x9B, 0xDA, 0x3A, 0x56, 0x75, 0x83, 0xE7},
    {0x01, 0x30, 0xDA, 0x18, 0x70, 0x09, 0x59, 0x27, 0x34, 0x96, 0xB4, 0xF4, 0x92, 0x48, 0x60, 0xE0},
    {0xC3, 0xED, 0xA2, 0xB3, 0xA1, 0x45, 0xF7, 0x17, 0x01, 0x8F, 0x34, 0xBC, 0xCA, 0x66, 0xF0, 0x25},
    {0x6B, 0xC6, 0xC8, 0x47, 0xB2, 0x19, 0xBC, 0xFC, 0xD7, 0xAD, 0x7C, 0xCA, 0xB4, 0x2F, 0xCA, 0x15},
    {0x0A, 0x6E, 0xA0, 0xA9, 0x97, 0x13, 0x68, 0x1C, 0x2A, 0xDC, 0xA4, 0x5F, 0x3B, 0x05, 0x91, 0xB3},
    {0x3F, 0x77, 0x3A, 0x8D, 0x9A, 0x0D, 0xB3, 0x7A, 0xA9, 0x99, 0xBB, 0x1E, 0x51, 0x55, 0xFD, 0xC8},
    {}};

std::uintptr_t fn_14cd37(u32 flags, u32 title_id)
{
    // Xbox 0x0014CD37
    // XCalculateSignatureBeginEx (0x0014CEB4) calls this for flags 0 and 1.
    // XMemAlloc(0x7C, 0x24830000). Failure stores last-error 8 and returns
    // -1. The flags occupy the first dword. Certificate TitleID at
    // [0x00010118]+8 selects XboxSignatureKey (thunk 0x00202F9C). Any other
    // id walks AlternateTitleIDs at certificate +0x5C, sixteen dwords. A zero
    // id stores last-error 5 and returns -1. A hit, or a walk that ends below
    // index 0x40, selects XboxAlternateSignatureKeys (thunk 0x00202FA0) plus
    // index * 16. The key pointer is stored at the next dword. 0x0014CBE9
    // copies 0x10 key bytes into a 0x40 block, XORs each dword with
    // 0x36363636, then XcSHAInit and XcSHAUpdate of that block fill the
    // 0x74-byte SHA context. The port releases the block when the id is
    // rejected.
    auto* ctx = new (std::nothrow) XapiCalcSigContext;
    if (ctx == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }

    ctx->flags = flags;
    const u8* key = kXboxSignatureKey;
    if (title_id != kCalcSigTitleId)
    {
        u32 index = 0;
        while (index < 0x10)
        {
            const u32 alt = kCalcSigAlternateTitleIds[index];
            if (alt == 0)
            {
                delete ctx;
                g_xapi_last_error = 5;
                return static_cast<std::uintptr_t>(-1);
            }
            if (title_id == alt)
            {
                break;
            }
            ++index;
        }
        if (index >= 0x40)
        {
            delete ctx;
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
        key = kXboxAlternateSignatureKeys[index];
    }

    ctx->key = key;
    u8 block[64] = {};
    std::memcpy(block, key, 0x10);
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x36363636u;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_init(ctx->sha);
    xapi_sha_update(ctx->sha, block, 0x40);
    return reinterpret_cast<std::uintptr_t>(ctx);
}

// Kernel XboxHDKey (thunk 0x00202F98, ordinal 323). Per console, not stored
// in the XBE. All zeros sends 0x0014CCC2 through ExQueryNonVolatileSetting.
constexpr u8 kXboxHDKey[16] = {};

void xapi_hmac_begin(XapiShaContext& ctx, const u8* key, u32 key_len)
{
    // Xbox 0x0014CBE9. Cap the key at 0x40, zero-pad, XOR each dword with
    // 0x36363636, then XcSHAInit and XcSHAUpdate of that block.
    if (key_len > 0x40)
    {
        key_len = 0x40;
    }
    u8 block[64] = {};
    if (key != nullptr && key_len != 0)
    {
        std::memcpy(block, key, key_len);
    }
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x36363636u;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, block, 0x40);
}

void xapi_hmac_final(XapiShaContext& ctx, const u8* key, u32 key_len, u8* out)
{
    // Xbox 0x0014CC50. Cap the key at 0x40, zero-pad, XOR each dword with
    // 0x5C5C5C5C. XcSHAFinal writes the inner digest into the 20 bytes that
    // follow that block. XcSHAInit, then one XcSHAUpdate of those 0x54 bytes,
    // then XcSHAFinal into out.
    if (key_len > 0x40)
    {
        key_len = 0x40;
    }
    u8 block[0x54] = {};
    if (key != nullptr && key_len != 0)
    {
        std::memcpy(block, key, key_len);
    }
    for (int i = 0; i < 16; ++i)
    {
        u32 word = 0;
        std::memcpy(&word, block + i * 4, 4);
        word ^= 0x5C5C5C5Cu;
        std::memcpy(block + i * 4, &word, 4);
    }
    xapi_sha_final(ctx, block + 0x40);
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, block, 0x54);
    xapi_sha_final(ctx, out);
}

int xapi_signature_nonroamable(XapiCalcSigContext& ctx, const u8* old_sig, u8* out)
{
    // Xbox 0x0014CCC2. OR the 16 XboxHDKey bytes. Any nonzero byte is the
    // HMAC key. All zeros calls ExQueryNonVolatileSetting(0xFFFF, type,
    // 256-byte image, result length) and uses the 16 bytes at image+0x36.
    // The status is ignored. The inner SHA is restarted on the signature
    // context (0x0014CBE9), updated with the 20-byte roamable digest, then
    // finished by 0x0014CC50 into out. Always returns 1.
    const u8* key = kXboxHDKey;
    u8 image[0x100] = {};
    bool any = false;
    for (u8 byte : kXboxHDKey)
    {
        if (byte != 0)
        {
            any = true;
            break;
        }
    }
    if (!any)
    {
        key = image + 0x36;
    }
    xapi_hmac_begin(ctx.sha, key, 0x10);
    xapi_sha_update(ctx.sha, old_sig, 0x14);
    xapi_hmac_final(ctx.sha, key, 0x10, out);
    return 1;
}

u32 fn_14cdb7(XapiCalcSigContext* ctx, u8* signature)
{
    // Xbox 0x0014CDB7
    // XCalculateSignatureEnd for flags 0 and 1 (dispatcher 0x0014CF57).
    // A null signature skips the digest and still frees the context. Otherwise
    // 0x0014CC50 writes the 20-byte HMAC. Flag bit 0 (non-roamable) lands that
    // digest in a 20-byte stack buffer and 0x0014CCC2 HMACs it with XboxHDKey
    // into the caller's buffer. XMemFree uses tag 0x24830000. A zero from
    // 0x0014CCC2 returns GetLastError; that helper returns 1.
    int ok = 1;
    if (signature != nullptr)
    {
        u8 local[20];
        u8* dest = signature;
        if ((ctx->flags & 1u) != 0)
        {
            dest = local;
        }
        xapi_hmac_final(ctx->sha, ctx->key, 0x10, dest);
        if ((ctx->flags & 1u) != 0)
        {
            ok = xapi_signature_nonroamable(*ctx, local, signature);
        }
    }
    delete ctx;
    if (ok == 0)
    {
        return g_xapi_last_error;
    }
    return 0;
}

std::uintptr_t fn_14ce19()
{
    // Xbox 0x0014CE19
    // XCalculateSignatureBeginEx (0x0014CEB4) for flag 2. XMemAlloc(0x7C,
    // 0x24830000). Failure stores last-error 8 and returns -1. The flags dword
    // is 2 and the key pointer is null. XcSHAInit fills the 0x74-byte SHA
    // context. No signature-key block is mixed in. 0x0014CE4F updates that
    // SHA and 0x0014CE69 finishes it.
    auto* ctx = new (std::nothrow) XapiCalcSigContext;
    if (ctx == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }

    ctx->key = nullptr;
    ctx->flags = 2;
    xapi_sha_init(ctx->sha);
    return reinterpret_cast<std::uintptr_t>(ctx);
}

u32 fn_14ce4f(XapiCalcSigContext* ctx, const u8* data, u32 size)
{
    // Xbox 0x0014CE4F
    // XCalculateSignatureUpdate. XcSHAUpdate feeds the caller's bytes into
    // the SHA context that sits 8 bytes into the block from 0x0014CD37 or
    // 0x0014CE19. Always returns 0.
    xapi_sha_update(ctx->sha, data, size);
    return 0;
}

u32 fn_14ce69(XapiCalcSigContext* ctx, u8* signature)
{
    // Xbox 0x0014CE69
    // XCalculateSignatureEnd for flag 2 (dispatcher 0x0014CF57). A null
    // signature skips the digest and still frees the context. Otherwise
    // XcSHAFinal writes the 20-byte SHA-1 into the caller's buffer from the
    // context started by 0x0014CE19. XMemFree uses tag 0x24830000. Always
    // returns 0.
    if (signature != nullptr)
    {
        xapi_sha_final(ctx->sha, signature);
    }
    delete ctx;
    return 0;
}

// Host stand-in for the kernel handle NtCreateFile returns. CloseHandle is a
// separate call; until then the stream stays open and delete-on-close waits.
struct XapiFileHandle
{
    std::filesystem::path path;
    bool directory = false;
    bool delete_on_close = false;
    std::unique_ptr<std::fstream> stream;
};

std::uintptr_t fn_14d08c(const char* name, u32 access, u32 share, const void* security, u32 disposition, u32 flags,
                         const void* template_file)
{
    // Xbox 0x0014D08C
    // CreateFileA. The security pointer and template handle are unused.
    // Disposition 1..5 becomes FILE_CREATE, FILE_OVERWRITE_IF, FILE_OPEN,
    // FILE_OPEN_IF, or FILE_OVERWRITE. TRUNCATE_EXISTING also requires
    // GENERIC_WRITE (0x40000000); any other disposition stores
    // XapiSetLastNTError(STATUS_INVALID_PARAMETER) and returns -1.
    // RtlInitAnsiString records whether the 16-bit length ends in '\\'.
    // NtCreateFile uses ObDosDevicesDirectory (0xFFFFFFFD),
    // OBJ_CASE_INSENSITIVE, access | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
    // attributes flags & 0x7FA7, the caller's share mode, and create options
    // folded from the Win32 flags: write-through, overlapped (drops
    // FILE_SYNCHRONOUS_IO_NONALERT), no-buffering, random, sequential,
    // delete-on-close (also ORs DELETE into the access), and backup
    // semantics (FILE_OPEN_FOR_BACKUP_INTENT, and not FILE_NON_DIRECTORY_FILE).
    // Success returns the handle. CREATE_ALWAYS plus FILE_OVERWRITTEN, or
    // OPEN_ALWAYS plus FILE_OPENED, stores 0xB7; every other success stores 0.
    // STATUS_OBJECT_NAME_COLLISION stores 0x50. STATUS_FILE_IS_A_DIRECTORY
    // stores 3 when the name ends in '\\' and 5 otherwise. Other failures
    // keep the Win32 code from XapiSetLastNTError.
    // The port opens a host path. Share mode is not enforced. A directory
    // opened with FILE_FLAG_BACKUP_SEMANTICS yields a handle with no stream.
    (void)share;
    (void)security;
    (void)template_file;

    constexpr u32 kGenericWrite = 0x40000000u;
    constexpr u32 kFileCreate = 2;
    constexpr u32 kFileOverwriteIf = 5;
    constexpr u32 kFileOpen = 1;
    constexpr u32 kFileOpenIf = 3;
    constexpr u32 kFileOverwrite = 4;
    constexpr u32 kFileOpened = 1;
    constexpr u32 kFileCreated = 2;
    constexpr u32 kFileOverwritten = 3;

    u32 nt_disposition = 0;
    if (disposition == 1)
    {
        nt_disposition = kFileCreate;
    }
    else if (disposition == 2)
    {
        nt_disposition = kFileOverwriteIf;
    }
    else if (disposition == 3)
    {
        nt_disposition = kFileOpen;
    }
    else if (disposition == 4)
    {
        nt_disposition = kFileOpenIf;
    }
    else if (disposition == 5 && (access & kGenericWrite) != 0)
    {
        nt_disposition = kFileOverwrite;
    }
    else
    {
        g_xapi_last_error = 0x57;
        return static_cast<std::uintptr_t>(-1);
    }

    if (name == nullptr)
    {
        g_xapi_last_error = 123;
        return static_cast<std::uintptr_t>(-1);
    }

    const u16 ansi_length = static_cast<u16>(std::strlen(name));
    const bool ends_with_slash = ansi_length >= 2 && name[ansi_length - 1] == '\\';
    if (ansi_length == 0)
    {
        g_xapi_last_error = 123;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool backup = (flags & 0x02000000u) != 0;
    const bool delete_on_close = (flags & 0x04000000u) != 0;
    const u32 attributes = flags & 0x7FA7u;
    const bool want_write = (access & kGenericWrite) != 0 || (access & 0x2u) != 0 || (access & 0x4u) != 0;
    const bool truncate = nt_disposition == kFileOverwrite || nt_disposition == kFileOverwriteIf;

    const std::filesystem::path fs_path(name);
    std::error_code ec;
    const bool exists = std::filesystem::exists(fs_path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
    {
        if (ec == std::errc::permission_denied)
        {
            g_xapi_last_error = 5;
        }
        else if (ec == std::errc::invalid_argument)
        {
            g_xapi_last_error = 123;
        }
        else if (ec.value() != 0)
        {
            g_xapi_last_error = static_cast<u32>(ec.value());
        }
        else
        {
            g_xapi_last_error = 3;
        }
        return static_cast<std::uintptr_t>(-1);
    }

    bool is_directory = false;
    if (exists)
    {
        ec.clear();
        is_directory = std::filesystem::is_directory(fs_path, ec);
        if (ec)
        {
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
    }

    auto parent_is_directory = [](const std::filesystem::path& path)
    {
        const auto parent = path.parent_path();
        if (parent.empty())
        {
            return true;
        }
        std::error_code parent_ec;
        return std::filesystem::is_directory(parent, parent_ec) && !parent_ec;
    };

    auto missing_error = [&]()
    {
        std::filesystem::path leaf = fs_path;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        const auto parent = leaf.parent_path();
        std::error_code parent_ec;
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else if (ends_with_slash || nt_disposition == kFileCreate || nt_disposition == kFileOpenIf ||
                 nt_disposition == kFileOverwriteIf)
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
    };

    if (is_directory && nt_disposition == kFileCreate)
    {
        g_xapi_last_error = 0x50;
        return static_cast<std::uintptr_t>(-1);
    }
    if (is_directory && !backup)
    {
        g_xapi_last_error = ends_with_slash ? 3u : 5u;
        return static_cast<std::uintptr_t>(-1);
    }
    if (is_directory && (nt_disposition == kFileOverwrite || nt_disposition == kFileOverwriteIf))
    {
        g_xapi_last_error = ends_with_slash ? 3u : 5u;
        return static_cast<std::uintptr_t>(-1);
    }

    if (!exists)
    {
        if (nt_disposition == kFileOpen || nt_disposition == kFileOverwrite || ends_with_slash ||
            !parent_is_directory(fs_path))
        {
            missing_error();
            return static_cast<std::uintptr_t>(-1);
        }
    }

    if (exists && !is_directory && nt_disposition == kFileCreate)
    {
        g_xapi_last_error = 0x50;
        return static_cast<std::uintptr_t>(-1);
    }

    const bool creating = !exists;
    if (exists && !is_directory && (truncate || want_write))
    {
        std::error_code perm_ec;
        const auto perms = std::filesystem::status(fs_path, perm_ec).permissions();
        if (!perm_ec && (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
        {
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }
    }

    auto* handle = new (std::nothrow) XapiFileHandle;
    if (handle == nullptr)
    {
        g_xapi_last_error = 8;
        return static_cast<std::uintptr_t>(-1);
    }
    handle->path = fs_path;
    handle->directory = is_directory;
    handle->delete_on_close = delete_on_close;

    u32 information = kFileOpened;
    if (!is_directory)
    {
        if (truncate || creating)
        {
            std::ofstream created(fs_path, std::ios::binary | std::ios::trunc);
            if (!created)
            {
                delete handle;
                g_xapi_last_error = 5;
                return static_cast<std::uintptr_t>(-1);
            }
            information = creating ? kFileCreated : kFileOverwritten;
        }

        auto mode = std::ios::binary | std::ios::in;
        if (want_write || truncate)
        {
            mode |= std::ios::out;
        }

        handle->stream = std::make_unique<std::fstream>(fs_path, mode);
        if (!handle->stream->is_open())
        {
            delete handle;
            if (creating)
            {
                std::error_code remove_ec;
                std::filesystem::remove(fs_path, remove_ec);
            }
            g_xapi_last_error = 5;
            return static_cast<std::uintptr_t>(-1);
        }

        if ((creating || disposition == 2) && (attributes & 1u) != 0)
        {
            std::error_code perm_ec;
            std::filesystem::permissions(fs_path, std::filesystem::perms::owner_write,
                                         std::filesystem::perm_options::remove, perm_ec);
        }
    }

    if ((disposition == 2 && information == kFileOverwritten) || (disposition == 4 && information == kFileOpened))
    {
        g_xapi_last_error = 0xB7;
    }
    else
    {
        g_xapi_last_error = 0;
    }
    return reinterpret_cast<std::uintptr_t>(handle);
}

bool fn_14d217(const char* path, const void* security)
{
    // Xbox 0x0014D217
    // CreateDirectoryA for XCreateSaveGame. The security pointer is unused.
    // RtlInitAnsiString, then NtCreateFile on ObDosDevicesDirectory
    // (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE, FILE_LIST_DIRECTORY|SYNCHRONIZE,
    // FILE_ATTRIBUTE_NORMAL, share read|write, FILE_CREATE, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // Success closes the handle. Failure stores the Win32 error: 0xB7 when
    // the name is taken, 3 when a parent is missing.
    (void)security;
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return false;
    }

    std::error_code ec;
    if (std::filesystem::create_directory(std::filesystem::path(path), ec))
    {
        return true;
    }

    if (!ec || ec == std::errc::file_exists)
    {
        g_xapi_last_error = 0xB7;
    }
    else if (ec == std::errc::no_such_file_or_directory)
    {
        g_xapi_last_error = 3;
    }
    else if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
    }
    else if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 123;
    }
    else if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
    }
    else
    {
        g_xapi_last_error = 0xB7;
    }
    return false;
}

bool fn_14d283(const char* path)
{
    // Xbox 0x0014D283
    // RemoveDirectoryA. RtlInitAnsiString, then NtOpenFile on
    // ObDosDevicesDirectory (0xFFFFFFFD) with OBJ_CASE_INSENSITIVE,
    // DELETE|SYNCHRONIZE, share read|write|delete, and
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // Success writes FileDispositionInformation (delete) and NtClose. Failure
    // stores the Win32 error: 145 when the directory is not empty, 267 when
    // the path is not a directory, 2 when the name is missing, 3 when a parent
    // is missing.
    if (path == nullptr || path[0] == '\0')
    {
        g_xapi_last_error = 123;
        return false;
    }

    std::error_code ec;
    const std::filesystem::path fs_path(path);
    if (std::filesystem::is_directory(fs_path, ec))
    {
        ec.clear();
        if (std::filesystem::remove(fs_path, ec))
        {
            return true;
        }
    }
    else if (!ec)
    {
        std::error_code exists_ec;
        if (std::filesystem::exists(fs_path, exists_ec))
        {
            g_xapi_last_error = 267;
            return false;
        }
    }

    if (!ec || ec == std::errc::no_such_file_or_directory)
    {
        std::error_code parent_ec;
        const auto parent = fs_path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            g_xapi_last_error = 3;
        }
        else
        {
            g_xapi_last_error = 2;
        }
    }
    else if (ec == std::errc::directory_not_empty)
    {
        g_xapi_last_error = 145;
    }
    else if (ec == std::errc::permission_denied)
    {
        g_xapi_last_error = 5;
    }
    else if (ec == std::errc::invalid_argument)
    {
        g_xapi_last_error = 123;
    }
    else if (ec.value() != 0)
    {
        g_xapi_last_error = static_cast<u32>(ec.value());
    }
    else
    {
        g_xapi_last_error = 145;
    }
    return false;
}

// DAT_0032CA00, DAT_0032CA04, DAT_0032CA0C, DAT_0032CA08. Retail .data defaults
// for a zeroed RTL_HEAP_PARAMETERS block.
constexpr u32 kHeapSegmentReserve = 0x100000;
constexpr u32 kHeapSegmentCommit = 0x2000;
constexpr u32 kHeapDecommitFree = 0x1000;
constexpr u32 kHeapDecommitTotal = 0x10000;

// DAT_0054951D bit 3 inserts the 0x60c debug tail. DAT_0054951E bit 5 ORs
// 0x80 into the create flags. The retail image leaves both bytes zero.
u8 g_heap_nt_flag_1d = 0;
u8 g_heap_nt_flag_1e = 0;

#if defined(_WIN32)
struct HeapVm
{
    void*(__stdcall* alloc)(void* address, std::uintptr_t size, unsigned long type, unsigned long protect);
    int(__stdcall* free)(void* address, std::uintptr_t size, unsigned long type);
    std::uintptr_t(__stdcall* query)(const void* address, void* buffer, std::uintptr_t length);
};

struct HeapMemInfo
{
    void* base;
    void* allocation_base;
    unsigned long allocation_protect;
#if defined(_WIN64)
    unsigned short partition_id;
    unsigned short pad;
#endif
    std::uintptr_t region_size;
    unsigned long state;
    unsigned long protect;
    unsigned long type;
};

u32 heap_get_u32(u32 addr)
{
    u32 value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), 4);
    return value;
}

void heap_set_u32(u32 addr, u32 value)
{
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr)), &value, 4);
}

u16 heap_get_u16(u32 addr)
{
    u16 value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), 2);
    return value;
}

void heap_set_u16(u32 addr, u16 value)
{
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(addr)), &value, 2);
}

u8* heap_at(u32 addr)
{
    return reinterpret_cast<u8*>(static_cast<std::uintptr_t>(addr));
}

u32 heap_page_span(u32 end, u32 start)
{
    const i32 bytes = static_cast<i32>(end - start);
    return static_cast<u32>(bytes / static_cast<i32>(0x1000));
}

HeapVm heap_vm()
{
    HeapVm vm{};
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (kernel == nullptr)
    {
        return vm;
    }
    void* alloc_proc = GetProcAddress(kernel, "VirtualAlloc");
    void* free_proc = GetProcAddress(kernel, "VirtualFree");
    void* query_proc = GetProcAddress(kernel, "VirtualQuery");
    std::memcpy(&vm.alloc, &alloc_proc, sizeof(vm.alloc));
    std::memcpy(&vm.free, &free_proc, sizeof(vm.free));
    std::memcpy(&vm.query, &query_proc, sizeof(vm.query));
    return vm;
}

void* heap_reserve(HeapVm vm, u32 bytes)
{
    constexpr u32 kStep = 0x1000000;
    for (u32 hint = 0x20000000u; hint < 0x70000000u; hint += kStep)
    {
        if (static_cast<u64>(hint) + bytes > 0x100000000ull)
        {
            break;
        }
        void* block = vm.alloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(hint)), bytes, 0x2000u, 4u);
        if (block != nullptr)
        {
            return block;
        }
    }
    return nullptr;
}

// 0x0014D9BC. Link `units` of free 16-byte granules at `entry`. Sizes below
// 0x80 take the dedicated list at heap+0x180 and set the bitmap at heap+0x160.
// Larger sizes walk the list at heap+0x180 in ascending size order. A run
// above 0xff00 is split; 0xff01 keeps a 0x11-granule tail instead of a
// one-granule fragment. Bit 0x10 on the entry suppresses the following
// entry's previous-size store.
void heap_add_free(u32 heap_addr, u32 entry, u32 units)
{
    const u8 index = heap_at(entry)[4];
    const u8 saved_flags = heap_at(entry)[5];
    u16 previous = heap_get_u16(entry + 2);
    heap_set_u32(heap_addr + 0x30, heap_get_u32(heap_addr + 0x30) + units);

    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(index) * 4u + 0x60u);
    const u32 limit = segment == 0 ? 0u : heap_get_u32(segment + 0x2c);

    if (units == 0)
    {
        return;
    }

    while (units != 0)
    {
        u32 chunk = units;
        u8 flags = saved_flags;
        if (units > 0xff00u)
        {
            chunk = 0xff00u;
            if (units == 0xff01u)
            {
                chunk = 0xfeF0u;
            }
            flags = 0;
        }
        flags = static_cast<u8>(flags & 0xf8u);
        heap_set_u16(entry + 2, previous);
        heap_at(entry)[4] = index;
        heap_at(entry)[5] = flags;
        heap_set_u16(entry, static_cast<u16>(chunk));

        u32 before = 0;
        if (chunk >= 0x80u)
        {
            const u32 head = heap_addr + 0x180u;
            before = heap_get_u32(head);
            while (before != head)
            {
                if (static_cast<u16>(chunk) <= heap_get_u16(before - 8u))
                {
                    break;
                }
                before = heap_get_u32(before);
            }
        }
        else
        {
            before = heap_addr + 0x180u + chunk * 8u;
            if (heap_get_u32(before) == before)
            {
                u8* map = heap_at(heap_addr + 0x160u + (chunk >> 3));
                *map = static_cast<u8>(*map | static_cast<u8>(1u << (chunk & 7u)));
            }
        }

        const u32 blink = heap_get_u32(before + 4u);
        heap_set_u32(entry + 8u, before);
        heap_set_u32(entry + 0xcu, blink);
        heap_set_u32(blink, entry + 8u);
        heap_set_u32(before + 4u, entry + 8u);

        units -= chunk;
        entry += chunk << 4;
        previous = static_cast<u16>(chunk);
        if (limit != 0 && entry >= limit)
        {
            return;
        }
    }

    if ((saved_flags & 0x10u) == 0)
    {
        heap_set_u16(entry + 2u, previous);
    }
}

// 0x0014DEFF. `segment` sits on the 16-byte header boundary. The segment
// signature is 0xFFEEFFEE. `segment_index` is the byte stored at entry+4
// and the slot at heap+0x60. Granules past the segment header through the
// committed end become one free entry via 0x0014D9BC. Uncommitted pages are
// recorded on a descriptor popped from heap+0x4c. A commit that cannot
// cover the segment header returns false.
bool heap_init_segment(HeapVm vm, u32 heap_addr, u32 segment, u32 segment_index, u32 commit_base, u32 commit_end,
                       u32 reserve_end, u32 user_flag)
{
    const u32 first = (segment + 0x57u) & ~0xfu;
    const u32 header_units = (first - segment) >> 4;
    if (first + 0x10u >= commit_end)
    {
        if (first + 0x10u >= reserve_end)
        {
            return false;
        }
        u32 extra = first - commit_end + 0x10u;
        extra = (extra + 0xfffu) & ~0xfffu;
        if (vm.alloc(heap_at(commit_end), extra, 0x1000u, 4u) == nullptr)
        {
            return false;
        }
        commit_end += extra;
    }

    const u32 reserve_pages = heap_page_span(reserve_end, commit_base);
    const u32 uncommitted_pages = heap_page_span(reserve_end, commit_end);
    const u16 heap_units = heap_addr == commit_base ? heap_get_u16(heap_addr) : 0;

    const u8 index_byte = static_cast<u8>(segment_index);
    heap_set_u16(segment, static_cast<u16>(header_units));
    heap_set_u16(segment + 2u, heap_units);
    heap_at(segment)[4] = index_byte;
    heap_at(segment)[5] = 1;
    heap_set_u32(segment + 0x10u, 0xffeeffeeu);
    heap_set_u32(segment + 0x14u, user_flag);
    heap_set_u32(segment + 0x18u, heap_addr);
    heap_set_u32(segment + 0x20u, commit_base);
    heap_set_u32(segment + 0x24u, reserve_pages);
    heap_set_u32(segment + 0x28u, first);
    heap_set_u32(segment + 0x2cu, commit_base + (reserve_pages << 12));
    heap_at(segment)[5] = 1;
    heap_set_u32(segment + 0x30u, uncommitted_pages);

    if (uncommitted_pages != 0)
    {
        const u32 node = heap_get_u32(heap_addr + 0x4cu);
        if (node != 0)
        {
            heap_set_u32(heap_addr + 0x4cu, heap_get_u32(node));
            const u32 bytes = uncommitted_pages << 12;
            heap_set_u32(node + 4u, commit_end);
            heap_set_u32(node + 8u, bytes);
            heap_set_u32(node, heap_get_u32(segment + 0x38u));
            heap_set_u32(segment + 0x38u, node);
            heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) + 1u);
            if (bytes >= heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, bytes);
            }
        }
    }

    heap_set_u32(heap_addr + 0x60u + static_cast<u32>(index_byte) * 4u, segment);
    heap_at(first)[5] = 0x10;
    heap_set_u32(segment + 0x40u, first);
    heap_set_u16(first + 2u, heap_get_u16(segment));
    heap_at(first)[4] = index_byte;
    const i32 free_bytes = static_cast<i32>(commit_end - first);
    const u32 free_units = static_cast<u32>(free_bytes >> 4);
    heap_add_free(heap_addr, first, free_units);
    return true;
}

// Host stand-in for the critical section at heap+0x580. The embedded
// Xbox RTL_CRITICAL_SECTION is 0x1c bytes and is not a native x64
// CRITICAL_SECTION. Heaps that share a lock pointer share one mutex.
std::recursive_mutex* heap_lock_mutex(u32 lock_addr)
{
    static std::mutex gate;
    static std::unordered_map<u32, std::unique_ptr<std::recursive_mutex>> locks;
    std::lock_guard<std::mutex> hold(gate);
    std::unique_ptr<std::recursive_mutex>& slot = locks[lock_addr];
    if (!slot)
    {
        slot = std::make_unique<std::recursive_mutex>();
    }
    return slot.get();
}

struct HeapSerialize
{
    std::recursive_mutex* mu = nullptr;

    HeapSerialize(u32 heap_addr, u32 flags)
    {
        if ((flags & 1u) != 0)
        {
            return;
        }
        const u32 lock = heap_get_u32(heap_addr + 0x580u);
        if (lock == 0)
        {
            return;
        }
        mu = heap_lock_mutex(lock);
        mu->lock();
    }

    void unlock()
    {
        if (mu != nullptr)
        {
            mu->unlock();
            mu = nullptr;
        }
    }

    ~HeapSerialize()
    {
        unlock();
    }
};

// 0x001516A0. Bit scan forward of the bitmap word.
u32 heap_lowest_bit(u32 mask)
{
    return static_cast<u32>(std::countr_zero(mask));
}

// Unlink the free entry whose list node sits at entry+8. A small list that
// becomes empty clears its bit in the map at heap+0x160.
void heap_unlink_free(u32 heap_addr, u32 entry)
{
    const u32 flink = heap_get_u32(entry + 8u);
    const u32 blink = heap_get_u32(entry + 0xcu);
    heap_set_u32(blink, flink);
    heap_set_u32(flink + 4u, blink);
    const u16 units = heap_get_u16(entry);
    if (flink == blink && units < 0x80u)
    {
        u8* map = heap_at(heap_addr + 0x160u + (static_cast<u32>(units) >> 3));
        *map = static_cast<u8>(*map ^ static_cast<u8>(1u << (units & 7u)));
    }
}

// 0x0014D717 paints a free block whose flags include 0x04 with 0xFEEEFEEE
// from entry+0x18. Flag 0x02 shortens that run by four bytes.
void heap_paint_free(u32 entry)
{
    const u8 flags = heap_at(entry)[5];
    if ((flags & 4u) == 0)
    {
        return;
    }
    const u32 block_bytes = static_cast<u32>(heap_get_u16(entry)) << 4;
    if (block_bytes <= 0x18u)
    {
        return;
    }
    u32 bytes = block_bytes - 0x18u;
    if ((flags & 2u) != 0 && bytes > 4u)
    {
        bytes -= 4u;
    }
    u32 at = entry + 0x18u;
    const u32 end = at + (bytes & ~3u);
    while (at < end)
    {
        heap_set_u32(at, 0xfeeefeeeu);
        at += 4u;
    }
}

// Unlink a free entry, paint it when flag 0x04 is set, and drop its size
// from heap+0x30.
void heap_detach_free(u32 heap_addr, u32 entry)
{
    const u16 units = heap_get_u16(entry);
    heap_paint_free(entry);
    heap_unlink_free(heap_addr, entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - units);
}

// 0x0014D717. Absorb adjacent free entries into `entry` while the combined
// size stays at or below 0xff00 granules. `*units_io` is the running size.
// `remove` unlinks `entry` itself first; RtlFreeHeap passes false because
// the block is still busy. The returned entry may sit in front of the
// original. Flag 0x10 is the only flag kept, and it updates segment+0x40.
u32 heap_coalesce(u32 heap_addr, u32 entry, u32* units_io, bool remove)
{
    u32 units = *units_io;
    const u32 prev_bytes = static_cast<u32>(heap_get_u16(entry + 2u)) << 4;
    const u32 prev = entry - prev_bytes;
    if (prev != entry && (heap_at(prev)[5] & 1u) == 0)
    {
        const u32 combined = static_cast<u32>(heap_get_u16(prev)) + units;
        if (combined <= 0xff00u)
        {
            if (remove)
            {
                heap_detach_free(heap_addr, entry);
                remove = false;
            }
            const u8 last = static_cast<u8>(heap_at(entry)[5] & 0x10u);
            const u16 prev_units = heap_get_u16(prev);
            heap_detach_free(heap_addr, prev);
            heap_at(prev)[5] = last;
            if ((last & 0x10u) != 0)
            {
                const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(prev)[4]) * 4u + 0x60u);
                if (segment != 0)
                {
                    heap_set_u32(segment + 0x40u, prev);
                }
            }
            units += prev_units;
            *units_io = units;
            heap_set_u16(prev, static_cast<u16>(units));
            if ((heap_at(prev)[5] & 0x10u) == 0)
            {
                heap_set_u16(prev + (units << 4) + 2u, static_cast<u16>(units));
            }
            entry = prev;
        }
    }

    if ((heap_at(entry)[5] & 0x10u) == 0)
    {
        const u32 next = entry + (units << 4);
        if ((heap_at(next)[5] & 1u) == 0)
        {
            const u32 combined = static_cast<u32>(heap_get_u16(next)) + units;
            if (combined <= 0xff00u)
            {
                if (remove)
                {
                    heap_detach_free(heap_addr, entry);
                }
                const u8 last = static_cast<u8>(heap_at(next)[5] & 0x10u);
                const u16 next_units = heap_get_u16(next);
                heap_at(entry)[5] = last;
                if ((last & 0x10u) != 0)
                {
                    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(entry)[4]) * 4u + 0x60u);
                    if (segment != 0)
                    {
                        heap_set_u32(segment + 0x40u, entry);
                    }
                }
                heap_detach_free(heap_addr, next);
                units += next_units;
                *units_io = units;
                heap_set_u16(entry, static_cast<u16>(units));
                if ((heap_at(entry)[5] & 0x10u) == 0)
                {
                    heap_set_u16(entry + (units << 4) + 2u, static_cast<u16>(units));
                }
            }
        }
    }
    return entry;
}

// Insert a free entry that already has its size, flags, and segment index.
// Sizes below 0x80 use the dedicated list and set the bitmap when the list
// was empty. Larger sizes walk heap+0x180 in ascending order.
void heap_insert_free(u32 heap_addr, u32 entry)
{
    const u16 units = heap_get_u16(entry);
    u32 before = 0;
    if (units >= 0x80u)
    {
        const u32 head = heap_addr + 0x180u;
        before = heap_get_u32(head);
        while (before != head)
        {
            if (units <= heap_get_u16(before - 8u))
            {
                break;
            }
            before = heap_get_u32(before);
        }
    }
    else
    {
        before = heap_addr + 0x180u + static_cast<u32>(units) * 8u;
        if (heap_get_u32(before) == before)
        {
            u8* map = heap_at(heap_addr + 0x160u + (static_cast<u32>(units) >> 3));
            *map = static_cast<u8>(*map | static_cast<u8>(1u << (units & 7u)));
        }
    }

    const u32 blink = heap_get_u32(before + 4u);
    heap_set_u32(entry + 8u, before);
    heap_set_u32(entry + 0xcu, blink);
    heap_set_u32(blink, entry + 8u);
    heap_set_u32(before + 4u, entry + 8u);
}

// heap_insert_free, then heap+0x30 gains the entry size.
void heap_link_free_entry(u32 heap_addr, u32 entry)
{
    heap_insert_free(heap_addr, entry);
    const u16 units = heap_get_u16(entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) + units);
}

// 0x0014D4D8. Push an uncommitted-range node back onto heap+0x4c.
void heap_release_uc_node(u32 segment, u32 node)
{
    const u32 heap_addr = heap_get_u32(segment + 0x18u);
    heap_set_u32(node, heap_get_u32(heap_addr + 0x4cu));
    heap_set_u32(heap_addr + 0x4cu, node);
    heap_set_u32(node + 4u, 0);
    heap_set_u32(node + 8u, 0);
}

// 0x0014D3B8. Pop an uncommitted-range node for `segment`. An empty list at
// heap+0x4c commits another page of the zone at heap+0x48, or reserves a
// fresh 64KB zone and commits its first page. Descriptors are 0x10 bytes
// and start at zone+0x10.
u32 heap_ucr_pop(HeapVm vm, u32 segment)
{
    const u32 heap_addr = heap_get_u32(segment + 0x18u);
    if (heap_get_u32(heap_addr + 0x4cu) == 0)
    {
        u32 zone = heap_get_u32(heap_addr + 0x48u);
        u32 desc = 0;
        if (zone != 0 && heap_get_u32(zone + 8u) != heap_get_u32(zone + 4u))
        {
            const u32 addr = zone + heap_get_u32(zone + 8u);
            if (vm.alloc == nullptr || vm.alloc(heap_at(addr), 0x1000u, 0x1000u, 4u) == nullptr)
            {
                return 0;
            }
            heap_set_u32(zone + 8u, heap_get_u32(zone + 8u) + 0x1000u);
            desc = addr;
        }
        else
        {
            void* reserved = heap_reserve(vm, 0x10000u);
            if (reserved == nullptr)
            {
                return 0;
            }
            zone = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
            if (vm.alloc == nullptr || vm.alloc(heap_at(zone), 0x1000u, 0x1000u, 4u) == nullptr)
            {
                if (vm.free != nullptr)
                {
                    vm.free(heap_at(zone), 0, 0x8000u);
                }
                return 0;
            }
            heap_set_u32(zone, heap_get_u32(heap_addr + 0x48u));
            heap_set_u32(heap_addr + 0x48u, zone);
            heap_set_u32(zone + 4u, 0x10000u);
            heap_set_u32(zone + 8u, 0x1000u);
            desc = zone + 0x10u;
        }
        const u32 end = zone + heap_get_u32(zone + 8u);
        u32 tail = heap_addr + 0x4cu;
        for (; desc < end; desc += 0x10u)
        {
            heap_set_u32(tail, desc);
            tail = desc;
        }
        heap_set_u32(tail, 0);
    }
    const u32 node = heap_get_u32(heap_addr + 0x4cu);
    if (node == 0)
    {
        return 0;
    }
    heap_set_u32(heap_addr + 0x4cu, heap_get_u32(node));
    return node;
}

// 0x0014D4F9. Record `size` bytes of uncommitted memory at `address` on
// segment+0x38, coalescing with a neighbor that touches either edge.
// segment+0x34 counts the nodes. segment+0x1c tracks the largest run.
void heap_insert_ucr(HeapVm vm, u32 segment, u32 address, u32 size)
{
    u32 slot = segment + 0x38u;
    for (;;)
    {
        const u32 node = heap_get_u32(slot);
        if (node == 0)
        {
            break;
        }
        const u32 node_at = heap_get_u32(node + 4u);
        if (node_at > address)
        {
            if (address + size == node_at)
            {
                const u32 grown = heap_get_u32(node + 8u) + size;
                heap_set_u32(node + 8u, grown);
                heap_set_u32(node + 4u, address);
                if (grown > heap_get_u32(segment + 0x1cu))
                {
                    heap_set_u32(segment + 0x1cu, grown);
                }
                return;
            }
            break;
        }
        const u32 node_end = node_at + heap_get_u32(node + 8u);
        if (node_end == address)
        {
            size += heap_get_u32(node + 8u);
            address = node_at;
            heap_set_u32(slot, heap_get_u32(node));
            heap_release_uc_node(segment, node);
            heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) - 1u);
            if (size > heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, size);
            }
            continue;
        }
        slot = node;
    }

    const u32 fresh = heap_ucr_pop(vm, segment);
    if (fresh == 0)
    {
        return;
    }
    heap_set_u32(fresh + 4u, address);
    heap_set_u32(fresh + 8u, size);
    heap_set_u32(fresh, heap_get_u32(slot));
    heap_set_u32(slot, fresh);
    heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) + 1u);
    if (size >= heap_get_u32(segment + 0x1cu))
    {
        heap_set_u32(segment + 0x1cu, size);
    }
}

// 0x0014E1A4. Decommit the page-aligned middle of a free run. A one-granule
// edge keeps the whole page (0x101 granules). A commit routine at heap+0x584
// skips the decommit and returns the run through 0x0014D9BC, as does a
// failed release. The committed prefix and suffix go back on the free lists.
// Prefix sizes are already counted in heap+0x30; suffix sizes are counted
// by the list insert.
void heap_decommit(HeapVm vm, u32 heap_addr, u32 entry, u32 units)
{
    if (heap_get_u32(heap_addr + 0x584u) != 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    u32 behind = 0;
    const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(heap_at(entry)[4]) * 4u + 0x60u);
    if (segment == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }
    u32 page_start = (entry + 0xfffu) & ~0xfffu;
    u32 prefix = (page_start - entry) >> 4;
    if (static_cast<u16>(prefix) == 1u)
    {
        page_start += 0x1000u;
        prefix = 0x101u;
    }
    else
    {
        const u16 prev_units = heap_get_u16(entry + 2u);
        if (prev_units != 0 && page_start == entry)
        {
            behind = entry - (static_cast<u32>(prev_units) << 4);
        }
    }

    const u32 block_end = entry + (units << 4);
    u32 page_end = block_end & ~0xfffu;
    u32 suffix = (block_end - page_end) >> 4;
    u32 follower = 0;
    if (static_cast<u16>(suffix) == 1u)
    {
        page_end -= 0x1000u;
        suffix = 0x101u;
    }
    else if (static_cast<u16>(suffix) == 0 && (heap_at(entry)[5] & 0x10u) == 0)
    {
        follower = block_end;
    }

    const u32 suffix_units = static_cast<u32>(static_cast<u16>(suffix));
    const u32 suffix_bytes = suffix_units << 4;
    const u32 suffix_at = block_end - suffix_bytes;
    u32 hole = 0;
    if (page_end > page_start)
    {
        hole = page_end - page_start;
    }
    if (hole == 0 || vm.free == nullptr)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    const u32 node = heap_ucr_pop(vm, segment);
    if (node == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }
    const int released = vm.free(heap_at(page_start), hole, 0x4000u);
    heap_release_uc_node(segment, node);
    if (released == 0)
    {
        heap_add_free(heap_addr, entry, units);
        return;
    }

    heap_insert_ucr(vm, segment, page_start, hole);
    heap_set_u32(segment + 0x30u, heap_get_u32(segment + 0x30u) + (hole >> 12));

    if (static_cast<u16>(prefix) != 0)
    {
        const u32 prefix_units = static_cast<u32>(static_cast<u16>(prefix));
        heap_at(entry)[5] = 0x10;
        heap_set_u16(entry, static_cast<u16>(prefix));
        heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) + prefix_units);
        heap_set_u32(segment + 0x40u, entry);
        heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0xf8u);
        heap_insert_free(heap_addr, entry);
    }
    else if (behind != 0)
    {
        heap_at(behind)[5] = static_cast<u8>(heap_at(behind)[5] | 0x10u);
        heap_set_u32(segment + 0x40u, behind);
    }
    else
    {
        const u32 last = heap_get_u32(segment + 0x40u);
        if (last >= page_start && last < page_start + hole)
        {
            heap_set_u32(segment + 0x40u, heap_get_u32(segment + 0x28u));
        }
    }

    if (static_cast<u16>(suffix) != 0)
    {
        heap_set_u16(suffix_at + 2u, 0);
        heap_at(suffix_at)[4] = heap_at(segment)[4];
        heap_at(suffix_at)[5] = 0;
        heap_set_u16(suffix_at, static_cast<u16>(suffix));
        heap_set_u16(suffix_at + suffix_bytes + 2u, static_cast<u16>(suffix));
        heap_at(suffix_at)[5] = static_cast<u8>(heap_at(suffix_at)[5] & 0xf8u);
        heap_link_free_entry(heap_addr, suffix_at);
    }
    else if (follower != 0)
    {
        heap_set_u16(follower + 2u, 0);
    }
}

void* heap_reserve_hint(HeapVm vm, u32 bytes, bool top_down)
{
    constexpr u32 kStep = 0x1000000;
    if (top_down)
    {
        for (u32 hint = 0x70000000u - kStep; hint >= 0x20000000u; hint -= kStep)
        {
            if (static_cast<u64>(hint) + bytes > 0x100000000ull)
            {
                continue;
            }
            void* block = vm.alloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(hint)), bytes, 0x2000u, 4u);
            if (block != nullptr)
            {
                return block;
            }
            if (hint < 0x20000000u + kStep)
            {
                break;
            }
        }
        return nullptr;
    }
    return heap_reserve(vm, bytes);
}

// 0x0014D58C. Commit `*size_bytes` from an uncommitted descriptor on
// `segment`. `prefer` must match the descriptor address, or be 0. Returns
// the new heap entry at the committed address. The entry is not on a free
// list yet.
u32 heap_commit_pages(HeapVm vm, u32 heap_addr, u32 segment, u32* size_bytes, u32 prefer)
{
    (void)heap_addr;
    u32 prev = 0;
    u32 link = segment + 0x38u;
    u32 node = heap_get_u32(link);
    if (node == 0 || size_bytes == nullptr)
    {
        return 0;
    }
    const u32 need = *size_bytes;
    for (;;)
    {
        const u32 have = heap_get_u32(node + 8u);
        const u32 at = heap_get_u32(node + 4u);
        if (have >= need && (prefer == 0 || at == prefer))
        {
            break;
        }
        prev = node;
        link = node;
        node = heap_get_u32(node);
        if (node == 0)
        {
            return 0;
        }
    }

    const u32 address = heap_get_u32(node + 4u);
    if (vm.alloc(heap_at(address), need, 0x1000u, 4u) == nullptr)
    {
        return 0;
    }

    const u32 pages = need >> 12;
    heap_set_u32(segment + 0x30u, heap_get_u32(segment + 0x30u) - pages);
    if (heap_get_u32(segment + 0x1cu) == heap_get_u32(node + 8u))
    {
        heap_set_u32(segment + 0x1cu, 0);
    }

    const u32 hole = address;
    u32 preceding = 0;
    const u32 last = heap_get_u32(segment + 0x40u);
    if (last != 0 && (heap_at(last)[5] & 0x10u) != 0)
    {
        const u32 end = last + static_cast<u32>(heap_get_u16(last)) * 16u;
        if (end == hole)
        {
            preceding = last;
        }
    }
    if (preceding == 0)
    {
        u32 cursor = prev == 0 ? heap_get_u32(segment + 0x28u)
                               : heap_get_u32(prev + 4u) + heap_get_u32(prev + 8u);
        if ((heap_at(cursor)[5] & 0x10u) != 0)
        {
            preceding = cursor;
        }
        else
        {
            const u32 limit = heap_get_u32(segment + 0x2cu);
            u32 saved = cursor;
            for (;;)
            {
                const u16 step_units = heap_get_u16(cursor);
                if (step_units == 0)
                {
                    return 0;
                }
                saved = cursor;
                cursor += static_cast<u32>(step_units) * 16u;
                if (cursor >= limit || cursor == hole || heap_get_u16(cursor) == 0)
                {
                    if (cursor != hole)
                    {
                        return 0;
                    }
                    preceding = saved;
                    break;
                }
                if ((heap_at(cursor)[5] & 0x10u) != 0)
                {
                    preceding = cursor;
                    break;
                }
            }
        }
    }

    heap_at(preceding)[5] = static_cast<u8>(heap_at(preceding)[5] & 0xefu);
    heap_set_u32(node + 4u, heap_get_u32(node + 4u) + need);
    const u32 left = heap_get_u32(node + 8u) - need;
    heap_set_u32(node + 8u, left);

    u32 entry = address;
    if (left == 0)
    {
        const u32 end_addr = heap_get_u32(node + 4u);
        if (end_addr == heap_get_u32(segment + 0x2cu))
        {
            heap_at(entry)[5] = 0x10;
            heap_set_u32(segment + 0x40u, entry);
        }
        else
        {
            heap_at(entry)[5] = 0;
            heap_set_u32(segment + 0x40u, heap_get_u32(segment + 0x28u));
        }
        heap_set_u32(link, heap_get_u32(node));
        heap_release_uc_node(segment, node);
        heap_set_u32(segment + 0x34u, heap_get_u32(segment + 0x34u) - 1u);
    }
    else
    {
        heap_at(entry)[5] = 0x10;
        heap_set_u32(segment + 0x40u, entry);
    }

    heap_at(entry)[4] = heap_at(preceding)[4];
    const u16 units = static_cast<u16>(need >> 4);
    heap_set_u16(entry, units);
    heap_set_u16(entry + 2u, heap_get_u16(preceding));
    if ((heap_at(entry)[5] & 0x10u) == 0)
    {
        const u32 next = entry + static_cast<u32>(units) * 16u;
        heap_set_u16(next + 2u, units);
    }

    if (heap_get_u32(segment + 0x1cu) == 0)
    {
        for (u32 scan = heap_get_u32(segment + 0x38u); scan != 0; scan = heap_get_u32(scan))
        {
            const u32 bytes = heap_get_u32(scan + 8u);
            if (bytes >= heap_get_u32(segment + 0x1cu))
            {
                heap_set_u32(segment + 0x1cu, bytes);
            }
        }
    }
    return entry;
}

// 0x0014E01D. Commit `bytes` (page rounded by the caller) inside an existing
// segment, or reserve a new one when the heap is growable. The returned
// entry is already on a free list. Null means the heap could not grow.
u32 heap_extend(HeapVm vm, u32 heap_addr, u32 bytes)
{
    const u32 pages = (bytes + 0xfffu) >> 12;
    u32 rounded = pages << 12;
    u8 empty_index = 0x40;
    for (u8 index = 0; index < 0x40u; ++index)
    {
        const u32 segment = heap_get_u32(heap_addr + static_cast<u32>(index) * 4u + 0x60u);
        if (segment == 0)
        {
            if (empty_index == 0x40u)
            {
                empty_index = index;
            }
            continue;
        }
        if (pages > heap_get_u32(segment + 0x30u) || rounded > heap_get_u32(segment + 0x1cu))
        {
            continue;
        }
        u32 got = rounded;
        const u32 entry = heap_commit_pages(vm, heap_addr, segment, &got, 0);
        if (entry != 0)
        {
            heap_add_free(heap_addr, entry, got >> 4);
            return entry;
        }
    }

    if (empty_index == 0x40u || (heap_at(heap_addr)[0x14] & 2u) == 0)
    {
        return 0;
    }

    const u32 minimum = bytes + 0x1000u;
    u32 reserve = minimum;
    const u32 segment_reserve = heap_get_u32(heap_addr + 0x20u);
    if (reserve < segment_reserve)
    {
        reserve = segment_reserve;
    }

    void* reserved = nullptr;
    for (;;)
    {
        reserved = heap_reserve_hint(vm, reserve, false);
        if (reserved != nullptr)
        {
            break;
        }
        if (reserve == minimum)
        {
            return 0;
        }
        reserve >>= 1;
        if (reserve < minimum)
        {
            reserve = minimum;
        }
    }

    const u32 base = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
    heap_set_u32(heap_addr + 0x20u, heap_get_u32(heap_addr + 0x20u) + reserve);

    u32 commit = minimum;
    const u32 segment_commit = heap_get_u32(heap_addr + 0x24u);
    if (commit < segment_commit)
    {
        commit = segment_commit;
    }
    if (commit > reserve)
    {
        commit = reserve;
    }
    if (vm.alloc(heap_at(base), commit, 0x1000u, 4u) == nullptr)
    {
        vm.free(heap_at(base), 0, 0x8000u);
        return 0;
    }
    if (!heap_init_segment(vm, heap_addr, base, empty_index, base, base + commit, base + reserve, 0u))
    {
        vm.free(heap_at(base), 0, 0x8000u);
        return 0;
    }
    return heap_get_u32(base + 0x28u);
}

// Mark `entry` busy for `units` granules and return any leftover to the
// free lists. A one-granule leftover is absorbed. `kept_flags` is the free
// entry's flags before it is marked busy.
void heap_carve(u32 heap_addr, u32 entry, u32 units, u32 rounded, u32 asked)
{
    const u8 kept = heap_at(entry)[5];
    const u16 old_units = heap_get_u16(entry);
    heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - old_units);
    heap_at(entry)[5] = 1;
    u32 extra = static_cast<u32>(old_units) - units;
    heap_set_u16(entry, static_cast<u16>(units));
    heap_at(entry)[6] = static_cast<u8>(rounded - asked);
    heap_at(entry)[7] = 0;

    u8 tail_flags = kept;
    if (extra == 0)
    {
        if ((tail_flags & 0x10u) != 0)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] | 0x10u);
        }
        return;
    }
    if (extra == 1u)
    {
        heap_set_u16(entry, static_cast<u16>(units + 1u));
        heap_at(entry)[6] = static_cast<u8>(heap_at(entry)[6] + 0x10u);
        if ((tail_flags & 0x10u) != 0)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] | 0x10u);
        }
        return;
    }

    const u32 rem = entry + (units << 4);
    heap_at(rem)[5] = kept;
    heap_set_u16(rem + 2u, static_cast<u16>(units));
    heap_at(rem)[4] = heap_at(entry)[4];
    heap_set_u16(rem, static_cast<u16>(extra));

    if ((kept & 0x10u) == 0)
    {
        const u32 next = rem + (extra << 4);
        if ((heap_at(next)[5] & 1u) != 0)
        {
            heap_set_u16(next + 2u, static_cast<u16>(extra));
            heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
            heap_link_free_entry(heap_addr, rem);
        }
        else
        {
            heap_at(rem)[5] = heap_at(next)[5];
            const u16 next_units = heap_get_u16(next);
            heap_unlink_free(heap_addr, next);
            heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - next_units);
            extra += next_units;
            if (extra < 0xff01u)
            {
                heap_set_u16(rem, static_cast<u16>(extra));
                if ((heap_at(rem)[5] & 0x10u) == 0)
                {
                    heap_set_u16(rem + (extra << 4) + 2u, static_cast<u16>(extra));
                }
                heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
                heap_link_free_entry(heap_addr, rem);
            }
            else
            {
                heap_add_free(heap_addr, rem, extra);
            }
        }
    }
    else
    {
        heap_at(rem)[5] = static_cast<u8>(heap_at(rem)[5] & 0x10u);
        heap_link_free_entry(heap_addr, rem);
    }

    if ((heap_at(rem)[5] & 0x10u) != 0)
    {
        const u32 segment = heap_get_u32(heap_addr + 0x60u + static_cast<u32>(heap_at(rem)[4]) * 4u);
        if (segment != 0)
        {
            heap_set_u32(segment + 0x40u, rem);
        }
    }
}

// First free block on the large list at heap+0x180 whose size is at least
// `units`. The list is ordered, so the tail is the largest.
u32 heap_take_large(u32 heap_addr, u32 units)
{
    const u32 head = heap_addr + 0x180u;
    const u32 last_link = heap_get_u32(head + 4u);
    if (last_link == head)
    {
        return 0;
    }
    const u32 last_entry = last_link - 8u;
    if (static_cast<u32>(heap_get_u16(last_entry)) < units)
    {
        return 0;
    }
    for (u32 link = heap_get_u32(head); link != head; link = heap_get_u32(link))
    {
        const u32 entry = link - 8u;
        if (units <= heap_get_u16(entry))
        {
            heap_unlink_free(heap_addr, entry);
            return entry;
        }
    }
    return 0;
}

// Smallest dedicated list at or above `units` (units < 0x80) whose bitmap
// bit is set. Returns the unlinked tail entry.
u32 heap_take_bitmap(u32 heap_addr, u32 units)
{
    const u32 word = units >> 5;
    u32 mask = ~((1u << (units & 31u)) - 1u) & heap_get_u32(heap_addr + 0x160u + word * 4u);
    u32 next = heap_addr + 0x160u + (word + 1u) * 4u;
    u32 base = 0;

    if (word == 0)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            next += 4u;
            if (mask == 0)
            {
                mask = heap_get_u32(next);
                next += 4u;
                if (mask == 0)
                {
                    mask = heap_get_u32(next);
                    if (mask == 0)
                    {
                        return 0;
                    }
                    base = heap_addr + 0x480u;
                }
                else
                {
                    base = heap_addr + 0x380u;
                }
            }
            else
            {
                base = heap_addr + 0x280u;
            }
        }
        else
        {
            base = heap_addr + 0x180u;
        }
    }
    else if (word == 1)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            next += 4u;
            if (mask == 0)
            {
                mask = heap_get_u32(next);
                if (mask == 0)
                {
                    return 0;
                }
                base = heap_addr + 0x480u;
            }
            else
            {
                base = heap_addr + 0x380u;
            }
        }
        else
        {
            base = heap_addr + 0x280u;
        }
    }
    else if (word == 2)
    {
        if (mask == 0)
        {
            mask = heap_get_u32(next);
            if (mask == 0)
            {
                return 0;
            }
            base = heap_addr + 0x480u;
        }
        else
        {
            base = heap_addr + 0x380u;
        }
    }
    else if (word == 3)
    {
        if (mask == 0)
        {
            return 0;
        }
        base = heap_addr + 0x480u;
    }
    else
    {
        return 0;
    }

    const u32 list = base + heap_lowest_bit(mask) * 8u;
    const u32 entry = heap_get_u32(list + 4u) - 8u;
    heap_unlink_free(heap_addr, entry);
    return entry;
}

void* heap_virtual_alloc(HeapVm vm, u32 heap_addr, u32 flags, u32 asked, u32 rounded)
{
    u32 bytes = (rounded + 0x20u + 0xfffu) & ~0xfffu;
    const bool top_down = (flags & 8u) == 0;
    void* reserved = heap_reserve_hint(vm, bytes, top_down);
    if (reserved == nullptr)
    {
        return nullptr;
    }
    if (vm.alloc(reserved, bytes, 0x1000u, 4u) == nullptr)
    {
        vm.free(reserved, 0, 0x8000u);
        return nullptr;
    }

    const u32 base = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
    std::memset(heap_at(base), 0, 0x30);
    heap_set_u16(base + 0x20u, static_cast<u16>(bytes - asked));
    heap_at(base)[0x25] = 0x0b;
    heap_set_u32(base + 0x18u, bytes);
    heap_set_u32(base + 0x1cu, bytes);

    const u32 head = heap_addr + 0x58u;
    const u32 blink = heap_get_u32(head + 4u);
    heap_set_u32(base, head);
    heap_set_u32(base + 4u, blink);
    heap_set_u32(blink, base);
    heap_set_u32(head + 4u, base);
    return heap_at(base + 0x30u);
}
#endif

void* fn_14e44e(u32 flags, void* heap_base, u32 reserve_size, u32 commit_size, void* lock, const void* parameters)
{
    // Xbox 0x0014E44E
    // RtlCreateHeap, stdcall, six arguments. 0x0014D311 (HeapCreate) ORs
    // the caller flags with 0x1000, forces HEAP_GROWABLE when the commit is
    // 0, and passes a null base and lock. 0x0015105A creates the process
    // heap with flags 2, reserve from XBE+0x134 (0x100000), commit from
    // XBE+0x138 (0x1000), and a zeroed 0x30-byte parameter block.
    // A parameter pointer is copied only when its first dword is 0x30. A
    // fault on that copy returns null. Zero fields take the .data defaults.
    // Commit and reserve round up to pages; a zero commit becomes 0x1000
    // and a zero reserve becomes 0x40000, or the commit rounded to 64KB
    // when the commit was already non-zero. HEAP_NO_SERIALIZE with a lock
    // returns null. A null lock embeds a critical section and grows the
    // header from 0x590 to 0x5ac. A caller lock sets flag 0x80000000.
    // A null base reserves then commits. A caller base with a commit
    // routine uses InitialCommit and InitialReserve and refuses
    // HEAP_GROWABLE. Otherwise the base has to be the start of a region
    // that is not free. The header signature is 0xEEFFEEFF. Eight
    // 16-byte descriptor nodes chain from +0x4c, 128 free lists sit at
    // +0x180, and the lock pointer is +0x580. 0x0014DEFF then builds
    // segment 0 and the first free entry. Failure of that step returns
    // null and leaves a reservation in place. The port keeps the image in
    // the low 4GB so the 32-bit links stay intact.
#if !defined(_WIN32)
    (void)flags;
    (void)heap_base;
    (void)reserve_size;
    (void)commit_size;
    (void)lock;
    (void)parameters;
    return nullptr;
#else
#if defined(_WIN64)
    static_assert(sizeof(HeapMemInfo) == 48u);
#else
    static_assert(sizeof(HeapMemInfo) == 28u);
#endif

    HeapVm vm = heap_vm();
    if (vm.alloc == nullptr || vm.free == nullptr || vm.query == nullptr)
    {
        return nullptr;
    }

    u32 segment_reserve = 0;
    u32 segment_commit = 0;
    u32 decommit_free = 0;
    u32 decommit_total = 0;
    u32 maximum_allocation = 0;
    u32 virtual_threshold = 0;
    u32 initial_commit = 0;
    u32 initial_reserve = 0;
    u32 commit_routine = 0;
    if (parameters != nullptr)
    {
        u32 length = 0;
        std::memcpy(&length, parameters, 4);
        if (length == 0x30u)
        {
            u8 raw[0x30]{};
            std::memcpy(raw, parameters, 0x30);
            std::memcpy(&segment_reserve, raw + 0x04, 4);
            std::memcpy(&segment_commit, raw + 0x08, 4);
            std::memcpy(&decommit_free, raw + 0x0c, 4);
            std::memcpy(&decommit_total, raw + 0x10, 4);
            std::memcpy(&maximum_allocation, raw + 0x14, 4);
            std::memcpy(&virtual_threshold, raw + 0x18, 4);
            std::memcpy(&initial_commit, raw + 0x1c, 4);
            std::memcpy(&initial_reserve, raw + 0x20, 4);
            std::memcpy(&commit_routine, raw + 0x24, 4);
        }
    }

    if ((g_heap_nt_flag_1e & 0x20u) != 0)
    {
        flags |= 0x80u;
    }
    if (segment_reserve == 0)
    {
        segment_reserve = kHeapSegmentReserve;
    }
    if (segment_commit == 0)
    {
        segment_commit = kHeapSegmentCommit;
    }
    if (decommit_free == 0)
    {
        decommit_free = kHeapDecommitFree;
    }
    if (decommit_total == 0)
    {
        decommit_total = kHeapDecommitTotal;
    }
    if (maximum_allocation == 0)
    {
        maximum_allocation = 0x7ffdefffu;
    }
    if (virtual_threshold == 0 || virtual_threshold > 0xff000u)
    {
        virtual_threshold = 0xff000u;
    }

    if (commit_size == 0)
    {
        commit_size = 0x1000u;
        if (reserve_size == 0)
        {
            reserve_size = 0x40000u;
        }
        else
        {
            reserve_size = (reserve_size + 0xfffu) & 0xfffff000u;
        }
    }
    else
    {
        commit_size = (commit_size + 0xfffu) & 0xfffff000u;
        if (reserve_size == 0)
        {
            reserve_size = (commit_size + 0xffffu) & 0xffff0000u;
        }
        else
        {
            reserve_size = (reserve_size + 0xfffu) & 0xfffff000u;
        }
    }

    u32 header_bytes = 0x590u;
    bool embed_lock = false;
    if ((flags & 1u) == 0)
    {
        if (lock == nullptr)
        {
            header_bytes = 0x5acu;
            embed_lock = true;
        }
        else
        {
            flags |= 0x80000000u;
            if (reinterpret_cast<std::uintptr_t>(lock) > 0xffffffffu)
            {
                return nullptr;
            }
        }
    }
    else if (lock != nullptr)
    {
        return nullptr;
    }

    const bool caller_base = heap_base != nullptr;
    u32 base_addr = 0;
    u32 commit_base = 0;
    u32 commit_end = 0;
    u32 user_flag = 0;
    if (!caller_base)
    {
        if (commit_routine != 0)
        {
            return nullptr;
        }
        void* reserved = heap_reserve(vm, reserve_size);
        if (reserved == nullptr)
        {
            return nullptr;
        }
        base_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(reserved));
        commit_base = base_addr;
        commit_end = base_addr;
        user_flag = 0;
    }
    else
    {
        if (reinterpret_cast<std::uintptr_t>(heap_base) > 0xffffffffu)
        {
            return nullptr;
        }
        base_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap_base));
        if (commit_routine != 0)
        {
            if (initial_commit == 0 || initial_reserve == 0 || initial_reserve < initial_commit || (flags & 2u) != 0)
            {
                return nullptr;
            }
            if (static_cast<u64>(base_addr) + initial_reserve > 0x100000000ull)
            {
                return nullptr;
            }
            std::memset(heap_base, 0, 0x1000);
            commit_base = base_addr;
            commit_end = base_addr + initial_commit;
            reserve_size = initial_reserve;
            user_flag = 1;
        }
        else
        {
            HeapMemInfo info{};
            if (vm.query(heap_base, &info, sizeof(info)) == 0 || info.base != heap_base || info.state == 0x10000u)
            {
                return nullptr;
            }
            if (info.region_size > 0xffffffffu)
            {
                return nullptr;
            }
            commit_base = base_addr;
            if (info.state == 0x1000u)
            {
                const u32 region = static_cast<u32>(info.region_size);
                if (static_cast<u64>(base_addr) + region > 0x100000000ull)
                {
                    return nullptr;
                }
                std::memset(heap_base, 0, 0x1000);
                commit_size = region;
                commit_end = base_addr + region;
                reserve_size = region;
                HeapMemInfo next{};
                if (vm.query(heap_at(commit_end), &next, sizeof(next)) != 0 && next.state == 0x2000u &&
                    next.region_size <= 0xffffffffu &&
                    static_cast<u64>(next.region_size) + region <= 0xffffffffull)
                {
                    reserve_size = static_cast<u32>(next.region_size) + region;
                }
            }
            else
            {
                commit_size = 0x1000u;
                commit_end = base_addr;
            }
            user_flag = 1;
        }
    }

    if (commit_end == commit_base)
    {
        if (commit_size == 0)
        {
            commit_size = 0x1000u;
        }
        if (vm.alloc(heap_at(commit_base), commit_size, 0x1000u, 4u) == nullptr)
        {
            if (caller_base)
            {
                return nullptr;
            }
            vm.free(heap_at(base_addr), 0, 0x8000u);
            return nullptr;
        }
        commit_end = commit_base + commit_size;
    }

    u32 chain = base_addr + 0x4cu;
    u32 node = (base_addr + 0x597u) & ~7u;
    for (int count = 8; count != 0; --count)
    {
        heap_set_u32(chain, node);
        chain = node;
        node += 0x10u;
    }
    heap_set_u32(chain, 0);
    u32 tail = node;
    u32 span = header_bytes + 0x80u;
    if ((g_heap_nt_flag_1d & 8u) != 0)
    {
        const u32 aligned = (tail + 7u) & ~7u;
        heap_set_u32(base_addr + 0x17cu, aligned);
        span += 0x60cu;
        tail = aligned + 0x60cu;
    }
    span = (span + 0xfu) & ~0xfu;
    heap_set_u16(base_addr, static_cast<u16>(span >> 4));
    heap_at(base_addr)[5] = 1;
    heap_set_u32(base_addr + 0x10u, 0xeeffeeffu);
    heap_set_u32(base_addr + 0x14u, flags);
    heap_set_u32(base_addr + 0x18u, flags & 0x6001007du);
    heap_set_u16(base_addr + 0x170u, static_cast<u16>(heap_get_u16(base_addr + 0x170u) | 0xffffu));
    heap_set_u16(base_addr + 0x3au, static_cast<u16>(tail - base_addr));
    heap_set_u32(base_addr + 0x3cu, 0);
    for (u32 list = 0; list < 0x80u; ++list)
    {
        const u32 head = base_addr + 0x180u + list * 8u;
        heap_set_u32(head, head);
        heap_set_u32(head + 4u, head);
    }
    heap_set_u32(base_addr + 0x5cu, base_addr + 0x58u);
    heap_set_u32(base_addr + 0x58u, base_addr + 0x58u);

    u32 lock_addr = lock == nullptr ? 0u : static_cast<u32>(reinterpret_cast<std::uintptr_t>(lock));
    if (embed_lock)
    {
        heap_set_u32(tail + 4u, 0xffffffffu);
        lock_addr = tail;
    }
    heap_set_u32(base_addr + 0x580u, lock_addr);

    const u32 reserve_end = commit_base + reserve_size;
    if (!heap_init_segment(vm, base_addr, base_addr + span, 0u, commit_base, commit_end, reserve_end, user_flag))
    {
        return nullptr;
    }

    heap_set_u16(base_addr + 0x38u, 0);
    heap_set_u32(base_addr + 0x20u, segment_reserve);
    heap_set_u32(base_addr + 0x24u, segment_commit);
    heap_set_u32(base_addr + 0x28u, decommit_free >> 4);
    heap_set_u32(base_addr + 0x2cu, decommit_total >> 4);
    heap_set_u32(base_addr + 0x34u, maximum_allocation);
    heap_set_u32(base_addr + 0x1cu, (virtual_threshold + 0xfu) >> 4);
    heap_set_u32(base_addr + 0x584u, commit_routine);
    heap_set_u32(base_addr + 0x50u, 0x1fu);
    heap_set_u32(base_addr + 0x54u, 0xfffffff0u);
    return heap_at(base_addr);
#endif
}

void* fn_14e872(void* heap, u32 flags, u32 size)
{
    // Xbox 0x0014E872
    // RtlAllocateHeap. Flags are OR'd with the force-flags at heap+0x18.
    // A zero size allocates one byte. The byte count plus 0x1f is rounded
    // up to a 16-byte granule; that count is the block size in granules,
    // and the caller pointer is the entry plus 0x10. Granules below 0x80
    // take the dedicated list at heap+0x180 when it is non-empty, otherwise
    // the bitmap at heap+0x160 (0x001516A0 is the lowest set bit). A miss
    // walks the large list at heap+0x180. Granules at or above 0x80 skip
    // the bitmap. Granules above the threshold at heap+0x1c are a virtual
    // block when the heap flags at +0x14 include HEAP_GROWABLE: the region
    // is rounded_size+0x20, page aligned, header flags 0x0b, user data at
    // +0x30, linked on heap+0x58. MEM_TOP_DOWN is set when HEAP_ZERO_MEMORY
    // is clear. Anything else asks 0x0014E01D to commit or reserve a
    // segment. The taken entry is marked busy. A leftover granule is
    // absorbed; a larger leftover is a free entry, coalesced with a
    // following free entry, and sizes at or above 0xff01 go through
    // 0x0014D9BC. HEAP_NO_SERIALIZE skips the lock at heap+0x580. The lock
    // drops before HEAP_ZERO_MEMORY fills the caller's bytes. Virtual
    // blocks skip that fill. HEAP_GENERATE_EXCEPTIONS raises
    // STATUS_NO_MEMORY with the rounded size. The port keeps reservations
    // in the low 4GB and uses a host mutex in place of the Xbox critical
    // section. The fs:[0x20]+0x250 alloc notify is absent here.
#if !defined(_WIN32)
    (void)heap;
    (void)flags;
    (void)size;
    return nullptr;
#else
    if (heap == nullptr || reinterpret_cast<std::uintptr_t>(heap) > 0xffffffffu)
    {
        return nullptr;
    }

    HeapVm vm = heap_vm();
    if (vm.alloc == nullptr || vm.free == nullptr)
    {
        return nullptr;
    }

    const u32 heap_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap));
    const u32 asked = size;
    flags |= heap_get_u32(heap_addr + 0x18u);
    u32 bytes = asked == 0 ? 1u : asked;
    bytes += 0x1fu;
    const u32 rounded = bytes & ~0xfu;
    const u32 units = bytes >> 4;

    HeapSerialize lock(heap_addr, flags);
    u32 entry = 0;
    void* user = nullptr;

    if (units < 0x80u)
    {
        const u32 list = heap_addr + 0x180u + units * 8u;
        if (heap_get_u32(list) != list)
        {
            entry = heap_get_u32(list + 4u) - 8u;
            const u8 kept = heap_at(entry)[5];
            heap_unlink_free(heap_addr, entry);
            heap_set_u32(heap_addr + 0x30u, heap_get_u32(heap_addr + 0x30u) - units);
            heap_at(entry)[5] = static_cast<u8>((kept & 0x10u) | 1u);
            heap_at(entry)[6] = static_cast<u8>(rounded - asked);
            heap_at(entry)[7] = 0;
        }
        else
        {
            entry = heap_take_bitmap(heap_addr, units);
            if (entry == 0)
            {
                entry = heap_take_large(heap_addr, units);
            }
            if (entry == 0)
            {
                entry = heap_extend(vm, heap_addr, rounded);
                if (entry != 0)
                {
                    heap_unlink_free(heap_addr, entry);
                }
            }
            if (entry != 0)
            {
                heap_carve(heap_addr, entry, units, rounded, asked);
            }
        }
    }
    else if (units > heap_get_u32(heap_addr + 0x1cu))
    {
        if ((heap_at(heap_addr)[0x14] & 2u) != 0)
        {
            user = heap_virtual_alloc(vm, heap_addr, flags, asked, rounded);
        }
    }
    else
    {
        entry = heap_take_large(heap_addr, units);
        if (entry == 0)
        {
            entry = heap_extend(vm, heap_addr, rounded);
            if (entry != 0)
            {
                heap_unlink_free(heap_addr, entry);
            }
        }
        if (entry != 0)
        {
            heap_carve(heap_addr, entry, units, rounded, asked);
        }
    }

    if (user == nullptr && entry != 0)
    {
        user = heap_at(entry + 0x10u);
        lock.unlock();
        if ((flags & 8u) != 0)
        {
            std::memset(user, 0, asked);
        }
        return user;
    }

    if (user != nullptr)
    {
        return user;
    }

    lock.unlock();
    if ((flags & 4u) != 0)
    {
        const std::uintptr_t info = rounded;
        RaiseException(0xC0000017u, 0, 1, &info);
    }
    return nullptr;
#endif
}

bool fn_14efed(void* heap, u32 flags, void* block)
{
    // Xbox 0x0014EFED
    // RtlFreeHeap. A null block returns true. The force-flags at heap+0x18
    // are OR'd into the caller's flags for the lock test. HEAP_NO_SERIALIZE
    // skips the lock at heap+0x580. The header is the user pointer minus
    // 0x10. Flag bit 3 is a virtual allocation: unlink the node at user-0x30
    // from heap+0x58, drop the lock, and MEM_RELEASE the region. Any other
    // block is coalesced with free neighbors by 0x0014D717 (the block is
    // still busy, so the remove flag is 0). Sizes below 0x80 join that
    // dedicated list and set the bitmap at heap+0x160. Larger sizes join
    // the ordered list at heap+0x180 when the size is below heap+0x28 or
    // the free total plus this size stays below heap+0x2c. Sizes above
    // 0xff00 go through 0x0014D9BC. Anything else is decommitted by
    // 0x0014E1A4. 0x0014F1CE drops the lock on the way out. The
    // fs:[0x20]+0x250 free notify is absent here. The port keeps addresses
    // in the low 4GB and uses a host mutex in place of the Xbox critical
    // section.
#if !defined(_WIN32)
    (void)heap;
    (void)flags;
    return block == nullptr;
#else
    if (block == nullptr)
    {
        return true;
    }
    if (heap == nullptr || reinterpret_cast<std::uintptr_t>(heap) > 0xffffffffu ||
        reinterpret_cast<std::uintptr_t>(block) > 0xffffffffu)
    {
        return false;
    }

    const u32 heap_addr = static_cast<u32>(reinterpret_cast<std::uintptr_t>(heap));
    const u32 user = static_cast<u32>(reinterpret_cast<std::uintptr_t>(block));
    flags |= heap_get_u32(heap_addr + 0x18u);
    HeapSerialize lock(heap_addr, flags);

    const u32 header = user - 0x10u;
    if ((heap_at(header)[5] & 8u) != 0)
    {
        const u32 base = header - 0x20u;
        const u32 flink = heap_get_u32(base);
        const u32 blink = heap_get_u32(base + 4u);
        heap_set_u32(blink, flink);
        heap_set_u32(flink + 4u, blink);
        lock.unlock();
        HeapVm vm = heap_vm();
        if (vm.free == nullptr || vm.free(heap_at(base), 0, 0x8000u) == 0)
        {
            return false;
        }
        return true;
    }

    HeapVm vm = heap_vm();
    u32 units = heap_get_u16(header);
    const u32 entry = heap_coalesce(heap_addr, header, &units, false);
    if (units < 0x80u)
    {
        heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0x10u);
        heap_set_u16(entry, static_cast<u16>(units));
        heap_link_free_entry(heap_addr, entry);
    }
    else if (units < heap_get_u32(heap_addr + 0x28u) ||
             heap_get_u32(heap_addr + 0x30u) + units < heap_get_u32(heap_addr + 0x2cu))
    {
        if (units <= 0xff00u)
        {
            heap_at(entry)[5] = static_cast<u8>(heap_at(entry)[5] & 0x10u);
            heap_set_u16(entry, static_cast<u16>(units));
            heap_link_free_entry(heap_addr, entry);
        }
        else
        {
            heap_add_free(heap_addr, entry, units);
        }
    }
    else
    {
        heap_decommit(vm, heap_addr, entry, units);
    }
    return true;
#endif
}

// Kernel LaunchDataPage (thunk 0x00202EDC, ordinal 164). 0x0015028C publishes
// the 0x1000-byte page. 0x0015040D reads it on the next entry.
u8 g_xapi_launch_page[0x1000] = {};
bool g_xapi_launch_page_set = false;

// \??\D: target. OBJECT_STRING at 0x0032CED8 is "\\??\\D:" (length 6).
// NtQuerySymbolicLinkObject failure uses "\\Device\\Cdrom0".
char g_xapi_d_target[0x208] = "\\Device\\Cdrom0";

// \??\U: and \??\T: targets filled by 0x00150CC6. Empty means the link has
// not been created. A second IoCreateSymbolicLink returns
// STATUS_OBJECT_NAME_COLLISION.
char g_xapi_u_target[0x168] = {};
char g_xapi_t_target[0x168] = {};

// TitleMeta.xbx bytes 0x00150992 would write for T: when $$XTINFO is absent.
// Size is the wchar count times two, with no trailing NUL.
u8 g_xapi_title_meta[0x11C] = {};
u32 g_xapi_title_meta_size = 0;

u32 fn_15028c(const char* path, const void* launch_data, u32 flags, u32 launch_type)
{
    // Xbox 0x0015028C
    // Body of XLaunchNewImageA (0x0014AD66). A null path reboots through
    // 0x0014AEED with launch type 1 when launch data is present and
    // 0xFFFFFFFF when it is absent. D:\ and d:\ keep the tail and join it
    // to the \??\D: target with a semicolon. T:\$u\ and t:\$U\ expand to
    // \Device\Harddisk0\Partition1\TDATA\%08x\$u\. Y:\ and y:\ expand to
    // \Device\Harddisk0\Partition2\ when flags bit 1 is set. 0x0014ADA8
    // fills the 0x1000 launch page (title id from the XBE certificate,
    // launch type, path at +8, and 0xC00 bytes of launch data at +0x400
    // when the type is not 0xFFFFFFFF). 0x00150089 writes the D: target
    // and the path tail at +0x214, and turns a type of 0xFFFFFFFF into 0,
    // except for a D:\ launch with no launch data. The expanded path has
    // to fit in 0x168 bytes. Success calls the object at fs:[0x20]+0x250
    // vfunc +0x14 with (2, 0), then HalReturnToFirmware(2). Any other
    //     path returns 0x57. The port keeps the page for 0x0015040D and returns
    // RtlNtStatusToDosError(0).
    constexpr u32 kTitleId = 0x4D530053u;
    constexpr char kCdrom[] = "\\Device\\Cdrom0";

    const u32 stored_type = launch_data != nullptr ? launch_type : 0xFFFFFFFFu;

    auto copy_n = [](char* dst, const char* src, int cch) -> int
    {
        if (cch <= 0)
        {
            return 0;
        }
        int i = 0;
        while (i < cch - 1 && src[i] != '\0')
        {
            dst[i] = src[i];
            ++i;
        }
        dst[i] = '\0';
        return i;
    };

    auto fill_page = [&](const char* image, const char* prefix, const char* alt_tail)
    {
        std::memset(g_xapi_launch_page, 0, sizeof(g_xapi_launch_page));
        std::memcpy(g_xapi_launch_page + 0, &stored_type, 4);
        std::memcpy(g_xapi_launch_page + 4, &kTitleId, 4);
        if (image != nullptr)
        {
            char* dest = reinterpret_cast<char*>(g_xapi_launch_page + 8);
            if (prefix != nullptr)
            {
                const int n = copy_n(dest, prefix, 0x207);
                dest[n] = ';';
                copy_n(dest + n + 1, image, 0x208 - (n + 1));
            }
            else
            {
                copy_n(dest, image, 0x208);
            }
        }
        if (stored_type != 0xFFFFFFFFu && launch_data != nullptr)
        {
            std::memcpy(g_xapi_launch_page + 0x400, launch_data, 0xC00);
        }
        if (alt_tail != nullptr)
        {
            const int n1 = static_cast<int>(std::strlen(kCdrom));
            const int n2 = static_cast<int>(std::strlen(alt_tail));
            if (n2 + (n1 + 1) + 8 < 0x1EC)
            {
                u8* rec = g_xapi_launch_page + 0x214;
                const u16 n1_nul = static_cast<u16>(n1 + 1);
                rec[0] = 1;
                rec[1] = 0;
                rec[2] = static_cast<u8>(n1_nul & 0xFF);
                rec[3] = static_cast<u8>(n1_nul >> 8);
                std::memcpy(rec + 4, &stored_type, 4);
                if (stored_type == 0xFFFFFFFFu)
                {
                    const u32 zero = 0;
                    std::memcpy(g_xapi_launch_page, &zero, 4);
                }
                std::memcpy(rec + 8, kCdrom, static_cast<size_t>(n1) + 1);
                std::memcpy(rec + 8 + n1 + 1, alt_tail, static_cast<size_t>(n2) + 1);
            }
        }
        g_xapi_launch_page_set = true;
    };

    if (path == nullptr)
    {
        fill_page(nullptr, nullptr, nullptr);
        return 0;
    }

    const char c0 = path[0];
    if ((c0 == 'D' || c0 == 'd') && path[1] == ':' && path[2] == '\\')
    {
        const char* tail = path + 3;
        fill_page(tail, kCdrom, launch_data != nullptr ? tail : nullptr);
        return 0;
    }

    std::string expanded;
    const char* tail = nullptr;
    if ((c0 == 'T' || c0 == 't') && path[1] == ':' && path[2] == '\\' && path[3] == '$' &&
        (path[4] == 'U' || path[4] == 'u') && path[5] == '\\')
    {
        char hex[9];
        for (int i = 0; i < 8; ++i)
        {
            const u32 nib = (kTitleId >> ((7 - i) * 4)) & 0xFu;
            hex[i] = static_cast<char>(nib < 10u ? '0' + nib : 'a' + (nib - 10u));
        }
        hex[8] = '\0';
        tail = path + 6;
        expanded = std::string("\\Device\\Harddisk0\\Partition1\\TDATA\\") + hex + "\\$u\\" + tail;
    }
    else if ((flags & 2u) != 0 && (c0 == 'Y' || c0 == 'y') && path[1] == ':' && path[2] == '\\')
    {
        tail = path + 3;
        expanded = std::string("\\Device\\Harddisk0\\Partition2\\") + tail;
    }
    else
    {
        return 0x57;
    }

    if (expanded.size() >= 0x168)
    {
        return 0x57;
    }
    fill_page(expanded.c_str(), nullptr, tail);
    return 0;
}

bool xapi_user_leaf_path(char* dst, int cap, const char* leaf)
{
    // 0x0014FE7D. _snprintf(dst, cap, "t:\\$u\\%s", leaf). A negative count
    // stores last-error 0x7A and returns false.
    const char prefix[] = "t:\\$u\\";
    int n = 0;
    for (const char* s = prefix; *s != '\0'; ++s)
    {
        if (n + 1 >= cap)
        {
            return false;
        }
        dst[n++] = *s;
    }
    if (leaf != nullptr)
    {
        for (const char* s = leaf; *s != '\0'; ++s)
        {
            if (n + 1 >= cap)
            {
                return false;
            }
            dst[n++] = *s;
        }
    }
    if (n >= cap)
    {
        return false;
    }
    dst[n] = '\0';
    return true;
}

bool xapi_read_capped(const char* path, std::string& out, size_t cap)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    out.clear();
    char buf[4096];
    while (in)
    {
        in.read(buf, sizeof(buf));
        const std::streamsize n = in.gcount();
        if (n <= 0)
        {
            break;
        }
        if (out.size() + static_cast<size_t>(n) > cap)
        {
            return false;
        }
        out.append(buf, static_cast<size_t>(n));
    }
    return static_cast<bool>(in) || in.eof();
}

void xapi_hmac_sha1(const u8* key, u32 key_len, const u8* data, u32 size, u8 out[20])
{
    XapiShaContext ctx;
    xapi_hmac_begin(ctx, key, key_len);
    if (data != nullptr && size != 0)
    {
        xapi_sha_update(ctx, data, size);
    }
    xapi_hmac_final(ctx, key, key_len, out);
}

int xapi_cached_xbe_status(const char* leaf, u32 title_id, u32 version)
{
    // Body of 0x0015010A, called as (leaf, title id, certificate version, 0).
    // The fourth argument is unused. Builds t:\$u\contentmeta.xbx (0x0014FE7D
    // / "t:\\$u\\%s") and opens it GENERIC_READ, share read, OPEN_EXISTING.
    // 0x00151982 reads the header: magic 0x46534358 ('XSCF') at +0x14, size at
    // +0x18 capped to 0x88. With the check flag set it HMACs XboxHDKey over the
    // title id (0x0015187E, XcHMAC) and that key over the header from +0x14,
    // plus any bytes past 0x88, and compares the digest to the first 0x14
    // bytes. Flag bit 0 at +0x20 has to be set. The dword at +0x38 has to be
    // 0x44. That many bytes are read from the file offset at +0x34. XcSHAInit /
    // Update / Final (ordinals 335, 336, 337) of those bytes must match the
    // 20 bytes at +0x3C. The first dword is a count of 1..16 versions; the
    // certificate version has to be one of them. 0x0014FEC7 then opens
    // t:\$u\<leaf>, reads 0x178 bytes of XBE header, seeks to
    // CertificateAddress - BaseAddress, reads 0x1D0 certificate bytes, and the
    // title id at +8 has to match. Failure returns 0x8007000D. Success returns 0.
    constexpr int kFail = static_cast<int>(0x8007000Du);
    constexpr u32 kMagic = 0x46534358u;

    char meta_path[0x104];
    if (!xapi_user_leaf_path(meta_path, 0x104, "contentmeta.xbx"))
    {
        return kFail;
    }

    std::string meta;
    if (!xapi_read_capped(meta_path, meta, 0x100000) || meta.size() < 0x1Cu)
    {
        return kFail;
    }

    u32 hdr_size = 0;
    std::memcpy(&hdr_size, meta.data() + 0x18, 4);
    if (hdr_size < 0x1Cu)
    {
        return kFail;
    }
    const u32 covered = hdr_size < 0x88u ? hdr_size : 0x88u;
    if (meta.size() < covered)
    {
        return kFail;
    }

    u8 header[0x88] = {};
    std::memcpy(header, meta.data(), covered);
    u32 magic = 0;
    std::memcpy(&magic, header + 0x14, 4);
    if (magic != kMagic)
    {
        return kFail;
    }

    u8 content_key[20];
    u8 title_bytes[4];
    std::memcpy(title_bytes, &title_id, 4);
    xapi_hmac_sha1(kXboxHDKey, 0x10, title_bytes, 4, content_key);

    XapiShaContext ctx;
    xapi_hmac_begin(ctx, content_key, 0x14);
    xapi_sha_update(ctx, header + 0x14, covered - 0x14u);
    if (hdr_size > 0x88u)
    {
        if (meta.size() < hdr_size)
        {
            return kFail;
        }
        xapi_sha_update(ctx, reinterpret_cast<const u8*>(meta.data() + 0x88), hdr_size - 0x88u);
    }
    u8 digest[20];
    xapi_hmac_final(ctx, content_key, 0x14, digest);
    if (std::memcmp(digest, header, 20) != 0)
    {
        return kFail;
    }
    if ((header[0x20] & 1u) == 0)
    {
        return kFail;
    }

    u32 payload_off = 0;
    u32 payload_size = 0;
    std::memcpy(&payload_off, header + 0x34, 4);
    std::memcpy(&payload_size, header + 0x38, 4);
    if (payload_size != 0x44u)
    {
        return kFail;
    }
    if (static_cast<u64>(payload_off) + payload_size > meta.size())
    {
        return kFail;
    }

    const u8* payload = reinterpret_cast<const u8*>(meta.data() + payload_off);
    xapi_sha_init(ctx);
    xapi_sha_update(ctx, payload, 0x44);
    u8 payload_digest[20];
    xapi_sha_final(ctx, payload_digest);
    if (std::memcmp(payload_digest, header + 0x3C, 20) != 0)
    {
        return kFail;
    }

    u32 count = 0;
    std::memcpy(&count, payload, 4);
    if (count == 0 || count > 0x10u)
    {
        return kFail;
    }
    bool found_version = false;
    for (u32 i = 0; i < count; ++i)
    {
        u32 entry = 0;
        std::memcpy(&entry, payload + 4u + i * 4u, 4);
        if (entry == version)
        {
            found_version = true;
            break;
        }
    }
    if (!found_version)
    {
        return kFail;
    }

    char image_path[0x104];
    if (leaf == nullptr || !xapi_user_leaf_path(image_path, 0x104, leaf))
    {
        return kFail;
    }

    std::ifstream image(image_path, std::ios::binary);
    if (!image)
    {
        return kFail;
    }
    u8 xbe_header[0x178];
    image.read(reinterpret_cast<char*>(xbe_header), sizeof(xbe_header));
    if (image.gcount() != static_cast<std::streamsize>(sizeof(xbe_header)))
    {
        return kFail;
    }
    u32 image_base = 0;
    u32 cert_addr = 0;
    std::memcpy(&image_base, xbe_header + 0x104, 4);
    std::memcpy(&cert_addr, xbe_header + 0x118, 4);
    if (cert_addr < image_base)
    {
        return kFail;
    }
    image.seekg(static_cast<std::streamoff>(cert_addr - image_base), std::ios::beg);
    u8 certificate[0x1D0];
    image.read(reinterpret_cast<char*>(certificate), sizeof(certificate));
    if (image.gcount() != static_cast<std::streamsize>(sizeof(certificate)))
    {
        return kFail;
    }
    u32 image_title = 0;
    std::memcpy(&image_title, certificate + 8, 4);
    if (image_title != title_id)
    {
        return kFail;
    }
    return 0;
}

u32 fn_15040d()
{
    // Xbox 0x0015040D
    // Runs after the 0x1050-byte stack probe (0x00166F70). Certificate TitleID
    // 0xFFFE0000 returns 0. Otherwise 0x0014FF93 reads LaunchDataPage. Bit 0
    // at page+0x214 selects the device path at +0x21C and the type dword at
    // +0x218, and the call returns 1. With the bit clear the type is the dword
    // at the start of the page and the call returns 0. A type other than
    // 0xFFFFFFFF copies 0xC00 bytes from page+0x400. A null page leaves the
    // type at 0xFFFFFFFF. When that call returns 0, 0x0014FFF7 queries the
    // \??\D: symbolic link into a 0x168 buffer, or copies "\\Device\\Cdrom0"
    // when the query fails.
    // XeImageFileName (thunk 0x00202FC8, ordinal 326) is an OBJECT_STRING.
    // When its length is greater than the directory string, _strnicmp of that
    // prefix has to be 0. The bytes after the prefix and one separator are
    // the leaf. _snprintf of "%s%s" / "t:\\$u\\" / leaf into 0x168 bytes
    // returns 0 from this function when the count is negative. 0x0015010A
    // then checks that leaf against the certificate title id and version. A
    // non-negative status calls 0x0015028C with the t:\$u path, the launch
    // data pointer (null when the type is 0xFFFFFFFF), flags 0, and that type.
    // Retail success reboots inside 0x0015028C. When 0x0014FF93 returned
    // non-zero, IoDeleteSymbolicLink and IoCreateSymbolicLink (ordinals 69
    // and 67) retarget \??\D: at the directory string. The port stores that
    // target. Always returns 0.
    constexpr u32 kUnsetTitleId = 0xFFFE0000u;
    constexpr u32 kCertificateVersion = 2u;
    constexpr char kXeImageFileName[] = "\\Device\\CdRom0\\default.xbe";
    constexpr u16 kXeImageFileNameLength = static_cast<u16>(sizeof(kXeImageFileName) - 1u);
    constexpr char kCdromFallback[] = "\\Device\\Cdrom0";

    const u32 title_id = kCalcSigTitleId;
    if (title_id == kUnsetTitleId)
    {
        return 0;
    }

    u32 launch_type = 0xFFFFFFFFu;
    bool from_launch = false;
    char dir[0x400];
    dir[0] = '\0';
    u8 launch_data[0xC00];
    bool have_data = false;

    if (g_xapi_launch_page_set)
    {
        if ((g_xapi_launch_page[0x214] & 1u) != 0)
        {
            std::memcpy(&launch_type, g_xapi_launch_page + 0x218, 4);
            const char* page_path = reinterpret_cast<const char*>(g_xapi_launch_page + 0x21C);
            size_t n = 0;
            while (n + 1 < sizeof(dir) && page_path[n] != '\0')
            {
                dir[n] = page_path[n];
                ++n;
            }
            dir[n] = '\0';
            from_launch = true;
        }
        else
        {
            std::memcpy(&launch_type, g_xapi_launch_page, 4);
        }
        if (launch_type != 0xFFFFFFFFu)
        {
            std::memcpy(launch_data, g_xapi_launch_page + 0x400, 0xC00);
            have_data = true;
        }
    }

    if (!from_launch)
    {
        const char* src = g_xapi_d_target[0] != '\0' ? g_xapi_d_target : kCdromFallback;
        size_t n = 0;
        while (n + 1 < 0x168 && src[n] != '\0')
        {
            dir[n] = src[n];
            ++n;
        }
        dir[n] = '\0';
    }

    const size_t dir_len = std::strlen(dir);
    if (kXeImageFileNameLength > dir_len)
    {
        bool prefix = true;
        for (size_t i = 0; i < dir_len; ++i)
        {
            unsigned char a = static_cast<unsigned char>(kXeImageFileName[i]);
            unsigned char b = static_cast<unsigned char>(dir[i]);
            if (a >= 'A' && a <= 'Z')
            {
                a = static_cast<unsigned char>(a + 32);
            }
            if (b >= 'A' && b <= 'Z')
            {
                b = static_cast<unsigned char>(b + 32);
            }
            if (a != b)
            {
                prefix = false;
                break;
            }
        }
        if (prefix)
        {
            const u32 skip = static_cast<u32>(dir_len) + 1u;
            const u32 count = kXeImageFileNameLength - skip;
            char leaf[0x168];
            if (count < sizeof(leaf))
            {
                if (count != 0)
                {
                    std::memcpy(leaf, kXeImageFileName + skip, count);
                }
                leaf[count] = '\0';

                char launch_path[0x168];
                if (!xapi_user_leaf_path(launch_path, 0x168, leaf))
                {
                    return 0;
                }
                if (xapi_cached_xbe_status(leaf, title_id, kCertificateVersion) >= 0)
                {
                    const void* data = nullptr;
                    if (launch_type != 0xFFFFFFFFu && have_data)
                    {
                        data = launch_data;
                    }
                    fn_15028c(launch_path, data, 0, launch_type);
                }
            }
        }
    }

    if (from_launch)
    {
        size_t n = 0;
        while (n + 1 < sizeof(g_xapi_d_target) && dir[n] != '\0')
        {
            g_xapi_d_target[n] = dir[n];
            ++n;
        }
        g_xapi_d_target[n] = '\0';
    }
    return 0;
}

std::int64_t* fn_15059a(std::int64_t* timeout, u32 milliseconds)
{
    // Xbox 0x0015059A
    // stdcall. Milliseconds become a relative NT timeout: an unsigned
    // 32x32 multiply by 10000 (100-nanosecond units per millisecond) stored
    // as a two's-complement 64-bit negation. 0xFFFFFFFF is an infinite wait:
    // the buffer is left untouched and the result is null, which the kernel
    // treats as no timeout. 0x001506C1 passes the pointer as the timeout of
    // NtWaitForSingleObjectEx (thunk 0x00202FDC, ordinal 234) with wait mode
    // 1. 0x00150705 does the same for NtWaitForMultipleObjectsEx (thunk
    // 0x00202FE0, ordinal 235). 0x00150785 passes it to KeDelayExecutionThread
    // (thunk 0x00202FE4, ordinal 99) and, when the result is null, substitutes
    // a local interval of 0x8000000000000000.
    if (milliseconds == 0xFFFFFFFFu)
    {
        return nullptr;
    }
    const u64 ticks = static_cast<u64>(milliseconds) * 10000u;
    *timeout = -static_cast<std::int64_t>(ticks);
    return timeout;
}

// OBJECT_STRING / ANSI_STRING. Length is the byte count of the object name.
struct XapiAnsiString
{
    u16 length;
    u16 maximum_length;
    const char* buffer;
};

int fn_150833(const XapiAnsiString* name, int bytes_per_cluster)
{
    // Xbox 0x00150833
    // NtOpenFile with a null root, OBJ_CASE_INSENSITIVE, FILE_READ_DATA|SYNCHRONIZE,
    // share read|write, and FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|
    // FILE_OPEN_FOR_FREE_SPACE_QUERY. NtQueryVolumeInformationFile then reads
    // FileFsSizeInformation (class 3, 24 bytes) and NtClose drops the handle.
    // When that query succeeds and sectors-per-allocation-unit times bytes-per-sector
    // is not bytes_per_cluster, the status becomes STATUS_UNRECOGNIZED_VOLUME
    // (0xC000014F). 0x00151000 asks for 0x4000. 0x0014BFCF asks for
    // 0x4000 << (XBE init flags >> 30) on \Device\Harddisk0\Partition%d\.
    constexpr int kStatusInvalidParameter = static_cast<int>(0xC000000Du);
    constexpr int kStatusNotSupported = static_cast<int>(0xC00000BBu);

    if (name == nullptr || (name->length > 0 && name->buffer == nullptr))
    {
        return kStatusInvalidParameter;
    }

#if defined(_WIN32)
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    struct FsSizeInformation
    {
        std::int64_t total_allocation_units;
        std::int64_t available_allocation_units;
        unsigned long sectors_per_allocation_unit;
        unsigned long bytes_per_sector;
    };
    using NtOpenFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                          IoStatusBlock* io, unsigned long share, unsigned long options);
    using NtQueryVolumeInformationFileFn = long(__stdcall*)(void* handle, IoStatusBlock* io, void* info,
                                                            unsigned long length, unsigned long info_class);
    using NtCloseFn = long(__stdcall*)(void* handle);

    static_assert(sizeof(FsSizeInformation) == 0x18);

    void* ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == nullptr)
    {
        return kStatusNotSupported;
    }
    NtOpenFileFn nt_open = nullptr;
    NtQueryVolumeInformationFileFn nt_query = nullptr;
    NtCloseFn nt_close = nullptr;
    void* open_proc = GetProcAddress(ntdll, "NtOpenFile");
    void* query_proc = GetProcAddress(ntdll, "NtQueryVolumeInformationFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    std::memcpy(&nt_open, &open_proc, sizeof(nt_open));
    std::memcpy(&nt_query, &query_proc, sizeof(nt_query));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    if (nt_open == nullptr || nt_query == nullptr || nt_close == nullptr)
    {
        return kStatusNotSupported;
    }

    std::wstring wide(static_cast<size_t>(name->length), L'\0');
    for (u16 i = 0; i < name->length; ++i)
    {
        wide[i] = static_cast<unsigned char>(name->buffer[i]);
    }
    const auto wide_bytes = static_cast<size_t>(wide.size()) * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        return static_cast<int>(0xC0000106); // STATUS_NAME_TOO_LONG
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40; // OBJ_CASE_INSENSITIVE

    IoStatusBlock io{};
    void* handle = nullptr;
    long status = nt_open(&handle, 0x100001u, &attributes, &io, 3u, 0x800021u);
    if (status < 0)
    {
        return static_cast<int>(status);
    }

    FsSizeInformation info{};
    status = nt_query(handle, &io, &info, 0x18u, 3u);
    nt_close(handle);
    if (status >= 0)
    {
        const u32 cluster = static_cast<u32>(info.sectors_per_allocation_unit) *
                            static_cast<u32>(info.bytes_per_sector);
        if (std::bit_cast<int>(cluster) != bytes_per_cluster)
        {
            status = static_cast<int>(0xC000014Fu);
        }
    }
    return static_cast<int>(status);
#else
    (void)bytes_per_cluster;
    return kStatusNotSupported;
#endif
}

int fn_1508ab(std::uintptr_t section, char* path, int capacity, const char* leaf)
{
    // Xbox 0x001508AB
    // stdcall. lstrcpynA (0x0014ACB2) appends leaf at the NUL of path. The
    // count is capacity minus strlen(path). RtlInitAnsiString (thunk
    // 0x00202EE8) then NtCreateFile (thunk 0x00202F88): null root,
    // OBJ_CASE_INSENSITIVE, GENERIC_WRITE|SYNCHRONIZE (0x40100000),
    // FILE_ATTRIBUTE_SYSTEM, FILE_SHARE_READ, FILE_OPEN_IF, and
    // FILE_WRITE_THROUGH|FILE_SYNCHRONOUS_IO_NONALERT (0x22). The byte at the
    // append point is cleared after that call returns.
    // STATUS_OBJECT_NAME_COLLISION (0xC0000035) becomes 0. There is no handle
    // to close on that path. Any other success calls NtQueryInformationFile
    // (thunk 0x00202F18) for FileNetworkOpenInformation (class 0x22, 0x38
    // bytes). When that succeeds and EndOfFile is 0, XLoadSectionByHandle
    // (0x0014AF9B, XeLoadSection thunk 0x00202EEC) and XGetSectionSize
    // (0x0014AFDA, the dword at handle+8) feed NtWriteFile (thunk 0x00202F14)
    // at a null offset. XFreeSectionByHandle (0x0014AFBC, XeUnloadSection
    // thunk 0x00202EF0) follows. NtClose (thunk 0x00202EF4) always runs after
    // a successful create. The NTSTATUS is the result. A failed section load
    // stores last-error through 0x0014B03A and leaves the query status in
    // place. 0x00150C73 passes the $$XTIMAGE handle, a 0x104-byte directory,
    // and \TitleImage.xbx. 0x00150C96 does the same for $$XSIMAGE and
    // \SaveImage.xbx.
    // The host create also asks for FILE_READ_ATTRIBUTES. Without it the
    // size query is STATUS_ACCESS_DENIED and the write never runs. XeLoadSection
    // reads the section raw bytes from default.xbe. The directory is --game-dir
    // when that flag is present, otherwise OriginalFiles. The buffer is
    // zero-padded out to the virtual size. Dropping that buffer is the unload.
    constexpr int kNotSupported = static_cast<int>(0xC00000BBu);
    constexpr int kInvalidParameter = static_cast<int>(0xC000000Du);
    constexpr long kNameCollision = static_cast<long>(0xC0000035u);

    if (path == nullptr)
    {
        return kInvalidParameter;
    }

    const size_t length = std::strlen(path);
    char* const append_at = path + length;
    const int room = capacity - static_cast<int>(length);
    if (room > 0 && leaf != nullptr)
    {
        int left = room;
        const char* src = leaf;
        char* dst = append_at;
        while (left > 1 && *src != '\0')
        {
            *dst = *src;
            ++dst;
            ++src;
            --left;
        }
        if (left > 0)
        {
            *dst = '\0';
        }
    }

    struct AppendGuard
    {
        char* at;
        ~AppendGuard()
        {
            if (at != nullptr)
            {
                *at = '\0';
            }
        }
    } guard{append_at};

#if !defined(_WIN32)
    (void)section;
    return kNotSupported;
#else
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    using NtCreateFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                            IoStatusBlock* io, std::int64_t* allocation, unsigned long file_attributes,
                                            unsigned long share, unsigned long disposition, unsigned long options,
                                            void* ea, unsigned long ea_length);
    using NtQueryInformationFileFn = long(__stdcall*)(void* handle, IoStatusBlock* io, void* info, unsigned long length,
                                                      unsigned long info_class);
    using NtWriteFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                           void* buffer, unsigned long length, std::int64_t* offset, unsigned long* key);
    using NtCloseFn = long(__stdcall*)(void* handle);
    using RtlNtStatusToDosErrorFn = unsigned long(__stdcall*)(long status);
    using GetCommandLineAFn = char*(__stdcall*)();

    void* ntdll = GetModuleHandleA("ntdll.dll");
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (ntdll == nullptr || kernel == nullptr)
    {
        return kNotSupported;
    }

    NtCreateFileFn nt_create = nullptr;
    NtQueryInformationFileFn nt_query = nullptr;
    NtWriteFileFn nt_write = nullptr;
    NtCloseFn nt_close = nullptr;
    RtlNtStatusToDosErrorFn rtl_dos = nullptr;
    GetCommandLineAFn command_line = nullptr;
    void* create_proc = GetProcAddress(ntdll, "NtCreateFile");
    void* query_proc = GetProcAddress(ntdll, "NtQueryInformationFile");
    void* write_proc = GetProcAddress(ntdll, "NtWriteFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    void* rtl_proc = GetProcAddress(ntdll, "RtlNtStatusToDosError");
    void* cmd_proc = GetProcAddress(kernel, "GetCommandLineA");
    std::memcpy(&nt_create, &create_proc, sizeof(nt_create));
    std::memcpy(&nt_query, &query_proc, sizeof(nt_query));
    std::memcpy(&nt_write, &write_proc, sizeof(nt_write));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    std::memcpy(&rtl_dos, &rtl_proc, sizeof(rtl_dos));
    std::memcpy(&command_line, &cmd_proc, sizeof(command_line));
    if (nt_create == nullptr || nt_query == nullptr || nt_write == nullptr || nt_close == nullptr)
    {
        return kNotSupported;
    }

    auto set_last = [&](long code)
    {
        if (rtl_dos != nullptr)
        {
            g_xapi_last_error = rtl_dos(code);
        }
        else
        {
            g_xapi_last_error = 0x1F;
        }
    };

    const size_t full_len = std::strlen(path);
    std::wstring wide(full_len, L'\0');
    for (size_t i = 0; i < full_len; ++i)
    {
        wide[i] = static_cast<unsigned char>(path[i]);
    }
    const auto wide_bytes = wide.size() * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        return static_cast<int>(0xC0000106);
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40;

    IoStatusBlock io{};
    void* handle = nullptr;
    // 0x80 is FILE_READ_ATTRIBUTES, so FileNetworkOpenInformation can succeed
    // on the host. Retail passes 0x40100000 alone.
    long status = nt_create(&handle, 0x40100080u, &attributes, &io, nullptr, 4u, 1u, 3u, 0x22u, nullptr, 0u);
    if (status < 0)
    {
        if (status == kNameCollision)
        {
            return 0;
        }
        return static_cast<int>(status);
    }

    u8 info[0x38]{};
    status = nt_query(handle, &io, info, 0x38u, 0x22u);
    if (status >= 0)
    {
        u32 eof_lo = 0;
        u32 eof_hi = 0;
        std::memcpy(&eof_lo, info + 0x28, sizeof(eof_lo));
        std::memcpy(&eof_hi, info + 0x2C, sizeof(eof_hi));
        if (eof_lo == 0 && eof_hi == 0)
        {
            const XbeSectionRecord* rec = nullptr;
            if (section != 0 && section != static_cast<std::uintptr_t>(-1))
            {
                const auto* candidate = reinterpret_cast<const XbeSectionRecord*>(section);
                for (const XbeSectionRecord& item : g_xbe_sections)
                {
                    if (&item == candidate)
                    {
                        rec = candidate;
                        break;
                    }
                }
            }

            const void* loaded = nullptr;
            unsigned long write_size = 0;
            std::unique_ptr<u8[]> image;
            if (rec == nullptr)
            {
                set_last(static_cast<long>(0xC0000008u));
            }
            else if (rec->virtual_size == 0)
            {
                static const u8 empty = 0;
                loaded = &empty;
            }
            else
            {
                image.reset(new (std::nothrow) u8[rec->virtual_size]());
                if (image == nullptr)
                {
                    set_last(static_cast<long>(0xC000009Au));
                }
                else
                {
                    std::filesystem::path game_dir{"OriginalFiles"};
                    if (command_line != nullptr)
                    {
                        const char* cmd = command_line();
                        const char* flag = cmd != nullptr ? std::strstr(cmd, "--game-dir") : nullptr;
                        if (flag != nullptr)
                        {
                            flag += std::strlen("--game-dir");
                            while (*flag == ' ' || *flag == '\t')
                            {
                                ++flag;
                            }
                            if (*flag == '"')
                            {
                                ++flag;
                                const char* end = std::strchr(flag, '"');
                                if (end != nullptr && end != flag)
                                {
                                    game_dir = std::filesystem::path(std::string(flag, end));
                                }
                            }
                            else if (*flag != '\0')
                            {
                                const char* end = flag;
                                while (*end != '\0' && *end != ' ' && *end != '\t')
                                {
                                    ++end;
                                }
                                if (end != flag)
                                {
                                    game_dir = std::filesystem::path(std::string(flag, end));
                                }
                            }
                        }
                    }

                    const u32 copy = rec->raw_size < rec->virtual_size ? rec->raw_size : rec->virtual_size;
                    std::ifstream in(game_dir / "default.xbe", std::ios::binary);
                    bool read_ok = static_cast<bool>(in);
                    if (read_ok && copy != 0)
                    {
                        in.seekg(0, std::ios::end);
                        const auto file_end = in.tellg();
                        const auto need = static_cast<std::streamoff>(rec->raw_address) + static_cast<std::streamoff>(copy);
                        if (file_end < 0 || file_end < need)
                        {
                            read_ok = false;
                        }
                        else
                        {
                            in.seekg(static_cast<std::streamoff>(rec->raw_address), std::ios::beg);
                            in.read(reinterpret_cast<char*>(image.get()), static_cast<std::streamsize>(copy));
                            read_ok = in.gcount() == static_cast<std::streamsize>(copy);
                        }
                    }
                    if (!read_ok)
                    {
                        set_last(static_cast<long>(0xC0000034u));
                        image.reset();
                    }
                    else
                    {
                        loaded = image.get();
                        write_size = rec->virtual_size;
                    }
                }
            }

            if (loaded != nullptr)
            {
                status = nt_write(handle, nullptr, nullptr, nullptr, &io, const_cast<void*>(loaded), write_size, nullptr,
                                  nullptr);
            }
        }
    }

    nt_close(handle);
    return static_cast<int>(status);
#endif
}

// OBJECT_STRING in .data. Length is mutable: 0x00150992 decrements it when
// the mapped directory already ends in a backslash.
struct XapiLetterString
{
    u16 length;
    u16 maximum_length;
    const char* buffer;
};

static_assert(sizeof("\\??\\U:") - 1 == 6);
static_assert(sizeof("\\Device\\Harddisk0\\partition1\\UDATA") - 1 == 0x22);
static_assert(sizeof("\\??\\T:") - 1 == 6);
static_assert(sizeof("\\Device\\Harddisk0\\partition1\\TDATA") - 1 == 0x22);

XapiLetterString g_xapi_u_drive{6, 7, "\\??\\U:"};
XapiLetterString g_xapi_u_data{0x22, 0x23, "\\Device\\Harddisk0\\partition1\\UDATA"};
XapiLetterString g_xapi_t_drive{6, 7, "\\??\\T:"};
XapiLetterString g_xapi_t_data{0x22, 0x23, "\\Device\\Harddisk0\\partition1\\TDATA"};

int xapi_map_letter_to_directory(XapiLetterString* drive, XapiLetterString* dir, const char* leaf, int create_directory,
                                 const u16* title_name, int set_time)
{
    // Body of XapiMapLetterToDirectory (0x00150992) for the two calls from
    // 0x00150CC6. Both pass create_directory 1 and set_time 0. Retail opens
    // the directory OBJECT_STRING with NtCreateFile FILE_OPEN_IF, access
    // FILE_LIST_DIRECTORY|SYNCHRONIZE, share read|write, and options
    // FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_FOR_BACKUP_INTENT.
    // STATUS_NOT_A_DIRECTORY (0xC0000103) is treated as success. The leaf is
    // joined with a backslash and created the same way, disposition
    // FILE_OPEN_IF when create_directory is set and FILE_OPEN otherwise,
    // access 0x120117. A null title name stops there. Otherwise
    // XGetSectionHandleA looks up $$XTINFO, $$XTIMAGE, and $$XSIMAGE. A
    // missing $$XTINFO with an empty title skips TitleMeta.xbx; any other
    // case creates it and, when EndOfFile is 0, writes the section or the
    // generated wide string. 0x001508AB then writes \TitleImage.xbx and
    // \SaveImage.xbx. IoCreateSymbolicLink (thunk 0x00202F78) points the
    // drive OBJECT_STRING at that directory. The port does not open
    // \Device\Harddisk0, so the host volume stays intact. The link target
    // is stored the same way \??\D: is. A slot that is already filled
    // returns STATUS_OBJECT_NAME_COLLISION.
    (void)create_directory;
    (void)set_time;
    constexpr int kCollision = static_cast<int>(0xC0000035u);
    constexpr int kNameTooLong = static_cast<int>(0xC0000106u);

    if (drive == nullptr || dir == nullptr || drive->buffer == nullptr || dir->buffer == nullptr || leaf == nullptr)
    {
        return static_cast<int>(0xC000000Du);
    }

    char path[0x168];
    size_t n = 0;
    while (n + 1 < sizeof(path) && dir->buffer[n] != '\0')
    {
        path[n] = dir->buffer[n];
        ++n;
    }
    path[n] = '\0';

    u16 len = dir->length;
    if (len == 0 || len >= sizeof(path))
    {
        return kNameTooLong;
    }
    if (path[len - 1] != '\\')
    {
        path[len] = '\\';
    }
    else
    {
        dir->length = static_cast<u16>(len - 1);
        len = dir->length;
    }

    size_t at = static_cast<size_t>(len) + 1u;
    for (const char* src = leaf; *src != '\0'; ++src)
    {
        if (at + 1 >= sizeof(path))
        {
            return kNameTooLong;
        }
        path[at++] = *src;
    }
    if (at >= sizeof(path))
    {
        return kNameTooLong;
    }
    path[at] = '\0';

    if (title_name != nullptr)
    {
        const std::uintptr_t xtinfo = fn_14af34("$$XTINFO");
        const std::uintptr_t xtimage = fn_14af34("$$XTIMAGE");
        const std::uintptr_t xsimage = fn_14af34("$$XSIMAGE");
        const bool empty_title = title_name[0] == 0;
        // $$XTINFO present: retail writes that section into TitleMeta.xbx.
        // Missing, with a title: _snwprintf of L"%lc%ls%lc%ls%ls" (BOM,
        // L"TitleName", '=', the title, L"\r\n") into 0x8E wchars.
        if (xtinfo == static_cast<std::uintptr_t>(-1) && !empty_title)
        {
            u16 units[0x8E];
            int count = 0;
            auto push_unit = [&](u16 unit)
            {
                if (count < 0x8E)
                {
                    units[count] = unit;
                    ++count;
                }
            };
            push_unit(0xFEFFu);
            const char key[] = "TitleName";
            for (const char* src = key; *src != '\0'; ++src)
            {
                push_unit(static_cast<u8>(*src));
            }
            push_unit(static_cast<u16>('='));
            for (const u16* src = title_name; *src != 0; ++src)
            {
                push_unit(*src);
            }
            push_unit(static_cast<u16>('\r'));
            push_unit(static_cast<u16>('\n'));
            const u32 bytes = static_cast<u32>(count) * 2u;
            if (bytes <= sizeof(g_xapi_title_meta))
            {
                std::memcpy(g_xapi_title_meta, units, bytes);
                if (bytes < sizeof(g_xapi_title_meta))
                {
                    std::memset(g_xapi_title_meta + bytes, 0, sizeof(g_xapi_title_meta) - bytes);
                }
                g_xapi_title_meta_size = bytes;
            }
        }
        (void)xtimage;
        (void)xsimage;
    }

    char* slot = nullptr;
    if (drive->length >= 5 && drive->buffer[4] == 'U')
    {
        slot = g_xapi_u_target;
    }
    else if (drive->length >= 5 && drive->buffer[4] == 'T')
    {
        slot = g_xapi_t_target;
    }
    if (slot == nullptr)
    {
        return static_cast<int>(0xC000000Du);
    }
    if (slot[0] != '\0')
    {
        return kCollision;
    }

    size_t i = 0;
    while (i + 1 < sizeof(g_xapi_u_target) && path[i] != '\0')
    {
        slot[i] = path[i];
        ++i;
    }
    slot[i] = '\0';
    return 0;
}

int fn_150cc6(u32 title_id, const u16* title_name)
{
    // Xbox 0x00150CC6
    // stdcall, ret 8. 0x001510E1 passes the certificate TitleID at +8 and
    // the TitleName pointer at +0xC. _sprintf (0x001661F9) writes "%08lx"
    // into 12 bytes. XapiMapLetterToDirectory maps \??\U: onto
    // \Device\Harddisk0\partition1\UDATA\<id> with a null title name. A
    // negative status is returned as-is. Otherwise the same call maps
    // \??\T: onto \Device\Harddisk0\partition1\TDATA\<id> and passes the
    // title name. That second status is the result. 0x001510E6 treats a
    // negative status other than STATUS_DISK_FULL as a dash boot.
    char leaf[12];
    for (int i = 0; i < 8; ++i)
    {
        const u32 nib = (title_id >> ((7 - i) * 4)) & 0xFu;
        leaf[i] = static_cast<char>(nib < 10u ? '0' + nib : 'a' + (nib - 10u));
    }
    leaf[8] = '\0';

    const int status = xapi_map_letter_to_directory(&g_xapi_u_drive, &g_xapi_u_data, leaf, 1, nullptr, 0);
    if (status < 0)
    {
        return status;
    }
    return xapi_map_letter_to_directory(&g_xapi_t_drive, &g_xapi_t_data, leaf, 1, title_name, 0);
}

int fn_150e37()
{
    // Xbox 0x00150E37
    // cdecl. 0x00151068 calls this after 0x0014E44E. A negative status is
    // passed to 0x00150D1B as (1, 0, 0). A non-negative status continues
    // startup.
    // [0x00010118] is the certificate. The dword at +0 is the header size.
    // Above 0x1D8, SecurityFlags is the dword at +0x1D8; otherwise it is 0.
    // Only bit 0 is kept. AllowedMedia at +0x9C is masked with 0xFFFFFF, and
    // bit 0 replaces that mask with 2 (DVD_X2). Any other media returns 0.
    // The retail certificate size is 0x1EC, SecurityFlags is 3, and
    // AllowedMedia is 2, so the DVD path runs with bit 0 set.
    // NtOpenFile (thunk 0x00202F00, ordinal 202) opens the OBJECT_STRING at
    // 0x0032CEE0 ("\Device\CdRom0"): null root, OBJ_CASE_INSENSITIVE,
    // GENERIC_READ|SYNCHRONIZE (0x80100000), FILE_SHARE_READ, and
    // FILE_SYNCHRONOUS_IO_NONALERT (0x20). A negative open status is the
    // result, with no close.
    // The 0x2C-byte pass-through is zeroed, then length 0x2C, DataIn 1,
    // transfer length 0x1C, and a 0x1C-byte data buffer. The CDB is mode
    // sense 0x5A, page 0x3E, allocation length 0x001C. NtDeviceIoControlFile
    // (thunk 0x00202F28, ordinal 196) sends IOCTL 0x4D014 with that input
    // and a null output. A negative status closes and returns. Byte 0x0B of
    // the page must be 1, or the status is STATUS_UNRECOGNIZED_MEDIA
    // (0xC0000014). Bytes 0x0A and 0x0C must both be nonzero, or the status
    // is STATUS_UNSUCCESSFUL (0xC0000001). Those page failures retry through
    // five attempts.
    // When that status is non-negative and SecurityFlags bit 0 is set, a
    // second pass-through uses transfer length 0x664 and CDB
    // AD 00 FF 02 FD FF FE 00 06 64 00 C0. The same IOCTL fills the buffer.
    // 0x00150D80 checks those 0x664 bytes. A negative result from that call
    // retries up to five times. An IOCTL failure does not. NtClose (thunk
    // 0x00202EF4, ordinal 187) follows a successful open and its status is
    // dropped. The IOCTL or 0x00150D80 status is the result.
    // The host has no Xbox DVD challenge response. Returning a negative
    // status would make 0x00151068 leave through 0x00150D1B. A genuine disc
    // returns 0, and so does the port.
    constexpr u32 kCertificateSize = 0x1ECu;
    constexpr u32 kSecurityFlags = 3u;
    constexpr u32 kAllowedMedia = 2u;

    u32 security_flags = 0;
    if (kCertificateSize > 0x1D8u)
    {
        security_flags = kSecurityFlags;
    }
    security_flags &= 1u;
    u32 media = kAllowedMedia & 0x00FFFFFFu;
    if (security_flags != 0u)
    {
        media = 2u;
    }
    if (media != 2u)
    {
        return 0;
    }
    return 0;
}

int fn_151000(const XapiAnsiString* name)
{
    // Xbox 0x00151000
    // stdcall, ret 4. 0x00151084 passes the OBJECT_STRING at 0x0032CEE8
    // (\Device\Harddisk0\partition1\) when XBE init-flag bit 3 is clear.
    // 0x00150833 checks that volume's cluster size against 0x4000. A
    // negative status, including STATUS_UNRECOGNIZED_VOLUME, sends startup
    // through 0x00150D1B.
    return fn_150833(name, 0x4000);
}

int fn_15118e(const XapiAnsiString* name, u32 bytes_per_cluster)
{
    // Xbox 0x0015118E
    // FATX format for XMountUtilityDrive (0x0014BFCF) and 0x0014C0D4.
    // stdcall, two arguments, returns 1 or 0. 0x0014BFCF passes
    // \Device\Harddisk0\Partition%d\ and 0x0014C0D4 passes \??\Z:, both
    // with cluster size 0x4000 << (XBE init flags >> 30).
    // NtOpenFile takes the OBJECT_STRING in param_1, a null root,
    // OBJ_CASE_INSENSITIVE, FILE_READ_DATA|FILE_WRITE_DATA|SYNCHRONIZE,
    // share 0, and FILE_NO_INTERMEDIATE_BUFFERING|FILE_SYNCHRONOUS_IO_ALERT.
    // IOCTL_DISK_GET_DRIVE_GEOMETRY (0x70000) supplies bytes per sector.
    // bsf of that size is the shift that turns the cluster size into
    // sectors-per-cluster. IOCTL_DISK_GET_PARTITION_INFO (0x74004) supplies
    // the partition length. The sector size rounds up to a page, and the
    // cluster size is raised to that page. The header is max(0x1000, page).
    // Cluster slots are (length / cluster size) + 1. Counts below 0xFFF0 use
    // 16-bit FAT entries; otherwise 4-byte entries. The FAT byte count rounds
    // up to a page. The volume has to hold the header, that FAT, and one
    // cluster, or SetLastError(0x70) and return 0.
    // One page is filled with 0xFF, then the FATX superblock: magic
    // 0x58544146, volume id from the low dword of KeQuerySystemTime, sectors
    // per cluster in the low byte, root cluster 1, and a zero word at +0x10.
    // That page is written at offset 0 and the header tail is zero. The FAT
    // is written next, starting at the header offset: 16-bit media 0xFFF8
    // and 0xFFFF, or 32-bit 0xFFFFFFF8 and 0xFFFFFFFF, then zeros. One
    // cluster of 0xFF follows for the root directory. XMemFree, then
    // FSCTL_DISMOUNT_VOLUME (0x90020), NtClose. A failed write still
    // dismounts. Success returns 1. Other failures go through
    // XapiSetLastNTError, including 0xC000009A when the page allocation fails.
    // The port opens read-only and shared, builds those pages, and does not
    // call NtWriteFile or dismount. A host disk stays intact. When the layout
    // fits, last-error is 5 and the result is 0.
    constexpr long kStatusInvalidParameter = static_cast<long>(0xC000000Du);
    constexpr long kStatusAccessDenied = static_cast<long>(0xC0000022u);
    constexpr long kStatusInsufficient = static_cast<long>(0xC000009Au);
    constexpr long kStatusNotSupported = static_cast<long>(0xC00000BBu);

    auto set_last_nt = [](void* ntdll, long status)
    {
        using RtlFn = unsigned long(__stdcall*)(long);
        RtlFn rtl = nullptr;
        void* proc = ntdll != nullptr ? GetProcAddress(ntdll, "RtlNtStatusToDosError") : nullptr;
        std::memcpy(&rtl, &proc, sizeof(rtl));
        if (rtl != nullptr)
        {
            g_xapi_last_error = rtl(status);
        }
        else
        {
            g_xapi_last_error = 0x1F;
        }
    };

    if (name == nullptr || (name->length > 0 && name->buffer == nullptr))
    {
        g_xapi_last_error = 0x57;
        return 0;
    }

#if !defined(_WIN32)
    (void)bytes_per_cluster;
    (void)kStatusInvalidParameter;
    (void)kStatusAccessDenied;
    (void)kStatusInsufficient;
    (void)kStatusNotSupported;
    g_xapi_last_error = 50;
    return 0;
#else
    struct UnicodeString
    {
        u16 length;
        u16 maximum_length;
        wchar_t* buffer;
    };
    struct ObjectAttributes
    {
        unsigned long length;
        void* root_directory;
        UnicodeString* object_name;
        unsigned long attributes;
        void* security_descriptor;
        void* security_quality_of_service;
    };
    struct IoStatusBlock
    {
        void* pointer;
        std::uintptr_t information;
    };
    using NtOpenFileFn = long(__stdcall*)(void** handle, unsigned long access, ObjectAttributes* attributes,
                                          IoStatusBlock* io, unsigned long share, unsigned long options);
    using NtDeviceIoControlFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx,
                                                     IoStatusBlock* io, unsigned long code, void* in_buf,
                                                     unsigned long in_len, void* out_buf, unsigned long out_len);
    using NtWriteFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                           void* buffer, unsigned long length, std::int64_t* offset, unsigned long* key);
    using NtFsControlFileFn = long(__stdcall*)(void* handle, void* event, void* apc, void* apc_ctx, IoStatusBlock* io,
                                               unsigned long code, void* in_buf, unsigned long in_len, void* out_buf,
                                               unsigned long out_len);
    using NtCloseFn = long(__stdcall*)(void* handle);
    using VirtualAllocFn = void*(__stdcall*)(void* addr, std::uintptr_t size, unsigned long type, unsigned long protect);
    using VirtualFreeFn = int(__stdcall*)(void* addr, std::uintptr_t size, unsigned long type);
    using GetTimeFn = void(__stdcall*)(u32* file_time);

    void* ntdll = GetModuleHandleA("ntdll.dll");
    void* kernel = GetModuleHandleA("kernel32.dll");
    if (ntdll == nullptr || kernel == nullptr)
    {
        set_last_nt(ntdll, kStatusNotSupported);
        return 0;
    }

    NtOpenFileFn nt_open = nullptr;
    NtDeviceIoControlFileFn nt_ioctl = nullptr;
    NtWriteFileFn nt_write = nullptr;
    NtFsControlFileFn nt_fsctl = nullptr;
    NtCloseFn nt_close = nullptr;
    VirtualAllocFn virtual_alloc = nullptr;
    VirtualFreeFn virtual_free = nullptr;
    GetTimeFn get_time = nullptr;
    void* open_proc = GetProcAddress(ntdll, "NtOpenFile");
    void* ioctl_proc = GetProcAddress(ntdll, "NtDeviceIoControlFile");
    void* write_proc = GetProcAddress(ntdll, "NtWriteFile");
    void* fsctl_proc = GetProcAddress(ntdll, "NtFsControlFile");
    void* close_proc = GetProcAddress(ntdll, "NtClose");
    void* alloc_proc = GetProcAddress(kernel, "VirtualAlloc");
    void* free_proc = GetProcAddress(kernel, "VirtualFree");
    void* time_proc = GetProcAddress(kernel, "GetSystemTimeAsFileTime");
    std::memcpy(&nt_open, &open_proc, sizeof(nt_open));
    std::memcpy(&nt_ioctl, &ioctl_proc, sizeof(nt_ioctl));
    std::memcpy(&nt_write, &write_proc, sizeof(nt_write));
    std::memcpy(&nt_fsctl, &fsctl_proc, sizeof(nt_fsctl));
    std::memcpy(&nt_close, &close_proc, sizeof(nt_close));
    std::memcpy(&virtual_alloc, &alloc_proc, sizeof(virtual_alloc));
    std::memcpy(&virtual_free, &free_proc, sizeof(virtual_free));
    std::memcpy(&get_time, &time_proc, sizeof(get_time));
    if (nt_open == nullptr || nt_ioctl == nullptr || nt_write == nullptr || nt_fsctl == nullptr || nt_close == nullptr ||
        virtual_alloc == nullptr || virtual_free == nullptr)
    {
        set_last_nt(ntdll, kStatusNotSupported);
        return 0;
    }

    std::wstring wide(static_cast<size_t>(name->length), L'\0');
    for (u16 i = 0; i < name->length; ++i)
    {
        wide[i] = static_cast<unsigned char>(name->buffer[i]);
    }
    const auto wide_bytes = static_cast<size_t>(wide.size()) * sizeof(wchar_t);
    if (wide_bytes > 0xFFFFu)
    {
        set_last_nt(ntdll, static_cast<long>(0xC0000106));
        return 0;
    }

    UnicodeString unicode{};
    unicode.length = static_cast<u16>(wide_bytes);
    unicode.maximum_length = unicode.length;
    unicode.buffer = wide.empty() ? nullptr : wide.data();

    ObjectAttributes attributes{};
    attributes.length = sizeof(attributes);
    attributes.object_name = &unicode;
    attributes.attributes = 0x40;

    // Retail asks for read|write and share 0. The port opens read-only and
    // shared so a \Device\Harddisk or \??\Z: name cannot lock or rewrite a
    // host volume. commit_fatx stays clear for that reason.
    const bool commit_fatx = false;
    IoStatusBlock io{};
    void* handle = nullptr;
    long status = nt_open(&handle, 0x100001u, &attributes, &io, 3u, 0x18u);
    if (status < 0)
    {
        set_last_nt(ntdll, status);
        return 0;
    }

    auto close_handle = [&]()
    {
        if (handle != nullptr)
        {
            nt_close(handle);
            handle = nullptr;
        }
    };

    u8 geometry[0x18]{};
    status = nt_ioctl(handle, nullptr, nullptr, nullptr, &io, 0x70000u, nullptr, 0, geometry, 0x18u);
    if (status < 0)
    {
        close_handle();
        set_last_nt(ntdll, status);
        return 0;
    }

    u32 bytes_per_sector = 0;
    std::memcpy(&bytes_per_sector, geometry + 0x14, sizeof(bytes_per_sector));
    if (bytes_per_sector == 0)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }

    const u32 sector_shift = static_cast<u32>(std::countr_zero(bytes_per_sector)) & 0x1Fu;
    u8 part_info[0x20]{};
    status = nt_ioctl(handle, nullptr, nullptr, nullptr, &io, 0x74004u, nullptr, 0, part_info, 0x20u);
    if (status < 0)
    {
        close_handle();
        set_last_nt(ntdll, status);
        return 0;
    }

    const u32 chunk = (bytes_per_sector + 0xFFFu) & 0xFFFFF000u;
    if (chunk == 0 || (bytes_per_cluster != 0 && (bytes_per_cluster % chunk) != 0 && bytes_per_cluster > chunk))
    {
        // A span that is not a multiple of the page never reaches zero in
        // the retail write loop. Reject it instead of spinning.
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }
    if (bytes_per_cluster < chunk)
    {
        bytes_per_cluster = chunk;
    }
    if (bytes_per_cluster == 0 || (bytes_per_cluster % chunk) != 0)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInvalidParameter);
        return 0;
    }

    const u32 header = chunk > 0x1000u ? chunk : 0x1000u;
    u64 part_len = 0;
    std::memcpy(&part_len, part_info + 8, sizeof(part_len));
    const u32 part_high = static_cast<u32>(part_len >> 32);
    const u32 part_low = static_cast<u32>(part_len);
    if (part_high == 0 && header >= part_low)
    {
        close_handle();
        g_xapi_last_error = 0x70;
        return 0;
    }

    const u32 count = static_cast<u32>(part_len / bytes_per_cluster) + 1u;
    const bool fat16 = count < 0xFFF0u;
    const u32 fat_bytes = fat16 ? count * 2u : count * 4u;
    const u32 fat_aligned = (fat_bytes + chunk - 1u) & ~(chunk - 1u);
    const u64 need = static_cast<u64>(header) + fat_aligned + bytes_per_cluster;
    if (part_len < need)
    {
        close_handle();
        g_xapi_last_error = 0x70;
        return 0;
    }

    u8* page = static_cast<u8*>(virtual_alloc(nullptr, chunk, 0x3000u, 0x04u));
    if (page == nullptr)
    {
        close_handle();
        set_last_nt(ntdll, kStatusInsufficient);
        return 0;
    }

    auto write_page = [&](u64 offset, u32 length) -> long
    {
        if (!commit_fatx)
        {
            (void)offset;
            (void)length;
            return kStatusAccessDenied;
        }
        std::int64_t byte_offset = static_cast<std::int64_t>(offset);
        return nt_write(handle, nullptr, nullptr, nullptr, &io, page, length, &byte_offset, nullptr);
    };

    auto fail_write = [&](long write_status) -> int
    {
        virtual_free(page, 0, 0x8000u);
        page = nullptr;
        if (commit_fatx)
        {
            nt_fsctl(handle, nullptr, nullptr, nullptr, &io, 0x90020u, nullptr, 0, nullptr, 0);
        }
        close_handle();
        set_last_nt(ntdll, write_status);
        return 0;
    };

    std::memset(page, 0xFF, chunk);
    u32* words = reinterpret_cast<u32*>(page);
    words[0] = 0x58544146u;
    u32 volume_id = 0;
    if (get_time != nullptr)
    {
        u32 file_time[2]{};
        get_time(file_time);
        volume_id = file_time[0];
    }
    words[1] = volume_id;
    words[2] = (bytes_per_cluster >> sector_shift) & 0xFFu;
    words[3] = 1;
    reinterpret_cast<u16*>(page)[8] = 0;

    u64 offset = 0;
    for (u32 left = header; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        std::memset(page, 0, chunk);
        offset += chunk;
    }

    offset = header;
    std::memset(page, 0, chunk);
    if (fat16)
    {
        reinterpret_cast<u16*>(page)[0] = 0xFFF8;
        reinterpret_cast<u16*>(page)[1] = 0xFFFF;
    }
    else
    {
        words[0] = 0xFFFFFFF8u;
        words[1] = 0xFFFFFFFFu;
    }
    for (u32 left = fat_aligned; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        words[0] = 0;
        words[1] = 0;
        offset += chunk;
    }

    std::memset(page, 0xFF, chunk);
    for (u32 left = bytes_per_cluster; left != 0; left -= chunk)
    {
        status = write_page(offset, chunk);
        if (status < 0)
        {
            return fail_write(status);
        }
        offset += chunk;
    }

    virtual_free(page, 0, 0x8000u);
    if (commit_fatx)
    {
        nt_fsctl(handle, nullptr, nullptr, nullptr, &io, 0x90020u, nullptr, 0, nullptr, 0);
    }
    close_handle();
    if (!commit_fatx)
    {
        set_last_nt(ntdll, kStatusAccessDenied);
        return 0;
    }
    return 1;
#endif
}

// 0x00549508. Bit 0 of XC_MISC (config value 0x11). The timer DPC at
// 0x00151AA3 powers off only when this is nonzero.
u32 g_xapi_auto_power_enabled = 0;
// 0x0054950C. Set when XQueryValue of that config value fails.
u32 g_xapi_auto_power_failed = 0;
// KeSetTimer due time for the notification timer at 0x005494E0.
std::int64_t g_xapi_auto_power_due = 0;
// HalInitiateShutdown requested by the DPC. The host stays up.
u32 g_xapi_auto_power_shutdown = 0;
void (*g_xapi_auto_power_dpc)() = nullptr;

void fn_151aa3()
{
    // Xbox 0x00151AA3
    // KDPC for the auto power-down timer, stdcall, ret 0x10. The four
    // arguments are unused. A nonzero dword at 0x00549508 calls
    // HalInitiateShutdown (thunk 0x00203008, ordinal 360), which asks the
    // SMC to power off. The port records that request and keeps running.
    if (g_xapi_auto_power_enabled != 0)
    {
        g_xapi_auto_power_shutdown = 1;
    }
}

void fn_151a88()
{
    // Xbox 0x00151A88
    // XAutoPowerDownResetTimer. KeSetTimer (thunk 0x00203004, ordinal 149)
    // on the timer at 0x005494E0, with the DPC at 0x005494C0. The due time
    // immediate is 0xFFFFFFCDB5659000: a relative interval of 216000000000
    // units of 100 nanoseconds, which is 6 hours. 0x0015059A builds that
    // same two's-complement timeout from 21600000 milliseconds.
    std::int64_t due = 0;
    fn_15059a(&due, 6u * 60u * 60u * 1000u);
    g_xapi_auto_power_due = due;
}

void fn_151ab5()
{
    // Xbox 0x00151AB5
    // Called from 0x0015102E during XAPI startup, before the process heap.
    // KeInitializeDpc (thunk 0x00202FAC, ordinal 107) binds 0x005494C0 to
    // 0x00151AA3 with a null context. KeInitializeTimerEx (thunk 0x00202F48,
    // ordinal 113) makes 0x005494E0 a notification timer (type 0). Both
    // 0x00549508 and 0x0054950C are cleared.
    // XboxHardwareInfo (thunk 0x00202FF0, ordinal 322) is one byte of flags.
    // Bit 3 skips the config read. Retail leaves that bit clear, so
    // XQueryValue (0x00150811) runs: ExQueryNonVolatileSetting (ordinal 24)
    // for value 0x11 (XC_MISC), four bytes, then RtlNtStatusToDosError
    // (ordinal 301). A zero DOS error stores the value masked with 1 into
    // 0x00549508. Any other error stores 1 into 0x0054950C. 0x00151A88 then
    // arms the six-hour timer. The host has no EEPROM, so the query fails
    // and auto power-down stays off.
    g_xapi_auto_power_dpc = &fn_151aa3;
    g_xapi_auto_power_enabled = 0;
    g_xapi_auto_power_failed = 0;

    constexpr u32 kHardwareFlags = 0;
    if ((kHardwareFlags & 8u) == 0u)
    {
        u32 misc = 0;
        // No EEPROM image. ExQueryNonVolatileSetting does not succeed, and
        // the DOS error from that status is nonzero (ERROR_NOT_SUPPORTED).
        const int query_status = 50;
        if (query_status == 0)
        {
            g_xapi_auto_power_enabled = misc & 1u;
        }
        else
        {
            g_xapi_auto_power_failed = 1;
        }
    }

    fn_151a88();
}

int xapi_fs_ntstatus(const std::filesystem::path& opened, const std::error_code& ec)
{
    constexpr int kNameNotFound = static_cast<int>(0xC0000034u);
    constexpr int kPathNotFound = static_cast<int>(0xC000003Au);
    constexpr int kNotDirectory = static_cast<int>(0xC0000103u);
    constexpr int kAccessDenied = static_cast<int>(0xC0000022u);
    constexpr int kDirNotEmpty = static_cast<int>(0xC0000101u);
    constexpr int kInvalid = static_cast<int>(0xC000000Du);
    constexpr int kUnsuccessful = static_cast<int>(0xC0000001u);

    if (!ec)
    {
        return 0;
    }
    if (ec == std::errc::no_such_file_or_directory || ec.value() == 2 || ec.value() == 3)
    {
        std::filesystem::path leaf = opened;
        if (leaf.filename().empty())
        {
            leaf = leaf.parent_path();
        }
        std::error_code parent_ec;
        const auto parent = leaf.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, parent_ec))
        {
            return kPathNotFound;
        }
        return kNameNotFound;
    }
    if (ec == std::errc::not_a_directory || ec.value() == 267)
    {
        return kNotDirectory;
    }
    if (ec == std::errc::permission_denied || ec.value() == 5)
    {
        return kAccessDenied;
    }
    if (ec == std::errc::directory_not_empty || ec.value() == 145)
    {
        return kDirNotEmpty;
    }
    if (ec == std::errc::invalid_argument || ec.value() == 123)
    {
        return kInvalid;
    }
    return kUnsuccessful;
}

int xapi_delete_file_entry(const std::filesystem::path& path)
{
    // 0x001514B2 file arm. FILE_ATTRIBUTE_READONLY (bit 0) is cleared with
    // NtSetInformationFile class 4 before FileDispositionInformation. The
    // attribute dword keeps bits 0x3126 and sets FILE_ATTRIBUTE_NORMAL.
    // A failed attribute write skips the delete and returns that status.
    std::error_code ec;
    const auto perms = std::filesystem::status(path, ec).permissions();
    if (ec)
    {
        return xapi_fs_ntstatus(path, ec);
    }
    if (perms != std::filesystem::perms::unknown &&
        (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        std::filesystem::permissions(path, perms | std::filesystem::perms::owner_write, ec);
        if (ec)
        {
            return static_cast<int>(0xC0000022u);
        }
    }

    ec.clear();
    if (!std::filesystem::remove(path, ec) || ec)
    {
        if (!ec)
        {
            return static_cast<int>(0xC0000034u);
        }
        return xapi_fs_ntstatus(path, ec);
    }
    return 0;
}

int xapi_delete_directory_tree(const std::filesystem::path& dir)
{
    // 0x001514B2. NtQueryDirectoryFile on the open directory, class 1
    // (FileDirectoryInformation), 0x146 bytes, one entry, a null pattern,
    // restart on the first call. Each name is opened under that directory.
    // A directory (attribute bit 0x10) is walked by calling this again.
    // A file with the readonly bit is given FILE_ATTRIBUTE_NORMAL first.
    // FileDispositionInformation then deletes the child. The first failure
    // closes the child and returns without deleting this directory.
    // STATUS_NO_MORE_FILES (0x80000006) or STATUS_NO_SUCH_FILE (0xC000000F)
    // ends the scan. This directory is then marked normal (readonly
    // stripped; that status is ignored) and deleted. The disposition
    // status is the result.
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return xapi_fs_ntstatus(dir, ec);
    }

    const std::filesystem::directory_iterator end;
    while (it != end)
    {
        const std::filesystem::path child = it->path();
        const std::filesystem::path name = child.filename();
        std::error_code type_ec;
        const auto sym = it->symlink_status(type_ec);
        const bool child_dir = !type_ec && std::filesystem::is_directory(sym);
        std::error_code step_ec;
        it.increment(step_ec);

        if (name != "." && name != "..")
        {
            int status = 0;
            if (type_ec)
            {
                status = xapi_fs_ntstatus(child, type_ec);
            }
            else if (child_dir)
            {
                status = xapi_delete_directory_tree(child);
            }
            else
            {
                status = xapi_delete_file_entry(child);
            }
            if (status < 0)
            {
                return status;
            }
        }
        if (step_ec)
        {
            return xapi_fs_ntstatus(dir, step_ec);
        }
    }
    it = std::filesystem::directory_iterator();

    const auto perms = std::filesystem::status(dir, ec).permissions();
    if (!ec && perms != std::filesystem::perms::unknown &&
        (perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none)
    {
        std::filesystem::permissions(dir, perms | std::filesystem::perms::owner_write, ec);
    }
    ec.clear();
    if (!std::filesystem::remove(dir, ec) || ec)
    {
        if (!ec)
        {
            return static_cast<int>(0xC0000034u);
        }
        return xapi_fs_ntstatus(dir, ec);
    }
    return 0;
}

int fn_15162f(const char* path, void* root)
{
    // Xbox 0x0015162F
    // Called from 0x0014C972 when the save name in SaveMeta.xbx matches.
    // RtlInitAnsiString on the path, then NtOpenFile with that ANSI string,
    // RootDirectory = param_2, OBJ_CASE_INSENSITIVE, access
    // FILE_LIST_DIRECTORY|FILE_WRITE_ATTRIBUTES|DELETE|SYNCHRONIZE (0x110101),
    // share read|write, and FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|
    // FILE_OPEN_FOR_BACKUP_INTENT (0x4021). Success hands the handle and a
    // 0x148-byte directory buffer to 0x001514B2, then NtClose. The NTSTATUS
    // is returned as-is; the caller maps it with RtlNtStatusToDosError.
    // A null root is an absolute path. The port has no NT directory-handle
    // table, so a non-null root is STATUS_INVALID_HANDLE. A null or empty
    // name is STATUS_OBJECT_NAME_INVALID. A file is STATUS_NOT_A_DIRECTORY
    // and is left in place.
    constexpr int kInvalidHandle = static_cast<int>(0xC0000008u);
    constexpr int kNameInvalid = static_cast<int>(0xC0000033u);
    constexpr int kNotDirectory = static_cast<int>(0xC0000103u);

    if (root != nullptr)
    {
        return kInvalidHandle;
    }
    if (path == nullptr || path[0] == '\0')
    {
        return kNameInvalid;
    }

    const std::filesystem::path fs_path(path);
    std::error_code ec;
    if (std::filesystem::is_directory(fs_path, ec))
    {
        return xapi_delete_directory_tree(fs_path);
    }
    if (!ec)
    {
        std::error_code exists_ec;
        if (std::filesystem::exists(fs_path, exists_ec) && !exists_ec)
        {
            return kNotDirectory;
        }
    }
    if (!ec)
    {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
    }
    return xapi_fs_ntstatus(fs_path, ec);
}

// 0x00338278. The image dword is 0x2694. Nothing stores this address.
// _87except skips _matherr while the dword is nonzero. The pow entries at
// 0x00170460 and 0x0017049E take the same test.
u32 g_crt_fp_skip_matherr = 0x2694u;

// 0x004DD0D0. BSS, image value 0. Nothing in the image writes this address.
// The math epilogues compare it and, when it is nonzero, restore the saved
// x87 control word without building an exception record. Sites: 0x00167548,
// 0x001675AC, 0x00167D37, 0x00167D54, 0x0017052A, 0x00170726, 0x0017078C.
u32 g_crt_fp_skip_epilogue = 0;

struct CrtMathError
{
    int type;
    char* name;
    double arg1;
    double arg2;
    double retval;
};

// EDX descriptor saved at [ebp-0x94]. Byte 0 is the name length. The C string
// starts at +1 and is padded to 12 bytes. +0x0D is 1 for a unary operation
// (sinh, cosh, tanh, exp). +0x0E is the opcode passed to _87except.
struct CrtMathDesc
{
    u8 name_len;
    char name[12];
    u8 unary;
    u8 opcode;
};

static_assert(offsetof(CrtMathDesc, unary) == 0x0D);
static_assert(offsetof(CrtMathDesc, opcode) == 0x0E);

// Caller's frame. 0x00170523 and the fall-through at 0x0017052A never build
// their own EBP; both are called with the math wrapper's frame still in EBP
// and the result in ST0.
struct CrtMathCall
{
    u8 flags;
    u16 cw;
    u8 status;
    CrtMathDesc* desc;
    double arg1;
    double arg2;
    double result;
};

void crt_restore_x87_cw(u32 raw)
{
    // __ctrlfp (0x00168809) with mask 0xFFFF. fnstcw / fldcw on the raw
    // control word. The host control word uses the _controlfp layout.
#if defined(_WIN32)
    u32 value = 0;
    if ((raw & 0x01u) != 0)
    {
        value |= _EM_INVALID;
    }
    if ((raw & 0x02u) != 0)
    {
        value |= _EM_DENORMAL;
    }
    if ((raw & 0x04u) != 0)
    {
        value |= _EM_ZERODIVIDE;
    }
    if ((raw & 0x08u) != 0)
    {
        value |= _EM_OVERFLOW;
    }
    if ((raw & 0x10u) != 0)
    {
        value |= _EM_UNDERFLOW;
    }
    if ((raw & 0x20u) != 0)
    {
        value |= _EM_INEXACT;
    }
    switch (raw & 0xC00u)
    {
    case 0x400u:
        value |= _RC_DOWN;
        break;
    case 0x800u:
        value |= _RC_UP;
        break;
    case 0xC00u:
        value |= _RC_CHOP;
        break;
    default:
        value |= _RC_NEAR;
        break;
    }
    switch (raw & 0x300u)
    {
    case 0x000u:
        value |= _PC_24;
        break;
    case 0x200u:
        value |= _PC_53;
        break;
    default:
        value |= _PC_64;
        break;
    }
    unsigned int previous = 0;
    (void)_controlfp_s(&previous, value, _MCW_EM | _MCW_RC | _MCW_PC);
#else
    int round = FE_TONEAREST;
    switch (raw & 0xC00u)
    {
    case 0x400u:
        round = FE_DOWNWARD;
        break;
    case 0x800u:
        round = FE_UPWARD;
        break;
    case 0xC00u:
        round = FE_TOWARDZERO;
        break;
    default:
        break;
    }
    std::fesetround(round);
#endif
}

int crt_math_exc_handled(u32 flags, double* retval, u32 cw)
{
    // _handle_exc (0x00168493). Software flag bits, not the x87 status
    // layout: 1 overflow, 2 underflow, 4 zero-divide, 8 invalid, 0x10
    // inexact. A masked primary exception is retired and the rest of the
    // primary tests are skipped. Inexact is then retired when precision
    // is masked (control-word bit 5). The return is 1 only when every
    // raised bit was masked. 0x0016882D plants the sticky x87 status bit;
    // ported callers do not read that word.
    u32 remain = flags & 0x1fu;
    bool primary = false;
    if ((flags & 8u) != 0)
    {
        if ((cw & 1u) != 0)
        {
            remain &= ~8u;
            primary = true;
        }
    }
    if (!primary && (flags & 4u) != 0)
    {
        if ((cw & 4u) != 0)
        {
            remain &= ~4u;
            primary = true;
        }
    }
    if (!primary && (flags & 1u) != 0)
    {
        if ((cw & 8u) != 0)
        {
            const double positive_inf = std::bit_cast<double>(0x7FF0000000000000ULL);
            const double max_finite = std::bit_cast<double>(0x7FEFFFFFFFFFFFFFULL);
            const bool positive = *retval > 0.0;
            double adjusted = *retval;
            switch (cw & 0xC00u)
            {
            case 0x400u:
                adjusted = positive ? max_finite : -positive_inf;
                break;
            case 0x800u:
                adjusted = positive ? positive_inf : -max_finite;
                break;
            case 0xC00u:
                adjusted = positive ? max_finite : -max_finite;
                break;
            case 0x000u:
                adjusted = positive ? positive_inf : -positive_inf;
                break;
            default:
                break;
            }
            *retval = adjusted;
            remain &= ~1u;
            primary = true;
        }
    }
    if (!primary && (flags & 2u) != 0)
    {
        if ((cw & 0x10u) != 0)
        {
            // The math routine hands over a value scaled by 2^1536 so the
            // 80-bit interim did not underflow. Scale it back. Below
            // 2^-1074 the result is a signed zero. From there up to the
            // smallest normal the mantissa is shifted, discarding low bits.
            if (*retval == 0.0)
            {
                remain &= ~2u;
            }
            else
            {
                const bool neg = std::signbit(*retval);
                int exp = 0;
                const double mant = std::frexp(*retval, &exp);
                exp -= 1536;
                if (exp < -1074)
                {
                    *retval = neg ? -0.0 : 0.0;
                }
                else if (exp >= -1021)
                {
                    double scaled = std::ldexp(std::fabs(mant), exp);
                    if (neg)
                    {
                        scaled = -scaled;
                    }
                    *retval = scaled;
                }
                else
                {
                    u64 bits = std::bit_cast<u64>(std::fabs(mant));
                    bits &= 0x000fffffffffffffULL;
                    bits |= 0x0010000000000000ULL;
                    int shift = -1021 - exp;
                    while (shift > 0)
                    {
                        bits >>= 1u;
                        shift -= 1;
                    }
                    double scaled = std::bit_cast<double>(bits);
                    if (neg)
                    {
                        scaled = -scaled;
                    }
                    *retval = scaled;
                }
                remain &= ~2u;
            }
            primary = true;
        }
    }
    if ((flags & 0x10u) != 0 && (cw & 0x20u) != 0)
    {
        remain &= ~0x10u;
    }
    return remain == 0 ? 1 : 0;
}

void fn_16f095(int opcode, CrtMathError* exc, u16* cw_ptr)
{
    // Xbox 0x0016F095
    // _87except. The unary dispatcher at 0x0016B723 and the binary one at
    // 0x0016B70C build a struct _exception (type, name, arg1, arg2, retval)
    // and call here with the operation code in the first argument, that
    // record, and the caller's x87 control word. Codes 0x10, 0x16, and 0x1D
    // are atan2, fmod, and pow: two operands. The other codes are unary.
    //
    // Type 1 (_DOMAIN) and 5 (_TLOSS) raise invalid (8). Type 2 (_SING)
    // raises zero-divide (4). Type 3 (_OVERFLOW) raises overflow|inexact
    // (0x11). Type 4 (_UNDERFLOW) raises underflow|inexact (0x12). Type 8
    // is the inexact report from the dispatcher epilogue and raises 0x10.
    // Type 7 is the pow special case: the type is rewritten to _DOMAIN and
    // the IEEE handler is skipped. Any other type skips it as well.
    //
    // _handle_exc (0x00168493) retires masked bits and, for overflow and
    // underflow, rewrites retval. A zero return calls _raise_exc
    // (0x001681EF), which raises the Win32 float status (invalid 0xC0000090,
    // zero-divide 0xC000008E, overflow 0xC0000091, underflow 0xC0000093,
    // inexact 0xC000008F) with one argument, the IEEE record. The later
    // status overwrites the earlier one, so invalid wins over zero-divide,
    // then overflow, underflow, and inexact.
    //
    // __ctrlfp (0x00168809) then reloads the saved control word. Type 8,
    // or a nonzero dword at 0x00338278, skips _matherr (0x0016DA59). That
    // body returns 0, and a nonzero return would skip the errno store.
    // 0x00168680 stores EDOM (0x21) for type 1 and ERANGE (0x22) for types
    // 2 and 3, through _errno (0x00168AF5).
    const u32 saved_cw = static_cast<u32>(*cw_ptr);
    u32 flags = 0;
    bool report = true;
    switch (exc->type)
    {
    case 1:
    case 5:
        flags = 8u;
        break;
    case 2:
        flags = 4u;
        break;
    case 3:
        flags = 0x11u;
        break;
    case 4:
        flags = 0x12u;
        break;
    case 7:
        exc->type = 1;
        report = false;
        break;
    case 8:
        flags = 0x10u;
        break;
    default:
        report = false;
        break;
    }

    if (report)
    {
        if (crt_math_exc_handled(flags, &exc->retval, saved_cw) == 0)
        {
            alignas(8) u8 record[0x80] = {};
            u32* operand2_flags = reinterpret_cast<u32*>(record + 0x40);
            if (opcode == 0x10 || opcode == 0x16 || opcode == 0x1d)
            {
                std::memcpy(record + 0x30, &exc->arg2, sizeof(exc->arg2));
                *operand2_flags = (*operand2_flags & 0xffffffe3u) | 3u;
            }
            else
            {
                *operand2_flags &= 0xfffffffeu;
            }

            u32 cause = 0;
            u32 code = 0;
            if ((flags & 0x10u) != 0)
            {
                cause |= 1u;
                code = 0xC000008Fu;
            }
            if ((flags & 2u) != 0)
            {
                cause |= 2u;
                code = 0xC0000093u;
            }
            if ((flags & 1u) != 0)
            {
                cause |= 4u;
                code = 0xC0000091u;
            }
            if ((flags & 4u) != 0)
            {
                cause |= 8u;
                code = 0xC000008Eu;
            }
            if ((flags & 8u) != 0)
            {
                cause |= 0x10u;
                code = 0xC0000090u;
            }
            std::memcpy(record + 4, &cause, sizeof(cause));
#if defined(_WIN32)
            std::uintptr_t info = reinterpret_cast<std::uintptr_t>(record);
            RaiseException(code, 0, 1, &info);
#else
            (void)code;
#endif
        }
    }

    crt_restore_x87_cw(saved_cw);

    if (exc->type != 8 && g_crt_fp_skip_matherr == 0)
    {
        // 0x0016DA59 returns 0, so control falls through to the errno store.
        const int matherr_result = 0;
        if (matherr_result != 0)
        {
            return;
        }
    }

    if (exc->type == 1)
    {
        *fn_168af5() = 0x21;
    }
    else if (exc->type > 1 && exc->type <= 3)
    {
        *fn_168af5() = 0x22;
    }
}

bool crt_fpu_inexact()
{
    // fnstsw bit 5 (precision). The host status word uses a different layout.
#if defined(_WIN32)
    return (_statusfp() & _SW_INEXACT) != 0;
#else
    return std::fetestexcept(FE_INEXACT) != 0;
#endif
}

double crt_math_scale_underflow(double value)
{
    // fscale by +1536.0 (0x0020A244). A result still below the smallest
    // normal is multiplied by +0.0 (0x0020A254), which keeps the sign.
    value = std::ldexp(value, 1536);
    const double min_normal = std::bit_cast<double>(0x0010000000000000ULL);
    if (std::fabs(value) < min_normal)
    {
        value *= 0.0;
    }
    return value;
}

double crt_math_scale_overflow(double value)
{
    // fscale by -1536.0 (0x0020A23C). A result still above the largest
    // finite double is multiplied by +infinity (0x0020A24C).
    value = std::ldexp(value, -1536);
    const double max_finite = std::bit_cast<double>(0x7FEFFFFFFFFFFFFFULL);
    if (std::fabs(value) > max_finite)
    {
        const double positive_inf = std::bit_cast<double>(0x7FF0000000000000ULL);
        value *= positive_inf;
    }
    return value;
}

double fn_17052a(CrtMathCall& call)
{
    // Xbox 0x0017052A
    // Math-wrapper epilogue. The caller's EBP is still the wrapper frame
    // and the result is in ST0. 0x00170523 clears flags bit 0 and falls in.
    // 0x00170460 and 0x0017049E call this entry with bit 0 already set.
    //
    // 0x004DD0D0 nonzero loads the control word saved at [ebp-0xA4] and
    // returns ST0 unchanged (0x00170582). Otherwise ST0 is stored as a
    // double at [ebp-0x2D0] and the status byte at [ebp-0x90] selects the
    // record:
    //   0     inexact check
    //   -1    overflow when the double's exponent field is 0x7FF0, else
    //         the inexact check
    //   -2    underflow when that field is 0, overflow when it is 0x7FF0,
    //         else the inexact check
    //   else  the byte, sign-extended, is the exception type
    // A second test for 0 after the -1/-2 compares restores the control
    // word and returns. Status 0 already took the inexact check, so that
    // test is unreachable.
    //
    // The inexact check reports type 8 only when control-word bit 5 is
    // clear (precision unmasked) and status-word bit 5 is set. Any other
    // inexact outcome loads the saved control word and returns ST0.
    // Overflow (type 3) scales ST0 by 2^-1536. Underflow (type 4) scales
    // by 2^1536. The exponent field is the top word of the stored double,
    // masked with 0x7FF0.
    //
    // The record at [ebp-0x8E] is type, name at descriptor+1, the operands,
    // and the scaled ST0. Flags bit 0 clear copies the operands from
    // [ebp+8] and, when descriptor+0x0D is not 1, from [ebp+0x10]. Bit 0
    // set keeps the slots the caller already wrote; this port holds those
    // in arg1 and arg2. _87except (0x0016F095) is called with the opcode
    // at descriptor+0x0E, that record, and the saved control word. ST0 is
    // reloaded from the record's retval, then the control word is loaded
    // again at 0x00170582. _87except already reloads it; the port leaves
    // that restore to fn_16f095.
    if (g_crt_fp_skip_epilogue != 0)
    {
        crt_restore_x87_cw(call.cw);
        return call.result;
    }

    const u64 bits = std::bit_cast<u64>(call.result);
    const u16 exponent = static_cast<u16>((bits >> 48) & 0x7FF0u);
    const int status = static_cast<int>(static_cast<std::int8_t>(call.status));

    int type = 0;
    bool report = false;
    double value = call.result;
    if (status == -1)
    {
        if (exponent == 0x7FF0u)
        {
            type = 3;
            value = crt_math_scale_overflow(value);
            report = true;
        }
    }
    else if (status == -2)
    {
        if (exponent == 0)
        {
            type = 4;
            value = crt_math_scale_underflow(value);
            report = true;
        }
        else if (exponent == 0x7FF0u)
        {
            type = 3;
            value = crt_math_scale_overflow(value);
            report = true;
        }
    }
    else if (status != 0)
    {
        type = status;
        report = true;
    }

    if (!report)
    {
        if ((call.cw & 0x20u) != 0 || !crt_fpu_inexact())
        {
            crt_restore_x87_cw(call.cw);
            return call.result;
        }
        type = 8;
    }

    CrtMathError exc{};
    exc.type = type;
    exc.name = call.desc->name;
    exc.arg1 = call.arg1;
    if ((call.flags & 1u) != 0 || call.desc->unary != 1)
    {
        exc.arg2 = call.arg2;
    }
    exc.retval = value;
    u16 cw = call.cw;
    const int opcode = static_cast<int>(static_cast<std::int8_t>(call.desc->opcode));
    fn_16f095(opcode, &exc, &cw);
    call.result = exc.retval;
    return call.result;
}

double fn_170523(CrtMathCall& call)
{
    // Xbox 0x00170523
    // Seven-byte entry: and byte ptr [ebp-0x2C8], 0xFE. It clears bit 0 of
    // the dispatch flag and falls into 0x0017052A. Bit 0 set means the
    // operands were already stored in the exception record; clearing it
    // forces the copy from the wrapper's stack args at [ebp+8] and, for a
    // binary op, [ebp+0x10]. The unary wrappers at 0x00170671 and the
    // two-argument wrapper at 0x001704DB call this entry.
    call.flags = static_cast<u8>(call.flags & 0xFEu);
    return fn_17052a(call);
}

void fn_16f266(void* stream)
{
    // Xbox 0x0016F266
    // __finally of ungetc (0x0016F22B). __SEH_prolog (0x00166D58) is entered
    // with local size 0x0C and the scope table at 0x00209ED8: one entry,
    // enclosing level -1, a null filter, and this handler. ungetc pushes
    // the FILE* at [ebp+0xc] and calls _lock_file (0x00168970), stores try
    // level 0 at [ebp-4], then calls _ungetc_lk (0x0016F1BF) with the
    // character at [ebp+8] and that FILE*. The success path at 0x0016F258
    // sets the try level to -1 and calls here. _local_unwind2 (0x00167062)
    // runs the same handler. The ten bytes push [ebp+0xc], call _unlock_file
    // (0x001689C2), pop the cdecl argument, and return.
    //
    // _unlock_file compares the pointer with _iob and with 0x00337F58.
    // Inside that span the lock number is (byte delta >> 5) + 0x10
    // (_STREAM_LOCKS). _unlock (0x0016810F) then leaves the critical
    // section stored in _locktable[index] at 0x00337AB0. Outside the
    // span it calls RtlLeaveCriticalSection (kernel ordinal 294, thunk
    // 0x00202EA4) on the critical section at FILE+0x20. A null pointer
    // would take that path at address 0x20; the port returns instead
    // of faulting.
    fn_16619a(stream);
}
} // namespace

int fn_16f172(const void* lp, u32 ucb)
{
    // Xbox 0x0016F172
    // _ValidateRead. cdecl; the caller pops the address and the byte count.
    // ESI is set to 1, then XAPILIB::IsBadReadPtr (0x0014A840) is called.
    // A nonzero result clears ESI, and EAX returns that flag: 1 when the
    // range can be read, 0 when it cannot. A zero count is readable.
    // FindHandler (0x0016BEF6) probes one byte of a rethrown exception and
    // treats 0 as __inconsistency.
#if defined(_WIN32)
    return IsBadReadPtr(lp, ucb) == 0 ? 1 : 0;
#else
    if (ucb == 0)
    {
        return 1;
    }
    return lp != nullptr ? 1 : 0;
#endif
}

int fn_16f18d(void* lp, u32 ucb)
{
    // Xbox 0x0016F18D
    // _ValidateWrite. cdecl; the caller pops the address and the byte count.
    // ESI is set to 1, then XAPILIB::IsBadWritePtr (0x0014A8A5) is called.
    // A nonzero result clears ESI, and EAX returns that flag: 1 when the
    // range can be written, 0 when it cannot. A zero count is writable.
    // BuildCatchObject (0x0016BC55) probes one byte of the catch slot and
    // treats 0 as __inconsistency.
#if defined(_WIN32)
    return IsBadWritePtr(lp, ucb) == 0 ? 1 : 0;
#else
    if (ucb == 0)
    {
        return 1;
    }
    return lp != nullptr ? 1 : 0;
#endif
}

int fn_16f1a8(const void* lpfn)
{
    // Xbox 0x0016F1A8
    // _ValidateExecute. cdecl; the caller pops the address.
    // ESI is set to 1, then XAPILIB::IsBadCodePtr (0x0014A90E) is called.
    // That thunk probes one byte with IsBadReadPtr (0x0014A840) and returns
    // stdcall. A nonzero result clears ESI, and EAX returns that flag: 1
    // when the byte can be read, 0 when it cannot. A null pointer fails.
    // BuildCatchObject (0x0016BC55) probes the copy function at
    // CatchableType+0x18 and treats 0 as __inconsistency. The filter at
    // 0x0016C22C probes the routine at DAT_004DD2C4 and returns 0 when the
    // probe fails.
    return fn_16f172(lpfn, 1);
}

// Condition opcodes are the index into the 27-entry table at 0x00314258.
// The paired size is what 0x00124C80 copies into the stride-0x20 record.
struct ScriptCond
{
    u8 raw[0x20]{};
};

constexpr u32 kScriptCondSize[27] = {
    0x10, 0x10, 0x08, 0x10, 0x18, 0x14, 0x10, 0x0C, 0x0C, 0x0C, 0x14, 0x14, 0x10, 0x10,
    0x10, 0x14, 0x10, 0x1C, 0x10, 0x20, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x0C,
};

constexpr const char* kScriptCondName[27] = {
    "scriptConditionId_TimeOut",
    "scriptConditionId_NumberGhouliesAlive",
    "scriptConditionId_ScareSourceProximity",
    "scriptConditionId_ObjectPickedUp",
    "scriptConditionId_NumberGhouliesKilled",
    "scriptConditionId_NumberOfKnockdowns",
    "scriptConditionId_PlayerApproachesDoor",
    "scriptConditionId_HauntedObjectActivated",
    "scriptConditionId_PlayerEntersRegion",
    "scriptConditionId_PlayerLeavesRegion",
    "scriptConditionId_SpecificGhouliesKilled",
    "scriptConditionId_SpecificGhouliesAlive",
    "scriptConditionId_ObjectByMarkerRemoved",
    "scriptConditionId_PlayerDeflectionContact",
    "scriptConditionId_PlayerHitGhouly",
    "scriptConditionId_GhoulyTriggered",
    "scriptConditionId_GhoulyExistsInScene",
    "scriptConditionId_ObjectByMarkerBroken",
    "scriptConditionId_ActorHasNoEnergy",
    "scriptConditionId_EveryNthKill",
    "scriptConditionId_NoGhoulySpawnsActive",
    "scriptConditionId_IsChallenge21Active",
    "scriptConditionId_IsChallenge21NotActive",
    "scriptConditionId_GhoulyEntersRegion",
    "scriptConditionId_GhoulyHasEnergyLEQ",
    "scriptConditionId_GhoulySignal",
    "scriptConditionId_ObjectByMarkerNotBroken",
};

std::vector<ScriptCond> g_script_conds;
std::vector<u32> g_script_fired;

// DAT_0050E4DC. 0x00116810 allocates the 0x214-byte node. +4 is next. +8 is
// the type 0x00128A00 compares with 0x7D before it calls the handlers.
// DAT_0050E4D8 is the anchor immediately before that head. 0x00116810 stores
// the anchor as the first node's prev, so 0x000E9210's store at prev+4 writes
// the new head.
struct LiveScript
{
    LiveScript* prev = nullptr;
    LiveScript* next = nullptr;
    u32 type = 0;
    // Xbox +0xC. 0x00132720 compares this word with the word at record+8.
    u16 word_c = 0;
};
static_assert(offsetof(LiveScript, word_c) == offsetof(LiveScript, type) + 4);
LiveScript g_live_anchor{};
LiveScript*& g_live_scripts = g_live_anchor.next;

// DAT_0050E568. Handler 2 writes this block. 0x0050E570 and 0x0050E660 are
// the bases of the scaled stores. 0x001077B0 returns at most 0xE9, which is
// the last index those stores reach.
constexpr u32 kSceneWordBase = 0x0050E568u;
constexpr u32 kSceneWordCount = ((0x0050E660u + 0xEAu * 4u) - kSceneWordBase) / 4u;
static_assert((0x0050E758u - kSceneWordBase) / 4u < kSceneWordCount);
static_assert((0x0050E660u + 0xE9u * 4u - kSceneWordBase) / 4u < kSceneWordCount);
u32 g_scene_words[kSceneWordCount]{};

u32& scene_word(u32 addr)
{
    return g_scene_words[(addr - kSceneWordBase) / 4u];
}

u32 cond_u32(const ScriptCond& rec, u32 at)
{
    return u32(rec.raw[at]) | (u32(rec.raw[at + 1]) << 8) | (u32(rec.raw[at + 2]) << 16) | (u32(rec.raw[at + 3]) << 24);
}

void cond_put_u32(ScriptCond& rec, u32 at, u32 v)
{
    rec.raw[at] = u8(v);
    rec.raw[at + 1] = u8(v >> 8);
    rec.raw[at + 2] = u8(v >> 16);
    rec.raw[at + 3] = u8(v >> 24);
}

float cond_f32(const ScriptCond& rec, u32 at)
{
    const u32 bits = cond_u32(rec, at);
    float v = 0.f;
    std::memcpy(&v, &bits, 4);
    return v;
}

void cond_put_f32(ScriptCond& rec, u32 at, float v)
{
    u32 bits = 0;
    std::memcpy(&bits, &v, 4);
    cond_put_u32(rec, at, bits);
}

u32 node_u32(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel + 4 > node.size || node.offset + rel + 4 > script.bytes.size())
    {
        return 0;
    }
    return read_u32(script.bytes, node.offset + rel);
}

const ScriptNode* script_node_at(const SceneScript& script, u32 offset)
{
    for (const ScriptNode& node : script.nodes)
    {
        if (node.offset == offset)
        {
            return &node;
        }
    }
    return nullptr;
}

const char* script_condition_name(u32 opcode)
{
    if (opcode >= 27)
    {
        return "scriptConditionId_Unknown";
    }
    return kScriptCondName[opcode];
}

void log_script_nodes(const SceneScript& script)
{
    const std::ios_base::fmtflags saved = std::cout.flags();
    for (const ScriptNode& node : script.nodes)
    {
        std::cout << "  node +0x" << std::hex << node.offset << " size 0x" << node.size << " type 0x" << node.type;
        if (!node.text.empty())
        {
            std::cout << " " << node.text;
        }
        std::cout << "\n";
    }
    std::cout.flags(saved);
}

void script_vm_reset()
{
    g_script_conds.clear();
    g_script_fired.clear();
}

// Xbox 0x00124C80. Grows DAT_0051234C by the 0x20 stride and copies `size` bytes.
void fn_124c80(const ScriptCond& src, u32 size, u32 opcode)
{
    ScriptCond rec{};
    if (size > sizeof(rec.raw))
    {
        size = u32(sizeof(rec.raw));
    }
    if (size != 0)
    {
        std::memcpy(rec.raw, src.raw, size);
    }
    const std::ios_base::fmtflags saved = std::cout.flags();
    std::cout << "script condition opcode " << opcode << " " << script_condition_name(opcode) << " size 0x" << std::hex
              << size << " link +0x" << cond_u32(rec, 4) << "\n";
    std::cout.flags(saved);
    g_script_conds.push_back(rec);
}

// Xbox 0x00132650. ucomiss of the float at +8 against 0. Equal, and not a
// NaN, returns 1 so the eval pass fires the link and drops the record.
int fn_132650(const ScriptCond& rec)
{
    const float left = cond_f32(rec, 8);
    if (left == 0.f)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x00132680. Opcode 1 in the table at 0x00314330. Walks DAT_00511B48.
// Each node whose word at [node+0x18]+0x94 is nonzero adds one. A count at
// or below the int at record+8, and a ucomiss of the float at record+0xc
// against 0 that is equal and not a NaN, returns 1.
int fn_132680(const ScriptCond& rec)
{
    int count = 0;
    for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
    {
        if (link->at_18->word_94 != 0)
        {
            count = count + 1;
        }
    }
    if (count <= static_cast<i32>(cond_u32(rec, 8)) && cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x000E9210. EAX is the DAT_0050E4DC node. A non-null prev stores next
// at prev+4. A non-null next stores prev at next+0. Both links are then
// cleared. The tail matches 0x000663E0: 0x000F7100 selects the heap and
// 0x0014D360 frees the node.
void fn_e9210(LiveScript* node)
{
    LiveScript* prev = node->prev;
    if (prev != nullptr)
    {
        prev->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = prev;
    }
    node->prev = nullptr;
    node->next = nullptr;
    fn_663e0(node);
}

// Xbox 0x001326D0. Opcode 2 in the table at 0x00314330. The record is not
// read. Walks DAT_0050E4DC. An empty list, or no node whose dword at +8 is
// 0xB8, returns 0. Xbox then adds 8 and returns 0 when that address is 0.
// Otherwise the walk restarts at the head. The next pointer is taken before
// 0x000E9210, and every node with 0xB8 at +8 is passed there. The walk
// returns 1.
int fn_1326d0(const ScriptCond&)
{
    LiveScript* found = g_live_scripts;
    for (;;)
    {
        if (found == nullptr)
        {
            return 0;
        }
        if (found->type == 0xb8u)
        {
            break;
        }
        found = found->next;
    }
    if (reinterpret_cast<std::uintptr_t>(found) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    LiveScript* cursor = g_live_scripts;
    do
    {
        LiveScript* next = cursor->next;
        if (cursor->type == 0xb8u)
        {
            fn_e9210(cursor);
        }
        cursor = next;
    } while (cursor != nullptr);
    return 1;
}

// Xbox 0x00132720. Opcode 3 in the table at 0x00314330. Walks DAT_0050E4DC
// until a node whose dword at +8 is 0xAD. An empty list returns 0. The
// address of that dword is tested against 0. The word four bytes after it
// is compared with the word at record+8. A miss takes next, and returns 0
// when that pointer is 0. Otherwise it walks next until a node has the same
// dword at +8, returning 0 if the walk ends. The address of that dword is
// tested again. A hit ucomiss the float at record+0xC with the 0 at
// 0x0032B100. Equal, and not a NaN, passes the node (that address minus 8)
// to 0x000E9210 and returns 1. Every other path returns 0.
int fn_132720(const ScriptCond& rec)
{
    LiveScript* node = g_live_scripts;
    for (;;)
    {
        if (node == nullptr)
        {
            return 0;
        }
        if (node->type == 0xadu)
        {
            break;
        }
        node = node->next;
    }
    if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    do
    {
        if (node->word_c == want)
        {
            if (cond_f32(rec, 0xc) != 0.f)
            {
                return 0;
            }
            fn_e9210(node);
            return 1;
        }
        LiveScript* cursor = node->next;
        const u32 saved = node->type;
        if (cursor == nullptr)
        {
            return 0;
        }
        while (cursor->type != saved)
        {
            cursor = cursor->next;
            if (cursor == nullptr)
            {
                return 0;
            }
        }
        node = cursor;
    } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
    return 0;
}

// DAT_004E1C34. 0x001327A0 compares this dword with 0x80000000.
u32 g_4e1c34 = 0;

// Dwords 0x0012AE40 reads. The first is 0x00512C6C. Six fields sit 0xF4
// apart and the row stride is 0x5B8. Category n is 0x3A24 bytes later.
// 0x0012AF10 walks seven categories, so the last read is category 6.
constexpr u32 kStat512c6c = 0x00512C6Cu;
constexpr u32 kStat512c6cLast = 0x0051303Cu + 6u * 0x3A24u + 9u * 0x5B8u + 0xF4u;
i32 g_stat_512c6c[(kStat512c6cLast + 4u - kStat512c6c) / 4u]{};

i32& stat_512c6c(u32 addr)
{
    return g_stat_512c6c[(addr - kStat512c6c) / 4u];
}

// Xbox 0x0012AE40. ECX is the category. The caller clears it. ECX is
// multiplied by 0x3A24 and 0x0051303C is added. Ten rows each add the
// dwords at -0x3D0, -0x2DC, -0x1E8, -0xF4, +0xF4, and +0, then the cursor
// advances by 0x5B8.
int fn_12ae40(i32 category)
{
    i32 sum = 0;
    u32 cursor = 0x0051303Cu + static_cast<u32>(category) * 0x3A24u;
    i32 rows = 10;
    do
    {
        const i32 edi = stat_512c6c(cursor - 0x2dcu);
        i32 esi = stat_512c6c(cursor - 0x3d0u);
        esi += edi;
        esi += stat_512c6c(cursor - 0x1e8u);
        esi += stat_512c6c(cursor - 0xf4u);
        esi += stat_512c6c(cursor + 0xf4u);
        esi += stat_512c6c(cursor);
        sum += esi;
        cursor += 0x5b8u;
        rows -= 1;
    } while (rows != 0);
    return sum;
}

// Xbox 0x001327A0. Opcode 4 in the table at 0x00314330. 0x0012AE40 is
// compared with the int at record+8. A smaller count returns 0. A zero
// dword at +0x10 stores 1 there and stores whether DAT_004E1C34 is below
// 0x80000000 at +0x14. A zero at +0x14 then ucomiss the float at +0xC
// with the 0 at 0x0032B100 and returns 0 when they differ or either is
// NaN. A nonzero +0x14 returns 0 while DAT_004E1C34 stays below
// 0x80000000. Every other path returns 1.
int fn_1327a0(ScriptCond& rec)
{
    const i32 count = fn_12ae40(0);
    if (count < static_cast<i32>(cond_u32(rec, 8)))
    {
        return 0;
    }
    if (static_cast<i32>(cond_u32(rec, 0x10)) == 0)
    {
        const u32 below = g_4e1c34 < 0x80000000u ? 1u : 0u;
        cond_put_u32(rec, 0x10, 1u);
        cond_put_u32(rec, 0x14, below);
    }
    if (static_cast<i32>(cond_u32(rec, 0x14)) == 0)
    {
        if (cond_f32(rec, 0xc) != 0.f)
        {
            return 0;
        }
    }
    else if (g_4e1c34 < 0x80000000u)
    {
        return 0;
    }
    return 1;
}

// Xbox 0x00132800. Opcode 5 in the table at 0x00314330. The word at
// record+0xC is shifted left by 2. Those dwords of DAT_005172F4 and
// DAT_0051696C are added and compared with the int at record+8. A smaller
// sum returns 0. Otherwise the float at record+0x10 is ucomiss'd with the
// 0 at 0x0032B100. Equal, and not a NaN, returns 1. Every other path
// returns 0.
int fn_132800(const ScriptCond& rec)
{
    const u16 index = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    const u32 scaled = u32(index) * 4u;
    const i32 sum = stat_512c6c(0x005172F4u + scaled) + stat_512c6c(0x0051696Cu + scaled);
    if (static_cast<i32>(cond_u32(rec, 8)) <= sum && cond_f32(rec, 0x10) == 0.f)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x00124BE0. A zero keeps the record. Any other value queues the link
// at +4. A return of exactly 1 also removes the record and the slot that
// slides down is evaluated next. Opcode 0 is 0x00132650. Opcode 1 is
// 0x00132680. Opcode 2 is 0x001326D0. Opcode 3 is 0x00132720. Opcode 4 is
// 0x001327A0. Opcode 5 is 0x00132800. Other opcodes are not evaluated until
// their functions are translated.
void fn_124be0(Game& g)
{
    size_t i = 0;
    while (i < g_script_conds.size())
    {
        ScriptCond& rec = g_script_conds[i];
        const u32 opcode = cond_u32(rec, 0);
        int fired = 0;
        if (opcode == 0)
        {
            fired = fn_132650(rec);
        }
        else if (opcode == 1)
        {
            fired = fn_132680(rec);
        }
        else if (opcode == 2)
        {
            fired = fn_1326d0(rec);
        }
        else if (opcode == 3)
        {
            fired = fn_132720(rec);
        }
        else if (opcode == 4)
        {
            fired = fn_1327a0(rec);
        }
        else if (opcode == 5)
        {
            fired = fn_132800(rec);
        }
        if (fired == 0)
        {
            ++i;
            continue;
        }
        g_script_fired.push_back(cond_u32(rec, 4));
        if (fired == 1)
        {
            g_script_conds.erase(g_script_conds.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
    (void)g;
}

u8 node_u8(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel >= node.size || node.offset + rel >= script.bytes.size())
    {
        return 0;
    }
    return script.bytes[node.offset + rel];
}

u16 node_u16(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel + 2 > node.size || node.offset + rel + 2 > script.bytes.size())
    {
        return 0;
    }
    return read_u16(script.bytes, node.offset + rel);
}

std::string node_cstr(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel >= node.size || node.offset + rel >= script.bytes.size())
    {
        return {};
    }
    const size_t begin = node.offset + rel;
    const size_t end = std::min(script.bytes.size(), static_cast<size_t>(node.offset + node.size));
    size_t at = begin;
    while (at < end && script.bytes[at] != 0)
    {
        ++at;
    }
    return std::string(reinterpret_cast<const char*>(script.bytes.data() + begin), at - begin);
}

// Names at 0x0042A854. Count passed to 0x000FBFB0 is 0xE9.
const char* const kObjTagActor[0xE9] = {
    "objTag_Actor",
    "objTag_Actor_Baron",
    "objTag_Actor_BaronPlane",
    "objTag_Actor_Boy",
    "objTag_Actor_Butler",
    "objTag_Actor_Cameron",
    "objTag_Actor_Chicken",
    "objTag_Actor_Cook",
    "objTag_Actor_CookPlayable",
    "objTag_Actor_Doll",
    "objTag_Actor_Fiddler",
    "objTag_Actor_GhoulieGirl",
    "objTag_Actor_GiantHand",
    "objTag_Actor_Girl",
    "objTag_Actor_GrimReaper",
    "objTag_Actor_HauntedChair",
    "objTag_Actor_HauntedCoat",
    "objTag_Actor_HauntedDoor",
    "objTag_Actor_HauntedDoorRound",
    "objTag_Actor_HauntedPicture",
    "objTag_Actor_HauntedTV",
    "objTag_Actor_Hunchback",
    "objTag_Actor_Imp",
    "objTag_Actor_ImpChallenge",
    "objTag_Actor_ImpFire",
    "objTag_Actor_ImpFlying",
    "objTag_Actor_ImpNinja",
    "objTag_Actor_ImpNinjaBig",
    "objTag_Actor_ImpRoger",
    "objTag_Actor_JessieClyde",
    "objTag_Actor_Medusa",
    "objTag_Actor_MiniCooper",
    "objTag_Actor_MrsMop",
    "objTag_Actor_Mummy",
    "objTag_Actor_MummyCursed",
    "objTag_Actor_Pirate",
    "objTag_Actor_PirateLeader",
    "objTag_Actor_PrisonerChestChap",
    "objTag_Actor_PrisonerDoggyGirl",
    "objTag_Actor_PrisonerFilmReel",
    "objTag_Actor_PrisonerFridgeLady",
    "objTag_Actor_PrisonerFridgeLadyFat",
    "objTag_Actor_PrisonerLaundryLad",
    "objTag_Actor_PrisonerPlatterLad",
    "objTag_Actor_PrisonerSarco",
    "objTag_Actor_PrisonerSaunaGirl",
    "objTag_Actor_PrisonerSerpentYouth",
    "objTag_Actor_PrisonerThornGirl",
    "objTag_Actor_Ribs",
    "objTag_Actor_RibsFNPC",
    "objTag_Actor_RibsKey",
    "objTag_Actor_Skeleton",
    "objTag_Actor_SkeletonMaid",
    "objTag_Actor_Spider",
    "objTag_Actor_UglyGirl",
    "objTag_Actor_Vampire",
    "objTag_Actor_VampireChicken",
    "objTag_Actor_Warlock",
    "objTag_Actor_Worm",
    "objTag_Actor_Zombie",
    "objTag_Actor_zzz",
    "objTag_Avatar",
    "objTag_Avatar_DizzyBat",
    "objTag_Avatar_Fx_AnimEmitter",
    "objTag_Avatar_Fx_BurnUp",
    "objTag_Avatar_Fx_Explode",
    "objTag_Avatar_Fx_MotionBlur",
    "objTag_Avatar_HatstandBase",
    "objTag_Avatar_StandardLampBase",
    "objTag_Background",
    "objTag_BgBreakKey",
    "objTag_Body",
    "objTag_Body_Camera",
    "objTag_Body_Human",
    "objTag_Body_JessieClyde",
    "objTag_Body_Player",
    "objTag_Body_Vampire",
    "objTag_Body_Zombie",
    "objTag_Camera",
    "objTag_FxCustom_DustUp",
    "objTag_FxCustom_Electric",
    "objTag_FxCustom_FadeOut",
    "objTag_FxCustom_FadeToGrey",
    "objTag_FxCustom_Fire",
    "objTag_FxCustom_MutateHalo",
    "objTag_FxCustom_SolidFadeIn",
    "objTag_Mind",
    "objTag_Mind_Cameraman",
    "objTag_Mind_Cinema",
    "objTag_Mind_Human",
    "objTag_Mind_Player",
    "objTag_Misc_AnimDelete",
    "objTag_Misc_AnimatedLight",
    "objTag_Misc_ButtonBat",
    "objTag_Misc_ChickenEgg",
    "objTag_Misc_Furnace",
    "objTag_Misc_GhoulyKey",
    "objTag_Misc_KnockDownSphere",
    "objTag_Misc_MummyCursedSlabTop",
    "objTag_Misc_Prop",
    "objTag_Misc_SSSDoor",
    "objTag_Misc_SSSDropper",
    "objTag_Misc_Scare",
    "objTag_Misc_ScareFigure",
    "objTag_Misc_SpiderWeb",
    "objTag_Misc_SuperScaryShock",
    "objTag_Move",
    "objTag_Powerup",
    "objTag_SceneControl",
    "objTag_Spawn",
    "objTag_State",
    "objTag_Strategy",
    "objTag_Strategy_Director",
    "objTag_Strategy_HauntedCoat",
    "objTag_Strategy_HauntedDoor",
    "objTag_Strategy_Human",
    "objTag_Strategy_Player",
    "objTag_Weapon",
    "objTag_Weapon_Anchor",
    "objTag_Weapon_Ankh",
    "objTag_Weapon_Anvil",
    "objTag_Weapon_Bandage",
    "objTag_Weapon_BaronCane",
    "objTag_Weapon_Barrel",
    "objTag_Weapon_Bathtub",
    "objTag_Weapon_Bellows",
    "objTag_Weapon_Blunderbuss",
    "objTag_Weapon_Book",
    "objTag_Weapon_Bottle",
    "objTag_Weapon_BottleContainer",
    "objTag_Weapon_Broom",
    "objTag_Weapon_Burger",
    "objTag_Weapon_BurgerContainer",
    "objTag_Weapon_Cake",
    "objTag_Weapon_Candle",
    "objTag_Weapon_Candlestick",
    "objTag_Weapon_Chair",
    "objTag_Weapon_ChessBoard",
    "objTag_Weapon_ChestOfDrawers",
    "objTag_Weapon_ChristmasPudding",
    "objTag_Weapon_CoalScuttle",
    "objTag_Weapon_CookingPot",
    "objTag_Weapon_Crate",
    "objTag_Weapon_Crate2",
    "objTag_Weapon_Crutches",
    "objTag_Weapon_Cue",
    "objTag_Weapon_Cymbal",
    "objTag_Weapon_Drum",
    "objTag_Weapon_Dustbin",
    "objTag_Weapon_Dustpan",
    "objTag_Weapon_Egg",
    "objTag_Weapon_EggContainer",
    "objTag_Weapon_FeatherDuster",
    "objTag_Weapon_FeedingTrough",
    "objTag_Weapon_Figurine",
    "objTag_Weapon_FilmReel",
    "objTag_Weapon_FireBall",
    "objTag_Weapon_FireExtinguisher",
    "objTag_Weapon_FishingRod",
    "objTag_Weapon_FlourBag",
    "objTag_Weapon_FrenchStick",
    "objTag_Weapon_FruitBowl",
    "objTag_Weapon_FryingPan",
    "objTag_Weapon_Garlic",
    "objTag_Weapon_GiantScissors",
    "objTag_Weapon_GlassPane",
    "objTag_Weapon_Gnome",
    "objTag_Weapon_Gramophone",
    "objTag_Weapon_Guitar",
    "objTag_Weapon_HandSaw",
    "objTag_Weapon_HatStand",
    "objTag_Weapon_IroningBoard",
    "objTag_Weapon_JointOfMeat",
    "objTag_Weapon_LaboratoryBed",
    "objTag_Weapon_Ladder",
    "objTag_Weapon_LargePlate",
    "objTag_Weapon_LaundryBasket",
    "objTag_Weapon_Mailbox",
    "objTag_Weapon_Mirror",
    "objTag_Weapon_MummyCurse",
    "objTag_Weapon_Oar",
    "objTag_Weapon_OilDrum",
    "objTag_Weapon_PaintBall",
    "objTag_Weapon_PaintCan",
    "objTag_Weapon_PaintCanContainer",
    "objTag_Weapon_Painting",
    "objTag_Weapon_PicnicTable",
    "objTag_Weapon_PirateChest",
    "objTag_Weapon_PirateRum",
    "objTag_Weapon_PirateSword",
    "objTag_Weapon_Pitchfork",
    "objTag_Weapon_Plank",
    "objTag_Weapon_PlantPot",
    "objTag_Weapon_PopCan",
    "objTag_Weapon_PopCanGun",
    "objTag_Weapon_Pot",
    "objTag_Weapon_PotContainer",
    "objTag_Weapon_Pumpkin",
    "objTag_Weapon_Rake",
    "objTag_Weapon_RollingPin",
    "objTag_Weapon_SilverServiceTray",
    "objTag_Weapon_Skip",
    "objTag_Weapon_SmallTree",
    "objTag_Weapon_SnookerTable",
    "objTag_Weapon_Sofa",
    "objTag_Weapon_SofaButler",
    "objTag_Weapon_Spade",
    "objTag_Weapon_Speaker",
    "objTag_Weapon_SpecialPot",
    "objTag_Weapon_SpiderSpitBall",
    "objTag_Weapon_StandardLamp",
    "objTag_Weapon_Stool",
    "objTag_Weapon_SwordFish",
    "objTag_Weapon_Syringe",
    "objTag_Weapon_SyringeContainer",
    "objTag_Weapon_Table",
    "objTag_Weapon_TableLamp",
    "objTag_Weapon_Television",
    "objTag_Weapon_ToiletBrush",
    "objTag_Weapon_ToiletRoll",
    "objTag_Weapon_ToiletRollContainer",
    "objTag_Weapon_ToyBox",
    "objTag_Weapon_TreeStump",
    "objTag_Weapon_Trophy",
    "objTag_Weapon_Trunk",
    "objTag_Weapon_Umbrella",
    "objTag_Weapon_Vase",
    "objTag_Weapon_WarlockBolt",
    "objTag_Weapon_WashingPowder",
    "objTag_Weapon_WaterPistol",
    "objTag_Weapon_Welly",
    "objTag_Weapon_Wheelbarrow",
    "objTag_Weapon_ZombieArm",
};

// Xbox 0x000FBFB0. Binary search. High starts at count-1. A miss returns -1.
int fn_fbfb0(i32 count, const char* const* table, const char* key)
{
    i32 high = count - 1;
    i32 low = 0;
    if (high < 0)
    {
        return -1;
    }
    const u8* want = reinterpret_cast<const u8*>(key);
    while (low <= high)
    {
        const i32 mid = (low + high) >> 1;
        const u8* row = reinterpret_cast<const u8*>(table[mid]);
        u8 tc = row[0];
        u8 kc = want[0];
        if (tc == kc)
        {
            i32 i = 0;
            for (;;)
            {
                const u8 prev = row[i];
                i += 1;
                if (prev == 0)
                {
                    return mid;
                }
                tc = row[i];
                kc = want[i];
                if (tc != kc)
                {
                    break;
                }
            }
        }
        const i32 diff = i32(tc) - i32(kc);
        if (diff < 0)
        {
            low = mid + 1;
        }
        else if (diff > 0)
        {
            high = mid - 1;
        }
        else
        {
            return mid;
        }
    }
    return -1;
}

// Xbox 0x000F96F0. AL is the value. The stack byte selects the pair at +0x294.
void fn_f96f0(CamPairObject* obj, u8 index, u8 value)
{
    const int pair = int(index) * 2;
    const int byte_index = pair >> 3;
    const int shift = pair & 7;
    if (g_pair_cache[index] != value)
    {
        g_pair_cache[index] = value;
    }
    const u8 mask = static_cast<u8>(3u << shift);
    const u8 packed = static_cast<u8>(value << shift);
    u8 cur = obj->pairs[byte_index];
    cur = static_cast<u8>(cur & static_cast<u8>(~mask));
    cur = static_cast<u8>(cur | packed);
    obj->pairs[byte_index] = cur;
}

// Xbox 0x00117400. Bit 0 of the mask drives DAT_0050E764. Bit 1 drives
// DAT_0050E768. A rising edge (counter becomes 1) and a falling edge
// (counter becomes 0) walk DAT_00511B38. Note 0x26 when the flag is set,
// 0x27 when it is clear. Vtable +0xC runs when the word at link+0x3a8
// equals 2, or the dword at object+0x93c is nonzero, only for the bit-1
// edge. The other objects get the bit-0 edge.
void fn_117400(u32 mask, i32 flag)
{
    const u8 bits = static_cast<u8>(mask);
    i32 edge0 = 0;
    i32 edge1 = 0;
    if (flag != 0)
    {
        if ((bits & 1u) != 0)
        {
            g_count_e764 -= 1;
            if (g_count_e764 == 0)
            {
                edge0 = 1;
            }
        }
        if ((bits & 2u) != 0)
        {
            g_block_decay -= 1;
            if (g_block_decay == 0)
            {
                edge1 = 1;
            }
        }
    }
    else
    {
        if ((bits & 1u) != 0)
        {
            g_count_e764 += 1;
            if (g_count_e764 == 1)
            {
                edge0 = 1;
            }
        }
        if ((bits & 2u) != 0)
        {
            g_block_decay += 1;
            if (g_block_decay == 1)
            {
                edge1 = 1;
            }
        }
    }
    const i32 edge[2] = {edge0, edge1};
    for (i32 edi = 0; edi < 2; ++edi)
    {
        if (edge[edi] == 0)
        {
            continue;
        }
        const i32 note = flag != 0 ? 0x26 : 0x27;
        for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
        {
            CamFocus* obj = link->host;
            if (obj == nullptr)
            {
                continue;
            }
            i32 match = 0;
            if (obj->word_cdc == 2 || std::bit_cast<u32>(obj->pulse) != 0)
            {
                match = 1;
            }
            if (match != edi || obj->on_note == nullptr)
            {
                continue;
            }
            obj->on_note(obj, note);
        }
    }
}

// Xbox 0x00124CC0. Handler 1. Type 0x12 calls 0x00117400. Type 0x90 retires
// the actor whose short at +6 matches the tag. Type 0x9F packs the byte at
// +0x4C into the pair selected by the byte at +0x48. Any other type returns 3.
int fn_124cc0(Game& g, const ScriptNode& node)
{
    const u32 type = node.type;
    if (type == 0x12)
    {
        const u32 word = node_u32(g.scene_script, node, 8);
        fn_117400(0xffff, word != 0 ? 1 : 0);
    }
    else if (type == 0x90)
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const i32 found = fn_fbfb0(0xe9, kObjTagActor, key.c_str());
        const u16 id = found == -1 ? u16{0} : static_cast<u16>(found + 1);
        for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
        {
            CamFocus* obj = link->host;
            if (obj == nullptr || obj->tag_6 != id)
            {
                continue;
            }
            // 0x00112D10 on the child at +0x920, then vtable +0xC with 0x80
            // and 0x0003ED70 while +0x908 is still null.
            release_host(*obj);
        }
    }
    else
    {
        if (type != 0x9f)
        {
            return 3;
        }
        const std::string key = node_cstr(g.scene_script, node, 8);
        const i32 found = fn_fbfb0(0xe9, kObjTagActor, key.c_str());
        const u16 id = found == -1 ? u16{0} : static_cast<u16>(found + 1);
        const u8 index = node_u8(g.scene_script, node, 0x48);
        const u8 value = node_u8(g.scene_script, node, 0x4c);
        for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
        {
            CamFocus* obj = link->host;
            if (obj == nullptr || obj->tag_6 != id)
            {
                continue;
            }
            if (obj->pair_204 != nullptr)
            {
                fn_f96f0(obj->pair_204, index, value);
            }
            if (obj->pair_310 != nullptr)
            {
                fn_f96f0(obj->pair_310, index, value);
            }
        }
    }
    return 2;
}

// Xbox 0x001077B0. The cstring arrives in EAX. 0x000FBFB0 searches the actor
// tags. A miss returns 0. A hit returns the 1-based index, zero-extended
// from 16 bits.
u32 fn_1077b0(const char* key)
{
    const i32 found = fn_fbfb0(0xe9, kObjTagActor, key);
    if (found == -1)
    {
        return 0;
    }
    return u32(u16(found + 1));
}

// Xbox 0x00124E50. Handler 2. Types 0x32 through 0x38 store the dword at
// node+8 and return 2. Types 0x34 and 0x36 scale by 0x001077B0. That call
// passes the cstring at node+0xC in EAX. Type 0x37 stores the float at
// node+8 times 0.01 (0x0032B124). Any other type returns 3.
int fn_124e50(Game& g, const ScriptNode& node)
{
    const u32 word = node_u32(g.scene_script, node, 8);
    switch (node.type)
    {
    case 0x32:
        scene_word(0x0050E568u) = word;
        return 2;
    case 0x33:
        scene_word(0x0050E56Cu) = word;
        return 2;
    case 0x34:
    {
        const std::string key = node_cstr(g.scene_script, node, 0xC);
        const u32 slot = fn_1077b0(key.c_str()) & 0xffffu;
        scene_word(0x0050E570u + slot * 4u) = word;
        return 2;
    }
    case 0x35:
        scene_word(0x0050E570u) = word;
        return 2;
    case 0x36:
    {
        const std::string key = node_cstr(g.scene_script, node, 0xC);
        const u32 slot = fn_1077b0(key.c_str()) & 0xffffu;
        scene_word(0x0050E660u + slot * 4u) = word;
        return 2;
    }
    case 0x37:
    {
        const float scaled = std::bit_cast<float>(word) * 0.01f;
        scene_word(0x0050E754u) = std::bit_cast<u32>(scaled);
        return 2;
    }
    case 0x38:
        scene_word(0x0050E758u) = word;
        return 2;
    default:
        return 3;
    }
}

// Xbox 0x00122CA0. Walks DAT_00511AA4. The link is object+0x900 and the next
// pointer is at +4, so the port follows PulseNode::next. EDI is compared with
// the word at object+6. Each match calls vtable +0xC with EBX, the message.
// An empty slot is skipped.
void fn_122ca0(u16 tag, const i32* message)
{
    PulseNode* node = g_pulse_nodes;
    while (node != nullptr)
    {
        PulseNode* next = node->next;
        if (node->tag_6 == tag && node->on_message != nullptr)
        {
            node->on_message(node, message);
        }
        node = next;
    }
}

// Xbox 0x00129ED0. DX is compared with the word at object+0x1FC. A zero DX
// returns null without walking. The list is DAT_00511AA4. The first match
// returns that object. A miss returns null.
PulseNode* fn_129ed0(u16 key)
{
    if (key == 0)
    {
        return nullptr;
    }
    for (PulseNode* node = g_pulse_nodes; node != nullptr; node = node->next)
    {
        if (node->word_1fc == key)
        {
            return node;
        }
    }
    return nullptr;
}

// Xbox 0x00124F30. Handler 3. Type 0x5D searches the cstring at node+8 in the
// actor tags. A miss calls 0x00122CA0 with tag 0. A hit calls it with the
// 1-based index, zero-extended from 16 bits. Both pass a message id of 1.
// Type 0x71 loads the word at node+8 into DX for 0x00129ED0. A hit calls
// vtable +0xC with a message id of 0x13. Any other type returns 3.
int fn_124f30(Game& g, const ScriptNode& node)
{
    if (node.type == 0x5d)
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        i32 message = 1;
        const i32 found = fn_fbfb0(0xe9, kObjTagActor, key.c_str());
        if (found == -1)
        {
            fn_122ca0(0, &message);
            return 2;
        }
        fn_122ca0(static_cast<u16>(found + 1), &message);
    }
    else
    {
        if (node.type != 0x71)
        {
            return 3;
        }
        PulseNode* obj = fn_129ed0(node_u16(g.scene_script, node, 8));
        if (obj != nullptr)
        {
            i32 message = 0x13;
            if (obj->on_message != nullptr)
            {
                obj->on_message(obj, &message);
            }
            return 2;
        }
    }
    return 2;
}

// DAT_00512350. Records are 8 bytes. DAT_00512370 is the count, 0x20 bytes
// above the base, so four records sit in front of it. 0x00125070 drains the
// count. The port keeps the record that count indexes.
struct PairSlot
{
    u32 at_8 = 0;
    u32 at_c = 0;
};
std::vector<PairSlot> g_pair_slots;
i32 g_pair_slot_count = 0;

// Xbox 0x00124FC0. Handler 4. The dword at node+4 must be 0x98. Any other
// type returns 3. The second argument is the flag 0x00128A00 passes: nonzero
// when a DAT_0050E4DC node has 0x7D at +8. Zero stores the dword at node+8
// at DAT_00512350 + DAT_00512370*8, the dword at node+0xC in the next dword,
// then increments DAT_00512370. Nonzero walks DAT_00511B20. The byte at
// node+8 selects the pair. The byte at node+0xC is the value in AL for
// 0x000F96F0. The object is the dword at link-0x730, then the dword at
// link-0x624. A null object skips that call. Both paths return 2.
int fn_124fc0(Game& g, const ScriptNode& node, int param_2)
{
    if (node.type != 0x98)
    {
        return 3;
    }
    if (param_2 != 0)
    {
        const u8 index = node_u8(g.scene_script, node, 8);
        const u8 value = node_u8(g.scene_script, node, 0xC);
        for (BodyLink* link = g_body_links; link != nullptr; link = link->next)
        {
            if (link->pair_204 != nullptr)
            {
                fn_f96f0(link->pair_204, index, value);
            }
            if (link->pair_310 != nullptr)
            {
                fn_f96f0(link->pair_310, index, value);
            }
        }
        return 2;
    }
    const size_t slot = size_t(g_pair_slot_count);
    g_pair_slots.resize(slot + 1);
    g_pair_slots[slot].at_8 = node_u32(g.scene_script, node, 8);
    g_pair_slots[slot].at_c = node_u32(g.scene_script, node, 0xC);
    g_pair_slot_count = g_pair_slot_count + 1;
    return 2;
}

// Names at 0x004232AC. 0x000FBFB0 is called with count 9.
const char* const kChallengeId[9] = {
    "challengeId_Attacks",
    "challengeId_Buttons",
    "challengeId_Challenge21",
    "challengeId_Damage",
    "challengeId_Health",
    "challengeId_Kills",
    "challengeId_KillsBOB",
    "challengeId_Survive",
    "challengeId_Time",
};

// Xbox 0x001251E0. Handler 5. The dword at node+4 is the type. Type 0x7A
// calls 0x000FBFB0 with the cstring at node+8 and passes that index to
// 0x0004A100. Type 0x7B calls 0x0004A240. The other listed types build the
// record 0x0004ADD0 reads through EAX: opcode at +0, a word or dword at +8,
// and for type 0x80 the dword at node+0x48 at +0xC. Type 0x7C stores opcode
// 0x10 and float 0. Type 0x7D stores opcode 0x11 and the float at node+8.
// Type 0x7F returns 2 when 0x001077B0 of the cstring at node+8 is 0; otherwise
// opcode 9 and a second call, and the word is AX. Types 0x7E (opcode 5) and
// 0x82 (opcode 4) share that word store. Type 0x80 stores opcode 0xA. Type
// 0x81 stores opcode 0x12. Type 0x83 stores opcode 0xB and the dword at
// node+8. Type 0x84 stores opcode 0xC. Type 0x85 stores opcode 0. Type 0x86
// stores opcode 3. 0x0004A100, 0x0004A240, and 0x0004ADD0 are not translated,
// so those calls are not made. Any other type returns 3. A handled type
// returns 2.
int fn_1251e0(Game& g, const ScriptNode& node)
{
    struct Arg
    {
        u32 opcode = 0;
        u32 at_8 = 0;
        u32 at_c = 0;
    };
    Arg arg{};
    switch (node.type)
    {
    case 0x7a:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const i32 found = fn_fbfb0(9, kChallengeId, key.c_str());
        (void)found;
        return 2;
    }
    case 0x7b:
        return 2;
    case 0x7c:
        arg.opcode = 0x10;
        arg.at_8 = 0;
        (void)arg;
        return 2;
    case 0x7d:
        arg.opcode = 0x11;
        arg.at_8 = node_u32(g.scene_script, node, 8);
        (void)arg;
        return 2;
    case 0x7e:
        arg.opcode = 5;
        break;
    case 0x7f:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        if (static_cast<u16>(fn_1077b0(key.c_str())) == 0)
        {
            return 2;
        }
        arg.opcode = 9;
        arg.at_8 = u32(static_cast<u16>(fn_1077b0(key.c_str())));
        (void)arg;
        return 2;
    }
    case 0x80:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        arg.opcode = 0xA;
        arg.at_8 = u32(static_cast<u16>(fn_1077b0(key.c_str())));
        arg.at_c = node_u32(g.scene_script, node, 0x48);
        (void)arg;
        return 2;
    }
    case 0x81:
        arg.opcode = 0x12;
        (void)arg;
        return 2;
    case 0x82:
        arg.opcode = 4;
        break;
    case 0x83:
        arg.opcode = 0xB;
        arg.at_8 = node_u32(g.scene_script, node, 8);
        (void)arg;
        return 2;
    case 0x84:
        arg.opcode = 0xC;
        (void)arg;
        return 2;
    case 0x85:
        arg.opcode = 0;
        (void)arg;
        return 2;
    case 0x86:
        arg.opcode = 3;
        (void)arg;
        return 2;
    default:
        return 3;
    }
    const std::string key = node_cstr(g.scene_script, node, 8);
    arg.at_8 = u32(static_cast<u16>(fn_1077b0(key.c_str())));
    (void)arg;
    return 2;
}

// Xbox 0x00125450. One node of the action walk. Type 0x3C stops the walk.
// The listed types copy a condition and tell the runner to advance. Every
// other type stops the walk. The word filled by 0x001077B0 is left 0.
int fn_125450(Game& g, const ScriptNode& node)
{
    u32 opcode = 0;
    bool as_float = false;
    bool word_then_far = false;
    bool payload_at_8 = false;
    switch (node.type)
    {
    case 0x3C:
        return 1;
    case 0x3D:
        opcode = 0;
        as_float = true;
        break;
    case 0x3E:
        opcode = 1;
        payload_at_8 = true;
        break;
    case 0x40:
        opcode = 3;
        word_then_far = true;
        break;
    case 0x47:
        opcode = 8;
        payload_at_8 = true;
        break;
    case 0x48:
        opcode = 9;
        payload_at_8 = true;
        break;
    case 0x94:
        opcode = 23;
        word_then_far = true;
        break;
    case 0x95:
        opcode = 24;
        word_then_far = true;
        break;
    case 0x99:
        opcode = 25;
        word_then_far = true;
        break;
    default:
        return 3;
    }
    ScriptCond rec{};
    cond_put_u32(rec, 0, opcode);
    cond_put_u32(rec, 4, node.link);
    if (as_float)
    {
        const i32 ticks = i32(node_u32(g.scene_script, node, 8));
        cond_put_f32(rec, 8, float(ticks));
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
    }
    else if (word_then_far)
    {
        cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0x48));
    }
    else if (payload_at_8)
    {
        cond_put_u32(rec, 8, node_u32(g.scene_script, node, 8));
        if (kScriptCondSize[opcode] > 0xC)
        {
            cond_put_u32(rec, 0xC, node_u32(g.scene_script, node, 0xC));
        }
    }
    fn_124c80(rec, kScriptCondSize[opcode], opcode);
    return 2;
}

// DAT_00512378. Type 0x29 sets this after 0x00117400. The clear is the
// 0x0005B780 hit, which is not translated.
i32 g_flag_2378 = 0;

// DAT_0051237C. Records are 4 bytes: the word at node+8, then the word at
// node+0xC. DAT_005123AC is the count, 0x30 bytes above the base, so twelve
// records sit in front of it. The port keeps the record that count indexes.
struct TagPair
{
    u16 at_8 = 0;
    u16 at_c = 0;
};
std::vector<TagPair> g_tag_pairs;
i32 g_tag_pair_count = 0;

// DAT_005123B0. Records are 8 bytes. DAT_00512410 is the count, 0x60 bytes
// above the base. The word is at +0 and the dword is at +4.
struct QueuedTag
{
    u16 at_8 = 0;
    u32 at_c = 0;
};
std::vector<QueuedTag> g_queued_tags;
i32 g_queued_tag_count = 0;

// DAT_00512414. The next dword after DAT_00512410. Type 0x18 stores 1 here,
// then stores 0 when DAT_005064D8 is 3.
i32 g_flag_2414 = 0;

// DAT_00512418. Type 0x2a stores 1 after a live dword at node+0x10c, and
// stores 0 when 0x001412F0 returns nonzero.
i32 g_flag_2418 = 0;

// DAT_00512420. Records are 4 bytes: the byte at node+8, then the word at
// node+0xC. DAT_00512440 is the count, 0x20 bytes above the base, so eight
// records sit in front of it. Type 0x67 with a zero second argument appends
// one record. The port keeps the record that count indexes.
struct GateRec2420
{
    u8 at_8 = 0;
    u16 at_c = 0;
};
std::vector<GateRec2420> g_gate_2420;
i32 g_gate_2440 = 0;

// DAT_00512448. DAT_00512444 is the count, the dword immediately before this
// base. DAT_0051245C begins 0x14 bytes above the base, so five dwords sit in
// front of it. Type 0x9E stores the dword at node+8. 0x000F29A0 is not
// translated, so the parallel dword at DAT_0051245C is not stored.
std::vector<u32> g_mark_2448;
i32 g_mark_2444 = 0;

// DAT_005064D8. Type 0x18 compares this with 3. 0x000D0530 would store 0.
i32 g_flag_64d8 = 0;

// DAT_0050EA9C. Type 0x1A tests bit 0x10 of the low byte.
u32 g_flags_ea9c = 0;

// DAT_00506540. Type 0x54 stores the floats at node+8, +0xC, +0x10, and +0x14,
// then the dword at node+0x18 into DAT_00506550.
float g_script_6540 = 0.f;
float g_script_6544 = 0.f;
float g_script_6548 = 0.f;
float g_script_654c = 0.f;
u32 g_script_6550 = 0;

// Xbox 0x001259B0. Handler 7. The dword at node+4 is the type. Type 0x4F
// with a zero second argument stores the word at node+8 and the dword at
// node+0xC at DAT_005123B0 + DAT_00512410*8, then increments DAT_00512410.
// A nonzero second argument calls 0x001117E0. EAX is the word at node+8 and
// EBX is the dword at node+0xC. Type 0x56 builds a message whose first dword
// is 0x67 and whose second dword is the word at node+8, then calls
// 0x00122F70 with that message in EDI. Type 0x29 with a nonzero second
// argument and a zero third argument reads DAT_00512378. Zero calls
// 0x000532E0 on the node and passes its return, the word at node+8 in EAX,
// the word at node+0xC, and the dword at node+0x14 to 0x0005B540. The dword
// at node+0x10 then selects the return: zero returns 2, nonzero calls
// 0x00117400(0xffff, 0), sets DAT_00512378, and returns 0. Nonzero calls
// 0x0005B780 with the word at node+8 in SI. A miss compares 8 with the dword
// at node+0xC. A hit compares the written word. A match calls
// 0x00117400(0xffff, 1), clears DAT_00512378, and returns 2. The other path
// stores those two words in the 4-byte record at DAT_0051237C and grows
// DAT_005123AC when the index has reached the count. 0x001117E0, 0x00122F70,
// 0x000532E0, 0x0005B540, and 0x0005B780 are not translated, so those calls
// are not made. The 0x0005B780 match is not taken. Any other type returns 3.
// A handled store returns 2.
int fn_1259b0(Game& g, const ScriptNode& node, int param_2, int param_3)
{
    const u32 type = node.type;
    if (type != 0x29)
    {
        if (type == 0x4f)
        {
            if (param_2 == 0)
            {
                const size_t slot = size_t(g_queued_tag_count);
                g_queued_tags.resize(slot + 1);
                g_queued_tags[slot].at_8 = node_u16(g.scene_script, node, 8);
                g_queued_tags[slot].at_c = node_u32(g.scene_script, node, 0xC);
                g_queued_tag_count = g_queued_tag_count + 1;
                return 2;
            }
            const u16 word = node_u16(g.scene_script, node, 8);
            const u32 ebx = node_u32(g.scene_script, node, 0xC);
            (void)word;
            (void)ebx;
            return 2;
        }
        if (type == 0x56)
        {
            const u32 message_0 = 0x67;
            const u32 message_1 = u32(node_u16(g.scene_script, node, 8));
            (void)message_0;
            (void)message_1;
            return 2;
        }
        return 3;
    }
    if ((param_2 != 0) && (param_3 == 0))
    {
        if (g_flag_2378 == 0)
        {
            const u16 ax = node_u16(g.scene_script, node, 8);
            const u16 arg_c = node_u16(g.scene_script, node, 0xC);
            const u32 arg_14 = node_u32(g.scene_script, node, 0x14);
            (void)ax;
            (void)arg_c;
            (void)arg_14;
            if (node_u32(g.scene_script, node, 0x10) == 0)
            {
                return 2;
            }
            fn_117400(0xffff, 0);
            g_flag_2378 = 1;
        }
        else
        {
            const u16 key = node_u16(g.scene_script, node, 8);
            const u32 expect = node_u32(g.scene_script, node, 0xC);
            (void)key;
            (void)expect;
        }
        return 0;
    }
    i32 index = 0;
    if (0 < g_tag_pair_count)
    {
        const u32 key = node_u32(g.scene_script, node, 8);
        do
        {
            if (u32(g_tag_pairs[size_t(index)].at_8) == key)
            {
                break;
            }
            index = index + 1;
        } while (index < g_tag_pair_count);
    }
    if (size_t(g_tag_pairs.size()) <= size_t(index))
    {
        g_tag_pairs.resize(size_t(index) + 1);
    }
    g_tag_pairs[size_t(index)].at_8 = node_u16(g.scene_script, node, 8);
    g_tag_pairs[size_t(index)].at_c = node_u16(g.scene_script, node, 0xC);
    if (g_tag_pair_count <= index)
    {
        g_tag_pair_count = g_tag_pair_count + 1;
    }
    return 2;
}

// Xbox 0x00126140. Handler 8. The dword at node+4 is the type. The table
// covers types 0x17 through 0x88. Any other type, and the holes in that
// range, return 3.
//
// Type 0x17 returns 0 when the third argument is nonzero. Zero tests the
// dword at node+0x10. Nonzero would call 0x000D0530. The word at node+8,
// the dword at node+0xC, and ESI = node+0x14 would then go to 0x000D0550,
// and the case returns 2. Those two calls are not translated.
//
// Type 0x18 tests DAT_00512414. Zero would call 0x000D0680, stores 1, and
// returns 0. Nonzero compares DAT_005064D8 with 3. Equal stores 0 and
// returns 2. Otherwise it returns 0. 0x000D0680 is not translated.
//
// Type 0x54 copies five fields into DAT_00506540 and returns 2.
//
// Every other handled type would build a stack message and pass it to
// 0x000D05D0 in EDI. The header calls 0x00125C80, 0x00125CC0, 0x00125D00,
// 0x00125D40, 0x00125D80, 0x00125DC0, 0x00125E00, 0x00125E40, 0x00125E80,
// 0x00125EC0, 0x00125F00, 0x00125F40, 0x00125F80, 0x00125FC0, 0x00126000,
// 0x00126040, 0x00126080, 0x001260C0, and 0x00126100 are not translated, and
// neither is 0x000D05D0, so those calls are not made. 0x001077B0 runs where
// the dump calls it: EAX is the cstring at the listed node offset.
// 0x0001BB60, 0x00126940, and 0x00116C50 are not translated, so no index is
// invented and those calls are not made. A handled type returns 2. Type
// 0x1A returns 2 before 0x000D05D0 when bit 0x10 of DAT_0050EA9C is set.
// The second argument is unused.
int fn_126140(Game& g, const ScriptNode& node, int param_2, int param_3)
{
    (void)param_2;
    auto push_named = [&](u32 gate, u32 key_at, u16* words, u32& count)
    {
        if (node_u32(g.scene_script, node, gate) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, key_at);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            count = count + 1;
        }
    };
    switch (node.type)
    {
    case 0x17:
        if (param_3 != 0)
        {
            return 0;
        }
        if (node_u32(g.scene_script, node, 0x10) != 0)
        {
            // 0x000D0530 is not translated.
        }
        {
            const u16 ax = node_u16(g.scene_script, node, 8);
            const u32 arg = node_u32(g.scene_script, node, 0xC);
            (void)ax;
            (void)arg;
        }
        return 2;
    case 0x18:
        if (g_flag_2414 == 0)
        {
            g_flag_2414 = 1;
            return 0;
        }
        if (g_flag_64d8 == 3)
        {
            g_flag_2414 = 0;
            return 2;
        }
        return 0;
    case 0x19:
        // 0x00125C80, XMM0 = 1.5 (0x0032B210). 0x000D05D0 is not translated.
        return 2;
    case 0x1a:
    {
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        (void)value;
        if ((g_flags_ea9c & 0x10u) != 0)
        {
            return 2;
        }
        return 2;
    }
    case 0x1b:
        // 0x00125CC0, XMM0 = 0. 0x000D05D0 is not translated.
        return 2;
    case 0x1c:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const u16 word = static_cast<u16>(fn_1077b0(key.c_str()));
        const u32 absent = node_u32(g.scene_script, node, 0x48) == 0 ? 1u : 0u;
        (void)word;
        (void)absent;
        return 2;
    }
    case 0x1d:
    {
        const u32 lo = node_u32(g.scene_script, node, 8);
        const u32 hi = node_u32(g.scene_script, node, 0xC) + 1u;
        (void)lo;
        (void)hi;
        const std::string key = node_cstr(g.scene_script, node, 0x10);
        const u16 word = static_cast<u16>(fn_1077b0(key.c_str()));
        (void)word;
        return 2;
    }
    case 0x1e:
    {
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        (void)value;
        return 2;
    }
    case 0x1f:
        // 0x00126040, XMM0 = 2.0 (0x0032B110). 0x000D05D0 is not translated.
        return 2;
    case 0x20:
    {
        const u32 word = u32(node_u16(g.scene_script, node, 8));
        (void)word;
        return 2;
    }
    case 0x21:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const u16 word = static_cast<u16>(fn_1077b0(key.c_str()));
        const u32 slot = 0xffffffffu;
        (void)word;
        (void)slot;
        return 2;
    }
    case 0x22:
    {
        const u32 slot = 0xffffffffu;
        (void)slot;
        return 2;
    }
    case 0x23:
        // 0x00125EC0, XMM0 = 1.5. 0x000D05D0 is not translated.
        return 2;
    case 0x25:
    {
        u16 words[5]{};
        u32 count = 0;
        const u16 header = 0;
        push_named(0x48, 8, words, count);
        push_named(0x8c, 0x4c, words, count);
        push_named(0xd0, 0x90, words, count);
        push_named(0x114, 0xd4, words, count);
        push_named(0x158, 0x118, words, count);
        (void)header;
        (void)words;
        (void)count;
        return 2;
    }
    case 0x26:
        // 0x00125F40, XMM0 = 1.5. 0x000D05D0 is not translated.
        return 2;
    case 0x27:
    {
        const u32 value = node_u32(g.scene_script, node, 8);
        (void)value;
        return 2;
    }
    case 0x28:
        // 0x00125F80, XMM0 = 1.5. 0x000D05D0 is not translated.
        return 2;
    case 0x50:
    {
        u16 words[5]{};
        u32 count = 0;
        push_named(0x48, 8, words, count);
        push_named(0x8c, 0x4c, words, count);
        push_named(0xd0, 0x90, words, count);
        push_named(0x114, 0xd4, words, count);
        push_named(0x158, 0x118, words, count);
        if (node_u32(g.scene_script, node, 0x15c) != 0)
        {
            // 0x00126940(0) is not translated. EDI would be the chosen word.
        }
        (void)words;
        (void)count;
        return 2;
    }
    case 0x54:
        g_script_6540 = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        g_script_6544 = std::bit_cast<float>(node_u32(g.scene_script, node, 0xC));
        g_script_6548 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x10));
        g_script_654c = std::bit_cast<float>(node_u32(g.scene_script, node, 0x14));
        g_script_6550 = node_u32(g.scene_script, node, 0x18);
        return 2;
    case 0x5f:
    {
        const u32 lo = node_u32(g.scene_script, node, 8);
        const u32 hi = node_u32(g.scene_script, node, 0xC) + 1u;
        (void)lo;
        (void)hi;
        return 2;
    }
    case 0x87:
        // 0x001260C0, XMM0 = 1.5. 0x000D05D0 is not translated.
        return 2;
    case 0x88:
    {
        u16 words[5]{};
        u32 payloads[5]{};
        u32 count = 0;
        if (node_u32(g.scene_script, node, 0x4c) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 8);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            payloads[count] = node_u32(g.scene_script, node, 0x48);
            count = count + 1;
        }
        if (node_u32(g.scene_script, node, 0x94) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 0x50);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            payloads[count] = node_u32(g.scene_script, node, 0x90);
            count = count + 1;
        }
        if (node_u32(g.scene_script, node, 0xdc) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 0x98);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            payloads[count] = node_u32(g.scene_script, node, 0xd8);
            count = count + 1;
        }
        if (node_u32(g.scene_script, node, 0x124) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 0xe0);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            payloads[count] = node_u32(g.scene_script, node, 0x120);
            count = count + 1;
        }
        if (node_u32(g.scene_script, node, 0x16c) != 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 0x128);
            words[count] = static_cast<u16>(fn_1077b0(key.c_str()));
            payloads[count] = node_u32(g.scene_script, node, 0x168);
            count = count + 1;
        }
        if (node_u32(g.scene_script, node, 0x170) != 0)
        {
            // 0x00126940(0) is not translated. EDI would be the chosen word.
        }
        (void)words;
        (void)payloads;
        (void)count;
        return 2;
    }
    default:
        return 3;
    }
}

// Xbox 0x001269D0. Handler 9. The dword at node+4 is the type. Type 0x49 ORs
// the dword at node+8 into DAT_0050EA9C and returns 2. Type 0x4A clears those
// bits and returns 2. Any other type returns 3.
int fn_1269d0(Game& g, const ScriptNode& node)
{
    if (node.type == 0x49)
    {
        g_flags_ea9c = g_flags_ea9c | node_u32(g.scene_script, node, 8);
        return 2;
    }
    if (node.type != 0x4a)
    {
        return 3;
    }
    g_flags_ea9c = g_flags_ea9c & ~node_u32(g.scene_script, node, 8);
    return 2;
}

// Xbox 0x00126A10. Handler 10. The dword at node+4 is the type. The byte map
// at 0x00126D94 covers types 0x2A through 0x97. Any other type, and the holes
// in that range, return 3.
//
// Type 0x2A tests DAT_00512418. Zero would call 0x0013E8D0 with EBX = node+8,
// EDX = the dword at node+0x88, and node+0x8c on the stack. That call is not
// translated. A zero dword at node+0x10c returns 2. Any other value stores 1
// and returns 0. Nonzero would call 0x001412F0. That call is not translated,
// so its nonzero return is not taken and this path returns 0.
//
// Type 0x2B would call 0x000F4E50 with EAX = node+8. That call is not
// translated. The case returns 2.
//
// Type 0x51 calls 0x001077B0 with EAX = node+8. It walks DAT_00511B38. A
// matching word at object+6, and a zero dword at node+0x48 or that dword
// equal to the word at object+0x1FC, calls vtable +0xC with message id 1.
// An empty list returns 2. A finished walk returns 2.
//
// Type 0x58 builds a message whose first dword is 0x65. 0x0002B710 would
// fill the low word of the second dword. That call is not translated, so
// the word stays 0. 0x001077B0 runs with EAX = node+8. 0x00122D40 would
// take that tag in EDI and the message in EBX. That call is not translated.
// The case returns 2.
//
// Type 0x5C stores message id 0x36, calls 0x001077B0 with EAX = node+8, and
// would pass that tag in EDI with the message in EBX to 0x00122D40. That
// call is not translated. The case returns 2.
//
// Type 0x5E calls 0x001077B0 with EAX = node+8 and the dword at node+0x4c
// on the stack. XMM0 is the float at node+0x48. 0x000F4930 would take the
// tag, that dword, and XMM0. That call is not translated. The case returns 2.
//
// Type 0x6B calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, stores the byte at
// node+0x48 into object+0x969. A live pointer at object+0x958 whose pointer
// at +0x10 is live would call 0x00029C80 with EAX = that pointer, ECX = the
// slot, and EDX = 1. That call is not translated, so a zero return is not
// taken and the vtable call that would clear +0x10 is not made. An empty
// list returns 2. A finished walk returns 2.
//
// Type 0x70 calls 0x001077B0 with EAX = node+8. ESI would be that tag, XMM1
// the float at node+0x48, and XMM2 the float at node+0x4c, for 0x0010F6C0.
// That call is not translated. The case returns 2.
//
// Type 0x78 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, would call 0x00019660
// with EAX = the object and the dword at node+0x48 on the stack. That call
// is not translated. An empty list returns 2. A finished walk returns 2.
//
// Type 0x97 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, would call 0x00028210.
// That call is not translated. The word at object+0x990 is cleared. The
// message id is 0x4D and the dword at node+0x48 follows it. 0x000412A0
// writes the world position and 0x00041820 writes the world euler. Vtable
// +0xC receives that message. An empty list returns 2. A finished walk
// returns 2.
int fn_126a10(Game& g, const ScriptNode& node)
{
    switch (node.type)
    {
    case 0x2a:
        if (g_flag_2418 == 0)
        {
            const std::string name = node_cstr(g.scene_script, node, 8);
            const u32 key = node_u32(g.scene_script, node, 0x88);
            const std::string extra = node_cstr(g.scene_script, node, 0x8c);
            (void)name;
            (void)key;
            (void)extra;
            if (node_u32(g.scene_script, node, 0x10c) == 0)
            {
                return 2;
            }
            g_flag_2418 = 1;
        }
        return 0;
    case 0x2b:
    {
        const std::string name = node_cstr(g.scene_script, node, 8);
        (void)name;
        return 2;
    }
    case 0x51:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 mark = node_u32(g.scene_script, node, 0x48);
        for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
        {
            CamFocus* obj = link->host;
            if (obj == nullptr || obj->tag_6 != tag)
            {
                continue;
            }
            if (mark != 0 && mark != u32(obj->word_1fc))
            {
                continue;
            }
            i32 message = 1;
            if (obj->on_note != nullptr)
            {
                obj->on_note(obj, message);
            }
        }
        return 2;
    }
    case 0x58:
    {
        const u32 message_0 = 0x65;
        const u32 message_1 = 0;
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        (void)message_0;
        (void)message_1;
        (void)tag;
        return 2;
    }
    case 0x5c:
    {
        const u32 message = 0x36;
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        (void)message;
        (void)tag;
        return 2;
    }
    case 0x5e:
    {
        const u32 arg = node_u32(g.scene_script, node, 0x4c);
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 0x48));
        (void)arg;
        (void)tag;
        (void)value;
        return 2;
    }
    case 0x6b:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const u8 stored = node_u8(g.scene_script, node, 0x48);
        for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
        {
            if (link->actor == nullptr)
            {
                continue;
            }
            if (tag < 2 || link->actor->class_key == tag)
            {
                link->byte_969 = stored;
                if (link->slot != nullptr && link->slot->flag_10 != nullptr)
                {
                    // 0x00029C80 is not translated. A zero return is not taken.
                }
            }
        }
        return 2;
    }
    case 0x70:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const float xmm1 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x48));
        const float xmm2 = std::bit_cast<float>(node_u32(g.scene_script, node, 0x4c));
        (void)tag;
        (void)xmm1;
        (void)xmm2;
        return 2;
    }
    case 0x78:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 arg = node_u32(g.scene_script, node, 0x48);
        for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
        {
            if (link->actor == nullptr)
            {
                continue;
            }
            if (tag < 2 || link->actor->class_key == tag)
            {
                (void)arg;
            }
        }
        return 2;
    }
    case 0x97:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
        {
            if (link->actor == nullptr)
            {
                continue;
            }
            if (tag >= 2 && link->actor->class_key != tag)
            {
                continue;
            }
            link->word_990 = 0;
            u32 msg[9]{};
            msg[0] = 0x4d;
            msg[1] = node_u32(g.scene_script, node, 0x48);
            const Vec3 pos = actor_world_position(*link->actor);
            const Vec3 euler = actor_world_euler(*link->actor);
            msg[2] = std::bit_cast<u32>(pos.x);
            msg[3] = std::bit_cast<u32>(pos.y);
            msg[4] = std::bit_cast<u32>(pos.z);
            msg[5] = std::bit_cast<u32>(euler.x);
            msg[6] = std::bit_cast<u32>(euler.y);
            msg[7] = std::bit_cast<u32>(euler.z);
            if (link->on_message != nullptr)
            {
                link->on_message(link->actor, msg);
            }
        }
        return 2;
    }
    default:
        return 3;
    }
}

// Xbox 0x00126E10. Handler 11. The dword at node+4 is the type. The byte map
// at 0x001271F0 covers types 0x4D through 0x9E. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second argument
// is the flag 0x00128A00 passes: nonzero when a DAT_0050E4DC node has 0x7D
// at +8.
//
// Type 0x4D loads the word at node+8 into DX for 0x00129ED0. A hit would call
// 0x00041820 and 0x000412A0 with ESI set to that object, add the float at
// node+0x8C times 0.017453292 (0x0032B0FC) to the second euler float, then
// call 0x00112C50 and 0x00112CC0. 0x00112C50 is not translated. The port's
// pulse object does not carry +0x08, +0x14, +0x38, or +0x88, so the two
// camera writes are not applied to it and 0x00112CC0 is not called.
//
// Type 0x4E multiplies the float at node+0x10 by 0.01 (0x0032B124). 0x00019200
// would take that product in XMM1 and ECX = 0x0054DD7C. That call is not
// translated, so its nonzero return is not taken and 0x001400A0 is not called.
//
// Type 0x60 loads the word at node+8 into DX for 0x00129ED0. A hit would call
// 0x00112CC0 with ESI set to that object. That call is not made.
//
// Type 0x62 would call 0x0011F1C0(0x13, 1). A nonzero AX walks the records at
// DAT_0051108C+8, comparing the word at record+6 with AX and the dword at
// record+0x28 with the dword at node+8. A hit would call 0x0013D250. A miss
// calls 0x0011F1C0(0x13, 0) and stops when AX matches the first return or AX
// is 0. 0x0011F1C0 is not translated, so the walk is not entered.
//
// Type 0x67 with a zero second argument stores the byte at node+8 and the
// word at node+0xC into DAT_00512420 and increments DAT_00512440. Nonzero
// with a zero dword at node+0xC would call 0x0012C8A0 with the byte at node+8
// and 0x0012C950. Any other dword would call 0x0012C8A0 with 0x0012C930, then
// 0x00122CE0 with EDI pointing at message id 0x4A. Those calls are not
// translated.
//
// Type 0x6E would call 0x001249B0 with EDI = the dword at node+8 and EAX = 8.
// That call is not translated.
//
// Type 0x73 would call 0x001400D0. CX is the word at node+8. The nine stack
// words are node+0xC through node+0x2C. That call is not translated.
//
// Type 0x74 would call 0x00115F40 with EDI pointing at message id 1. That
// call is not translated.
//
// Type 0x8E would call 0x001189D0. XMM0 is the float at node+0xC, ECX is the
// dword at node+8, EAX points at node+0x1C, and the stack dwords are node+0x10,
// node+0x14, and node+0x18. That call is not translated.
//
// Type 0x9D would call 0x00049D10. A zero return loads the dword at node+8
// into ECX for 0x000F2A20. A nonzero return from that call increments
// DAT_00506C84 and calls 0x0012D7E0 with EDI = 1 and that count on the stack.
// 0x00049D10 and 0x000F2A20 are not translated, so the increment is not made.
//
// Type 0x9E with a nonzero dword at node+0x10 stores the dword at node+8 at
// DAT_00512448 + DAT_00512444*4. 0x000F29A0 would take ECX = that dword and
// its EAX would be stored at DAT_0051245C. That call is not translated, so
// the parallel dword is not stored. DAT_00512444 is then incremented. A
// nonzero dword at node+0xC would call 0x000F29D0. Zero would call 0x000F29F0.
// Both take ECX = the dword at node+8. Neither call is translated.
int fn_126e10(Game& g, const ScriptNode& node, int param_2)
{
    switch (node.type)
    {
    case 0x4d:
    {
        PulseNode* obj = fn_129ed0(node_u16(g.scene_script, node, 8));
        if (obj != nullptr)
        {
            const float extra = std::bit_cast<float>(node_u32(g.scene_script, node, 0x8c)) *
                                std::bit_cast<float>(u32{0x3c8efa35u});
            (void)extra;
        }
        return 2;
    }
    case 0x4e:
    {
        const float gate = std::bit_cast<float>(node_u32(g.scene_script, node, 0x10)) *
                           std::bit_cast<float>(u32{0x3c23d70au});
        (void)gate;
        return 2;
    }
    case 0x60:
    {
        PulseNode* obj = fn_129ed0(node_u16(g.scene_script, node, 8));
        if (obj != nullptr)
        {
            // 0x00112CC0 is not translated onto this object.
        }
        return 2;
    }
    case 0x62:
        return 2;
    case 0x67:
        if (param_2 == 0)
        {
            const size_t slot = size_t(g_gate_2440);
            g_gate_2420.resize(slot + 1);
            g_gate_2420[slot].at_8 = node_u8(g.scene_script, node, 8);
            g_gate_2420[slot].at_c = node_u16(g.scene_script, node, 0xc);
            g_gate_2440 = g_gate_2440 + 1;
            return 2;
        }
        if (node_u32(g.scene_script, node, 0xc) == 0)
        {
            const u8 index = node_u8(g.scene_script, node, 8);
            (void)index;
            return 2;
        }
        {
            const u8 index = node_u8(g.scene_script, node, 8);
            (void)index;
        }
        return 2;
    case 0x6e:
    {
        const u32 key = node_u32(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x73:
    {
        const u16 at_8 = node_u16(g.scene_script, node, 8);
        const u16 at_c = node_u16(g.scene_script, node, 0xc);
        const u16 at_10 = node_u16(g.scene_script, node, 0x10);
        const u16 at_14 = node_u16(g.scene_script, node, 0x14);
        const u16 at_18 = node_u16(g.scene_script, node, 0x18);
        const u16 at_1c = node_u16(g.scene_script, node, 0x1c);
        const u16 at_20 = node_u16(g.scene_script, node, 0x20);
        const u16 at_24 = node_u16(g.scene_script, node, 0x24);
        const u16 at_28 = node_u16(g.scene_script, node, 0x28);
        const u16 at_2c = node_u16(g.scene_script, node, 0x2c);
        (void)at_8;
        (void)at_c;
        (void)at_10;
        (void)at_14;
        (void)at_18;
        (void)at_1c;
        (void)at_20;
        (void)at_24;
        (void)at_28;
        (void)at_2c;
        return 2;
    }
    case 0x74:
        return 2;
    case 0x8e:
    {
        const float xmm0 = std::bit_cast<float>(node_u32(g.scene_script, node, 0xc));
        const u32 ecx = node_u32(g.scene_script, node, 8);
        const u32 at_10 = node_u32(g.scene_script, node, 0x10);
        const u32 at_14 = node_u32(g.scene_script, node, 0x14);
        const u32 at_18 = node_u32(g.scene_script, node, 0x18);
        (void)xmm0;
        (void)ecx;
        (void)at_10;
        (void)at_14;
        (void)at_18;
        return 2;
    }
    case 0x9d:
        return 2;
    case 0x9e:
    {
        if (node_u32(g.scene_script, node, 0x10) != 0)
        {
            const size_t slot = size_t(g_mark_2444);
            g_mark_2448.resize(slot + 1);
            g_mark_2448[slot] = node_u32(g.scene_script, node, 8);
            g_mark_2444 = g_mark_2444 + 1;
        }
        const u32 key = node_u32(g.scene_script, node, 8);
        (void)key;
        if (node_u32(g.scene_script, node, 0xc) != 0)
        {
            return 2;
        }
        return 2;
    }
    default:
        return 3;
    }
}

// DAT_0050E4E0. Name bytes, 0x80 long. The next dword is DAT_0050E560.
char g_name_e4e0[0x80]{};
i32 g_flag_e560 = 0;

// DAT_00512470. Type 0x13 sets this when the dword at node+0x88 is nonzero.
// Type 0x16 sets it when the dword at node+0x8C is nonzero. Type 0x53 sets it
// on the first visit.
i32 g_flag_2470 = 0;
// DAT_00512474. Type 0x14 reads this. 0x000582A0 is not translated, so the
// store of 1 on that nonzero return is not made.
i32 g_flag_2474 = 0;
// DAT_0051248C. Type 0x13 compares this dword with 3 and would pass that
// flag twice to 0x0004EDA0. The byte at [DAT_0051248C]+0x30F144 is g_decay_arm.
i32 g_state_248c = 0;
// DAT_004EE1EC. Type 0x14 returns 0 while this is nonzero. The text writer
// in entity.cpp stores 0.
i32 g_draw_e1ec = 0;

// Xbox 0x00127340. Handler 12. The dword at node+4 is the type. The byte map
// at 0x00127458 covers types 0x3A through 0x8D. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second and third
// arguments from 0x00128A00 are unused.
//
// Type 0x3A would call 0x00116D20. ESI is node+8 and the stack dword is 3.
// That call is not translated.
//
// Type 0x3B tests the byte at DAT_0050E4E0. Zero falls through and returns 2.
// Nonzero would call 0x00038460 with that address. The dword at bank+0x88 is
// decremented. A result less than 1, with a bank id other than 0x12 and a
// nonzero return from 0x00038590, would call 0x00038360. 0x00038260 then
// takes the bank in EAX and the id is stored as 0. 0x00038590, 0x00038360,
// and 0x00038260 are not translated, and the bank object from 0x00038460 is
// not visible here, so those stores are not made. The byte at DAT_0050E4E0
// is then cleared.
//
// Type 0x45 tests DAT_0050E560. Nonzero falls through and returns 2. Zero
// would call 0x00116C90, stores 1, and returns 2. That call is not translated.
//
// Type 0x55 calls 0x001077B0 with EAX = the cstring at node+8. ECX would be
// that return for 0x00116C50. That call is not translated.
//
// Type 0x57 would call 0x00116D20. ESI is node+8 and the stack dword is 2.
// That call is not translated.
//
// Type 0x8D would call 0x00038720. EAX is node+8 and the stack dword is 6.
// That call is not translated. The case falls through and returns 2.
int fn_127340(Game& g, const ScriptNode& node)
{
    switch (node.type)
    {
    case 0x3a:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x3b:
        if (g_name_e4e0[0] != 0)
        {
            g_name_e4e0[0] = 0;
            return 2;
        }
        break;
    case 0x45:
        if (g_flag_e560 == 0)
        {
            g_flag_e560 = 1;
            return 2;
        }
        break;
    case 0x55:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        const u32 tag = fn_1077b0(key.c_str());
        (void)tag;
        return 2;
    }
    case 0x57:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x8d:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        break;
    }
    default:
        return 3;
    }
    return 2;
}

// Xbox 0x001274B0. Handler 13. The dword at node+4 is the type. The byte map
// at 0x00127A2C covers types 0x13 through 0x9C. Slots are 0x13, 0x14, 0x15,
// 0x16, 0x53, 0x9B, and 0x9C. Any other type returns 3. The second argument
// from 0x00128A00 is unused. The third argument is the value that walk passes.
//
// Type 0x13 with a nonzero third argument returns 2. Zero with DAT_00512470
// clear would call 0x00087830. A return of 2 copies
// "aid_model_ghoulies_actor_boy" and "aid_model_ghoulies_actor_girl" into
// 0x80-byte buffers and calls 0x0004E610 with EAX and ESI on those buffers.
// That call is not translated, so the copy is not made. 0x0004EDA0 would
// then take node+8 and two copies of (DAT_0051248C == 3). That call is not
// translated. A nonzero dword at node+0x88 stores 1, calls 0x00117400 with
// 0xFFFF when node+0x8C is zero and with 2 otherwise, then would call
// 0x000F2BB0 with ECX = 1. A zero return, or a nonzero return from
// 0x00049D10, would call 0x00127AC0. Those three calls are not translated,
// so 0x00127AC0 is not called. The case returns 0. A zero dword at node+0x88
// returns 2. Nonzero DAT_00512470 would call 0x0004EFE0 and 0x000532E0 with
// ECX = the node. A zero from 0x0004EFE0 returns 0 when DAT_00512478 is zero
// or when 0x00127B10 returns zero. A zero from 0x000532E0 clears
// DAT_00512470, calls 0x0004ECE0, calls 0x00117400 with the same mask and
// flag 1, calls 0x000583A0 when DAT_004EE1EC is nonzero, and calls
// 0x00127B50 and returns 2 when DAT_00512478 is nonzero. Those calls are
// not translated, so the zero returns are not taken and this path returns 2.
//
// Type 0x14 with a nonzero third argument returns 2. Nonzero DAT_00512474
// returns 0 while DAT_004EE1EC is nonzero. Otherwise it calls
// 0x00117400(0xFFFF, 1), clears DAT_00512474, and returns 2. Zero calls
// 0x001077B0 with EAX = node+8. A tag above 1 would call 0x001291A0(tag, 0).
// A zero return would return 2. That call is not translated, so the return
// is not taken. A nonzero dword at node+0x8C would call 0x00049D10. A
// nonzero return would return 2. That call is not translated, so the return
// is not taken. A nonzero dword at node+0x90 tests bit 0x8000000 of
// DAT_0050EA9C when node+0x94 is 0, and bit 0x10000000 when node+0x94 is 1.
// A set bit returns 2. Any other node+0x94 skips the test. 0x000FBF90 would
// take EAX = node+0x48. A nonzero return would build "%s%d" at DAT_00508EB8
// through 0x001661F9 when node+0x88 is nonzero, calling 0x0001BB60 with
// ECX = that dword + 1, EDX = 1, and ESI = 0x0054DD7C. 0x000582A0 would
// then take ECX = that buffer, or node+0x48 when node+0x88 is zero, and a
// stack 0. A nonzero return calls 0x00117400(0xFFFF, 0), stores 1, and ORs
// the same DAT_0050EA9C bit. node+0x94 equal to 1 returns 0 after the OR.
// The other path returns 0 as well. 0x000FBF90, 0x0001BB60, and 0x000582A0
// are not translated, so that block is not entered and the case returns 2.
//
// Type 0x15 would call 0x00106D50 with the byte at node+0x88, the dword at
// node+0x8C, and 1. EDI is node+8. 0x00106DD0 then takes DL = that byte.
// Neither call is translated. The case returns 2.
//
// Type 0x16 with a nonzero third argument returns 2. Nonzero DAT_00512470
// would call 0x0004EFE0. A zero return returns 0. A nonzero return clears
// the flag, calls 0x0004ECE0, and returns 2. That call is not translated,
// so the nonzero return is not taken and this path returns 0. A zero flag
// with a nonzero dword at node+0x8C stores 1 and returns 0. Otherwise the
// case returns 2.
//
// Type 0x53 with a nonzero third argument returns 2. Zero with DAT_00512470
// clear would call 0x0004EDA0(node+8, 0, 1) and pass that return with
// "aid_model_ghoulies_cutscene_reliccandle" to 0x0004EA50. A zero return
// stores 0 at DAT_00512480. A nonzero return calls 0x00107720 with EAX =
// "aid_objparams_ghoulies_scenecontrol_candle", stores the model at +8,
// stores 0x00107520 at DAT_00512480, and calls 0x000663E0 on that block.
// 0x0011F1C0(0xC, 1) would then run. A nonzero AX calls 0x0011F260,
// 0x0011F2A0, 0x00100DF0(pi), 0x0004EF20, and 0x0004EF80. Those calls are
// not translated, so DAT_00512480 is not stored. The flag is set and the
// case returns 0. Nonzero DAT_00512470 would call 0x0004EFE0. A zero return
// returns 0. A nonzero return calls 0x000532E0 with ECX = the node. A
// nonzero return from that call returns 2. Zero clears the flag, calls
// 0x0004ECE0 and 0x00127FF0, and returns 2 when DAT_00512480 is zero.
// Otherwise it calls 0x00116140, clears DAT_00512480, and returns 2.
// 0x0004EFE0 is not translated, so its nonzero return is not taken and this
// path returns 0.
//
// Type 0x9B calls 0x000FB120 with 1.0, EAX = node+8, and ECX = a stack
// vector. Type 0x9C calls 0x000493D0 with EDX = node+8 and EAX = that
// vector. DAT_00511B40 null, or the sentinel 0x93C, zeros the vector.
// Any other pointer calls 0x000412A0 with ESI = that pointer minus 0x93C
// and EDI = the vector. 0x000FB120 and 0x000493D0 are not translated, and
// the port's 0x000412A0 does not take this ESI/EDI pair, so those calls
// are not made. Both cases return 2.
int fn_1274b0(Game& g, const ScriptNode& node, int param_3)
{
    switch (node.type)
    {
    case 0x13:
        if (param_3 == 0)
        {
            if (g_flag_2470 == 0)
            {
                const i32 same = g_state_248c == 3 ? 1 : 0;
                (void)same;
                if (node_u32(g.scene_script, node, 0x88) != 0)
                {
                    g_flag_2470 = 1;
                    const u32 mask = node_u32(g.scene_script, node, 0x8c) == 0 ? 0xffffu : 2u;
                    fn_117400(mask, 0);
                    return 0;
                }
            }
        }
        return 2;
    case 0x14:
        if (param_3 != 0)
        {
            return 2;
        }
        if (g_flag_2474 != 0)
        {
            if (g_draw_e1ec != 0)
            {
                return 0;
            }
            fn_117400(0xffff, 1);
            g_flag_2474 = 0;
            return 2;
        }
        {
            const std::string key = node_cstr(g.scene_script, node, 8);
            const u16 tag = static_cast<u16>(fn_1077b0(key.c_str()));
            if (tag > 1)
            {
                // 0x001291A0 is not translated.
            }
        }
        if (node_u32(g.scene_script, node, 0x8c) != 0)
        {
            // 0x00049D10 is not translated.
        }
        if (node_u32(g.scene_script, node, 0x90) != 0)
        {
            const u32 kind = node_u32(g.scene_script, node, 0x94);
            if (kind == 0)
            {
                if ((g_flags_ea9c & 0x8000000u) != 0)
                {
                    return 2;
                }
            }
            else if (kind == 1)
            {
                if ((g_flags_ea9c & 0x10000000u) != 0)
                {
                    return 2;
                }
            }
        }
        return 2;
    case 0x15:
    {
        const u8 index = node_u8(g.scene_script, node, 0x88);
        const u32 word = node_u32(g.scene_script, node, 0x8c);
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)index;
        (void)word;
        (void)key;
        return 2;
    }
    case 0x16:
        if (param_3 == 0)
        {
            if (g_flag_2470 != 0)
            {
                return 0;
            }
            if (node_u32(g.scene_script, node, 0x8c) != 0)
            {
                g_flag_2470 = 1;
                return 0;
            }
        }
        return 2;
    case 0x53:
        if (param_3 == 0)
        {
            if (g_flag_2470 == 0)
            {
                g_flag_2470 = 1;
            }
            return 0;
        }
        return 2;
    case 0x9b:
        return 2;
    case 0x9c:
        return 2;
    default:
        return 3;
    }
}

// DAT_00430d08. Types 0x46 and 0xA6 store 0 when the second argument is zero.
// The same path stores a zero byte at DAT_00430d14. 0x0011EB30 clears both
// from entity.cpp; that copy is private to that file.
i32 g_flag_30d08 = 0;
u8 g_byte_30d14 = 0;

// Xbox 0x00127BC0. Handler 14. The dword at node+4 is the type. The byte map
// at 0x00127F6C covers types 0x2C through 0xA6. Slot 14 of the table at
// 0x00127F30 returns 3, and the holes in that range use it. A type outside
// the range returns 3 as well. The second argument is the flag 0x00128A00
// passes. The third argument is the value that walk passes.
//
// Type 0x2C with a nonzero third argument returns 2. Zero calls 0x001292D0.
// A null return returns 2. A live object loads the dword at object+0x95C
// into EAX, calls 0x0001B810 with two stack zeros, calls 0x00112CC0, and
// returns 2. 0x001292D0 is not translated, so the live path is not entered.
//
// Type 0x2D with a nonzero third argument returns 2. Zero would call
// 0x00127FF0 and return 2. That call is not translated.
//
// Type 0x2E calls 0x001292D0. A null return returns 2. A live object tests
// the dword at node+8. Zero calls 0x00042090. Nonzero calls 0x000420B0.
// EAX is the object and ECX is that dword. Both return 2. 0x001292D0 is not
// translated, so neither call is made.
//
// Type 0x2F calls 0x001292D0. A null return returns 2. A live object passes
// the stack slot at frame+0x1C to 0x00049DB0 in EAX, stores 7 in the next
// dword, and calls vtable +0xC with that slot. That call is not translated.
//
// Type 0x30 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x32 and a second dword of 0.
//
// Type 0x31 with a zero second argument calls 0x00123AD0. EAX is node+8 and
// ECX is 0. That call is not translated. A nonzero second argument calls
// 0x001292D0. A null return returns 2. A live object calls 0x0013A720 with
// EAX = node+8, ECX = the object, and a stack 1, then returns 2.
//
// Type 0x46 with a zero second argument stores 0 at DAT_00430d08 and a zero
// byte at DAT_00430d14, then returns 2. Nonzero calls 0x001292D0. A null
// return returns 2. A live object is pushed for 0x0013A7C0, then 0x0012ECC0
// runs, and the case returns 2. Those calls are not translated.
//
// Type 0x6F calls 0x001292D0. A null return returns 0. A live object calls
// 0x00028360 with ESI set to that object. AX 0 returns 2. A nonzero AX
// returns 2 when the dword at object+0x984 is nonzero, and 0 when that
// dword is 0. 0x001292D0 is not translated, so the live path is not entered
// and the case returns 0.
//
// Type 0x76 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x42 and a second dword of 1.
//
// Type 0x77 is that same call with a second dword of 0.
//
// Type 0x91 calls 0x001292D0. A null return returns 2. A live object calls
// 0x00128DF0 with the object, two zeros, the address of the node pointer,
// 0x4240, and 0. vtable +0xC then takes message id 0x37 and three zero
// dwords. A nonzero return from 0x00128DF0 would call 0x000412A0 with ESI
// set to that return and EDI aimed at the first zero. Those calls are not
// translated.
//
// Type 0x96 calls 0x001292D0. A null return returns 2. A live object calls
// 0x0011F1C0 with EAX = 0 and stack words 2, 1. A zero AX returns 2. Nonzero
// compares the word at record+0x28 from 0x0011EF20 with the dword at node+8.
// A miss calls 0x0011F1C0 with EAX = the previous AX and stack words 2, 0,
// and returns 2 when that AX is 0. A hit calls 0x0002B430 with EAX = the
// object and ECX = record+0xC, then 0x0002B560 with ECX = record+0x18.
// 0x001292D0 is not translated, so the walk is not entered. The case
// returns 2.
//
// Type 0xA0 calls 0x001292D0. A null return returns 2. A live object stores
// 0x2C over the first argument and calls vtable +0xC with that address.
//
// Type 0xA6 with a zero second argument takes the same DAT_00430d08 store as
// type 0x46. Nonzero calls 0x001292D0. A null return returns 2. A live object
// is pushed for 0x0013A860. A zero return returns 2. Nonzero calls 0x0012ECC0
// and returns 2. Those calls are not translated.
int fn_127bc0(Game& g, const ScriptNode& node, int param_2, int param_3)
{
    switch (node.type)
    {
    case 0x2c:
        if (param_3 == 0)
        {
            // 0x001292D0 is not translated, so object+0x95C is not loaded.
            return 2;
        }
        return 2;
    case 0x2d:
        if (param_3 == 0)
        {
            // 0x00127FF0 is not translated.
            return 2;
        }
        return 2;
    case 0x2e:
        return 2;
    case 0x2f:
        return 2;
    case 0x30:
        return 2;
    case 0x31:
        if (param_2 == 0)
        {
            const u32 at_8 = node_u32(g.scene_script, node, 8);
            (void)at_8;
            return 2;
        }
        return 2;
    case 0x46:
        if (param_2 == 0)
        {
            g_flag_30d08 = 0;
            g_byte_30d14 = 0;
            return 2;
        }
        return 2;
    case 0x6f:
        return 0;
    case 0x76:
        return 2;
    case 0x77:
        return 2;
    case 0x91:
        return 2;
    case 0x96:
        return 2;
    case 0xa0:
        return 2;
    case 0xa6:
        if (param_2 == 0)
        {
            g_flag_30d08 = 0;
            g_byte_30d14 = 0;
            return 2;
        }
        return 2;
    default:
        return 3;
    }
}

// DAT_0050EA98. Type 0x4 stores the dword at node+8.
u32 g_flag_ea98 = 0;
// DAT_00427958. Type 0x6 stores the dword at node+8.
u32 g_flag_27958 = 0;
// DAT_00430D10. Type 0x7 stores the dword at node+8. entity.cpp keeps a
// private copy of this address.
i32 g_script_30d10 = 0;
// DAT_00507E78. Type 0xA clears the first word before 0x000FF0A0.
u16 g_loc_7e78 = 0;
// Image at 0x00314DE8. Type 0x61 indexes it from four bases. The aid_ string
// at 0x00314E50 is the end of the numeric image.
constexpr u32 kTuneDe8 = 0x00314de8u;
constexpr u32 kScriptTune[] = {
    0x40a00000u, 0x41a00000u, 0x41f00000u, 0x42200000u, 0x00000000u,
    0x40e00000u, 0x41700000u, 0x41c80000u, 0x420c0000u, 0x00000000u,
    0x0000002du, 0x00000028u, 0x00000023u, 0x0000001eu, 0x00000019u,
    0x0000003cu, 0x00000000u, 0x3d4ccccdu, 0x3dcccccdu, 0x3e4ccccdu,
    0xbf800000u, 0x00000000u, 0x3d4ccccdu, 0x3dcccccdu, 0x3e4ccccdu,
    0x00000000u,
};

u32 script_tune(u32 base, u32 index)
{
    const u32 slot = (base - kTuneDe8) / 4u + index;
    if (slot >= std::size(kScriptTune))
    {
        return 0;
    }
    return kScriptTune[slot];
}

// DAT_00545AEC, DAT_00545AF0, DAT_00545AF4, DAT_00545AF8. Type 0x61 stores
// the four indexes. The looked-up words land at DAT_00434008, DAT_00434014,
// DAT_00434010, DAT_0043402C, and DAT_00434030.
u32 g_tune_5aec = 0;
u32 g_tune_5af0 = 0;
u32 g_tune_5af4 = 0;
u32 g_tune_5af8 = 0;
u32 g_tune_34008 = 0;
u32 g_tune_34014 = 0;
u32 g_tune_34010 = 0;
u32 g_tune_3402c = 0;
u32 g_tune_34030 = 0;
// DAT_00512134, DAT_00512138, DAT_00512104. Type 0x64.
u32 g_color_2134 = 0;
u32 g_flag_2138 = 0;
u32 g_flag_2104 = 0;
// DAT_0050E1B4. DAT_0050E1B0 is the count, 4 bytes in front of the words.
// The next dword is DAT_0050E1C4.
std::vector<u16> g_tags_e1b4;
i32 g_tag_e1b0 = 0;
// DAT_00506C60. Type 0x89 stores node+8 * 60 + 0.99.
float g_script_6c60 = 0.f;
// DAT_00509B74 and DAT_00509B70. Type 0x8A. entity.cpp keeps private copies
// for the light pass.
i32 g_script_9b74 = 0;
i32 g_script_9b70 = 0;
// DAT_005110B4. Type 0xA2 stores 1 when node+8 is 0.
u32 g_flag_10b4 = 0;
// DAT_0050EF1C. Type 0xA4 stores 1 when node+8 is nonzero.
u32 g_flag_ef1c = 0;
// DAT_00510650. Type 0xA5 stores 1 when node+8 is nonzero.
u32 g_flag_0650 = 0;

// Xbox 0x00128050. Handler 15. The dword at node+4 is the type. The byte map
// at 0x001286BC covers types 0x1 through 0xA5. Any other type, and the holes
// in that range, return 3. The second argument is the flag 0x00128A00 passes.
// The third argument is the value that walk passes. A handled type returns 2.
//
// Type 0x1 would call 0x0011C890 with EAX = node+8. That call is not translated.
//
// Type 0x2 with a zero second argument would call 0x0011C950 with EDX = node+8.
// A nonzero second argument would call 0x00107720 with EAX = node+8. A zero
// return falls through and returns 2. A nonzero return would call 0x00107520
// with that block, then 0x000663E0 on it. 0x0011C950, 0x00107720, and
// 0x00107520 are not translated, so the nonzero return is not taken.
//
// Type 0x4 stores the dword at node+8 into DAT_0050EA98.
// Types 0x5 and 0xC return 2.
// Type 0x6 stores the dword at node+8 into DAT_00427958.
// Type 0x7 stores the dword at node+8 into DAT_00430D10.
//
// Type 0x8 calls 0x001077B0 with EAX = node+8. 0x0010F690 would take that tag
// in ECX and the dword at node+0x48 on the stack. That call is not translated.
// The walk of DAT_00511B48 still runs. A word at object+6 equal to the tag,
// and a live pointer at object+0x958, stores the dword at node+0x48 into
// that pointer +0x6C. An empty list returns 2. A finished walk returns 2.
//
// Type 0x9 calls 0x001077B0 with EAX = node+8. 0x0010F660 would take that tag
// in ECX and the float at node+0x48 in XMM0. The message is id 0x4E and that
// same float. 0x00122D40 would take the tag in EDI and the message in EBX.
// Those two calls are not translated.
//
// Type 0xA clears the word at DAT_00507E78, then would call 0x000FF0A0 with
// "aid_loctext_ghoulies_script_scenenames", "scenenames", node+8, and that
// word. A zero return returns 2. A nonzero return would call 0x000FBE10 on
// the word. A nonzero return from that call returns 2. Zero, with a nonzero
// dword at node+0x48 and a zero third argument, draws when DAT_00512484 is
// live. A null DAT_00512484 would call 0x0005F390(0, 0, 0x5E4), step
// DAT_0054DD7C, and store 2.5, 0.5, and the new seed at +0x14, +0x18, and
// +0x1C. 0x0005F550, 0x000606D0, and 0x0005F580 would then run, and
// 0x0011DCF0 runs on the way out. 0x000FF0A0 is not translated, so its
// nonzero return is not taken and that block is not entered.
//
// Type 0xB would call 0x00123AD0. ECX is the byte at node+8 and EAX is node+9.
// Type 0x4B would call 0x000FD780 with ESI = node+8.
// Type 0x4C would call 0x0003A1F0 with ESI = the dword at node+8.
// Type 0x52 would call 0x0012AA50 with node+8 on the stack.
// Those calls are not translated.
//
// Type 0x61 stores four indexes and the words they select from 0x00314DE8,
// 0x00314DFC, 0x00314E10, 0x00314E28, and 0x00314E38. The second float uses
// the index at node+8, the same index as the first.
//
// Type 0x64 packs the bytes at node+0x28, node+0x2C, and node+0x30 into
// DAT_00512134. DAT_00512138 is 1 when the dword at node+8 is nonzero.
// DAT_00512104 copies that dword. 0x001236D0 would take the packed bytes at
// node+0xC, node+0x10, node+0x14, and node+0x18 twice, then the dwords at
// node+0x1C, node+0x20, and node+0x24. XMM0 is the float at node+0x1C.
// That call is not translated.
//
// Type 0x6D calls 0x001077B0 with EAX = node+8 and stores the word at
// DAT_0050E1B4 + DAT_0050E1B0*2, then increments the count.
//
// Type 0x79 would call 0x00114050. ECX is the dword at node+8. BL is 2 when
// that dword is 0 and 1 otherwise. That call is not translated.
//
// Type 0x89 stores the float at node+8 times 60 plus 0.99 into DAT_00506C60.
// 0x000F27C0 would then run with EAX = 1. That call is not translated.
//
// Type 0x8A with a nonzero dword at node+8 stores 0 at DAT_00509B74 and 1 at
// DAT_00509B70. Zero stores 1 and 0.
//
// Type 0x8F builds the vector (0, 1, 0) and the float at node+8 times
// 0.0055555557 times pi. 0x001047E0 would take that vector and that angle.
// The cross product that follows reads the matrix that call writes, and
// 0x000FE160 would take the result. Neither call is translated, so the
// product is not formed.
//
// Type 0x9A would call 0x000F2740. Type 0xA1 would call 0x000FEA90 with EAX =
// the dword at node+8. Those calls are not translated.
//
// Type 0xA2 stores 1 at DAT_005110B4 when the dword at node+8 is 0.
// Type 0xA4 stores 1 at DAT_0050EF1C when that dword is nonzero.
// Type 0xA5 stores 1 at DAT_00510650 when that dword is nonzero.
int fn_128050(Game& g, const ScriptNode& node, int param_2, int param_3)
{
    switch (node.type)
    {
    case 1:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 2:
        if (param_2 == 0)
        {
            const std::string key = node_cstr(g.scene_script, node, 8);
            (void)key;
            return 2;
        }
        {
            const std::string key = node_cstr(g.scene_script, node, 8);
            (void)key;
        }
        return 2;
    case 4:
        g_flag_ea98 = node_u32(g.scene_script, node, 8);
        return 2;
    case 5:
    case 0xc:
        return 2;
    case 6:
        g_flag_27958 = node_u32(g.scene_script, node, 8);
        return 2;
    case 7:
        g_script_30d10 = i32(node_u32(g.scene_script, node, 8));
        return 2;
    case 8:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const u32 stored = node_u32(g.scene_script, node, 0x48);
        for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
        {
            if (link->actor == nullptr || link->actor->class_key != tag)
            {
                continue;
            }
            if (link->slot == nullptr)
            {
                continue;
            }
            link->slot->at_6c = stored;
        }
        return 2;
    }
    case 9:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 0x48));
        const u32 message_0 = 0x4e;
        (void)tag;
        (void)value;
        (void)message_0;
        return 2;
    }
    case 0xa:
    {
        g_loc_7e78 = 0;
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        (void)param_3;
        return 2;
    }
    case 0xb:
    {
        const u8 index = node_u8(g.scene_script, node, 8);
        const std::string key = node_cstr(g.scene_script, node, 9);
        (void)index;
        (void)key;
        return 2;
    }
    case 0x4b:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x4c:
    {
        const u32 key = node_u32(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x52:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0x61:
    {
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        const u32 at_c = node_u32(g.scene_script, node, 0xc);
        const u32 at_10 = node_u32(g.scene_script, node, 0x10);
        const u32 at_14 = node_u32(g.scene_script, node, 0x14);
        g_tune_34008 = script_tune(0x00314de8u, at_8);
        g_tune_5aec = at_8;
        g_tune_5af0 = at_c;
        g_tune_34014 = script_tune(0x00314dfcu, at_8);
        g_tune_5af4 = at_10;
        g_tune_3402c = script_tune(0x00314e28u, at_10);
        g_tune_5af8 = at_14;
        g_tune_34010 = script_tune(0x00314e10u, at_c);
        g_tune_34030 = script_tune(0x00314e38u, at_14);
        return 2;
    }
    case 0x64:
    {
        const u32 color = u32(node_u8(g.scene_script, node, 0xc)) |
                          (u32(node_u8(g.scene_script, node, 0x10)) << 8) |
                          (u32(node_u8(g.scene_script, node, 0x14)) << 16) |
                          (u32(node_u8(g.scene_script, node, 0x18)) << 24);
        g_color_2134 = u32(node_u8(g.scene_script, node, 0x28)) |
                       (u32(node_u8(g.scene_script, node, 0x2c)) << 8) |
                       (u32(node_u8(g.scene_script, node, 0x30)) << 16);
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        g_flag_2138 = at_8 != 0 ? 1u : 0u;
        g_flag_2104 = at_8;
        const u32 at_1c = node_u32(g.scene_script, node, 0x1c);
        const u32 at_20 = node_u32(g.scene_script, node, 0x20);
        const u32 at_24 = node_u32(g.scene_script, node, 0x24);
        const float xmm0 = std::bit_cast<float>(at_1c);
        (void)color;
        (void)at_20;
        (void)at_24;
        (void)xmm0;
        return 2;
    }
    case 0x6d:
    {
        const u16 tag = static_cast<u16>(fn_1077b0(node_cstr(g.scene_script, node, 8).c_str()));
        const size_t slot = size_t(g_tag_e1b0);
        g_tags_e1b4.resize(slot + 1);
        g_tags_e1b4[slot] = tag;
        g_tag_e1b0 = g_tag_e1b0 + 1;
        return 2;
    }
    case 0x79:
    {
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        const u8 bl = at_8 == 0 ? u8{2} : u8{1};
        (void)bl;
        return 2;
    }
    case 0x89:
    {
        const float value = std::bit_cast<float>(node_u32(g.scene_script, node, 8));
        g_script_6c60 = value * std::bit_cast<float>(0x42700000u) + std::bit_cast<float>(0x3f7d70a4u);
        return 2;
    }
    case 0x8a:
        if (node_u32(g.scene_script, node, 8) != 0)
        {
            g_script_9b74 = 0;
            g_script_9b70 = 1;
            return 2;
        }
        g_script_9b74 = 1;
        g_script_9b70 = 0;
        return 2;
    case 0x8f:
    {
        const float x = 0.f;
        const float y = std::bit_cast<float>(0x3f800000u);
        const float z = 0.f;
        const float angle = std::bit_cast<float>(node_u32(g.scene_script, node, 8)) *
                            std::bit_cast<float>(0x3bb60b61u) * std::bit_cast<float>(0x40490fdbu);
        (void)x;
        (void)y;
        (void)z;
        (void)angle;
        return 2;
    }
    case 0x9a:
        return 2;
    case 0xa1:
    {
        const u32 key = node_u32(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0xa2:
        g_flag_10b4 = node_u32(g.scene_script, node, 8) == 0 ? 1u : 0u;
        return 2;
    case 0xa4:
        g_flag_ef1c = node_u32(g.scene_script, node, 8) != 0 ? 1u : 0u;
        return 2;
    case 0xa5:
        g_flag_0650 = node_u32(g.scene_script, node, 8) != 0 ? 1u : 0u;
        return 2;
    default:
        return 3;
    }
}

// Xbox 0x00128770. Handler 16. The dword at node+4 is the type. Type 0x39
// loads the dword at node+8 into EDI and calls 0x0012D050. That call walks
// DAT_00511B28, the first node of DAT_00511B24. A zero EDI calls 0x00147860
// on each object. A nonzero EDI decrements the dword at object+0xAD0 and
// calls 0x0003EEB0 when that counter hits 0. 0x0012D050 is not translated.
// The case returns 2.
//
// Type 0x59 stores 1 in the incoming argument slot, then calls 0x000FBFB0
// with count 0xE9, the names at 0x0042A854, and the cstring at node+8. A miss
// zeros EDI. A hit increments the index and zero-extends it into EDI. Both
// paths point EBX at that slot and call 0x00122FA0. That call walks
// DAT_00511B28, compares DI with the word at object+6, and calls vtable +0xC
// with EBX on a match. 0x00122FA0 is not translated. Both paths return 2.
// Any other type returns 3.
int fn_128770(Game& g, const ScriptNode& node)
{
    if (node.type == 0x39)
    {
        const u32 flag = node_u32(g.scene_script, node, 8);
        (void)flag;
        return 2;
    }
    if (node.type != 0x59)
    {
        return 3;
    }
    const i32 message = 1;
    const std::string key = node_cstr(g.scene_script, node, 8);
    const i32 found = fn_fbfb0(0xe9, kObjTagActor, key.c_str());
    if (found == -1)
    {
        const u16 tag = 0;
        (void)tag;
        (void)message;
        return 2;
    }
    const u16 tag = static_cast<u16>(found + 1);
    (void)tag;
    (void)message;
    return 2;
}

// DAT_00512514. 0x00128AB0 stores 1 when the dword it kept at DAT_0051248C is 0.
// Type 0xF reads it. script_set_name does not write this dword.
i32 g_gate_2514 = 0;
// DAT_00512518. Type 0xF stores 0, then 1 when DAT_0051251C is already 0.
i32 g_gate_2518 = 0;
// DAT_0051251C. Types 0xF and 0x10 decrement it. Type 0x11 increments it.
i32 g_gate_251c = 0;
// DAT_0050E798 and DAT_0050E79C. Type 0x69 copies node+8 and node+0xC.
u32 g_script_e798 = 0;
u32 g_script_e79c = 0;

// Xbox 0x00128BC0. Handler 0 of the table at 0x00312650. The dword at node+4
// is the type. The byte map at 0x00128D24 covers types 0 through 0x69. Slot 7
// of the table at 0x00128D04 returns 3, and every type in that range other
// than 0, 0xD, 0xE, 0xF, 0x10, 0x11, and 0x69 uses it. A type above 0x69
// returns 3. The second argument is the flag 0x00128A00 passes. The third
// argument is the value that walk passes.
//
// Type 0 returns 1.
//
// Type 0xD calls 0x00053170 with the address of node+8 and a stack 0. That
// call compares the first dword with "aid_". It is not translated. The case
// returns 2.
//
// Type 0xE calls 0x00123B00. The dword at node+0x90 equal to 1 takes the
// following call when that return is nonzero. Equal to 2 takes it when the
// return is zero. A nonzero dword at node+0x94 calls 0x00049D10 and takes
// the same call when that return is nonzero. 0x00123B00 and 0x00049D10 are
// not translated, so those nonzero returns are not taken. The zero return
// of 0x00123B00 with node+0x90 equal to 2 would call 0x000532B0 with ECX set
// to the node cursor and EDX set to 0xF, then return 2. 0x000532B0 is not
// translated, so the cursor stays put and the case returns 2.
// Any other node+0x90 calls 0x00128AB0. EAX is the cstring at node+0x10, ECX
// is the dword at node+8, and EDX is the dword at node+0xC. A nonzero third
// argument with a zero dword at node+8 returns 0. A zero second argument and
// a zero third argument return 0. Otherwise the case returns 2.
//
// Type 0xF stores 0 at DAT_00512518. A zero DAT_00512514 returns 2. A zero
// DAT_0051251C stores 1 at DAT_00512518 and returns 0. Any other count
// decrements DAT_0051251C and returns 2.
//
// Type 0x10 with a nonzero third argument returns 2. A zero third argument
// returns 0 when DAT_0051251C is 0, and otherwise decrements it and returns 2.
//
// Type 0x11 with a zero third argument increments DAT_0051251C and returns 2.
// A nonzero third argument returns 2.
//
// Type 0x69 stores the dword at node+0xC into DAT_0050E79C and the dword at
// node+8 into DAT_0050E798, then returns 2.
int fn_128bc0(Game& g, const ScriptNode& node, int param_2, int param_3)
{
    switch (node.type)
    {
    case 0:
        return 1;
    case 0xd:
    {
        const std::string key = node_cstr(g.scene_script, node, 8);
        (void)key;
        return 2;
    }
    case 0xe:
    {
        const u32 mode = node_u32(g.scene_script, node, 0x90);
        if (mode == 1)
        {
            // 0x00123B00 is not translated, so its nonzero return is not taken.
        }
        else if (mode == 2)
        {
            // 0x000532B0 is not translated, so the cursor is not advanced.
            return 2;
        }
        if (node_u32(g.scene_script, node, 0x94) != 0)
        {
            // 0x00049D10 is not translated, so its nonzero return is not taken.
        }
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        const u32 at_c = node_u32(g.scene_script, node, 0xc);
        (void)at_c;
        script_set_name(g, node_cstr(g.scene_script, node, 0x10));
        if (param_3 != 0 && at_8 == 0)
        {
            return 0;
        }
        if (param_2 == 0 && param_3 == 0)
        {
            return 0;
        }
        return 2;
    }
    case 0xf:
        g_gate_2518 = 0;
        if (g_gate_2514 != 0)
        {
            if (g_gate_251c == 0)
            {
                g_gate_2518 = 1;
                return 0;
            }
            g_gate_251c = g_gate_251c - 1;
            return 2;
        }
        return 2;
    case 0x10:
        if (param_3 == 0)
        {
            if (g_gate_251c == 0)
            {
                return 0;
            }
            g_gate_251c = g_gate_251c - 1;
            return 2;
        }
        return 2;
    case 0x11:
        if (param_3 == 0)
        {
            g_gate_251c = g_gate_251c + 1;
            return 2;
        }
        return 2;
    case 0x69:
        g_script_e79c = node_u32(g.scene_script, node, 0xc);
        g_script_e798 = node_u32(g.scene_script, node, 8);
        return 2;
    default:
        return 3;
    }
}

// Xbox 0x00128A00. Walks fired file offsets. Handler 0 is 0x00128BC0.
// Handler 1 is 0x00124CC0.
// Handler 2 is 0x00124E50. Handler 3 is 0x00124F30. Handler 4 is 0x00124FC0.
// Handler 5 is 0x001251E0. Handler 6 is 0x00125450. Handler 7 is 0x001259B0.
// Handler 8 is 0x00126140. Handler 9 is 0x001269D0. Handler 10 is 0x00126A10.
// Handler 11 is 0x00126E10. Handler 12 is 0x00127340. Handler 13 is 0x001274B0.
// Handler 14 is 0x00127BC0. Handler 15 is 0x00128050. Handler 16 is 0x00128770.
// Handler 0 is the first slot of the table at 0x00312650. The flag passed to handler 4 is nonzero
// when DAT_0050E4DC has a node whose dword at +8 is 0x7D. Handler 0 receives that same flag.
// Handler 7 receives that same flag.
// Its third argument is 0: 0x00128A00 clears EDI whenever that flag is set,
// and the handler only reads the third argument on that path. Handler 0
// receives that same third argument. Handler 8
// receives that same third argument. Handler 13 receives it as well. Handler 14
// receives that same second argument and that same third argument. Handler 15
// receives that same second argument and that same third argument. Handler 16
// reads only the node. A node type none of them covers stops
// the walk and leaves the
// fired id in place. Return 1 stops on a type-0 node from handler 0, and on a
// type-0x3C node. Return 0 means
// the cursor left the blob, or stopped on an untranslated node, and the
// drain keeps that fired id.
int fn_128a00(Game& g, u32 offset)
{
    int param_2 = 0;
    for (LiveScript* live = g_live_scripts; live != nullptr; live = live->next)
    {
        if (live->type == 0x7d)
        {
            param_2 = 1;
            break;
        }
    }
    for (int step = 0; step < 256; ++step)
    {
        const ScriptNode* node = script_node_at(g.scene_script, offset);
        if (node == nullptr || node->size == 0)
        {
            return 0;
        }
        int handler = fn_128bc0(g, *node, param_2, 0);
        if (handler == 3)
        {
            handler = fn_124cc0(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_124e50(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_124f30(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_124fc0(g, *node, param_2);
        }
        if (handler == 3)
        {
            handler = fn_1251e0(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_125450(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_1259b0(g, *node, param_2, 0);
        }
        if (handler == 3)
        {
            handler = fn_126140(g, *node, param_2, 0);
        }
        if (handler == 3)
        {
            handler = fn_1269d0(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_126a10(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_126e10(g, *node, param_2);
        }
        if (handler == 3)
        {
            handler = fn_127340(g, *node);
        }
        if (handler == 3)
        {
            handler = fn_1274b0(g, *node, 0);
        }
        if (handler == 3)
        {
            handler = fn_127bc0(g, *node, param_2, 0);
        }
        if (handler == 3)
        {
            handler = fn_128050(g, *node, param_2, 0);
        }
        if (handler == 3)
        {
            handler = fn_128770(g, *node);
        }
        if (handler == 1)
        {
            return 1;
        }
        if (handler != 2)
        {
            return 0;
        }
        offset = node->link;
    }
    return 0;
}

// Xbox 0x001288C0. Walks the fired ids from the back. A zero from 0x00128A00
// stops and leaves the remaining entries in place.
void fn_1288c0(Game& g)
{
    for (size_t i = g_script_fired.size(); i > 0;)
    {
        --i;
        const int ran = fn_128a00(g, g_script_fired[i]);
        if (ran == 0)
        {
            return;
        }
        g_script_fired.erase(g_script_fired.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

// Xbox 0x0004B440. The loaded blob base is pushed twice on retail; one file
// offset is the same cursor. 0x001288C0 then walks it. 0x00128A00 does not
// copy an aid_script into the 0x0011EB30 request buffer.
void script_vm_bind(Game& g)
{
    script_vm_reset();
    if (g.scene_script.nodes.empty())
    {
        return;
    }
    g_script_fired.push_back(g.scene_script.nodes.front().offset);
    fn_1288c0(g);
}

// Xbox 0x00117100, then the eval at 0x00117140. 0x00133070 is not translated,
// so a timeout record is not stepped. DAT_0050E794 is left clear so the eval
// pass runs. Opcodes other than 0 are not evaluated.
void script_vm_tick(Game& g, float dt)
{
    (void)dt;
    fn_124be0(g);
    fn_1288c0(g);
}

void load_scene_contents(Game& g, std::string_view name)
{
    script_set_default_name(g);
    g.slot = 0;
    g.slot_up = false;
    g.slot_down = false;
    g.slot_back = false;
    if (name.empty())
    {
        return;
    }
    script_set_name(g, name);
    if (const auto rel = aid_to_bundle_rel(g.scene_name))
    {
        const auto path = g.aids.bundle_path(*rel);
        if (std::filesystem::exists(path))
        {
            g.scene_bundle = &g.aids.load_bundle_rel(*rel);
            std::cout << "loaded " << *rel << " entries " << g.scene_bundle->entries.size() << "\n";
            auto scripts = entries_of_type(*g.scene_bundle, ResourceType::Script);
            if (scripts.empty())
            {
                std::cout << kNoScript << "\n";
            }
            for (const auto* e : scripts)
            {
                g.scene_script = parse_scene_script(e->cpu, e->name);
                std::cout << "  script " << e->name << " bytes " << e->cpu.size()
                          << " root=" << g.scene_script.root_offset << " aids " << g.scene_script.aids.size()
                          << " nodes " << g.scene_script.nodes.size() << "\n";
                log_script_nodes(g.scene_script);
                log_script_refs(g, g.scene_script);
                script_vm_bind(g);
            }
            for (const auto* e : entries_of_type(*g.scene_bundle, ResourceType::AidList))
            {
                bool already = false;
                for (const auto& n : g.script_aidlists)
                {
                    if (n == e->name)
                    {
                        already = true;
                        break;
                    }
                }
                if (already)
                {
                    continue;
                }
                auto names = parse_aidlist(e->cpu);
                g.script_aidlists.push_back(e->name);
                std::cout << "  aidlist " << e->name << " (" << names.size() << ")\n";
                for (size_t i = 0; i < names.size() && i < 12; ++i)
                {
                    std::cout << "    [" << i << "] " << names[i] << "\n";
                }
                if (names.size() > 12)
                {
                    std::cout << "    ...\n";
                }
            }
            load_cutscene_reel(g);
            load_scene_models(g);
            load_ui(g);
        }
        else
        {
            std::cout << "scene bundle missing: " << path.string() << "\n";
        }
    }
    begin_frontend(g);
}

void script_set_default_name(Game& g)
{
    // Xbox 0x001287F0: copy default into DAT_00512494 and zero script globals.
    script_vm_reset();
    g.scene_name = kNoScript;
    g.scene_script = {};
    g.script_aidlists.clear();
    g.cutscenes = {};
    release_models(g);
    release_props(g);
    g.models.clear();
    g.cutscene_props.clear();
    g.cutscene_props_index = static_cast<size_t>(-1);
    g.ui.clear();
}

void script_set_name(Game& g, std::string_view name)
{
    // Xbox 0x00128AB0: EAX = C string, copies into DAT_00512494.
    // The condition list belongs to the script that was bound, so a new name drops it.
    script_vm_reset();
    g.scene_name = std::string(name);
}

void cutscene_queue_start(Game& g)
{
    auto& q = g.cutscenes;
    if (q.items.empty())
    {
        return;
    }
    q.index = 0;
    q.time = 0.f;
    q.playing = true;
    log_cutscene_begin(g);
}

void scene_order_go(Game& g, size_t index)
{
    // Xbox 0x0012C2D0 returns the sceneorder slot. 0x0012C5D0 loads that script.
    if (index >= g.scene_order.size())
    {
        return;
    }
    g.scene_index = index;
    std::cout << "scene order [" << index << "] " << g.scene_order[index] << "\n";
    load_scene_contents(g, g.scene_order[index]);
    cutscene_queue_start(g);
}

void script_load(Game& g, std::string_view name)
{
    // Xbox 0x0012C5D0. 0x0012C2D0 looks the requested aid up in the directory
    // at 0x0042AC80 (0x00116220, stride 0x90) and loads that scene. A name that
    // is not on the boot list is still a script: the directory is every aid_script.
    if (name.empty())
    {
        return;
    }
    for (size_t i = 0; i < g.scene_order.size(); ++i)
    {
        if (g.scene_order[i] == name)
        {
            scene_order_go(g, i);
            return;
        }
    }
    std::cout << "script " << std::string(name) << "\n";
    load_scene_contents(g, name);
    cutscene_queue_start(g);
}

void script_request(Game& g, std::string_view name)
{
    // Xbox 0x0011EB30. EAX is the source string. It is copied to scene+0x80
    // (DAT_00510ED8) and the mode argument is stored at DAT_00510F58.
    // Loading waits for 0x0011E4C0.
    if (std::memcmp(g.requested_script, "aid_", 4) == 0)
    {
        g.requested_script[0] = '\0';
    }
    size_t i = 0;
    while (i + 1 < sizeof(g.requested_script) && i < name.size())
    {
        g.requested_script[i] = name[i];
        ++i;
    }
    g.requested_script[i] = '\0';
    // The mode argument is stored at DAT_00510F58. Nothing in the binary reads it.
}

void script_pump(Game& g)
{
    // Xbox 0x0011E4C0 is not translated. frame_tick calls it with EAX = DAT_00510E58.
    // The aid_ name at scene+0x80 stays queued. This does not load the script
    // and does not mark the stream finished.
    (void)g;
}

void scene_order_advance(Game& g)
{
    // Xbox 0x0012C5D0 via 0x0008F5F6. The finished script is followed by the next
    // sceneorder name, loaded by the same path as a requested script.
    const size_t next = g.scene_index + 1;
    if (next >= g.scene_order.size())
    {
        return;
    }
    script_load(g, g.scene_order[next]);
}

void cutscene_queue_tick(Game& g, float dt)
{
    // camera_apply 0x00025AE0: 0x0001EEC0 when camera+0xfc != 0, then 0x0001EC20
    // when camera+0x154 > 0, then 0x00025190 (host+0x1f8 anchors through
    // 0x0011BE40), then 0x0001FB40, then 0x00027960, then 0x0001DD00,
    // then 0x00025130.
    if (g_camera_channels.gate != 0)
    {
        fn_1eec0(g_camera_channels);
    }
    if (!g_camera_channels.items.empty())
    {
        fn_1ec20(g_camera_channels);
    }
    for (CamAnchor* node = g_cam_focus.anchors; node != nullptr; node = node->next)
    {
        fn_11be40(*node);
    }
    fn_1fb40(g_camera_pulse, dt);
    fn_27960(g_camera_timers, g_camera_pulse, dt);
    fn_1dd00(g_camera_wash, dt);
    fn_25130(g_camera_pairs);
    focus_bounds_tick();
    script_action_opcode_f();
    auto& q = g.cutscenes;
    if (!q.playing || q.items.empty())
    {
        return;
    }
    q.time += dt;
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
                q.time = 0.f;
                if (g.scene_name.find("attract") != std::string::npos)
                {
                    scene_order_advance(g);
                }
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
    if (g.camera && g.camera->actor)
    {
        Vec3 pos;
        Quat orient;
        float fov = 0.9f;
        if (sample_cutscene_camera(q.items[q.index], q.time, pos, orient, fov))
        {
            g.camera->actor->position = pos;
            g.camera->actor->quat = orient;
            g.camera->actor->fov = fov;
        }
    }
}

void draw_cutscene_props(Game& g)
{
    sync_cutscene_props(g);
    const auto& q = g.cutscenes;
    if (!q.playing || q.index >= q.items.size())
    {
        return;
    }
    const Cutscene& cut = q.items[q.index];
    for (const auto& prop : g.cutscene_props)
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
        if (!actor.mina.empty() && !prop.mesh.joints.empty() && !prop.mesh.bones.empty() && cut.duration > 0.f)
        {
            const float norm = q.time / cut.duration;
            const auto pose = sample_mina(actor.mina, norm);
            const int n = int(std::min(pose.size(), std::min(prop.mesh.joints.size(), size_t(127))));
            float bones[128 * 16]{};
            if (n > 0)
            {
                // Xbox 0x0002FAC0. The slot-to-joint bind is ndMtxArray (0x155E60),
                // applied when the mesh uploads the palette.
                skin_matrices(prop.mesh.joints, pose, bones, n);
                gfx_set_bones(bones, n);
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
        gfx_draw_mesh(prop.mesh, origin, scale);
    }
    gfx_set_bones(nullptr, 0);
}

namespace
{

// Transform programs from the D3D section. Each instruction is four dwords.
// 0x0017FC28 is used while DAT_00181CE4 is 0. 0x0017FB78 is used when that
// global is set and device flag bit 1 is set. 0x0017FAB8 is the other set.
constexpr u32 k_d3d_xform_mode0[] = {
    0x00000000u, 0x0020001Bu, 0x0836106Cu, 0x2F100FF8u, 0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u,
    0x00000000u, 0x002008FFu, 0x0836106Cu, 0x2070F828u, 0x00000000u, 0x0240081Bu, 0x1436186Cu, 0x2F20F824u,
    0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u, 0x00000000u, 0x00200200u, 0x0836106Cu, 0x2070F830u,
    0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u, 0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u,
    0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u, 0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u,
    0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u, 0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};
constexpr u32 k_d3d_xform_flag[] = {
    0x00000000u, 0x022000AAu, 0x0836086Cu, 0x201FF828u, 0x00000000u, 0x0020081Bu, 0x0836106Cu, 0x2070F820u,
    0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u, 0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u,
    0x00000000u, 0x0240021Bu, 0x14361800u, 0x2F20F834u, 0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u,
    0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u, 0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u,
    0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u, 0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u,
    0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};
constexpr u32 k_d3d_xform_main[] = {
    0x00000000u, 0x0020001Bu, 0x0836106Cu, 0x2F100FF8u, 0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u,
    0x00000000u, 0x0400001Bu, 0x083613FCu, 0x2070F82Cu, 0x00000000u, 0x0240081Bu, 0x1436186Cu, 0x2F20F824u,
    0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u, 0x00000000u, 0x00200200u, 0x0836106Cu, 0x2070F830u,
    0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u, 0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u,
    0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u, 0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u,
    0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u, 0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};

// DAT_00181E08. BeginStateBig reads this device, not the fastcall argument.
// Vertex-shader constant file is 192 registers. Program cache is 136 slots.
constexpr u32 k_d3d_vs_const_regs = 192u;
constexpr u32 k_d3d_vs_program_slots = 136u;

struct D3dScreenDevice
{
    u32* cursor = nullptr; // +0
    u32* fence = nullptr;  // +4
    u32 flags = 0;         // +8
    u8* active = nullptr;  // +0x790. Byte +4 bit 1 allows the upload.
    u8* depth = nullptr;   // +0x19f8. Byte +0xD is the surface D3DFORMAT.
    float proj_clip = 0.f; // +0x938. Projection factor for sample mode 2.
    float span_x = 0.f;    // +0x940
    float span_y = 0.f;    // +0x944
    float scale_x = 0.f;   // +0x958
    float scale_y = 0.f;   // +0x95c
    i32 view_x = 0;        // +0xed0
    i32 view_y = 0;        // +0xed4
    i32 view_w = 0;        // +0xed8
    i32 view_h = 0;        // +0xedc
    float min_z = 0.f;     // +0xee0
    float max_z = 0.f;     // +0xee4
    float origin_x = 0.f;  // +0xee8
    float origin_y = 0.f;  // +0xeec
    float clip_min_screen = 0.f; // +0x1bf8
    float clip_max_screen = 0.f; // +0x1bfc
    float clip_min_view = 0.f;   // +0x1c00
    float clip_max_view = 0.f;   // +0x1c04
    u8 clip_lock = 0;            // +0x1c08. Bit 0 keeps the screen pair, bit 1 the viewport pair.
    // +0x1098. 136 instruction slots, four dwords each. The next device
    // dword is +0x1918. 0x00172EE0 writes here; 0x00176080 reads the head.
    u32 program[k_d3d_vs_program_slots * 4u]{};
    std::vector<u32> push;
    u32 cursor_index = 0;
    u32 fence_index = 0;
};

D3dScreenDevice* g_d3d_device = nullptr;
// DAT_00180d50. Four dwords per register. 192 * 16 ends at 0x00181950.
u32 g_d3d_vs_constants[k_d3d_vs_const_regs * 4u]{};
i32 g_d3d_program_mode = 0;   // DAT_00181CE4
i32 g_d3d_sample_mode = 0;    // DAT_00181DAC
i32 g_d3d_multisample_aa = 0; // DAT_00181DD0, D3DRS_MULTISAMPLEANTIALIAS
i32 g_d3d_yuv_enable = 0;     // DAT_00181DF0, D3DRS_YUVENABLE

void d3d_bind_cursor(D3dScreenDevice& device)
{
    if (device.push.empty())
    {
        device.cursor = nullptr;
        device.fence = nullptr;
        return;
    }
    device.cursor = device.push.data() + device.cursor_index;
    device.fence = device.push.data() + device.fence_index;
}

u32* d3d_begin_state_big(u32 count)
{
    // Slice of 0x00177710 used by 0x00173900. The cursor must sit `count`
    // dwords before the fence plus 0x200 bytes. A miss grows through
    // 0x001775C0. DAT_001845AC starts at 0, so the grown size is
    // count*4+0x204. A null global device has nothing to write into.
    D3dScreenDevice* device = g_d3d_device;
    if (device == nullptr)
    {
        return nullptr;
    }
    const u32 end_byte = device->cursor_index * 4u + count * 4u;
    const u32 fence_limit = device->fence_index * 4u + 0x200u;
    if (end_byte >= fence_limit)
    {
        u32 bytes = count * 4u + 0x204u;
        const u32 have = device->fence_index * 4u;
        if (bytes < have)
        {
            bytes = have;
        }
        device->fence_index = (bytes + 3u) / 4u;
    }
    const u32 need = device->cursor_index + count;
    if (device->push.size() < need || device->push.size() < device->fence_index)
    {
        device->push.resize(std::max(need, device->fence_index));
    }
    d3d_bind_cursor(*device);
    return device->cursor;
}

float d3d_u32_float(i32 bits)
{
    return static_cast<float>(static_cast<u32>(bits));
}

u32 d3d_float_bits(float value)
{
    return std::bit_cast<u32>(value);
}

void d3d_bias_multisample(float& x, float& y, u32 flags)
{
    if ((flags & 0x8000u) != 0 && g_d3d_multisample_aa != 0)
    {
        x -= 0.5f;
        y -= 0.5f;
    }
}

void fn_1710e0(float* offset, float* scale)
{
    // Xbox 0x001710E0
    // stdcall (ret 8). Only caller is 0x00171C20. Ghidra names the call
    // D3DDevice_GetViewportOffsetAndScale; the body reads DAT_00181E08,
    // not the fastcall device. A null global would fault on Xbox.
    //
    // Corner is view_x/view_y converted as unsigned pixels, times the
    // multisample scales, plus the viewport origin. Flag bit 0x8000 and
    // a nonzero D3DRS_MULTISAMPLEANTIALIAS subtract 0.5 from both axes.
    // Width and height become half extents (height is negated). Offset is
    // the biased corner plus those extents, with z = min_z * span_y.
    // Scale is (half width, negated half height, (max_z-min_z)*span_y, 0).
    if (offset == nullptr || scale == nullptr)
    {
        return;
    }
    if (g_d3d_device == nullptr)
    {
        offset[0] = offset[1] = offset[2] = offset[3] = 0.f;
        scale[0] = scale[1] = scale[2] = scale[3] = 0.f;
        return;
    }
    const D3dScreenDevice& device = *g_d3d_device;
    float x = d3d_u32_float(device.view_x) * device.scale_x + device.origin_x;
    float y = d3d_u32_float(device.view_y) * device.scale_y + device.origin_y;
    d3d_bias_multisample(x, y, device.flags);
    const float half_w = d3d_u32_float(device.view_w) * device.scale_x * 0.5f;
    const float half_h = d3d_u32_float(device.view_h) * device.scale_y * -0.5f;
    offset[0] = x + half_w;
    offset[1] = y - half_h;
    offset[2] = device.min_z * device.span_y;
    offset[3] = 0.f;
    scale[0] = half_w;
    scale[1] = half_h;
    scale[2] = (device.max_z - device.min_z) * device.span_y;
    scale[3] = 0.f;
}

void d3d_store_sample_clip(float& clip_min, float& clip_max, const D3dScreenDevice& device)
{
    clip_max = device.span_y;
    clip_min = (device.span_x * device.proj_clip) * device.span_y;
}

u32* fn_171c20(D3dScreenDevice* device, u32* packet)
{
    // Xbox 0x00171C20
    // Fastcall. Writes the viewport push-buffer words and returns the
    // cursor after them. Callers pass the device in ecx and the cursor
    // in edx, then store eax back at device+0. A null active record
    // would fault on Xbox; the port leaves the cursor alone.
    //
    // Byte +4 of the record at +0x790 selects the packet. Bits 0x12 clear
    // emits SET_VIEWPORT_OFFSET (0x100A20, four floats) of the unsigned
    // pixel corner (view_x, view_y) times scale plus origin, z and w zero,
    // with the same 0.5 bias as 0x001710E0. Either bit set calls 0x001710E0
    // on DAT_00181E08 and, unless flag bit 0x200 is set, emits that offset
    // plus SET_VIEWPORT_SCALE (0x100AF0, four floats).
    //
    // Both paths then emit SET_CLIP_MIN/SET_CLIP_MAX (0x00080394, two
    // floats). Bit 0 of +0x1c08 keeps the screen pair at +0x1bf8/+0x1bfc.
    // Bit 1 keeps the viewport pair at +0x1c00/+0x1c04. Otherwise sample
    // mode 2 stores (span_x * proj_clip * span_y, span_y). The screen path
    // otherwise stores (min_z * span_y, max_z * span_y). The viewport path
    // otherwise stores (0, span_y).
    if (device == nullptr || packet == nullptr || device->active == nullptr)
    {
        return packet;
    }

    float clip_min = 0.f;
    float clip_max = 0.f;
    if ((device->active[4] & 0x12) == 0)
    {
        float x = d3d_u32_float(device->view_x) * device->scale_x + device->origin_x;
        float y = d3d_u32_float(device->view_y) * device->scale_y + device->origin_y;
        d3d_bias_multisample(x, y, device->flags);
        packet[0] = 0x100a20u;
        packet[1] = d3d_float_bits(x);
        packet[2] = d3d_float_bits(y);
        packet[3] = 0;
        packet[4] = 0;
        packet += 5;
        if ((device->clip_lock & 1u) == 0)
        {
            if (g_d3d_sample_mode == 2)
            {
                d3d_store_sample_clip(device->clip_min_screen, device->clip_max_screen, *device);
            }
            else
            {
                device->clip_min_screen = device->min_z * device->span_y;
                device->clip_max_screen = device->max_z * device->span_y;
            }
        }
        clip_min = device->clip_min_screen;
        clip_max = device->clip_max_screen;
    }
    else
    {
        float offset[4]{};
        float scale[4]{};
        fn_1710e0(offset, scale);
        if ((device->flags & 0x200u) == 0)
        {
            packet[0] = 0x100a20u;
            packet[1] = d3d_float_bits(offset[0]);
            packet[2] = d3d_float_bits(offset[1]);
            packet[3] = d3d_float_bits(offset[2]);
            packet[4] = d3d_float_bits(offset[3]);
            packet[5] = 0x100af0u;
            packet[6] = d3d_float_bits(scale[0]);
            packet[7] = d3d_float_bits(scale[1]);
            packet[8] = d3d_float_bits(scale[2]);
            packet[9] = d3d_float_bits(scale[3]);
            packet += 10;
        }
        if ((device->clip_lock & 2u) == 0)
        {
            if (g_d3d_sample_mode == 2)
            {
                d3d_store_sample_clip(device->clip_min_view, device->clip_max_view, *device);
            }
            else
            {
                device->clip_min_view = 0.f;
                device->clip_max_view = device->span_y;
            }
        }
        clip_min = device->clip_min_view;
        clip_max = device->clip_max_view;
    }

    packet[0] = 0x00080394u;
    packet[1] = d3d_float_bits(clip_min);
    packet[2] = d3d_float_bits(clip_max);
    return packet + 3;
}

u32* fn_171e10(D3dScreenDevice* device, u32* packet)
{
    // Xbox 0x00171E10
    // Fastcall. Writes one NV097_SET_CONTROL0 word (0x40290: method 0x290,
    // count 1) and returns the cursor after it. Callers pass the device in
    // ecx and the cursor in edx, then store eax at device+0. A null device
    // or cursor would fault on Xbox; the port leaves the cursor alone.
    //
    // The data dword starts at 0x100001. Nonzero D3DRS_YUVENABLE
    // (DAT_00181DF0) replaces that with 0x10100001 (bit 24). Sample mode 2
    // (DAT_00181DAC) sets bit 0x10000. A depth surface at +0x19f8 whose
    // format byte +0xD is a float Z format sets bit 0x1000: 0x2B F24S8,
    // 0x2D F16, 0x2F LIN_F24S8, 0x31 LIN_F16.
    //
    // The sample-mode setter at 0x001722A0 calls this when mode 2 is entered
    // or left, then 0x00171C20. The YUV setter at 0x00172430 stores its
    // argument at DAT_00181DF0 first. Render-target setup at 0x00174335
    // calls it with the device in ecx.
    if (device == nullptr || packet == nullptr)
    {
        return packet;
    }

    u32 control = 0x100001u;
    if (g_d3d_yuv_enable != 0)
    {
        control = 0x10100001u;
    }
    if (g_d3d_sample_mode == 2)
    {
        control |= 0x10000u;
    }
    if (device->depth != nullptr)
    {
        const u8 format = device->depth[0x0d];
        if (format == 0x2bu || format == 0x2du || format == 0x2fu || format == 0x31u)
        {
            control |= 0x1000u;
        }
    }
    packet[0] = 0x40290u;
    packet[1] = control;
    return packet + 2;
}

void fn_172720(u32* dest, const u32* src, int count)
{
    // Xbox 0x00172720
    // stdcall (ret 0xC). Copies `count` dwords from src to dest, low to high.
    // 0x001727B0 is the only caller. It takes this path when the push-buffer
    // run is at least 0x80 dwords, the dword at device+0xc44 is 1, and the
    // offset formed from device+0xbc0 is 0. Shorter runs there use rep movsd.
    //
    // The prefix length is ((-dest) & 0x1F) >> 2, the dwords until dest is
    // 32-byte aligned. A count greater than that prefix is a rep movsd of
    // the prefix, then 16-dword chunks, then a rep movsd tail. Each chunk
    // is movq into all eight MMX registers and movntq back out, with
    // prefetchnta at src+0x40 and src+0x60, so the chunk is read before it
    // is written. A count that does not exceed the prefix is one rep movsd.
    // EMMS runs before the return.
    if (count <= 0)
    {
        return;
    }

    const unsigned raw = static_cast<unsigned>(-static_cast<int>(reinterpret_cast<std::uintptr_t>(dest)));
    const int align = static_cast<int>((raw & 0x1Fu) >> 2);
    int left = count;
    if (align < left)
    {
        left -= align;
        for (int n = align; n != 0; --n)
        {
            *dest = *src;
            ++dest;
            ++src;
        }
        while (left >= 0x10)
        {
            u32 chunk[16];
            std::memcpy(chunk, src, sizeof(chunk));
            std::memcpy(dest, chunk, sizeof(chunk));
            dest += 16;
            src += 16;
            left -= 0x10;
        }
    }
    for (; left != 0; --left)
    {
        *dest = *src;
        ++dest;
        ++src;
    }
}

// DAT_0017FCE8. Each texture coordinate's size is a two-bit field starting
// at FVF bit 16: 0 is two floats, 1 is three, 2 is four, 3 is one.
constexpr u32 k_fvf_tex_components[4] = {2u, 3u, 4u, 1u};

// DAT_0017FA34. Bytes in one component, indexed by the low nibble of a
// D3DVSDT code: 0 color, 1 normalized short, 2 float, 4 packed byte,
// 5 short, 6 packed normal. Indices 8..15 are the bytes that follow the
// eight-byte table in the image.
constexpr u8 k_d3d_vsdt_bytes[16] = {
    0x01, 0x02, 0x04, 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x03,
};

// DAT_0017FA3C. Component count, indexed by the high nibble. Nibble 7 is
// FLOAT2H and counts as three floats. Indices 8..15 spill into the 1.0f
// constant at 0x0017FA48.
constexpr u8 k_d3d_vsdt_count[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f,
};

// One vertex-shader input register. type at +0 is the D3DVSDT code.
// source at +4 is 0 for a stream load, 1 for a tessellated normal, and 2
// for a tessellated UV. tess_in at +5 is the tessellator input register.
// stream at +8 and byte_offset at +0xc belong to the next register.
struct D3dFvfElement
{
    u32 type;
    u8 source;
    u8 tess_in;
    u16 unused6;
    u32 stream;
    u32 byte_offset;
};

// Parsed FVF declaration. 0x001739F0 keeps one at 0x00184478.
// element[0] is +0x1c. Sixteen registers are typed; further texture
// coordinates continue the same 0x10 stride. Register 0's stream index
// and vertex byte offset live at +0x14 and +0x18, the same slots each
// element keeps for the following register.
struct D3dFvfDecl
{
    u32 unused0;
    u32 flags; // +4
    u32 unused8;
    u32 unusedc;
    u32 tex_mask; // +0x10
    u32 reg0_stream; // +0x14
    u32 reg0_offset; // +0x18
    D3dFvfElement element[24];
};

static_assert(offsetof(D3dFvfDecl, flags) == 4);
static_assert(offsetof(D3dFvfDecl, tex_mask) == 0x10);
static_assert(offsetof(D3dFvfDecl, reg0_stream) == 0x14);
static_assert(offsetof(D3dFvfDecl, reg0_offset) == 0x18);
static_assert(offsetof(D3dFvfDecl, element) == 0x1c);
static_assert(offsetof(D3dFvfElement, source) == 4);
static_assert(offsetof(D3dFvfElement, tess_in) == 5);
static_assert(offsetof(D3dFvfElement, stream) == 8);
static_assert(offsetof(D3dFvfElement, byte_offset) == 0xc);
static_assert(sizeof(D3dFvfElement) == 0x10);

void fn_172da0(D3dFvfDecl* decl, u32 fvf)
{
    // Xbox 0x00172DA0
    // stdcall (ret 8). Expands an FVF into the declaration registers.
    // 0x001739F0 is the only caller: when the shader argument's low bit
    // is clear it passes the declaration at 0x00184478 and that FVF.
    // A null declaration would fault on Xbox; the port returns.
    //
    // +4 and +0x10 are cleared. The type dword of registers 0..15 is set
    // to 2. Every other dword is left as the caller left it.
    //
    // FVF & 0xE selects the position. 4 is XYZRHW: register 0 becomes
    // 0x42 (FLOAT4), the vertex is 16 bytes, and flag bit 1 is set. Any
    // other nonzero code is XYZ: register 0 becomes 0x32 (FLOAT3) and the
    // vertex is 12 bytes. Codes of 6 and up are XYZBn. The weight count
    // is ((code - 6) >> 1) + 1, stored in register 1 as (count << 4) | 2,
    // with byte offset 12 at register 0's +0xc. The running size becomes
    // count * 4 + 12.
    //
    // Bit 0x10 adds a normal at register 2 (FLOAT3, 12 bytes). Bit 0x40
    // adds diffuse at register 3 (0x40, D3DCOLOR, 4 bytes) and sets flag
    // 0x400. Bit 0x80 adds specular at register 4 (D3DCOLOR, 4 bytes) and
    // sets flag 0x800. Each attribute's vertex offset is the size
    // accumulated before it, written at the previous register's +0xc.
    //
    // Bits 11:8 are the texture-coordinate count. Coordinate i consumes
    // the next two bits from FVF bit 16, maps them through DAT_0017FCE8,
    // and writes register 9+i as (components << 4) | 2. The running size
    // grows by components * 4. +0x10 ORs components << (i * 8). The shift
    // is masked to five bits, matching x86 shl.
    if (decl == nullptr)
    {
        return;
    }

    decl->flags = 0;
    decl->tex_mask = 0;
    for (int reg = 0; reg < 16; ++reg)
    {
        decl->element[reg].type = 2;
    }

    const u32 position = fvf & 0xeu;
    u32 cursor = 0;
    if (position == 4u)
    {
        decl->flags |= 2u;
        decl->element[0].type = 0x42u;
        cursor = 0x10u;
    }
    else if (position != 0u)
    {
        decl->element[0].type = 0x32u;
        cursor = 0xcu;
        if (position >= 6u)
        {
            const u32 weights = ((position - 6u) >> 1) + 1u;
            decl->element[0].byte_offset = 0xcu;
            decl->element[1].type = (weights << 4) | 2u;
            cursor = weights * 4u + 0xcu;
        }
    }

    if ((fvf & 0x10u) != 0u)
    {
        decl->element[1].byte_offset = cursor;
        decl->element[2].type = 0x32u;
        cursor += 0xcu;
    }
    if ((fvf & 0x40u) != 0u)
    {
        decl->element[2].byte_offset = cursor;
        decl->element[3].type = 0x40u;
        decl->flags |= 0x400u;
        cursor += 4u;
    }
    if ((fvf & 0x80u) != 0u)
    {
        decl->element[3].byte_offset = cursor;
        decl->element[4].type = 0x40u;
        decl->flags |= 0x800u;
        cursor += 4u;
    }

    u32 tex_count = (fvf >> 8) & 0xfu;
    u32 tex_fmt = fvf >> 16;
    u32 shift = 0;
    u32 reg = 9;
    while (tex_count != 0u)
    {
        const u32 comps = k_fvf_tex_components[tex_fmt & 3u];
        decl->element[reg].type = (comps << 4) | 2u;
        decl->element[reg - 1u].byte_offset = cursor;
        cursor += comps * 4u;
        decl->tex_mask |= comps << (shift & 31u);
        tex_fmt >>= 2;
        shift += 8u;
        --tex_count;
        ++reg;
    }
}

void fn_172ee0(u32* shader, u32 slot)
{
    // Xbox 0x00172EE0
    // stdcall (ret 8). Shadows a vertex-shader token stream into the
    // constant file and the transform-program cache. 0x001736A0 is the
    // only caller. It passes the shader (its handle minus one) and the
    // program slot, and only when device flag bit 0x10 is clear. A null
    // shader would fault on Xbox; the port returns.
    //
    // Tokens start at shader+0x114. A token is (count << 18) | method,
    // then count payload dwords. The word after each payload is the next
    // token, and a zero word ends the walk. The token under the cursor
    // is consumed even when its count is 0.
    //
    // Count 1 retargets the constant cursor and does not read the method.
    // The cursor becomes DAT_00180d50 + payload[0] * 16. It starts at the
    // shader object. Method 0xB00 (SET_TRANSFORM_PROGRAM) copies its
    // payload to device+0x1098 + slot * 16 and advances that cursor. Any
    // other method copies its payload to the constant cursor and advances
    // it. 0x00176080 later pushes the first eight program dwords back out
    // through 0x001738C0.
    //
    // A null device would fault on a 0xB00 copy; the port skips program
    // copies. A constant index of 192 or more would leave DAT_00180d50,
    // and a slot of 136 or more would leave the program cache; the port
    // keeps the previous constant cursor and drops program words that
    // do not fit.
    if (shader == nullptr)
    {
        return;
    }

    u32* token = shader + 0x45;
    u32* constants = shader;
    bool constants_in_file = false;
    u32* program = nullptr;
    u32 program_left = 0;
    if (g_d3d_device != nullptr && slot < k_d3d_vs_program_slots)
    {
        program = g_d3d_device->program + static_cast<size_t>(slot) * 4u;
        program_left = (k_d3d_vs_program_slots - slot) * 4u;
    }

    for (;;)
    {
        const u32 header = *token;
        const u32 count = header >> 18;
        const u32* data = token + 1;
        if (count == 1u)
        {
            const u32 index = data[0];
            if (index < k_d3d_vs_const_regs)
            {
                constants = g_d3d_vs_constants + static_cast<size_t>(index) * 4u;
                constants_in_file = true;
            }
        }
        else if ((header & 0xffffu) == 0xb00u)
        {
            if (program != nullptr && count != 0u)
            {
                u32 n = count;
                if (n > program_left)
                {
                    n = program_left;
                }
                if (n != 0u)
                {
                    std::memcpy(program, data, static_cast<size_t>(n) * sizeof(u32));
                    program += n;
                    program_left -= n;
                }
                if (n != count)
                {
                    program = nullptr;
                    program_left = 0;
                }
            }
        }
        else if (count != 0u && constants != nullptr)
        {
            u32 n = count;
            if (constants_in_file)
            {
                const u32* file_end = g_d3d_vs_constants + k_d3d_vs_const_regs * 4u;
                const u32 room = static_cast<u32>(file_end - constants);
                if (n > room)
                {
                    n = room;
                }
            }
            if (n != 0u)
            {
                std::memcpy(constants, data, static_cast<size_t>(n) * sizeof(u32));
            }
            if (n == count)
            {
                constants += count;
            }
            else
            {
                constants = nullptr;
                constants_in_file = false;
            }
        }
        token += 1u + count;
        if (*token == 0u)
        {
            break;
        }
    }
}

u32* fn_172f90(u32* dest, const u32* src, u32 count)
{
    // Xbox 0x00172F90
    // stdcall (ret 0xC). Writes `count` dwords from src as
    // SET_TRANSFORM_PROGRAM packets and returns the cursor after them.
    // 0x00173900 is the only caller. It passes the push-buffer cursor
    // after SET_TRANSFORM_PROGRAM_LOAD of slot 0, then stores the end
    // at device+0. A null dest would fault on Xbox; the port returns it.
    //
    // Each packet holds at most 0x20 payload dwords. The header is
    // (n << 18) | 0xB00, then those n dwords. The walk is a do-while, so
    // a zero count still writes one header of 0xB00 and does not read src.
    if (dest == nullptr)
    {
        return dest;
    }

    do
    {
        u32 n = count;
        if (n > 0x1fu)
        {
            n = 0x20u;
        }
        *dest = (n << 18) | 0xB00u;
        if (n != 0u)
        {
            std::memcpy(dest + 1, src, static_cast<size_t>(n) * sizeof(u32));
            src += n;
        }
        count -= n;
        dest += n + 1u;
    }
    while (count != 0u);
    return dest;
}

void fn_173000(D3dFvfDecl* decl, const u32* tokens, int fixed_function)
{
    // Xbox 0x00173000
    // stdcall (ret 0xC). Walks a D3DVSD token stream into the declaration
    // registers. D3DDevice_CreateVertexShader (0x001732A0) calls it with
    // the new shader object, the declaration, and 1 when the shader
    // function pointer is null. A null declaration or token pointer would
    // fault on Xbox; the port returns. A register of 24 or more would
    // write past this declaration; the port skips that store.
    //
    // The top three bits select the token. 0 is NOP. 0x20000000 selects
    // the stream (low nibble) and resets the byte cursor. 0x40000000 with
    // bit 28 set skips: the count is bits 19:16, in dwords unless bit 27
    // is set, when the count is already in bytes. Bit 28 clear loads the
    // register in bits 4:0. Bits 23:16 are the D3DVSDT code, stored as the
    // register type. The stream index and byte cursor are stored in the
    // previous register (register 0 uses +0x14 and +0x18). The source byte
    // becomes 0.
    //
    // 0x60000000 is a tessellator token. The output register's stream and
    // offset are cleared, its type becomes 2, and byte +5 receives the
    // input register in bits 23:20. Bit 28 clear writes source 1 (normal)
    // and bit 28 set writes source 2 (UV).
    //
    // 0x80000000 is a constant-memory token. Bits 28:25 count 4-dword
    // vectors, and those dwords are skipped. Any other kind, including
    // D3DVSD_END (0xFFFFFFFF), returns.
    //
    // When fixed_function is set, registers 9..12 OR a component count
    // into +0x10. The count is the type's high nibble, except 7 becomes 4
    // (FLOAT2H) and type 0x16 becomes 3 (NORMPACKED3). The shift is
    // (register * 8 - 72), masked to five bits the way x86 shl masks its
    // count. Registers 3, 4, 7, and 8 set flag bits 0x400, 0x800, 0x1000,
    // and 0x2000. The cursor then grows by
    // DAT_0017FA3C[type >> 4] * DAT_0017FA34[type & 0xF].
    if (decl == nullptr || tokens == nullptr)
    {
        return;
    }

    u32 stream = 0;
    u32 cursor = 0;
    for (;;)
    {
        const u32 token = *tokens;
        ++tokens;
        const u32 kind = token & 0xe0000000u;
        if (kind > 0x60000000u)
        {
            if (kind != 0x80000000u)
            {
                return;
            }
            const u32 vectors = (token >> 0x19) & 0xfu;
            tokens += static_cast<size_t>(vectors) * 4u;
            continue;
        }
        if (kind == 0x60000000u)
        {
            const u32 reg = token & 0x1fu;
            if (reg < 24u)
            {
                if (reg == 0u)
                {
                    decl->reg0_stream = 0;
                    decl->reg0_offset = 0;
                }
                else
                {
                    decl->element[reg - 1u].stream = 0;
                    decl->element[reg - 1u].byte_offset = 0;
                }
                decl->element[reg].type = 2u;
                decl->element[reg].tess_in = static_cast<u8>((token >> 0x14) & 0xfu);
                decl->element[reg].source = (token & 0x10000000u) == 0u ? 1u : 2u;
            }
            continue;
        }
        if (kind == 0u)
        {
            continue;
        }
        if (kind == 0x20000000u)
        {
            stream = token & 0xfu;
            cursor = 0;
            continue;
        }

        if ((token & 0x10000000u) != 0u)
        {
            u32 skip = (token >> 16) & 0xfu;
            if ((token & 0x08000000u) == 0u)
            {
                skip <<= 2;
            }
            cursor += skip;
            continue;
        }

        const u32 type = (token >> 16) & 0xffu;
        const u32 reg = token & 0x1fu;
        if (reg < 24u)
        {
            if (reg == 0u)
            {
                decl->reg0_stream = stream;
                decl->reg0_offset = cursor;
            }
            else
            {
                decl->element[reg - 1u].stream = stream;
                decl->element[reg - 1u].byte_offset = cursor;
            }
            decl->element[reg].type = type;
            decl->element[reg].source = 0;
        }
        if (fixed_function != 0)
        {
            if (reg >= 9u && reg <= 0xcu)
            {
                u32 comps = (type >> 4) & 0xfu;
                if (comps == 7u)
                {
                    comps = 4u;
                }
                else if (type == 0x16u)
                {
                    comps = 3u;
                }
                const u32 shift = (reg * 8u - 0x48u) & 31u;
                decl->tex_mask |= comps << shift;
            }
            if (reg == 3u)
            {
                decl->flags |= 0x400u;
            }
            else if (reg == 4u)
            {
                decl->flags |= 0x800u;
            }
            else if (reg == 7u)
            {
                decl->flags |= 0x1000u;
            }
            else if (reg == 8u)
            {
                decl->flags |= 0x2000u;
            }
        }
        cursor += static_cast<u32>(k_d3d_vsdt_count[(type >> 4) & 0xfu]) *
                   static_cast<u32>(k_d3d_vsdt_bytes[type & 0xfu]);
    }
}

u32 fn_1731a0(const u32* tokens, u32* dest)
{
    // Xbox 0x001731A0
    // stdcall (ret 8). Turns D3DVSD_CONST tokens into transform-constant
    // packets and returns how many dwords that is. D3DDevice_CreateVertexShader
    // (0x001732A0) calls it twice: first with a null dest, then shifts the
    // count by 2 to size the shader object; later it writes the packets
    // immediately after the program image that 0x00172F90 left at
    // shader+0x114. A null token pointer would fault on Xbox; the port
    // returns 0. A null dest is the size query and still walks the stream.
    //
    // Tokens run until D3DVSD_END (0xFFFFFFFF). A constant-memory token has
    // top bits 0x80000000. Bits 28:25 are the vector count and the low byte
    // is the constant index. Any other token, including NOP, is one dword
    // and does not change the index.
    //
    // The index starts at 0xDEADBEEF, so the first constant always emits
    // SET_TRANSFORM_CONSTANT_LOAD (0x41EA4) plus that index. A later token
    // emits the load only when its index is not the previous token's index
    // plus one. The stored index is the token's own index, not the index
    // after its vectors.
    //
    // Each run is a do-while of at most eight vectors. The header is
    // (n << 20) | 0xB80, SET_TRANSFORM_CONSTANT of n*4 dwords, then those
    // dwords. A count of zero still writes one 0xB80 header and reads no
    // payload. The dword count includes every header and payload dword.
    if (tokens == nullptr)
    {
        return 0;
    }

    u32 token = *tokens;
    ++tokens;
    u32 written = 0;
    u32 prev = 0xdeadbeefu;
    while (token != 0xffffffffu)
    {
        if ((token & 0xe0000000u) == 0x80000000u)
        {
            u32 count = (token >> 0x19) & 0xfu;
            const u32 address = token & 0xffu;
            if (address != prev + 1u)
            {
                if (dest != nullptr)
                {
                    dest[0] = 0x41ea4u;
                    dest[1] = address;
                    dest += 2;
                }
                written += 2u;
            }
            prev = address;
            do
            {
                u32 chunk = count;
                if (chunk >= 8u)
                {
                    chunk = 8u;
                }
                const u32 dwords = chunk * 4u;
                if (dest != nullptr)
                {
                    dest[0] = (chunk << 20) | 0xb80u;
                    if (dwords != 0u)
                    {
                        std::memcpy(dest + 1, tokens, static_cast<size_t>(dwords) * sizeof(u32));
                    }
                    dest += 1u + dwords;
                }
                tokens += dwords;
                written += dwords + 1u;
                count -= chunk;
            }
            while (count != 0u);
        }
        token = *tokens;
        ++tokens;
    }
    return written;
}

void fn_173900(D3dScreenDevice* device)
{
    // Xbox 0x00173900
    // Fastcall. Reloads the screen-space transform program and constants 0
    // and 1. D3D_CommonSetMultiSampleModeAndScale (0x00171F40) calls it
    // after writing the multisample scales, the sample-mode setter at
    // 0x001722A0 calls it when mode 2 is entered or left, and the viewport
    // origin store at 0x00175060 calls it after writing +0xee8 / +0xeec.
    //
    // Byte +4 bit 1 on the record at +0x790 must be set. A null record
    // would fault on Xbox; the port returns.
    //
    // DAT_00181CE4 == 0 selects the 48-dword program at 0x0017FC28.
    // Otherwise flag bit 1 selects the 44-dword program at 0x0017FB78,
    // and a clear bit selects the 48-dword program at 0x0017FAB8.
    //
    // D3DDevice_BeginStateBig reserves program_dwords+0x1E. The packet is
    // SET_TRANSFORM_CONSTANT_LOAD (0x41EA4) of index 0, SET_TRANSFORM_CONSTANT
    // (0x200B80) of eight dwords, SET_TRANSFORM_PROGRAM_LOAD (0x41E9C) of
    // slot 0, then 0x00172F90 which prefixes each run of at most 0x20
    // program dwords with (count<<18)|0xB00 (SET_TRANSFORM_PROGRAM).
    // The cursor stored at device+0 becomes the end of that copy.
    //
    // Constant 0 is scale_x, scale_y, span_y, and either span_y*span_x
    // when DAT_00181DAC is 2 or 1.0. Constant 1 is the viewport origin
    // with a 0.5 bias subtracted when flag bit 0x8000 is set and
    // D3DRS_MULTISAMPLEANTIALIAS is nonzero, then two zeros.
    if (device == nullptr || device->active == nullptr || (device->active[4] & 2u) == 0)
    {
        return;
    }

    const u32* program = k_d3d_xform_mode0;
    u32 program_words = static_cast<u32>(sizeof(k_d3d_xform_mode0) / sizeof(k_d3d_xform_mode0[0]));
    if (g_d3d_program_mode != 0)
    {
        if ((device->flags & 2u) != 0)
        {
            program = k_d3d_xform_flag;
            program_words = static_cast<u32>(sizeof(k_d3d_xform_flag) / sizeof(k_d3d_xform_flag[0]));
        }
        else
        {
            program = k_d3d_xform_main;
            program_words = static_cast<u32>(sizeof(k_d3d_xform_main) / sizeof(k_d3d_xform_main[0]));
        }
    }

    u32* packet = d3d_begin_state_big(program_words + 0x1eu);
    if (packet == nullptr)
    {
        return;
    }

    float scale = 1.f;
    if (g_d3d_sample_mode == 2)
    {
        scale = device->span_y * device->span_x;
    }
    float bias = 0.f;
    if ((device->flags & 0x8000u) != 0 && g_d3d_multisample_aa != 0)
    {
        bias = 0.5f;
    }

    packet[0] = 0x41ea4u;
    packet[1] = 0;
    packet[2] = 0x200b80u;
    packet[3] = std::bit_cast<u32>(device->scale_x);
    packet[4] = std::bit_cast<u32>(device->scale_y);
    packet[5] = std::bit_cast<u32>(device->span_y);
    packet[6] = std::bit_cast<u32>(scale);
    packet[7] = std::bit_cast<u32>(device->origin_x - bias);
    packet[8] = std::bit_cast<u32>(device->origin_y - bias);
    packet[9] = 0;
    packet[10] = 0;
    packet[11] = 0x41e9cu;
    packet[12] = 0;

    u32* dest = fn_172f90(packet + 0x0d, program, program_words);

    if (g_d3d_device != nullptr)
    {
        g_d3d_device->cursor_index = static_cast<u32>(dest - g_d3d_device->push.data());
        d3d_bind_cursor(*g_d3d_device);
        device->cursor = g_d3d_device->cursor;
    }
}

void fn_176f10(u32* dest, u32 header, const u32* matrix)
{
    // Xbox 0x00176F10
    // stdcall (ret 0xC). Writes a 17-dword push-buffer packet. The first
    // dword is the method header. The next sixteen are the row-major 4x4
    // transposed, one source column per output row. Callers advance the
    // cursor by 0x44 themselves. A null dest or matrix would fault on Xbox.
    //
    // The header is (4 << 20) | method, sixteen floats of one matrix.
    // 0x001737A0 stores the constant 4x4 at 0x0017FA48 under 0x400840,
    // 0x400880, 0x4008C0, and 0x400900. 0x00174BC0 stores its two matrix
    // arguments under 0x400480 and 0x400680. The constant upload at
    // 0x001797A0 stores (device+0xdd0) * (device+0xc50) under 0x400480.
    // When DAT_00181D94 is zero, 0x400680 receives that product times the
    // matrix at device+0x970; otherwise it receives device+0x970. The
    // same upload then stores (device+0xe10), (device+0xe50), and
    // (device+0xe90), each times (device+0xc50), under 0x4004C0, 0x400500,
    // and 0x400540.
    dest[0] = header;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            dest[1 + col * 4 + row] = matrix[row * 4 + col];
        }
    }
}

void fn_177af0(float* out, const float* left, const float* right)
{
    // Xbox 0x00177AF0
    // stdcall (ret 0xC). SSE 4x4 product, out = left * right, row-major.
    // Output row r is left[r][0]*right.row0 + left[r][1]*right.row1
    // + left[r][2]*right.row2 + left[r][3]*right.row3.
    // D3D_UpdateProjectionViewportTransform (0x00171210) stores
    // (device+0xc90) * viewport at device+0x970. The constant upload at
    // 0x001797A0 calls the same product. Both matrices are read before
    // the first store, so out may alias left or right.
    float a[16];
    float b[16];
    std::memcpy(a, left, sizeof(a));
    std::memcpy(b, right, sizeof(b));
    for (int row = 0; row < 4; ++row)
    {
        const float x = a[row * 4 + 0];
        const float y = a[row * 4 + 1];
        const float z = a[row * 4 + 2];
        const float w = a[row * 4 + 3];
        out[row * 4 + 0] = x * b[0] + y * b[4] + z * b[8] + w * b[12];
        out[row * 4 + 1] = x * b[1] + y * b[5] + z * b[9] + w * b[13];
        out[row * 4 + 2] = x * b[2] + y * b[6] + z * b[10] + w * b[14];
        out[row * 4 + 3] = x * b[3] + y * b[7] + z * b[11] + w * b[15];
    }
}

} // namespace

} // namespace gbtg
