#include "gbtg/engine_internal.h"
#include "gbtg/gfx.h"
#include "gbtg/movie.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace gbtg
{
namespace
{

Entity* spawn(Game& g, const char* type_name, Actor& actor)
{
    const EntityType* type = find_entity_type(type_name);
    g.entities.push_back(Entity{type, &actor, actor.position, actor.position, actor.euler, false});
    Entity& e = g.entities.back();
    if (e.type && e.type->tick)
    {
        e.type->tick(e);
    }
    return &e;
}

} // namespace

void Game::init(const std::filesystem::path& game_dir, std::string_view scene_select)
{
    // Xbox: _cinit runs entity type static ctors, then WinMain 0x00032390:
    //   gfx_init_32470, fs_init_33980, game_init_32630.
    register_entity_types();
    clock_init(clock);
    aids.open(game_dir);
    movie_set_root(game_dir);
    movie_boot_probe();
    std::cout << "fs_init: " << aids.game_dir.string() << " common.bnl entries " << aids.common.entries.size() << "\n";
    std::cout << "clock " << clock_fps(clock) << " Hz dt=" << clock.dt << "\n";
    std::cout << "entity types " << all_entity_types().size() << "\n";

    script_set_default_name(*this);
    if (const BundleEntry* list = find_entry(aids.common, kSceneOrder))
    {
        scene_order = parse_aidlist(list->cpu);
        std::cout << kSceneOrder << " (" << scene_order.size() << ")\n";
        for (size_t i = 0; i < scene_order.size() && i < 12; ++i)
        {
            std::cout << "  [" << i << "] " << scene_order[i] << "\n";
        }
        if (scene_order.size() > 12)
        {
            std::cout << "  ...\n";
        }
        if (!scene_order.empty())
        {
            size_t index = 0;
            if (!scene_select.empty())
            {
                bool digits = true;
                for (char c : scene_select)
                {
                    if (c < '0' || c > '9')
                    {
                        digits = false;
                        break;
                    }
                }
                if (digits)
                {
                    index = std::min(scene_order.size() - 1, size_t(std::stoul(std::string(scene_select))));
                }
                else
                {
                    for (size_t i = 0; i < scene_order.size(); ++i)
                    {
                        if (scene_order[i].find(scene_select) != std::string::npos)
                        {
                            index = i;
                            break;
                        }
                    }
                }
                std::cout << "scene select [" << index << "] " << scene_order[index] << "\n";
            }
            scene_index = index;
            // 0x0011EB30 queues the aid. 0x0011E4C0 loads it once the stream enters state 4.
            script_request(*this, scene_order[index]);
        }
    }

    actors.reserve(16);
    entities.reserve(16);
    actors.emplace_back();
    camera = spawn(*this, "entityBodyCamera", actors.back());

    game_init_pump();
    std::cout << "game_init: io idle\n";
    // 0x0011E4C0. The first pass accepts the queued aid into state 3.
    // The next pass enters state 4 and loads that script.
    // No simulation frame has elapsed during initialization. In particular,
    // do not consume the transition's 1/60-second timer before the first tick.
    clock.dt = 0.f;
    script_pump(*this);
}

void frame_tick(Game& g, float dt, const InputState& in)
{
    // Xbox 0x00033B50.
    clock_pad(g.clock, dt);    // 0x000FC030
    clock_frame_time(g.clock); // 0x00033C80
    g.time = g.clock.time;
    g.frame = g.clock.frame;
    if (in.quit)
    {
        g.running = false;
        return;
    }
    pad_tick(g, in); // 0x000FA3C0 / 0x000FAD80
    gameselect_tick(g, in);
    // 0x00033C47. 0x0011E4C0. A queued aid_script loads when the stream enters state 4.
    script_pump(g);
    // 0x0011E927. The condition list is updated after the pump, so a request
    // issued by a fired action waits until the next frame.
    script_vm_tick(g, g.clock.dt);
    audio_tick(g, g.clock.dt); // 0x0015B780, 0x0003A040, 0x000396F0, 0x00038640, 0x00038C20
    cutscene_queue_tick(g, g.clock.dt);
    for (auto& e : g.entities)
    {
        if (e.type && e.type->tick)
        {
            e.type->tick(e);
        }
    }
    file_pump(); // 0x000E9E60
    // 0x00033B50 walks DAT_0050E0B8 before present. Present walks it twice more.
    movie_advance_time(g.clock.dt);
    movie_present();
    // Present 0x00033480 / 0x00032AB0 / 0x00033A90. The PC backend draws the scene models
    // with the body camera; Xbox D3D8 would issue the original calls here.
    if (g.camera)
    {
        gfx_set_camera(g.camera->pose_position, g.camera->pose_quat, g.camera->pose_fov);
    }
    for (const Mesh& mesh : g.models)
    {
        gfx_draw_mesh(mesh);
    }
    draw_cutscene_props(g);
    draw_ui(g);
    draw_scene_transition(g);
}

void Game::tick(float dt, const InputState& in)
{
    frame_tick(*this, dt, in);
}

} // namespace gbtg
