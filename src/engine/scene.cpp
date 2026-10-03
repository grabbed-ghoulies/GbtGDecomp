#include "gbtg/anim.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/model.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#if defined(_WIN32)
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
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

int fn_14aaed(const u16* src, int cch, u8* dst, int cb)
{
    // Xbox 0x0014AAED
    // WideCharToMultiByte for code page 65001. A cch of -1 counts through the
    // NUL (0x00166ECB) and includes it. Bytes that fit are stored. A nonzero
    // cb shorter than the encoded size returns 0; 0x0014B00C would record
    // last-error 0x7A.
    if (cch == -1)
    {
        int length = 0;
        while (src[length] != 0)
        {
            ++length;
        }
        cch = length + 1;
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
    // path returns 0x57. The port keeps the page and returns
    // RtlNtStatusToDosError(0).
    constexpr u32 kTitleId = 0x4D530053u;
    constexpr char kCdrom[] = "\\Device\\Cdrom0";
    static u8 launch_page[0x1000];

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
        std::memset(launch_page, 0, sizeof(launch_page));
        std::memcpy(launch_page + 0, &stored_type, 4);
        std::memcpy(launch_page + 4, &kTitleId, 4);
        if (image != nullptr)
        {
            char* dest = reinterpret_cast<char*>(launch_page + 8);
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
            std::memcpy(launch_page + 0x400, launch_data, 0xC00);
        }
        if (alt_tail != nullptr)
        {
            const int n1 = static_cast<int>(std::strlen(kCdrom));
            const int n2 = static_cast<int>(std::strlen(alt_tail));
            if (n2 + (n1 + 1) + 8 < 0x1EC)
            {
                u8* rec = launch_page + 0x214;
                const u16 n1_nul = static_cast<u16>(n1 + 1);
                rec[0] = 1;
                rec[1] = 0;
                rec[2] = static_cast<u8>(n1_nul & 0xFF);
                rec[3] = static_cast<u8>(n1_nul >> 8);
                std::memcpy(rec + 4, &stored_type, 4);
                if (stored_type == 0xFFFFFFFFu)
                {
                    const u32 zero = 0;
                    std::memcpy(launch_page, &zero, 4);
                }
                std::memcpy(rec + 8, kCdrom, static_cast<size_t>(n1) + 1);
                std::memcpy(rec + 8 + n1 + 1, alt_tail, static_cast<size_t>(n2) + 1);
            }
        }
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
