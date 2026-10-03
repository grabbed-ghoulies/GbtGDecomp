#include "gbtg/anim.hpp"
#include "gbtg/engine_internal.hpp"
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
}
#endif

namespace gbtg
{
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
    g.cutscenes.loop = true;
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
        g.cutscene_props.clear();
        g.cutscene_props_index = static_cast<size_t>(-1);
        return;
    }
    if (g.cutscene_props_index == q.index)
    {
        return;
    }
    g.cutscene_props_index = q.index;
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
// vtable+0xC receives (object, {id 0x47, host}).
struct PulseNode
{
    void (*handle)(PulseNode* self, i32 id, void* host) = nullptr;
    PulseNode* next = nullptr;
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
        return static_cast<int>(g_crt_pctype[c] & 8u);
    }
    return static_cast<int>(static_cast<u8>(g_crt_pctype[c]) & 8u);
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
struct CrtTidData
{
    u32 tid;
    u32 thandle;
    u8 rest[0x80];
};

static_assert(sizeof(CrtTidData) == 0x88);

struct CrtModuleTls
{
    // Xbox stores the _tiddata pointer at offset 0xC of the 0x90-byte TLS
    // block (TLS directory SizeOfZeroFill, AddressOfIndex 0x00434234).
    void* ptd = nullptr;
};

CrtLockSlot g_crt_locks[0x24];
std::unique_ptr<std::recursive_mutex> g_crt_lock_mu[0x24];
XThreadNotify g_xthread_notify_head;
XThreadNotify g_crt_thread_notify;
std::recursive_mutex g_xthread_notify_cs;
CrtModuleTls g_crt_module_tls;
u32 g_crt_tls_index = 0;

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
    // 10. The host constructor either throws before the table lock or
    // succeeds; the spin count is not a host parameter. The finally still
    // leaves lock 10. Success returns 1.
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
        errno = 12;
        return 0;
    }

    if (locknum == 10)
    {
        g_crt_lock_mu[10] = std::move(pcs);
        g_crt_locks[10].cs = g_crt_lock_mu[10].get();
        return 1;
    }

    fn_1681c1(10);
    if (g_crt_locks[locknum].cs == nullptr)
    {
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
            std::terminate();
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
            std::terminate();
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
    // RTL_CRITICAL_SECTION; the spin count is not a host parameter.
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
} // namespace

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
                          << " root=" << g.scene_script.root_offset << " aids " << g.scene_script.aids.size() << "\n";
                log_script_refs(g, g.scene_script);
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
    g.scene_name = kNoScript;
    g.scene_script = {};
    g.script_aidlists.clear();
    g.cutscenes = {};
    g.models.clear();
    g.cutscene_props.clear();
    g.cutscene_props_index = static_cast<size_t>(-1);
    g.ui.clear();
}

void script_set_name(Game& g, std::string_view name)
{
    // Xbox 0x00128AB0: EAX = C string, copies into DAT_00512494.
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
            const auto pose = sample_mina(actor.mina, q.time / cut.duration);
            const auto bind = sample_mina(actor.mina, 0.f);
            const int n = int(std::min(pose.size(), std::min(prop.mesh.joints.size(), size_t(128))));
            float bones[128 * 16]{};
            if (n > 0)
            {
                skin_matrices(prop.mesh.joints, pose, bones, n, bind.size() >= size_t(n) ? &bind : nullptr);
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

} // namespace gbtg
