#include "entities/trigger_actor.h"
#include "entities/cinema_mind_lifecycle.h"
#include "entities/cinema_mind_requests.h"
#include "entities/cinema_mind_control.h"
#include "entities/body_orientation.h"
#include "entities/mind_target_policy.h"
#include "navigation/mind_escape_position.h"
#include "navigation/grid_preparation.h"
#include "navigation/model_grid.h"
#include "navigation/cell_registration.h"
#include "entities/entity_heap.h"
#include "scene/ambient.h"
#include "scene/background_geometry.h"
#include "scene/background_ambient.h"
#include "entities/actor_matrix_setup.h"
#include "entities/actor_matrix_sampling.h"
#include "graphics/model_points.h"
#include "graphics/matrix_inverse.h"
#include "scene/background_sectors.h"
#include "graphics/point_graph.h"
#include "scene/background_lifetime.h"
#include "scene/background_animation.h"
#include "cutscenes/door_registration.h"
#include "graphics/pose_matrix.h"
#include "graphics/effect_particles.h"
#include "graphics/actor_effect_stream.h"
#include "runtime/aid_list_lookup.h"
#include "scene/background_construction.h"
#include "scene/background_parts.h"
#include "entities/runtime_list.h"
#include "entities/physical_shape_events.h"
#include "navigation/grid_regions.h"
#include "entities/ghouly_spawn_record.h"
#include "navigation/mind_navigation_angles.h"
#include "entities/body_damage_motion.h"
#include "entities/cinema_mind_request_update.h"
#include "entities/actor_collision_culling.h"
#include "entities/actor_shape_push.h"
#include "graphics/vector_normalize.h"
#include "scripts/script_conditions.h"
#include "entities/model_sphere_collision.h"
#include "entities/collision_shape_dispatch.h"
#include "entities/physical_mesh_queries.h"
#include "entities/mesh_sphere_geometry.h"
#include <cstring>
#include <bit>
#include <iostream>

using namespace gbtg;

int main()
{
    u32 count;
    std::cin >> count;
    for (u32 index = 0; index < count; ++index)
    {
        u32 mode, random, slot_word;
        std::cin >> mode >> random >> slot_word;
        std::array<u8, 0x1000> parameters;
        FactoryObject object;
        object.size = 0xC6C;
        object.bytes = std::make_unique<u8[]>(object.size);
        for (u8& byte : parameters)
        {
            u32 value;
            std::cin >> value;
            byte = u8(value);
        }
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            u32 value;
            std::cin >> value;
            object.bytes[offset] = u8(value);
        }
        i32 slot = i32(slot_word);
        bool result;
        std::vector<u8> graph_bytes;
        if (mode == 0)
        {
            result = initialize_runtime_area_actor(object, parameters);
        }
        else if (mode == 1)
        {
            Actor actor;
            GhoulyController controller;
            GhoulySpawnRecord* load = nullptr;
            const i32 blocked = -1;
            RuntimeTriggerActorContext context{{object.bytes.get(), object.size}, actor,
                controller, random, 0.f, {}, load, blocked, {}};
            result = initialize_runtime_trigger_actor(context, parameters);
        }
        else if (mode == 2)
        {
            result = find_runtime_trigger_spawn_slot({object.bytes.get(), object.size}, slot);
        }
        else if (mode == 3)
        {
            std::span<const u8> resource;
            AnimationResourceRegistry registry;
            ResourceErrorState error{};
            RuntimeCinemaMindLifecycle context{{object.bytes.get(), object.size}, parameters, resource,
                registry, error, random, [](const void*) -> u32
                {
                    throw Error("cinema-mind fixture unexpectedly acquired a resource");
                }};
            result = initialize_runtime_cinema_mind(context, parameters);
        }
        else if (mode == 4)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler = {read_f32(parameters, 0x14), read_f32(parameters, 0x18), read_f32(parameters, 0x1C)};
            const auto target = read_u32(parameters, 0) == 0 ? std::span<const u8>{} :
                std::span<const u8>{parameters}.subspan(0x400);
            result = initialize_runtime_cinema_mind_request(actor, target,
                {object.bytes.get(), object.size}, random) != 0;
        }
        else if (mode == 5)
        {
            refresh_runtime_cinema_mind_action_permission({object.bytes.get(), object.size}, parameters,
                i32(read_u32(parameters, 0)), std::span<const u8>{parameters}.subspan(0x400));
            result = true;
        }
        else if (mode == 6)
        {
            Actor actor;
            actor.animation_rate = read_f32(parameters, 0x92C);
            actor.animation_pause = read_f32(parameters, 0x930);
            const float freeze = read_f32(parameters, 8);
            const float dt = read_f32(parameters, 0xC);
            const float scale = read_f32(parameters, 0x10);
            const u32 zero = 0;
            RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            RuntimeActorSoundQueues sounds{};
            update_runtime_cinema_mind_sound({object.bytes.get(), object.size},
                std::span<const u8>{parameters}.subspan(0x100),
                std::span<const u8>{parameters}.subspan(0x400), actor, clock, random, sounds);
            result = true;
        }
        else if (mode == 7 || mode == 8)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            if (mode == 7)
            {
                const Vec3 point{read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
                face_runtime_body_point({object.bytes.get(), object.size}, actor, point);
                result = true;
            }
            else
            {
                result = face_runtime_cinema_mind_target(std::span<const u8>{parameters}.subspan(0x600),
                    {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400)) != 0;
            }
        }
        else if (mode == 9)
        {
            Actor actor, target_actor;
            RuntimeScriptMindQuery query{};
            RuntimeMindTarget target{nullptr, std::span<const u8>{parameters}.subspan(0x600),
                0x10002600, target_actor, parameters, query, nullptr};
            result = runtime_mind_target_is_eligible({object.bytes.get(), object.size}, parameters,
                actor, 0x10002000, target, read_u32(parameters, 0) != 0);
        }
        else if (mode == 10)
        {
            std::array<RuntimeInputAxisWriteDevice, 4> children;
            for (size_t index = 0; index < children.size(); ++index)
            {
                children[index].record = {object.bytes.get() + 0x100 + index * 0x80, 0x80};
            }
            RuntimeInputAxisWriteDevice device{{object.bytes.get(), 0x80},
                {&children[0], &children[1], &children[2], &children[3]}};
            set_runtime_input_vertical_axes(device,
                {read_f32(parameters, 0), read_f32(parameters, 4)}, {object.bytes.get() + 0x800, 0x80});
            result = true;
        }
        else if (mode == 42)
        {
            const u32 subtype = read_u32(parameters, 0);
            const u32 mask = read_u32(parameters, 4);
            if (subtype < 2)
            {
                if (subtype == 0)
                {
                    set_physical_shape_event_bits({object.bytes.get(), 0x14}, mask);
                }
                else
                {
                    clear_physical_shape_event_bits({object.bytes.get(), 0x14}, mask);
                }
            }
            else
            {
                std::unordered_map<const u8*, PhysicalShapeEventList> lists;
                for (size_t index = 0; index < 4; ++index)
                {
                    const size_t shape = (index < 2 ? 0x6B0 : 0x790) + (index % 2) * 0x30;
                    const auto bytes = std::span<const u8>(object.bytes.get(), object.size);
                    lists.emplace(object.bytes.get() + shape + 0x10,
                        PhysicalShapeEventList{read_u32(bytes, shape + 0x10),
                            object.bytes.get() + read_u32(bytes, shape + 0x14) - 0x10000000u});
                }
                const std::function<std::span<const u8>(u32)> resolve = [&](u32 address)
                {
                    return std::span<const u8>(object.bytes.get(), object.size).subspan(address - 0x10000000u);
                };
                PhysicalShapeEventContext context{0, 0, 0,
                    [mask, &parameters](const u8* event, void*)
                    {
                        const std::span<u8> row{const_cast<u8*>(event), 0x14};
                        return read_u32(parameters, 0xC) != 0 ? set_physical_shape_event_bits(row, mask) : clear_physical_shape_event_bits(row, mask);
                    }, nullptr, lists};
                visit_scene_script_shape_events(read_u32(parameters, 0x10), u8(read_u32(parameters, 8)), resolve, context);
            }
            result = true;
        }
        else if (mode == 41)
        {
            if (read_u32(parameters, 0) == 0)
            {
                std::array<entity_detail::NavGrid, 4> grids;
                entity_detail::g_nav_count = std::bit_cast<i32>(read_u32(parameters, 4));
                for (u32 index = 0; index < grids.size(); ++index)
                {
                    grids[index].width = 4;
                    grids[index].depth = 4;
                    grids[index].cells.resize(16);
                    std::memcpy(grids[index].cells.data(), object.bytes.get() + index * 0x120, 0x120);
                    entity_detail::g_nav_grids[index] = &grids[index];
                }
                entity_detail::set_all_navigation_regions_blocked(read_u16(parameters, 8),
                    std::bit_cast<i16>(read_u16(parameters, 0xA)));
                for (u32 index = 0; index < grids.size(); ++index)
                {
                    std::memcpy(object.bytes.get() + index * 0x120, grids[index].cells.data(), 0x120);
                    entity_detail::g_nav_grids[index] = nullptr;
                }
                entity_detail::g_nav_count = 0;
            }
            else
            {
                Actor actor;
                RuntimeModelInstance primary{}, secondary{};
                ActorModel model{};
                model.runtime = &primary;
                actor.model = (read_u32(parameters, 8) & 1) != 0 ? &model : nullptr;
                std::memcpy(primary.node_flags.data(), object.bytes.get() + 0x900, 8);
                std::memcpy(secondary.node_flags.data(), object.bytes.get() + 0x910, 8);
                AnimationResourceRegistry registry;
                ResourceErrorState error{};
                RuntimeModelResourceViews views;
                RuntimeModelDeferredPool deferred;
                u32 fence = 0;
                RuntimeShaderRegistrationState shaders;
                auto scratch = std::make_unique<RuntimeShaderAnimationScratch>();
                const std::function<void(u8*)> heap_free = [](u8* pointer)
                {
                    engine_heap_free(pointer);
                };
                ActorEffectModelContext models{registry, error, views, deferred, fence, {}, shaders, *scratch, heap_free};
                RuntimeActorSoundQueues sounds;
                Vec3 origin{};
                ActorEffectStreamContext effects{models, sounds, random, origin, origin};
                i32 count = std::bit_cast<i32>(read_u32(parameters, 0x14));
                i32 slot = std::bit_cast<i32>(read_u32(parameters, 0x10));
                const std::array<u8, 5> groups{};
                BackgroundPartDispatch context{std::span<const u8>(parameters).subspan(0x400, 0x22C), count, slot, groups, sounds, effects};
                Vec3 position{}, source{};
                advance_background_part_stage(actor, (read_u32(parameters, 8) & 2) != 0 ? &secondary : nullptr,
                    {object.bytes.get(), 0x9C}, 0, 0, std::span<const u8>(parameters).subspan(0x700),
                    position, source, {reinterpret_cast<u32*>(object.bytes.get() + 0x940), 32}, context);
                std::memcpy(object.bytes.get() + 0x900, primary.node_flags.data(), 8);
                std::memcpy(object.bytes.get() + 0x910, secondary.node_flags.data(), 8);
                std::memcpy(object.bytes.get() + 0xB80, &slot, 4);
            }
            result = true;
        }
        else if (mode == 40)
        {
            Actor actor;
            std::memcpy(actor.xform, parameters.data() + 0x400, 48);
            std::memcpy(&actor.position, parameters.data() + 0x610, 12);
            Vec3 origin;
            std::memcpy(&origin, parameters.data() + 0x600, 12);
            float* active = read_u32(parameters, 8) != 0 ? reinterpret_cast<float*>(object.bytes.get() + 0x840) : nullptr;
            const std::function<std::span<const u8>(u32)> resolve = [&](u32 address)
            {
                return std::span<const u8>(parameters).subspan(0x500 + address - 0x10008000u, 64);
            };
            RuntimeActorMatrixSampling context{std::span<u8>(object.bytes.get(), 0x90),
                std::span<const u8>(parameters).subspan(0x700, 0x30),
                std::span<float, 12>(reinterpret_cast<float*>(object.bytes.get() + 0x800), 12), active, origin, resolve};
            sample_actor_model_matrix(actor, i32(read_u32(parameters, 0)),
                *reinterpret_cast<Vec3*>(object.bytes.get() + 0x700),
                *reinterpret_cast<Vec3*>(object.bytes.get() + 0x710), i32(read_u32(parameters, 4)), context);
            const u32 address = active == context.scratch.data() ? 0x0050D7A0u : active == nullptr ? 0 : 0x10000840u;
            std::memcpy(object.bytes.get() + 0x750, &address, 4);
            result = true;
        }
        else if (mode == 39)
        {
            const u32 subtype = read_u32(parameters, 0);
            u32 value = 0;
            if (subtype < 2)
            {
                const auto rows = std::span<const u8>{parameters}.subspan(0x400, read_u32(parameters, 4) * 0x9C);
                const i32 part = std::bit_cast<i32>(read_u32(parameters, 8));
                if (subtype == 0)
                {
                    value = u32(find_active_background_part(rows, part));
                }
                else
                {
                    Actor actor;
                    ActorModel model{std::span<const u8>{parameters}.subspan(0x900, 0x200)};
                    actor.model = read_u32(parameters, 0xC) != 0 ? &model : nullptr;
                    if (read_u32(parameters, 0x10) == 0)
                    {
                        model.cpu = {};
                    }
                    std::array<u8, 0x100> zero{};
                    value = background_part_can_be_destroyed(actor, rows, part, zero);
                }
            }
            else if (subtype == 2)
            {
                value = background_part_settings_value(reinterpret_cast<const char*>(parameters.data() + 0x100),
                    std::span<const u8>{parameters}.subspan(0x400), std::bit_cast<i32>(read_u32(parameters, 4)));
            }
            else if (subtype < 6)
            {
                ActorEffectNode effect{};
                effect.bytes = subtype == 5 ? std::span<u8>{object.bytes.get(), object.size} : std::span<u8>{parameters}.subspan(0x400, 0x390);
                auto particle = std::span<u8>{object.bytes.get() + 0x800, 0x150};
                const i32 pal = std::bit_cast<i32>(read_u32(parameters, 4));
                const i32 override_value = std::bit_cast<i32>(read_u32(parameters, 8));
                if (subtype == 3)
                {
                    initialize_actor_effect_particle(particle, effect, random);
                }
                else if (subtype == 4)
                {
                    initialize_actor_effect_particle_frames(particle, effect, random, pal, override_value);
                }
                else
                {
                    const u8* selected = allocate_actor_effect_particle(effect, 0x10000000u,
                        read_u32(parameters, 0xC), random, pal, override_value);
                    value = selected != nullptr ? 0x10000000u + u32(selected - object.bytes.get()) : 0;
                }
            }
            else
            {
                ActorEffectNode effect{};
                effect.bytes = {object.bytes.get(), object.size};
                AnimationResourceRegistry registry;
                ResourceErrorState error{};
                RuntimeModelResourceViews views;
                RuntimeModelDeferredPool deferred;
                u32 fence = 0;
                RuntimeShaderRegistrationState shaders;
                auto scratch = std::make_unique<RuntimeShaderAnimationScratch>();
                const std::function<void(u8*)> heap_free = [](u8* pointer)
                {
                    engine_heap_free(pointer);
                };
                ActorEffectModelContext models{registry, error, views, deferred, fence, {}, shaders, *scratch, heap_free};
                RuntimeActorSoundQueues sounds;
                Vec3 origin{};
                ActorEffectStreamContext stream{models, sounds, random, origin, origin};
                AnimationResourceRecord record;
                record.name = reinterpret_cast<const char*>(parameters.data() + 0x100);
                record.hash = animation_resource_hash(record.name.c_str());
                record.resource_slot = 1;
                record.references = i32(read_u32(parameters, 0x10));
                registry.records.push_back(record);
                AnimationLoadedResource loaded;
                loaded.cpu = std::span<const u8>{parameters}.subspan(0x800, 0x100);
                registry.loaded.push_back(loaded);
                if (subtype == 6)
                {
                    if (read_u32(parameters, 4) != 0)
                    {
                        effect.texture = loaded.cpu;
                    }
                    reset_actor_effect_to_debris(effect, models);
                }
                else
                {
                    value = interpret_named_actor_effect(&effect, record.name.c_str(), nullptr, stream);
                }
                const u32 references = u32(registry.records[0].references);
                std::memcpy(object.bytes.get() + 0xB84, &references, 4);
            }
            std::memcpy(object.bytes.get() + 0xB80, &value, 4);
            result = true;
        }
        else if (mode == 38)
        {
            const u32 subtype = read_u32(parameters, 0);
            u32 value = 0;
            if (subtype == 0)
            {
                ObjectSizes sizes;
                runtime_background_storage_sizes(sizes);
                std::memcpy(object.bytes.get() + 0x800, &sizes, sizeof(sizes));
            }
            else if (subtype == 1)
            {
                Actor actor;
                ActorModel model{std::span<const u8>{parameters}.subspan(0x400)};
                actor.model = read_u32(parameters, 4) != 0 ? &model : nullptr;
                if (read_u32(parameters, 8) == 0)
                {
                    model.cpu = {};
                }
                value = actor_model_link_head_offset(actor);
            }
            else if (subtype == 2)
            {
                std::array<entity_detail::ListHead, 6> nodes{};
                const u32 count = read_u32(parameters, 4);
                for (u32 index = 0; index < count; ++index)
                {
                    nodes[index].next = &nodes[index + 1];
                    nodes[index + 1].prev = &nodes[index];
                }
                nodes[5].next = &nodes[0];
                append_runtime_list_node(nodes[0], nodes[5]);
                for (u32 index = 0; index < nodes.size(); ++index)
                {
                    for (u32 side = 0; side < 2; ++side)
                    {
                        const void* pointer = side == 0 ? nodes[index].prev : nodes[index].next;
                        u32 address = 0;
                        for (u32 other = 0; other < nodes.size(); ++other)
                        {
                            if (pointer == &nodes[other])
                            {
                                address = 0x10004000u + other * 0x20u;
                            }
                        }
                        std::memcpy(object.bytes.get() + 0x800 + index * 8 + side * 4, &address, 4);
                    }
                }
            }
            else if (subtype == 3)
            {
                entity_detail::NavGrid grid;
                grid.width = std::bit_cast<i32>(read_u32(parameters, 4));
                grid.depth = std::bit_cast<i32>(read_u32(parameters, 8));
                grid.cells.resize(16);
                std::memcpy(grid.cells.data(), parameters.data() + 0x400, 0x120);
                value = u32(entity_detail::set_navigation_region_blocked(grid, read_u16(parameters, 0xC),
                    std::bit_cast<i16>(read_u16(parameters, 0xE))));
                std::memcpy(object.bytes.get() + 0x800, grid.cells.data(), 0x120);
            }
            else if (subtype == 4)
            {
                value = u32(find_background_part_settings(reinterpret_cast<const char*>(parameters.data() + 0x100),
                    std::span<const u8>{parameters}.subspan(0x400), std::bit_cast<i32>(read_u32(parameters, 4))));
            }
            else
            {
                Actor actor;
                ActorModel model{std::span<const u8>{parameters}.subspan(0x400, 0x200)};
                actor.model = read_u32(parameters, 8) != 0 ? &model : nullptr;
                std::array<u8, 0x80> zero{};
                const char* fallback = reinterpret_cast<const char*>(parameters.data() + 0x300);
                const char* selected = background_part_rumble(actor, i32(read_u32(parameters, 4)),
                    std::span<const u8>{parameters}.subspan(0x700), std::bit_cast<i32>(read_u32(parameters, 0x10)), fallback, zero);
                value = selected == fallback ? 0x00421E58u : 0x10008000u + u32(selected - reinterpret_cast<const char*>(parameters.data() + 0x700));
                std::strcpy(reinterpret_cast<char*>(object.bytes.get() + 0x800), selected);
            }
            std::memcpy(object.bytes.get() + 0x700, &value, 4);
            result = true;
        }
        else if (mode == 37)
        {
            const i32 found = find_runtime_aid_list_entry(std::span<const u8>{parameters}.subspan(0x400),
                reinterpret_cast<const char*>(parameters.data() + 0x100));
            std::memcpy(object.bytes.get() + 0x700, &found, 4);
            result = true;
        }
        else if (mode == 36)
        {
            const u32 subtype = read_u32(parameters, 0);
            const std::array<DoorRoomAlias, 2> aliases{{{"all", "centralhallway"}, {"whitebackground", "(storybook desk)"}}};
            const char* room = reinterpret_cast<const char*>(parameters.data() + 0x100);
            if (subtype == 0)
            {
                const char* suffix = runtime_door_room_suffix(room, aliases);
                u32 address = 0;
                if (suffix == aliases[0].target)
                {
                    address = 0x0032AEB4;
                }
                else if (suffix == aliases[1].target)
                {
                    address = 0x0032AE90;
                }
                else if (suffix != nullptr)
                {
                    address = 0x10002500u + u32(suffix - room);
                }
                std::memcpy(object.bytes.get() + 0x700, &address, 4);
                if (suffix != nullptr)
                {
                    std::strcpy(reinterpret_cast<char*>(object.bytes.get() + 0x800), suffix);
                }
                else
                {
                    object.bytes[0x800] = 0;
                }
                result = suffix != nullptr;
            }
            else if (subtype == 1)
            {
                const std::array<const char*, 3> variants{"open", "noentry", "ghouly"};
                const std::array<const char*, 12> shapes{"square_1_highright", "square_1_highleft",
                    "square_2_highright", "square_2_highleft", "square_3_highright", "square_3_highleft",
                    "round_1_highright", "round_1_highleft", "round_2_highright", "round_2_highleft",
                    "round_3_highright", "round_3_highleft"};
                const RuntimeDoorNames names{room, aliases, variants, shapes, {}};
                AnimationResourceRegistry resources;
                if (read_u32(parameters, 0x10) != 0)
                {
                    AnimationResourceRecord record;
                    record.name = reinterpret_cast<const char*>(parameters.data() + 0x200);
                    record.hash = animation_resource_hash(record.name.c_str());
                    resources.records.push_back(record);
                }
                result = build_runtime_door_clip_names(reinterpret_cast<char*>(object.bytes.get() + 0x800),
                    reinterpret_cast<char*>(object.bytes.get() + 0x880), read_u16(parameters, 8),
                    read_u16(parameters, 0xA), i32(read_u32(parameters, 0xC)), read_u16(parameters, 4), names, resources);
            }
            else
            {
                Vec3 position, euler, scale;
                std::memcpy(&position, parameters.data() + 0x400, 12);
                std::memcpy(&euler, parameters.data() + 0x40C, 12);
                std::memcpy(&scale, parameters.data() + 0x418, 12);
                build_runtime_pose_matrix(reinterpret_cast<float*>(object.bytes.get() + 0x800), position, euler, scale,
                    read_u32(parameters, 4) != 0 ? reinterpret_cast<float*>(object.bytes.get() + 0x840) : nullptr);
                result = true;
            }
        }
        else if (mode == 35)
        {
            const u32 subtype = read_u32(parameters, 0);
            std::array<u8, 0x600> cpu;
            std::memcpy(cpu.data(), parameters.data() + 0x400, cpu.size());
            const std::span<const u8> model = subtype == 2 ? std::span<const u8>{} : std::span<const u8>{cpu};
            std::array<RuntimePointGraph, 3> rows{};
            for (u32 index = 0; index < rows.size(); ++index)
            {
                rows[index].fields = {object.bytes.get() + index * 0x1E0, 0x1E0};
            }
            RuntimeBackgroundAnimation first{subtype == 1 ? nullptr : &model,
                std::span<const u8>{cpu}.subspan(0x200),
                subtype == 3 ? std::span<const u8>{} : std::span<const u8>{cpu}.subspan(0x300),
                subtype == 5 ? std::span<RuntimePointGraph>{} : std::span<RuntimePointGraph>{rows},
                std::bit_cast<i32>(read_u32(parameters, 0x10)), std::span<const u8>{parameters}.subspan(0x3C0)};
            const auto resolve = [&](u32 address) -> std::span<const u8>
            {
                if (address == 0x10008800u)
                {
                    return std::span<const u8>{parameters}.subspan(0x40);
                }
                if (address >= 0x10009000u && address < 0x1000C000u)
                {
                    return std::span<const u8>{parameters}.subspan(0xA00 + ((address - 0x10009000u) / 0x1000u) * 0x80);
                }
                return std::span<const u8>{parameters}.subspan(0x100 + address - 0x1000D000u);
            };
            std::vector<std::pair<u8*, u32>> allocations;
            initialize_background_animation(subtype == 0 ? nullptr : &first, resolve,
                [&](u8* pointer, u32 size)
                {
                    const u32 address = 0x10010000u + u32(allocations.size()) * 0x10000u;
                    std::memset(pointer, 0, size);
                    allocations.emplace_back(pointer, size);
                    return address;
                });
            const u32 blocks = u32(allocations.size());
            std::memcpy(object.bytes.get() + 0x700, &blocks, 4);
            for (const auto& [pointer, size] : allocations)
            {
                const auto* bytes = reinterpret_cast<const u8*>(&size);
                graph_bytes.insert(graph_bytes.end(), bytes, bytes + 4);
                graph_bytes.insert(graph_bytes.end(), pointer, pointer + size);
                engine_heap_free(pointer);
            }
            result = true;
        }
        else if (mode == 34)
        {
            RuntimePointGraph graph;
            graph.fields = {object.bytes.get(), 0x1E0};
            std::array<Vec3, 64> source;
            std::memcpy(source.data(), parameters.data() + 0x400, sizeof(source));
            u32 allocation_size = 0;
            build_runtime_point_graph(graph, source, read_u32(parameters, 0),
                [&](u8* pointer, u32 size)
                {
                    allocation_size = size;
                    std::memset(pointer, 0, size);
                    return 0x10010000u;
                });
            graph_bytes.assign(graph.allocation, graph.allocation + allocation_size);
            std::memcpy(object.bytes.get() + 0x700, &allocation_size, 4);
            engine_heap_free(graph.allocation);
            result = true;
        }
        else if (mode == 32)
        {
            const u32 subtype = read_u32(parameters, 0);
            RuntimePointGraph graph;
            graph.fields = {object.bytes.get(), 0x1E0};
            result = true;
            if (subtype == 0)
            {
                allocate_runtime_point_graph_storage(graph, read_u32(parameters, 4), read_u32(parameters, 8),
                    [](u8*, u32)
                    {
                        return 0x10006000u;
                    });
                engine_heap_free(graph.allocation);
            }
            else
            {
                std::memcpy(object.bytes.get() + 0x400, parameters.data() + 0x400, 0x300);
                std::memcpy(object.bytes.get() + 0x800, parameters.data() + 0x800, 0x2A0);
                graph.points = {object.bytes.get() + 0x400, 0x300};
                graph.edges = {object.bytes.get() + 0x800, 0x2A0};
                graph.points_address = 0x10004000u;
                if (subtype == 1)
                {
                    g_point_graph_sort = &graph;
                    const u32 compared = u32(compare_runtime_point_graph_edges(graph.edges.data(), graph.edges.data() + 0x1C));
                    std::memcpy(object.bytes.get() + 0x700, &compared, 4);
                    g_point_graph_sort = nullptr;
                }
                else if (subtype == 2)
                {
                    sort_runtime_point_graph_edges(graph);
                }
                else
                {
                    prepare_runtime_point_graph_edges(graph);
                }
            }
        }
        else if (mode == 33)
        {
            const u32 subtype = read_u32(parameters, 0);
            const u32 flags = read_u32(parameters, 4);
            std::vector<std::pair<u8*, u32>> blocks;
            std::vector<u32> released;
            const auto allocate = [&](u32 address, u32 size)
            {
                auto* pointer = static_cast<u8*>(engine_heap_allocate(i32(size)));
                blocks.emplace_back(pointer, address);
                return pointer;
            };
            const std::function<void(u8*)> heap_free = [&](u8* pointer)
            {
                for (auto& block : blocks)
                {
                    if (block.first == pointer)
                    {
                        released.push_back(block.second);
                        engine_heap_free(pointer);
                        block.first = nullptr;
                        return;
                    }
                }
                throw Error("unmapped background fixture heap release");
            };
            GhoulySpawnPool* pool = nullptr;
            if ((subtype == 0 || subtype == 3) && (flags & 1) != 0)
            {
                const u32 count = read_u32(parameters, 8);
                pool = reinterpret_cast<GhoulySpawnPool*>(allocate(0x10006000,
                    u32(sizeof(GhoulySpawnPool)) + count * u32(sizeof(BackgroundShapeEvent))));
                pool->stride = sizeof(BackgroundShapeEvent);
                pool->grow = 10;
                pool->begin = reinterpret_cast<u8*>(pool + 1);
                pool->cursor = pool->begin + count * sizeof(BackgroundShapeEvent);
                pool->limit = pool->cursor;
                for (u32 row = 0; row < count; ++row)
                {
                    const BackgroundShapeEvent event{object.bytes.get() + 0x800 + read_u32(parameters, 0x400 + row * 8) * 20,
                        read_u32(parameters, 0x404 + row * 8)};
                    std::memcpy(pool->begin + row * sizeof(event), &event, sizeof(event));
                }
            }
            const u8 row_count = u8(read_u32(parameters, 0xC));
            std::array<BackgroundAnimationAllocation, 8> rows{};
            if (subtype == 1 || (subtype == 3 && (flags & 32) != 0))
            {
                for (u32 row = 0; row < row_count; ++row)
                {
                    if ((read_u32(parameters, 0x14) & (1u << row)) != 0)
                    {
                        rows[row].allocation = allocate(0x1000C000u + row * 0x100u, 16);
                    }
                }
            }
            RuntimeActorSoundQueues sounds;
            AnimationResourceRegistry registry;
            RuntimeModelDeferredPool deferred;
            RuntimeShaderRegistrationState shaders;
            u32 fence = 0;
            RuntimeModelSoundRelease model_sound{sounds, registry, deferred, fence, shaders, heap_free};
            RuntimeModelSoundObject auxiliary{};
            auxiliary.sound_2dc.enabled = parameters[0x20];
            auxiliary.sound_2dc.previous_bone = read_u16(parameters, 0x22);
            if (subtype == 2 || (subtype == 3 && (flags & 8) != 0))
            {
                auxiliary.allocation = allocate(0x10009000, 0x390);
            }
            if (subtype == 0)
            {
                restore_background_shape_event_flags(pool, heap_free);
                const u32 zero = 0;
                std::memcpy(object.bytes.get() + 0x9E4, &zero, 4);
            }
            else if (subtype == 1)
            {
                release_background_animation_allocations(row_count, rows, heap_free);
            }
            else if (subtype == 2)
            {
                release_runtime_model_sound_object(auxiliary, model_sound);
                object.bytes[0x750] = auxiliary.sound_2dc.enabled;
                std::memcpy(object.bytes.get() + 0x752, &auxiliary.sound_2dc.previous_bone, 2);
            }
            else
            {
                RuntimeBackgroundLifetime background{};
                background.events = pool;
                background.first_push_buffer = (flags & 2) != 0 ? allocate(0x10007000, 16) : nullptr;
                background.second_push_buffer = (flags & 4) != 0 ? allocate(0x10008000, 16) : nullptr;
                background.has_push_buffer = i32(read_u32({object.bytes.get(), object.size}, 0x9E0));
                background.auxiliary = (flags & 8) != 0 ? &auxiliary : nullptr;
                background.volumes = (flags & 16) != 0 ? allocate(0x1000A000, 16) : nullptr;
                background.animation = (flags & 32) != 0 ? allocate(0x1000B000, 16) : nullptr;
                entity_detail::ListHead previous;
                entity_detail::ListHead next;
                previous.next = &background.link;
                next.prev = &background.link;
                background.link.prev = (read_u32(parameters, 0x10) & 1) != 0 ? &previous : nullptr;
                background.link.next = (read_u32(parameters, 0x10) & 2) != 0 ? &next : nullptr;
                background.disables_clear = std::bit_cast<i32>(read_u32(parameters, 0x1C));
                i32 disable_clear = std::bit_cast<i32>(read_u32({object.bytes.get(), object.size}, 0x700));
                RuntimeBackgroundRelease services{row_count, rows, disable_clear, model_sound, heap_free};
                destroy_runtime_background(background, services);
                for (u32 offset : {0x9D8u,0x9DCu,0x9E0u,0x940u,0x94Cu,0x950u,0x934u,0x938u,0x93Cu,0x9E4u})
                {
                    const u32 zero = 0;
                    std::memcpy(object.bytes.get() + offset, &zero, 4);
                }
                std::memcpy(object.bytes.get() + 0x700, &disable_clear, 4);
                const u32 previous_next = previous.next == &next ? 0x10004100u :
                    previous.next == nullptr ? 0u : 0x10000934u;
                const u32 next_previous = next.prev == &previous ? 0x10004000u :
                    next.prev == nullptr ? 0u : 0x10000934u;
                std::memcpy(object.bytes.get() + 0x708, &previous_next, 4);
                std::memcpy(object.bytes.get() + 0x70C, &next_previous, 4);
            }
            const u32 count = u32(released.size());
            std::memcpy(object.bytes.get() + 0x500, &count, 4);
            for (u32 row = 0; row < count; ++row)
            {
                std::memcpy(object.bytes.get() + 0x504 + row * 4, &released[row], 4);
            }
            for (const auto& block : blocks)
            {
                if (block.first != nullptr)
                {
                    engine_heap_free(block.first);
                }
            }
            result = true;
        }
        else if (mode == 31)
        {
            const u32 subtype = read_u32(parameters, 0);
            result = true;
            if (subtype == 4)
            {
                std::array<float, 16> input;
                std::array<float, 16> output;
                std::memcpy(input.data(), parameters.data() + 0x400, 64);
                std::memcpy(output.data(), object.bytes.get() + 0x700, 64);
                float determinant = read_f32({object.bytes.get(), object.size}, 0x740);
                const bool alias = read_u32(parameters, 0xC) != 0;
                invert_runtime_graphics_matrix(alias ? std::span<float, 16>{input} : std::span<float, 16>{output},
                    read_u32(parameters, 8) != 0 ? &determinant : nullptr, input);
                std::memcpy(object.bytes.get() + 0x700, alias ? input.data() : output.data(), 64);
                std::memcpy(object.bytes.get() + 0x740, &determinant, 4);
            }
            else if (subtype == 5)
            {
                double value;
                std::memcpy(&value, parameters.data() + 0x400, 8);
                result = runtime_graphics_double_is_finite(value);
            }
            else
            {
                i32 count = i32(read_u32({object.bytes.get(), object.size}, 0x700));
                std::array<float, 5> scalars;
                std::memcpy(scalars.data(), object.bytes.get() + 0x704, 20);
                std::array<std::span<u8>, 4> grids;
                RuntimeBackgroundSectors state{grids, count, scalars[0], scalars[1], scalars[2],
                    scalars[3], scalars[4], std::span<const u8>{parameters}.subspan(0x100),
                    std::bit_cast<i32>(read_u32(parameters, 0x18))};
                u8 first = parameters[0x20];
                u8 second = parameters[0x21];
                u8 third = parameters[0x22];
                if (subtype == 0)
                {
                    accumulate_runtime_sector_brightness(first, second, third, state);
                }
                else if (subtype == 1)
                {
                    remap_runtime_sector_color(first, second, third, state);
                    object.bytes[0x750] = first;
                    object.bytes[0x751] = second;
                    object.bytes[0x752] = third;
                }
                else if (subtype == 2)
                {
                    find_runtime_sector_settings(reinterpret_cast<const char*>(parameters.data() + 0x380), state);
                }
                else
                {
                    std::memcpy(object.bytes.get() + 0x800, parameters.data() + 0x800, 128);
                    std::span<u8> grid{object.bytes.get() + 0x800, 128};
                    if (subtype == 3)
                    {
                        register_runtime_background_sector(grid, reinterpret_cast<const char*>(parameters.data() + 0x380), state);
                        for (u32 row = 0; row < 4; ++row)
                        {
                            const u32 present = grids[row].data() == grid.data();
                            std::memcpy(object.bytes.get() + 0x718 + row * 4, &present, 4);
                        }
                    }
                    else
                    {
                        u32 visited = 0;
                        visit_runtime_sector_colors(grid, [&](std::span<u8>, const Vec3& point, u8& a, u8& b, u8& c)
                        {
                            std::memcpy(object.bytes.get() + 0x500 + visited * 12, &point, 12);
                            ++visited;
                            accumulate_runtime_sector_brightness(a, b, c, state);
                        });
                        std::memcpy(object.bytes.get() + 0x4F0, &visited, 4);
                    }
                }
                std::memcpy(object.bytes.get() + 0x700, &count, 4);
                std::memcpy(object.bytes.get() + 0x704, scalars.data(), 20);
            }
        }
        else if (mode == 30)
        {
            std::array<Vec3, 8> vertices;
            std::memcpy(vertices.data(), parameters.data() + 0x400, sizeof(vertices));
            ActorModel model;
            model.cpu = std::span<const u8>{parameters}.subspan(0x800);
            Actor actor;
            actor.model = read_u32(parameters, 4) != 0 ? &model : nullptr;
            if (read_u32(parameters, 4) == 1)
            {
                model.cpu = {};
            }
            const u32 subtype = read_u32(parameters, 0);
            if (subtype == 0)
            {
                BackgroundCollisionBounds bounds;
                std::memcpy(&bounds.low, object.bytes.get() + 0x700, 12);
                std::memcpy(&bounds.high, object.bytes.get() + 0x70C, 12);
                bounds.vertices = vertices;
                result = accumulate_background_triangle_bounds(parameters.data() + 0x380, bounds) != 0;
                std::memcpy(object.bytes.get() + 0x700, &bounds.low, 12);
                std::memcpy(object.bytes.get() + 0x70C, &bounds.high, 12);
            }
            else if (subtype == 1)
            {
                actor.scale_x = read_f32(parameters, 0x20);
                actor.radius_y = read_f32(parameters, 0x24);
                actor.radius_xz = read_f32(parameters, 0x28);
                actor.scale_200 = read_f32(parameters, 0x2C);
                model.physics_shapes = read_u32(parameters, 0x10) != 0 ?
                    std::span<const u8>{parameters}.subspan(0x600, 0x40) : std::span<const u8>{};
                const std::unordered_map<const u8*, PhysicalShapeEventList> lists{
                    {parameters.data() + 0x620, {read_u32(parameters, 0x620), parameters.data() + 0x500}}};
                g_amb_volume_enabled = object.bytes[0x700];
                g_background_collision_bounds_enabled = object.bytes[0x701];
                std::memcpy(g_amb_color_lo, object.bytes.get() + 0x704, 12);
                std::memcpy(g_amb_color_hi, object.bytes.get() + 0x710, 12);
                std::memcpy(&g_background_collision_bounds_low, object.bytes.get() + 0x71C, 12);
                std::memcpy(&g_background_collision_bounds_high, object.bytes.get() + 0x728, 12);
                initialize_background_bounds(actor, vertices, lists);
                object.bytes[0x700] = g_amb_volume_enabled;
                object.bytes[0x701] = g_background_collision_bounds_enabled;
                std::memcpy(object.bytes.get() + 0x704, g_amb_color_lo, 12);
                std::memcpy(object.bytes.get() + 0x710, g_amb_color_hi, 12);
                std::memcpy(object.bytes.get() + 0x71C, &g_background_collision_bounds_low, 12);
                std::memcpy(object.bytes.get() + 0x728, &g_background_collision_bounds_high, 12);
                result = true;
            }
            else if (subtype == 3)
            {
                const u32 count = read_u32(parameters, 0x14);
                Vec3 output;
                if (read_u32(parameters, 0x18) != 0)
                {
                    average_runtime_model_points(vertices[0], std::span<const Vec3>{vertices}.first(count));
                    output = vertices[0];
                }
                else
                {
                    average_runtime_model_points(output, std::span<const Vec3>{vertices}.first(count));
                }
                std::memcpy(object.bytes.get() + 0x700, &output, 12);
                result = true;
            }
            else if (subtype == 4 || subtype == 5)
            {
                const auto cpu = std::span<const u8>{parameters}.subspan(0x800);
                const u32 offset = subtype == 4 ? runtime_model_animation_matrix_array(cpu) :
                    runtime_matrix_array_matrix_offset(cpu, std::bit_cast<i32>(read_u32(parameters, 0x14)));
                std::memcpy(object.bytes.get() + 0x700, &offset, 4);
                result = true;
            }
            else
            {
                std::array<EntityBonePose, 2> poses;
                for (u32 pose = 0; pose < 2; ++pose)
                {
                    poses[pose].packed = read_u32(parameters, 0x44 + pose * 4);
                    poses[pose].mats.resize(32);
                    std::memcpy(poses[pose].mats.data(), parameters.data() + 0x100 + pose * 0x100, 128);
                }
                model.primary_shape_pose = &poses[0];
                model.alternate_shape_pose = read_u32(parameters, 8) != 0 ? &poses[1] : nullptr;
                std::memcpy(actor.xform, parameters.data() + 0x40 + 0x10, 48);
                std::array<Vec3, 21> emitters;
                std::memcpy(emitters.data(), object.bytes.get() + 0x800, sizeof(emitters));
                i32 count = std::bit_cast<i32>(read_u32({object.bytes.get(), object.size}, 0x700));
                i32 vary = std::bit_cast<i32>(read_u32({object.bytes.get(), object.size}, 0x704));
                float scale = read_f32({object.bytes.get(), object.size}, 0x708);
                Vec3 vary_point;
                std::memcpy(&vary_point, object.bytes.get() + 0x70C, 12);
                i32 phase = std::bit_cast<i32>(read_u32({object.bytes.get(), object.size}, 0x718));
                Vec3 phase_point;
                std::memcpy(&phase_point, object.bytes.get() + 0x71C, 12);
                std::array<float, 4> color;
                std::memcpy(color.data(), object.bytes.get() + 0x728, 16);
                std::array<float, 12> scratch;
                std::memcpy(scratch.data(), object.bytes.get() + 0x740, 48);
                RuntimeBackgroundAmbient state{emitters, count, vary_point, vary, scale, color, phase, phase_point, scratch};
                initialize_background_ambient_nodes(actor, random, state);
                std::memcpy(object.bytes.get() + 0x800, emitters.data(), sizeof(emitters));
                std::memcpy(object.bytes.get() + 0x700, &count, 4);
                std::memcpy(object.bytes.get() + 0x704, &vary, 4);
                std::memcpy(object.bytes.get() + 0x708, &scale, 4);
                std::memcpy(object.bytes.get() + 0x70C, &vary_point, 12);
                std::memcpy(object.bytes.get() + 0x718, &phase, 4);
                std::memcpy(object.bytes.get() + 0x71C, &phase_point, 12);
                std::memcpy(object.bytes.get() + 0x728, color.data(), 16);
                std::memcpy(object.bytes.get() + 0x740, scratch.data(), 48);
                result = true;
            }
        }
        else if (mode == 29)
        {
            using namespace entity_detail;
            ActorModel model;
            model.cpu = std::span<const u8>{parameters}.subspan(0x800);
            Actor actor;
            actor.model = &model;
            std::array<float, 256> costs;
            std::memcpy(costs.data(), parameters.data() + 0x400, sizeof(costs));
            g_amb_color_hi[1] = read_f32(parameters, 4);
            u8* nodes = nullptr;
            NavigationSceneStorage storage;
            build_scene_navigation(read_u32(parameters, 0) == 0 ? nullptr : &actor, costs, storage, nodes);
            std::memcpy(object.bytes.get() + 0x700, &g_nav_count, 4);
            for (i32 floor = 0; floor < g_nav_count; ++floor)
            {
                const auto* grid = g_nav_grids[floor];
                std::memcpy(object.bytes.get() + 0x704 + floor * 16, &grid->width, 16);
                std::memcpy(object.bytes.get() + 0x800 + floor * 0x120, grid->cells.data(), 16 * sizeof(NavCell));
            }
            std::memcpy(object.bytes.get() + 0x730, g_nav_min_y, 8);
            std::memcpy(object.bytes.get() + 0x738, g_nav_max_y, 8);
            if (g_nav_count != 0)
            {
                std::memcpy(object.bytes.get() + 0x740, &storage.bounds.min_x, 16);
                std::memcpy(object.bytes.get() + 0x500, nodes, 16 * 20);
            }
            const auto* records = g_nav_floor_connections[0];
            const u32 rows = records != nullptr ? u32(records->end - records->begin) / 12 : 0;
            std::memcpy(object.bytes.get() + 0x660, &rows, 4);
            for (u32 row = 0; row < rows; ++row)
            {
                std::memcpy(object.bytes.get() + 0x664 + row * 8, records->begin + row * 12, 8);
            }
            result = true;
        }
        else if (mode == 28)
        {
            using namespace entity_detail;
            const NavigationModelGrid source{std::bit_cast<i32>(read_u32(parameters, 4)),
                std::bit_cast<i32>(read_u32(parameters, 8)), std::bit_cast<i32>(read_u32(parameters, 0xC)),
                std::span<const u8>{parameters}.subspan(0x800)};
            NavigationModelBounds bounds;
            std::memcpy(&bounds.low, object.bytes.get() + 0x700, 12);
            std::memcpy(&bounds.high, object.bytes.get() + 0x70C, 12);
            result = true;
            switch (read_u32(parameters, 0))
            {
            case 0:
                navigation_model_bounds(source, bounds.low, bounds.high);
                std::memcpy(object.bytes.get() + 0x700, &bounds.low, 12);
                std::memcpy(object.bytes.get() + 0x70C, &bounds.high, 12);
                break;
            case 1:
            {
                const i32 quotient = align_navigation_model_bounds(source, bounds);
                std::memcpy(object.bytes.get() + 0x700, &bounds.low, 12);
                std::memcpy(object.bytes.get() + 0x70C, &bounds.high, 12);
                std::memcpy(object.bytes.get() + 0x718, &bounds.min_x, 16);
                std::memcpy(object.bytes.get() + 0x728, &quotient, 4);
                break;
            }
            default:
            {
                NavigationGridOwner owner;
                if (read_u32(parameters, 0) == 2)
                {
                    owner = allocate_navigation_grid(source.width, source.depth, source.cell);
                }
                else
                {
                    g_amb_color_hi[1] = read_f32(parameters, 0x10);
                    auto blocked = decode_navigation_model_grid(0, source, bounds, owner);
                    std::memcpy(object.bytes.get() + 0x600, &blocked->count, 4);
                    std::memcpy(object.bytes.get() + 0x604, &blocked->bytes, 4);
                    std::memcpy(object.bytes.get() + 0x608, blocked->bits, blocked->bytes);
                    std::memcpy(object.bytes.get() + 0x710, &bounds.min_x, 16);
                    g_nav_grids[0] = nullptr;
                }
                std::memcpy(object.bytes.get() + 0x700, &owner->width, 16);
                std::memcpy(object.bytes.get() + 0x800, owner->cells.data(), owner->cells.size() * sizeof(NavCell));
                break;
            }
            }
        }
        else if (mode == 27)
        {
            using namespace entity_detail;
            NavGrid grid;
            grid.width = 4;
            grid.depth = 4;
            grid.cells.resize(16);
            std::memcpy(grid.cells.data(), parameters.data() + 0x800, 16 * sizeof(NavCell));
            std::array<float, 256> costs;
            std::memcpy(costs.data(), parameters.data() + 0x400, sizeof(costs));
            g_nav_grids[0] = &grid;
            g_nav_min_y[0] = read_f32(parameters, 0x20);
            g_nav_max_y[0] = read_f32(parameters, 0x24);
            g_nav_min_y[1] = read_f32(parameters, 0x38);
            const i32 count = std::bit_cast<i32>(read_u32(parameters, 4));
            result = true;
            switch (read_u32(parameters, 0))
            {
            case 0:
                accumulate_navigation_height_bounds(0, count);
                break;
            case 1:
                initialize_navigation_edge_costs(0, count, costs);
                break;
            case 2:
                initialize_navigation_height_connections(0, count, read_f32(parameters, 8), costs);
                break;
            case 4:
                mark_navigation_isolated_cells(0, count);
                break;
            case 5:
            {
                std::array<NavigationConnectionPair, 8> pairs;
                std::memcpy(pairs.data(), parameters.data() + 0x200, sizeof(pairs));
                remove_navigation_connections(0, std::span<const NavigationConnectionPair>{pairs}.first(read_u32(parameters, 0x3C)));
                break;
            }
            case 6:
            {
                collect_navigation_floor_connections(0, count);
                const auto* records = g_nav_floor_connections[0];
                const u32 rows = records != nullptr ? u32(records->end - records->begin) / 12 : 0;
                std::memcpy(object.bytes.get() + 0x600, &rows, 4);
                for (u32 row = 0; row < rows; ++row)
                {
                    std::memcpy(object.bytes.get() + 0x604 + row * 8, records->begin + row * 12, 8);
                }
                if (g_nav_floor_connections[0] != nullptr)
                {
                    engine_heap_free(g_nav_floor_connections[0]);
                }
                g_nav_floor_connections[0] = nullptr;
                break;
            }
            default:
            {
                float cost = read_f32(parameters, 0x28);
                i32 destination = std::bit_cast<i32>(read_u32(parameters, 0x2C));
                i32 direction = std::bit_cast<i32>(read_u32(parameters, 0x30));
                result = next_navigation_neighbor(grid, read_u32(parameters, 0xC),
                    read_u32(parameters, 0x10), read_u32(parameters, 0x14),
                    read_u32(parameters, 0x18), u16(read_u32(parameters, 0x1C)),
                    read_u32(parameters, 0x34), costs, cost, destination, direction) != nullptr;
                std::memcpy(object.bytes.get() + 0x78, &cost, 4);
                std::memcpy(object.bytes.get() + 0x7C, &destination, 4);
                std::memcpy(object.bytes.get() + 0x80, &direction, 4);
                break;
            }
            }
            std::memcpy(object.bytes.get() + 0x70, &g_nav_min_y[0], 4);
            std::memcpy(object.bytes.get() + 0x74, &g_nav_max_y[0], 4);
            std::memcpy(object.bytes.get() + 0x800, grid.cells.data(), 16 * sizeof(NavCell));
            g_nav_grids[0] = nullptr;
        }
        else if (mode == 26)
        {
            std::array<LiveScript, 8> nodes{};
            const u32 count = read_u32(parameters, 4);
            for (u32 row = 0; row < count; ++row)
            {
                nodes[row].type = read_u32(parameters, 0x400 + row * 4);
                nodes[row].next = row + 1 < count ? &nodes[row + 1] : nullptr;
            }
            g_live_scripts = count != 0 ? &nodes[0] : nullptr;
            result = find_runtime_script_event_type(read_u32(parameters, 0)) != nullptr;
            g_live_scripts = nullptr;
        }
        else if (mode == 25)
        {
            const Vec3 first{read_f32(parameters, 0x680), read_f32(parameters, 0x684), read_f32(parameters, 0x688)};
            const Vec3 second{read_f32(parameters, 0x6A0), read_f32(parameters, 0x6A4), read_f32(parameters, 0x6A8)};
            const Vec3 direction{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 output, contact;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            std::memcpy(&contact, object.bytes.get() + 0x740, 12);
            switch (read_u32(parameters, 0))
            {
            case 0:
                normalize_runtime_graphics_vector(output, first);
                result = true;
                break;
            case 1:
                result = runtime_graphics_scalars_equal(read_f32(parameters, 0x3C0), read_f32(parameters, 0x3C4));
                break;
            case 2:
                divide_runtime_graphics_vector(output, first, read_f32(parameters, 0x3C0));
                result = true;
                break;
            case 3:
                sweep_runtime_directed_cylinder_push(first, second, read_f32(parameters, 0x3C0),
                    read_f32(parameters, 0x3C4), direction, output, normalize_runtime_graphics_vector);
                result = true;
                break;
            default:
            {
                Actor actor, other;
                actor.position = {read_f32(parameters, 0x520), read_f32(parameters, 0x524), read_f32(parameters, 0x528)};
                other.position = {read_f32(parameters, 0x540), read_f32(parameters, 0x544), read_f32(parameters, 0x548)};
                result = push_runtime_attached_cylinder(actor, other,
                    *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400),
                    *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x440),
                    read_u32(parameters, 4) != 0 ? &direction : nullptr, first,
                    output, contact, normalize_runtime_graphics_vector);
                break;
            }
            }
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
            std::memcpy(object.bytes.get() + 0x740, &contact, 12);
        }
        else if (mode == 24)
        {
            Actor actor;
            ActorModel model{parameters};
            actor.model = read_u32(parameters, 0x3C4) != 0 ? &model : nullptr;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.radius_y = read_f32(parameters, 0x34);
            actor.radius_xz = read_f32(parameters, 0x38);
            actor.model_size_x = read_f32(parameters, 0x1C4);
            actor.model_size_y = read_f32(parameters, 0x1C8);
            actor.model_size_z = read_f32(parameters, 0x1CC);
            actor.model_mode = i32(read_u32(parameters, 0x118));
            std::memcpy(actor.xform, parameters.data() + 0x500, 48);
            const auto& merged = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400);
            Vec3 output, contact;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            std::memcpy(&contact, object.bytes.get() + 0x740, 12);
            RuntimeActorShapePush context{actor, reinterpret_cast<const u32*>(parameters.data() + 0x3A0),
                contact, {}, {}};
            result = push_runtime_actor_shapes(nullptr, nullptr, merged,
                reinterpret_cast<EntityCylinderBound*>(parameters.data() + 0x440),
                i32(read_u32(parameters, 0x3C0)), output, context);
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
            std::memcpy(object.bytes.get() + 0x740, &contact, 12);
        }
        else if (mode == 23)
        {
            Actor actor, other;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            other.position = {read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
            actor.radius_y = read_f32(parameters, 0x34);
            actor.radius_xz = read_f32(parameters, 0x38);
            actor.model_size_x = read_f32(parameters, 0x1C4);
            actor.model_size_y = read_f32(parameters, 0x1C8);
            actor.model_size_z = read_f32(parameters, 0x1CC);
            const auto& fixed = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x400);
            const auto& moving = *reinterpret_cast<const EntityCylinderBound*>(parameters.data() + 0x440);
            const Vec3 offset{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 output;
            std::memcpy(&output, object.bytes.get() + 0x700, 12);
            switch (read_u32(parameters, 0))
            {
            case 0:
            {
                float height, radius;
                runtime_actor_cylinder_metrics(actor, output, height, radius);
                std::memcpy(object.bytes.get() + 0x70C, &height, 4);
                std::memcpy(object.bytes.get() + 0x710, &radius, 4);
                result = true;
                break;
            }
            case 1:
                result = runtime_offset_cylinders_apart(fixed, moving, offset);
                break;
            case 2:
                runtime_actor_planar_delta(actor, other, output);
                result = true;
                break;
            default:
                result = push_runtime_attachment_projection(actor, other, fixed, output);
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &output, 12);
        }
        else if (mode == 22)
        {
            const Vec3* vertices = reinterpret_cast<const Vec3*>(parameters.data() + 0x400);
            const Vec3 point{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 normal{read_f32(parameters, 0x640), read_f32(parameters, 0x644), read_f32(parameters, 0x648)};
            result = read_u32(parameters, 0) == 0
                ? runtime_projected_point_in_triangle(normal, point, vertices[1], vertices[2])
                : runtime_sphere_intersects_triangle(point, read_f32(parameters, 0x3C0), vertices[0], vertices[1], vertices[2]);
        }
        else if (mode == 21)
        {
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            RuntimeCollisionShapeCall call{parameters.data() + 0x800, 0x10002800, world, inverse, nullptr,
                0x10002400, start, end, read_f32(parameters, 0x3C0), read_u32(parameters, 0x3B0),
                i32(read_u32(parameters, 0x3B4)), record, nullptr, stationary};
            const auto resolve = [&](u32 address, size_t size)
            {
                return std::span<const u8>{parameters}.subspan(address - 0x10002000, size);
            };
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = collide_runtime_physical_mesh_segment(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 1:
                result = collide_runtime_physical_mesh_sweep(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 2:
                result = collect_runtime_physical_mesh_contacts(call, std::span<const u8>{parameters}.subspan(0x800, 0x34), resolve);
                break;
            case 3:
                result = collide_runtime_posed_mesh_segment(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            case 4:
                result = collide_runtime_posed_mesh_sweep(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            default:
                result = collect_runtime_posed_mesh_contacts(call, nullptr, std::span<const u8>{parameters}.subspan(0x800, 0x1C), resolve);
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 20)
        {
            const Vec3* vertices = reinterpret_cast<const Vec3*>(parameters.data() + 0x400);
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const Vec3 origin{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 movement{read_f32(parameters, 0x620), read_f32(parameters, 0x624), read_f32(parameters, 0x628)};
            const Vec3 normal{read_f32(parameters, 0x640), read_f32(parameters, 0x644), read_f32(parameters, 0x648)};
            switch (read_u32(parameters, 0))
            {
            case 0:
            {
                std::array<i32, 3> lower, upper;
                runtime_mesh_query_cell_bounds(std::span<const u8>{parameters}.subspan(0x800), origin,
                    movement, read_f32(parameters, 0x3C0), lower, upper);
                std::memcpy(object.bytes.get() + 0x700, lower.data(), 12);
                std::memcpy(object.bytes.get() + 0x70C, upper.data(), 12);
                result = true;
                break;
            }
            case 1:
            {
                float fraction = read_f32({object.bytes.get(), object.size}, 0x700);
                result = intersect_runtime_mesh_triangle(movement, vertices[1], vertices[2], vertices[0], origin, fraction);
                std::memcpy(object.bytes.get() + 0x700, &fraction, 4);
                break;
            }
            case 2:
                write_runtime_mesh_triangle_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x800),
                    0x10002800, vertices, normal, origin, read_f32(parameters, 0x3C0), world);
                result = true;
                break;
            default:
                write_runtime_posed_mesh_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x800),
                    0x10002800, vertices, normal, origin, read_f32(parameters, 0x3C0), world,
                    read_u32(parameters, 0x3C4) != 0 ? reinterpret_cast<const float*>(parameters.data() + 0x540) : nullptr);
                result = true;
                break;
            }
        }
        else if (mode == 19)
        {
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            const std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>> physical;
            const u32* masks = reinterpret_cast<const u32*>(parameters.data() + 0x3A4);
            RuntimeCollisionShapeDispatch context{{nullptr, 0, world, inverse, nullptr, 0, start, end,
                read_f32(parameters, 0x3C0), read_u32(parameters, 0x3B0), 0, record, nullptr, stationary},
                masks, std::span<const u8>{parameters}.subspan(0x900),
                [&](const void* pointer)
                {
                    return 0x10002000u + u32(static_cast<const u8*>(pointer) - parameters.data());
                }, physical};
            const auto list = std::span<const u8>{parameters}.subspan(0x800);
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = dispatch_runtime_model_segment_shapes(list, context) != 0;
                break;
            case 1:
                result = dispatch_runtime_model_swept_shapes(list, context) != 0;
                break;
            default:
                result = dispatch_runtime_model_contact_shapes(list, context) != 0;
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 18)
        {
            const auto& node = *reinterpret_cast<const EntityShapeNode*>(parameters.data() + 0x400);
            const float* world = reinterpret_cast<const float*>(parameters.data() + 0x500);
            const float* inverse = reinterpret_cast<const float*>(parameters.data() + 0x540);
            const Vec3 start{read_f32(parameters, 0x600), read_f32(parameters, 0x604), read_f32(parameters, 0x608)};
            const Vec3 center{read_f32(parameters, 0x620), read_f32(parameters, 0x624), read_f32(parameters, 0x628)};
            Vec3 end{read_f32({object.bytes.get(), object.size}, 0x700), read_f32({object.bytes.get(), object.size}, 0x704),
                read_f32({object.bytes.get(), object.size}, 0x708)};
            const Vec3 stationary{};
            const std::span<u8> record = read_u32(parameters, 0x3C4) != 0 ? std::span<u8>{object.bytes.get(), 0x54} : std::span<u8>{};
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = collide_runtime_model_sphere_segment(node, 0x10002400, world, inverse, nullptr, start, end, record, stationary);
                break;
            case 1:
                result = collide_runtime_model_sphere_sweep(node, 0x10002400, world, inverse, nullptr, start, end,
                    read_f32(parameters, 0x3C0), record, stationary);
                break;
            case 2:
                result = collect_runtime_model_sphere_contact(node, 0x10002400, world, inverse, nullptr, start,
                    read_f32(parameters, 0x3C0), nullptr);
                break;
            default:
                write_runtime_sphere_contact({object.bytes.get(), 0x54}, std::span<const u8>{parameters}.subspan(0x400),
                    0x10002400, start, center, read_f32(parameters, 0x3C0), world);
                result = true;
                break;
            }
            std::memcpy(object.bytes.get() + 0x700, &end, 12);
        }
        else if (mode == 17)
        {
            result = runtime_collision_shape_list_has_physical_shape(std::span<const u8>{parameters}.subspan(0x800));
        }
        else if (mode == 16)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            const Vec3 start{read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
            const Vec3 end{read_f32(parameters, 0x440), read_f32(parameters, 0x444), read_f32(parameters, 0x448)};
            const float radius = read_f32(parameters, 0x3C0);
            switch (read_u32(parameters, 0))
            {
            case 0:
                result = runtime_collision_spheres_overlap(actor.position, read_f32(parameters, 0x3C4), start, radius);
                break;
            case 1:
                result = runtime_actor_culls_segment(actor, parameters, start, end);
                break;
            case 2:
                result = runtime_actor_culls_swept_sphere(actor, parameters, start, end, radius);
                break;
            default:
                result = runtime_actor_culls_sphere(actor, parameters, start, radius);
                break;
            }
        }
        else if (mode == 15)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler.y = read_f32(parameters, 0x18);
            result = update_runtime_cinema_mind_facing_request(std::span<const u8>{parameters}.subspan(0x600),
                {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400),
                std::span<const u8>{parameters}.subspan(0x800)) != 0;
        }
        else if (mode == 14)
        {
            std::array<float, 2> direction{read_f32(parameters, 0), read_f32(parameters, 4)};
            scale_runtime_planar_direction(direction, read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, direction.data(), 8);
            result = true;
        }
        else if (mode == 12)
        {
            const float angle = interpolate_runtime_mind_angle(read_f32(parameters, 0),
                read_f32(parameters, 4), read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, &angle, 4);
            result = true;
        }
        else if (mode == 13)
        {
            const Vec3 source{read_f32(parameters, 0), read_f32(parameters, 4), read_f32(parameters, 8)};
            Vec3 output;
            offset_runtime_horizontal_point(output, source, read_f32(parameters, 0xC), read_f32(parameters, 0x10));
            std::memcpy(object.bytes.get() + 0x70, &output, sizeof(output));
            result = true;
        }
        else
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            RuntimeMindEscapePosition context{actor, parameters, std::span<const u8>{parameters}.subspan(0x400),
                parameters.data() + 0x800, nullptr, random, 0.f};
            Vec3 point;
            result = select_runtime_mind_escape_position(context, read_f32(parameters, 0x3C0),
                read_f32(parameters, 0x3C4), read_f32(parameters, 0x3C8), point);
            if (result)
            {
                std::memcpy(object.bytes.get() + 0x70, &point, sizeof(point));
            }
        }
        std::cout << result << ' ' << random << ' ' << u32(slot) << ' ';
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            std::cout << u32(object.bytes[offset]) << ' ';
        }
        for (u8 byte : graph_bytes)
        {
            std::cout << u32(byte) << ' ';
        }
        std::cout << '\n';
    }
}
