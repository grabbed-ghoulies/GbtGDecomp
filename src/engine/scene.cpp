#include "gbtg/anim.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/model.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

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

// Body-camera channels at +0x150 (count +0x154). camera_apply calls 0x0001EC20
// when that count is greater than 0. Host is camera+8: +0x50 is the bind list
// walked by 0x00030B40, +0x204 is the slot table (float* at +0x1cc).
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
    u8 mark[2] = {};  // +0xec and +0xf8
    i32 gate = 0;     // +0xfc
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
    // camera_apply 0x00025AE0 calls 0x0001EC20 when camera+0x154 > 0, then 0x0001DD00.
    if (!g_camera_channels.items.empty())
    {
        fn_1ec20(g_camera_channels);
    }
    fn_1dd00(g_camera_wash, dt);
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
