#include "scene_actor_services_state.h"
#include "../graphics/boot_shader_tables.h"
#include "../entities/mind_fields.h"
#include <cstddef>
#include "../entities/body_fields.h"
#include "../entities/player_fields.h"

// The State's construction: every service binding the scene's actors, bodies, minds and controls call.
namespace gbtg
{
namespace
{
// Statsheet rows are 0x87 bytes, each starting with its tag; the objTag_Null row (*0x0042A850) ends the sheet.
constexpr size_t kStatsheetRow = 0x87;

// Xbox 0x00133B10, EAX the statsheet: walks the rows until one is objTag_Null and returns 1.
i32 statsheet_find_null_row(std::span<const u8> rows, const char* null_tag)
{
    for (size_t row = 0;; row += kStatsheetRow)
    {
        if (row + kStatsheetRow > rows.size())
        {
            throw Error("statsheet has no objTag_Null row");
        }
        if (std::strcmp(reinterpret_cast<const char*>(rows.data() + row), null_tag) == 0)
        {
            return 1;
        }
    }
}

// Xbox 0x00133B70, EAX the statsheet: the rows before objTag_Null, plus one.
i32 statsheet_row_count(std::span<const u8> rows, const char* null_tag)
{
    i32 count = 0;
    for (size_t row = 0;; row += kStatsheetRow)
    {
        if (row + kStatsheetRow > rows.size())
        {
            throw Error("statsheet has no objTag_Null row");
        }
        if (std::strcmp(reinterpret_cast<const char*>(rows.data() + row), null_tag) == 0)
        {
            return count + 1;
        }
        ++count;
    }
}
} // namespace

void SceneActorServices::State::actor_callouts(u32 address, u32 id, const RuntimeActorCreationOptions* options)
{
    auto* object = object_of(scene.resolve_address(address, 0).data());
    if (object == nullptr || !actors->actors.contains(object->bytes.get()))
    {
        unbound("actor +0x8C callouts of an address outside the scene actors");
    }
    if (object->type == 5)
    {
        auto& player = players->player(*object);
        dispatch_actor_attachment_records(player.actor_instance.startup.resource.resource.data(), i32(id), nullptr,
            options, player.callout, 0);
        return;
    }
    auto& view = actors->view(*object);
    dispatch_actor_attachment_records(view.startup.resource.resource.data(), i32(id), nullptr, options,
        base_of(*object).base.callouts, 0);
}

RuntimeBodyStateContext SceneActorServices::State::body_state_context(u32 body_address)
{
    const auto bytes = scene.resolve_address(body_address, 0);
    auto* player = players->find_child_owner(bytes.data());
    if (player == nullptr || player->body.data() != bytes.data())
    {
        unbound("state action whose body has no player owner");
    }
    auto& instance = player->actor_instance;
    return RuntimeBodyStateContext{player->body, player->actor, player->body_flags,
        instance.animation.controller.get(), instance.shape, instance.startup.models.secondary.get(),
        instance.startup.secondary_mode, sounds, node_cache, player->secondary, player->second_receiver,
        [this, player](std::span<const u32> words)
        {
            // The body's actual vtable +0xC.
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            child_message(player->body_object, buffer.data());
        },
        player->body_selection,
        [this](u32 address, size_t size) -> std::span<const u8>
        {
            // State parameters are executable objparams (cloned by 0x00107720); their pointers are XBE
            // addresses. Size 0 asks for a NUL-terminated string.
            if (size == 0)
            {
                return {reinterpret_cast<const u8*>(image.string(address)), 1};
            }
            return image.memory(address, size);
        },
        player->mind,
        [this](u32 address) -> Vec3
        {
            if (address == 0)
            {
                throw Error("0x000412A0 reads an actor through address zero");
            }
            return actor_world_position(*actor_or_retired(address));
        }};
}

SceneActorServices::State::State(SceneFactoryRuntime& owner, Game& owner_game)
    : scene(owner),
      game(owner_game),
      image(*owner.executable),
      resources(owner.resources),
      startup{resources.registry, resources.error, resources.model_views, image.animation_tables,
          resources.actor_models, deferred, fence, resources.copy_types, shaders, *scratch, heap_free, live_actors,
          random},
      effect_models{resources.registry, resources.error, resources.model_views, deferred, fence, resources.copy_types,
          shaders, *scratch, heap_free},
      effects{effect_models, sounds, random, origin, default_euler},
      scares{scene.factory, image.pools, scene.objects, scare_flags,
          [this]() -> u32
          {
              return cases.next == nullptr ? 0u : address_of(cases.next); // DAT_00511B40
          },
          default_euler},
      intro_sound{reinterpret_cast<u32&>(health_style), intro_scale,
          reinterpret_cast<i16&>(scoreboard_globals.health_second_voice), intro_bits, sounds},
      camera{scene.factory, image.pools, image.string(0x00314E50), data(0x00314EB0, 0x80), g_flags_ea9c,
          camera_players,
          [this](FactoryObject& object, const std::array<u32, 3>& message)
          {
              if (object.bytes == nullptr)
              {
                  unbound("player camera message without a camera object");
              }
              u32 words[0x42]{message[0], message[1], message[2]};
              object_message(object, words);
          }},
      clock{pause, frame_delta, time_scale, pause_references[1], data(0x0030F144, 4), decay_index},
      resume{pause_references, data(0x0030F144, 4), decay_index},
      intro_notices{g_flags_ea9c, g_scene_flags->bytes[0], intro_pending, intro_name, cases, case_body_bits}
{
    actor_tags = strings(0x0042A854, 0xE9);
    object_tags = strings(0x0042A850, 0xEA);
    weapon_names = strings(0x00434104, 5);
    class_names = strings(0x004340EC, 5);
    mind_goal_names = strings(0x003397BC, 0x31);
    decay_enabled = data(0x0030F144, 4);
    std::memcpy(&intro_scale, data(0x00433BEC, 4).data(), 4);
    std::memcpy(&jump_tunables, data(0x00428680, sizeof(jump_tunables)).data(), sizeof(jump_tunables));
    std::memcpy(&skeleton_fetch_delay, data(0x004288AC, 4).data(), 4);
    // 0x00116DA0, from 0x0011CAC0 before scene startup.
    queue_limits[0] = 3;
    std::fill(class_queue_limits.begin(), class_queue_limits.end(), 99u);
    // Boot (0x00033030): DAT_00545AC4 is the statsheet resource and 0x00133B70 counts its rows up to
    // objTag_Null.
    statsheet = acquire_animation_resource(resources.registry, "aid_misc_ghoulies_statsheet_actors");
    if (statsheet.data() == nullptr)
    {
        throw Error("scene actors require aid_misc_ghoulies_statsheet_actors");
    }
    const char* null_tag = image.string(read_u32(data(0x0042A850, 4), 0));
    statsheet_find_null_row(statsheet, null_tag); // 0x00133B10; the boot drops its 1.
    statsheet_rows = u32(statsheet_row_count(statsheet, null_tag)); // 0x00133B70 into DAT_00545AC8
    case_body_bits = [this](const entity_detail::ListHead* node) -> const EntityBitBlock*
    {
        // A DAT_00511B40 node is a player's actor +0x93C (links[1]); its actor +0x95C +0x15C is the bit block.
        for (auto* player : players->instances())
        {
            if (static_cast<const void*>(&player->links[1]) == static_cast<const void*>(node))
            {
                player->refresh_body_views();
                return player->body_bits;
            }
        }
        unbound("case actor body bits (actor +0x95C +0x15C) of a node with no native player");
    };
    word_messages.memory_at = [this](u32 address) -> std::span<const u8>
    {
        // A message word points at a record of the size its reader takes (an animation event, a mix's tables),
        // or into a heap block heap_address registered.
        if (address >= 0x30000000u && address < next_heap_address)
        {
            auto block = heap_blocks.upper_bound(address);
            if (block != heap_blocks.begin())
            {
                --block;
                const size_t offset = address - block->first;
                if (offset < block->second.size())
                {
                    return block->second.subspan(offset);
                }
            }
            throw Error("message word names no registered heap block");
        }
        return scene.resolve_tail(address);
    };
    word_messages.reference_at = [this](u32 address) -> void*
    {
        if (const auto found = message_references.find(address); found != message_references.end())
        {
            return found->second;
        }
        const u8* bytes = scene.resolve_address(address, 0).data();
        return actors->actors.at(bytes);
    };
    word_messages.animation_record_at = [](u32) -> const AnimationRecord*
    {
        unbound("animation record words");
    };

    std::memcpy(&health_style, data(0x00433BE8, 4).data(), 4);
    brew = std::make_unique<RuntimeBrewMeter>(RuntimeBrewMeter{brew_state, brew_intensity, brew_counter,
        brew_textures, brew_half_widths, brew_widget, widget_context, data(0x00312A1C, sizeof(HudCounterPreset)), resources.registry,
        resources.error, address_zero});
    brew->rows = data(0x00312A30, 0x310).data(); // "challengeMenu_restart": butlersBrew_No, butlersBrew_Yes
    health_display = std::make_unique<RuntimePlayerHealthDisplay>(RuntimePlayerHealthDisplay{health_active,
        health_style, health_intensity, health_counter, health_mode, health_textures, health_widths,
        powerup_rows[0xA].active, // DAT_00512230, the Invulnerability powerup row.
        story_byte, data(0x00313A40, 5 * sizeof(HealthBandRow)), data(0x00313A18, sizeof(HudCounterPreset)), data(0x00313A2C, sizeof(HudCounterPreset)), resources.registry,
        resources.error});
    scoreboard_hud = std::make_unique<RuntimeScoreboardHud>(RuntimeScoreboardHud{scoreboard_globals, *brew,
        *health_display, overlay_digits, powerup_board, hud_enabled, g_flags_ea9c, score_refresh, score_points,
        health_style, difficulty, prompt_lock, prompt_owner, hud, hud_size[0], data(0x00313390, 12 * 0x80),
        0xFFFFFFFFu, resources.registry, resources.error, // DAT_00314F48.
        [this](RuntimeDialogueWidget* widget)
        {
            destroy_dialogue_widget(widget, resources.registry, *text_registry);
        },
        [this](RuntimeDialogueTextPool* pool)
        {
            release_dialogue_text_pool(pool, *text_registry);
        },
        [this](u16 voice)
        {
            stop_audio_event_voice(i16(voice), 0, voices);
        },
        [this]()
        {
            return runtime_area_dialogue_active(area_dialogue);
        }});
    {
        scoreboard_resources = std::make_unique<RuntimeScoreboardResources>(RuntimeScoreboardResources{
            resources.registry, resources.error, data(0x00314170, sizeof(HudCounterPreset))});
        scoreboard_update = std::make_unique<RuntimeScoreboardUpdate>(RuntimeScoreboardUpdate{*scoreboard_hud,
            *scoreboard_resources, audio.voices, g_scene_paused, *reinterpret_cast<const float*>(g_scene_times), g_scene_hold_time,
            g_scene_flags->bytes[0], challenge_mode, challenge_count, data(0x00314170, sizeof(HudCounterPreset)),
            data(0x00312A1C, sizeof(HudCounterPreset)), data(0x00312F8C, sizeof(HudCounterPreset)), 2.f, 0.6f, 0.8f, 1.5f, 0.1f, // DAT_0032B110, DAT_0032B248, DAT_0032B244, DAT_0032B210, DAT_0032B254.
            [this](std::span<u32> message)
            {
                // 0x00122D10: DAT_00511B40 holds the players' actor +0x93C nodes (links[1]).
                for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.case_head.next);
                     node != nullptr;)
                {
                    auto* current = node;
                    node = static_cast<entity_detail::ListHead*>(node->next);
                    NativePlayerInstance* owner = nullptr;
                    for (auto* player : players->instances())
                    {
                        if (static_cast<void*>(&player->links[1]) == static_cast<void*>(current))
                        {
                            owner = player;
                        }
                    }
                    if (owner == nullptr)
                    {
                        unbound("DAT_00511B40 node with no native player");
                    }
                    object_message(*object_of(owner->actor_bytes.data()), message);
                }
            },
            intro_scale, intro_bits, random, clock,
            [this](u32 actor) -> NativePlayerInstance*
            {
                for (auto* player : players->instances())
                {
                    if (player->actor_address == actor)
                    {
                        return player;
                    }
                }
                return nullptr;
            },
            *reinterpret_cast<const i32*>(g_scene_record + 0x104), intro_notices, data(0x00433B54, 0x18 + sizeof(HudCounterPreset))});
    }
    // Boot registers the callout hook (0x0004F610) with the other resource hooks.
    callout_resources = std::make_unique<RuntimeCalloutResources>(RuntimeCalloutResources{resources.registry,
        resources.error, linked_records,
        [this](const void* pointer)
        {
            return address_of(pointer);
        }});
    register_runtime_callout_resource_hook(resources.registry, *callout_resources);
    actors = std::make_unique<NativeActorFactoryOwner>(NativeActorFactoryBindings{startup, scene.factory, sounds,
        frame_objects,
        [this](Actor& actor, const void* message)
        {
            actor_message(actor, message);
        },
        empty_name,
        [this](const void* pointer)
        {
            return resources.memory.address_of(static_cast<const u8*>(pointer));
        },
        [this](const void* pointer)
        {
            return heap_address(pointer, sizeof(ModelPoseBuffer));
        },
        [this](Actor& actor, const Actor& other, const Vec3& push, const Vec3& contact, u16 bone)
        {
            std::array<u32, 9> message{0xE, address_of(&other), std::bit_cast<u32>(push.x),
                std::bit_cast<u32>(push.y), std::bit_cast<u32>(push.z), std::bit_cast<u32>(contact.x),
                std::bit_cast<u32>(contact.y), std::bit_cast<u32>(contact.z), bone};
            auto* object = object_of(scene.resolve_address(address_of(&actor), 0).data());
            object_message(*object, message);
        }});
    // 0x0003B3C0: an animation event becomes message {7, position, slot, event, phase} to the actor's vtable +0xC;
    // the event goes by a scene address bound for the call.
    actors->bindings.object_of = [this](const u8* bytes)
    {
        return object_of(bytes);
    };
    actors->bindings.object_address = [this](const u8* bytes)
    {
        return address_of(bytes);
    };
    actors->bindings.animation_message = [this](Actor& actor, const ActorAnimationMessage& message)
    {
        auto* event = const_cast<u8*>(message.event.data());
        u32 address = scene.find_address(event);
        const bool bound = address == 0;
        if (bound)
        {
            address = scene.bind_address({event, message.event.size()});
        }
        std::array<u32, 0x42> words{};
        words[0] = 7;
        words[1] = std::bit_cast<u32>(message.position);
        words[2] = message.slot;
        words[3] = address;
        words[4] = u32(message.phase);
        try
        {
            actor_message(actor, words.data());
        }
        catch (...)
        {
            if (bound)
            {
                scene.unbind_address(event);
            }
            throw;
        }
        if (bound)
        {
            scene.unbind_address(event);
        }
    };
    // The update slot goes with its actor's view, so a later view that reuses the memory binds its own.
    actors->released = [this](Actor& actor)
    {
        actor_updates.erase(&actor);
        // A player's view goes before its object's base destroy reports the release (released_binding), which keeps
        // the position: it is taken here, while the actor still lives.
        released_positions[&actor] = actor_world_position(actor);
        // A live cutscene's row may still name the actor: scene 2c5's script (type 0x60, 0x00112CC0) retires the
        // brambles a door clip holds, and 0x001232D0 frees them while the clip plays on. Retail's row keeps the
        // freed block, whose bytes the engine heap leaves alone; the host frees it, so the row lets go and the clip's
        // release (0x0004E9E0) skips it.
        if (cutscenes != nullptr)
        {
            for (auto& [instance, rows] : cutscenes->instances)
            {
                for (auto& row : rows.actors)
                {
                    if (row.actor == &actor)
                    {
                        row.actor = nullptr;
                    }
                }
            }
        }
        for (const auto& [record, row] : mind_target_rows)
        {
            if (row->target != nullptr && &row->target->actor == &actor)
            {
                retired_targets[row->target->address] = retire_target(*row, *row->target);
            }
        }
    };
    actors->bindings.unlink_active_node = [this](const u8* bytes)
    {
        unlink_active_node(bytes);
    };
    actors->install(scene.types);
    players = std::make_unique<NativePlayerFactoryOwner>(NativePlayerFactoryBindings{*actors,
        NativePlayerSceneBindings{scene.factory, image.pools, resources.registry, resources.error, random,
        input_release, input_activation, g_flags_ea9c, g_flag_ea98, player_count, active_strategies,
        address_zero,
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        actor_tags.data(), object_tags.data(), statsheet.data(), statsheet, weapon_names.data(), class_names.data(),
        read_u32(data(0x00338EE0, 4), 0), // initial_channel_word
        matrix_boundaries,
        [this](FactoryObject& object) -> RuntimePlayerAttachmentActor&
        {
            attaching_object = &object;
            attaching_bytes = object.bytes.get();
            return weapon_attachment_at(scene.find_address(object.bytes.get()));
        },
        attachment_actors,
        scale_exclusions, scale_exclusion_count, animation_classes, animation_class_count, data(0x00433D68, 0x10),
        data(0x00433CA8, 0x10), resources.loaded_sizes, image.state_tables, image.attack_sequences,
        animation_cache, animation_cache_count, image.state_row_names, g_actor_effect_nodes,
        [this](const Actor& actor)
        {
            return model_views_of(actor);
        },
        mind_settings, intro_sound, camera, cameras, g_scene_flags->bytes[0], g_script_30d10,
        g_script_30d0c, scene_name.data(), strategy_ranges, pause_groups, overrides, latches, game, clock,
        resume, query,
        [this](u32 address) -> RuntimeStrategyMessageActor*
        {
            return strategy_message_actor(address);
        },
        [this](u32 address) -> RuntimeMindTarget*
        {
            return address == 0 ? nullptr : mind_target_at(address);
        },
        targets, script_records, data(0x003398C8, 0x400), data(0x00217F18, 0x200), mind_goal_names.data(),
        goal_scratch,
        [this](std::span<const u8> table) -> const RuntimeMindGoalTable*
        {
            if (table.data() == nullptr)
            {
                return nullptr;
            }
            return image.goal_tables.at(table.data());
        },
        effects, node_cache, reset_matrix, nullptr, callout_objects, linked_records,
        *brew, *health_display, input_state,
        keyboard, voices, brews_taken, brew_accepted, prompt_lock, prompt_owner, difficulty, unlock_flags,
        scene_kills, brew_kills_required, story_byte, intro_pending, intro_name, cases, case_body_bits,
        std::span<ActorAttachmentSlot, 20>(g_actor_attachment_slots), queue_limits, class_queue_limits,
        [this](const ActorLiveAttachment* record) -> const RuntimeMindTarget*
        {
            refresh_mind_targets();
            if (record == nullptr)
            {
                return nullptr;
            }
            return mind_target_rows.at(record)->target.get();
        },
        [this](const void* owner) -> std::span<const u8>
        {
            // 0x00112970 stores the member's actor at link +8.
            auto& object = object_of_actor(*static_cast<Actor*>(const_cast<void*>(owner)));
            return {object.bytes.get(), object.size};
        },
        active_actors, notices, occupancy_initial, data(0x004287BC, 0x40),
        [this](const char* name, u32 actor) -> u32
        {
            // 0x0003DF40: the named template cloned (0x00107720) with the actor at +8, then made (0x00107520).
            auto clone = clone_executable_resource(image.pools, name);
            if (clone.bytes == nullptr)
            {
                return 0;
            }
            std::memcpy(clone.bytes.get() + 8, &actor, 4);
            FactoryObject* made = scene.keep_object(create_factory_object(scene.factory,
                {clone.bytes.get(), clone.size}));
            clone.bytes.reset(); // 0x000663E0.
            return made == nullptr || made->bytes == nullptr ? 0u : address_of(made->bytes.get());
        },
        [this](u32 address, std::span<const u32> message)
        {
            // The object's vtable +0xC; a player's body, mind or strategy goes through its own child dispatch.
            const u8* bytes = scene.resolve_address(address, 0).data();
            std::array<u32, 0x42> words{};
            std::copy(message.begin(), message.end(), words.begin());
            if (auto* owner = players->find_child_owner(bytes); owner != nullptr)
            {
                for (FactoryObject* child : {&owner->strategy_object, &owner->mind_object, &owner->body_object})
                {
                    if (child->bytes.get() == bytes)
                    {
                        child_message(*child, words.data());
                        return;
                    }
                }
            }
            auto* object = object_of(bytes);
            if (object == nullptr)
            {
                unbound("strategy message to an address that is not a scene object");
            }
            object_message(*object, words);
        },
        [this](u32 address) -> RuntimeStrategyBodyView
        {
            // A player body (actor +0x95C) by its retail address.
            auto* player = players->find_child_owner(scene.resolve_address(address, 0).data());
            if (player == nullptr || player->body.empty() || scene.address_of(player->body.data()) != address)
            {
                unbound("held body view of an address that is not a player body");
            }
            return RuntimeStrategyBodyView{address, player->body, player->body_bits};
        },
        [this](const Actor& actor) -> RuntimeStrategyBodyView
        {
            auto& object = object_of_actor(const_cast<Actor&>(actor));
            if (object.type != 5)
            {
                // 0x000B4F50 loads actor +0x95C before 0x000B4EF0 decides whether to touch it. Only a player
                // has a body there; other actors give the word and no body bytes.
                const u32 word = object.size >= 0x960 ? read_u32({object.bytes.get(), object.size}, 0x95C) : 0;
                if (word != 0)
                {
                    unbound("target body view of a factory type " + std::to_string(object.type) + " actor");
                }
                return RuntimeStrategyBodyView{0, {}, nullptr};
            }
            auto& player = players->player(object);
            if (player.body.empty())
            {
                return RuntimeStrategyBodyView{0, {}, nullptr};
            }
            return RuntimeStrategyBodyView{scene.address_of(player.body.data()), player.body, player.body_bits};
        },
        [this](const Actor& actor) -> const RuntimeBodyModelNodes*
        {
            return model_views_of(actor).nodes; // Actor +0x204 block directory.
        },
        jump_tunables.head_marker_index,
        [this](const Actor& actor)
        {
            return address_of(object_of_actor(const_cast<Actor&>(actor)).bytes.get());
        },
        [this](const Actor& actor) -> std::span<const u8>
        {
            auto& object = object_of_actor(const_cast<Actor&>(actor));
            return {object.bytes.get(), object.size};
        },
        [this](u32 address) -> std::span<const u8>
        {
            // A scene object's whole instance (a weapon's +0xD8C owner lies past the actor's 0xCE0); otherwise the
            // words through +0xCDC that 0x000AB2D0 reads.
            if (auto* object = object_of(scene.resolve_address(address, 0).data()); object != nullptr)
            {
                return {object->bytes.get(), object->size};
            }
            return scene.resolve_address(address, 0xCE0);
        },
        [this](u32 address) -> const char*
        {
            return image.string(address);
        },
        [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                throw Error("a strategy reads an actor through address zero");
            }
            return actor_or_retired(address);
        },
        skeleton_fetch_delay, data(0x004288B0, 0x40), sounds,
        [this](u32 address) -> u8*
        {
            return resources.memory.pointer(address);
        },
        image.attack_tables,
        [this](Actor& actor, u32 object) -> bool
        {
            return release_actor_frame_object(actor, object); // 0x0003DE30
        },
        [this]() -> GhoulySpawnContext&
        {
            return *ghouly_spawns;
        },
        [this]() -> std::span<const u8>
        {
            return story_bits;
        },
        data(0x00428734, 0x40)}});
    players->install(scene.types);
    players->bindings.scene.grim_reaper_parameters = data(0x00302B7C, 0x38);
    players->bindings.scene.chicken_parameters = data(0x00302B5C, 0x18);
    players->bindings.scene.haunted_picture_movement = data(0x00302E80, 0x34);
    players->bindings.scene.imp_flying_parameters = data(0x00303124, 0x28);
    players->bindings.scene.imp_ninja_parameters = data(0x00303198, 0x18);
    players->bindings.scene.jessie_clyde_parameters = data(0x003031B8, 0x2C);
    players->bindings.scene.scare_at = [this](const Vec3& point, u32 actor, float intensity, float scale, i32 broadcast,
                                           u32 value)
    {
        request_actor_scare_at(point, actor, intensity, scale, broadcast, value, scares); // 0x0013B270
    };
    players->bindings.scene.mummy_cursed_parameters = data(0x00428848, 0x40);
    players->bindings.scene.create_patched = [this](const char* name,
                                                 const std::function<void(std::span<u8>)>& patch) -> u32
    {
        // 0x00107720 clones the named template, the caller patches the clone, 0x00107520 makes it, 0x000663E0.
        auto clone = clone_executable_resource(image.pools, name);
        if (clone.bytes == nullptr)
        {
            unbound(std::string("cursed mummy spawn template ") + name + " that does not clone");
        }
        patch({clone.bytes.get(), clone.size});
        FactoryObject* made = scene.keep_object(create_factory_object(scene.factory,
            {clone.bytes.get(), clone.size}));
        clone.bytes.reset();
        return made == nullptr || made->bytes == nullptr ? 0u : address_of(made->bytes.get());
    };
    players->bindings.scene.activate = [this](u32 address)
    {
        activate_actor_bind_tree(*actors->actors.at(scene.resolve_address(address, 0).data())); // 0x00112CC0
    };
    players->bindings.scene.burst = [this](u32 address, float lifetime, const char* name)
    {
        Actor& actor = *actors->actors.at(scene.resolve_address(address, 0).data());
        burst_runtime_actor_into_bits(actor, address, lifetime, name, empty_name,
            [this](std::span<const u8> parameters)
            {
                scene.keep_object(create_factory_object(scene.factory, parameters)); // 0x00107520
            }); // 0x0013B1F0
    };
    players->bindings.scene.scare_near = [this](const Actor& actor, float radius)
    {
        if (director_targets == nullptr)
        {
            unbound("0x0013B660's DAT_00511B38 walk before the director targets");
        }
        scare_near_runtime_actor(actor, radius, director_targets->marks(),
            [this](u32 address, std::span<u32> words)
            {
                auto* object = object_of(scene.resolve_address(address, 0).data());
                if (object == nullptr)
                {
                    unbound("0x0013B660 message to an address that is not a scene object");
                }
                object_message(*object, words);
            }); // 0x0013B660
    };
    players->bindings.scene.knock_back = [this](NativePlayerInstance& player)
    {
        player.refresh_views();
        auto& graph = body_messages[player.body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, player);
        }
        auto& entry = graph->damage_entry();
        // DAT_00511B20 - 0x934: the first background, or 0.
        auto* first = backgrounds->first_background();
        RuntimeBodyAttachedActor* background = first == nullptr ? nullptr
            : &attached_actor_at(address_of(first->bytes.data()));
        receive_runtime_body_knock_back(player.body, player.actor, player.actor_bytes, background, entry); // 0x00020690
    };
    players->bindings.scene.event_at = [this](u32 address) -> std::span<const u8>
    {
        // The animation event 0x0003B3C0 bound to a scene address for message 7; kind +0xC, name +0x10.
        return scene.resolve_tail(address);
    };
    players->bindings.scene.select_request = [this](NativePlayerInstance& player, u16 key)
    {
        auto& graph = mind_messages[player.mind.data()];
        if (graph == nullptr)
        {
            unbound("0x0001D210 on a body whose mind has no message graph");
        }
        return graph->select_attachment_request(key);
    };
    players->bindings.scene.start_meter = [this](u32 actor, u32 target, float value)
    {
        start_runtime_baddie_meter(actor, target, value, scoreboard_hud->white, scoreboard_globals,
            resources.registry, resources.error); // 0x0012E5E0
    };
    players->bindings.scene.stop_meter = [this]()
    {
        stop_runtime_baddie_meter(scoreboard_hud->white, scoreboard_globals, resources.registry); // 0x0012E6C0
    };
    players->bindings.scene.meter_shown = [this]()
    {
        return scoreboard_globals.word_a18 != 0; // DAT_00545A18
    };
    players->bindings.scene.meter_actor = [this]()
    {
        return scoreboard_globals.word_a20; // DAT_00545A20
    };
    players->bindings.scene.refresh_pause = [this]()
    {
        refresh_pause_groups();
    };
    players->bindings.scene.mind_bytes_at = [this](u32 address) -> std::span<const u8>
    {
        return scene.resolve_tail(address); // The mind's words through its target (+0x10).
    };
    players->bindings.scene.first_live_attachment_at = [this](u32 actor_address) -> u32
    {
        return first_live_attachment_at(actor_address);
    };
    players->bindings.scene.attached_actor_at = [this](u32 address) -> RuntimeBodyAttachedActor&
    {
        return attached_actor_at(address);
    };
    players->bindings.scene.medusa_parameters = data(0x004287F0, 0x58);
    players->bindings.scene.hunchback_parameters = data(0x00428770, 0x50);
    players->bindings.scene.keep_object = [this](FactoryObject object)
    {
        released_weapons.push_back(std::move(object));
    };
    players->bindings.scene.actor_callouts = [this](u32 actor, u32 id, const RuntimeActorCreationOptions* options)
    {
        actor_callouts(actor, id, options);
    };
    players->bindings.scene.light_cones = &light_cones;
    players->bindings.scene.contact_actor = [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
    {
        return segment_contact_actor(probe);
    };
    players->bindings.scene.contact_report = [this](NativePlayerInstance& player, RuntimeContactReport& report)
    {
        // {0x41, word, velocity, object, contacts, point, reply}.
        std::array<u32, 0x42> words{};
        words[0] = report.id;
        words[1] = u32(report.word);
        words[2] = std::bit_cast<u32>(report.velocity.x);
        words[3] = std::bit_cast<u32>(report.velocity.y);
        words[4] = std::bit_cast<u32>(report.velocity.z);
        words[5] = address_of(&report.object->actor);
        words[7] = std::bit_cast<u32>(report.point.x);
        words[8] = std::bit_cast<u32>(report.point.y);
        words[9] = std::bit_cast<u32>(report.point.z);
        words[10] = report.reply;
        dispatch_contact_words(player, words, report.contacts, 6);
    };
    players->bindings.scene.markers = [this]()
    {
        return static_cast<const u8*>(script_records); // DAT_0051108C.
    };
    players->bindings.scene.create_object = [this](std::span<const u8> parameters)
    {
        scene.keep_object(create_factory_object(scene.factory, parameters)); // 0x00107520
    };
    players->bindings.scene.body_state = [this](NativePlayerInstance& player)
    {
        if (player.body.empty())
        {
            unbound("0x0001FEB0 on a player with no body");
        }
        return body_state_context(scene.address_of(player.body.data()));
    };
    // DAT_00511B40 holds the players' actor +0x93C nodes (links[1]); each gets the message through its vtable +0xC.
    // Xbox 0x00122D10
    players->bindings.scene.tell_players = [this](std::span<const u32> message)
    {
        for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.case_head.next);
             node != nullptr;)
        {
            auto* current = node;
            node = static_cast<entity_detail::ListHead*>(node->next);
            NativePlayerInstance* owner = nullptr;
            for (auto* player : players->instances())
            {
                if (static_cast<void*>(&player->links[1]) == static_cast<void*>(current))
                {
                    owner = player;
                }
            }
            if (owner == nullptr)
            {
                unbound("DAT_00511B40 node with no native player");
            }
            std::array<u32, 0x42> words{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
            object_message(*object_of(owner->actor_bytes.data()), words);
        }
    };
    players->bindings.scene.queue_event = [this](std::span<const u8> message)
    {
        script_queue_event(game, message); // 0x00116810
    };
    players->bindings.scene.audio_banks = &audio.streams;
    players->bindings.scene.blob_shadows = &blob_shadows;
    players->bindings.scene.controller_message = [this](GhoulyController& controller, void* data)
    {
        player_controller_message(controller, data);
    };
    players->bindings.scene.attach_actor_messages = [this](FactoryObject& object, NativePlayerInstance& player)
    {
        attach_player_messages(object, player);
    };
    players->bindings.scene.attach_body_methods = [this](NativePlayerInstance& player)
    {
        if (!player.body.empty())
        {
            body_graph(player);
        }
    };
    fx_custom = std::make_unique<RuntimeFxCustomContext>(RuntimeFxCustomContext{
        [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actor_or_retired(address);
        },
        [this](Actor& actor) -> ActorFrameState&
        {
            // By the actor itself: a player's body action is made while the player is still being constructed.
            return actors->view(actor).frame;
        },
        [this](const Actor& actor) -> std::span<const u8>
        {
            auto& object = object_of_actor(const_cast<Actor&>(actor));
            return {object.bytes.get(), object.size};
        },
        frame_objects, attachment_draws, resources.registry, resources.error, sounds, shaders, random, pause,
        frame_delta, time_scale,
        [this]() -> BillboardFlushContext
        {
            return BillboardFlushContext{g_billboard_queue, *dialogue_draw, d3d_device.outer(), render_image};
        },
        [this](Actor& actor) -> RuntimeActorStartupState&
        {
            return actors->view(actor).startup;
        },
        [this](Actor& actor) -> std::span<u8>
        {
            auto& object = object_of_actor(actor);
            return {object.bytes.get(), object.size};
        },
        [this](const u8* bytes)
        {
            return address_of(bytes);
        },
        [this](const u8* resource)
        {
            return resources.memory.address_of(resource);
        },
        [this](Actor& actor, u32 object)
        {
            return release_actor_frame_object(actor, object); // 0x0003DE30
        }});
    install_runtime_fx_custom_classes(scene.types, *fx_custom);
    players->minds.install_cinema(scene.types);
    players->defer_release = true;
    released_binding = [this](const u8* bytes)
    {
        const u32 address = scene.address_of(bytes);
        if (const auto actor = actors->actors.find(bytes); actor != actors->actors.end())
        {
            const FactoryObject* object = object_of(bytes);
            const size_t size = object != nullptr ? object->size : 0;
            released_actors[address] = ReleasedActor{std::vector<u8>(bytes, bytes + size),
                actor_world_position(*actor->second)};
        }
        for (auto* player : players->instances())
        {
            if (player->actor_bytes.data() == bytes)
            {
                const auto kept = released_positions.find(&player->actor);
                const Vec3 position = kept != released_positions.end() ? kept->second
                                                                       : actor_world_position(player->actor);
                if (kept != released_positions.end())
                {
                    released_positions.erase(kept);
                }
                released_actors[address] = ReleasedActor{
                    std::vector<u8>(player->actor_bytes.begin(), player->actor_bytes.end()), position};
            }
        }
        if (const auto found = active_nodes.find(address); found != active_nodes.end())
        {
            auto** link = &latches;
            while (*link != nullptr && *link != found->second.get())
            {
                link = &(*link)->next;
            }
            if (*link != nullptr)
            {
                *link = (*link)->next;
            }
            active_nodes.erase(found);
        }
        if (classes != nullptr)
        {
            classes->release_view(bytes);
        }
        background_views.erase(bytes);
        player_messages.erase(bytes);
        mind_messages.erase(bytes);
        camera_mind_parents.erase(bytes);
        body_messages.erase(bytes);
        bases.erase(bytes);
        // The prop contact view holds the object's bytes; a later object at this actor address must build its own.
        if (const auto owner = prop_contact_owners.find(bytes); owner != prop_contact_owners.end())
        {
            prop_contact_views.erase(owner->second);
            prop_contact_owners.erase(owner);
        }
        for (auto* player : players->instances())
        {
            if (player->actor_bytes.data() == bytes)
            {
                actor_updates.erase(&player->actor);
                model_views.erase(&player->actor);
                // The contact view holds the actor; a later instance at this address must build its own.
                player_contact_views.erase(player);
                players->release_player(bytes);
                return;
            }
        }
    };
    scene.released = released_binding;

    frame = std::make_unique<ActorFrameContext>(ActorFrameContext{effects, nullptr, node_cache, reset_matrix,
        matrix_boundaries, pause, frame_delta, time_scale, 0, pause_references[1], decay_enabled[decay_index]});
    emitter = std::make_unique<NativeSceneEmitterBindings>(NativeSceneEmitterBindings{*frame,
        g_actor_effect_nodes, clock, display_view, g_runtime_profile.challenges.bytes, g_scene_flags->bytes[0],
        g_flags_ea9c,
        emitter_players, resume, intro_bits, intro_notices, emitters,
        [this](u32 id, RuntimeFxEmitterPlayer& player, u32 kind, float amount, u32 extra)
        {
            if (powerup_awards == nullptr)
            {
                unbound("powerup award 0x00123E70 before its context");
            }
            award_runtime_powerup(i32(id), scene.address_of(player.actor_bytes.data()), kind, amount, extra,
                *powerup_awards); // 0x00123E70, EDX the id
        }});
    marker_objects = std::make_unique<RuntimeMarkerObjects>(RuntimeMarkerObjects{scene.factory, image.pools,
        markers, address_zero, empty_name, resources.registry, resources.error, random, scene.objects,
        [this](FactoryObject& object) -> ActorAnimationState&
        {
            return actors->view(object).animation;
        },
        [this](FactoryObject& object, std::span<const u32> message)
        {
            object_message(object, {const_cast<u32*>(message.data()), message.size()});
        }});
    load_runtime_weapon_stat_tables(weapon_stats, resources.registry, resources.error); // 0x001486C0
    // 0x0011C3F0 keeps the ghoul intro's names (DAT_00506E40) for the run.
    ghoul_intro_names = acquire_animation_resource(resources.registry, "aid_loctext_ghoulies_actor_ghoulyintro");
    if (ghoul_intro_names.data() == nullptr)
    {
        resource_error(resources.error, 2); // 0x00032700
    }
    ghoul_intro_tunables = std::make_unique<RuntimeGhoulIntroTunables>(RuntimeGhoulIntroTunables{
        data(0x0030C748, 0x34), 0.85f, 0.9f, // DAT_0032B17C, DAT_0032B174.
        0.5f, 2.7f, 0.6f, // DAT_0032B0F4, DAT_0032B580, DAT_0032B248.
        1.5f, 3.1415927f}); // DAT_0032B210, DAT_0032B148.
    ghoul_intro_context = std::make_unique<RuntimeGhoulIntroContext>(RuntimeGhoulIntroContext{ghoul_intro, notices,
        *ghoul_intro_tunables, g_flags_ea9c, clock, active_strategies, door_camera_hold, random, actor_tags.data(),
        object_tags.data(), ghoul_intro_names, resources.text, resources.registry, resources.error, text_registry,
        resources.metrics, widget_context.viewport_width, next_widget_id, render_width, render_height, audio.streams,
        [this](const Actor& actor, std::span<const u32> message)
        {
            object_message(object_of_actor(const_cast<Actor&>(actor)),
                {const_cast<u32*>(message.data()), message.size()});
        },
        [this](const Actor& actor) -> std::span<u8>
        {
            FactoryObject& object = object_of_actor(const_cast<Actor&>(actor));
            if (object.type != 5)
            {
                unbound("ghoul intro actor of factory type " + std::to_string(object.type) + " has no player mind");
            }
            auto& player = players->player(object);
            player.refresh_views();
            return player.mind;
        },
        [this](const Actor& actor) -> std::span<const u8>
        {
            FactoryObject& object = object_of_actor(const_cast<Actor&>(actor));
            if (object.type != 5)
            {
                unbound("ghoul intro actor of factory type " + std::to_string(object.type) + " has no player body");
            }
            auto& player = players->player(object);
            player.refresh_views();
            return player.body;
        },
        [this](u32 address) -> Vec3
        {
            // Mind +0x10 names a DAT_00545988 record; its position is record +0x20.
            std::span<const u8> bytes;
            if (const auto found = target_views.find(address); found != target_views.end())
            {
                bytes = found->second->bytes;
            }
            else if (const auto retired = retired_targets.find(address); retired != retired_targets.end())
            {
                bytes = retired->second->bytes;
            }
            else
            {
                unbound("ghoul intro mind target outside the DAT_00545988 records");
            }
            return Vec3{read_f32(bytes, 0x20), read_f32(bytes, 0x24), read_f32(bytes, 0x28)};
        },
        [](u8 mask, i32 release)
        {
            script_update_decay_latches(mask, release); // 0x00117400
        }});
    weapon_services = std::make_unique<RuntimeWeaponServices>(RuntimeWeaponServices{weapon_stats,
        [this](u16 tag) -> const char*
        {
            return image.string(read_u32(data(0x0042A850 + u32(tag) * 4, 4), 0)); // DAT_0042A850[tag].
        },
        resources.text, wide_scratch,
        [this](i32 points, u32 category, const char16_t* text, i32 quiet)
        {
            if (score == nullptr)
            {
                unbound("weapon score award before the score tables");
            }
            award_runtime_score(points, category, text, quiet, *score); // 0x0012DE90
        },
        broken_parts, sounds, std::span<float, 12>(sampling_scratch.data(), 12), sampling_active,
        [this](const Actor& actor, u32 address) -> std::span<const u8>
        {
            if (address < 0x10000000u)
            {
                if (actor.model == nullptr || address > actor.model->cpu.size())
                {
                    throw Error("weapon model offset lies outside its model");
                }
                return actor.model->cpu.subspan(address);
            }
            if (address >= 0x40000000u)
            {
                return resources.memory.tail(address);
            }
            return scene.resolve_tail(address);
        },
        [this](Actor& actor, ActorFrameAttachment* head)
        {
            ActorFrameReleaseContext frames{actors->bindings.factory, actors->bindings.frame_objects,
                actors->bindings.object_of,
                [this, &actor](const ActorReferenceMessage& message)
                {
                    // Message 0x14 names the released object by its retail address (0x00027B20 reads it).
                    std::array<u32, 0x42> words{message.id, address_of(message.value)};
                    actors->bindings.message(actor, words.data());
                }};
            release_actor_frame_attachments(head, frames); // 0x0003DE90
        },
        g_flags_ea9c,
        [this](const Actor& actor) -> u32
        {
            for (const auto& [bytes, native] : actors->actors)
            {
                if (native == &actor)
                {
                    return scene.address_of(bytes);
                }
            }
            unbound("weapon message to an actor outside the scene");
        },
        [this](u32 address) -> std::span<const u8>
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            return player.body;
        },
        *marker_objects,
        [this](FactoryObject object)
        {
            return scene.keep_object(std::move(object));
        },
        display_view, occupancy_initial, default_euler, scoreboard_globals, data(0x00312F8C, sizeof(HudCounterPreset)),
        [this](u32 holder)
        {
            auto& player = players->player_at(holder);
            player.refresh_views();
            detach_runtime_body_actor(player.body, player.body_attachment); // 0x0001B4C0
        },
        [this](const void* pointer, size_t size) -> u32
        {
            const u32 found = scene.find_address(pointer);
            if (found != 0)
            {
                return found;
            }
            weapon_bound.insert(pointer);
            return scene.bind_address({static_cast<u8*>(const_cast<void*>(pointer)), size});
        },
        [this](const void* pointer)
        {
            if (weapon_bound.erase(pointer) != 0)
            {
                scene.unbind_address(pointer);
            }
        },
        [this](u32 address) -> void*
        {
            return const_cast<u8*>(scene.resolve_address(address, 0).data());
        },
        contact_totals, contact_session,
        [this](Actor& actor) -> ActorAnimationState*
        {
            return actors->animations.at(&actor);
        },
        actors->bindings.frame_objects, audio.voices, g_flag_ea98,
        [this]() -> CutsceneRuntimeContext&
        {
            if (cutscenes == nullptr)
            {
                unbound("weapon cutscene before the scene cutscene runtime");
            }
            return *cutscenes;
        }});
    weapon_services->trail = [this](u32 address)
    {
        return ActorTrailContext{scene.factory, scene.objects, address, pause, frame_delta, time_scale, pal,
            pal_override, empty_name};
    };
    weapon_services->startup = [this]() -> RuntimeActorStartupContext&
    {
        return startup;
    };
    weapon_services->target_at = [this](u32 address) -> RuntimeMindTarget*
    {
        return mind_target_at(address);
    };
    weapon_services->player_cells = [this](u32 address) -> entity_detail::NavigationCellRegistration**
    {
        NativePlayerInstance* player = players->find_player_at(address);
        if (player == nullptr)
        {
            unbound("weapon reads the +0xCE8 cells of an actor that is no player");
        }
        return &player->navigation;
    };
    weapon_services->pal = &pal;
    candle_services = std::make_unique<RuntimeCandleServices>(RuntimeCandleServices{shaders, point_lights, g_flag_2138,
        g_flag_2104, g_billboard_queue, pause_references, decay_enabled, decay_index, candle_hit, candle_spot});
    weapon_services->candle = [this]() -> RuntimeCandleServices&
    {
        return *candle_services;
    };
    weapon_services->object_of = [this](const u8* bytes)
    {
        return object_of(bytes);
    };
    weapon_services->pal_override = &pal_override;
    weapon_services->effects = &effects;
    weapon_services->display = [this]() -> RuntimeDisplaySetupContext&
    {
        return *display_setup;
    };
    weapon_services->backgrounds = [this]()
    {
        return background_list();
    };
    // Xbox 0x0001B680, ECX body
    weapon_services->empty_hand = [this](const Actor& holder)
    {
        auto& object = object_of_actor(const_cast<Actor&>(holder));
        if (object.type != 5)
        {
            unbound("weapon holder hand of a factory type " + std::to_string(object.type) + " actor");
        }
        auto& player = players->player(object);
        if (player.body.empty() || player.body_flags == nullptr)
        {
            unbound("weapon holder without a body (actor +0x95C)");
        }
        if (body_record(player.body).attached == 0)
        {
            return;
        }
        const u32 zero = 0;
        body_record(player.body).first_receiver = zero;
        player.first_receiver = nullptr;
        player.body_flags->bytes[4] &= 0xFB;
        body_record(player.body).variant[0] = 0;
        body_record(player.body).attached = zero;
        player.attached = nullptr;
    };
    weapon_services->hold_secondary = [this](const Actor& holder, u32 weapon)
    {
        auto& object = object_of_actor(const_cast<Actor&>(holder));
        if (object.type != 5)
        {
            unbound("weapon holder +0x4C of a factory type " + std::to_string(object.type) + " actor");
        }
        auto& player = players->player(object);
        if (player.body.empty())
        {
            unbound("weapon holder without a body (actor +0x95C)");
        }
        if (weapon == 0)
        {
            player.secondary = nullptr;
            player.second_receiver = nullptr;
        }
        else
        {
            auto& attached = weapon_attachment_at(weapon);
            player.secondary = &attached.object;
            player.second_receiver = &attached.receiver;
        }
        body_record(player.body).unk_4C = weapon;
        body_record(player.body).second_receiver = weapon;
    };
    classes = std::make_unique<NativeSceneActorClassOwner>(NativeSceneActorClassBindings{*actors, *marker_objects,
        effects, prompt_lock, area_dialogue, random, occupancy_initial,
        [this](FactoryObject& object) -> ActorBaseContext&
        {
            return base_of(object).base;
        },
        [this](u32 address)
        {
            // The rest of the instance from the address, for reads at retail offsets.
            return scene.resolve_tail(address);
        },
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        [this](u32 address, std::span<u32> message)
        {
            const auto bytes = scene.resolve_address(address, 0);
            auto* object = object_of(bytes.data());
            if (object == nullptr)
            {
                unbound("message to an object outside the scene");
            }
            object_message(*object, message);
        },
        [this](FactoryObject object)
        {
            return scene.keep_object(std::move(object));
        },
        word_messages, *emitter,
        [this](u32 address)
        {
            return object_of(scene.resolve_address(address, 0).data());
        },
        [this]() -> RuntimeActorSnapshots&
        {
            if (snapshots == nullptr)
            {
                unbound("actor snapshots before the scene's draw contexts");
            }
            return *snapshots;
        },
        ambient_points_enabled, g_flag_2104,
        [this](const void* pointer, size_t size)
        {
            return heap_address(pointer, size);
        },
        [this](u32 address, size_t size) -> std::span<const u8>
        {
            if (size == 0)
            {
                const char* text = image.string(address);
                return {reinterpret_cast<const u8*>(text), std::strlen(text) + 1};
            }
            return image.memory(address, size);
        },
        resources.copy_types,
        [this](const Actor& actor) -> const ActorMatrixBindings&
        {
            return actor_matrix_bindings(actor);
        },
        matrix_boundaries,
        [this](FactoryObject& object)
        {
            refresh_shape_poses(object);
        },
        global_attachments, image.pools,
        [this](const entity_detail::LatchNode& node) -> u32
        {
            for (const auto& [address, latch] : active_nodes)
            {
                if (latch.get() == &node)
                {
                    return address;
                }
            }
            unbound("an active-list node outside the scene's actors");
        },
        [this]()
        {
            if (director_targets == nullptr)
            {
                unbound("DAT_00511B38 walk before the director targets");
            }
            return director_targets->marks();
        },
        [this]()
        {
            return case_actors();
        },
        query,
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return segment_contact_actor(probe);
        },
        default_euler, navigation_nodes, *weapon_services,
        [this](ActorEffectNode& effect, float step)
        {
            tick_owned_effect(effect, step);
        },
        effect_collision,
        [this]() -> RuntimeModelSoundRelease&
        {
            if (effect_release == nullptr)
            {
                unbound("effect node release before the scene's effect views");
            }
            return *effect_release;
        },
        pal, pal_override,
        [this]() -> GhoulySpawnContext&
        {
            if (ghouly_spawns == nullptr)
            {
                unbound("0x0013E790 before the scene's ghouly spawn queues");
            }
            return *ghouly_spawns;
        },
        [this]()
        {
            return background_list();
        },
        [this](u32 address, i32 part, const Vec3& source, u32 value)
        {
            with_background_context(background_at(address), [&](Actor& actor, BackgroundMessageContext& context)
            {
                break_background_part_if_ready(actor, part, source, value, context.zero, context.parts); // 0x00045D40
            });
        },
        [this](u32 address, i32 part)
        {
            hold_background_part(background_at(address).state.volumes, part); // 0x00046440
        },
        [this]() -> RuntimeActorPauseGroups&
        {
            return refresh_pause_groups();
        },
        [this](std::span<const u8> message)
        {
            script_queue_event(game, message); // 0x00116810
        },
        point_lights, audio.streams,
        [this](const Actor& actor) -> std::span<const u8>
        {
            for (auto* player : players->instances())
            {
                if (&player->actor == &actor)
                {
                    player->refresh_body_views();
                    return player->body;
                }
            }
            unbound("Super Scary dropper reads +0x95C of an actor that is not a player");
        },
        empty_name,
        [this](u32 texture) -> u8*
        {
            return resources.memory.pointer(texture);
        },
        &clock, &display_view, &screen_tint, &g_flags_ea9c,
        [this]() -> u32
        {
            // DAT_00511B40 and its body's +0x15C: byte 5 bit 4.
            if (cases.next == nullptr)
            {
                return 0;
            }
            const EntityBitBlock* bits = case_body_bits(static_cast<const entity_detail::ListHead*>(cases.next));
            if (bits == nullptr)
            {
                unbound("the case player's body bits (+0x95C +0x15C)");
            }
            return (bits->bytes[5] >> 4) & 1u;
        }});
    classes->install(scene.types);
    classes->defer_release = true;
    install_runtime_move_action_classes(scene.types, [this](u32 address) -> std::span<u8>
    {
        return scene.resolve_address(address, sizeof(BodyRecord));
    });
    install_runtime_body_state_classes(scene.types, [this](u32 body_address)
    {
        return body_state_context(body_address);
    },
    [this](u32 body_address)
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("hit state whose body has no player owner");
        }
        std::span<const u8> target;
        if (!player->mind.empty() && mind_record(player->mind).action_record != 0)
        {
            target = target_views.at(mind_record(player->mind).action_record)->bytes;
        }
        return RuntimeHitStateLifecycle{player->body, player->actor, player->actor_bytes, player->body_flags, target};
    },
    [this](u32 body_address) -> RuntimeDizzyStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("dizzy state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->dizzy_context();
    },
    [this](u32 body_address) -> RuntimeKnockdownContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("knockdown state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->knockdown_context();
    },
    [this](u32 body_address) -> RuntimeDieStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("die state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->die_context();
    },
    [this](u32 body_address) -> RuntimeAnimStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("anim state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->anim_context();
    },
    [this](u32 body_address) -> RuntimePickupStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("pick-up state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->pickup_context();
    },
    [this](u32 body_address) -> RuntimePlayerDeathContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("player death state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->player_death_context();
    },
    [this](u32 body_address) -> RuntimeGrabbedStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("grabbed hold state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->grabbed_context();
    },
    [this](u32 body_address) -> RuntimeThrowStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("throw state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->throw_context();
    });
    install_runtime_drone_appear_state_class(scene.types,
        [this](u32 body_address) -> RuntimeDroneAppearStateContext&
        {
            const auto bytes = scene.resolve_address(body_address, 0);
            auto* player = players->find_child_owner(bytes.data());
            if (player == nullptr || player->body.data() != bytes.data())
            {
                unbound("drone appear state whose body has no player owner");
            }
            auto& graph = body_messages[player->body.data()];
            if (graph == nullptr)
            {
                graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
            }
            return graph->drone_appear_context();
        });
    install_runtime_arc_state_class(scene.types, [this](u32 body_address) -> RuntimeArcStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("arc state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->arc_context();
    });
    install_runtime_walk_state_class(scene.types, [this](u32 body_address) -> RuntimeWalkStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("walk state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->walk_context();
    });
    install_runtime_jump_on_state_class(scene.types, [this](u32 body_address) -> RuntimeJumpOnStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("jump on/off state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->jump_on_context();
    });
    install_runtime_arc_attack_state_class(scene.types, [this](u32 body_address) -> RuntimeArcAttackStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("arc attack state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->arc_attack_context();
    });
    install_runtime_generate_ghouly_state_class(scene.types, [this](u32 body_address) -> RuntimeGenerateGhoulyContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("generate ghouly state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->generate_ghouly_context();
    });
    install_runtime_yoyo_spin_state_class(scene.types, [this](u32 body_address) -> RuntimeYoYoSpinStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("yo-yo spin state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->yoyo_spin_context();
    });
    install_runtime_spawn_ghouly_state_class(scene.types, [this](u32 body_address) -> RuntimeSpawnGhoulyStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("spawn ghouly state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->spawn_ghouly_context();
    });
    install_runtime_slide_state_class(scene.types, [this](u32 body_address) -> RuntimeSlideStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("slide state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->slide_context();
    });
    install_runtime_scrabble_state_class(scene.types, [this](u32 body_address) -> RuntimeScrabbleStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("scrabble state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->scrabble_context();
    });
    install_runtime_look_for_weapon_state_class(scene.types,
        [this](u32 body_address) -> RuntimeLookForWeaponStateContext&
        {
            const auto bytes = scene.resolve_address(body_address, 0);
            auto* player = players->find_child_owner(bytes.data());
            if (player == nullptr || player->body.data() != bytes.data())
            {
                unbound("look-for-weapon state whose body has no player owner");
            }
            auto& graph = body_messages[player->body.data()];
            if (graph == nullptr)
            {
                graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
            }
            return graph->look_for_weapon_context();
        });
    install_runtime_drone_dance_state_class(scene.types, [this](u32 body_address) -> RuntimeDroneDanceStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("drone dance state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->drone_dance_context();
    });
    install_runtime_spin_dizzy_state_class(scene.types, [this](u32 body_address) -> RuntimeSpinDizzyStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("type 0x86 state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->spin_dizzy_context();
    });
    install_runtime_stunned_state_class(scene.types, [this](u32 body_address) -> RuntimeStunnedStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("type 0x89 state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->stunned_context();
    });
    install_runtime_endure_state_class(scene.types, [this](u32 body_address) -> RuntimeEndureStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("type 0x84 state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->endure_context();
    });
    install_runtime_hit_stream_state_class(scene.types, [this](u32 body_address) -> RuntimeHitStreamStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("hit-stream state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->hit_stream_context();
    });
    install_runtime_shoot_state_class(scene.types, [this](u32 body_address) -> RuntimeShootStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("shoot state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->shoot_context();
    });
    install_runtime_flamethrower_state_class(scene.types, [this](u32 body_address) -> RuntimeFlamethrowerStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("flamethrower state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->flamethrower_context();
    });
    install_runtime_hover_state_class(scene.types, [this](u32 body_address) -> RuntimeHoverStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("hover state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->hover_context();
    });
    install_runtime_flip_state_class(scene.types, [this](u32 body_address) -> RuntimeFlipStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("flip state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->flip_context();
    });
    install_runtime_scoop_whack_state_class(scene.types, [this](u32 body_address) -> RuntimeScoopWhackStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("scoop and whack state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->scoop_whack_context();
    });
    install_runtime_slam_smash_state_class(scene.types, [this](u32 body_address) -> RuntimeSlamSmashStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("slam smash state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->slam_smash_context();
    });
    install_runtime_grab_throw_state_class(scene.types, [this](u32 body_address) -> RuntimeGrabThrowStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("grab and throw state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->grab_throw_context();
    });
    install_runtime_grabbed_hold_state_class(scene.types, [this](u32 body_address) -> RuntimeGrabbedHoldStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("grabbed state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->grabbed_hold_context();
    });
    install_runtime_bandaged_state_class(scene.types, [this](u32 body_address) -> RuntimeBandagedStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("bandaged state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->bandaged_context();
    });
    install_runtime_blood_suck_state_class(scene.types, [this](u32 body_address) -> RuntimeBloodSuckStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("blood suck state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->blood_suck_context();
    });
    install_runtime_shocked_state_class(scene.types, [this](u32 body_address) -> RuntimeShockedStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("shocked state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->shocked_context();
    });
    install_runtime_transform_state_class(scene.types, [this](u32 body_address) -> RuntimeTransformStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("transform state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->transform_context();
    });
    install_runtime_furnace_hit_state_class(scene.types, [this](u32 body_address) -> RuntimeFurnaceHitStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("furnace hit state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->furnace_hit_context();
    });
    install_runtime_explode_state_class(scene.types, [this](u32 body_address) -> RuntimeExplodeStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("explode state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->explode_context();
    });
    install_runtime_death_state_class(scene.types, [this](u32 body_address) -> RuntimeDeathStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("death state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->death_context();
    });
    install_runtime_swing_state_class(scene.types, [this](u32 body_address) -> RuntimeSwingStateContext&
    {
        const auto bytes = scene.resolve_address(body_address, 0);
        auto* player = players->find_child_owner(bytes.data());
        if (player == nullptr || player->body.data() != bytes.data())
        {
            unbound("swing state whose body has no player owner");
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        return graph->swing_context();
    });
    scene.install_base_class();

    // The kind handlers (DAT_0042A720) over the markers each scene loads (begin_scene).
    spawn = std::make_unique<RuntimePlayerSpawnContext>(RuntimePlayerSpawnContext{scene.factory, image.pools,
        scene_name.data(), std::span<const u8, 0x210>(data(0x00430DA0, 0x210).data(), 0x210),
        g_scene_flags->bytes[0], g_flags_ea9c, g_byte_30d14});
    player_start = std::make_unique<RuntimeMarkerPlayerStart>(RuntimeMarkerPlayerStart{*spawn, markers,
        address_zero, reinterpret_cast<const u32&>(g_player_start_enabled), g_script_30d0c, g_scene_flags->bytes[0],
        g_flags_ea9c,
        []() -> bool
        {
            return script_end_gate_set(); // 0x00123B00
        },
        scene.objects,
        [this](FactoryObject& object, std::span<const u32> message)
        {
            object_message(object, {const_cast<u32*>(message.data()), message.size()});
        }});
    sounds.static_voice_rows = scene_named_sound_records(); // DAT_0051254C, kind 7's named sounds.
    // DAT_0030CB18 holds eleven scare figure names and DAT_0030D098 five dropper names, 0x80 bytes each.
    marker_additional = std::make_unique<RuntimeMarkerAdditionalBindings>(RuntimeMarkerAdditionalBindings{
        scene_named_sound_records(), sounds.static_voice_count, image.memory(0x0030CB18, 11 * 0x80).data(),
        image.memory(0x0030D098, 5 * 0x80).data(), marker_nearest_retained, marker_dropper_retained});
    marker_bindings = std::make_unique<RuntimeSceneMarkerBindings>(RuntimeSceneMarkerBindings{*player_start,
        *marker_objects, actor_tags.data(), camera_start, marker_additional.get()});
    handlers = bind_runtime_scene_marker_handlers(*marker_bindings);
    // Kind 0x19 (0x000FFA30) reads DAT_00511B20 as it runs: the first +0x934 node's actor, a background, and its
    // +0x8F4 list.
    handlers.handlers[0x19] = [this](u8* record)
    {
        Actor* player = nullptr;
        entity_detail::ListHead* attachments = &global_attachments;
        if (auto* first = backgrounds->first_background(); first != nullptr)
        {
            auto& view = actors->view(*object_of(first->bytes.data()));
            player = &first->state.actor;
            attachments = &view.live_attachments;
        }
        else if (entity_detail::g_world.body_a.next != nullptr)
        {
            unbound("scene marker kind 0x19's first +0x934 node is not a background");
        }
        RuntimeMarkerPlayerShapeBindings shape{player, *attachments, global_attachments, origin,
            marker_shape_retained};
        attach_runtime_marker_player_shape(record, shape); // 0x000FFA30
    };
    callout_names = strings(0x00421C9C, 0x55);
    flag_names = strings(0x00338FB0, 0x41);
    players->bindings.scene.flag_names = std::span<const char* const>(flag_names.data(), flag_names.size());
    surface_names = strings(0x004340A0, 0x12);
    shock_pass = {image.string(0x003168A4), image.string(0x00316890), image.string(0x0031687C),
        image.string(0x00316860)};
    shock_fail = {image.string(0x00316854), image.string(0x00316840), image.string(0x0031682C),
        image.string(0x00316810)};
    std::memcpy(death_actions.data(), data(0x00217EB8, 6).data(), 6);
    // DAT_00433C9C holds 0x48 rows of 0xC, one per statsheet key.
    damage_tables = std::make_unique<RuntimeBodyDamageTables>(RuntimeBodyDamageTables{data(0x00433C9C, 0xC * 0x48),
        statsheet});
    players->bindings.scene.damage_tables = damage_tables.get();
    footstep_kinds = strings(0x00423620, 1);
    footsteps = std::make_unique<FootstepSoundNames>(FootstepSoundNames{data(0x00314FBC, 0x20), surface_names});
    load_background_part_tables(part_tables, resources.registry, resources.loaded_sizes, resources.error);
    load_runtime_sector_settings(sectors, resources.registry, resources.loaded_sizes, resources.error);
    score = std::make_unique<RuntimeScoreAwards>(RuntimeScoreAwards{score_total, score_running_total, g_flags_ea9c,
        {reinterpret_cast<const float*>(data(0x003130B4, 0x20).data()), 8}, score_texts, score_categories,
        score_points, score_refresh});
    {
        const auto aliases = data(0x00434034, 0x10);
        door_aliases = {DoorRoomAlias{image.string(read_u32(aliases, 0)), image.string(read_u32(aliases, 4))},
            DoorRoomAlias{image.string(read_u32(aliases, 8)), image.string(read_u32(aliases, 0xC))}};
    }
    door_variants = strings(0x00424970, 8);
    door_shapes = strings(0x00425118, 12);
    door_names = std::make_unique<RuntimeDoorNames>(RuntimeDoorNames{room.data(), door_aliases, door_variants,
        door_shapes, data(kDoorShapes, 13 * sizeof(DoorShapeRow))}); // Rows 0..12 (12: a room door 0x0005B540 adds); names follow.
    // Actor +0x204 (model reference and its instance) and +0x310 (secondary instance) of a door track actor.
    door_models = [this](Actor* actor) -> RuntimeDoorActorModels
    {
        if (actor == nullptr)
        {
            throw Error("door clip reads a track actor that 0x0004DEC0 did not create");
        }
        for (const auto& [bytes, native] : actors->actors)
        {
            if (native == actor)
            {
                auto& view = actors->view(*object_of(bytes));
                return RuntimeDoorActorModels{actor->model == nullptr ? nullptr : &actor->model->cpu,
                    actor->model == nullptr ? nullptr : actor->model->runtime, view.startup.models.secondary.get()};
            }
        }
        throw Error("door track actor is not a scene actor");
    };
    // Cutscene instances are engine-heap blocks (0x0004C850); door rows store their token addresses.
    door_instance_address = [this](const RuntimeCutsceneInstance* instance)
    {
        // FactoryObject owners move when the track vector grows; refresh their native pointer views.
        for (auto& object : cutscene_actors)
        {
            if (object.bytes != nullptr)
            {
                auto* actor = actors->actors.at(object.bytes.get());
                actor_object_addresses[actor] = scene.address_of(object.bytes.get());
            }
        }
        return instance == nullptr ? 0u : heap_address(instance, sizeof(RuntimeCutsceneInstance));
    };
    // The track vector grows as cutscenes create actors, so look the owner up when it is needed.
    actor_objects = [this](Actor* actor) -> FactoryObject*
    {
        for (auto& object : cutscene_actors)
        {
            if (object.bytes != nullptr && actors->actors.at(object.bytes.get()) == actor)
            {
                return &object;
            }
        }
        throw Error("cutscene track actor has no factory object");
    };
    scene.controls->context.clock = &clock;
    scene.controls->context.audio = audio_banks;
    // The actor sound queues stop the same event voices (DAT_004E0CE8) through 0x00039690.
    sounds.stop_voice = [this](i16 index)
    {
        stop_audio_event_voice(index, 0, voices);
    };
    cutscene_factory = std::make_unique<CutsceneActorFactoryContext>(CutsceneActorFactoryContext{scene.factory,
        image.pools, replacement_source.data(), replacement_target.data()});
    cutscene_initialization = std::make_unique<CutsceneActorInitializationContext>(
        CutsceneActorInitializationContext{*cutscene_factory, cutscene_actors, actors->cutscene_bindings,
            resources.registry, resources.error, random, nullptr, node_cache, reset_matrix});
    cutscenes = std::make_unique<CutsceneRuntimeContext>(CutsceneRuntimeContext{resources.registry, resources.error,
        resources.cutscene_views.views, {}, *cutscene_initialization, scene.factory, image.pools, actor_objects,
        actors->render, actors->animations, actor_object_addresses, actor_updates, *audio_banks, effects, scares,
        model_renderer, fog, camera_impulse, camera_event_asset, *footsteps,
        footstep_kinds, retail_cutscene_event_sizes, retail_cutscene_dialogue_resources[0].data(),
        listener_position, random, i32(g_language), movie_flag_38, movie_flag_3c, replacement_source.data(),
        replacement_target.data(), nullptr, nullptr, movie_slots, movie_mode, {}, {}, {}, {}, engine_heap_free,
        [this](Actor& actor, i32 view, i32 pass)
        {
            actor_draw(actor, view, pass);
        }});
    doors = std::make_unique<RuntimeDoorContext>(RuntimeDoorContext{door_pool, door_rows, *door_names,
        cutscenes.get(), &door_camera, door_instance_address, door_models, node_cache});
    doors->camera_registered = &door_camera_registered;
    doors->paused = &door_paused;
    doors->active_strategies = &active_strategies;
    doors->camera_hold = &door_camera_hold;
    doors->runtime_flags = &g_flags_ea9c;
    doors->camera_name = image.string(0x00323C40);
    std::memcpy(&door_added_scale, data(0x0042A840, 0xC).data(), 0xC);
    doors->added_position = &origin;
    doors->added_euler = &default_euler;
    doors->added_scale = &door_added_scale;
    doors->actor_message = [this](Actor& actor, std::span<u32> message)
    {
        object_message(object_of_actor(actor), message);
    };
    // 0x0013B1F0 on a door track actor (door state 7, 0x0005A9B0).
    doors->burst = [this](Actor& actor, float lifetime, const char* name)
    {
        FactoryObject& object = object_of_actor(actor);
        burst_runtime_actor_into_bits(actor, address_of(object.bytes.get()), lifetime, name, empty_name,
            [this](std::span<const u8> parameters)
            {
                scene.keep_object(create_factory_object(scene.factory, parameters)); // 0x00107520
            });
    };
    g_runtime_doors = doors.get();
    event_board_resources = std::make_unique<RuntimeScoreboardResources>(RuntimeScoreboardResources{
        resources.registry, resources.error, data(0x00314170, sizeof(HudCounterPreset))});
    {
        event_sequence = std::make_unique<ScriptEventSequenceContext>(ScriptEventSequenceContext{*doors,
            scoreboard_globals.score_board, *event_board_resources, audio.voices, prompt_owner,
            g_script_30d0c, data(kEventStepTypes, 0x13 * sizeof(EventStepType)), 1.2f, 2.f, 1.5f, // DAT_0032B24C, DAT_0032B110, DAT_0032B210.
            [this]()
            {
                // DAT_00511B40: the first case-list node (player actor +0x93C, links[1]); its +0x20 is the body.
                const auto* node = entity_detail::g_world.case_head.next;
                if (node == nullptr)
                {
                    return true;
                }
                for (auto* player : players->instances())
                {
                    if (static_cast<const void*>(&player->links[1]) == static_cast<const void*>(node))
                    {
                        player->refresh_body_views();
                        if (player->body_flags == nullptr)
                        {
                            unbound("DAT_00511B40 player without the body +0x15C bit block");
                        }
                        return (player->body_flags->bytes[3] & 8) != 0;
                    }
                }
                unbound("DAT_00511B40 node with no native player");
            },
            [this]() -> i32
            {
                auto* runtime = find_scene_cutscene_runtime(game);
                return runtime == nullptr ? -1 : runtime->index; // DAT_004E1C34
            },
            [this]()
            {
                std::vector<std::array<u32, 3>> listed;
                for (const auto* node = latches; node != nullptr; node = node->next)
                {
                    u32 owner = 0;
                    for (const auto& [address, latch] : active_nodes)
                    {
                        if (latch.get() == node)
                        {
                            owner = address;
                            break;
                        }
                    }
                    if (owner == 0)
                    {
                        unbound("DAT_00511AA4 node with no scene actor");
                    }
                    const auto bytes = scene.resolve_tail(owner);
                    if (bytes.size() < 0x90C)
                    {
                        unbound("DAT_00511AA4 actor " + std::to_string(owner) + " smaller than its +0x908 word");
                    }
                    listed.push_back({owner, read_u16(bytes, 6), read_u32(bytes, 0x908)});
                }
                return listed;
            },
            [this](u32 address)
            {
                // 0x000D0200 inlines 0x00112CC0: 0x00112D10 on +0x920, then message 0x80 and 0x0003ED70.
                activate_actor_bind_tree(*actors->actors.at(scene.resolve_address(address, 0).data()));
            },
            &random,
            [this]() -> const Actor*
            {
                const auto listed = case_actors(); // DAT_00511B40.
                return listed.empty() ? nullptr : &listed.front().actor;
            },
            [this]()
            {
                return static_cast<const u8*>(script_records); // DAT_0051108C.
            },
            &occupancy_initial,
            [this](const char* name, const std::function<void(std::span<u8>)>& patch) -> u32
            {
                auto clone = clone_executable_resource(image.pools, name); // 0x00107720
                if (clone.bytes == nullptr)
                {
                    return 0;
                }
                patch({clone.bytes.get(), clone.size});
                FactoryObject* made = scene.keep_object(create_factory_object(scene.factory,
                    {clone.bytes.get(), clone.size})); // 0x00107520
                clone.bytes.reset(); // 0x000663E0
                return made == nullptr || made->bytes == nullptr ? 0u : address_of(made->bytes.get());
            },
            [this](u32 address, std::span<u32> words)
            {
                auto* object = object_of(scene.resolve_address(address, 0).data());
                if (object == nullptr)
                {
                    unbound("event sequence message to an address that is not a scene object");
                }
                object_message(*object, words);
            },
            [this](u32 address, size_t size)
            {
                return scene.resolve_address(address, size);
            },
            [this](u32 address) -> std::span<u8>
            {
                NativePlayerInstance* player = players->find_player_at(address);
                if (player == nullptr || player->strategy.data() == nullptr)
                {
                    unbound("0x000CF7B0 reads +0x954 of an actor with no native strategy");
                }
                return player->strategy;
            },
            [this](const void* node) -> u32
            {
                // DAT_00511B48 nodes are player actor +0x944 (links[2]).
                for (auto* player : players->instances())
                {
                    if (&player->links[2] == node)
                    {
                        return player->actor_address;
                    }
                }
                unbound("DAT_00511B48 node without a native player");
            },
            [this](u32 address)
            {
                activate_actor_bind_tree(*actors->actors.at(scene.resolve_address(address, 0).data())); // 0x00112CC0
            }});    }
    g_event_sequence_context = event_sequence.get();
    ghouly_spawns = std::make_unique<GhoulySpawnContext>(GhoulySpawnContext{random, g_flags_ea9c,
        *reinterpret_cast<i32*>(&scene_word(0x0050E568u)), *reinterpret_cast<i32*>(&scene_word(0x0050E56Cu)),
        *reinterpret_cast<i32*>(&player_count), queue_limits.data(), image.pools});
    aid_spawns = std::make_unique<AidSpawnQueueContext>(AidSpawnQueueContext{*ghouly_spawns, resources.registry,
        resources.error, controller_actors, g_event_sequence_state,
        *reinterpret_cast<i32*>(&g_event_sequence.waiting)});
    // Relocated resources live from 0x40000000 (SceneResourceMemory); scene objects below.
    background_allocation = [this](u8* allocation, u32 size)
    {
        return heap_address(allocation, size);
    };
    background_address = [this](const void* pointer) -> u32
    {
        for (const auto& [bytes, views] : background_views)
        {
            if (auto* background = backgrounds->find(bytes); background != nullptr &&
                pointer == &background->lifetime.link)
            {
                return scene.address_of(bytes) + 0x934;
            }
        }
        try
        {
            return address_of(pointer);
        }
        catch (const Error&)
        {
            // Engine-heap records the constructor stores as words (attachments, anchors, event pools).
            return heap_address(pointer, 0x100);
        }
    };
    effect_release = std::make_unique<RuntimeModelSoundRelease>(RuntimeModelSoundRelease{sounds, resources.registry,
        deferred, fence, shaders, heap_free});
    backgrounds = std::make_unique<NativeBackgroundClassOwner>(NativeBackgroundClassBindings{*actors,
        entity_detail::g_world.body_a,
        [this](NativeBackgroundInstance& background, NativeActorFactoryInstance& view,
            RuntimeBackgroundAnimation*& first)
        {
            return background_services(background, view, first);
        },
        disable_framebuffer_clear, *effect_release, heap_free,
        [this](NativeBackgroundInstance& background, std::span<u32> message)
        {
            background_message(background, message);
        },
        [this](NativeBackgroundInstance& background)
        {
            background_update(background);
        }});
    backgrounds->install(scene.types);
    // Boot display, input and text globals, in 0x00033030 order. 0x000FC310 reads the display mode
    // (DAT_005096E8, the host's 640x480) and, for a 640-wide mode, sets DAT_00509704 from DAT_0042A54C == 0;
    // its video-standard, video-flag and vertical-blank calls have no host counterpart. It ends in 0x000FC190.
    if (hud_size[0] == 0x280)
    {
        entity_detail::g_aspect_exact = read_u32(data(0x0042A54C, 4), 0) == 0 ? 1 : 0;
    }
    layout_runtime_hud(entity_detail::g_aspect_exact, hud_size[0], hud_size[1], hud);
    initialize_display_views(hud_size[0], hud_size[1], render_width, render_height);
    input_state.selected_mask = port_masks[1];
    // The boot master pad's native address resolves in this scene's input map.
    input_addresses[game.native_input->master_address] = game.native_input->master_pad;
    input_views[game.native_input->master_address] = RuntimeDialogueInputDevice{game.native_input->master_pad,
        {nullptr, nullptr, nullptr, nullptr}};
    input_views[game.native_input->master_address] = RuntimeDialogueInputDevice{game.native_input->master_pad,
        {nullptr, nullptr, nullptr, nullptr}};
    {
        const auto defaults = data(0x00424900, 0x70);
        auto& draw = dialogue_draw_state;
        draw.origin_x = read_f32(defaults, 0);
        draw.origin_y = read_f32(defaults, 4);
        draw.paragraph_scale = read_f32(defaults, 0x10);
        draw.paragraph_offset = read_f32(defaults, 0x14);
        draw.wave_speed = read_f32(defaults, 0x18);
        draw.wave_height = read_f32(defaults, 0x1C);
        draw.scale_x = read_f32(defaults, 0x3C);
        draw.scale_y = read_f32(defaults, 0x40);
        draw.line_step = read_f32(defaults, 0x48);
        draw.space = read_f32(defaults, 0x4C);
        draw.width = read_f32(defaults, 0x50);
        draw.lines = std::bit_cast<i32>(read_u32(defaults, 0x54));
        draw.fade = read_f32(defaults, 0x6C);
        trophy_flags = read_u32(defaults, 0x58);
        tag_state.alignment = std::bit_cast<i32>(read_u32(defaults, 0x5C));
        tag_state.tab = std::bit_cast<i32>(read_u32(defaults, 0x64));
        tag_state.no_paragraph = std::bit_cast<i32>(read_u32(defaults, 0x68));
        tag_state.scale_x = read_f32(defaults, 0x34);
        tag_state.scale_y = read_f32(defaults, 0x38);
        page_settings = {std::bit_cast<i32>(read_u32(defaults, 0x44)), std::bit_cast<i32>(read_u32(defaults, 0x28)),
            std::bit_cast<i32>(read_u32(defaults, 0x60))};
        std::memcpy(default_world.data(), data(0x0042A7C0, 0x30).data(), 0x30);
    }
    {
        const auto tables = data(0x00425160, 8);
        text_system = std::make_unique<RuntimeDialogueTextSystem>(RuntimeDialogueTextSystem{fallback_texture,
            paragraph_disabled, text_fonts, dialogue_draw_state, text_registry, text_registry_ready, text_tables,
            {data(read_u32(tables, 0), 0xC).data(), data(read_u32(tables, 4), 0xC).data()}});
        initialize_runtime_dialogue_text(g_language, *text_system, resources.registry, resources.error);
        for (size_t index = 0; index < text_fonts.size(); ++index)
        {
            resources.publish_font(index, text_fonts[index]);
            // 0x0005F250 reads these same words (DAT_005043BC + font * 8) as each font's kerning pairs; its
            // search can touch row `count`, the terminating zero row.
            const auto& table = text_tables[index];
            resources.kerning[index] = RuntimeTextKerning{table.count,
                {table.rows, (size_t(table.count) + 1) * 0xC}};
        }
    }
    // DAT_003150D0: the widget icon names (stride 0x80) 0x00142C20 indexes, and the DAT_004E1C58 texture.
    for (u32 index = 0; index < 10; ++index)
    {
        widget_icon_names.push_back(reinterpret_cast<const char*>(data(0x003150D0 + index * 0x80, 0x80).data()));
    }
    widget_context.icon_names = widget_icon_names;
    widget_context.fallback_texture = fallback_texture.data();
    trophies = std::make_unique<DialogueTrophyContext>(DialogueTrophyContext{g_challenge_selection.thresholds, dialogue_owned,
        trophy_flags, trophy_names.data(), g_retail_dialogue_token_names, resources.registry, resources.error});
    tags = std::make_unique<DialogueTagContext>(DialogueTagContext{tag_state, *trophies, sound_tags,
        g_retail_dialogue_tokens, g_retail_dialogue_sound_swaps,
        {reinterpret_cast<const i32*>(g_game_progress.words_18f8.data()), g_game_progress.words_18f8.size() / 4},
        g_challenge_selection.selected, g_challenge_run.score, story_byte, g_flags_ea9c});
    const float initial_scale = read_f32(data(0x00424910, 4), 0);
    const float drawing_scale = read_f32(data(0x0042493C, 4), 0);
    measurement = std::make_unique<DialogueMeasurementContext>(DialogueMeasurementContext{*tags, resources.metrics,
        resources.text, {g_runtime_profile.challenges.size, g_runtime_profile.challenges.bytes.data()},
        g_runtime_profile.times[1], 0, g_runtime_profile.times[2], g_runtime_profile.times[3], 0,
        g_runtime_profile.times[0], 0.f,
        initial_scale, drawing_scale, paragraph_disabled,
        wide_scratch.data(), narrow_scratch.data()});
    widget_allocation = std::make_unique<DialogueWidgetAllocationContext>(DialogueWidgetAllocationContext{
        widget_context, text_registry});
    widget_bats = std::make_unique<DialogueWidgetBatFactoryContext>(DialogueWidgetBatFactoryContext{scene.factory,
        image.pools, story_byte, dialogue_bats, actors->actors});
    bat_views = [this](Actor* bat) -> DialogueWidgetBatView
    {
        for (const auto& [bytes, native] : actors->actors)
        {
            if (native == bat)
            {
                auto& view = actors->view(*object_of(bytes));
                pause = widget_update->freeze;
                frame_delta = widget_update->dt;
                time_scale = widget_update->scene_scale;
                frame->freeze = pause;
                frame->dt = frame_delta;
                frame->scene_scale = time_scale;
                frame->block_decay = pause_references[1];
                frame->decay_enabled = decay_enabled[decay_index];
                return DialogueWidgetBatView{view.bat, view.frame, *frame};
            }
        }
        throw Error("dialogue widget bat is not a scene actor");
    };
    widget_update = std::make_unique<DialogueWidgetUpdateContext>(DialogueWidgetUpdateContext{widget_context,
        input_state, voices, bat_views, widget_callbacks, pause, frame_delta, time_scale});
    widget_callbacks.emplace(0x00058B80, [this]()
    {
        accept_runtime_dialogue_button(dialogue_state);
    });
    const u32 first_category = read_u32(data(0x00421A1C, 4), 0);
    const u32 second_category = read_u32(data(0x00421A24, 4), 0);
    dialogue = std::make_unique<RuntimeDialogueActivationContext>(RuntimeDialogueActivationContext{dialogue_state,
        g_flags_ea9c, active_strategies, frontend_pause, random, story_byte, hud_size[0], hud.inner_bottom,
        hud.text_width, first_category, second_category,
        image.string(0x00323C2C), reinterpret_cast<const char*>(data(0x002FEAB8, 0x80).data()),
        data(0x002FEB50, 4).data(), replacement_source.data(), replacement_target.data(), *widget_allocation,
        *widget_bats, dialogue_page, page_settings, *measurement, input_activation, input_release, listener, voices,
        *cutscenes, *widget_update, input_addresses, input_views, 0, pause, frame_delta, time_scale});
    script_dialogue = std::make_unique<RuntimeScriptDialogueContext>(RuntimeScriptDialogueContext{*dialogue, query,
        g_flag_2474, g_flags_ea9c, random, narrow_scratch.data()});
    // Cutscene caption events (types 0x14 and the 0x15 page) enqueue through the same dialogue (0x000582A0).
    cutscenes->dialogue = dialogue.get();
    // Cutscene movie events (0x00106D50) start in the boot movie lists. A frame copy (0x00107360) locks the
    // hooked texture's data in this scene's resource memory.
    auto& movie_startup = game.native_movies->session->startup();
    cutscenes->movies = &movie_startup;
    movie_startup.lock_texture = [this](void* texture) -> RuntimeMovieLockedSurface
    {
        return lock_movie_texture(u32(reinterpret_cast<std::uintptr_t>(texture)));
    };
    model_renderer.frames = render_frame_storage;
    RuntimeNodeTypeRegistry node_registry{resources.node_types.bytes, node_registry_count};
    render_image.resolve = [this](u32 address) -> u8*
    {
        // Node descriptors live in the relocated arena; their vtables remain in
        // the executable. These are genuine file-backed words, not substituted callbacks.
        if (address < 0x40000000u)
        {
            return const_cast<u8*>(image.memory(address, 1).data());
        }
        u8* pointer = resources.memory.pointer(address);
        if (pointer == nullptr)
        {
            throw Error("model renderer reads a relocated address of zero");
        }
        return pointer;
    };
    initialize_runtime_model_renderer(model_renderer, node_registry, *shader_fragments);
    boot_vertex_shaders = std::make_unique<RuntimeBootVertexShaders>(RuntimeBootVertexShaders{render_image,
        [](std::string_view source, std::vector<u32>& program) -> i32
        {
            program = assemble_xg_vertex_shader(source);
            return 0;
        },
        [this](const i32* declaration, const u32* program, u32& handle, u32 usage) -> i32
        {
            d3d_device.create_vertex_shader(declaration, program, handle, usage);
            return 0;
        },
        [this](u32 handle)
        {
            d3d_device.dialogue().vertex_shader(handle);
        },
        [this](u32 reg, const float* values, u32 count)
        {
            d3d_device.dialogue().constants(reg, values, count);
        }});
    create_runtime_boot_vertex_shaders(*boot_vertex_shaders);
    boot_pixel_shaders = std::make_unique<RuntimeBootPixelShaders>(RuntimeBootPixelShaders{render_image,
        [](std::string_view source, XboxPixelShaderDefinition& definition) -> i32
        {
            definition = assemble_xg_pixel_shader(source);
            return 0;
        },
        [this](const XboxPixelShaderDefinition& definition, u32& handle) -> i32
        {
            const auto allocation = resources.memory.copy(
                {reinterpret_cast<const u8*>(definition.data()), sizeof(definition)}, 4);
            handle = resources.d3d.next_handle++;
            resources.d3d.pixel_shaders.emplace(handle, allocation.address);
            boot_pixel_allocations.emplace_back(handle, allocation.address);
            return 0;
        },
        [this](u32 factory) -> u32
        {
            if (factory == 0x00063F40)
            {
                return create_runtime_boot_texture_pixel_shader(*boot_pixel_shaders);
            }
            unbound("boot pixel shader factory " + std::to_string(factory));
        }});
    create_runtime_boot_pixel_shaders(*boot_pixel_shaders);
    // Boot 0x00033030 -> 0x000F8740: the model library's matrix scratch, deferred releases and hooks. The hooks reach
    // the draw contexts this constructor makes later; nothing draws before it returns.
    RuntimeModelLibraryBoot model_library{model_matrix_allocation, model_matrix_scratch, deferred,
        model_library_hooks, model_library_allocator,
        [](i32 size, u32 alignment)
        {
            return static_cast<u8*>(engine_heap_allocate_aligned(size, alignment));
        },
        [this](u32 shader)
        {
            post_runtime_model_shader(shader, *model_shader_combiner);
        },
        [this](u32)
        {
            apply_runtime_model_material_colors(*model_shader_combiner);
        },
        [this](u32 texture, u32 stage)
        {
            // 0x000F9BC0 passes the texture to 0x00053550 in ESI.
            u8* header = resources.memory.pointer(texture);
            if (header == nullptr)
            {
                throw Error("model material binds an unmapped texture");
            }
            bind_dialogue_texture(header, stage, *dialogue_draw);
        },
        [](u32 size)
        {
            return engine_heap_allocate(i32(size));
        },
        [](void* block)
        {
            engine_heap_free(block); // HeapFree on the block's heap, as 0x000D8AC0 does.
        }};
    boot_runtime_model_library(model_library);
    for (size_t index = 0; index < render_frame_storage.size(); ++index)
    {
        render_frames.emplace(&render_frame_storage[index], DialogueRenderFrame{{}, frame_cameras[index]});
    }
    const u32 quad_one = read_u32(data(0x0042515C, 4), 0);
    const std::array<float, 2> quad_top{read_f32(data(0x0042514C, 8), 0), read_f32(data(0x0042514C, 8), 4)};
    const std::array<float, 2> quad_bottom{read_f32(data(0x00425154, 8), 0), read_f32(data(0x00425154, 8), 4)};
    const float ordinary_line_scale = read_f32(data(0x00424908, 4), 0);
    const float tab_line_scale = read_f32(data(0x0042490C, 4), 0);
    dialogue_draw = std::make_unique<RuntimeDialogueDrawContext>(RuntimeDialogueDrawContext{*dialogue,
        dialogue_draw_state, resources.metrics, d3d_device.dialogue(), model_renderer, render_frames, deferred_states,
        light_override, fog.disabled, sampling_scratch, sampling_active, dialogue_projection, dialogue_view,
        dialogue_world, sprite_world,
        std::span<const u32, 4>(reinterpret_cast<const u32*>(&entity_detail::g_display_records[0].fitted_x), 4),
        render_height, render_width, hud_size[0], hud_size[1], 0xFFFFFFFFu, // DAT_00314F48.
        read_u32(data(kDialogueVertexShader.handle, 4), 0), read_u32(data(kTexturedDiffusePixelShader, 4), 0),
        shader_constants_at(kDialogueVertexShader.constants),
        {0, 0, 0}, quad_one, quad_top, quad_bottom, fallback_texture.data(), 0.f, 0,
        []() -> std::int64_t
        {
            return host_performance_counter();
        }, ordinary_line_scale, tab_line_scale});
    display_setup = std::make_unique<RuntimeDisplaySetupContext>(RuntimeDisplaySetupContext{*dialogue_draw,
        clip_bounds, device_projection, device_view, device_world, projection_transpose, default_world, frontend_pause,
        display_address});
    sprites = std::make_unique<RuntimeDialogueSpriteQueue>(RuntimeDialogueSpriteQueue{sprite_records, sprite_count});
    scoreboard_draw = std::make_unique<RuntimeScoreboardDraw>(RuntimeScoreboardDraw{*sprites, g_scene_hold_time,
        brew_accepted});
    scoreboard_draw->immediate = dialogue_draw.get();
    scoreboard_hud->draw = scoreboard_draw.get();
    scoreboard_hud->counter_presets = {data(0x00314170, sizeof(HudCounterPreset)), data(0x00314184, sizeof(HudCounterPreset))};
    scoreboard_hud->damage_preset = data(0x00312F8C, sizeof(HudCounterPreset));
    dialogue_outer = std::make_unique<RuntimeDialogueOuterDrawContext>(RuntimeDialogueOuterDrawContext{*dialogue_draw,
        render_targets, d3d_device.outer(), *sprites, &resources.special_textures, *display_setup, text_draw_count,
        entity_detail::g_aspect_preset, entity_detail::g_video_widescreen, g_flags_ea9c, display_address,
        read_u32(data(kScreenLineVertexShader.handle, 4), 0), read_u32(data(kPassColourPixelShader, 4), 0),
        shader_constants_at(kScreenLineVertexShader.constants), line_retained});
    dialogue_draw->panel_color = 0xE0202020u; // DAT_002FF2E0.
    dialogue_draw->panel_alpha = 224.f; // DAT_0032B49C.
    dialogue_draw->outer = dialogue_outer.get();
    dialogue_outer->panel_line_width = read_u32(data(0x00425168, 4), 0);
    brew->allocation = widget_allocation.get();
    brew->update = widget_update.get();
    brew->widget_draw = dialogue_outer.get();
    bind_scene_dialogue(game, *script_dialogue, *dialogue_outer);
    // The scene cutscene stack (DAT_004E1B40..DAT_004E1C54) that script node 0x13 starts (0x0004EDA0) and the
    // scene tick advances (0x0004EBD0), over this scene's cutscene context.
    install_scene_cutscene_runtime(game, SceneCutsceneBindings{*cutscenes, g_flags_ea9c, g_pause_count, g_transition_pause_count,
        g_scene_active_count, g_sim_freeze, g_sim_dt, g_sim_scale, prompt_lock});
    // The attract control (factory type 0x9B) over this scene's resources, text, input and cutscene stack.
    attract_services = std::make_unique<AttractControlServices>(AttractControlServices{resources.registry,
        resources.error, resources.text, text_registry, resources.metrics, widget_context.viewport_width,
        next_widget_id, random, hud.center_x, hud.bottom, text_fonts[0], g_pause_count, input_activation, input_release,
        input_addresses, input_views, input_state, game.clock.frame, scene_cutscene_runtime(game), node_cache,
        [this](Actor& actor) -> RuntimeModelInstance*
        {
            return actors->view(object_of_actor(actor)).startup.models.secondary.get();
        },
        [this](const char* name)
        {
            script_request(game, name); // 0x0011EB30 with mode 1.
        },
        [this]()
        {
            play_start_button_laugh(random, image, voices); // 0x0013BAF0
        },
        i32(g_language), data(0x004275DC, 5 * 4), g_sim_freeze, g_sim_dt, g_sim_scale});
    scene.controls->context.attract = attract_services.get();
    // 0x00123E70's tables and HUD: the powerup rows, the award caption, the bonus board and the challenge bits.
    powerup_names = strings(0x0042ABF8, 20);
    powerup_table = std::make_unique<RuntimePowerupTable>(RuntimePowerupTable{powerup_rows, powerup_board,
        resources.registry});
    powerup_caption = std::make_unique<RuntimePowerupCaption>(RuntimePowerupCaption{resources.text,
        wide_scratch.data(), powerup_names, scoreboard_globals.award_pool, text_registry, hud_size[0],
        hud.inner_bottom, resources.metrics, widget_context.viewport_width, text_registry_ready, random});
    bonus_counters = std::make_unique<RuntimeBonusCounters>(RuntimeBonusCounters{scoreboard_globals.bonus_board,
        scoreboard_globals.bonus_ids, scoreboard_globals.bonus_active, scoreboard_globals.bonus_counts});
    challenge_saves = std::make_unique<SaveGameContext>(SaveGameContext{game.native_movies->session->files(),
        g_runtime_profile, scene_order_held});
    challenge_award = std::make_unique<RuntimeChallengeAward>(RuntimeChallengeAward{g_runtime_profile,
        intro_notices, *bonus_counters, *scoreboard_resources, *challenge_saves});
    powerup_awards = std::make_unique<RuntimePowerupAwardContext>(RuntimePowerupAwardContext{*powerup_table,
        *powerup_caption, *scoreboard_resources, *challenge_award, audio.streams, data(0x003121D0, 6 * 0x80)});
    placement_spawn = std::make_unique<RuntimeActorPlacementSpawn>(RuntimeActorPlacementSpawn{scene.factory,
        image.pools, script_records,
        [this](FactoryObject made) -> u32
        {
            FactoryObject* kept = scene.keep_object(std::move(made));
            return kept == nullptr || kept->bytes == nullptr ? 0u : address_of(kept->bytes.get());
        },
        [this](u32 address, std::span<const u32> words)
        {
            FactoryObject* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("0x001349E0 messages an address that is not a scene object");
            }
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            object_message(*object, buffer);
        },
        [this](u32 address) -> std::span<u8>
        {
            FactoryObject* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("0x001349E0 writes an address that is not a scene object");
            }
            return {object->bytes.get(), object->size};
        }});
    minicooper_spawn = std::make_unique<RuntimeMiniCooperSpawn>(RuntimeMiniCooperSpawn{*placement_spawn,
        [this](u32 address) -> const Actor&
        {
            return *actors->actors.at(scene.resolve_address(address, 0).data());
        },
        [this](u32 address) -> EntityBitBlock&
        {
            FactoryObject* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr || object->type != 5)
            {
                unbound("powerup kind 0xC's Mini Cooper is not a player actor");
            }
            auto& player = players->player(*object);
            player.refresh_views();
            if (player.body_bits == nullptr)
            {
                unbound("powerup kind 0xC's Mini Cooper has no body +0x15C bit block");
            }
            return *player.body_bits;
        },
        scared_count, scared_actors, scared_times});
    powerup_awards->minicooper = minicooper_spawn.get();
    powerup_awards->complete_event = [this]()
    {
        return complete_script_event_sequence(game); // 0x000D0870
    };
    powerup_awards->marked_actors = [this]()
    {
        std::vector<Actor*> actors;
        for (const auto& mark : marked_actors())
        {
            actors.push_back(mark.actor);
        }
        return actors;
    };
    // DAT_00511B48 nodes are player actor +0x944 (links[2]).
    const auto active_actor = [this](const void* node) -> u32
    {
        if (node == nullptr)
        {
            return 0;
        }
        for (auto* player : players->instances())
        {
            if (static_cast<const void*>(&player->links[2]) == node)
            {
                return player->actor_address;
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    powerup_actors = std::make_unique<RuntimePowerupActors>(RuntimePowerupActors{
        [active_actor]()
        {
            return active_actor(entity_detail::g_world.alt.next);
        },
        [this, active_actor](u32 address)
        {
            return active_actor(players->player_at(address).links[2].next);
        },
        [this](u32 address)
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            return RuntimePowerupActor{player.actor, player.actor_bytes, player.strategy, player.mind, player.ring};
        },
        query,
        [this](u32 address) -> RuntimeMindTarget&
        {
            // A mind can still name a record released this frame (retail reads its freed block); the retired
            // copy keeps those bytes.
            return *mind_target_at(address);
        },
        [this](u32 address, std::span<const u32> message)
        {
            std::array<u32, 0x42> words{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
            object_message(*object_of(players->player_at(address).actor_bytes.data()), words);
        },
        pause_references, data(0x0030F144, 4), decay_index, address_zero});
    powerup_awards->active_actors = powerup_actors.get();
    // The trophy-heads control (factory type 0x92) over the cutscene stack, the scene's players and its objects.
    for (u32 row = 0; row < 22; ++row)
    {
        const auto bytes = data(0x00301920 + row * 0x18, 0x10);
        const auto text = [this](u32 address) -> const char*
        {
            return address == 0 ? nullptr : image.string(address);
        };
        trophy_head_rows.push_back(TrophyHeadRow{text(read_u32(bytes, 0)), text(read_u32(bytes, 4)),
            text(read_u32(bytes, 8)), read_f32(bytes, 0xC)});
    }
    trophy_heads_services = std::make_unique<TrophyHeadsControlServices>(TrophyHeadsControlServices{
        resources.registry, resources.error, scene_cutscene_runtime(game), voices, random, g_pause_count, g_transition_pause_count,
        g_flags_ea9c, g_scene_flags->bytes[0], g_runtime_profile.progress.bytes[4], g_runtime_profile.challenges.bytes,
        g_script_30d0c, pulse, trophy_head_rows,
        g_scene_gameplay_time, g_scene_gameplay_mark, g_scene_gameplay_tick, prompt_owner, default_euler, g_sim_freeze, g_sim_dt, g_sim_scale,
        [this](Actor& actor) -> GhoulySpawnPool*&
        {
            return reinterpret_cast<GhoulySpawnPool*&>(actors->view(object_of_actor(actor)).allocation_8fc);
        },
        [this](Actor& actor) -> u32
        {
            return address_of(object_of_actor(actor).bytes.get());
        },
        [this](std::span<const u8> message)
        {
            script_queue_event(game, message);
        },
        [this]() -> u32
        {
            FactoryObject* object = scene.keep_object(create_runtime_player_actor(nullptr, origin, origin, *spawn));
            return object == nullptr ? 0 : address_of(object->bytes.get());
        },
        [this](u32 address) -> std::span<u8>
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            return player.body;
        },
        [this](u32 address)
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (scene.address_of(bytes) == address && actor != nullptr)
                {
                    activate_actor_bind_tree(*actor); // 0x00112CC0, ESI the actor
                    return;
                }
            }
            throw Error("trophy-heads control retires an address that is not a scene actor");
        },
        [this](std::span<const u8> parameters)
        {
            scene.keep_object(create_factory_object(scene.factory, parameters));
        },
        *powerup_awards});
    scene.controls->context.trophy_heads = trophy_heads_services.get();
    // The gaming room's haunted chairs (factory type 0x93).
    haunted_chairs_services = std::make_unique<HauntedChairsControlServices>(HauntedChairsControlServices{
        scene_cutscene_runtime(game),
        std::span<const u32, 9>(reinterpret_cast<const u32*>(data(0x00301B30, 0x24).data()), 9), script_records,
        [this](const Actor& actor) -> u32
        {
            return address_of(object_of_actor(const_cast<Actor&>(actor)).bytes.get());
        },
        [this](Actor& actor) -> std::span<u8>
        {
            auto& object = object_of_actor(actor);
            return {object.bytes.get(), object.size};
        },
        [this](Actor& actor) -> ActorRenderRegistration&
        {
            return actors->view(object_of_actor(actor)).render;
        }});
    scene.controls->context.haunted_chairs = haunted_chairs_services.get();
    // The relics store's Super Scary mummy (factory type 0x94).
    const auto scene_actor_at = [this](u32 address) -> Actor*
    {
        auto* object = object_of(scene.resolve_address(address, 0).data());
        if (object == nullptr || !actors->actors.contains(object->bytes.get()))
        {
            unbound("relics store control: an address that is not a scene actor");
        }
        return actors->actors.at(object->bytes.get());
    };
    const auto latch_owner = [this](const entity_detail::LatchNode& node) -> u32
    {
        for (const auto& [address, latch] : active_nodes)
        {
            if (latch.get() == &node)
            {
                return address;
            }
        }
        unbound("DAT_00511AA4 node with no scene actor");
    };
    const auto latch_bytes = [this](u32 address)
    {
        return scene.resolve_address(address, 0x200);
    };
    relics_store_services = std::make_unique<RelicsStoreControlServices>(RelicsStoreControlServices{
        scene_cutscene_runtime(game), *candle_services, resources.registry, resources.error, sounds, random,
        g_pause_count, g_transition_pause_count, g_flags_ea9c, pal, pal_override, g_scene_gameplay_time, g_scene_gameplay_mark, g_scene_gameplay_tick, g_sim_freeze, g_sim_dt,
        g_sim_scale, display_view, data(0x00427600, 0x18),
        [this](Actor& actor) -> const float&
        {
            return actors->view(object_of_actor(actor)).frame.elapsed;
        },
        [latch_owner, latch_bytes](u16 key)
        {
            return find_runtime_actor_by_class(key, latch_owner, latch_bytes); // 0x00129EA0
        },
        [latch_owner, latch_bytes](u16 key)
        {
            return find_runtime_actor_by_id(key, latch_owner, latch_bytes); // 0x00129ED0
        },
        scene_actor_at,
        [this](u32 address) -> std::span<const u8>
        {
            return scene.resolve_address(address, 0x200);
        },
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        [this](u32 address, std::span<u32> message)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("relics store control message to an address that is not a scene object");
            }
            object_message(*object, message);
        },
        [this](const char* name, const Vec3& position) -> u32
        {
            const Vec3 euler{};
            FactoryObject* object = scene.keep_object(create_runtime_player_actor(name, position, euler, *spawn));
            return object == nullptr ? 0 : address_of(object->bytes.get());
        },
        [this](std::span<const u8> parameters)
        {
            scene.keep_object(create_factory_object(scene.factory, parameters));
        },
        [this](std::span<const u8> message)
        {
            script_queue_event(game, message);
        }});
    scene.controls->context.relics_store = relics_store_services.get();
    // A relic candle's flame (factory type 0x9C), which the walk-in script action 0x001274B0 makes.
    candle_control_services = std::make_unique<CandleControlServices>(CandleControlServices{*candle_services,
        resources.registry, resources.error, sounds, g_sim_freeze, g_sim_dt, g_sim_scale,
        [this](Actor& actor) -> const float&
        {
            return actors->view(object_of_actor(actor)).frame.elapsed;
        },
        scene_actor_at});
    scene.controls->context.candle = candle_control_services.get();
    // Cutscene event 0x13 calls the walk cutscene's +0x150 (0x00081630) with +0x154, the control's address.
    weapon_services->cutscenes().event_callbacks[0x00081630] = [this](RuntimeCutsceneInstance&, u32 owner, u32)
    {
        auto* object = object_of(scene.resolve_address(owner, 0).data());
        if (object == nullptr)
        {
            unbound("relics store pulse for an object outside the scene");
        }
        auto& control = scene.controls->instance(*object);
        pulse_runtime_relics_store_control(scene_control_view<SceneControlBase>(control.state),
            *control.relics_store, *relics_store_services); // 0x00081630
    };
    // The scene's effect emitters (factory type 0x97, entitySceneControlFx).
    fx_services = std::make_unique<SceneFxControlServices>(SceneFxControlServices{clock, random, effects,
        [this](ActorEffectNode& effect, float step)
        {
            tick_owned_effect(effect, step); // 0x000EF2D0
        }});
    scene.controls->context.fx = fx_services.get();
    // Chapter 2 scene 4's cutscene and background morphs (factory type 0x95).
    chapter2_scene4_services = std::make_unique<Chapter2Scene4ControlServices>(Chapter2Scene4ControlServices{
        scene_cutscene_runtime(game), clock, random, g_script_30d0c, g_scene_flags->bytes[0], g_flags_ea9c,
        voices.reverb_index, g_gate_251c, background_morphs});
    scene.controls->context.chapter2_scene4 = chapter2_scene4_services.get();
    // Chapter 5's countdown (factory type 0x9D, script type 0x89).
    chapter5_countdown_services = std::make_unique<Chapter5CountdownServices>(Chapter5CountdownServices{
        g_scene_flags->bytes[0], g_chapter5_mode, g_script_6c60, g_chapter5_fail_wait, g_chapter5_message,
        g_chapter5_gold, story_bits, g_flags_ea9c, g_script_e794, clock, *scoreboard_hud, *scoreboard_resources,
        data(0x00314170, sizeof(HudCounterPreset)), *dialogue, audio.streams,
        [](u8 mask, i32 release)
        {
            g_actor_pause(mask, release); // 0x00117400
        },
        []()
        {
            return scene_end_flag_set(); // 0x00049D10
        }});
    g_chapter5_countdown = chapter5_countdown_services.get();
    // The Super Scary challenge (factory type 0x98) a trophy head's attack creates.
    const u32 panel_color = read_u32(data(0x0042795C, 4), 0);
    super_scary_services = std::make_unique<SuperScaryControlServices>(SuperScaryControlServices{
        resources.registry, resources.error, scene_cutscene_runtime(game), voices, audio.streams, random, g_pause_count,
        g_flags_ea9c, g_scene_flags->bytes[0], g_runtime_profile.progress.bytes, frontend_state, prompt_owner,
        powerup_rows[0x12].active, // DAT_005122F0, the ShockerBlocker powerup row.
        powerup_rows[0xA].active, data(0x00302198, 4 * 0x50), g_flag_27958,
        panel_color, 0xFFFFFFFFu, 0xFF0000FFu, // DAT_00314F48, DAT_00314F30.
        hud_size, display_view, scene_name.data(), clock,
        [](u8 mask, i32 release)
        {
            g_actor_pause(mask, release); // 0x00117400
        },
        input_activation, input_release, input_addresses, input_views, input_state, game.clock.frame, *dialogue,
        *sprites, *dialogue_outer, *health_display, health_intensity,
        [this]() -> u32
        {
            // DAT_00511B40: the first case-list node, a player's actor +0x93C (links[1]).
            for (auto* player : players->instances())
            {
                if (static_cast<const void*>(&player->links[1]) == entity_detail::g_world.case_head.next)
                {
                    return player->actor_address;
                }
            }
            throw Error("Super Scary challenge needs DAT_00511B40's player; the case list is empty");
        },
        [this](u32 address) -> SuperScaryActorView
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr || !actors->actors.contains(object->bytes.get()))
            {
                unbound("Super Scary actor that is not a scene actor");
            }
            auto& view = actors->view(*object);
            return SuperScaryActorView{view.bat.actor, view.animation, view.startup.secondary_mode,
                view.startup.models.secondary != nullptr};
        },
        [this](u32 address) -> std::span<u8>
        {
            auto& player = case_player(address);
            player.refresh_views();
            return player.body;
        },
        [this](u32 address) -> const Actor*
        {
            auto& player = case_player(address);
            player.refresh_views();
            return player.attached == nullptr ? nullptr : player.attached->actor; // Body +0x48
        },
        [this](u32 address)
        {
            auto& player = case_player(address);
            player.refresh_views();
            release_runtime_player_attachment(0, 0, player.attachment); // 0x0001B810(0, 0)
        },
        [this](u32 address, std::span<u32> message)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("Super Scary message to an address that is not a scene object");
            }
            std::array<u32, 0x42> words{};
            std::copy(message.begin(), message.end(), words.begin());
            object_message(*object, words);
        },
        [this](u32 address)
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (scene.address_of(bytes) == address && actor != nullptr)
                {
                    activate_actor_bind_tree(*actor); // 0x00112CC0, ESI the actor
                    return;
                }
            }
            throw Error("Super Scary challenge retires an address that is not a scene actor");
        },
        [this](u32 address)
        {
            unregister_actor_render_bins(super_scary_render(address)); // 0x0003CBB0, EDI the actor
        },
        [this](u32 address)
        {
            register_actor_render_bins(super_scary_render(address)); // 0x0003CAE0, EDI the actor
        },
        [this](const std::array<float, 2>& position, float scale, float depth) -> u32
        {
            FactoryObject* object = scene.keep_object(create_dialogue_button_bat(scene.factory, image.pools,
                story_byte, position, scale, depth)); // 0x0013B580
            if (object == nullptr)
            {
                return 0;
            }
            bind_button_bat(*object);
            return address_of(object->bytes.get());
        },
        [this](u32 address) -> SuperScaryBatView
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr || object->type != 0x15)
            {
                unbound("Super Scary bat that is not a button bat");
            }
            auto& view = actors->view(*object);
            return SuperScaryBatView{address, view.bat, view.animation};
        },
        [this](u32 address, i32 view, i32 pass)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("Super Scary bat draw of an address that is not a scene object");
            }
            actor_draw(actors->view(*object).bat.actor, view, pass);
        },
        [](std::span<const u32> message)
        {
            broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data())); // 0x00122C70
        },
        [this](std::span<const u32> message)
        {
            // 0x00122D10: DAT_00511B40 holds the players' actor +0x93C nodes (links[1]).
            for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.case_head.next);
                 node != nullptr;)
            {
                auto* current = node;
                node = static_cast<entity_detail::ListHead*>(node->next);
                NativePlayerInstance* owner = nullptr;
                for (auto* player : players->instances())
                {
                    if (static_cast<void*>(&player->links[1]) == static_cast<void*>(current))
                    {
                        owner = player;
                    }
                }
                if (owner == nullptr)
                {
                    unbound("DAT_00511B40 node with no native player");
                }
                std::array<u32, 0x42> words{};
                std::copy(message.begin(), message.end(), words.begin());
                object_message(*object_of(owner->actor_bytes.data()), words);
            }
        },
        [this](const char* name, const Vec3& position, float scale)
        {
            load_actor_camera_shake(name, position, scale, resources.registry, resources.error); // 0x000FB120
        },
        [](const Vec3& point) -> float
        {
            // 0x000874F0: the floor's cell +2 under the point; with no cell the float is the floor index's bits.
            const i32 floor = entity_detail::navigation_floor_index(point, -1, 0); // 0x00110BF0
            const i32 cell = entity_detail::navigation_cell_index(floor, point); // 0x00110EF0, EBX point
            if (cell == -1)
            {
                return std::bit_cast<float>(floor);
            }
            if (floor < 0 || floor >= entity_detail::kNavSlots || entity_detail::g_nav_grids[floor] == nullptr)
            {
                throw Error("0x000874F0 reads a cell of a floor DAT_0050E240 does not hold");
            }
            return float(entity_detail::g_nav_grids[floor]->cells.at(size_t(cell)).bottom); // Cell +2.
        },
        [this](u32 address)
        {
            auto& player = case_player(address);
            player.refresh_views();
            const auto* body_actor = actors->actors.at(scene.resolve_address(player.body_actor_address, 0).data());
            const Vec3 position = actor_world_position(*body_actor); // 0x000412A0, ESI body +8
            std::memcpy(player.body.data() + offsetof(BodyRecord, position.x), &position, sizeof(position));
        },
        super_scary_text,
        []() -> bool
        {
            return scene_end_flag_set(); // 0x00049D10
        },
        [this]() -> i32
        {
            return scene_player_character(game); // 0x00087830
        },
        [this](u32 player, u32 head, const u8* binding)
        {
            Actor* rider = nullptr;
            Actor* parent = nullptr;
            for (const auto& [bytes, actor] : actors->actors)
            {
                const u32 address = scene.address_of(bytes);
                if (address == player)
                {
                    rider = actor;
                }
                if (address == head)
                {
                    parent = actor;
                }
            }
            if (rider == nullptr)
            {
                throw Error("Super Scary binds a player that is not a scene actor (0x0003B500)");
            }
            bind_runtime_actor_marker_now(*rider, parent, binding, 0, origin, default_euler); // 0x0003B500
        }});
    scene.controls->context.super_scary = super_scary_services.get();
    bind_snapshot_texture_create();
    create_boot_render_targets();
    bind_book_services();
    map_view_services = std::make_unique<MapViewControlServices>(MapViewControlServices{*book_services,
        std::span<const MapViewRoom, 6>(reinterpret_cast<const MapViewRoom*>(data(0x00427628, 0x330).data()), 6),
        scene_name.data(),
        [this](u32 address) -> const char*
        {
            return address == 0 ? nullptr : image.string(address);
        },
        [this]() -> RuntimeActorPauseGroups&
        {
            return refresh_pause_groups();
        },
        dialogue_state.map_retained});
    scene.controls->context.map_view = map_view_services.get();
    glow_pass = std::make_unique<RuntimeGlowPass>(RuntimeGlowPass{*dialogue_outer, glow_targets, g_transition_guard,
        [this](u32 address)
        {
            return read_u32(data(address, 4), 0);
        },
        [this](u32 value)
        {
            d3d_device.set_state(0x40370, value); // D3D 0x001723C0, NV097_SET_STENCIL_OP_FAIL.
        },
        [this](u32 value)
        {
            d3d_device.set_state(SceneD3DDevice::kPSTextureModes, value); // D3D 0x00171020.
        },
        [this](u32 address)
        {
            // Loaded texture resources sit in the resource arena (from 0x40000000).
            if (address < 0x40000000)
            {
                unbound("glow texture outside the resource arena");
            }
            return resources.memory.tail(address).data();
        }});
    avatar_glow = std::make_unique<RuntimeAvatarGlow>(RuntimeAvatarGlow{*glow_pass, mirror_pass,
        [this](u32 address)
        {
            return read_u32(data(address, 4), 0);
        }});
    // The light grid's part vertex buffers (0x00112D70): a resource header and its data in the resource arena.
    light_grid_d3d.create_vertex_buffer = [this](u32 length) -> u32
    {
        const u32 header = resources.memory.allocate(0x14, 4).address;
        const u32 vertices = resources.memory.allocate(length, 0x10).address;
        auto& record = resources.d3d.resources[header];
        record.kind = 1;
        record.data = vertices;
        record.header = {length, 0, 0, 0, vertices, 0};
        return header;
    };
    light_grid_d3d.lock = [this](u32 buffer) -> std::span<u8>
    {
        return resources.memory.tail(resources.d3d.resources.at(buffer).data);
    };
    light_grid_d3d.release = [this](u32 resource)
    {
        resources.d3d.resources.erase(resource);
    };
    point_light_pass = std::make_unique<RuntimeScenePointLightPass>(RuntimeScenePointLightPass{*dialogue_outer,
        point_light_targets, glow_targets, light_grid, point_lights,
        RuntimeSceneCameraLight{g_flag_2104, g_camera_light_color, g_camera_light_second_color, g_camera_light_1c,
            g_camera_light_20, g_camera_light_24, g_camera_light_28, g_color_2134},
        display_view, model_renderer.fog_render_state, hud_size[0], hud_size[1],
        [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)
        {
            return snapshot_d3d.create_texture(width, height, levels, usage, format, pool); // D3D 0x00177840
        },
        [this](u32 stream, u32 buffer, u32 stride)
        {
            d3d_device.set_vertex_stream(stream, buffer, stride); // D3D 0x00173610
        },
        [this](u32 primitive, u32 first, u32 count)
        {
            d3d_device.draw_vertices(primitive, first, count); // D3D 0x00172920
        },
        [this](u32 stage, u32 texture)
        {
            d3d_device.dialogue().texture(stage, texture == 0 ? nullptr : resources.memory.pointer(texture));
        },
        [this](u32 value)
        {
            d3d_device.set_state(SceneD3DDevice::kZBias, value); // D3D 0x00171660
        },
        [this](u32 value)
        {
            d3d_device.set_state(SceneD3DDevice::kMultiSampleAntiAlias, value); // D3D 0x00172600
        },
        [this](u32 value)
        {
            d3d_device.set_state(SceneD3DDevice::kMultiSampleRenderTargetMode, value); // D3D 0x001725D0
        },
        [this](u32 address)
        {
            return read_u32(data(address, 4), 0);
        },
        [this](u32 address)
        {
            return reinterpret_cast<const float*>(data(address, 0xC * 16).data());
        },
        [this](u32 part) -> const Vec3&
        {
            // DAT_0050E374's +0x950 row's +4: its point rows (stride 0x30), the first one's position. Retail keeps
            // the background the light grid was built from (0x00112D70), not the list's current first.
            auto* first = backgrounds->live(light_grid_background) ? light_grid_background : nullptr;
            if (first == nullptr || part >= first->state.graphs.size() || first->state.graphs[part].points.size() < 12)
            {
                unbound("point-light part " + std::to_string(part) + " centre without the background's point graph ("
                    + (first == nullptr ? std::string("no background")
                        : std::to_string(first->state.graphs.size()) + " graphs"
                            + (part < first->state.graphs.size()
                                ? ", " + std::to_string(first->state.graphs[part].points.size()) + " point bytes"
                                : std::string())) + ")");
            }
            return *reinterpret_cast<const Vec3*>(first->state.graphs[part].points.data());
        },
        [this](const u8* record)
        {
            return resources.memory.address_of(record);
        }});
    bind_gameselect_services();
    final_score_services = std::make_unique<FinalScoreServices>(FinalScoreServices{*gameselect_services,
        *challenge_saves, {}, {}, {},
        []() -> i32
        {
            return i32(read_u32({g_scene_record, 0x108}, 0x104)); // DAT_00510F5C, scene record +0x104.
        }});
    scene.controls->context.final_score = final_score_services.get();
    gameselect_services->final_score = final_score_services.get();
    g_app_error_services = gameselect_services.get();
    bind_transition_services();
    bind_pause_services();
    // The scene model renderer over the relocated resources and the host D3D device.
    std::memcpy(retained_scene_color.data(), data(0x0032E4E4, 0x10).data(), 0x10);
    std::memcpy(override_base_color.data(), data(0x0042A570, 0x10).data(), 0x10);
    cull_mode = read_u32(data(0x0032E3D0, 4), 0);
    light_boundaries.set_constant = [this](u32 slot, const u32* words)
    {
        d3d_device.set_constant_words(slot, words, 1);
    };
    light_boundaries.transform_normal = [](Vec3& output, const Vec3& input, const float* matrix)
    {
        transform_runtime_graphics_normal(output, input, matrix);
    };
    light_boundaries.normalize = [](Vec3& output, const Vec3& input)
    {
        normalize_runtime_graphics_vector(output, input);
    };
    light_boundaries.transform_coord = [](Vec3& output, const Vec3& input, const float* matrix)
    {
        transform_runtime_graphics_coord(output, input, matrix);
    };
    light_upload = std::make_unique<SceneLightUploadContext>(SceneLightUploadContext{model_renderer,
        environment_world_space, retained_scene_color, light_records, light_boundaries});
    ambient_context = std::make_unique<SceneAmbientContext>(SceneAmbientContext{ambient_grids, 0, light_override, {}, {}, 0,
        ambient_points});
    std::memcpy(ambient_context->override_color.data(), data(0x0042A560, 0x10).data(), 0x10);
    // DAT_0042A584 / DAT_0042A588: the override light angles, .data.
    light_direction.override_pitch_degrees = read_f32(data(0x0042A584, 4), 0);
    light_direction.override_yaw_degrees = read_f32(data(0x0042A588, 4), 0);
    environment = std::make_unique<SceneEnvironmentSetupContext>(SceneEnvironmentSetupContext{*light_upload, fog,
        *ambient_context, light_direction,
        *reinterpret_cast<const std::array<float, 4>*>(background_ambient.light_color.data()), override_base_color,
        environment_primary, environment_secondary});
    const float override_intensity = read_f32(data(0x0042A580, 4), 0);
    light_construction = std::make_unique<SceneLightConstructionContext>(SceneLightConstructionContext{*environment,
        scene_color, retained_scene_color, g_scene_light_intensity, override_intensity, directional_light, shaders.records,
        temporary_lights});
    model_draw = std::make_unique<SceneModelDrawContext>(SceneModelDrawContext{render_image, *light_construction,
        matrix_boundaries});
    d3d_device.fog_enable = &model_renderer.fog_render_state;
    d3d_device.deferred_texture_states = &deferred_states[(0x00181970 - 0x00181968) / 4]; // DAT_00181970
    d3d_device.texture_size = [this](std::uintptr_t texture) -> std::array<u32, 2>
    {
        const auto& record = resources.d3d.resources.at(u32(texture));
        return {record.header[0], record.header[1]};
    };
    snapshot_d3d.surface_level = [this](u32 texture, u32 level)
    {
        return d3d_device.outer().surface(DialogueSurfaceHandle(texture), level);
    };
    snapshot_d3d.depth_stencil = [this]()
    {
        return d3d_device.outer().depth_buffer();
    };
    snapshot_d3d.render_target = [this]()
    {
        return d3d_device.outer().back_buffer(0);
    };
    snapshot_d3d.set_render_target = [this](std::uintptr_t color, std::uintptr_t depth)
    {
        d3d_device.outer().render_target(color, depth);
    };
    snapshot_d3d.z_enable = [this](u32 value)
    {
        d3d_device.set_state(SceneD3DDevice::kZEnable, value);
    };
    snapshot_d3d.stencil_enable = [this](u32 value)
    {
        d3d_device.set_state(SceneD3DDevice::kStencilEnable, value);
    };
    snapshot_d3d.set_state = [this](u32 method, u32 value)
    {
        d3d_device.set_state(method, value);
    };
    snapshot_d3d.clear = [this](u32 count, const void* rectangles, u32 flags, u32 color, float depth, u32 stencil)
    {
        d3d_device.outer().clear(count, rectangles, flags, color, depth, stencil);
    };
    snapshot_d3d.release = [this](std::uintptr_t handle)
    {
        d3d_device.outer().release(handle);
    };
    // Boot 0x0011C3F0 -> 0x00119160: the shadow volumes' depth ramp, and the regions cleared.
    create_runtime_shadow_ramp(shadow_volume_targets, background_regions, background_region_count,
        [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)
        {
            return snapshot_d3d.create_texture(width, height, levels, usage, format, pool); // D3D 0x00177840
        },
        [this](u32 texture)
        {
            return resources.memory.tail(resources.d3d.resources.at(texture).data); // D3DTexture_LockRect
        });
    snapshots = std::make_unique<RuntimeActorSnapshots>(RuntimeActorSnapshots{*display_setup, snapshot_d3d,
        deferred_states[(0x00181C70 - 0x00181968) / 4], depth_function, cull_mode,
        [this](Actor& actor, const AnimationMatrix& world, u32 kind)
        {
            if (actor.model == nullptr || actor.model->runtime == nullptr)
            {
                unbound("actor snapshot of an actor without a model");
            }
            RuntimeModelInstance& instance = *actor.model->runtime;
            flag_origins[&instance] = instance.node_flags.data();
            draw_runtime_scene_model(instance, resources.memory.address_of(instance.source->cpu.data()), world, kind,
                *model_draw);
        },
        actor_snapshots, actor_snapshot_count,
        [this]()
        {
            snapshot_saved = d3d_device.take_commands();
        },
        [this]()
        {
            auto commands = d3d_device.take_commands();
            const SceneD3DPresentContext present{resources.memory, resources.d3d, d3d_device.vertex_shaders,
                [this](const u8* record) -> const RgbaImage&
                {
                    return texture_image(record);
                },
                [this](std::uintptr_t color, std::uintptr_t depth)
                {
                    present_render_target(color, depth);
                },
                [this](u32 texture)
                {
                    copy_back_buffer(texture);
                },
                [this]()
                {
                    finish_render_target();
                }};
            present_scene_d3d_commands(commands, present);
            d3d_device.commands = std::move(snapshot_saved);
            snapshot_saved.clear();
        }});
    tree_dispatch = std::make_unique<ModelTreeDispatchContext>(ModelTreeDispatchContext{model_renderer, render_image,
        tree_stack, tree_stack_index, frame_combined, frame_cameras, render_retained_204, render_mask, fog_pixel_state,
        cull_mode, node_callbacks});
    tree_dispatch->multiply = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& first,
        const RuntimeShaderMatrix& second)
    {
        multiply_runtime_graphics_matrices(output.data(), first.data(), second.data());
    };
    tree_dispatch->scaling = [](RuntimeShaderMatrix& output, float x, float y, float z)
    {
        runtime_graphics_scaling_matrix(output.data(), x, y, z);
    };
    tree_dispatch->set_cull_mode = [this](u32 mode)
    {
        d3d_device.set_state(SceneD3DDevice::kCullMode, mode); // D3D 0x00171420.
    };
    tree_dispatch->set_fog_color = [this](u32 color)
    {
        d3d_device.set_state(SceneD3DDevice::kFogColor, color); // D3D 0x001713D0.
    };
    install_runtime_model_tree_dispatch(*tree_dispatch);
    install_runtime_model_group_dispatch(*tree_dispatch, node_callbacks);
    matrix_array_draw = std::make_unique<RuntimeModelMatrixArrayDraw>(RuntimeModelMatrixArrayDraw{
        *tree_dispatch, model_matrix_scratch, published_model_matrices});
    node_callbacks[0x00155F80] = [this](u32 node, void* storage)
    {
        dispatch_runtime_model_matrix_array(node, *static_cast<RuntimeModelInstanceStorage*>(storage),
            *matrix_array_draw);
    };
    node_callbacks[0x00157B90] = [this](u32 node, void* storage)
    {
        auto& frame = runtime_render_frame(model_renderer);
        const std::function<void(u32, const float*, u32)> constants =
            [this](u32 reg, const float* values, u32 count)
            {
                d3d_device.set_constant_words(reg, reinterpret_cast<const u32*>(values), count);
            };
        RuntimeModelRigidSkinDraw context{*matrix_array_draw, render_frames.at(&frame).projection,
            matrix_boundaries, constants};
        dispatch_runtime_model_rigid_skin(node, storage, context);
    };
    node_callbacks[0x001569D0] = [this](u32 node, void* storage)
    {
        const std::function<ModelPoseBuffer*(void*, u32)> pose = [](void* storage, u32 offset)
        {
            auto& instance = *static_cast<RuntimeModelInstanceStorage*>(storage);
            const auto found = instance.skeleton_poses.find(offset);
            if (found != instance.skeleton_poses.end())
            {
                return found->second;
            }
            if (read_u32(instance.bytes, offset) != 0)
            {
                throw Error("unmapped native skeleton pose pointer word");
            }
            return static_cast<ModelPoseBuffer*>(nullptr);
        };
        RuntimeModelSkeletonDraw context{*tree_dispatch, pose};
        dispatch_runtime_model_skeleton(node, storage, context);
    };
    render_nodes = std::make_unique<ModelRenderNodeContext>(ModelRenderNodeContext{*tree_dispatch, stream_descriptors,
        active_pixel_shader, depth_function, blend_enabled,
        [this](u32 shader)
        {
            d3d_device.set_state(SceneD3DDevice::kPixelShader, shader);
        },
        [this](u32 method, u32 value)
        {
            d3d_device.set_state(method, value);
        },
        [this](u32 enabled)
        {
            d3d_device.set_state(SceneD3DDevice::kZEnable, enabled);
        }});
    install_runtime_render_node_dispatch(*render_nodes, node_callbacks);
    model_shader_states = std::make_unique<RuntimeModelShaderStates>(RuntimeModelShaderStates{
        depth_function, deferred_states[(0x00181C70 - 0x00181968) / 4], blend_enabled,
        deferred_states[(0x00181C60 - 0x00181968) / 4], deferred_states[(0x00181C98 - 0x00181968) / 4],
        deferred_states[(0x00181C68 - 0x00181968) / 4], deferred_states[(0x00181C6C - 0x00181968) / 4],
        deferred_states[(0x00181C64 - 0x00181968) / 4],
        [this](u32 method, u32 value)
        {
            d3d_device.set_state(method, value);
        },
        [this](u32 value)
        {
            d3d_device.set_state(SceneD3DDevice::kZEnable, value);
        }});
    model_shader_draw = std::make_unique<RuntimeModelShaderDraw>(RuntimeModelShaderDraw{
        *tree_dispatch, *model_shader_states,
        [this](u32 shader)
        {
            d3d_device.dialogue().pixel_shader(shader); // D3DDevice_SetPixelShader (0x00175370).
        }, model_library_hooks});
    model_shader_combiner = std::make_unique<RuntimeModelShaderCombiner>(RuntimeModelShaderCombiner{
        *model_shader_draw, std::span<u32>(deferred_states).subspan((0x00181B70 - 0x00181968) / 4),
        deferred_states[0], deferred_states[(0x00181C44 - 0x00181968) / 4], shader_texture_stage,
        [this](u32 setter, u32 value)
        {
            d3d_device.set_state(setter == 0x001722A0 ? SceneD3DDevice::kZEnable : setter, value);
        }});
    node_callbacks[0x00157CE0] = [this](u32 node, void*)
    {
        dispatch_runtime_model_shader(node, *model_shader_draw);
    };
    transform_draw = std::make_unique<RuntimeModelTransformDraw>(RuntimeModelTransformDraw{
        *tree_dispatch, frame_cameras, [this](i32 index) -> RuntimeShaderMatrix&
        {
            return render_frames.at(&render_frame_storage[index]).projection;
        }, model_projection_register, model_world_register, matrix_boundaries,
        [this](u32 reg, const float* values, u32 count)
        {
            d3d_device.set_constant_words(reg, reinterpret_cast<const u32*>(values), count);
        }});
    node_callbacks[0x00157FC0] = [this](u32 node, void* storage)
    {
        dispatch_runtime_model_transform(node, static_cast<RuntimeModelInstanceStorage*>(storage)->bytes,
            *transform_draw);
    };
    node_callbacks[0x00157D50] = [this](u32 node, void* storage)
    {
        dispatch_runtime_model_billboard(node, static_cast<RuntimeModelInstanceStorage*>(storage)->bytes,
            *transform_draw);
    };
    node_callbacks[0x00157ED0] = [this](u32, void*)
    {
        restore_runtime_model_transform(*transform_draw);
    };
    node_callbacks[0x00157D30] = [this](u32, void*)
    {
        restore_runtime_model_billboard(*transform_draw);
    };
    vertex_fragments = std::make_unique<RuntimeShaderFragmentContext>(RuntimeShaderFragmentContext{
        model_renderer, shader_fragments->buckets, shader_fragments->programs});
    vertex_cache = std::make_unique<RuntimeModelVertexShaderCacheContext>(RuntimeModelVertexShaderCacheContext{
        *resources.shader_cache, render_image, *vertex_fragments, vertex_fragment_word,
        [this](const i32* declaration, const u32* program, u32& handle, u32 usage)
        {
            d3d_device.create_vertex_shader(declaration, program, handle, usage);
        }});
    vertex_draw = std::make_unique<RuntimeModelVertexShaderDraw>(RuntimeModelVertexShaderDraw{
        *transform_draw, *vertex_cache, stream_descriptors, vertex_parameters, vertex_auxiliary,
        [this](u32 stream, u32 resource, u32 stride)
        {
            d3d_device.set_vertex_stream(stream, resource, stride);
        },
        [this](u32 handle)
        {
            d3d_device.set_vertex_shader(handle); // D3DDevice_SetVertexShader (0x001739F0)
        }});
    node_callbacks[0x00158510] = [this](u32 node, void*)
    {
        // Pointer-width view of the existing fog words at render state +0x198/+0x19C.
        vertex_parameters[0] = model_renderer.state.fog_reciprocal;
        vertex_parameters[1] = model_renderer.state.fog_scaled_end;
        dispatch_runtime_model_vertex_shader(node, *vertex_draw);
    };
    // DAT_00336130 starts with its .data value; boot 0x000F8740 bound the hooks.
    std::memcpy(material_globals.material_vector.data(), data(0x00336130, 0xC).data(), 0xC);
    material_bind = std::make_unique<RuntimeModelMaterialBind>(RuntimeModelMaterialBind{*tree_dispatch,
        material_globals, texture_records, deferred_states, deferred_states[(0x00181C60 - 0x00181968) / 4],
        deferred_states[(0x00181C58 - 0x00181968) / 4], deferred_states[(0x00181C64 - 0x00181968) / 4],
        environment_primary, environment_secondary, model_library_hooks,
        [this](u32 stage, u32 texture)
        {
            d3d_device.dialogue().texture(stage, texture == 0 ? nullptr : render_image.pointer(texture));
        },
        [this](u32 stage, u32 type, u32 value)
        {
            // D3D_g_DeferredTextureState, DAT_00181970.
            deferred_states[2 + stage * 0x20 + type] = value;
        },
        [this](u32 reg, const float* values, u32 count)
        {
            d3d_device.set_constant_words(reg, reinterpret_cast<const u32*>(values), count);
        },
        [this](u32 reg, const float* values, u32 count)
        {
            d3d_device.outer().pixel_constants(reg, values, count);
        },
        [this](u32 method, u32 value)
        {
            d3d_device.set_state(method, value);
        }});
    node_callbacks[0x00157090] = [this](u32 node, void* storage)
    {
        dispatch_runtime_model_shader_parameters(node, *static_cast<RuntimeModelInstanceStorage*>(storage),
            *material_bind);
    };
    node_callbacks[0x001571B0] = [this](u32 node, void* storage)
    {
        dispatch_runtime_background_shader_parameters(node, *static_cast<RuntimeModelInstanceStorage*>(storage),
            *material_bind, bound_culling);
    };
    blendshape_draw = std::make_unique<RuntimeModelBlendshapeDraw>(RuntimeModelBlendshapeDraw{*tree_dispatch,
        stream_descriptors, vertex_auxiliary,
        [this](RuntimeBlendshapeVertexBuffer& buffer)
        {
            // The instance's 0x18-byte entry; the vertex shader draw binds its D3D resource at +4.
            const u32 entry = heap_address(&buffer, 0x18);
            resources.d3d.resources[entry + 4].data = resources.memory.address_of(buffer.data);
            return entry;
        }});
    node_callbacks[0x00157A50] = [this](u32 node, void* storage)
    {
        dispatch_runtime_model_blendshape(node, *static_cast<RuntimeModelInstanceStorage*>(storage),
            *blendshape_draw);
    };
    screen_model_draw = std::make_unique<RuntimeScreenModelDraw>(RuntimeScreenModelDraw{*display_setup, *model_draw});
    button_bat_draw = std::make_unique<RuntimeButtonBatDraw>(RuntimeButtonBatDraw{*screen_model_draw,
        d3d_device.outer(), origin});
    push_buffers = std::make_unique<ModelPushBufferDrawContext>(ModelPushBufferDrawContext{*tree_dispatch,
        flag_origins,
        [this](u32 primitive, u32 count, u32 indices)
        {
            d3d_device.draw_indexed(primitive, count, indices); // Carries the deferred address modes.
        }});
    install_runtime_push_buffer_dispatch(*push_buffers, node_callbacks);
    // Camera director (0x000AA250) and rig (0x000781F0) tunables are .data words read from the image.
    std::memcpy(&rig_tunables.rig, data(0x004274B8, sizeof(CameraRigTunables)).data(), sizeof(CameraRigTunables));
    motion_steps = i32(read_u32(data(0x004312E0, 4), 0));
    {
        const auto words = data(0x00428708, 0x2C);
        director_data = DirectorData{read_u32(words, 0), i32(read_u32(words, 4)), i32(read_u32(words, 8)),
            read_u32(words, 0xC), read_f32(words, 0x10), read_f32(words, 0x14), read_u32(words, 0x18),
            read_u32(words, 0x1C), i32(read_u32(words, 0x20)), read_u32(words, 0x24), read_u32(words, 0x28),
            rig_tunables.view_real(camera_view_tunable::near_range)};
    }
    director_tunables = std::make_unique<RuntimeDirectorTunables>(RuntimeDirectorTunables{director_data.keep_state,
        director_data.minimum_count, director_data.minimum_priority, director_data.ground_heights,
        director_data.range_extension, director_data.lead_distance, director_data.focus_targets,
        director_data.carried_targets, director_data.region_mode, director_data.skip_candidates,
        director_data.input_heading, director_data.near_range});
    director_targets = std::make_unique<RuntimeDirectorTargets>(RuntimeDirectorTargets{*director_tunables,
        g_flags_ea9c,
        [this]()
        {
            return marked_actors();
        },
        [this](u32 address)
        {
            return director_actor(players->player_at(address));
        },
        [this](std::span<u8, 0x20> output, u32 address)
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            player.camera_source.body = player.body;
            player.camera_source.case_link = player.attached != nullptr
                ? std::span<const u8>(player.attached->bytes) : std::span<const u8>();
            build_runtime_player_camera_target(output, player.camera_source, camera);
        },
        [this](u32 address) -> const RuntimeDialogueInputDevice*
        {
            for (auto* player : players->instances())
            {
                if (player_record(player->actor_bytes).input_device == address)
                {
                    player->refresh_input_views();
                    return &player->input_device;
                }
            }
            unbound("input device view for actor +0x960 word " + std::to_string(address));
        },
        input_state});
    camera_rig = std::make_unique<RuntimeCameraRigContext>(RuntimeCameraRigContext{rig_tunables, *director_tunables,
        clock, unlock_flags, powerup_rows[0x11].active, render_width, render_height, director_targets->actor_at,
        director_targets->input_device, input_state});
    body_services = std::make_unique<SceneBodyServices>(SceneBodyServices{game, scene.factory, image.pools,
        scene.objects, sounds, effects, latches, entity_detail::g_world.body_a, global_attachments, origin, default_euler, random,
        address_zero, clock, statsheet.data(), object_tags.data(), *damage_tables, *footsteps, surface_names,
        object_tags, image.string(0x0031C354),
        {image.string(0x00315A98), image.string(0x00315AE0), image.string(0x00315AFC), image.string(0x00315AF4)},
        std::span<const char* const, 0x55>(callout_names.data(), 0x55),
        std::span<const char* const, 0x41>(flag_names.data(), 0x41), shock_pass, shock_fail, data(0x00217DE8, 0xD0),
        death_actions, pause_references, decay_enabled, decay_index,
        powerup_rows[8].active, g_flags_ea9c, g_flag_ea98,
        reinterpret_cast<u32&>(g_script_30d0c), goal_scratch, empty_name, occupancy_initial,
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        [this](u32 address) -> Actor*
        {
            return actor_or_retired(address);
        },
        [this](u32 address) -> std::span<const u8>
        {
            if (const auto released = released_actors.find(address); released != released_actors.end())
            {
                return released->second.bytes;
            }
            auto* object = object_of(scene.resolve_address(address, 0).data());
            return {object->bytes.get(), object->size};
        },
        [this](u32 address, std::span<const u32> words)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            try
            {
                object_message(*object, {const_cast<u32*>(words.data()), words.size()});
            }
            catch (const std::exception& error)
            {
#if defined(GBTG_INSPECTOR_APP)
                if (game.inspection != nullptr)
                {
                    const auto found = game.inspection->owners.find(address);
                    if (found != game.inspection->owners.end())
                    {
                        throw SceneInspectionFailure(found->second,
                            "activation / message " + std::to_string(words[0]), error.what());
                    }
                }
#endif
                throw Error("activation actor " + std::to_string(address) + " type "
                    + std::to_string(object->type) + " message " + std::to_string(words[0])
                    + ": " + error.what());
            }
        },
        [this](u32 address) -> const entity_detail::ListHead*
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            return object->type == 5 ? &players->player(*object).links[0]
                : &actors->view(*object).body_node;
        },
        [this](u32 address) -> RuntimeBodyAttachedActor&
        {
            return attached_actor_at(address); // A player, or a weapon (a thrown one's knock-down sphere, 0x17).
        },
        [this](u32 address) -> std::span<const u8>
        {
            return players->player_at(address).body;
        },
        rig_tunables, entity_detail::g_look_profile, motion_steps, std::span<u32>(node_cache),
        {data(0x00338F64, 0x24), data(0x00338F88, 0x24), data(0x00338EE0, 0x84), data(0x00338EA4, 0x3C)},
        0.010000001f, // DAT_0032B22C.
        [this]()
        {
            std::array<float, 8> phases;
            std::memcpy(phases.data(), data(0x00339900, 0x20).data(), 0x20);
            return phases;
        }(), 0.45f, -0.3f, // DAT_0032B4F8, DAT_0032B3D8.
        1.5707964f, body_shape_push, // DAT_0032B140.
        [this]() -> std::span<const u8>
        {
            return door_pool == nullptr ? std::span<const u8>()
                : std::span<const u8>(door_pool->begin, size_t(door_pool->end - door_pool->begin));
        }, body_slide_contacts, {reinterpret_cast<const float*>(data(0x00217EC0, 0x48).data()), 18},
        [this](u32 address) -> std::span<const u8>
        {
            return scene.resolve_tail(address);
        }, data(0x004275BC, 0x14), *camera_rig, pause, frame_delta, time_scale, pal, pal_override,
        [this](u32 address) -> std::span<const u8>
        {
            const auto found = target_views.find(address);
            if (found == target_views.end())
            {
                unbound("mind target record " + std::to_string(address) + " outside DAT_00545988");
            }
            return found->second->bytes;
        },
        [this](u32 address, size_t size) -> std::span<const u8>
        {
            // The native address arenas: resources from 0x40000000, host heap blocks from 0x30000000, scene
            // objects from 0x10000000, and the XBE image below.
            if (address >= 0x40000000)
            {
                return resources.memory.tail(address);
            }
            if (address >= 0x30000000)
            {
                unbound("data record in the host heap arena");
            }
            if (address >= 0x10000000)
            {
                return scene.resolve_tail(address);
            }
            return image.memory(address, size);
        },
        [this](u32 address, u32 id)
        {
            actor_callouts(address, id, &g_actor_creation_options);
        },
        data(0x00217DB0, 0x108),
        [this](u32 address) -> RuntimeBodyDamageEntry&
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            auto& graph = body_messages[player.body.data()];
            if (graph == nullptr)
            {
                graph = std::make_unique<SceneBodyMessages>(*body_services, player);
            }
            return graph->damage_entry();
        },
        [this](std::span<const u8> parameters) -> u32
        {
            FactoryObject* object = scene.keep_object(create_factory_object(scene.factory, parameters));
            return object == nullptr ? 0 : address_of(object->bytes.get());
        },
        [this](u32 address) -> NativeActorFactoryInstance&
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("actor view of an object outside the scene");
            }
            return actors->view(*object);
        },
        [this](u32 body_address)
        {
            return body_state_context(body_address);
        },
        g_50ea01, {&g_50ea7c, &g_50ea80, &g_50ea84, &g_50ea88, &g_50ea8c, &g_50ea90},
        std::span<const u8, 2>(data(0x00433D08, 2).data(), 2),
        std::span<const float, 2>(reinterpret_cast<const float*>(data(0x00433D0C, 8).data()), 2),
        [this](u32 address, u32 id, const RuntimeActorCreationOptions* options)
        {
            actor_callouts(address, id, options);
        },
        [this](i32 count, u32 player, u32 actor) -> i32
        {
            // Xbox 0x0013FC50: each slot's records are copied first (0x000FCC90), then each owner gets message 8
            // with its copy, which is freed after; the replies' +4 words are summed.
            std::vector<std::pair<entity_detail::SegmentProbe*, GhoulySpawnPool*>> kept;
            for (i32 index = 0; index < count; ++index)
            {
                auto& slot = entity_detail::g_segment_slots[index];
                kept.emplace_back(slot.object, copy_segment_records(slot.pool));
            }
            i32 total = 0;
            for (auto& [probe, records] : kept)
            {
                RuntimeSweepNotice notice{8, 0, records, player, actor};
                segment_contact_actor(*probe).message(&notice);
                engine_heap_free(records);
                total += std::bit_cast<i32>(notice.reply);
            }
            return total;
        },
        [this](u32 address) -> const char*
        {
            return address == 0 ? nullptr : image.string(address);
        }});
    body_services->dizzy_input_preset = data(0x00428660, 0x10);
    body_services->camera_pulse = &pulse;
    body_services->streams = [this]() -> AudioBankStreams&
    {
        if (game.factory_scene == nullptr || game.factory_scene->controls == nullptr)
        {
            unbound("player death streams without the scene controls");
        }
        return game.factory_scene->controls->context.streams();
    };
    body_services->frontend_state = &frontend_state;
    body_services->dialogue = dialogue.get();
    body_services->begin_transition = []()
    {
        begin_scene_transition(reinterpret_cast<const char*>(g_scene_record + 0x80)); // 0x0012C5D0
    };
    body_services->weapon_attachment_at = [this](u32 address) -> RuntimePlayerAttachmentActor&
    {
        return weapon_attachment_at(address);
    };
    body_services->weapon_collision = [this](u32 address) -> RuntimeBodyCollisionState*&
    {
        const u8* bytes = scene.resolve_address(address, 0).data();
        if (!classes->owns_weapon(bytes))
        {
            unbound("attached actor +0x998 collision view of an actor that is not a scene weapon");
        }
        return classes->weapon_markers(bytes);
    };
    std::memcpy(&body_services->dizzy_input_scale, data(0x00428670, 4).data(), 4);
    body_services->contact_actor = [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
    {
        return segment_contact_actor(probe);
    };
    body_services->contact_totals = contact_totals;
    body_services->markers = &markers;
    body_services->contact_session = contact_session;
    body_services->contact_words = [this](NativePlayerInstance& player, std::span<u32> words, GhoulySpawnPool* contacts,
        size_t contacts_word)
    {
        dispatch_contact_words(player, words, contacts, contacts_word);
    };
    body_services->jump_tunables = &jump_tunables;
    body_services->struggle_preset = data(0x00302728, 0x10);
    body_services->bandaged_preset = data(0x0030260C, 0x10);
    body_services->actor_fade = [this](u32 address, float duration, const float* tint)
    {
        Actor& actor = *actors->actors.at(scene.resolve_address(address, 0).data());
        auto& object = object_of_actor(actor);
        const std::function<FactoryObject*(FactoryObject)> keep = [this](FactoryObject made)
        {
            return scene.keep_object(std::move(made));
        };
        RuntimeActorFade fade{actor, {object.bytes.get(), object.size}, address, scene.factory, keep, empty_name};
        spawn_runtime_actor_fade(duration, 0, 0, tint, fade); // 0x0013AF80
    };
    body_services->blood_suck_target = [this](u32 address) -> RuntimeBloodSuckTarget
    {
        RuntimeBloodSuckTarget target;
        target.peer = body_services->jump_peer(address);
        auto* object = object_of(scene.resolve_address(address, 0).data());
        target.view = &actors->view(*object);
        auto* player = players->find_player_at(address);
        if (player == nullptr)
        {
            return target;
        }
        auto& graph = body_messages[player->body.data()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        target.damage = &graph->damage_entry();
        target.body_message = [this, player](std::span<const u32> words)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            auto& owner = body_messages[player->body.data()];
            owner->dispatch(player->body_object, buffer); // Body vtable +0xC.
        };
        const u32 held = body_record(player->body).attached;
        if (held != 0)
        {
            auto* held_object = object_of(scene.resolve_address(held, 0).data());
            if (held_object == nullptr || !actors->actors.contains(held_object->bytes.get()))
            {
                unbound("blood suck target's held actor (body +0x48) is not a scene actor");
            }
            target.held = actors->actors.at(held_object->bytes.get());
            target.held_view = &actors->view(*held_object);
        }
        return target;
    };
    body_services->released_position = [this](u32 address) -> const Vec3*
    {
        const auto found = released_actors.find(address);
        return found == released_actors.end() ? nullptr : &found->second.position;
    };
    body_services->bind_block = [this](void* block, size_t size) -> u32
    {
        const u32 found = scene.find_address(block);
        if (found != 0)
        {
            return found;
        }
        weapon_bound.insert(block);
        return scene.bind_address({static_cast<u8*>(block), size});
    };
    body_services->unbind_block = [this](void* block)
    {
        if (weapon_bound.erase(block) != 0)
        {
            scene.unbind_address(block);
        }
    };
    body_services->intro = &intro_notices;
    body_services->story_byte = &story_bits[0];
    body_services->reply_message = [this](u32 address, std::span<u32> words)
    {
        auto* object = object_of(scene.resolve_address(address, 0).data());
        if (object == nullptr)
        {
            unbound("jump on/off message to an address that is not a scene object");
        }
        std::array<u32, 0x42> buffer{};
        std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
        object_message(*object, buffer);
        std::copy_n(buffer.begin(), std::min<size_t>(words.size(), buffer.size()), words.begin());
    };
    body_services->jump_peer = [this](u32 address) -> RuntimeJumpOnPeer
    {
        auto* object = object_of(scene.resolve_address(address, 0).data());
        if (object == nullptr || !actors->actors.contains(object->bytes.get()))
        {
            unbound("jump on/off peer at an address that is not a scene actor");
        }
        RuntimeJumpOnPeer peer;
        peer.actor = &actors->view(*object).bat.actor;
        peer.address = address;
        peer.bytes = {object->bytes.get(), object->size};
        if (auto* player = players->find_player_at(address); player != nullptr)
        {
            player->refresh_views();
            peer.body = player->body;
            peer.bits = player->body_bits;
            peer.mind = player->mind;
            peer.model = model_views_of(*peer.actor).nodes;
            peer.input = player->input.empty() ? nullptr : &player->input_device;
            if (peer.body.empty() || peer.bits == nullptr)
            {
                unbound("jump on/off peer whose body has no +0x15C bit block");
            }
        }
        return peer;
    };
    generate_ghouly = std::make_unique<RuntimeGenerateGhoulyScene>(RuntimeGenerateGhoulyScene{resources.registry,
        resources.error, shaders, live_actors,
        [this]() -> GhoulySpawnContext&
        {
            return *ghouly_spawns;
        },
        [this](u32 address) -> RuntimeActorLink&
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("actor +0x900 link of an object outside the scene");
            }
            return actors->view(*object).startup.live;
        },
        [this](u32 address) -> u32
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            return player.body.empty() ? 0u : body_record(player.body).attached; // Actor +0x95C -> +0x48.
        },
        [this](u32 address)
        {
            auto& player = players->player_at(address);
            player.refresh_views();
            drain_runtime_player_goals(player.goal_cleanup); // 0x00028210
        },
        [this](FactoryObject object) -> u32
        {
            FactoryObject* kept = scene.keep_object(std::move(object));
            return kept == nullptr ? 0u : address_of(kept->bytes.get());
        },
        [this](u32 address)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("0x001075C0 release of an object outside the scene");
            }
            release_factory_object(scene.factory, *object); // 0x001075C0
        },
        [this](const char* name, u32 actor) -> u32
        {
            // 0x0003DF40: the named template cloned (0x00107720) with the actor at +8, then made (0x00107520).
            auto clone = clone_executable_resource(image.pools, name);
            if (clone.bytes == nullptr)
            {
                return 0;
            }
            std::memcpy(clone.bytes.get() + 8, &actor, 4);
            FactoryObject* made = scene.keep_object(create_factory_object(scene.factory,
                {clone.bytes.get(), clone.size}));
            clone.bytes.reset(); // 0x000663E0.
            return made == nullptr || made->bytes == nullptr ? 0u : address_of(made->bytes.get());
        },
        [this](u32 address, float duration, const float* tint)
        {
            Actor& actor = *actors->actors.at(scene.resolve_address(address, 0).data());
            auto& object = object_of_actor(actor);
            const std::function<FactoryObject*(FactoryObject)> keep = [this](FactoryObject made)
            {
                return scene.keep_object(std::move(made));
            };
            RuntimeActorFade fade{actor, {object.bytes.get(), object.size}, address, scene.factory, keep, empty_name};
            spawn_runtime_actor_fade(duration, 0, 0, tint, fade); // 0x0013AF80
        },
        [this](u32 address)
        {
            activate_actor_bind_tree(*actors->actors.at(scene.resolve_address(address, 0).data())); // 0x00112CC0
        },
        [this]() -> RuntimeDisplaySetupContext&
        {
            return *display_setup;
        },
        [this]() -> BillboardFlushContext
        {
            return BillboardFlushContext{g_billboard_queue, *dialogue_draw, d3d_device.outer(), render_image};
        }});
    body_services->release_frame_object = [this](Actor& actor, u32 object)
    {
        return release_actor_frame_object(actor, object);
    };
    body_services->generate_ghouly = [this]() -> RuntimeGenerateGhoulyScene&
    {
        return *generate_ghouly;
    };
    body_services->marks = [this]()
    {
        if (director_targets == nullptr)
        {
            unbound("DAT_00511B38 walk before the director targets");
        }
        return director_targets->marks();
    };
    body_services->cutscenes = [this]() -> SceneCutsceneRuntime&
    {
        return scene_cutscene_runtime(game);
    };
    body_services->door_names = door_names.get();
    body_services->heap_address = [this](const void* pointer)
    {
        return heap_address(pointer, 4);
    };
    state_action_names = strings(0x003390E4, 0x174);
    goal_flag_names = strings(0x003397AC, 3);
    mind_services = std::make_unique<SceneMindServices>(SceneMindServices{game, resources.registry, resources.error,
        random, std::span<const char* const, 0x41>(flag_names.data(), 0x41), state_action_names.data(),
        callout_names.data(), goal_flag_names.data(), resume, story_bits, g_flag_ea98, mind_settings.count,
        mind_settings.rows, g_nav_grids, navigation_nodes, data(0x002FFEEE, 0x40), input_activation, input_release,
        port_devices, g_script_30d0c, g_flags_ea9c, pulse, image.attack_tables,
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        [this](u32 address) -> Actor*
        {
            // Mind +0x10 holds a DAT_00545988 target record; its actor is record +0x10.
            if (const auto found = target_records.find(address); found != target_records.end())
            {
                return &found->second->actor;
            }
            if (const auto retired = retired_targets.find(address); retired != retired_targets.end())
            {
                return &retired->second->actor;
            }
            return actor_or_retired(address);
        },
        [this](u32 address) -> std::span<const u8>
        {
            if (const auto found = target_views.find(address); found != target_views.end())
            {
                return found->second->bytes;
            }
            if (const auto retired = retired_targets.find(address); retired != retired_targets.end())
            {
                return retired->second->bytes;
            }
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("object view of a retail address outside the scene objects");
            }
            return {object->bytes.get(), object->size};
        },
        clock, {address_zero.data(), address_zero.size()}, occupancy_initial, input_state, keyboard,
        [this](FactoryObject& mind, RuntimePlayerMindRequest& request)
        {
            return players->minds.update_request(mind, request);
        },
        [this](FactoryObject& mind, RuntimePlayerMindRequest& request)
        {
            return players->minds.initialize_request(mind, request);
        },
        pause_references, *health_display, script_records, sounds, *damage_tables,
        {reinterpret_cast<const float*>(data(0x00217EC0, 0x48).data()), 18},
        [this](u32 address) -> RuntimeBodyAttachedActor&
        {
            return attached_actor_at(address);
        },
        [this](u32 address) -> RuntimeMindTarget&
        {
            // A mind can still name a record released this frame (retail reads its freed block); the retired
            // copy keeps those bytes.
            return *mind_target_at(address);
        },
        [this]() -> u32
        {
            const auto* next = entity_detail::g_world.alt.next;
            return next == nullptr ? 0 : address_of(next);
        },
        targets, target_views, target_eligibility, target_addresses, target_queries, embedded_targets, query,
        powerup_rows[0x11].active, // DAT_005122D8, the ReverseControls powerup row.
        data(0x002FFF24, 0x10),
        read_f32(data(0x004275B8, 4), 0), // angular_limit
        scoreboard_globals.meter_bd0, // DAT_00433BD0, the meter buzzer 0x0012ED30 plays and clears
        statsheet, data(0x00433D5C, 0xC), powerup_rows[11].active, powerup_rows[8].active,
        [this](u32 address) -> const u8*
        {
            // The native address arenas: resources from 0x40000000, host heap blocks from 0x30000000, scene
            // objects from 0x10000000, and the XBE image below.
            if (address >= 0x40000000)
            {
                return resources.memory.tail(address).data();
            }
            if (address >= 0x30000000)
            {
                unbound("retail data address in the host heap arena");
            }
            if (address >= 0x10000000)
            {
                return scene.resolve_tail(address).data();
            }
            return image.memory(address, 4).data();
        },
        [this](u32 address) -> std::span<const u8>
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("target body of an address outside the scene objects");
            }
            if (object->type == 5)
            {
                return players->player(*object).body;
            }
            // Other classes keep their own word at actor +0x95C (a weapon's, type 0xAA, holds a small count); the
            // target code reads it only for its own kinds, so a zero word, or one that is no scene object, gives an
            // empty view that raises if it is read.
            const u32 body = read_u32({object->bytes.get(), object->size}, 0x95C);
            if (body < 0x10000000 || body >= 0x30000000)
            {
                return {};
            }
            return scene.resolve_tail(body);
        },
        [this](const void* pointer, size_t size)
        {
            return heap_address(pointer, size);
        }});
    resources.memory.on_free = [this](const u8* begin, size_t size)
    {
        forget_texture_images(begin, size);
    };
    // A texture given back its disc bytes is decoded again, as after a movie frame.
    resources.on_texture_restored = [this](u32 data)
    {
        for (const auto& [record, resource] : resources.d3d.resources)
        {
            if (resource.kind == 3 && resource.data == data)
            {
                movie_textures.insert(resources.memory.pointer(record));
            }
        }
    };
    active_services = this;
    query.actors = [this]()
    {
        return marked_actor_views();
    };
    // 0x00028360 and the type-0x6F wait read a player's +0x980 goal ring and +0x984 word.
    query.goals = [this](const Actor& actor) -> RuntimeScriptActorGoals
    {
        for (auto* player : players->instances())
        {
            if (&player->actor == &actor)
            {
                return RuntimeScriptActorGoals{player->actor_bytes, player->ring};
            }
        }
        return {};
    };
    // 0x001412F0's test of DAT_00511B0C: actors wait on the live list to be activated.
    g_live_actors_pending = [this]()
    {
        return live_actors.next != nullptr;
    };
    // 0x001412F0's walk of DAT_00511B38: each player's +0x980 goal ring front (an empty ring reads address zero,
    // as retail does), +0x984 and +0x98C.
    g_loading_actors = [this]()
    {
        std::vector<RuntimeLoadingActor> actors;
        for (const auto& mark : marked_actors())
        {
            const RuntimePlayerGoalRing* ring = nullptr;
            for (auto* player : players->instances())
            {
                if (&player->actor == mark.actor)
                {
                    ring = player->ring;
                }
            }
            u16 goal = 0;
            if (ring != nullptr)
            {
                goal = ring->tail == ring->capacity ? read_u16(address_zero, 0)
                    : read_u16(ring->rows, size_t(ring->head) * ring->stride);
            }
            actors.push_back({read_u16(mark.bytes, 6), goal, read_u32(mark.bytes, 0x984), read_u32(mark.bytes, 0x98C),
                !mark.body.empty(), mark.body.empty() ? u16(0) : body_record(mark.body).energy});
        }
        return actors;
    };
    // 0x00122CE0's walk of DAT_00511B38: each actor's vtable +0xC.
    g_marked_actor_message = [this](std::span<const u32> message)
    {
        for (const auto& mark : marked_actors())
        {
            std::array<u32, 0x42> words{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
            auto* object = object_of(scene.resolve_address(mark.address, 0).data());
            if (object == nullptr)
            {
                unbound("DAT_00511B38 actor with no factory object");
            }
            object_message(*object, words);
        }
    };
    // 0x0013E980's walk of DAT_00511B50: each mode-2 player's +0x94C node, its +0xCE0, +0xCDC and +0x93C words and
    // its body's +0x24 position.
    g_runtime_invisibility = &powerup_rows[9].active; // DAT_00512218.
    g_runtime_weapon_boost = &powerup_rows[0xD].active; // DAT_00512278.
    g_grim_reaper_dizzy = &powerup_rows[2].active; // DAT_00512170.
    g_spawn_trigger_players = [this]()
    {
        std::vector<GhoulySpawnPlayerLink> links;
        for (const auto* node = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.slot.next);
             node != nullptr; node = static_cast<const entity_detail::ListHead*>(node->next))
        {
            NativePlayerInstance* owner = nullptr;
            for (auto* player : players->instances())
            {
                if (&player->links[3] == node)
                {
                    owner = player;
                }
            }
            if (owner == nullptr || owner->body.empty())
            {
                unbound("DAT_00511B50 node with no player and body");
            }
            const std::span<const u8> bytes = owner->actor_bytes;
            links.push_back({&owner->actor, i32(read_u32(bytes, 0xCE0)), i16(read_u16(bytes, 0xCDC)),
                read_u32(bytes, 0x93C), Vec3{body_record(owner->body).unk_24.x, body_record(owner->body).unk_24.y,
                    body_record(owner->body).unk_24.z}});
        }
        return links;
    };
    // Xbox 0x00117400's walk of DAT_00511B38 for the script and the dialogue.
    g_actor_pause = [this](u8 mask, i32 release)
    {
        RuntimeActorPauseGroups& groups = refresh_pause_groups();
        const bool walking = pause_walking;
        pause_walking = true;
        try
        {
            change_runtime_actor_pause(mask, release, groups);
        }
        catch (...)
        {
            pause_walking = walking;
            throw;
        }
        pause_walking = walking;
    };
    // Xbox 0x00122C70: every active actor (DAT_00511AA4, actor +0x900) gets the message through vtable +0xC. The
    // native senders' payloads are retail word arrays, except three that carry host pointers.
    g_actor_broadcast = [this](const i32* raw)
    {
        std::array<u32, 0x42> words{};
        const u32 id = u32(raw[0]);
        words[0] = id;
        switch (id)
        {
        case 0x8D:
        {
            const auto& release = *reinterpret_cast<const ActorAttachmentReleaseMessage*>(raw);
            words[1] = heap_address(release.record, 0x68);
            words[2] = u32(release.value);
            break;
        }
        case 0x85:
            words[1] = address_of(reinterpret_cast<const ActorModelAnchorReleaseMessage*>(raw)->anchor);
            break;
        case 0x81:
        {
            // 0x000D09A0's start notice: the steps (DAT_005064E0 rows) by address.
            const auto& start = *reinterpret_cast<const ScriptEventStartMessage*>(raw);
            words[1] = start.door;
            words[2] = start.count;
            words[3] = start.steps == nullptr ? 0 : heap_address(start.steps, size_t(start.count) * 0x88);
            break;
        }
        case 0x7E:
        case 0x7F:
        {
            // A cutscene instance (0x0004DB20's heap block): its stable heap address.
            const void* instance = reinterpret_cast<const ActorReferenceMessage*>(raw)->value;
            words[1] = instance == nullptr ? 0 : heap_address(instance, 4);
            break;
        }
        default:
        {
            static const std::unordered_map<u32, u32> lengths{{0x31, 3}, {0x6E, 8}, {0x6F, 5}, {0x70, 5}, {0x71, 2},
                {0x72, 5}, {0x73, 3}, {0x74, 3}, {0x75, 2}, {0x76, 2}, {0x77, 2}, {0x78, 2}, {0x79, 2}, {0x87, 3},
                {0xAB, 4}, {0x82, 1}, {0x83, 2}, {0x84, 1}, {0x86, 5}, {0x8E, 2}, {0x8F, 2},
                {0x6C, 1}, {0x6D, 1}, {0x2E, 2}}; // 0x6C / 0x6D: the giant hand's grab and its end (0x000AC740,
                // 0x000ACBF0); 0x2E: the death curse's meter running out, with the curse's actor (0x0012E070).
            const auto found = lengths.find(id);
            if (found == lengths.end())
            {
                unbound("broadcast message " + std::to_string(id) + " has no word layout");
            }
            std::memcpy(words.data(), raw, found->second * 4);
            break;
        }
        }
        for (auto* recipient = latches; recipient != nullptr;)
        {
            auto* current = recipient;
            recipient = recipient->next;
            u32 address = 0;
            for (const auto& [node_address, latch] : active_nodes)
            {
                if (latch.get() == current)
                {
                    address = node_address;
                    break;
                }
            }
            if (address == 0)
            {
                unbound("broadcast to an active node with no scene actor");
            }
            std::array<u32, 0x42> copy = words;
            object_message(*object_of(scene.resolve_address(address, 0).data()), copy);
        }
    };
    g_script_actor_bytes = [this](u32 address) -> std::span<const u8>
    {
        if (const auto released = released_actors.find(address); released != released_actors.end())
        {
            return released->second.bytes;
        }
        auto* object = object_of(scene.resolve_address(address, 0).data());
        if (object == nullptr)
        {
            unbound("script tally actor " + std::to_string(address) + " is not a scene object");
        }
        return {object->bytes.get(), object->size};
    };
    g_script_door_pose = [this](u16 door, std::span<u32, 0x42> pose) -> const u8*
    {
        // 0x00129ED0 (DX door) over DAT_00511AA4.
        const u32 found = find_runtime_actor_by_id(door,
            [this](const entity_detail::LatchNode& node) -> u32
            {
                for (const auto& [address, latch] : active_nodes)
                {
                    if (latch.get() == &node)
                    {
                        return address;
                    }
                }
                unbound("DAT_00511AA4 node with no scene actor");
            },
            [this](u32 address)
            {
                return scene.resolve_address(address, 0x200);
            });
        if (found != 0)
        {
            addresses.clear();
            for (const auto& [bytes, actor] : actors->actors)
            {
                addresses[scene.address_of(bytes)] = actor;
            }
            if (Actor* player = find_runtime_script_player(
                    cases.next == nullptr ? 0 : address_of(cases.next), addresses)) // 0x001292D0
            {
                const Vec3 position = actor_world_position(*player); // 0x000412A0
                const Vec3 euler = actor_world_euler(*player); // 0x00041820
                std::memcpy(&pose[1], &position, 12);
                std::memcpy(&pose[4], &euler, 12);
            }
            auto* object = object_of(scene.resolve_address(found, 0).data());
            if (object == nullptr)
            {
                unbound("door actor " + std::to_string(found) + " is not a scene object");
            }
            object_message(*object, pose); // Actor vtable +0xC.
        }
        return static_cast<const u8*>(script_records);
    };
    // DAT_00506C80 and DAT_00545970 are the dialogue measurement's counts; DAT_00506C8C is the count the player
    // strategy reads.
    g_script_kill_counters = [this]()
    {
        if (measurement == nullptr)
        {
            unbound("script kill counters before the dialogue measurement exists");
        }
        return RuntimeScriptKillCounters{&measurement->second_count, &scene_kills, &measurement->first_additional};
    };
    // DAT_00511B48 nodes are player actor +0x944 (links[2]); node +0x18 is actor +0x95C.
    g_script_body_list_message = [this](std::span<const u32> message)
    {
        message_body_list(message);
    };
    g_script_ghoul_intro_caption = [this](u16 tag, float delay, float lifetime)
    {
        queue_runtime_ghoul_intro_caption(tag, delay, lifetime, ghoul_intro); // 0x000F4930
    };
    g_script_drain_actor_goals = [this](Actor& actor)
    {
        auto& player = players->player(object_of_actor(actor));
        player.refresh_views();
        drain_runtime_player_goals(player.goal_cleanup); // 0x00028210
    };
    g_script_actor_message = [this](Actor& actor, std::span<u32> message)
    {
        std::array<u32, 0x42> words{};
        std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
        object_message(object_of_actor(actor), words);
    };
    g_script_actor_by_id = [this](u16 key) -> u32
    {
        return find_runtime_actor_by_id(key,
            [this](const entity_detail::LatchNode& node) -> u32
            {
                for (const auto& [address, latch] : active_nodes)
                {
                    if (latch.get() == &node)
                    {
                        return address;
                    }
                }
                unbound("DAT_00511AA4 node with no scene actor");
            },
            [this](u32 address)
            {
                return scene.resolve_address(address, 0x200);
            }); // 0x00129ED0's walk
    };
    g_script_object_message = [this](u32 address, std::span<u32> message)
    {
        std::array<u32, 0x42> words{};
        std::copy_n(message.begin(), std::min<size_t>(message.size(), words.size()), words.begin());
        auto* object = object_of(scene.resolve_address(address, 0).data());
        if (object == nullptr)
        {
            unbound("script message to an address outside the scene");
        }
        object_message(*object, words);
        std::copy_n(words.begin(), std::min<size_t>(message.size(), words.size()), message.begin());
    };
    g_script_live_player_position = [this](Vec3& position) -> bool
    {
        if (cases.next == nullptr) // DAT_00511B40.
        {
            return false;
        }
        auto& player = players->player_at(address_of(cases.next) - 0x93C);
        player.refresh_views();
        if (player.body.empty() || body_record(player.body).energy == 0)
        {
            return false;
        }
        position = actor_world_position(player.actor); // 0x000412A0
        return true;
    };
    g_script_door_distance = [this](u16 door, const Vec3& from, float& distance) -> bool
    {
        Vec3 at;
        return runtime_door_distance(door, from, at, distance, *doors); // 0x0005BEC0
    };
    g_alt_node_body = [this](const void* node) -> std::span<const u8>
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                player->refresh_views();
                return player->body;
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_alt_node_actor = [this](const void* node) -> Actor&
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                return player->actor; // Node - 0x944.
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_alt_node_actor_bytes = [this](const void* node) -> std::span<u8>
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                return player->actor_bytes;
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_script_exclude_actor_tag = [this](u16 tag)
    {
        if (excluded_count >= i32(excluded_tags.size()))
        {
            unbound("DAT_0050E22C excluded tags past their 0x20 words");
        }
        excluded_tags[size_t(excluded_count)] = tag;
        excluded_count = excluded_count + 1;
    };
    g_alt_node_mind = [this](const void* node) -> std::span<u8>
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                return player->mind;
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_alt_node_target_eligible = [this](const void* node) -> bool
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                const u32 record = mind_record(player->mind).target_record;
                RuntimeMindTarget* target = players->bindings.scene.target_at(record);
                if (target == nullptr)
                {
                    unbound("script type 0x6B: mind +0x10 " + std::to_string(record) + " is not a target record");
                }
                return runtime_mind_target_is_eligible(player->mind, player->actor_bytes, player->actor,
                    address_of(player->actor_bytes.data()), *target, true); // 0x00029C80, EDX 1
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_alt_node_message = [this](const void* node, std::span<const u32> message)
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                std::array<u32, 0x42> words{};
                std::copy_n(message.begin(), std::min(message.size(), words.size()), words.begin());
                player->self.message(words);
                return;
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
    g_alt_node_class = [this](const void* node) -> u16
    {
        for (auto* player : players->instances())
        {
            if (&player->links[2] == node)
            {
                return player->actor.class_key; // Node - 0x93E, actor +6.
            }
        }
        unbound("DAT_00511B48 node without a native player");
    };
}
} // namespace gbtg
