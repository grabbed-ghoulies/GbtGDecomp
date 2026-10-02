#include "gbtg/anim.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/model.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
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
