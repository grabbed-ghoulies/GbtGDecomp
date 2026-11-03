#include "scene_actor_services_state.h"

namespace gbtg
{
namespace scene_services_detail
{
[[noreturn]] void unbound(const std::string& what)
{
    throw Error("scene actor service is not bound: " + what);
}

// D3DX matrix routines the shader matrix code calls (D3D host boundary), on row-major 4x4 floats.
RuntimeShaderMatrixBoundaries d3dx_matrix_boundaries()
{
    RuntimeShaderMatrixBoundaries boundaries;
    boundaries.translation = [](RuntimeShaderMatrix& output, float x, float y, float z)
    {
        output = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, x, y, z, 1.f};
    };
    boundaries.scaling = [](RuntimeShaderMatrix& output, float x, float y, float z)
    {
        output = {x, 0.f, 0.f, 0.f, 0.f, y, 0.f, 0.f, 0.f, 0.f, z, 0.f, 0.f, 0.f, 0.f, 1.f};
    };
    boundaries.rotation_x = [](RuntimeShaderMatrix& output, float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        output = {1.f, 0.f, 0.f, 0.f, 0.f, c, s, 0.f, 0.f, -s, c, 0.f, 0.f, 0.f, 0.f, 1.f};
    };
    boundaries.rotation_y = [](RuntimeShaderMatrix& output, float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        output = {c, 0.f, -s, 0.f, 0.f, 1.f, 0.f, 0.f, s, 0.f, c, 0.f, 0.f, 0.f, 0.f, 1.f};
    };
    boundaries.rotation_z = [](RuntimeShaderMatrix& output, float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        output = {c, s, 0.f, 0.f, -s, c, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    };
    boundaries.multiply = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& left,
        const RuntimeShaderMatrix& right)
    {
        multiply_runtime_matrix(output, left, right);
    };
    boundaries.transpose = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
    {
        const RuntimeShaderMatrix source = input;
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                output[size_t(row * 4 + column)] = source[size_t(column * 4 + row)];
            }
        }
    };
    return boundaries;
}

SceneActorServices::State* active_services = nullptr;
} // namespace scene_services_detail

SceneActorServices::SceneActorServices(SceneFactoryRuntime& scene, Game& game)
    : state_(std::make_unique<State>(scene, game))
{
}

SceneActorServices::~SceneActorServices() = default;

void SceneActorServices::begin_scene()
{
    state_->begin_scene();
}

void SceneActorServices::reset_sound_queues()
{
    reset_actor_sound_queues(state_->sounds);
}

void SceneActorServices::end_scene()
{
    state_->end_scene();
}

void SceneActorServices::retire_actors()
{
    state_->retire_actors();
}

void SceneActorServices::begin_teardown()
{
    state_->begin_teardown();
}

bool SceneActorServices::in_scene() const
{
    return state_->in_scene;
}

void SceneActorServices::activate_actors()
{
    state_->activate_actors();
}

void SceneActorServices::hold_boot_scene_order()
{
    auto& services = *state_->gameselect_services;
    hold_runtime_scene_order(services.scene_order, services); // 0x0011C480: 0x001234B0, EAX *DAT_00430CF8
}

// The scene saves' context over the current scene's globals.
void SceneActorServices::State::with_scene_save(const std::function<void(RuntimeSceneSave&)>& run)
{
    SaveGameContext saves{game.native_movies->session->files(), g_runtime_profile, scene_order_held};
    RuntimeSceneSave save{g_runtime_profile, saves, *gameselect_services, reinterpret_cast<const char*>(g_scene_record),
        g_chapter5_gold, g_flags_ea9c, g_scene_gameplay_time};
    run(save);
}

void SceneActorServices::save_scene_start()
{
    state_->with_scene_save([](RuntimeSceneSave& save)
    {
        save_runtime_scene(save, g_script_30d0c); // DAT_00430D0C
    });
}

std::span<const u8> SceneActorServices::held_scene_order() const
{
    return state_->scene_order_held;
}

void SceneActorServices::present_views()
{
    auto& state = *state_;
    const SceneD3DPresentContext present{state.resources.memory, state.resources.d3d, state.d3d_device.vertex_shaders,
        [&state](const u8* record) -> const RgbaImage&
        {
            return state.texture_image(record);
        },
        [&state](std::uintptr_t color, std::uintptr_t depth)
        {
            state.present_render_target(color, depth);
        },
        [&state](u32 texture)
        {
            state.copy_back_buffer(texture);
        }};
    present_runtime_display_views([&state, &present](i32 slot)
    {
        state.d3d_device.commands.clear();
        state.present_view(slot); // 0x0011C240.
        auto commands = state.d3d_device.take_commands();
        present_scene_d3d_commands(commands, present);
        // Recycle the recording boundary's command capacity after synchronous presentation.
        commands.clear();
        state.d3d_device.commands = std::move(commands);
    }, *state.display_setup);
}

void SceneActorServices::start_background()
{
    state_->start_background();
}

void SceneActorServices::start_scene(const std::function<void()>& script_event)
{
    auto& state = *state_;
    start_background();
    // 0x0011CBD0 applies the script's door states before it creates the scene objects.
    ScriptDoorStartContext door_start{*state.doors, [&state](u32 id)
    {
        state.notify_body_list(id);
    }};
    script_apply_scene_door_states(door_start); // 0x00125B80
    state.create_scene_objects(); // 0x0011C980
    load_runtime_ghoul_intro_rows(*state.ghoul_intro_context); // 0x000F4440
    start_markers();
    // 0x00115FF0: the pending scene controls join the active and update lists.
    auto controls = state.control_updates();
    activate_runtime_scene_controls(controls);
    activate_actors();
    script_event();
    send_scene_start_notice();
    notify_runtime_scene_controls(controls); // 0x00116090
    send_scene_start();
    clear_runtime_scene_control_updates(controls); // 0x001160C0
    auto activation = state.activation_context();
    clear_runtime_scene_start_list(activation); // 0x00123240
    start_runtime_scene_named_sounds(state.sounds, state.voices); // 0x0012A1E0
    set_up_runtime_scoreboard(*state.scoreboard_hud); // 0x00132040
    start_runtime_scene_challenge(*state.challenge_services); // 0x00049BA0, challenge 21's run
    // The player start latch DAT_005462FC, set by 0x0011CAC0,
    // ends here: a later kind 2 marker (0x00127FF0 after a walk-in) spawns its player.
    g_player_start_enabled = 0;
}

void SceneActorServices::State::notify_body_list(u32 id)
{
    const std::array<u32, 1> message{id};
    message_body_list(message);
}

void SceneActorServices::State::message_body_list(std::span<const u32> words)
{
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.body_a.next); node != nullptr;)
    {
        auto* current = node;
        node = static_cast<entity_detail::ListHead*>(node->next);
        std::array<u32, 0x42> message{};
        std::copy_n(words.begin(), std::min<size_t>(words.size(), message.size()), message.begin());
        if (auto* background = backgrounds->owner_of(*current); background != nullptr)
        {
            background_message(*background, message);
            continue;
        }
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (static_cast<void*>(&player->links[0]) == static_cast<void*>(current))
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B20 node with no native background or player");
        }
        object_message(*object_of(owner->actor_bytes.data()), message);
    }
}

RuntimeSceneControlUpdates SceneActorServices::State::control_updates()
{
    RuntimeActorLink*& pending = g_scene_controls.pending.next; // DAT_0050E440 itself.
    return RuntimeSceneControlUpdates{pending, g_scene_controls.active, g_scene_controls.updates,
        control_update_link, control_message, control_update};
}

// Xbox 0x0011C980
void SceneActorServices::State::create_scene_objects()
{
    auto& requests = g_scene_object_requests;
    if (requests.count < 1)
    {
        requests.count = 0;
        return;
    }
    for (i32 index = 0; index < requests.count; ++index)
    {
        const char* name = requests.objects.data() + size_t(index) * 128;
        if (std::memcmp(name, "aid_", 4) != 0)
        {
            continue;
        }
        // 0x00034D00 finds the objparams; the clone and 0x00107520 creation follow, then the clone is freed.
        ClonedExecutableResource parameters = clone_executable_resource(image.pools, name);
        if (parameters.bytes == nullptr)
        {
            continue;
        }
        FactoryObject object = create_factory_object(scene.factory, {parameters.bytes.get(), parameters.size});
        if (object.bytes != nullptr)
        {
            scene.keep_object(std::move(object));
        }
    }
    requests.count = 0;
}

void SceneActorServices::send_scene_start_notice()
{
    auto activation = state_->activation_context();
    send_runtime_scene_start_notice(activation);
}

void SceneActorServices::send_scene_start()
{
    auto activation = state_->activation_context();
    send_runtime_scene_start(activation);
}

void SceneActorServices::send_actor_message(u32 address, std::span<const u32> message)
{
    auto& state = *state_;
    auto* object = state.object_of(state.scene.resolve_address(address, 0).data());
    if (object == nullptr)
    {
        throw Error("actor message has no owning factory object");
    }
    state.object_message(*object, {const_cast<u32*>(message.data()), message.size()});
}

void SceneActorServices::update_actor(u32 address, RuntimeEffectPassScratch& scratch)
{
    auto& state = *state_;
    auto* previous = state.effect_pass_scratch;
    state.effect_pass_scratch = &scratch;
    state.frame_delta = state.game.clock.dt;
    state.pal = state.game.clock.pal ? 1 : 0;
    try
    {
        state.update_actor(address);
    }
    catch (...)
    {
        state.effect_pass_scratch = previous;
        throw;
    }
    state.effect_pass_scratch = previous;
}

void SceneActorServices::State::refresh_emitter_players()
{
    emitter_players.clear();
    emitter_player_views.clear();
    // DAT_00511B40 holds the players' actor +0x93C nodes (links[1]).
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.case_head.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (static_cast<void*>(&player->links[1]) == static_cast<void*>(node))
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B40 node with no native player");
        }
        owner->refresh_body_views();
        if (owner->body.empty())
        {
            unbound("DAT_00511B40 player without its body for the emitters");
        }
        const auto found = actors->actors.find(scene.resolve_address(read_u32(owner->body, 8), 0).data());
        if (found == actors->actors.end() || found->second == nullptr)
        {
            unbound("DAT_00511B40 player body +8 outside the scene actors");
        }
        const Actor* carried = owner->attached != nullptr ? owner->attached->actor : nullptr; // Body +0x48.
        if (owner->attached != nullptr && carried == nullptr)
        {
            unbound("DAT_00511B40 player body +0x48 without its native actor");
        }
        emitter_player_views.push_back(std::make_unique<RuntimeFxEmitterPlayer>(RuntimeFxEmitterPlayer{
            owner->actor, owner->actor_bytes, owner->body, *found->second, carried,
            owner->actor_instance.startup.resource, owner->callout}));
        emitter_players.push_back(emitter_player_views.back().get());
    }
}

FactoryObject& SceneActorServices::State::object_of_actor(Actor& actor)
{
    for (const auto& [bytes, native] : actors->actors)
    {
        if (native == &actor)
        {
            if (auto* object = object_of(bytes); object != nullptr)
            {
                return *object;
            }
        }
    }
    throw Error("scene actor update for an actor outside the scene (class " + std::to_string(actor.class_key) + ")");
}

void SceneActorServices::update_scene_actors()
{
    auto& state = *state_;
    auto* previous = state.effect_pass_scratch;
    state.effect_pass_scratch = &state.frame_effect_scratch;
    state.pause = g_sim_freeze;
    state.frame_delta = g_sim_dt;
    state.time_scale = g_sim_scale;
    state.motion_steps = g_motion_steps;
    state.pal = state.game.clock.pal ? 1 : 0;
    state.refresh_mind_targets();
    state.refresh_pause_groups();
    state.refresh_emitter_players();
    // DAT_00511B40 minus 0x93C: the player 0x000F4E10 measures notices from.
    const u32 link = state.cases.next == nullptr ? 0 : state.address_of(state.cases.next);
    state.notice_player = link == 0 ? nullptr
                                    : state.actors->actors.at(state.scene.resolve_address(link - 0x93C, 0).data());
    const std::function<bool(Actor&)> retired = [&state](Actor& actor)
    {
        const auto found = state.actors->render.find(&actor);
        if (found == state.actors->render.end())
        {
            throw Error("scene actor update has no actor +0x908 view");
        }
        return found->second->child.prev != nullptr;
    };
    const std::function<void(Actor&)> update = [&state](Actor& actor)
    {
        state.update_actor(state.scene.address_of(state.object_of_actor(actor).bytes.get()));
    };
    const std::function<Actor&(entity_detail::ListHead&)> body_actor = [&state](entity_detail::ListHead& node)
        -> Actor&
    {
        if (auto* background = state.backgrounds->owner_of(node); background != nullptr)
        {
            return background->state.actor;
        }
        throw Error("scene +0x934 list node is not owned by a native background");
    };
    RuntimeSceneActorTreeUpdates updates{retired, update};
    RuntimeSceneBodyListUpdates bodies{entity_detail::g_world.body_a, body_actor, updates};
    try
    {
        update_runtime_scene_actor_tree(state.world_bind.child, updates);
        update_runtime_scene_body_list(bodies);
    }
    catch (...)
    {
        state.effect_pass_scratch = previous;
        throw;
    }
    state.effect_pass_scratch = previous;
}

void SceneActorServices::select_frame_display()
{
    // 0x00033B50 points DAT_005077A8 at the first display record before the frame's work.
    state_->display_view = &entity_detail::g_display_records[0];
    state_->display_address = 0x00507018;
}

Actor& SceneActorServices::State::bin_owner(entity_detail::ListHead& node)
{
    for (auto& [native, registration] : actors->render)
    {
        for (auto& bin : registration->bins)
        {
            if (&bin == &node)
            {
                return *native;
            }
        }
    }
    throw Error("view bin record has no scene actor");
}

RuntimeSceneShadowActor SceneActorServices::State::shadow_actor(Actor& actor)
{
    FactoryObject& object = object_of_actor(actor);
    auto& view = actors->view(object);
    Actor* held = nullptr;
    const u32 held_address = read_u32({object.bytes.get(), object.size}, 0x314);
    if (held_address != 0)
    {
        const auto found = actors->actors.find(scene.resolve_address(held_address, 0).data());
        if (found == actors->actors.end())
        {
            throw Error("actor +0x314 does not name a native scene actor");
        }
        held = found->second;
    }
    return RuntimeSceneShadowActor{actor, actor.model != nullptr ? actor.model->runtime : nullptr,
        view.startup.models.secondary.get(), view.startup.secondary_mode, view.startup.secondary_flags,
        view.frame.eased, view.frame.target, held};
}

// 0x00123270 over the port's DAT_00511B5C order: the nodes are linked for the drain, which empties the list.
void SceneActorServices::State::update_first_actors()
{
    std::vector<RuntimeActorLink> nodes(scene_nodes.size());
    RuntimeActorLink head;
    RuntimeActorLink* previous = &head;
    for (auto& node : nodes)
    {
        previous->next = &node;
        node.previous = previous;
        previous = &node;
    }
    const std::vector<u32> addresses = scene_nodes;
    const std::function<Actor&(RuntimeActorLink&)> actor = [this, &nodes, &addresses](RuntimeActorLink& node)
        -> Actor&
    {
        const u32 address = addresses[size_t(&node - nodes.data())];
        return *actors->actors.at(scene.resolve_address(address, 0).data());
    };
    const std::function<void(Actor&)> update = [this](Actor& updated)
    {
        update_actor(scene.address_of(object_of_actor(updated).bytes.get()));
    };
    RuntimeSceneActorUpdates updates{head.next, actor, update};
    // A new actor's first update can tick an effect it owns (the bone debris' 0x000EF2D0); the frame's scratch
    // stands for this pass's incoming stack bytes as it does for update_scene_actors.
    auto* scratch = effect_pass_scratch;
    effect_pass_scratch = &frame_effect_scratch;
    try
    {
        update_runtime_scene_actors(updates);
    }
    catch (...)
    {
        effect_pass_scratch = scratch;
        throw;
    }
    effect_pass_scratch = scratch;
    scene_nodes.clear();
}

void SceneActorServices::State::remove_retired_actors()
{
    const std::function<Actor&(entity_detail::ListHead&)> owner = [this](entity_detail::ListHead& node) -> Actor&
    {
        for (auto& [native, registration] : actors->render)
        {
            if (&registration->child == &node)
            {
                return *native;
            }
        }
        throw Error("retired +0x908 node has no scene actor");
    };
    const auto address = [this](const Actor& actor)
    {
        return scene.address_of(object_of_actor(const_cast<Actor&>(actor)).bytes.get());
    };
    RuntimeNoticeRemoval notice_removal{notices,
        [this]()
        {
            stop_runtime_ghoul_intro(*ghoul_intro_context); // 0x000F4C10
        },
        [this]() -> u32
        {
            return start_next_runtime_ghoul_intro(*ghoul_intro_context); // 0x000F4CC0
        },
        [this]()
        {
            finish_runtime_ghoul_intro(*ghoul_intro_context); // 0x000F4B90
        }};
    RuntimeSceneActorRemoval removal{entity_detail::g_world.child, owner,
        [this, address](const Actor& actor)
        {
            Message message;
            message.id = 0x7C;
            message.arg = const_cast<Actor*>(&actor);
            for (auto* recipient = latches; recipient != nullptr;)
            {
                auto* current = recipient;
                recipient = recipient->next;
                current->handle_message(*current, message);
            }
            // A weapon a player made or threw (0x0013A720 into the object list) is on retail's DAT_00511AA4 but has no
            // node here, so each weapon off the host latch list gets the notice by its own message (0x001462B0 clears
            // the holder and the marker rows naming the actor).
            const u32 gone = address(actor);
            std::vector<FactoryObject*> weapons;
            for (const u8* bytes : classes->weapons())
            {
                FactoryObject* weapon = object_of(bytes);
                if (weapon != nullptr && !active_nodes.contains(scene.address_of(bytes)))
                {
                    weapons.push_back(weapon);
                }
            }
            for (FactoryObject* weapon : weapons)
            {
                std::array<u32, 0x42> words{0x7C, gone};
                object_message(*weapon, words);
            }
        },
        [](std::span<const u32> words)
        {
            for (RuntimeActorLink* node = g_scene_controls.active.next; node != nullptr;)
            {
                RuntimeActorLink* next = node->next;
                g_scene_control_owner->message(g_scene_control_owner->instance_of(*node), words);
                node = next;
            }
        },
        [](const Actor& actor)
        {
            detach_actor_from_global_effects(const_cast<Actor*>(&actor), g_actor_effect_nodes);
        },
        notice_removal,
        [this, address](const Actor& actor)
        {
            const std::function<void(u32, std::span<const u32>)> message = [](u32, std::span<const u32>)
            {
                unbound("scare message during actor removal");
            };
            RuntimePowerupTable powerups{powerup_rows, powerup_board, resources.registry};
            RuntimeSceneScareTimers scare{powerups, scared_count, scared_actors, scared_times, clock, message};
            remove_runtime_scared_actor(address(actor), scare);
        },
        address,
        [this](Actor& actor)
        {
            release_factory_object(scene.factory, object_of_actor(actor));
        }};
    remove_runtime_retired_actors(removal);
}

// Xbox 0x00120EF0
void SceneActorServices::State::update_shadow_casters()
{
    const std::function<Actor&(entity_detail::ListHead&)> bin_actor = [this](entity_detail::ListHead& node)
        -> Actor&
    {
        return bin_owner(node);
    };
    const std::function<RuntimeSceneShadowActor(Actor&)> shadow = [this](Actor& actor)
    {
        return shadow_actor(actor);
    };
    std::span<const u8> background;
    if (auto* first = backgrounds->first_background(); first != nullptr && first->state.actor.model != nullptr)
    {
        background = first->state.actor.model->cpu;
    }
    const u32 view = (display_address - 0x00507018) / 0x1E4;
    const Actor* player = cases.next == nullptr ? nullptr
        : actors->actors.at(scene.resolve_address(address_of(cases.next) - 0x93C, 0).data());
    RuntimeSceneShadowCasters context{*display_setup, view, entity_detail::g_world.bins, bin_actor, shadow, player,
        background, ambient_points, ambient_points_enabled, g_light_offset, g_light_paused, g_light_running, random,
        g_light_filled};
    update_runtime_scene_shadow_casters(context);
}

// Xbox 0x0011E640
void SceneActorServices::tick_live_spawns()
{
    auto& state = *state_;
    state.classes->controller_actors(state.controller_actors);
    // DAT_00511B40: the first case-list node, the player's actor +0x93C (links[1]).
    state.spawn_players.clear();
    state.spawn_player_address = 0;
    for (auto* player : state.players->instances())
    {
        state.spawn_players[player->actor_address] = &player->actor;
        if (static_cast<const void*>(&player->links[1]) == static_cast<const void*>(entity_detail::g_world.case_head.next))
        {
            state.spawn_player_address = player->actor_address + 0x93C;
        }
    }
    LiveSpawnTickContext context{*state.aid_spawns, g_sim_freeze, g_sim_dt, g_sim_scale, state.spawn_player_address,
        state.spawn_players};
    tick_live_spawn_queues(context); // 0x0011B670
}

void SceneActorServices::update_scoreboard_hud()
{
    update_runtime_scoreboard_hud(*state_->scoreboard_update); // 0x00132210
}

void SceneActorServices::tick_screen_effects()
{
    auto& state = *state_;
    tick_runtime_screen_effects(state.screen_tint, state.pulse, state.clock, g_scene_transition.phase); // 0x00060D50
}

void SceneActorServices::run_scene_pass()
{
    auto& state = *state_;
    auto controls = state.control_updates();
    activate_runtime_scene_controls(controls); // 0x00115FF0
    auto activation = state.activation_context();
    activate_runtime_scene_actors(activation); // 0x00123070
    state.refresh_pause_groups(); // Activation links new players' +0x934 nodes onto DAT_00511B38.
    notify_runtime_scene_controls(controls); // 0x00116090
    send_runtime_scene_start(activation); // 0x001231A0
    update_runtime_scene_controls(controls); // 0x001160F0
    state.pause = g_sim_freeze;
    state.frame_delta = g_sim_dt;
    state.time_scale = g_sim_scale;
    state.pal = state.game.clock.pal ? 1 : 0;
    state.update_first_actors(); // 0x00123270
    const i32 run_state = i32(read_u32({g_scene_record, 0x108}, 0x104)); // DAT_00510F5C, scene record +0x104.
    if (0 < run_state && run_state < 3)
    {
        const std::function<void(u32, std::span<const u32>)> scare_message =
            [&state](u32 address, std::span<const u32> words)
        {
            auto* object = state.object_of(state.scene.resolve_address(address, 0).data());
            state.object_message(*object, {const_cast<u32*>(words.data()), words.size()});
        };
        RuntimePowerupTable powerups{state.powerup_rows, state.powerup_board, state.resources.registry};
        RuntimeSceneScareTimers scare{powerups, state.scared_count, state.scared_actors, state.scared_times,
            state.clock, scare_message};
        tick_runtime_scene_scare_timers(scare); // 0x00123BA0
        update_effects(state.frame_effect_scratch); // 0x000ED480
        advance_scene_gameplay_time(g_scene_times, state.clock, g_flags_ea9c); // 0x0012B1F0
    }
    update_sounds(); // 0x0012A150
    // 0x00115F70 over DAT_0050E450: each disposed control's object is destroyed and freed.
    std::vector<FactoryObject*> disposed;
    for (RuntimeActorLink* node = g_scene_controls.disposal.next; node != nullptr; node = node->next)
    {
        disposed.push_back(&state.scene.controls->object_of_disposal(*node,
            {&state.scene.objects, &state.dialogue_bats}));
    }
    std::vector<std::unique_ptr<RuntimeSceneObjectDisposalLink>> links;
    for (auto object = disposed.rbegin(); object != disposed.rend(); ++object)
    {
        auto* next = links.empty() ? nullptr : links.back().get();
        links.push_back(std::make_unique<RuntimeSceneObjectDisposalLink>(RuntimeSceneObjectDisposalLink{next,
            **object}));
    }
    dispose_runtime_scene_objects(links.empty() ? nullptr : links.back().get(), state.scene.factory);
    state.remove_retired_actors(); // 0x001232D0
    g_light_filled = 0; // DAT_0050D71C.
    state.update_shadow_casters(); // JMP 0x00120EF0: the shadow volumes present's 0x000FE960 draws.
}

void SceneActorServices::update_sounds()
{
    auto& state = *state_;
    const std::function<void(Actor&, u32, RuntimeActorSound&)> message =
        [&state](Actor& actor, u32 id, RuntimeActorSound& sound)
        {
            const u32 address = state.heap_address(&sound, sizeof(sound));
            const auto found = state.message_references.find(address);
            void* previous = found == state.message_references.end() ? nullptr : found->second;
            state.message_references[address] = &sound;
            std::array<u32, 0x42> words{};
            words[0] = id;
            words[1] = address;
            try
            {
                state.actor_message(actor, words.data());
            }
            catch (...)
            {
                if (previous != nullptr)
                {
                    state.message_references[address] = previous;
                }
                else
                {
                    state.message_references.erase(address);
                }
                throw;
            }
            if (previous != nullptr)
            {
                state.message_references[address] = previous;
            }
            else
            {
                state.message_references.erase(address);
            }
        };
    RuntimeActorSoundTick context{state.voices, state.display_view->position, message};
    tick_runtime_actor_sounds(state.sounds, context);
}

void SceneActorServices::update_effects(RuntimeEffectPassScratch& scratch)
{
    auto& state = *state_;
    state.frame_delta = state.game.clock.dt;
    state.pal = state.game.clock.pal ? 1 : 0;
    RuntimeEffectDebris debris{{state.debris_pool->particles, effect_debris_pool.size}, state.debris_pool->types,
        state.debris_pool->free_head, state.game.clock.frame, state.random};
    const Vec3 plane{read_f32(state.data(0x00433FFC, 12), 0),
        read_f32(state.data(0x00433FFC, 12), 4), read_f32(state.data(0x00433FFC, 12), 8)};
    const Vec3 minimum{g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2]};
    const Vec3 maximum{g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]};
    RuntimeEffectCollisionContext movement{state.effect_collision, state.clock, *state.display_view,
        plane, g_amb_volume_enabled, minimum, maximum, state.random};
    RuntimeEffectCollisionCommands commands{state.effect_collision, debris, state.voices, state.effects,
        *state.footsteps, state.effect_callback_result};
    const std::function<std::span<const u8>(u32)> resolve = [&state](u32 address)
    {
        return std::span<const u8>{state.resources.memory.tail(address)};
    };
    const std::function<u32(const u8*)> address = [&state](const u8* pointer)
    {
        for (auto* effect : g_actor_effect_nodes)
        {
            if (effect != nullptr && pointer == effect->bytes.data())
            {
                return state.heap_address(pointer, effect->bytes.size());
            }
        }
        // A binding inside an actor model's CPU view (its serialized copy) has the same offset in the relocated
        // CPU block the model resource occupies.
        for (const auto& [relocated, model] : state.resources.actor_models)
        {
            const u8* first = model.cpu.data();
            if (first != nullptr && first <= pointer && pointer < first + model.cpu.size())
            {
                return state.resources.memory.address_of(relocated) + u32(pointer - first);
            }
        }
        return state.resources.memory.address_of(pointer);
    };
    for (auto* effect : g_actor_effect_nodes)
    {
        if (effect != nullptr)
        {
            const u32 script = state.resources.memory.address_of(effect->script.data());
            std::memcpy(effect->bytes.data() + 0x134, &script, 4);
        }
    }
    RuntimeEffectSimulation simulation{movement, commands, resolve, state.pal, state.pal_override};
    RuntimeEffectTickContext tick{simulation, state.game.clock.frame, state.effect_sound_frame,
        address, state.display_view->forward, scratch};
    RuntimeModelSoundRelease release{state.sounds, state.resources.registry, state.deferred,
        state.fence, state.shaders, state.heap_free};
    RuntimeEffectTrails trails{{state.trail_pool->particles, effect_trail_pool.size}, state.trail_pool->types,
        state.trail_pool->free_head, state.random, state.pal, state.pal_override, resolve};
    RuntimeGlobalEffectTick context{g_actor_effect_nodes, tick, release, debris, trails, state.clock};
    tick_global_actor_effects(context);
}

void SceneActorServices::start_markers()
{
    auto& state = *state_;
    if (state.markers == nullptr)
    {
        throw Error("scene markers are not loaded");
    }
    // DAT_00545C48/DAT_00545CCC (pair and group ids) are filled by script types 0x4E and 0x73 before startup.
    const RuntimeSceneMarkerRandomizer randomizer{state.random, g_marker_swap_count, g_marker_swap_ids, g_marker_group_count, g_marker_group_ids};
    // Host diagnostics: a failing handler names its marker kind and id.
    RuntimeSceneMarkerHandlers handlers = state.handlers;
#if defined(GBTG_INSPECTOR_APP)
    if (state.game.inspection != nullptr)
    {
        state.game.inspection->owners.clear();
    }
#endif
    for (size_t kind = 0; kind < handlers.handlers.size(); ++kind)
    {
        if (handlers.handlers[kind])
        {
            handlers.handlers[kind] = [&state, kind, handler = handlers.handlers[kind]](u8* record)
            {
                u16 id;
                std::memcpy(&id, record + 6, 2);
#if defined(GBTG_INSPECTOR_APP)
                if (auto* inspection = state.game.inspection; inspection != nullptr)
                {
                    const size_t before = state.scene.objects.size();
                    const SceneInspectionMarker marker{u16(kind), id};
                    if (inspection->run_marker(marker, [&]()
                        {
                            handler(record);
                        }))
                    {
                        for (size_t index = before; index < state.scene.objects.size(); ++index)
                        {
                            inspection->owners[state.scene.find_address(state.scene.objects[index].bytes.get())] = marker;
                        }
                    }
                    return;
                }
#endif
                try
                {
                    handler(record);
                }
                catch (const std::exception& error)
                {
                    throw Error("marker kind " + std::to_string(kind) + " id " + std::to_string(id) + ": "
                        + error.what());
                }
            };
        }
    }
    // A marker's scene effect emitter (factory type 0x97) ticks its effect once as it starts (0x000EF2D0); the
    // frame's scratch stands for this pass's incoming stack bytes as it does for update_scene_actors.
    auto* scratch = state.effect_pass_scratch;
    state.effect_pass_scratch = &state.frame_effect_scratch;
    try
    {
        start_runtime_scene_markers(state.markers, randomizer, handlers);
    }
    catch (...)
    {
        state.effect_pass_scratch = scratch;
        throw;
    }
    state.effect_pass_scratch = scratch;
}

bool SceneActorServices::activation_pending() const
{
    return state_->live_actors.next != nullptr;
}

std::span<const u32> SceneActorServices::active_actor_addresses() const
{
    return state_->scene_nodes;
}

const u8* SceneActorServices::marker_records() const
{
    return static_cast<const u8*>(state_->script_records);
}

void SceneActorServices::restart_marker_player()
{
    restart_runtime_marker_player(marker_records(), state_->handlers); // 0x00127FF0
}

u32 SceneActorServices::find_actor_by_id(u16 key) const
{
    auto& state = *state_;
    return find_runtime_actor_by_id(key,
        [&state](const entity_detail::LatchNode& node) -> u32
        {
            for (const auto& [address, latch] : state.active_nodes)
            {
                if (latch.get() == &node)
                {
                    return address;
                }
            }
            unbound("DAT_00511AA4 node with no scene actor");
        },
        [&state](u32 address)
        {
            return state.scene.resolve_address(address, 0x200);
        });
}

// A button bat (type 0x15, 0x0013B580) the scene drives: its vtable +0x10 joins the actor updates. Its animation
// events reach its vtable +0xC through the actor owner's 0x0003B3C0 handler.
void SceneActorServices::State::bind_button_bat(FactoryObject& object)
{
    auto& view = actors->view(object);
    // The bat's vtable +0x10 (getter 0x0006D4A0) is 0x0006D5C0.
    actor_updates[&view.bat.actor] = [this](Actor& actor)
    {
        const auto view = bat_views(&actor);
        update_button_bat(view.bat, view.frame, view.context);
    };
}

NativePlayerInstance& SceneActorServices::State::case_player(u32 address)
{
    for (auto* player : players->instances())
    {
        if (player->actor_address == address)
        {
            return *player;
        }
    }
    throw Error("no native player at actor address " + std::to_string(address));
}

void SceneActorServices::queue_aid_spawns(const char* name)
{
    load_aid_spawn_queue(name, state_->resources.registry, state_->resources.error); // 0x000F4E50
}

// Xbox 0x00122CA0
void SceneActorServices::message_tagged_actors(u16 tag, std::span<const u32> message)
{
    auto& state = *state_;
    for (entity_detail::LatchNode* node = state.latches; node != nullptr;)
    {
        entity_detail::LatchNode* next = node->next;
        if (node->tag == tag)
        {
            u32 address = 0;
            for (const auto& [owner, latch] : state.active_nodes)
            {
                if (latch.get() == node)
                {
                    address = owner;
                }
            }
            if (address == 0)
            {
                unbound("DAT_00511AA4 node with no scene actor");
            }
            send_actor_message(address, message);
        }
        node = next;
    }
}

// Xbox 0x00112C50
void SceneActorServices::create_template_actor(const char* name, const Vec3& position, const Vec3& euler,
    float scale)
{
    auto& state = *state_;
    FactoryObject object = create_runtime_actor_at(name, &position, &euler, scale, state.image.pools,
        state.scene.factory, state.origin, state.default_euler);
    if (object.bytes != nullptr)
    {
        state.scene.keep_object(std::move(object));
    }
}

// Xbox 0x001163D0
void SceneActorServices::release_actor_snapshots()
{
    auto& state = *state_;
    if (state.snapshots == nullptr)
    {
        unbound("actor snapshot release before the scene's draw contexts");
    }
    release_runtime_actor_snapshots(*state.snapshots, state.decal_enable);
}

// Xbox 0x001162F0
void SceneActorServices::age_actor_snapshots()
{
    auto& state = *state_;
    if (state.snapshots == nullptr)
    {
        unbound("actor snapshots before the scene's draw contexts");
    }
    age_runtime_actor_snapshots(*state.snapshots);
}

// Xbox 0x000FDDA0
void SceneActorServices::animate_light_records()
{
    auto& state = *state_;
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: the lights flicker on frame-counter masks with a random roll each; they animate on retail ticks.
    if (!rare_replay_retail_tick())
    {
        return;
    }
    animate_runtime_light_records(state.shaders, rare_replay_retail_frame(state.game.clock.frame),
        state.game.clock.time, state.random);
#else
    animate_runtime_light_records(state.shaders, state.game.clock.frame, state.game.clock.time, state.random);
#endif
}

// Xbox 0x0011B7F0, from the frame tick after the drift moves on.
void SceneActorServices::rebuild_refraction_quad()
{
    build_runtime_refraction_quad(g_scene_refraction, state_->hud_size[0], state_->hud_size[1]);
}

void SceneActorServices::tick_post_models()
{
    RuntimePostModelContext context = state_->post_model_context();
    tick_runtime_post_models(context);
}

RuntimeIntroNoticeTick SceneActorServices::State::intro_notice_tick()
{
    return RuntimeIntroNoticeTick{g_script_e794, clock, prompt_owner, *dialogue,
        [](u8 mask, i32 release)
        {
            g_actor_pause(mask, release); // 0x00117400
        }};
}

// Xbox 0x0013FD60
void SceneActorServices::tick_intro_notice()
{
    RuntimeIntroNoticeTick context = state_->intro_notice_tick();
    tick_runtime_intro_notice(context);
}

// Xbox 0x0013FFD0
void SceneActorServices::finish_intro_notice()
{
    RuntimeIntroNoticeTick context = state_->intro_notice_tick();
    finish_runtime_intro_notice(context);
}

// Xbox 0x0012A9B0
void SceneActorServices::clear_post_models()
{
    RuntimePostModelContext context = state_->post_model_context();
    clear_runtime_post_models(context);
}

// Xbox 0x0012AA50
void SceneActorServices::set_post_model(const char* name)
{
    RuntimePostModelContext context = state_->post_model_context();
    set_runtime_post_model(name, context);
}

// Xbox 0x00049DE0
void SceneActorServices::tick_frontend_state()
{
    tick_runtime_challenge(*state_->challenge_services);
}

void SceneActorServices::show_script_scene_name(const char* key, u32 show, i32 repeated, float delay, float lifetime)
{
    auto& state = *state_;
    char16_t* name = state.wide_scratch.data(); // DAT_00507E78
    name[0] = 0;
    if (request_namespaced_localtext(state.image.string(0x0032A5AC), state.image.string(0x00324A9C), key, name,
        state.resources.text) == 0) // 0x000FF0A0, "scenenames"
    {
        return;
    }
    if (name[0] == 0) // 0x000FBE10
    {
        return;
    }
    if (show != 0 && repeated == 0)
    {
        if (state.script_title_pool == nullptr)
        {
            allocate_dialogue_text_pool(state.script_title_pool, 0, 0, 0x5E4, state.text_registry); // 0x0005F390
            state.random = state.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
            state.script_title_pool->origin = {read_f32(state.data(0x0032B20C, 4), 0),
                read_f32(state.data(0x0032B0F4, 4), 0), std::bit_cast<float>(state.random)};
        }
        RuntimeDialogueTextPool& pool = *state.script_title_pool;
        create_dialogue_text_record(pool, u"spam", -100.f, -100.f, 0.1f, 0.1f, delay, 0, 0, 0, state.resources.metrics,
            state.widget_context.viewport_width, state.next_widget_id, state.random); // 0x0005F550, EAX 0
        const float line = runtime_text_line_height(state.resources.metrics, 0,
            read_f32(state.data(0x0032B110, 4), 0)); // 0x000606D0, ECX 0
        const float half = read_f32(state.data(0x0032B0F4, 4), 0);
        create_dialogue_text_record(pool, name, float(state.render_width) * half,
            (float(state.render_height) - line) * half, 1.5f, 1.5f, lifetime, 1, 0, 0, state.resources.metrics,
            state.widget_context.viewport_width, state.next_widget_id, state.random); // 0x0005F580 into 0x0005F5C0
    }
    name_scene(name); // 0x0011DCF0, EAX DAT_00507E78
}

AudioListenerSettings& SceneActorServices::audio_listener()
{
    return state_->listener;
}

void SceneActorServices::set_reverb_preset(i32 preset)
{
    auto& state = *state_;
    set_audio_reverb_preset(state.listener, preset, state.resources.registry, state.resources.error); // 0x0003A1F0
}

void SceneActorServices::load_camera_shake(const char* name, const Vec3& position, float scale)
{
    auto& state = *state_;
    load_actor_camera_shake(name, position, scale, state.resources.registry, state.resources.error); // 0x000FB120
}

void SceneActorServices::load_camera_effect(const char* name, const Vec3& position)
{
    auto& state = *state_;
    load_actor_camera_effect(name, &position, state.resources.registry, state.resources.error); // 0x000493D0
}

void SceneActorServices::apply_scene_lighting(std::span<const u8> record)
{
    auto& state = *state_;
    apply_runtime_scene_lighting(record, state.fog, [&state]()
    {
        configure_runtime_scene_fog(state.model_renderer, state.fog); // 0x000FD5C0
    }); // 0x000FD780
}

void SceneActorServices::set_background_morphs_playing(u8 playing)
{
    set_runtime_background_morphs_playing(state_->background_morphs, playing); // 0x00114050
}

void SceneActorServices::clear_script_tag_tables()
{
    auto& state = *state_;
    state.scale_exclusions = {};
    state.scale_exclusion_count = 0; // DAT_0050E1B0
    state.animation_classes = {};
    state.animation_class_count = 0; // DAT_0050E16C
    state.mind_class_rows = {};
    state.mind_settings.count = 0; // DAT_0050E148
}

void SceneActorServices::add_scale_exclusion(u16 tag)
{
    auto& state = *state_;
    state.scale_exclusions.at(size_t(state.scale_exclusion_count)) = tag; // DAT_0050E1B4 + DAT_0050E1B0 * 2
    ++state.scale_exclusion_count;
}

void SceneActorServices::add_animation_class(u16 tag, float scale)
{
    auto& state = *state_;
    const size_t row = size_t(state.animation_class_count) * 8;
    if (state.animation_classes.size() < row + 8)
    {
        throw Error("0x0010F660 writes past DAT_0050E170's eight rows");
    }
    std::memcpy(state.animation_classes.data() + row, &tag, 2);
    std::memcpy(state.animation_classes.data() + row + 4, &scale, 4);
    ++state.animation_class_count; // DAT_0050E16C
}

void SceneActorServices::add_mind_class_setting(u16 tag, float value)
{
    auto& state = *state_;
    const size_t row = size_t(state.mind_settings.count) * 4;
    if (state.mind_class_rows.size() < row + 4)
    {
        throw Error("0x0010F690 writes past DAT_0050E14C's eight rows");
    }
    const u16 word = u16(truncate_runtime_float(value)); // 0x001673D8
    std::memcpy(state.mind_class_rows.data() + row, &tag, 2);
    std::memcpy(state.mind_class_rows.data() + row + 2, &word, 2);
    ++state.mind_settings.count; // DAT_0050E148
}

void SceneActorServices::release_script_title_pool()
{
    auto& state = *state_;
    RuntimeDialogueTextPool* pool = state.script_title_pool;
    if (pool != nullptr && pool->allocation->cursor <= pool->allocation->begin)
    {
        release_dialogue_text_pool(pool, *state.text_registry); // 0x0005F460
        state.script_title_pool = nullptr;
    }
}

void SceneActorServices::free_script_title_pool()
{
    auto& state = *state_;
    if (state.script_title_pool != nullptr)
    {
        release_dialogue_text_pool(state.script_title_pool, *state.text_registry); // 0x0005F460
        state.script_title_pool = nullptr;
    }
}

// Xbox 0x000F2BE0
void SceneActorServices::add_profile_scene_times(bool replay, float scene_time)
{
    auto& times = g_runtime_profile.times;
    if ((g_scene_flags->bytes[0] & 2) != 0)
    {
        times[0] = times[0] + i32(scene_time); // DAT_00506C78, DAT_004312D0
        if (!replay)
        {
            if (state_->measurement == nullptr)
            {
                unbound("0x000F2BE0's play counts before the dialogue measurement exists");
            }
            times[1] = times[1] + state_->measurement->first_additional; // DAT_00506C7C, DAT_00545970
            times[3] = times[3] + state_->measurement->third_additional; // DAT_00506C84, DAT_00545974
        }
    }
}

// Xbox 0x0004A4D0
void SceneActorServices::set_frontend_state(i32 next)
{
    set_runtime_challenge_state(next, *state_->challenge_services);
}

// Xbox 0x00049FF0
void SceneActorServices::release_challenge_frontend()
{
    end_runtime_challenge(*state_->challenge_services);
}

void SceneActorServices::clear_score_awards()
{
    clear_runtime_score_awards(*state_->score); // 0x0012DF90
}

void SceneActorServices::release_scene_dialogue()
{
    auto& state = *state_;
    auto& dialogue = state.dialogue_state;
    release_dialogue_speaker(state.dialogue_owned, state.resources.registry, 0, state.dialogue->mode_flags,
        state.dialogue->scene_flags); // 0x00057400
    release_dialogue_icon(state.dialogue_owned, state.resources.registry); // DAT_004E60C4, DAT_004E60C0 0x36
    if (dialogue.input.data() != nullptr) // DAT_004EE1F8
    {
        switch (read_u32(dialogue.input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(dialogue.input); // 0x00107DD0
            break;
        case 4:
            release_runtime_composite_input(dialogue.input, state.input_release); // 0x00107960
            break;
        case 5:
            release_runtime_actor_input(dialogue.input, state.input_release); // 0x00107B60
            break;
        default:
            break;
        }
        state.input_addresses.erase(dialogue.input_address);
        state.input_views.erase(dialogue.input_address);
        dialogue.input = {};
        dialogue.input_address = 0;
    }
    if (dialogue.widget != nullptr) // DAT_004EE2A8
    {
        destroy_dialogue_widget(dialogue.widget, state.resources.registry, *state.text_registry); // 0x00142070
        dialogue.widget = nullptr;
    }
}

void SceneActorServices::collect_model_releases()
{
    auto& state = *state_;
    collect_runtime_model_releases(state.deferred, state.fence, state.resources.registry, state.resources.blendshapes,
        state.resources.model_heap, state.resources.d3d_boundaries); // 0x000F9780
}

void SceneActorServices::flush_model_releases()
{
    auto& state = *state_;
    flush_runtime_model_releases(state.deferred, state.fence, state.resources.registry, state.resources.blendshapes,
        state.resources.model_heap, state.resources.d3d_boundaries); // 0x000F97F0
}

void SceneActorServices::refresh_master_pad()
{
    auto& state = *state_;
    refresh_runtime_input_device(state.game.native_input->master_address, state.game.frame, state.input_activation,
        state.input_release); // 0x00109470
}

ScenePauseServices& SceneActorServices::pause_services()
{
    return *state_->pause_services;
}

bool SceneActorServices::start_pressed()
{
    auto& state = *state_;
    const GhoulySpawnPool* devices = state.input_activation.entries; // DAT_0050E0E8, the live input stack.
    if ((devices->cursor - devices->begin) / devices->stride == 0)
    {
        return false;
    }
    const u8* row = devices->begin + (state.input_selected - 1) * devices->stride; // DAT_0050E0EC
    const u32 device = read_u32({row, devices->stride}, 0);
    if (device == 0)
    {
        return false;
    }
    state.input_state.devices = devices;
    state.input_state.selected = state.input_selected;
    if (const auto view = state.input_views.find(device); view != state.input_views.end())
    {
        return runtime_input_button_edge(view->second, 0x10, state.input_state) != 0; // 0x001083F0
    }
    const auto record = state.input_addresses.find(device); // An activation entry's own device record.
    if (record == state.input_addresses.end())
    {
        unbound("0x001083F0 on a DAT_0050E0E8 device with no native view");
    }
    const RuntimeDialogueInputDevice view{record->second, {nullptr, nullptr, nullptr, nullptr}};
    return runtime_input_button_edge(view, 0x10, state.input_state) != 0; // 0x001083F0
}

void SceneActorServices::count_script_challenge()
{
    auto& state = *state_;
    ++state.challenge_count; // DAT_00506C84
    show_runtime_bonus_counter(1, i32(state.challenge_count), *state.bonus_counters,
        *state.scoreboard_resources); // 0x0012D7E0, EDI 1
}

void SceneActorServices::activate_actor(u32 address)
{
    const auto found = actor_addresses().find(address);
    if (found == actor_addresses().end() || found->second == nullptr)
    {
        unbound("activated address that is not a scene actor");
    }
    activate_actor_bind_tree(*found->second);
}

void SceneActorServices::release_script_player_weapon()
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    auto& instance = state.players->player(state.object_of_actor(*player));
    instance.refresh_views();
    replace_runtime_player_attachment(instance.attachment); // 0x0013A7C0
    reset_runtime_meter(*state.scoreboard_update); // 0x0012ECC0
}

void SceneActorServices::drop_script_player_weapon()
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    auto& instance = state.players->player(state.object_of_actor(*player));
    instance.refresh_views();
    if (drop_runtime_player_attachment(instance.attachment)) // 0x0013A860
    {
        reset_runtime_meter(*state.scoreboard_update); // 0x0012ECC0
    }
}

void SceneActorServices::give_script_player_weapon(const char* name)
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    auto& instance = state.players->player(state.object_of_actor(*player));
    instance.refresh_views();
    FactoryObject weapon = create_runtime_player_attachment(name, 1, instance.attachment_creation); // 0x0013A720
    state.attaching_object = nullptr;
    state.attaching_bytes = nullptr;
    if (weapon.bytes != nullptr)
    {
        state.scene.keep_object(std::move(weapon));
    }
}

Actor* SceneActorServices::first_latched_actor(u16 tag) const
{
    auto& state = *state_;
    for (auto* node = state.latches; node != nullptr; node = node->next)
    {
        if (node->tag != tag)
        {
            continue;
        }
        for (const auto& [address, latch] : state.active_nodes)
        {
            if (latch.get() == node)
            {
                return state.actors->actors.at(state.scene.resolve_address(address, 0).data());
            }
        }
        unbound("DAT_00511AA4 node with no scene actor");
    }
    return nullptr;
}

void SceneActorServices::set_tagged_actor_model_nodes(u16 tag, u8 slot, u8 value)
{
    auto& state = *state_;
    for (const auto& mark : state.marked_actors())
    {
        if (read_u16(mark.bytes, 6) != tag)
        {
            continue;
        }
        Actor& actor = *mark.actor;
        if (actor.model != nullptr && actor.model->runtime != nullptr)
        {
            set_runtime_model_node_state(*actor.model->runtime, slot, value, state.node_cache); // 0x000F96F0
        }
        if (auto* secondary = state.actors->view(state.object_of_actor(actor)).startup.models.secondary.get())
        {
            set_runtime_model_node_state(*secondary, slot, value, state.node_cache); // 0x000F96F0
        }
    }
}

void SceneActorServices::set_body_list_model_nodes(u8 slot, u8 value)
{
    auto& state = *state_;
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.body_a.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next)) // DAT_00511B20, each node's +4.
    {
        Actor* actor = nullptr;
        if (auto* background = state.backgrounds->owner_of(*node); background != nullptr)
        {
            actor = &background->state.actor;
        }
        for (auto* player : state.players->instances())
        {
            if (static_cast<void*>(&player->links[0]) == static_cast<void*>(node))
            {
                actor = &player->actor;
            }
        }
        if (actor == nullptr)
        {
            unbound("DAT_00511B20 node with no native background or player");
        }
        // Node -0x730 (+0x204) and node -0x624 (+0x310); a null object skips its call.
        if (actor->model != nullptr && actor->model->runtime != nullptr)
        {
            set_runtime_model_node_state(*actor->model->runtime, slot, value, state.node_cache); // 0x000F96F0
        }
        if (auto* secondary = state.actors->view(state.object_of_actor(*actor)).startup.models.secondary.get())
        {
            set_runtime_model_node_state(*secondary, slot, value, state.node_cache); // 0x000F96F0
        }
    }
}

void SceneActorServices::retire_tagged_actors(u16 tag)
{
    auto& state = *state_;
    for (const auto& mark : state.marked_actors())
    {
        if (read_u16(mark.bytes, 6) == tag)
        {
            activate_actor_bind_tree(*mark.actor); // The inlined 0x00112CC0.
        }
    }
}

void SceneActorServices::change_script_shape_events(u8 category, bool set)
{
    auto& state = *state_;
    PhysicalShapeEventContext events{0, 0, 0,
        [set](const u8* event, void* user)
        {
            const std::span<u8> bytes{const_cast<u8*>(event), 0x14};
            const u32 mask = u32(reinterpret_cast<uintptr_t>(user));
            return set ? set_physical_shape_event_bits(bytes, mask) : clear_physical_shape_event_bits(bytes, mask);
        }, reinterpret_cast<void*>(uintptr_t(4)), state.resources.shape_lists};
    visit_scene_script_shape_events(entity_detail::g_world.segment, category,
        [&state](const entity_detail::SegmentProbe& probe) -> const Actor&
        {
            return state.actors->view(probe).bat.actor;
        }, events); // 0x0012C8A0
}

// Xbox 0x0013D250
void SceneActorServices::place_scene_camera(const Vec3& position, const Vec3& euler)
{
    auto& state = *state_;
    for (auto* node = state.latches; node != nullptr; node = node->next)
    {
        u32 address = 0;
        for (const auto& [node_address, latch] : state.active_nodes)
        {
            if (latch.get() == node)
            {
                address = node_address;
                break;
            }
        }
        if (address == 0)
        {
            unbound("DAT_00511AA4 node with no scene actor");
        }
        const auto bytes = state.scene.resolve_address(address, 0);
        Actor& actor = *state.actors->actors.at(bytes.data());
        if (actor.class_key != 6)
        {
            continue;
        }
        actor_set_world_position(actor, position); // 0x00041730
        normalize_euler_angles(actor.euler, euler); // 0x00105AA0 into actor +0x14
        actor.xform_valid = 0; // Actor +0x2A8.
        std::array<u32, 0x42> message{3};
        state.object_message(*state.object_of(bytes.data()), message);
        return;
    }
}

// The DAT_00511B40 player's 0x0002B430 / 0x0002B560: its world position and euler, and its body's +0x18 / +0x72C.
void SceneActorServices::place_script_player(const Vec3& position, const Vec3& euler)
{
    auto& state = *state_;
    const u32 link = player_link();
    if (link == 0)
    {
        unbound("script player placement without DAT_00511B40");
    }
    auto& player = state.players->player_at(link - 0x93C);
    player.refresh_views();
    actor_set_world_position(player.actor, position); // 0x00041730
    const Vec3 world = actor_world_position(player.actor); // Body +8 is the player.
    std::memcpy(player.body.data() + 0x18, &world, sizeof(world));
    set_runtime_player_actor_rotation(player.actor, player.actor, player.body, euler); // 0x0002B560
}

void SceneActorServices::start_pending_ghoul_intro()
{
    if (state_->notices.pending == 1)
    {
        start_runtime_ghoul_intro(*state_->ghoul_intro_context); // 0x000F4A00
    }
}

RuntimeBackgroundClothScene& SceneActorServices::State::background_cloth_scene()
{
    if (cloth_scene != nullptr)
    {
        return *cloth_scene;
    }
    cloth_context = std::make_unique<RuntimeBackgroundClothContext>(RuntimeBackgroundClothContext{
        [this](const void* block, size_t size)
        {
            return heap_address(block, size);
        },
        [this]() -> const Actor*
        {
            const auto listed = case_actors(); // DAT_00511B40.
            return listed.empty() ? nullptr : &listed.front().actor;
        },
        [this](const void* node) -> const Actor*
        {
            for (const auto& [bytes, native] : actors->actors)
            {
                FactoryObject* object = object_of(bytes);
                if (object != nullptr && &actors->view(*object).render.wide == node)
                {
                    return native;
                }
            }
            unbound("DAT_00511AFC node of no scene actor");
        },
        entity_detail::g_world.wide.next,
        [this](u32 address, size_t size)
        {
            return resources.memory.resolve(address, size);
        },
        [this](const Actor& actor, u8* output)
        {
            for (const auto& [bytes, native] : actors->actors)
            {
                if (native != &actor)
                {
                    continue;
                }
                FactoryObject* object = object_of(bytes);
                auto& view = actors->view(*object);
                build_runtime_actor_bound(view.bat.actor, output, 0xFFFFFFFFu, false,
                    reinterpret_cast<GhoulySpawnPool*&>(view.allocation_8fc)); // 0x000426F0, actor +0x8FC
                return;
            }
            unbound("0x000426F0 bound of an actor outside the scene");
        },
        origin, g_scene_hold_time,
        [this]() -> Vec3
        {
            if (display_view == nullptr)
            {
                throw Error("background cloth specular shading has no DAT_005077A8 display view");
            }
            return display_view->position; // DAT_005077A8 +0.
        }});
    cloth_scene = std::make_unique<RuntimeBackgroundClothScene>(RuntimeBackgroundClothScene{background_cloths,
        background_morphs, cloth_ramp_texture, *cloth_context,
        [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)
        {
            return snapshot_d3d.create_texture(width, height, levels, usage, format, pool); // D3D 0x00177840
        },
        [this](u32 texture) -> u8*
        {
            return resources.memory.pointer(resources.d3d.resources.at(texture).data); // D3DTexture_LockRect
        },
        [this](u32 texture)
        {
            resources.d3d.resources.erase(texture); // D3DResource_Release
            created_textures.erase(texture);
        },
        [this]() -> u32
        {
            auto* first = backgrounds->first_background();
            if (first == nullptr || first->state.actor.model == nullptr || first->state.actor.model->runtime == nullptr
                || first->state.actor.model->runtime->source == nullptr)
            {
                return 0;
            }
            return resources.memory.address_of(first->state.actor.model->runtime->source->cpu.data());
        },
        [this]() -> std::span<const float, 42>
        {
            return std::span<const float, 42>(display_view->frustum);
        },
        [this]() -> u8&
        {
            return display_view->mark; // +0x1E0.
        },
        voices, g_amb_color_lo, g_amb_color_hi});
    return *cloth_scene;
}

void SceneActorServices::tick_background_cloths(float step)
{
    tick_runtime_background_cloths(step, state_->background_cloth_scene()); // 0x00115BB0
}

void SceneActorServices::configure_scene_fog()
{
    configure_runtime_scene_fog(state_->model_renderer, state_->fog); // 0x000FD5C0
}

void SceneActorServices::update_ghoul_intro()
{
    update_runtime_ghoul_intro(*state_->ghoul_intro_context); // 0x000F4560
}

std::vector<RuntimeDirectorActor> SceneActorServices::marked_actors()
{
    return state_->marked_actors();
}

u32 SceneActorServices::player_link() const
{
    return state_->cases.next == nullptr ? 0 : state_->address_of(state_->cases.next);
}

const std::unordered_map<u32, Actor*>& SceneActorServices::actor_addresses() const
{
    auto& state = *state_;
    state.addresses.clear();
    for (const auto& [bytes, actor] : state.actors->actors)
    {
        state.addresses[state.scene.address_of(bytes)] = actor;
    }
    return state.addresses;
}

void SceneActorServices::clear_scene_light_cones()
{
    auto& state = *state_;
    clear_runtime_light_cones(state.light_cones, [&state](std::span<const u8> texture)
    {
        release_animation_resource(state.resources.registry, texture); // 0x00035C00
    }); // 0x0011AEA0
}

void SceneActorServices::run_script_effect_stream(const char* name)
{
    load_actor_effect_stream(name, nullptr, state_->effects); // 0x00053170
}

void SceneActorServices::reset_scene_score_words()
{
    auto& state = *state_;
    if (state.measurement != nullptr)
    {
        state.measurement->first_additional = 0; // DAT_00545970
        state.measurement->third_additional = 0; // DAT_00545974
    }
    state.score_total = 0; // DAT_0054597C
}

void SceneActorServices::release_script_player_attachment()
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    auto& instance = state.players->player(state.object_of_actor(*player));
    instance.refresh_views();
    release_runtime_player_attachment(0, 0, instance.attachment); // 0x0001B810, EAX the player's +0x95C
    activate_actor_bind_tree(*player); // 0x00112CC0, ESI the player
}

void SceneActorServices::send_player_through_exit(const Vec3& position, const Vec3& euler)
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // DAT_00511B40 - 0x93C
    if (player == nullptr)
    {
        return;
    }
    FactoryObject& object = state.object_of_actor(*player);
    auto& instance = state.players->player(object);
    instance.refresh_views();
    if (read_u32(instance.attachment.body, 0x48) != 0) // The +0x95C body's held actor.
    {
        RuntimeBodyAttachedActor* held = instance.attachment.actions.attached;
        if (held == nullptr)
        {
            unbound("the player's held actor (body +0x48) has no view");
        }
        const u32 leaving = 1;
        std::memcpy(held->bytes.data() + 0x9A4, &leaving, 4);
        let_go_runtime_player_attachment(instance.attachment); // 0x0001B6B0, EAX the player's +0x95C
    }
    std::array<u32, 9> message{};
    clear_runtime_goal_message(message); // 0x00049DB0: message 0x4D
    message[1] = 8; // The goal kind.
    std::memcpy(&message[2], &position, sizeof(Vec3));
    std::memcpy(&message[5], &euler, sizeof(Vec3));
    state.object_message(object, message); // Player vtable +0xC
}

void SceneActorServices::hold_script_player_attachments(u32 release)
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    auto& live = state.actors->view(state.object_of_actor(*player)).live_attachments; // Actor +0x8F4.
    if (release == 0)
    {
        hold_actor_live_attachments(live); // 0x00042090, EAX the player
    }
    else
    {
        unhold_actor_live_attachments(live); // 0x000420B0, EAX the player
    }
}

void SceneActorServices::point_script_player_at_nearest()
{
    auto& state = *state_;
    Actor* player = find_runtime_script_player(player_link(), actor_addresses()); // 0x001292D0
    if (player == nullptr)
    {
        return;
    }
    std::array<u32, 0x42> message{0x37, 0, 0, 0};
    float distance = 0.f;
    if (const Actor* nearest = find_nearest_runtime_script_actor(nullptr, *player, 0, 0.f, &distance, 0x4240, nullptr,
            state.query)) // 0x00128DF0
    {
        const Vec3 position = actor_world_position(*nearest); // 0x000412A0, ESI the actor, EDI message +4
        message[1] = std::bit_cast<u32>(position.x);
        message[2] = std::bit_cast<u32>(position.y);
        message[3] = std::bit_cast<u32>(position.z);
    }
    send_actor_message(state.address_of(player), message); // Vtable +0xC
}

int SceneActorServices::wait_script_player() const
{
    return wait_runtime_script_player(player_link(), actor_addresses(), state_->query);
}

std::pair<u16, u16> SceneActorServices::player_action_types(const Actor& actor, std::span<const u8>* second) const
{
    for (auto* player : state_->players->instances())
    {
        if (&player->actor == &actor)
        {
            if (second != nullptr)
            {
                const auto& action = player->body_selection.second;
                *second = action.bytes == nullptr ? std::span<const u8>()
                    : std::span<const u8>(action.bytes.get(), action.size);
            }
            return {player->body_selection.first.type, player->body_selection.second.type};
        }
    }
    return {0, 0};
}

size_t SceneActorServices::player_total() const
{
    return state_->players->player_total();
}
} // namespace gbtg
