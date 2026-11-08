#include "scene_actor_services_state.h"
#include "../entities/mind_fields.h"
#include <cstddef>
#include "../entities/body_fields.h"
#include "../entities/player_fields.h"

// The player and background updates, owned effects, script actor queries and actor matrix bindings.
namespace gbtg
{
// Player vtable +0x10: 0x0002C2B0 over the player's native state, with each child's slots by factory type.
void SceneActorServices::State::update_player(NativePlayerInstance& player)
{
    player.refresh_views();
    if (player.body.empty() || player.body_bits == nullptr)
    {
        unbound("player update 0x0002C2B0 without a body (actor +0x95C)");
    }
    auto& body_graph = body_messages[player.body.data()];
    if (body_graph == nullptr)
    {
        body_graph = std::make_unique<SceneBodyMessages>(*body_services, player);
    }
    const auto body_message = [this, &player](std::span<const u32> words)
    {
        std::array<u32, 0x42> buffer{};
        std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
        child_message(player.body_object, buffer.data());
    };
    RuntimeCameraMindView camera_view{player.mind, player.actor, player.body, player.body_bits, body_message};
    RuntimeCameraMindRequests camera_requests{camera_view, *camera_rig, player.body_selection, random,
        [this](const RuntimePlayerMindRequest& request)
        {
            return heap_address(&request, sizeof(RuntimePlayerMindRequest) + request.bytes.size());
        },
        [this, &player](const RuntimePlayerMindRequest& request) -> i32
        {
            auto& parent = camera_mind_parents[player.mind.data()];
            if (parent == nullptr)
            {
                parent = std::make_unique<SceneCameraMindParent>(*mind_services, player);
            }
            return parent->update(request.bytes); // 0x000295E0
        }};
    const std::span<u8> mind_view = player.mind;
    entity_detail::NavigationPathSearch* path_search = nullptr;
    if (!mind_view.empty() && mind_record(mind_view).path_search != 0)
    {
        // 0x00136700 allocates the search and stores its heap address at mind +0x1C.
        if (player.mind_storage == nullptr
            || heap_address(player.mind_storage, sizeof(RuntimeMindStorage)) != mind_record(mind_view).path_search)
        {
            unbound("mind +0x1C path search that 0x00136700 did not allocate");
        }
        path_search = player.mind_storage;
    }
    camera_requests.path_search = path_search; // Mind +0x1C for kind 0x12 (0x0007ACB0).
    const u8* floor_hint = nullptr;
    if (!mind_view.empty() && mind_record(mind_view).target_record != 0)
    {
        // Mind +0x10 is a DAT_00545988 target record; the host views are rebuilt when a record is new.
        const u32 record = mind_record(mind_view).target_record;
        if (!target_views.contains(record))
        {
            refresh_mind_targets();
        }
        if (const auto found = target_views.find(record); found != target_views.end())
        {
            floor_hint = found->second->bytes.data();
        }
        else if (const auto retired = retired_targets.find(record); retired != retired_targets.end())
        {
            floor_hint = retired->second->bytes.data();
        }
        else
        {
            unbound("mind +0x10 target record " + std::to_string(record) + " outside DAT_00545988");
        }
    }
    RuntimeBodyCollisionSphere* spheres = player.shapes.special; // Body +0x710.
    const EntityMaskHost mask{player.actor.mask_flag, player.actor.goal_ring, player.actor.mask_mode};
    RuntimeBodyCollision collision_view{player.body, player.actor, player.body_bits, spheres, mask, clock};
    RuntimePlayerRoomTracking room{player.self, player.body, player.allowed, player.current,
        player.strategy_object.bytes != nullptr};
    const RuntimeWorldBounds bounds{g_amb_volume_enabled, g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2],
        g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]};
    auto& instance = player.actor_instance;
    RuntimePlayerUpdate update{
        player.self,
        player.input,
        player_record(player.actor_bytes).input_device,
        input_activation,
        input_release,
        game.clock.frame,
        player.ring,
        address_zero,
        player.strategy_object,
        player.mind_object,
        player.body_object,
        [this, &player](FactoryObject& strategy)
        {
            switch (strategy.type)
            {
            case 0x4F:
                players->strategy_update(strategy);
                return;
            case 0x46:
                players->imp_fire_update(strategy);
                return;
            case 0x3B:
                players->butler_update(strategy);
                return;
            case 0x51:
                players->skeleton_update(strategy);
                return;
            case 0x58:
                players->zombie_update(strategy);
                return;
            case 0x52:
                players->spider_update(strategy);
                return;
            case 0x44:
                players->haunted_tv_update(strategy);
                return;
            case 0x40:
                players->haunted_chair_update(strategy);
                return;
            case 0x3C:
                players->generic_update(strategy);
                return;
            case 0x3D:
                players->ghoulie_girl_update(strategy);
                return;
            case 0x3F:
                players->grim_reaper_update(strategy);
                return;
            case 0x4C:
                players->mummy_update(strategy);
                return;
            case 0x54:
                players->vampire_update(strategy);
                return;
            case 0x57:
                players->worm_update(strategy);
                return;
            case 0x53:
                players->ugly_girl_update(strategy);
                return;
            case 0x50:
                players->ribs_update(strategy);
                return;
            case 0x3E:
                players->giant_hand_update(strategy);
                return;
            case 0x41:
                players->haunted_coat_update(strategy);
                return;
            case 0x43:
                players->haunted_picture_update(strategy);
                return;
            case 0x56:
                players->warlock_update(strategy);
                return;
            case 0x42:
                players->haunted_door_update(strategy);
                return;
            case 0x47:
                players->imp_flamethrower_update(strategy);
                return;
            case 0x3A:
                players->chicken_update(strategy);
                return;
            case 0x48:
                players->imp_flying_update(strategy);
                return;
            case 0x55:
                players->vampire_chicken_update(strategy);
                return;
            case 0x49:
                players->imp_ninja_update(strategy);
                return;
            case 0x4A:
                players->jessie_clyde_update(strategy);
                return;
            case 0x4D:
                players->mummy_cursed_update(strategy);
                return;
            case 0x4B:
                players->medusa_update(strategy);
                return;
            case 0x4E:
                players->pirate_update(strategy);
                return;
            case 0x37:
                players->baron_update(strategy);
                return;
            case 0x38:
                players->baron_plane_update(strategy);
                return;
            case 0x45:
                players->hunchback_update(strategy);
                return;
            case 0x39:
            {
                RuntimeDirectorUpdate director{strategy, player.actor, player.actor_address, player.mind, player.body,
                    [this, &player](std::span<const u32> words)
                    {
                        std::array<u32, 0x42> buffer{};
                        std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
                        child_message(player.body_object, buffer.data());
                    },
                    player.strategy_goals, player.strategy_setup, player.request_anchor,
                    players->bindings.scene.mind_request_sizes, *director_targets};
                update_runtime_director_strategy(director);
                return;
            }
            default:
                unbound("strategy vtable +0x10 of factory type " + std::to_string(strategy.type));
            }
        },
        [this, &player, &camera_requests](FactoryObject& mind, u32 input)
        {
            if (mind.type == 0x34 || mind.type == 0x35)
            {
                // Player mind 0x0007FB10 and cinema mind 0x0007CE30 over the mind's message graph.
                auto& slots = players->minds.view(mind);
                auto& graph = mind_messages[mind.bytes.get()];
                if (graph == nullptr)
                {
                    graph = std::make_unique<SceneMindMessages>(*mind_services, player, slots);
                }
                graph->prepare_tick();
                if (slots.cinema == nullptr && slots.tick == nullptr)
                {
                    unbound("player mind tick 0x0007FB10 context");
                }
                players->minds.tick(mind, input);
                return;
            }
            if (mind.type != 0x36)
            {
                unbound("mind vtable +0x10 of factory type " + std::to_string(mind.type));
            }
            // Mind +8 is the actor; its body +8 is the same actor.
            RuntimeMindTick tick{player.mind_events, player.requests, player.actor, player.actor, player.body, clock,
                [&camera_requests](RuntimePlayerMindRequest& request)
                {
                    return initialize_camera_mind_request(request, camera_requests);
                },
                [&camera_requests](RuntimePlayerMindRequest& request)
                {
                    return update_camera_mind_request(request, camera_requests);
                }};
            tick_runtime_parent_mind(tick);
        },
        [&body_graph](FactoryObject& body, u32 input) { body_graph->tick(body, input); },
        [&body_graph](FactoryObject& body) { body_graph->animate(body); },
        [&body_graph](FactoryObject& body) { body_graph->settle(body); },
        [&body_graph](FactoryObject& body) { return body_graph->supported(body); },
        [&player](FactoryObject& action, u32) -> u16
        {
            switch (action.type)
            {
            case 0x8F:
            case 0x6E: // Getter 0x00091AB0 keeps 0x00091F70 at +0x1C too.
                return runtime_walk_target_next_key(body_state_view<WalkTargetState>({action.bytes.get(), action.size}),
                    player.body_selection.animation);
            case 0x5E:
            case 0x62:
            case 0x64:
            case 0x73:
            case 0x74:
            case 0x7A:
            case 0x7C:
            case 0x7D:
            case 0x69:
            case 0x70:
            case 0x87:
            case 0x88:
            case 0x8A:
            case 0x8B:
            case 0x8E:
            case 0x66: // Getter 0x0008D930 stores 0x0002B620 at +0x1C too.
            case 0x78: // So does getter 0x00097FB0.
            case 0x5F: // And getter 0x000889B0.
            case 0x6F: // And getter 0x00093B60.
            case 0x80: // And getter 0x0009F420.
            case 0x63: // And getter 0x0008C5A0.
            case 0x7F: // And getter 0x0009F060.
            case 0x7E: // And getter 0x0009EB40.
            case 0x79: // And getter 0x0009A110.
            case 0x8D: // And getter 0x000A4CF0.
            case 0x6B: // And getter 0x00090430.
            case 0x77: // And getter 0x00097B20.
            case 0x6A: // And getter 0x0008F6F0.
            case 0x6D: // And getter 0x00090D30.
            case 0x81: // And getter 0x0009F6E0.
            case 0x76: // And getter 0x00097200.
            case 0x7B: // And getter 0x0009D8C0.
            case 0x83: // And getter 0x000A0C30.
            case 0x67: // And getter 0x0008E010.
            case 0x84: // And getter 0x000A1450.
            case 0x86: // And getter 0x000A21F0.
            case 0x89: // And getter 0x000A31A0.
            case 0x75: // And getter 0x00096A50.
            case 0x8C: // And getter 0x000A43A0.
            case 0x85: // And getter 0x000A1A10.
            case 0x90: // And getter 0x000A6A00.
            case 0x65: // And getter 0x0008D350.
            case 0x82: // And getter 0x000A03D0.
            case 0x61: // And getter 0x0008B200.
            case 0x60: // And getter 0x0008A300.
            case 0x71: // And getter 0x00095550.
            case 0x68: // And getter 0x0008E760.
            case 0x72: // And getter 0x000947A0.
                return runtime_body_action_next_key(action);
            case 0x5A:
            case 0x5C:
                // The 0x5A (0x005054E8) and 0x5C (0x00505500) class vtables have six slots; +0x1C reads the next
                // table's +4, 0x000CFFD0.
                return u16(accept_runtime_object_construction(action, {}) ? 1 : 0);
            default:
                unbound("body action vtable +0x1C of factory type " + std::to_string(action.type));
            }
        },
        path_search,
        floor_hint,
        collision_view,
        player.actor_bytes,
        player.body_bits,
        player.collision,
        player.body_selection,
        [this, &player](Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point, GhoulySpawnPool* contacts,
                        const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word) -> u32
        {
            RuntimePlayerContactReport report{player.body, player.actor_bytes, contact_totals, contact_session,
                [this, &player](void* payload)
                {
                    auto& report = *static_cast<RuntimeContactReport*>(payload);
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
                }};
            return report_runtime_player_contact(actor, object, point, contacts, velocity, word, report);
        },
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        { return segment_contact_actor(probe); },
        entity_detail::g_world.body_a,
        instance.animation,
        instance.pose,
        instance.startup.attachments,
        instance.pending,
        instance.frame.updates,
        instance.shape,
        *frame,
        clock,
        player.navigation,
        room,
        bounds};
    update_runtime_player(update);
}

// The background constructor's scene services. Its model views (the animation block and header, the
// collision vertices, matrix-array bindings and writable nodes) are read once the actor base has loaded the
// model.
std::unique_ptr<RuntimeBackgroundConstructionServices> SceneActorServices::State::background_services(
    NativeBackgroundInstance& background, NativeActorFactoryInstance& view, RuntimeBackgroundAnimation*& first_animation)
{
    Actor& actor = background.state.actor;
    auto& views = background_views[background.bytes.data()];
    views = std::make_unique<SceneBackgroundViews>();
    views->resolve = [this, &actor](u32 address, size_t size) -> std::span<const u8>
    {
        try
        {
            if (address < 0x10000000u)
            {
                if (actor.model == nullptr || address > actor.model->cpu.size())
                {
                    throw Error("model offset lies outside the background model");
                }
                const auto tail = actor.model->cpu.subspan(address);
                if (size > tail.size())
                {
                    throw Error("model range exceeds the background model");
                }
                return size == 0 ? tail : tail.first(size);
            }
            if (address >= 0x40000000u)
            {
                return size == 0 ? resources.memory.tail(address) : resources.memory.resolve(address, size);
            }
            return size == 0 ? scene.resolve_tail(address) : scene.resolve_address(address, size);
        }
        catch (const Error& error)
        {
            char text[24];
            std::snprintf(text, sizeof(text), "0x%08X", address);
            throw Error(std::string("background address ") + text + ": " + error.what());
        }
    };
    views->source = [resolve = views->resolve](u32 address)
    {
        return resolve(address, 0);
    };
    std::span<const Vec3> vertices;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        background.animation.model_reference = &actor.model->cpu;
        const u32 block = read_u32(cpu, 0x10);
        background.animation.animation_block = cpu.subspan(block);
        const u32 header = read_u32(cpu, block + 0x10);
        background.animation.header = header == 0 ? std::span<const u8>{} : cpu.subspan(header);
        if (actor.model->physics_shapes.data() != nullptr)
        {
            vertices = resources.collision_vertices.at(actor.model->physics_shapes.data());
        }
        if (actor.model->runtime != nullptr)
        {
            std::vector<RuntimeModelNode*> pending(actor.model->runtime->source->tree->roots.begin(),
                actor.model->runtime->source->tree->roots.end());
            while (!pending.empty())
            {
                RuntimeModelNode* node = pending.back();
                pending.pop_back();
                for (; node != nullptr; node = node->sibling)
                {
                    views->nodes[node] = std::span<u8>(const_cast<u8*>(node->bytes.data()), node->bytes.size());
                    if (node->type_name != nullptr && std::strncmp(node->type_name, "ndMtxArray", 11) == 0 &&
                        i32(read_u32(node->bytes, 0x24)) > 0)
                    {
                        views->matrix_bindings[node->bytes.data()] = views->resolve(read_u32(node->bytes, 0x20),
                            size_t(read_u32(node->bytes, 0x24)) * 8);
                    }
                    if (node->child != nullptr)
                    {
                        pending.push_back(node->child);
                    }
                }
            }
        }
    }
    return std::make_unique<RuntimeBackgroundConstructionServices>(RuntimeBackgroundConstructionServices{room,
        entity_detail::g_world.body_a, background_regions, background_region_count, vertices, resources.shape_lists,
        views->matrix_bindings, resources.copy_types, sectors, view.live_attachments, global_attachments,
        view.anchors, origin, first_animation, views->source, background_allocation, background_address, *doors,
        background_ambient, random, resources.registry, resources.error, disable_framebuffer_clear, views->nodes,
        matrix_boundaries});
}

std::vector<BackgroundListEntry> SceneActorServices::State::background_list()
{
    // DAT_00511B20 and each node's +4.
    std::vector<BackgroundListEntry> entries;
    for (const void* node = backgrounds->bindings.backgrounds.next; node != nullptr;
        node = static_cast<const entity_detail::ListHead*>(node)->next)
    {
        auto* background = backgrounds->owner_of(*static_cast<const entity_detail::ListHead*>(node));
        if (background == nullptr)
        {
            throw Error("+0x934 list node is not a native background");
        }
        entries.push_back({scene.address_of(background->bytes.data()), &background->state.actor});
    }
    return entries;
}

NativeBackgroundInstance& SceneActorServices::State::background_at(u32 address)
{
    if (address == 0)
    {
        throw Error("background read through address zero");
    }
    auto* background = backgrounds->find(scene.resolve_address(address, 0).data());
    if (background == nullptr)
    {
        throw Error("address is not a native background");
    }
    return *background;
}

// Message slot 0x00043E50 with this background's part state (actor +0x940..+0x958) and the scene's tables.
void SceneActorServices::State::background_message(NativeBackgroundInstance& background, std::span<u32> message)
{
    with_background_context(background, [&](Actor& actor, BackgroundMessageContext& context)
    {
        handle_background_message(actor, message, context);
    });
}

void SceneActorServices::State::with_background_context(NativeBackgroundInstance& background,
    const std::function<void(Actor&, BackgroundMessageContext&)>& use)
{
    FactoryObject* object = object_of(background.bytes.data());
    if (object == nullptr)
    {
        throw Error("background message has no owning factory object");
    }
    auto& view = actors->view(*object);
    Actor& actor = background.state.actor;
    auto& base = base_of(*object);
    auto& views = *background_views.at(background.bytes.data());
    const auto field = [&](size_t offset) -> i32&
    {
        return *reinterpret_cast<i32*>(background.bytes.data() + offset);
    };
    RuntimeActorMatrixSampling sampling{actor_matrix_records(background.matrices), background.matrices.matrices.node,
        std::span<float, 12>(sampling_scratch.data(), 12),
        sampling_active, origin, views.source};
    BackgroundPartDispatch dispatch{part_settings, part_setting_count, part_sound_slot,
        std::span<const u8, 5>(data(0x002FD200, 5).data(), 5), sounds, effects};
    const std::function<const Actor&(const entity_detail::SegmentProbe&)> probe_actor =
        [this](const entity_detail::SegmentProbe& probe) -> const Actor&
    {
        return actors->view(probe).bat.actor;
    };
    BackgroundPartRemoval removal{background.state.volumes, background.animation.header,
        entity_detail::g_world.segment, probe_actor, resources.shape_lists, view.startup.resource.resource.data(),
        base.callouts};
    auto* first = backgrounds->first_background();
    if (first == nullptr)
    {
        throw Error("background message has no first background on the +0x934 list");
    }
    RuntimeBackgroundGraphPush push{first->state.actor, first->animation, clock, random,
        background_views.at(first->bytes.data())->resolve};
    const std::span<RuntimePointGraph> graphs = background.lifetime.animation == nullptr ?
        std::span<RuntimePointGraph>{} : std::span<RuntimePointGraph>(background.state.graphs);
    BackgroundPartDestruction parts{background.lifetime.effect, field(0x944), field(0x948), graphs, field(0x954),
        {reinterpret_cast<char*>(background.bytes.data() + 0x958), 0x80}, background.matrices.matrices.count,
        view.startup.models.secondary.get(), node_cache, sampling, dispatch, removal, push, background_address, pal,
        pal_override};
    const std::function<void(std::span<u32>)> base_message = [&](std::span<u32> words)
    {
        dispatch_base_actor_word_message(base.base, words, word_messages);
    };
    const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
    {
        if (address == 0)
        {
            throw Error("background message reads an actor through address zero");
        }
        char text[96];
        std::snprintf(text, sizeof(text), "background message reads 0x%08X, which is no scene actor", address);
        const u8* bytes = nullptr;
        try
        {
            bytes = scene.resolve_tail(address).data();
        }
        catch (const Error&)
        {
            throw Error(text);
        }
        const auto found = actors->actors.find(bytes);
        if (found == actors->actors.end())
        {
            throw Error(text);
        }
        return found->second;
    };
    const std::function<void(Actor&, const void*)> reply = [this](Actor& receiver, const void* payload)
    {
        // The background's only actor message is the rumble 0x50: its +4 is the cue name's retail address.
        const auto& rumble = *static_cast<const BackgroundRumbleMessage*>(payload);
        if (rumble.id != 0x50)
        {
            unbound("background actor message " + std::to_string(rumble.id));
        }
        std::array<u32, 0x42> words{};
        words[0] = rumble.id;
        words[1] = address_of(rumble.name);
        actor_message(receiver, words.data());
    };
    const std::function<std::span<const u8>(u32)> body_of = [this](u32 address) -> std::span<const u8>
    {
        auto* player = players->find_player_at(address);
        if (player == nullptr)
        {
            unbound("background message reads +0x95C of an actor that is not a player");
        }
        player->refresh_views();
        return player->body;
    };
    BackgroundMessageContext context{base.base, base_message, parts, address_zero, *score, resources.text, part_text,
        wide_scratch.data(), broken_parts, reinterpret_cast<const char*>(data(0x00421E58, 0x2D).data()),
        entity_detail::g_world.anchor, *doors, actor_at, views.resolve, reply, body_of};
    use(actor, context);
}

// Native class +0x10 adapter; the translated slot is 0x000464D0.
void SceneActorServices::State::background_update(NativeBackgroundInstance& background)
{
    auto* first = backgrounds->first_background();
    auto& views = *background_views.at((background.lifetime.animation == nullptr ? &background : first)->bytes.data());
    if (background.lifetime.animation == nullptr && background.lifetime.effect != nullptr)
    {
        if (effect_pass_scratch == nullptr)
        {
            throw Error("background particle update requires the caller's incoming effect scratch bytes");
        }
        if (display_view == nullptr)
        {
            throw Error("background particle collision has no DAT_005077A8 display view");
        }
        RuntimeEffectDebris debris{{debris_pool->particles, kEffectDebrisCount}, debris_pool->types,
            debris_pool->free_head, game.clock.frame, random};
        const Vec3 plane = effect_collision_plane();
        const Vec3 minimum{g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2]};
        const Vec3 maximum{g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]};
        RuntimeEffectCollisionContext movement{effect_collision, clock, *display_view, plane,
            g_amb_volume_enabled, minimum, maximum, random};
        RuntimeEffectCollisionCommands commands{effect_collision, debris, voices, effects, *footsteps,
            effect_callback_result};
        const std::function<std::span<const u8>(u32)> resolve = [this](u32 address)
        {
            return std::span<const u8>{resources.memory.tail(address)};
        };
        const std::function<u32(const u8*)> address = [this, &background](const u8* pointer)
        {
            auto& effect = *background.lifetime.effect;
            return pointer == effect.bytes.data() ? heap_address(pointer, effect.bytes.size())
                : resources.memory.address_of(pointer);
        };
        auto& effect = *background.lifetime.effect;
        const u32 script = resources.memory.address_of(effect.script.data());
        std::memcpy(effect.bytes.data() + 0x134, &script, 4);
        RuntimeEffectSimulation simulation{movement, commands, resolve, pal, pal_override};
        const Vec3 direction = display_view->forward;
        RuntimeEffectTickContext tick{simulation, game.clock.frame, effect_sound_frame, address, direction,
            *effect_pass_scratch};
        RuntimeBackgroundUpdate context{first == nullptr ? nullptr : &first->state, clock, views.resolve,
            &tick, matrix_boundaries};
        update_runtime_background(background.state, context);
        return;
    }
    RuntimeBackgroundUpdate context{first == nullptr ? nullptr : &first->state, clock, views.resolve,
        nullptr, matrix_boundaries};
    update_runtime_background(background.state, context);
}

void SceneActorServices::State::tick_owned_effect(ActorEffectNode& effect, float step)
{
    if (effect_pass_scratch == nullptr)
    {
        throw Error("owned effect update requires the caller's incoming effect scratch bytes");
    }
    if (display_view == nullptr)
    {
        throw Error("owned effect collision has no DAT_005077A8 display view");
    }
    RuntimeEffectDebris debris{{debris_pool->particles, kEffectDebrisCount}, debris_pool->types,
        debris_pool->free_head, game.clock.frame, random};
    const Vec3 plane = effect_collision_plane();
    const Vec3 minimum{g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2]};
    const Vec3 maximum{g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]};
    RuntimeEffectCollisionContext movement{effect_collision, clock, *display_view, plane,
        g_amb_volume_enabled, minimum, maximum, random};
    RuntimeEffectCollisionCommands commands{effect_collision, debris, voices, effects, *footsteps,
        effect_callback_result};
    const std::function<std::span<const u8>(u32)> resolve = [this](u32 address)
    {
        return std::span<const u8>{resources.memory.tail(address)};
    };
    const std::function<u32(const u8*)> address = [this, &effect](const u8* pointer)
    {
        return pointer == effect.bytes.data() ? heap_address(pointer, effect.bytes.size())
            : resources.memory.address_of(pointer);
    };
    const u32 script = effect.script.data() == nullptr ? 0 : resources.memory.address_of(effect.script.data());
    std::memcpy(effect.bytes.data() + 0x134, &script, 4);
    RuntimeEffectSimulation simulation{movement, commands, resolve, pal, pal_override};
    const Vec3 direction = display_view->forward;
    RuntimeEffectTickContext tick{simulation, game.clock.frame, effect_sound_frame, address, direction,
        *effect_pass_scratch};
    tick_actor_effect(effect, step, tick); // 0x000EF2D0
}

std::vector<RuntimeScriptActorQueryView> SceneActorServices::State::case_actors()
{
    case_actor_views.clear();
    std::vector<RuntimeScriptActorQueryView> views;
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.case_head.next); node != nullptr;
        node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (&player->links[1] == node)
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B40 node without a native player");
        }
        owner->refresh_views();
        if (owner->body.empty() || owner->body_bits == nullptr)
        {
            unbound("DAT_00511B40 player without a body (actor +0x95C)");
        }
        auto view = std::make_unique<SceneCaseActor>();
        const auto body = std::span<const u8>(owner->body);
        auto& mind = view->mind;
        mind.state = i16(body_record(body).move_holds);
        mind.state_time = body_record(body).move_hold_time;
        const u32 case_actor = body_record(body).attached;
        mind.case_actor = case_actor == 0 ? nullptr : actors->actors.at(scene.resolve_address(case_actor, 0).data());
        mind.node = nullptr;
        mind.blocked = body_record(body).unk_69;
        mind.active = body_record(body).unk_6A;
        mind.suspended = body_record(body).unk_6B;
        mind.permitted = body_record(body).unk_6C;
        mind.timer = body_record(body).unk_70;
        mind.energy = i16(body_record(body).energy);
        mind.maximum_energy = i16(body_record(body).max_energy);
        mind.bits = owner->body_bits;
        view->tag = read_u16(owner->actor_bytes, 6);
        void* marker = player_record(owner->actor_bytes).live != 0
                           ? static_cast<void*>(owner->actor_bytes.data() + offsetof(PlayerRecord, live))
                           : nullptr;
        views.push_back(RuntimeScriptActorQueryView{owner->actor, view->tag, &view->mind, nullptr, marker});
        case_actor_views.push_back(std::move(view));
    }
    return views;
}

Actor* SceneActorServices::State::actor_or_retired(u32 address)
{
    if (const auto retired = retired_actors.find(address); retired != retired_actors.end())
    {
        return retired->second.get();
    }
    return actors->actors.at(scene.resolve_address(address, 0).data());
}

Actor* SceneActorServices::State::actor_at_address(u32 address)
{
    return address == 0 ? nullptr : actors->actors.at(scene.resolve_address(address, 0).data());
}

std::vector<RuntimeScriptActorQueryView> SceneActorServices::State::marked_actor_views()
{
    std::vector<RuntimeScriptActorQueryView> views;
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.mark.next); node != nullptr;
        node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (&player->links[0] == node)
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B38 node without a native player");
        }
        owner->refresh_views();
        if (owner->body.empty() || owner->body_bits == nullptr)
        {
            unbound("DAT_00511B38 player without a body (actor +0x95C)");
        }
        auto& view = marked_actor_query_views[owner];
        if (view == nullptr)
        {
            view = std::make_unique<SceneMarkedActor>();
        }
        const auto body = std::span<const u8>(owner->body);
        auto& mind = view->mind;
        mind.state = i16(body_record(body).move_holds);
        mind.state_time = body_record(body).move_hold_time;
        mind.case_actor = actor_at_address(body_record(body).attached);
        // Body +0x5C (the attachment request tables) and its +0x2C, read for flag 0x100000.
        mind.node = nullptr;
        if (owner->animation_resource.data() != nullptr)
        {
            if (owner->animation_resource.size() < 0x30)
            {
                unbound("DAT_00511B38 player body +0x5C without its +0x2C word");
            }
            view->node.active = read_u32(owner->animation_resource, 0x2C) == 0 ? nullptr
                : const_cast<u8*>(owner->animation_resource.data() + 0x2C);
            mind.node = &view->node;
        }
        mind.blocked = body_record(body).unk_69;
        mind.active = body_record(body).unk_6A;
        mind.suspended = body_record(body).unk_6B;
        mind.permitted = body_record(body).unk_6C;
        mind.timer = body_record(body).unk_70;
        mind.energy = i16(body_record(body).energy);
        mind.maximum_energy = i16(body_record(body).max_energy);
        mind.bits = owner->body_bits;
        view->tag = read_u16(owner->actor_bytes, 6);
        // Actor +0x958 (the mind): +0xC the reference source (0x00028C10), +0x10 the target record whose +0x10
        // is its actor (flag 0x400).
        RuntimeScriptActorQueryReference* reference = nullptr;
        if (player_record(owner->actor_bytes).mind != 0)
        {
            if (owner->mind.size() < 0x14)
            {
                unbound("DAT_00511B38 player mind (actor +0x958) without its +0x10 word");
            }
            const u32 source = mind_record(owner->mind).target;
            view->reference.source = source == 0 ? nullptr : actor_or_retired(source);
            view->reference.binding = nullptr;
            if (const u32 record = mind_record(owner->mind).target_record; record != 0)
            {
                refresh_mind_targets();
                if (const auto found = target_records.find(record); found != target_records.end())
                {
                    view->binding.owner = &found->second->actor;
                }
                else if (const auto retired = retired_targets.find(record); retired != retired_targets.end())
                {
                    view->binding.owner = &retired->second->actor; // A freed record's +0x10, never a live requester.
                }
                else
                {
                    unbound("DAT_00511B38 player mind +0x10 " + std::to_string(record) + " is not a target record");
                }
                view->reference.binding = &view->binding;
            }
            reference = &view->reference;
        }
        void* marker = player_record(owner->actor_bytes).live != 0
                           ? static_cast<void*>(owner->actor_bytes.data() + offsetof(PlayerRecord, live))
                           : nullptr;
        views.push_back(RuntimeScriptActorQueryView{owner->actor, view->tag, &view->mind, reference, marker});
    }
    return views;
}

const ActorMatrixBindings& SceneActorServices::State::actor_matrix_bindings(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        static const ActorMatrixBindings none;
        return none;
    }
    auto& bindings = model_matrix_bindings[actor.model->cpu.data()];
    if (bindings != nullptr)
    {
        return *bindings;
    }
    bindings = std::make_unique<ActorMatrixBindings>();
    if (actor.model->runtime == nullptr)
    {
        return *bindings;
    }
    const auto cpu = actor.model->cpu;
    const auto resolve = [this, cpu](u32 address, size_t size) -> std::span<const u8>
    {
        if (address < 0x10000000u)
        {
            if (address > cpu.size() || size > cpu.size() - address)
            {
                throw Error("matrix array bindings lie outside the actor model");
            }
            return cpu.subspan(address, size);
        }
        if (address >= 0x40000000u)
        {
            return resources.memory.resolve(address, size);
        }
        return scene.resolve_address(address, size);
    };
    std::vector<RuntimeModelNode*> pending(actor.model->runtime->source->tree->roots.begin(),
        actor.model->runtime->source->tree->roots.end());
    while (!pending.empty())
    {
        RuntimeModelNode* node = pending.back();
        pending.pop_back();
        for (; node != nullptr; node = node->sibling)
        {
            if (node->type_name != nullptr && std::strncmp(node->type_name, "ndMtxArray", 11) == 0 &&
                i32(read_u32(node->bytes, 0x24)) > 0)
            {
                (*bindings)[node->bytes.data()] = resolve(read_u32(node->bytes, 0x20),
                    size_t(read_u32(node->bytes, 0x24)) * 8);
            }
            if (node->child != nullptr)
            {
                pending.push_back(node->child);
            }
        }
    }
    return *bindings;
}

void SceneActorServices::State::update_actor(u32 address)
{
    const u8* bytes = scene.resolve_address(address, 0).data();
    auto* object = object_of(bytes);
    auto& view = actors->view(*object);
    auto found = actor_updates.find(&view.bat.actor);
    if (found == actor_updates.end() && (object->type == 2 || object->type == 0x16))
    {
        // Vtable +0x10 is the class's slot (getter 0x0003C080 keeps 0x000420D0), so a base actor updates even
        // before its first message, as a star bound under its owner does (0x0008CEF0).
        const u8* object_bytes = object->bytes.get();
        found = actor_updates.emplace(&view.bat.actor, [this, object_bytes](Actor&)
        {
            update_base_actor(*object_of(object_bytes));
        }).first;
    }
    if (found == actor_updates.end() && object->type == 0x17)
    {
        // Vtable +0x10 is the furnace's 0x0006E260.
        const u8* object_bytes = object->bytes.get();
        found = actor_updates.emplace(&view.bat.actor, [this, object_bytes](Actor&)
        {
            update_furnace(*object_of(object_bytes));
        }).first;
    }
    if (found == actor_updates.end())
    {
        throw Error("actor update slot is not translated for factory type " + std::to_string(object->type));
    }
    found->second(view.bat.actor);
}

// Scene startup ahead of the markers (0x0011CBD0): the door registration pool, the requested background
// (0x0011C8B0) and its activation (0x00123070), the navigation build (0x00111820) and the mirrors (0x0011F450).
void SceneActorServices::State::start_background()
{
    reset_runtime_screen_effects(screen_tint, pulse, screen_lines); // 0x00060C80
    // 0x0011CBD0's 0x000EC2D0 and 0x000EBB80: the debris and trail pools, which 0x000ED450 releases at each scene's
    // end (0x0011C5A0).
    const std::function<u32(const u8*)> effect_address = [this](const u8* pointer)
    {
        return resources.memory.address_of(pointer);
    };
    initialize_effect_debris(*debris_pool, resources.registry, resources.error, effect_address);
    initialize_effect_trails(*trail_pool, resources.registry, resources.error, effect_address);
    // 0x0011CBD0 after its marker load: the actor sound group capacities (DAT_00512520, DAT_00512534).
    sounds.groups[0].capacity = 0x14;
    sounds.groups[1].capacity = 6;
    // DAT_004EE2B8: 0x560 bytes, stride 0x154 and growth 4, so four rows after the header.
    door_pool = static_cast<entity_detail::NavigationCellRegistration*>(
        engine_heap_allocate(i32(sizeof(entity_detail::NavigationCellRegistration) + 4 * 0x154)));
    u8* rows = reinterpret_cast<u8*>(door_pool + 1);
    door_pool->begin = rows;
    door_pool->end = rows;
    door_pool->capacity = rows + 4 * 0x154;
    door_pool->stride = 0x154;
    door_pool->growth = 4;
    create_runtime_background_cloth_pools(background_cloth_scene()); // 0x00114180
    auto& requests = g_scene_object_requests;
    u32 head;
    std::memcpy(&head, requests.background.data(), 4);
    if (head == 0x5F646961)
    {
        const std::string name(requests.background.data());
        try
        {
            FactoryObject object = create_scene_background(scene.factory, name.c_str(), empty_name);
            if (object.bytes == nullptr)
            {
                throw Error("the factory created no object");
            }
            scene.keep_object(std::move(object));
        }
        catch (const std::exception& error)
        {
            throw Error("scene background " + name + ": " + error.what());
        }
    }
    requests.background[0] = 0;
    activate_actors();
    // 0x00111820 reads the actor of the first +0x934 node (DAT_00511B20).
    const Actor* background = nullptr;
    if (auto* first = backgrounds->first_background(); first != nullptr)
    {
        background = &first->state.actor;
    }
    else if (entity_detail::g_world.body_a.next != nullptr)
    {
        throw Error("the navigation build's first +0x934 node is not a background");
    }
    build_scene_navigation(background, {reinterpret_cast<const float*>(data(0x002FCFB8, 0x400).data()), 0x100},
        navigation, navigation_nodes);
    // 0x0011CBD0 runs 0x001153A0, 0x00123560, 0x00119990, then 0x0011F450.
    build_runtime_background_cloths(background_cloth_scene()); // 0x001153A0
    reset_runtime_scene_point_lights(background != nullptr && background->model != nullptr ? background->model->cpu
        : std::span<const u8>{}, point_lights); // 0x00123560
    // The first +0x934 node's light grid: **(its +0x204 +0x10), 0 when the node or its model is missing. 0x00119990
    // and 0x0011CBD0 (DAT_005462EC, the decals' grid) both read it.
    const auto first_model_word = [background]() -> u32
    {
        return background == nullptr || background->model == nullptr ? 0u
            : runtime_light_grid_offset(background->model->cpu);
    };
    // 0x00119990: no blob shadow is active (DAT_0050EC24, stride 0x18), the count and the next free are 0,
    // DAT_0050EF18 is the model word and DAT_0050EF1C 0.
    for (auto& entry : blob_shadow_entries)
    {
        entry.active = 0;
    }
    blob_shadow_count = 0; // DAT_0050EF10
    blob_shadow_free = 0; // DAT_0050EF14
    blob_shadow_enable = first_model_word(); // DAT_0050EF18
    blob_shadow_suppress = 0; // DAT_0050EF1C
    setup_runtime_scene_mirrors(background != nullptr && background->model != nullptr ? background->model->cpu
        : std::span<const u8>{}, mirrors); // 0x0011F450
    // 0x0011CBD0 after 0x0011F450: the actor snapshots' decal gate is the same model word, and none is counted.
    decal_enable = first_model_word(); // DAT_005462EC
    actor_snapshot_count = 0; // DAT_005462F0
    // 0x0011CBD0 then: the background's refraction mesh (0x0011B7A0) with its noise (0x0011B8F0) and quad
    // (0x0011B7F0) from no drift, and DAT_00510650 cleared.
    g_scene_refraction.model = background != nullptr && background->model != nullptr ? background->model->cpu
        : std::span<const u8>{};
    g_scene_refraction.mesh = find_runtime_refraction_mesh(g_scene_refraction.model); // DAT_00510580
    if (g_scene_refraction.mesh != nullptr)
    {
        create_runtime_refraction_noise(g_scene_refraction,
            [this](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)
            {
                return snapshot_d3d.create_texture(width, height, levels, usage, format, pool); // D3D 0x00177840
            },
            [this](u32 texture)
            {
                return resources.memory.tail(resources.d3d.resources.at(texture).data); // D3DTexture_LockRect
            },
            g_crt_random_seed);
        g_scene_refraction.drift_u = 0.f; // DAT_00510648
        g_scene_refraction.drift_v = 0.f; // DAT_0051064C
        build_runtime_refraction_quad(g_scene_refraction, hud_size[0], hud_size[1]); // DAT_005096E8, DAT_005096EA
    }
    g_flag_0650 = 0; // DAT_00510650
    // 0x00112D70 (after 0x0011F450 and the queue builds): the first background's light grid and parts.
    light_grid = {};
    light_grid_background = nullptr;
    if (auto* first = backgrounds->first_background(); first != nullptr && first->state.actor.model != nullptr)
    {
        light_grid.background = true;
        light_grid_background = first;
        light_grid.model = first->state.actor.model->cpu;
        // The serialized header keeps offsets; the relocated original (the model instance's source) gives the cells'
        // vertex buffer headers their retail addresses.
        const auto* runtime = first->state.actor.model->runtime;
        if (runtime == nullptr || runtime->source == nullptr)
        {
            unbound("light grid of a background without a model instance");
        }
        light_grid.model_address = resources.memory.address_of(runtime->source->cpu.data());
        if (first->state.lifetime.animation != nullptr)
        {
            light_grid.parts = {first->state.lifetime.animation, first->state.graphs.size() * 0x1E0};
        }
    }
    build_runtime_scene_light_grid(light_grid, light_grid_d3d); // 0x00112D70
}
} // namespace gbtg
