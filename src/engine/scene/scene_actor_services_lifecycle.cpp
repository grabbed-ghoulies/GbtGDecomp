#include "scene_actor_services_state.h"
#include "../entities/body_fields.h"
#include "../entities/player_fields.h"

// Scene begin and end, teardown, and the transition, pause, book and game-select service bindings.
namespace gbtg
{
void SceneActorServices::State::begin_scene()
{
    if (in_scene)
    {
        throw Error("the scene services begin a scene while one is running");
    }
    if (!first_scene)
    {
        reset_scene_data();
    }
    first_scene = false;
    const std::string& name = game.scene_script.name;
    if (name.size() >= scene_name.size())
    {
        throw Error("scene name runs past the scene record's 0x80 bytes");
    }
    scene_name.fill(0);
    std::memcpy(scene_name.data(), name.c_str(), name.size());
    // The startup (0x0011CBD0) loads the markers through 0x0011CB80 from this record.
    scene.released = released_binding;
    in_scene = true;
}

// The scene globals a scene starts from: what 0x0011C5A0 and 0x0011C480 clear, and the per-scene
// storage the services hold for the scene's actors, backgrounds, doors and lights.
void SceneActorServices::State::reset_scene_data()
{
    effect_collision = {};
    effect_callback_result = 0;
    effect_sound_frame = 0;
    frame_effect_scratch = {};
    scare_flags = 0;
    collision_push_contacts = {};
    body_slide_contacts = {};
    port_devices = {};
    pulse = {};
    scene_nodes.clear();
    scene_controls = nullptr;
    powerup_rows = {};
    powerup_board = nullptr;
    scared_count = 0;
    scared_actors = {};
    scared_times = {};
    excluded_count = 0;
    navigation_nodes = nullptr;
    excluded_tags = {};
    player_count = 0;
    // DAT_0050E148, DAT_0050E16C and DAT_0050E1B0 clear in the teardown (clear_script_tag_tables).
    camera_players = 0;
    // The pause counts (DAT_0050E764, DAT_0050E768) are 0x00116E00's to clear, before the scene loads
    // (reset_scene_script_runtime, 0x00116E00). A clear here ran after the loading scene's first pause and lost it.
    pause_nodes.clear();
    first_pause = nullptr;
    for (const auto& [address, view] : weapon_pickups)
    {
        if (view->pose)
        {
            attachment_actors.erase(&view->pose->object);
        }
    }
    weapon_pickups.clear();
    released_positions.clear();
    weapon_attachments.clear();
    // decay_index is DAT_0051248C, the script's title mode: only 0x001287F0 clears it (script_set_default_name).
    // A clear here ran after a restarted scene's first title, so its next title with the same id looked active.
    targets = nullptr;
    goal_scratch.clear();
    // brews_taken is the profile's DAT_00506C88: only a game's start or end clears it (0x0011EA50, 0x0011EAD0).
    brew_accepted = 0;
    difficulty = 0;
    intro_pending = 0;
    intro_name = {};
    // The queue and class limits are 0x00116DA0's, which 0x00116E00 runs before the script loads; clearing them
    // here, at the actor start, would undo the limits the script's head (handler 2) has just written.
    notice_pending = 0;
    notice_first = nullptr;
    notice_id = 0;
    notice_queue = {};
    notice_count = 0;
    notice_player = nullptr;
    brew_state = 0;
    brew_intensity = 0.f;
    brew_counter = {};
    health_active = 0;
    health_intensity = 0.f;
    health_counter = {};
    health_mode = 0;
    area_dialogue = nullptr;
    message_references.clear();
    emitter_players.clear();
    emitter_player_views.clear();
    emitters = {};
    room = {};
    background_regions = {};
    background_region_count = 0;
    sector_grids = {};
    sector_count = 0;
    sector_minimum = 0.f;
    sector_maximum = 0.f;
    sector_saturation = 0.f;
    sector_low = 0.f;
    sector_high = 0.f;
    disable_framebuffer_clear = 0;
    part_sound_slot = 0;
    broken_parts = 0;
    sampling_scratch = {};
    sampling_active = nullptr;
    door_rows.clear();
    door_camera_registered = 0;
    door_paused = 0;
    std::destroy_at(&navigation); // Its grids and node buffer go with the scene.
    std::construct_at(&navigation);
    replacement_source = {};
    replacement_target = {};
    listener_position = {};
    movie_flag_38 = 0;
    movie_flag_3c = 0;
    frontend_pause = 0;
    ambient_points = {};
    ambient_points_enabled = 0;
    ambient_point_free = 0;
    ambient_point_draw = 0;
    ambient_point_fc = 0;
    ambient_point_100 = 0;
    ambient_scale = {};
    environment_world_space = 0;
    light_records = {};
    scene_color = {};
    directional_light = {};
    temporary_lights = {};
    mirrors = {};
    mirror_pass = 0;
    // The light cones are cleared by the teardown's 0x0011AEA0 (clear_scene_light_cones).
    hud_enabled = 0;
    camera_start = {};
}

// The start of 0x0011C5A0, before the scene controls go: an open dialogue closes (0x000584D0(0)), the scene
// cutscenes go (0x0004ECE0) and the doors' cutscene registrations (0x0005B1E0).
void SceneActorServices::State::begin_teardown()
{
    if (!in_scene)
    {
        return;
    }
    if (script_dialogue != nullptr && script_dialogue->dialogue.state.mode != 0)
    {
        close_runtime_dialogue(script_dialogue->dialogue);
    }
    if (auto* stack = find_scene_cutscene_runtime(game); stack != nullptr)
    {
        release_scene_cutscenes(*stack);
    }
    RuntimeDoorRelease door_release{*doors, door_camera_registered, door_paused, active_strategies,
        door_camera_hold, g_flags_ea9c};
    release_runtime_doors(door_release);
}

// 0x0011C5A0's 0x00122C70: message 0x80 to every active actor (DAT_00511AA4, the +0x900 node's vtable +0xC), which
// retires it into the child list; 0x001232D0 then takes the retired actors out of the scene's lists. A retired
// actor is not freed: a cutscene that outlives the scene (a transition's book) still ticks and draws its own.
ActorRenderRegistration& SceneActorServices::State::super_scary_render(u32 address)
{
    for (const auto& [bytes, actor] : actors->actors)
    {
        if (scene.address_of(bytes) == address && actor != nullptr && actor->render_registration != nullptr)
        {
            return *actor->render_registration;
        }
    }
    unbound("Super Scary challenge renders an address that is not a registered scene actor");
}

void SceneActorServices::State::retire_actors()
{
    if (!in_scene || actors == nullptr)
    {
        return;
    }
    Message retire;
    retire.id = 0x80;
    for (entity_detail::LatchNode* node = latches; node != nullptr;)
    {
        entity_detail::LatchNode* next = node->next;
        node->handle_message(*node, retire);
        node = next;
    }
    if (entity_detail::g_world.child.next != nullptr)
    {
        remove_retired_actors(); // 0x001232D0
    }
}

// The scene's part of 0x0011C5A0, after its actors retired and the factory released the scene's objects.
void SceneActorServices::State::end_scene()
{
    if (!in_scene)
    {
        return;
    }
    if (actors != nullptr && entity_detail::g_world.child.next != nullptr)
    {
        remove_retired_actors(); // The scene controls' widgets retire their bats as the controls go (0x00115F00).
    }
    // 0x0011C6F6: the global effect nodes (0x000ED450) and 0x0011C6FB: the camera-facing sprite queue
    // (DAT_00504330) go with the scene, so the next scene's first flush draws none of this scene's sprites.
    release_global_actor_effects(g_actor_effect_nodes, *effect_release, *debris_pool, *trail_pool,
        resources.registry);
    g_billboard_queue.count = 0;
    unload_runtime_ghoul_intro(*ghoul_intro_context); // 0x000F44B0
    // 0x0011C5A0 later tears the scoreboard down.
    tear_down_runtime_scoreboard(*scoreboard_hud); // 0x00132100
    release_runtime_background_cloths(background_cloth_scene()); // 0x0011C5A0's 0x001143A0
    entity_detail::release_segment_slots(); // 0x0011C5A0's 0x001222A0: the contact slots and their pools.
    // 0x0011C5A0's 0x00123640, 0x001199F0 and 0x001133B0.
    reset_runtime_scene_point_light_pass(*point_light_pass, g_flag_2104, g_color_2134, [this](std::uintptr_t resource)
    {
        if (d3d_device.surface_textures.contains(resource))
        {
            d3d_device.outer().release(DialogueSurfaceHandle(resource));
            return;
        }
        resources.d3d.resources.erase(u32(resource));
        created_textures.erase(u32(resource));
    });
    reset_runtime_blob_shadows(blob_shadow_entries, blob_shadow_count, blob_shadow_free, blob_shadow_enable,
        point_light_pass->targets, [this](std::uintptr_t resource)
    {
        if (d3d_device.surface_textures.contains(resource))
        {
            d3d_device.outer().release(DialogueSurfaceHandle(resource));
            return;
        }
        resources.d3d.resources.erase(u32(resource));
        created_textures.erase(u32(resource));
    }); // 0x001199F0
    release_runtime_scene_light_grid(light_grid, light_grid_d3d); // 0x001133B0
    light_grid_background = nullptr;
    // The scene's cutscenes released their track actors (0x0004D720). Track actors a live cutscene still holds (one
    // that outlives the scene, as a transition's book does) stay; any other goes with the scene.
    std::unordered_set<const Actor*> held;
    for (const auto& [instance, rows] : cutscenes->instances)
    {
        for (const auto& row : rows.actors)
        {
            if (row.actor != nullptr)
            {
                held.insert(row.actor);
            }
        }
    }
    std::vector<FactoryObject> kept;
    for (auto object = cutscene_actors.rbegin(); object != cutscene_actors.rend(); ++object)
    {
        if (object->bytes == nullptr)
        {
            continue;
        }
        const auto view = actors->cutscene_bindings.find(object->bytes.get());
        const Actor* actor = view != actors->cutscene_bindings.end() ? &view->second->frame.actor : nullptr;
        if (actor != nullptr && held.contains(actor))
        {
            kept.push_back(std::move(*object));
        }
        else
        {
            release_factory_object(scene.factory, *object);
        }
    }
    cutscene_actors.clear();
    for (auto object = kept.rbegin(); object != kept.rend(); ++object)
    {
        cutscene_actors.push_back(std::move(*object));
    }
    for (auto* owned : {&cameras, &callout_objects})
    {
        for (auto object = owned->rbegin(); object != owned->rend(); ++object)
        {
            if (object->bytes != nullptr)
            {
                release_factory_object(scene.factory, *object);
            }
        }
        owned->clear();
    }
    released_actors.clear();
    for (auto object = released_weapons.rbegin(); object != released_weapons.rend(); ++object)
    {
        if (object->bytes != nullptr)
        {
            release_factory_object(scene.factory, *object);
        }
    }
    released_weapons.clear();
    // DAT_004EE2B8, which 0x0011CBD0 allocates for each scene.
    if (door_pool != nullptr)
    {
        engine_heap_free(door_pool);
        door_pool = nullptr;
    }
    // 0x0011C6C0: the light records and the fog record reset, so a scene's fog (the attract's black fog) does not
    // carry into the next.
    reset_runtime_light_environment(shaders, fog); // 0x000FD710
    in_scene = false;
}

extern i32 g_frontend_next_row; // DAT_00429BC0, scene.cpp.
extern u16 g_frontend_selected_game[0x80]; // DAT_00429BC4, scene.cpp.

RuntimeScreenTextureBoundaries SceneActorServices::State::screen_texture_boundaries()
{
    RuntimeScreenTextureBoundaries textures;
    textures.allocate = [this](u32 size)
    {
        return resources.memory.allocate(size, 0x10);
    };
    textures.free = [this](u32 address)
    {
        resources.memory.free(address);
    };
    textures.allocate_physical = [this](u32 size, u32 alignment)
    {
        return resources.memory.allocate(size, alignment);
    };
    textures.free_physical = [this](u32 address)
    {
        resources.memory.free(address);
    };
    textures.header = [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool, u32 texture) -> u32
    {
        resources.d3d_boundaries.set_texture_header(width, height, levels, usage, format, pool, texture, 0, 0);
        // XGSetTextureHeader's size for the linear 32-bit formats 0x000533A0 is given.
        if ((format != 0x12 && format != 0x1E && format != 0x3F) || levels != 1)
        {
            throw Error("XGSetTextureHeader size for format " + std::to_string(format) + " is not mapped");
        }
        return width * height * 4;
    };
    textures.register_resource = [this](u32 resource, u32 data)
    {
        resources.d3d_boundaries.register_resource(resource, data);
    };
    textures.address_of = [this](const u8* bytes)
    {
        return resources.memory.address_of(bytes);
    };
    return textures;
}

void SceneActorServices::State::bind_transition_services()
{
    transition_services = std::make_unique<SceneTransitionServices>(SceneTransitionServices{image, resources.registry,
        resources.error, text_registry, resources.metrics, widget_context.viewport_width, next_widget_id, random,
        resources.font_views, fallback_texture.data(), resources.text, hud_size, g_sim_dt, *book_services, voices,
        *dialogue_draw, *display_setup,
        [this]() -> i32
        {
            return game.script_dialogue == nullptr ? 0 : game.script_dialogue->dialogue.state.mode; // DAT_004EE1EC
        },
        book_pages[0],
        [this](u32 texture)
        {
            d3d_device.commands.push_back(SceneD3DCopy{texture}); // D3DDevice_CopyRects
        },
        [this](const u8* bytes) -> u32
        {
            return resources.memory.address_of(bytes);
        },
        *widget_allocation, *widget_bats, *widget_update, *dialogue_outer, input_activation, input_release,
        input_addresses, input_views,
        [this]() -> u32
        {
            return game.frame; // DAT_004DEA58
        },
        hud.inner_bottom});
    g_transition_services = transition_services.get();
    // The book's map menu shares the transition's prompt and input contexts.
    book_services->widget_allocation = widget_allocation.get();
    book_services->widget_bats = widget_bats.get();
    book_services->widget_update = widget_update.get();
    book_services->widget_draw = dialogue_outer.get();
    book_services->input = &input_activation;
    book_services->input_release = &input_release;
    book_services->input_addresses = &input_addresses;
    book_services->input_views = &input_views;
    book_services->frame = [this]() -> u32
    {
        return game.frame; // DAT_004DEA58
    };
    book_services->viewport_bottom = &hud.inner_bottom;
    initialize_scene_transition(); // Boot 0x0011C3F0's loop over the handlers' +0 slots.
}

void SceneActorServices::State::bind_pause_services()
{
    book_services->scene_paused = &g_scene_paused; // DAT_00510CE8
    book_services->pause_phase = &g_scene_pause.phase; // DAT_00510CF4
    pause_services = std::make_unique<ScenePauseServices>(ScenePauseServices{*book_services, *dialogue, audio.streams,
        [](float level)
        {
            scene_set_audio_level(level); // 0x00038110
        },
        [this]()
        {
            tick_scene_dialogue(game); // 0x00057E60
        },
        [this](const char* script) -> i32
        {
            return gameselect_script_chapter(script, *gameselect_services); // 0x0004B4D0
        },
        [this](const char* script) -> i32
        {
            return gameselect_script_scene(script, *gameselect_services); // 0x0011EBC0
        },
        [this](const char* script) -> i32
        {
            return gameselect_book_scene(script, *gameselect_services); // 0x0011EC70
        },
        []() -> std::span<const u8>
        {
            return g_runtime_profile.challenges.bytes; // *(DAT_00506C54+8)
        },
        difficulty, frontend_state, hud, reinterpret_cast<const char*>(g_scene_record),
        *reinterpret_cast<i32*>(g_scene_record + 0x108), g_scene_gameplay_time,
        [this]() -> std::array<i32, 2>
        {
            if (measurement == nullptr)
            {
                unbound("0x0011E1C0's play counts before the dialogue measurement exists");
            }
            return {measurement->first_additional, measurement->third_additional}; // DAT_00545970, DAT_00545974
        },
        [this]()
        {
            with_scene_save([](RuntimeSceneSave& save)
            {
                save_runtime_quit_scene(save); // 0x000F2CB0
            });
        },
        [this]()
        {
            release_actor_camera_shakes(resources.registry); // 0x000FAE40
        },
        [this]()
        {
            with_scene_save([](RuntimeSceneSave& save)
            {
                end_runtime_challenge_game(save); // 0x00049C30
            });
        },
        []()
        {
            stop_runtime_chapter5_countdown(chapter5_countdown("0x000F2780")); // 0x000F2780
        },
        []()
        {
            restart_run(challenge_runtime("0x0004A310")); // 0x0004A310
        },
        [this]() -> const char*
        {
            return reinterpret_cast<const char*>(scene_order_held.data()); // DAT_00511B64
        },
        [this]() -> const char*
        {
            return runtime_door_room_suffix(room.data(), door_aliases); // 0x001410F0 on DAT_00545D40
        }});
    // The pause prompt rows' buttons (0x0030F948 .. 0x00311AF8, row +0x84).
    const std::array<std::pair<u32, void (*)(ScenePauseServices&)>, 10> buttons{{
        {0x0011DEE0, close_scene_pause}, // pause_Return
        {0x0011DFA0, confirm_scene_pause_quit}, // pause_Quit
        {0x0011E110, show_scene_pause_stats}, // pause_Stats
        {0x0011E140, show_scene_pause_map}, // pause_Map
        {0x0011E150, show_scene_pause_rules}, // pause_EventRules, pause_ChallengeRules
        {0x0011E1A0, cancel_scene_pause_confirm}, // pause_No
        {0x0011E1C0, quit_scene_from_pause}, // pause_Yes (quit)
        {0x0011E320, restart_scene_from_pause}, // pause_Yes (restart)
        {0x0011E3E0, leave_scene_pause_map}, // pause_MapReturn
        {0x0011E410, confirm_scene_pause_restart}}}; // pause_RestartChallenge
    for (const auto& [address, button] : buttons)
    {
        widget_callbacks[address] = [this, button]()
        {
            button(*pause_services);
        };
    }
}

void SceneActorServices::State::bind_snapshot_texture_create()
{
    snapshot_d3d.create_texture = [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)
    {
        // D3DDevice_CreateTexture2: one level of 32-bit texels behind a resource header (LIN_D24S8, 0x2E, keeps
        // depth-stencil words).
        if (levels != 1 || (format != 6 && format != 0x12 && format != 0x2E))
        {
            unbound("D3DDevice_CreateTexture2 of a format or level count other than one 32-bit level");
        }
        const u32 header = resources.memory.allocate(0x14, 4).address;
        const u32 texels = resources.memory.allocate(width * height * 4, 0x80).address;
        auto& record = resources.d3d.resources[header];
        record.kind = 3;
        record.data = texels;
        record.header = {width, height, levels, usage, format, pool};
        created_textures.insert(header);
        return header;
    };
}

// Boot 0x00033030 -> 0x000620C0 over the scene's D3D boundaries.
void SceneActorServices::State::create_boot_render_targets()
{
    RuntimeBootRenderTargetD3D d3d;
    d3d.create_texture = [this](u32 width, u32 height, u32 depth, u32 levels, u32 usage, u32 format, u32 type)
    {
        if (depth != 1 || type != 3)
        {
            unbound("D3DDevice_CreateTexture2 of a volume or cube texture");
        }
        return snapshot_d3d.create_texture(width, height, levels, usage, format, type);
    };
    d3d.surface_level = [this](u32 texture, u32 level)
    {
        return d3d_device.outer().surface(DialogueSurfaceHandle(texture), level);
    };
    d3d.back_buffer = [this](i32 index)
    {
        return d3d_device.outer().back_buffer(u32(index));
    };
    d3d.depth_stencil = [this]()
    {
        return d3d_device.outer().depth_buffer();
    };
    d3d.describe = [](DialogueSurfaceHandle surface)
    {
        // The device's 640 x 480 buffers: LIN_X8R8G8B8 colour and LIN_D24S8 depth-stencil.
        RuntimeSurfaceDescription description;
        if (surface == SceneD3DDevice::kBackBuffer)
        {
            description.format = 0x1E;
            description.usage = 1; // D3DUSAGE_RENDERTARGET.
        }
        else if (surface == SceneD3DDevice::kDepthBuffer)
        {
            description.format = 0x2E;
            description.usage = 2; // D3DUSAGE_DEPTHSTENCIL.
        }
        else
        {
            unbound("D3DSurface_GetDesc of a surface other than the device's buffers");
        }
        description.type = 1; // D3DRTYPE_SURFACE.
        description.width = 640;
        description.height = 480;
        description.size = 640 * 480 * 4;
        return description;
    };
    d3d.release = [this](DialogueSurfaceHandle surface)
    {
        d3d_device.outer().release(surface);
    };
    d3d.create_surface = [this](u32 width, u32 height, u32, u32)
    {
        // 0x00175C90's two XMemAllocs: the header (0x81050001, a surface, with its memory at +4) and the memory.
        const RuntimeModelAllocation header = resources.memory.allocate(0x18, 4);
        const u32 memory = resources.memory.allocate(width * height * 4, 0x1000).address;
        const u32 common = 0x81050001u;
        std::memcpy(header.bytes.data(), &common, 4);
        std::memcpy(header.bytes.data() + 4, &memory, 4);
        return DialogueSurfaceHandle(header.address);
    };
    d3d.surface_data = [this](DialogueSurfaceHandle surface)
    {
        return read_u32(resources.memory.tail(u32(surface)), 4);
    };
    d3d.texture_header = [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool, u32 texture,
                             u32 data, u32 pitch)
    {
        resources.d3d_boundaries.set_texture_header(width, height, levels, usage, format, pool, texture, data, pitch);
        resources.d3d_boundaries.register_resource(texture, data);
    };
    d3d.lock_surface = [this](DialogueSurfaceHandle surface, u32)
    {
        return read_u32(resources.memory.tail(u32(surface)), 4);
    };
    d3d.textures = screen_texture_boundaries();
    glow_targets.depth_texture = resources.memory.allocate(0x14, 4).bytes.data(); // DAT_005047E8's storage.
    RuntimeBootRenderTargets targets{light_volume_texture, light_volume_surface, render_targets, book_pages,
        glow_targets, g_transition_guard};
    create_runtime_boot_render_targets(targets, d3d);
    // Presentation: the depth surface is drawn into through the texture over its memory, and that texture, a bare
    // XGSetTextureHeader header with no texture object round it, is decoded from its header words as a created one is.
    const u32 depth_texture = resources.memory.address_of(glow_targets.depth_texture);
    d3d_device.surface_textures[render_targets.depth_capture_surface] = depth_texture;
    created_textures.insert(depth_texture);
}

void SceneActorServices::State::bind_book_services()
{
    book_services = std::make_unique<TransitionBookServices>(TransitionBookServices{*cutscenes, resources.registry,
        resources.error, image, *display_setup, model_renderer, node_cache, g_sim_freeze, g_sim_dt, g_sim_scale,
        g_flags_ea9c, {resources.memory.address_of(book_pages[0].data()), resources.memory.address_of(book_pages[1].data())},
        [this](Actor& actor)
        {
            update_base_actor(object_of_actor(actor)); // 0x000420D0
        },
        [this](std::span<const u8> texture) -> u32
        {
            return resources.memory.address_of(texture.data());
        },
        [this](u32 texture)
        {
            d3d_device.commands.push_back(SceneD3DCopy{texture}); // D3DDevice_CopyRects
        },
        [this](u32 count, const void* rectangles, u32 flags, u32 color, float depth, u32 stencil)
        {
            dialogue_outer->d3d.clear(count, rectangles, flags, color, depth, stencil); // D3DDevice_Clear
        },
        [this](u8 enabled)
        {
            set_runtime_render_fog_enabled(model_renderer, enabled); // 0x00156EA0
        },
        [this]() -> i32
        {
            return i32(dialogue_outer->current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        },
        *dialogue_draw,
        [this](u32 texture) -> u8*
        {
            return resources.memory.pointer(texture);
        },
        [this](Actor& actor) -> RuntimeModelInstance*
        {
            return actors->view(object_of_actor(actor)).startup.models.secondary.get();
        },
        dialogue_outer->capture_configuration, 0, read_u32(data(0x004231C4, 4), 0), // page_clear_color; DAT_005462A4 is never written.
        text_registry, resources.metrics, widget_context.viewport_width, next_widget_id, random, resources.font_views,
        fallback_texture.data(), resources.text, voices,
        [this](float width)
        {
            dialogue_outer->d3d.line_width(width); // D3D 0x00171570
        },
        game.clock.time, story_bits, g_scene_flags->bytes[0], hud_size});
}

void SceneActorServices::State::bind_gameselect_services()
{
    // Boot 0x000E5A80 made the front end's keyboard device (DAT_00429BBC). The native input registry is the
    // scene's, so the scene maps the boot device's address while it runs.
    frontend_input = g_frontend.input;
    frontend_input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(frontend_input.data()));
    input_addresses[frontend_input_address] = frontend_input;
    input_views[frontend_input_address] = RuntimeDialogueInputDevice{frontend_input, {nullptr, nullptr, nullptr,
        nullptr}};
    RuntimeScreenTextureBoundaries textures = screen_texture_boundaries();
    gameselect_services = std::make_unique<GameSelectControlServices>(GameSelectControlServices{resources.registry,
        resources.error, g_runtime_profile, g_flags_ea9c, gameselect_text,
        image.string(read_u32(data(0x00430CF8, 4), 0)), // scene_order
        scene_order_held, scene_order_release, g_pause_count, input_activation, input_release, frontend_input_address,
        game.clock.frame, g_frontend_next_row, gameselect_stream_wait, 5.f, // DAT_0032B184.
        [this](i32 id) -> i32
        {
            return runtime_stream_bank_busy(id, nullptr, nullptr, audio.wavebank_streams); // 0x00039D50
        },
        audio.streams, g_pause_ditty, textures, *display_setup, g_sim_freeze, g_sim_dt, g_sim_scale, image,
        g_frontend, game.native_movies->session->startup(), hud_size, hud, reinterpret_cast<char16_t*>(g_frontend_selected_game),
        reinterpret_cast<i32*>(g_scene_record + 0x124), g_transition_phase, text_registry, resources.metrics,
        widget_context.viewport_width, next_widget_id, random, resources.font_views, fallback_texture.data(),
        *dialogue_draw, *dialogue_outer, widget_context, *widget_allocation, resources.text,
        [this](const char* resource, const char* group, const char* key) -> const char16_t*
        {
            // 0x000FF0A0 writes the text into DAT_00507E78.
            if (request_namespaced_localtext(resource, group, key, gameselect_lookup.data(), resources.text) == 0)
            {
                return nullptr;
            }
            return gameselect_lookup.data();
        },
        [this](const std::array<float, 2>& position, float scale, float depth) -> Actor*
        {
            auto object = create_dialogue_button_bat(widget_bats->factory, widget_bats->resources,
                widget_bats->configuration, position, scale, depth); // 0x0013B580
            if (object.bytes == nullptr)
            {
                return nullptr;
            }
            Actor* bat = widget_bats->actors.at(object.bytes.get());
            widget_bats->objects.push_back(std::move(object));
            bind_button_bat(widget_bats->objects.back());
            return bat;
        },
        [this](Actor& bat) -> ButtonBatScreen&
        {
            return bat_views(&bat).bat;
        },
        [this](Actor& bat, const ButtonBatPoint& target, float scale)
        {
            for (const auto& [bytes, native] : actors->actors)
            {
                if (native == &bat)
                {
                    auto& view = actors->view(*object_of(bytes));
                    fly_runtime_button_bat(view.bat, view.animation, target, scale, resources.registry,
                        resources.error, random); // 0x0006DA00
                    return;
                }
            }
            throw Error("game-select bat is not a scene actor");
        },
        [this](Actor& actor, i32 view, i32 pass)
        {
            actor_draw(actor, view, pass); // Actor vtable +0x14.
        },
        [this](RuntimeDialogueWidget& widget, bool device)
        {
            const RuntimeDialogueInputDevice* input = device ? &input_views.at(frontend_input_address) : nullptr;
            update_dialogue_widget(widget, input, *widget_update); // 0x00142460
        },
        [this](float* timer) -> u32
        {
            return tick_runtime_body_frame_timer(*timer, clock) ? 1 : 0; // 0x0012B2A0
        },
        [this](std::array<float, 2>& direction)
        {
            dialogue_input_direction(input_views.at(frontend_input_address), direction, input_state); // 0x001429E0
        },
        [this](const char* name, i32)
        {
            script_request(game, name); // 0x0011EB30 with mode 1.
        },
        g_scene_name_override, *cutscenes, *attract_services,
        [this]() -> i32
        {
            return game.script_dialogue == nullptr ? 0 : game.script_dialogue->dialogue.state.mode; // DAT_004EE1EC
        },
        voices, u32(g_language),
        [this](std::span<u8> device) -> u32
        {
            const u32 address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(device.data()));
            input_addresses[address] = device;
            input_views[address] = RuntimeDialogueInputDevice{device, {nullptr, nullptr, nullptr, nullptr}};
            return address;
        },
        [this](u32 address)
        {
            input_views.erase(address);
            input_addresses.erase(address);
        },
        [this](RuntimeDialogueWidget& widget, u32 device)
        {
            update_dialogue_widget(widget, &input_views.at(device), *widget_update); // 0x00142460
        },
        game.native_movies->session->files(),
        SaveImageBoundaries{
            [this](const char* aid) -> std::span<const u8>
            {
                return acquire_animation_resource(resources.registry, aid); // 0x00035E90
            },
            [this](std::span<const u8> texture)
            {
                release_animation_resource(resources.registry, texture); // 0x00035C00
            },
            [this](std::span<const u8> header, const char* path)
            {
                // XGWriteSurfaceOrTextureToXPR: the D3D header's data is the loaded resource's GPU bytes for its
                // frame (the same lookup the presenter's texture_image makes).
                for (const auto& loaded : resources.registry.loaded)
                {
                    const u8* begin = loaded.cpu.data();
                    if (begin == nullptr || header.data() < begin || header.data() >= begin + loaded.cpu.size())
                    {
                        continue;
                    }
                    for (size_t frame = 0; size_t(header.data() - begin) >= 28 + frame * 0x14; ++frame)
                    {
                        TextureHeader parsed;
                        const size_t start = size_t(header.data() - begin) - 28 - frame * 0x14;
                        if (!parse_texture_header(loaded.cpu.subspan(start), parsed)
                            || frame >= std::max<size_t>(parsed.tile_count, 1))
                        {
                            continue;
                        }
                        const size_t offset = size_t(parsed.texture_offset) + frame * parsed.data_size;
                        write_texture_xpr(header, loaded.gpu.subspan(offset, parsed.data_size), path);
                        return;
                    }
                }
                throw Error("the save image texture is not a loaded resource");
            },
            [this]()
            {
                resource_error(resources.error, 2); // 0x00032700(2)
            },
            random},
        *book_services,
        [this](u32 address) -> const RuntimeDialogueInputDevice&
        {
            return input_views.at(address);
        },
        port_masks.data() + 1, g_scene_paused,
        [this](Actor& actor)
        {
            actor_updates.at(&actor)(actor); // Actor vtable +0x10.
        },
        replacement_source.data(), replacement_target.data()});
    gameselect_services->scoreboard = scoreboard_hud.get();
    scene.controls->context.gameselect = gameselect_services.get();
    // The running challenge (DAT_004E17C8 .., script types 0x7A..0x86).
    const auto case_player = [this]() -> NativePlayerInstance*
    {
        // DAT_00511B40: the first case-list node (player actor +0x93C, links[1]); null or the 0x93C sentinel is none.
        const auto* node = entity_detail::g_world.case_head.next;
        if (node == nullptr)
        {
            return nullptr;
        }
        for (auto* player : players->instances())
        {
            if (static_cast<const void*>(&player->links[1]) == static_cast<const void*>(node))
            {
                player->refresh_body_views();
                return player;
            }
        }
        unbound("DAT_00511B40 node with no native player");
    };
    challenge_services = std::make_unique<ChallengeRuntimeServices>(ChallengeRuntimeServices{g_flags_ea9c,
        g_runtime_profile, g_scene_flags->bytes[0], clock, g_scene_gameplay_time, g_super_scary_presses, g_super_scary_reshuffle,
        score_total, active_actors,
        [case_player]() -> i32
        {
            NativePlayerInstance* player = case_player();
            return player == nullptr || player->body.empty() ? -1 : i32(body_record(player->body).energy); // Body +0x94
        },
        [case_player]()
        {
            NativePlayerInstance* player = case_player();
            if (player == nullptr)
            {
                return false;
            }
            if (player->body_flags == nullptr)
            {
                unbound("DAT_00511B40 player without the body +0x15C bit block");
            }
            return (player->body_flags->bytes[3] & 0x10) != 0;
        },
        [](u16 tag)
        {
            return script_count_actors_by_class(tag); // 0x00129240, DX the tag
        },
        [case_player]()
        {
            NativePlayerInstance* player = case_player(); // 0x001292D0
            return player != nullptr && !player->body.empty() && body_record(player->body).energy == 0;
        },
        *scoreboard_hud, *scoreboard_resources, data(0x00314170, sizeof(HudCounterPreset)), *dialogue, audio.streams,
        [](u8 mask, i32 release)
        {
            g_actor_pause(mask, release); // 0x00117400
        },
        [this](const char* name, i32)
        {
            script_request(game, name); // 0x0011EB30 with mode 1.
        },
        [this](const char* name)
        {
            hold_runtime_scene_order(name, *gameselect_services); // 0x001234B0
        },
        [this]()
        {
            return reinterpret_cast<const char*>(scene_order_held.data()); // DAT_00511B64
        },
        reinterpret_cast<const char*>(g_scene_record), g_frontend_next_row, g_frontend_selected_game, g_challenge_game_name,
        [this]()
        {
            // 0x0004A4D0 state 6's walk of DAT_00511B38: players whose +0xCDC word is 1, with energy and without
            // the body bit 0x1B.
            std::vector<u32> chosen;
            for (const auto& mark : marked_actors())
            {
                auto& player = players->player_at(mark.address);
                player.refresh_body_views();
                if (player_record(player.actor_bytes).mode != 1 || player.body.empty()
                    || body_record(player.body).energy == 0)
                {
                    continue;
                }
                if (player.body_flags == nullptr)
                {
                    unbound("a DAT_00511B38 player without the body +0x15C bit block");
                }
                if ((player.body_flags->bytes[3] & 8) == 0)
                {
                    chosen.push_back(mark.address);
                }
            }
            return chosen;
        },
        [this](u32 address)
        {
            auto& player = players->player_at(address);
            const RuntimePlayerGoalRing* ring = player.ring; // Actor +0x980
            if (ring == nullptr)
            {
                return false;
            }
            if (ring->tail == ring->capacity)
            {
                throw Error("0x0004A990 reads the goal at an empty ring's head (a null read in retail)");
            }
            const u16 goal = read_u16(ring->rows, size_t(ring->head) * ring->stride);
            return goal != 0 && player_record(player.actor_bytes).unk_984 == 0;
        },
        [this](u32 address)
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            drain_runtime_player_goals(player.goal_cleanup); // 0x00028240 until +0x980 is empty
        },
        [this](u32 address, std::span<const u32> message)
        {
            players->player_at(address).actor_message(message); // Vtable +0xC
        },
        *challenge_saves, resources.registry, text_registry, data(0x002FE2E0, 0x310).data(), hud});
    g_challenge_runtime = challenge_services.get();
    score_captions = std::make_unique<RuntimeScoreCaptions>(RuntimeScoreCaptions{text_registry, resources.metrics,
        resources.font_views, dialogue->wrap_width, widget_context.viewport_width, next_widget_id, random,
        resources.text, score_texts, wide_scratch.data(), score_total, data(0x004326EC, 0x10)});
    scoreboard_hud->captions = score_captions.get();
    // The slot prompts' row callbacks (+0x84).
    widget_callbacks[0x000E0210] = [this]()
    {
        move_runtime_slot_cursor(*gameselect_services);
    };
    widget_callbacks[0x000E02D0] = [this]()
    {
        select_runtime_slot(*gameselect_services);
    };
    widget_callbacks[0x000E04F0] = [this]()
    {
        delete_runtime_slot(*gameselect_services);
    };
    widget_callbacks[0x000E05D0] = [this]()
    {
        leave_runtime_slot_select(*gameselect_services);
    };
    // The contents prompts' rows (0x003069F0, 0x00306D00, 0x00307010, 0x00307320).
    widget_callbacks[0x000DB450] = [this]()
    {
        move_runtime_contents_cursor(*gameselect_services);
    };
    widget_callbacks[0x000DB510] = [this]()
    {
        leave_runtime_game_contents(*gameselect_services);
    };
    widget_callbacks[0x000DB570] = [this]()
    {
        play_runtime_game(*gameselect_services);
    };
    widget_callbacks[0x000D4E70] = [this]()
    {
        accept_runtime_final_score_super_scary(*final_score_services);
    };
    widget_callbacks[0x000DB5E0] = [this]()
    {
        show_runtime_game_stats(*gameselect_services);
    };
    widget_callbacks[0x000DB620] = [this]()
    {
        open_runtime_game_info(*gameselect_services);
    };
    widget_callbacks[0x000DB670] = [this]()
    {
        open_runtime_game_challenges(*gameselect_services);
    };
    widget_callbacks[0x000DB6D0] = [this]()
    {
        open_runtime_game_features(*gameselect_services);
    };
    // The features prompts' rows (0x00306570, "FeaturesMenu").
    widget_callbacks[0x000E4E40] = [this]()
    {
        activate_runtime_feature(*gameselect_services);
    };
    widget_callbacks[0x000D8080] = [this]()
    {
        move_runtime_features_cursor(*gameselect_services);
    };
    widget_callbacks[0x000D8030] = [this]()
    {
        leave_runtime_game_features(*gameselect_services);
    };
    // The challenges prompts' rows (0x00305CD8, "challengeMenu_basic").
    widget_callbacks[0x000D7770] = [this]()
    {
        play_runtime_challenge(*gameselect_services);
    };
    widget_callbacks[0x000D77C0] = [this]()
    {
        move_runtime_challenge_cursor(*gameselect_services);
    };
    widget_callbacks[0x000D7890] = [this]()
    {
        leave_runtime_game_challenges(*gameselect_services);
    };
    widget_callbacks[0x000D78E0] = [this]()
    {
        show_runtime_challenge_intro(*gameselect_services);
    };
    widget_callbacks[0x000D78F0] = [this]()
    {
        open_runtime_challenge_bonus_books(*gameselect_services);
    };
    // The app error screen's rows (0x00339F98 / 0x0033A2A8, "AppMainErrorDialogMenu_1" / "_2").
    widget_callbacks[0x00032F10] = []()
    {
        choose_app_error_a();
    };
    widget_callbacks[0x00032FA0] = []()
    {
        choose_app_error_b();
    };
    // The challenge restart prompts' rows (0x002FE2E0, "challengeMenu_restart").
    widget_callbacks[0x0004B350] = []()
    {
        choose_runtime_challenge_restart();
    };
    widget_callbacks[0x0004B360] = []()
    {
        choose_runtime_challenge_quit();
    };
    // The scene-replay prompts' rows (0x003095D0, "ReplayChapterMenu"): A and the stick are 0x000E4E40 and
    // 0x000D8080 above.
    widget_callbacks[0x000E30E0] = [this]()
    {
        leave_runtime_game_scene_replay(*gameselect_services);
    };
    // The chapter-replay and bonus-book-scene prompts' B (0x00309240, 0x003098E0).
    widget_callbacks[0x000E2360] = [this]()
    {
        leave_runtime_game_chapter_replay(*gameselect_services);
    };
    widget_callbacks[0x000E3F40] = [this]()
    {
        leave_runtime_game_bonus_book_replay(*gameselect_services);
    };
    // The options prompts' B and Y (0x00308C20, 0x00308F30).
    widget_callbacks[0x000E11D0] = [this]()
    {
        leave_runtime_game_options(*gameselect_services);
    };
    widget_callbacks[0x000E1890] = [this]()
    {
        explain_runtime_butlers_brew(*gameselect_services);
    };
    widget_callbacks[0x000E4E70] = [this]()
    {
        leave_runtime_game_story_books(*gameselect_services); // The story books' B (0x00309C70)
    };
    widget_callbacks[0x000E4ED0] = [this]()
    {
        page_runtime_button_bat_menu_down(*gameselect_services); // 0x000E4ED0: JMP 0x00143F30
    };
    widget_callbacks[0x000E4EC0] = [this]()
    {
        page_runtime_button_bat_menu_up(*gameselect_services); // 0x000E4EC0: JMP 0x00143D60
    };
    // The deletion prompts' rows (0x003077B0).
    widget_callbacks[0x000DBD50] = [this]()
    {
        leave_runtime_game_deletion(*gameselect_services);
    };
    widget_callbacks[0x000DBC20] = [this]()
    {
        confirm_runtime_game_deletion(*gameselect_services);
    };
    // The keyboard prompts' rows (0x0020AAD8) all name a bare RET (0x001628B0).
    widget_callbacks[0x001628B0] = []()
    {
    };
}

SceneActorServices::State::~State()
{
    begin_teardown();
    end_scene();
    if (scene.controls != nullptr && scene.controls->context.attract == attract_services.get())
    {
        scene.controls->context.attract = nullptr;
    }
    if (scene.controls != nullptr && trophy_heads_services != nullptr
        && scene.controls->context.trophy_heads == trophy_heads_services.get())
    {
        scene.controls->context.trophy_heads = nullptr;
    }
    if (scene.controls != nullptr && super_scary_services != nullptr
        && scene.controls->context.super_scary == super_scary_services.get())
    {
        scene.controls->context.super_scary = nullptr;
    }
    if (scene.controls != nullptr && map_view_services != nullptr
        && scene.controls->context.map_view == map_view_services.get())
    {
        scene.controls->context.map_view = nullptr;
    }
    if (scene.controls != nullptr && final_score_services != nullptr
        && scene.controls->context.final_score == final_score_services.get())
    {
        scene.controls->context.final_score = nullptr;
    }
    if (scene.controls != nullptr && gameselect_services != nullptr
        && scene.controls->context.gameselect == gameselect_services.get())
    {
        scene.controls->context.gameselect = nullptr;
    }
    if (scene.controls != nullptr && haunted_chairs_services != nullptr
        && scene.controls->context.haunted_chairs == haunted_chairs_services.get())
    {
        scene.controls->context.haunted_chairs = nullptr;
    }
    if (scene.controls != nullptr && relics_store_services != nullptr
        && scene.controls->context.relics_store == relics_store_services.get())
    {
        scene.controls->context.relics_store = nullptr;
    }
    if (scene.controls != nullptr && candle_control_services != nullptr
        && scene.controls->context.candle == candle_control_services.get())
    {
        scene.controls->context.candle = nullptr;
    }
    if (scene.controls != nullptr && chapter2_scene4_services != nullptr
        && scene.controls->context.chapter2_scene4 == chapter2_scene4_services.get())
    {
        scene.controls->context.chapter2_scene4 = nullptr;
    }
    if (scene.controls != nullptr && fx_services != nullptr && scene.controls->context.fx == fx_services.get())
    {
        scene.controls->context.fx = nullptr;
    }
    if (frontend_input.data() != nullptr)
    {
        input_views.erase(frontend_input_address);
        input_addresses.erase(frontend_input_address);
        frontend_input = {};
    }

    game.native_movies->session->startup().lock_texture = [](void*) -> RuntimeMovieLockedSurface
    {
        throw Error("movie frame copy 0x00107360 has no scene texture to lock");
    };
    if (find_scene_cutscene_runtime(game) != nullptr)
    {
        remove_scene_cutscene_runtime(game);
    }
    if (game.script_dialogue == script_dialogue.get() && script_dialogue != nullptr)
    {
        unbind_scene_dialogue(game);
    }
    // The input stack is boot-owned: the pool (perhaps grown here) goes back to it.
    game.native_input->pool = input_activation.entries;
    input_views.erase(game.native_input->master_address);
    input_addresses.erase(game.native_input->master_address);
    scene.released = nullptr;
    if (g_runtime_doors == doors.get())
    {
        g_runtime_doors = nullptr;
    }
    if (g_event_sequence_context == event_sequence.get())
    {
        g_event_sequence_context = nullptr;
    }
    if (active_services == this)
    {
        active_services = nullptr;
        g_actor_pause = nullptr;
        g_loading_actors = nullptr;
        g_live_actors_pending = nullptr;
        g_spawn_trigger_players = nullptr;
        g_runtime_invisibility = nullptr;
        g_chapter5_countdown = nullptr;
        g_challenge_runtime = nullptr;
        if (g_app_error_services == gameselect_services.get())
        {
            g_app_error_services = nullptr;
        }
        g_runtime_weapon_boost = nullptr;
        g_grim_reaper_dizzy = nullptr;
        g_marked_actor_message = nullptr;
        g_actor_broadcast = nullptr;
        g_alt_node_body = nullptr;
        g_alt_node_class = nullptr;
        g_alt_node_actor = nullptr;
        g_alt_node_actor_bytes = nullptr;
        g_alt_node_mind = nullptr;
        g_alt_node_target_eligible = nullptr;
        g_alt_node_message = nullptr;
        g_script_exclude_actor_tag = nullptr;
        g_script_actor_bytes = nullptr;
        g_script_actor_message = nullptr;
        g_script_drain_actor_goals = nullptr;
        g_script_body_list_message = nullptr;
        g_script_ghoul_intro_caption = nullptr;
        g_script_actor_by_id = nullptr;
        g_script_live_player_position = nullptr;
        g_script_door_distance = nullptr;
        g_script_object_message = nullptr;
        g_script_door_pose = nullptr;
        g_script_kill_counters = nullptr;
    }
    if (g_transition_services == transition_services.get())
    {
        g_transition_services = nullptr;
    }
    // Boot registers the callout hook once; the boot-owned registry outlives this scene's callout context, so the
    // hook this scene added goes with it.
    for (auto& hook : resources.registry.hooks)
    {
        if (hook.type != nullptr && std::strcmp(hook.type, "callout") == 0)
        {
            hook = AnimationResourceHook{};
        }
    }
    // The decoded images go with the scene; their uploads must not serve a later image at the same address.
    resources.memory.on_free = nullptr;
    resources.on_texture_restored = nullptr;
    for (const auto& [record, image] : texture_images)
    {
        gfx_release_sprite(*image);
    }
    texture_images.clear();
    if (statsheet.data() != nullptr)
    {
        release_animation_resource(resources.registry, statsheet);
    }
    for (const auto table : {part_settings, part_text, sectors.settings, fallback_texture, text_fonts[0],
        text_fonts[1]})
    {
        if (table.data() != nullptr)
        {
            release_animation_resource(resources.registry, table);
        }
    }
    if (text_registry != nullptr)
    {
        engine_heap_free(text_registry);
    }
    for (const auto& [handle, allocation] : boot_pixel_allocations)
    {
        resources.d3d.pixel_shaders.erase(handle);
        resources.memory.free(allocation);
    }
}
} // namespace gbtg
