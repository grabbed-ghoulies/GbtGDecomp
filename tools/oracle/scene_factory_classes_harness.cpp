#include "entities/trigger_actor.h"
#include "graphics/effect_debris.h"
#include "graphics/effect_trails.h"
#include "graphics/global_effect_tick.h"
#include "graphics/model_skeleton_draw.h"
#include "graphics/model_matrix_array_draw.h"
#include "graphics/model_shader_states.h"
#include "graphics/model_transform_draw.h"
#include "graphics/vertex_shader_code.h"
#include "graphics/model_renderer_boot.h"
#include "graphics/model_vertex_shader_draw.h"
#include "graphics/shader_optimization_driver.h"
#include "scene/scene_scare_timers.h"
#include "graphics/effect_collision_commands.h"
#include "graphics/effect_simulation.h"
#include "graphics/effect_tick.h"
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
#include "graphics/d3dx_matrix.h"
#include "entities/euler_wrapping.h"
#include "entities/random_placement.h"
#include "entities/bounded_channels.h"
#include "graphics/effect_channels.h"
#include "graphics/effect_placement.h"
#include "entities/model_binding_selection.h"
#include "graphics/effect_binding.h"
#include "graphics/effect_command_selection.h"
#include "graphics/effect_collision_response.h"
#include "entities/direction_pitch.h"
#include "runtime/random_state.h"
#include "entities/entity_collision.h"
#include "entities/entity.h"
#include "navigation/navigation.h"
#include "camera/plane_collision.h"
#include "graphics/effect_commands.h"
#include "gbtg/model.h"
#include "graphics/actor_effect_stream.h"
#include "runtime/aid_list_lookup.h"
#include "scene/background_construction.h"
#include "scene/background_update.h"
#include "scene/scene_gameplay_time.h"
#include "audio/actor_sound_tick.h"
#include "scene/background_parts.h"
#include "entities/runtime_list.h"
#include "entities/physical_shape_events.h"
#include "navigation/grid_regions.h"
#include "entities/ghouly_spawn_record.h"
#include "navigation/mind_navigation_angles.h"
#include "entities/body_damage_motion.h"
#include "entities/body_contacts.h"
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
        else if (mode == 84)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            const u32 subtype = read_u32(parameters, 0);
            RuntimeScoreboard board{};
            for (size_t index = 0; index < 3; ++index)
            {
                RuntimeScoreboardMessage message{};
                message.id = read_u32(bytes, 0x600 + index * 0x94);
                message.word_18 = read_u32(bytes, 0x618 + index * 0x94);
                board.messages.push_back(message);
            }
            if (subtype == 1)
            {
                set_runtime_scoreboard_message_warning(read_u32(parameters, 4),
                    read_u32(parameters, 8), board);
                for (size_t index = 0; index < 3; ++index)
                {
                    std::memcpy(bytes.data() + 0x618 + index * 0x94,
                        &board.messages[index].word_18, 4);
                }
            }
            else
            {
                board.messages.clear();
                RuntimeScoreboard* current_board = &board;
                AnimationResourceRegistry registry;
                std::array<RuntimePowerupRow, 20> rows;
                static_assert(sizeof(RuntimePowerupRow) == 0x18);
                std::memcpy(rows.data(), bytes.data(), sizeof(rows));
                RuntimePowerupTable table{rows, current_board, registry};
                i32 count = i32(read_u32(bytes, 0x420));
                std::array<u32, 4> actors;
                std::array<float, 4> times;
                std::memcpy(actors.data(), bytes.data() + 0x400, 16);
                std::memcpy(times.data(), bytes.data() + 0x410, 16);
                const float freeze = read_f32(parameters, 0x20);
                const float dt = read_f32(parameters, 0x24);
                const float scale = read_f32(parameters, 0x28);
                const u32 blocked = read_u32(parameters, 0x2C);
                const u32 zero = 0;
                RuntimeActorClock clock{freeze, dt, scale, blocked,
                    std::span<const u8>{parameters}.subspan(0x30, 1), zero};
                const std::function<void(u32, std::span<const u32>)> message =
                    [&](u32 actor, std::span<const u32> words)
                    {
                        std::memcpy(bytes.data() + 0x500, words.data(), 36);
                        std::memcpy(bytes.data() + 0x524, &actor, 4);
                        const u32 calls = read_u32(bytes, 0x528) + 1;
                        std::memcpy(bytes.data() + 0x528, &calls, 4);
                    };
                RuntimeSceneScareTimers context{table, count, actors, times, clock, message};
                if (subtype == 0)
                {
                    remove_runtime_scared_actor(read_u32(parameters, 4), context);
                }
                else
                {
                    tick_runtime_scene_scare_timers(context);
                }
                std::memcpy(bytes.data(), rows.data(), sizeof(rows));
                std::memcpy(bytes.data() + 0x400, actors.data(), 16);
                std::memcpy(bytes.data() + 0x410, times.data(), 16);
                std::memcpy(bytes.data() + 0x420, &count, 4);
            }
            result = true;
        }
        else if (mode == 92)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            std::array<const u32*, 3> programs;
            for (u32 index = 0; index < programs.size(); ++index)
            {
                programs[index] = reinterpret_cast<const u32*>(bytes.data() + 0x100 + index * 0x100);
            }
            u32 size = read_u32(bytes, 0xB80), count = read_u32(bytes, 0xB84);
            const u32 value = splice_runtime_vertex_shaders(reinterpret_cast<u32*>(bytes.data() + 0x600),
                read_u32(parameters, 4) != 0 ? &size : nullptr,
                read_u32(parameters, 8) != 0 ? &count : nullptr,
                programs.data(), read_u32(parameters, 0), 0, nullptr);
            std::memcpy(bytes.data() + 0xB80, &size, 4);
            std::memcpy(bytes.data() + 0xB84, &count, 4);
            std::memcpy(bytes.data() + 0xB88, &value, 4);
            result = true;
        }
        else if (mode == 93 || mode == 94)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            const auto append = [&](const void* source, size_t size)
            {
                const auto* begin = static_cast<const u8*>(source);
                graph_bytes.insert(graph_bytes.end(), begin, begin + size);
            };
            const auto word = [&](u32 value)
            {
                append(&value, 4);
            };
            RuntimeRelocationImage image{bytes, 0x10000000u, [&](u32 address)
            {
                if (address >= 0x0032E558 && address < 0x0032E7B8)
                {
                    return parameters.data() + 0x400 + address - 0x0032E558;
                }
                if (address >= 0x10002000 && address < 0x10003000)
                {
                    return parameters.data() + address - 0x10002000;
                }
                return bytes.data() + u32(address - 0x10000000u);
            }};
            auto cache = std::make_unique<RuntimeVertexShaderCache>();
            std::memcpy(&cache->entries[0], bytes.data() + 0x100, 0x944);
            RuntimeModelRenderContext renderer;
            auto fragments = std::make_unique<RuntimeShaderFragmentTable>();
            initialize_runtime_lighting(renderer, *fragments);
            renderer.lighting_flags = read_u32(parameters, 8);
            RuntimeShaderFragmentContext fragment_context{renderer, fragments->buckets, fragments->programs};
            u32 fragment_word = read_u32(parameters, 0x18);
            RuntimeModelVertexShaderCacheContext context{*cache, image, fragment_context, fragment_word,
                [&](const i32* declaration, const u32* program, u32& handle, u32 usage)
                {
                    word(0x1732A0);
                    word(handle);
                    word(usage);
                    append(declaration, 0x80);
                    append(program, size_t(1 + (program[0] >> 16) * 4) * 4);
                    const u32 calls = read_u32(bytes, 0xB84) + 1;
                    std::memcpy(bytes.data() + 0xB84, &calls, 4);
                    if (read_u32(parameters, 0x10) == 0)
                    {
                        handle = read_u32(parameters, 0xC);
                    }
                }};
            if (mode == 93)
            {
                const u32 handle = read_u32(parameters, 0) == 0 ?
                    select_runtime_model_vertex_shader(0, read_u32(parameters, 4), context) :
                    create_runtime_model_vertex_shader(cache->entries[0], read_u32(parameters, 4), context);
                std::memcpy(bytes.data() + 0xB80, &handle, 4);
            }
            else
            {
                std::array<RuntimeModelRenderFrame, 1> frames{};
                renderer.frames = frames;
                std::array<RuntimeShaderMatrix, 1> combined;
                RuntimeShaderMatrix projection, camera;
                std::memcpy(combined[0].data(), parameters.data() + 0x240, 0x40);
                std::memcpy(projection.data(), parameters.data() + 0x200, 0x40);
                std::memcpy(camera.data(), parameters.data() + 0x280, 0x40);
                std::array<const RuntimeShaderMatrix*, 1> cameras{&camera};
                std::array<ModelTreeStackEntry, 4> stack;
                i32 stack_index = -1;
                u32 retained = 0, fog = 0, cull = 0;
                u16 mask = 0;
                const std::unordered_map<u32, std::function<void(u32, void*)>> callbacks;
                ModelTreeDispatchContext tree{renderer, image, stack, stack_index, combined,
                    cameras, retained, mask, fog, cull, callbacks};
                u16 projection_register = image.half(0x10002020), world_register = image.half(0x10002022);
                RuntimeShaderMatrixBoundaries matrices;
                matrices.transpose = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
                {
                    const auto source = input;
                    for (u32 row = 0; row < 4; ++row)
                    {
                        for (u32 column = 0; column < 4; ++column)
                        {
                            output[row * 4 + column] = source[column * 4 + row];
                        }
                    }
                };
                RuntimeModelTransformDraw transforms{tree, cameras, [&](i32) -> RuntimeShaderMatrix&
                    {
                        return projection;
                    }, projection_register, world_register, matrices,
                    [&](u32 reg, const float* values, u32 vectors)
                    {
                        word(vectors == 4 ? 0x173440 : vectors == 1 ? 0x1733E0 : 0x1735D0);
                        word(reg);
                        word(vectors * 4);
                        append(values, vectors * 16);
                    }};
                std::array<u32, 16> streams;
                std::array<float, 4> shader_parameters;
                std::array<float, 8> auxiliary;
                std::memcpy(streams.data(), parameters.data() + 0x340, sizeof(streams));
                std::memcpy(shader_parameters.data(), parameters.data() + 0x2C0, sizeof(shader_parameters));
                std::memcpy(auxiliary.data(), parameters.data() + 0x2D0, sizeof(auxiliary));
                RuntimeModelVertexShaderDraw draw{transforms, context, streams, shader_parameters, auxiliary,
                    [&](u32 stream, u32 resource, u32 stride)
                    {
                        word(0x173610);
                        word(stream);
                        word(resource);
                        word(stride);
                    },
                    [&](u32 handle)
                    {
                        word(0x1739F0);
                        word(handle);
                        std::memcpy(bytes.data() + 0xB80, &handle, 4);
                    }};
                dispatch_runtime_model_vertex_shader(0x10002800, draw);
                std::memcpy(bytes.data() + 0xB88, &projection_register, 2);
                std::memcpy(bytes.data() + 0xB8A, &world_register, 2);
            }
            std::memcpy(bytes.data() + 0x100, &cache->entries[0], 0x944);
            result = true;
        }
        else if (mode == 91)
        {
            if (read_u32(parameters, 0) == 0)
            {
                unpack_runtime_vertex_instruction(reinterpret_cast<const u32*>(object.bytes.get() + 0x100),
                    reinterpret_cast<u32*>(object.bytes.get() + 0x100 + read_u32(parameters, 4)));
            }
            else
            {
                const u32 type = runtime_vertex_shader_code_type(reinterpret_cast<const u16*>(object.bytes.get() + 0x100));
                std::memcpy(object.bytes.get() + 0xB80, &type, 4);
            }
            result = true;
        }
        else if (mode == 90)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            RuntimeModelRenderContext renderer;
            std::array<RuntimeModelRenderFrame, 3> frames;
            renderer.frames = frames;
            renderer.frame_index = i32(read_u32(bytes, 0x6A4));
            RuntimeRelocationImage image{bytes, 0x10000000u};
            std::array<ModelTreeStackEntry, 8> stack;
            std::memcpy(stack.data(), bytes.data() + 0x710, sizeof(stack));
            i32 stack_index = -1;
            std::array<RuntimeShaderMatrix, 3> combined, projection;
            std::array<const RuntimeShaderMatrix*, 3> cameras;
            for (u32 index = 0; index < frames.size(); ++index)
            {
                const u32 offset = 0x400 + index * 0xC4;
                std::memcpy(combined[index].data(), bytes.data() + offset, 0x40);
                std::memcpy(frames[index].world.data(), bytes.data() + offset + 0x44, 0x40);
                std::memcpy(projection[index].data(), bytes.data() + offset + 0x84, 0x40);
                cameras[index] = reinterpret_cast<const RuntimeShaderMatrix*>(parameters.data() +
                    read_u32(bytes, offset + 0x40) - 0x10002000u);
            }
            u32 retained = 0, fog = 0, cull = 0;
            u16 mask = 0;
            const std::unordered_map<u32, std::function<void(u32, void*)>> callbacks;
            ModelTreeDispatchContext tree{renderer, image, stack, stack_index, combined,
                cameras, retained, mask, fog, cull, callbacks};
            ModelPoseBuffer pose;
            pose.packed_matrices.resize(4);
            std::memcpy(pose.packed_matrices.data(), parameters.data() + 0x400, 0x100);
            renderer.state.skeleton_pose = &pose;
            renderer.state.skeleton_matrices = 0x10000300u;
            u16 projection_register = image.half(0x100006A0), world_register = image.half(0x100006A2);
            RuntimeShaderMatrixBoundaries matrices;
            matrices.transpose = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
            {
                const RuntimeShaderMatrix source = input;
                for (u32 row = 0; row < 4; ++row)
                {
                    for (u32 column = 0; column < 4; ++column)
                    {
                        output[row * 4 + column] = source[column * 4 + row];
                    }
                }
            };
            RuntimeModelTransformDraw context{tree, cameras, [&](i32 index) -> RuntimeShaderMatrix&
                {
                    return projection[index];
                }, projection_register, world_register, matrices,
                [&](u32 reg, const float* values, u32 count)
                {
                    const u32 calls = read_u32(bytes, 0xB80);
                    const u32 offset = 0x800 + calls * 0x48;
                    std::memcpy(bytes.data() + offset, &reg, 4);
                    std::memcpy(bytes.data() + offset + 4, &count, 4);
                    std::memcpy(bytes.data() + offset + 8, values, count * 0x10);
                    const u32 next = calls + 1;
                    std::memcpy(bytes.data() + 0xB80, &next, 4);
                }};
            switch (read_u32(parameters, 0))
            {
            case 0:
                apply_runtime_model_node_transform(0x10000100, parameters, context);
                break;
            case 1:
                dispatch_runtime_model_transform(0x10000100, parameters, context);
                restore_runtime_model_transform(context);
                break;
            case 2:
                dispatch_runtime_model_billboard(0x10000100, parameters, context);
                restore_runtime_model_billboard(context);
                break;
            case 3:
                upload_runtime_model_frame_matrix(combined[renderer.frame_index], context);
                break;
            case 4:
                restore_runtime_model_transform(context);
                break;
            case 5:
                restore_runtime_model_billboard(context);
                break;
            case 6:
            {
                auto& matrix = *reinterpret_cast<RuntimeShaderMatrix*>(bytes.data() + 0x780);
                translate_runtime_model_joint_matrix(0x10000300u, matrix, matrix, image);
                break;
            }
            }
            for (u32 index = 0; index < frames.size(); ++index)
            {
                const u32 offset = 0x400 + index * 0xC4;
                std::memcpy(bytes.data() + offset, combined[index].data(), 0x40);
                const u32 camera = 0x10002000u + u32(reinterpret_cast<const u8*>(cameras[index]) - parameters.data());
                std::memcpy(bytes.data() + offset + 0x40, &camera, 4);
                std::memcpy(bytes.data() + offset + 0x44, frames[index].world.data(), 0x40);
                std::memcpy(bytes.data() + offset + 0x84, projection[index].data(), 0x40);
            }
            std::memcpy(bytes.data() + 0x6A4, &renderer.frame_index, 4);
            std::memcpy(bytes.data() + 0x6A8, &stack_index, 4);
            std::memcpy(bytes.data() + 0x710, stack.data(), sizeof(stack));
            result = true;
        }
        else if (mode == 89)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            RuntimeRelocationImage image{bytes, 0x10000000u, [&](u32 address)
            {
                if (address >= 0x0020A368 && address < 0x0020A4D8)
                {
                    return parameters.data() + 0x400 + address - 0x0020A368;
                }
                if (address >= 0x0020A148 && address < 0x0020A368)
                {
                    return parameters.data() + 0x600 + address - 0x0020A148;
                }
                return bytes.data() + u32(address - 0x10000000u);
            }};
            RuntimeModelRenderContext renderer;
            RuntimeModelInstance model{};
            model.state_1c0 = read_u32(parameters, 0x1C0);
            model.state_1c4 = read_u32(parameters, 0x1C4);
            model.state_1c8 = read_u32(parameters, 0x1C8);
            renderer.active_model = &model;
            renderer.rendering = read_u32(parameters, 0x10);
            std::array<ModelTreeStackEntry, 4> stack{};
            i32 stack_index = -1;
            std::array<RuntimeShaderMatrix, 1> combined{};
            std::array<const RuntimeShaderMatrix*, 1> cameras{};
            u32 retained = 0, fog = 0, cull = 0;
            u16 mask = 0;
            const std::unordered_map<u32, std::function<void(u32, void*)>> callbacks;
            ModelTreeDispatchContext tree{renderer, image, stack, stack_index, combined,
                cameras, retained, mask, fog, cull, callbacks};
            std::array<u32, 0xC0> shadows;
            std::memcpy(shadows.data(), bytes.data() + 0x400, sizeof(shadows));
            u32 dirty = read_u32(bytes, 0x700), stage = read_u32(bytes, 0x704);
            const auto record = [&](u32 method, u32 value)
            {
                u32 checksum = dirty;
                for (u32 index = 0; index < shadows.size(); ++index)
                {
                    checksum += (index + 1) * shadows[index];
                }
                const u32 calls = read_u32(bytes, 0xB80);
                const u32 words[]{method, value, checksum};
                std::memcpy(bytes.data() + 0x800 + calls * 12, words, sizeof(words));
                const u32 next = calls + 1;
                std::memcpy(bytes.data() + 0xB80, &next, 4);
            };
            const auto index = [](u32 address)
            {
                return (address - 0x00181B70) / 4;
            };
            RuntimeModelShaderStates states{shadows[index(0x181C54)], shadows[index(0x181C70)],
                shadows[index(0x181C5C)], shadows[index(0x181C60)], shadows[index(0x181C98)],
                shadows[index(0x181C68)], shadows[index(0x181C6C)], shadows[index(0x181C64)],
                record, [&](u32 value)
                {
                    record(0x1722A0, value);
                }};
            RuntimeModelShaderDraw draw{tree, states, [&](u32 value)
                {
                    record(0x175370, value);
                }, {}};
            RuntimeModelShaderCombiner combiner{draw, shadows, dirty, shadows[index(0x181C44)], stage, record};
            if (read_u32(parameters, 0) == 1)
            {
                configure_runtime_model_texture_combiner(read_u32(parameters, 8), i32(read_u32(parameters, 4)), combiner);
            }
            else
            {
                if (read_u32(parameters, 0xC) != 0)
                {
                    bind_runtime_model_shader_callback([&](u32 shader)
                    {
                        post_runtime_model_shader(shader, combiner);
                    }, draw);
                }
                dispatch_runtime_model_shader(0x10000100, draw);
            }
            std::memcpy(bytes.data() + 0x400, shadows.data(), sizeof(shadows));
            std::memcpy(bytes.data() + 0x700, &dirty, 4);
            std::memcpy(bytes.data() + 0x704, &stage, 4);
            result = true;
        }
        else if (mode == 87 || mode == 88)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            RuntimeModelRenderContext renderer;
            std::array<RuntimeModelRenderFrame, 1> frames{};
            renderer.frames = frames;
            RuntimeRelocationImage image{bytes, 0x10000000u};
            std::array<ModelTreeStackEntry, 4> stack{};
            i32 stack_index = -1;
            std::array<RuntimeShaderMatrix, 1> combined{};
            RuntimeShaderMatrix camera, projection;
            std::memcpy(combined[0].data(), parameters.data() + 0x200, 0x40);
            std::memcpy(camera.data(), parameters.data() + 0x240, 0x40);
            std::memcpy(projection.data(), parameters.data() + 0x200, 0x40);
            std::array<const RuntimeShaderMatrix*, 1> cameras{&camera};
            u32 retained = 0, fog = 0, cull = 0;
            u16 mask = 0;
            const std::unordered_map<u32, std::function<void(u32, void*)>> callbacks;
            ModelTreeDispatchContext tree{renderer, image, stack, stack_index, combined,
                cameras, retained, mask, fog, cull, callbacks};
            RuntimeModelMatrixScratch scratch;
            bind_runtime_model_matrix_scratch(reinterpret_cast<RuntimeShaderMatrix*>(bytes.data() +
                (mode == 87 ? 0x700 : 0x300)), 0x100, scratch);
            std::span<RuntimeShaderMatrix> published = scratch.matrices;
            RuntimeModelMatrixArrayDraw context{tree, scratch, published};
            if (mode == 87)
            {
                ModelPoseBuffer pose;
                pose.packed_matrices.resize(4);
                std::memcpy(pose.packed_matrices.data(), parameters.data() + 0x700, 0x100);
                renderer.state.skeleton_pose = &pose;
                RuntimeModelInstanceStorage storage{};
                storage.bytes = parameters;
                if (read_u32(parameters, 0) == 0)
                {
                    calculate_runtime_model_matrix_array(0x10000100, &pose, combined[0], camera,
                        parameters, context);
                }
                else
                {
                    dispatch_runtime_model_matrix_array(0x10000100, storage, context);
                }
                const u32 pointer = 0x10000000u + u32(reinterpret_cast<u8*>(published.data()) - bytes.data());
                std::memcpy(bytes.data() + 0xB80, &pointer, 4);
            }
            else
            {
                const std::function<void(u32, const float*, u32)> constants =
                    [&](u32 reg, const float* values, u32 count)
                    {
                        const u32 calls = read_u32(bytes, 0xB80);
                        if (reg == 0x74)
                        {
                            std::memcpy(bytes.data() + 0x800, values, count * 0x10);
                        }
                        else
                        {
                            const size_t offset = 0x840 + size_t(calls - 1) * 0x34;
                            std::memcpy(bytes.data() + offset, &reg, 4);
                            std::memcpy(bytes.data() + offset + 4, values, count * 0x10);
                        }
                        const u32 next = calls + 1;
                        std::memcpy(bytes.data() + 0xB80, &next, 4);
                    };
                RuntimeShaderMatrixBoundaries boundaries;
                boundaries.transpose = [](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
                {
                    const RuntimeShaderMatrix source = input;
                    for (size_t row = 0; row < 4; ++row)
                    {
                        for (size_t column = 0; column < 4; ++column)
                        {
                            output[row * 4 + column] = source[column * 4 + row];
                        }
                    }
                };
                RuntimeModelRigidSkinDraw skin{context, projection, boundaries, constants};
                dispatch_runtime_model_rigid_skin(0x10000100, nullptr, skin);
            }
            result = true;
        }
        else if (mode == 86)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            RuntimeModelRenderContext renderer;
            RuntimeRelocationImage image{bytes, 0x10000000u};
            std::array<ModelTreeStackEntry, 4> stack{};
            i32 stack_index = -1;
            std::array<RuntimeShaderMatrix, 1> combined{};
            std::array<const RuntimeShaderMatrix*, 1> cameras{};
            u32 retained = 0, fog = 0, cull = 0;
            u16 mask = 0;
            const std::unordered_map<u32, std::function<void(u32, void*)>> callbacks;
            ModelTreeDispatchContext tree{renderer, image, stack, stack_index, combined,
                cameras, retained, mask, fog, cull, callbacks};
            const std::function<ModelPoseBuffer*(void*, u32)> pose = [](void* storage, u32 offset)
            {
                return reinterpret_cast<ModelPoseBuffer*>(uintptr_t(read_u32(
                    {static_cast<const u8*>(storage), 0x1000}, offset)));
            };
            RuntimeModelSkeletonDraw context{tree, pose};
            dispatch_runtime_model_skeleton(0x10000100u, parameters.data(), context);
            const u32 pointer = u32(reinterpret_cast<uintptr_t>(renderer.state.skeleton_pose));
            std::memcpy(bytes.data() + 0xB80, &pointer, 4);
            std::memcpy(bytes.data() + 0xB84, &renderer.state.skeleton_matrices, 4);
            std::memcpy(bytes.data() + 0xB88, &renderer.state.skeleton_joint_count, 4);
            result = true;
        }
        else if (mode == 85)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            std::array<ActorEffectNode, 4> effects{};
            std::array<ActorEffectNode*, 150> nodes{};
            for (size_t index = 0; index < 4; ++index)
            {
                effects[index].bytes = bytes.subspan(0x100 + index * 0x100, 0x100);
                effects[index].binding_actor = reinterpret_cast<void*>(uintptr_t(
                    read_u32(effects[index].bytes, 0xC0)));
            }
            for (size_t index = 0; index < 150; ++index)
            {
                const u8 slot = parameters[0x100 + index];
                nodes[index] = slot == 4 ? nullptr : &effects[slot];
            }
            detach_actor_from_global_effects(reinterpret_cast<void*>(uintptr_t(read_u32(parameters, 0))), nodes);
            result = true;
        }
        else if (mode >= 80 && mode <= 83)
        {
            auto bytes = std::span<u8>{object.bytes.get(), object.size};
            const i32 pal = i32(read_u32(parameters, 0x10));
            const i32 override_pal = i32(read_u32(parameters, 0x14));
            const float freeze = read_f32(parameters, 0x20);
            const float dt = read_f32(parameters, 0x24);
            const float scale = read_f32(parameters, 0x28);
            const u32 zero = 0;
            RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            i32 free_head = i32(read_u32(parameters, 4));
            if (mode == 80)
            {
                const u32 subtype = read_u32(parameters, 0);
                if (subtype == 0)
                {
                    const float value = approach_runtime_scalar(read_f32(parameters, 0x30),
                        read_f32(parameters, 0x34), read_f32(parameters, 0x38));
                    std::memcpy(bytes.data() + 0xB80, &value, 4);
                }
                else if (subtype == 1)
                {
                    float current = read_f32(bytes, 0x100);
                    float rate = read_f32(bytes, 0x104);
                    approach_runtime_scalar_rate(current, rate, read_f32(parameters, 0x34), dt);
                    std::memcpy(bytes.data() + 0x100, &current, 4);
                    std::memcpy(bytes.data() + 0x104, &rate, 4);
                }
                else
                {
                    set_runtime_bounded_channel_limits(bytes.subspan(0x100, 0x14),
                        read_f32(parameters, 0x30), read_f32(parameters, 0x34), pal, override_pal);
                }
            }
            else if (mode == 81)
            {
                RuntimeEffectDebris context{bytes, std::span<const u8>{parameters}.subspan(0x400),
                    free_head, zero, random};
                tick_effect_debris(bytes.subspan(0, 0x74), context, clock);
                std::memcpy(bytes.data() + 0xB80, &free_head, 4);
            }
            else
            {
                RuntimeEffectTrails context{bytes, std::span<const u8>{parameters}.subspan(0x400),
                    free_head, random, pal, override_pal, [&](u32 address)
                    {
                        return std::span<const u8>{parameters}.subspan(address - 0x10002000);
                    }};
                if (mode == 82)
                {
                    set_effect_trail_phase(bytes.subspan(0, 0x84), i32(read_u32(parameters, 0)), context);
                }
                else
                {
                    tick_effect_trail(bytes.subspan(0, 0x84), context, clock);
                }
                std::memcpy(bytes.data() + 0xB80, &free_head, 4);
            }
            result = true;
        }
        else if (mode == 79)
        {
            std::array<RuntimeActorSound, 3> sounds{};
            RuntimeActorSoundGroup group{};
            const auto at = [&](u32 address) -> RuntimeSoundLink*
            {
                if (address == 0)
                {
                    return nullptr;
                }
                return address == 0x10000700 ? &group.pending : &sounds[(address - 0x10000100u) / 0x100u];
            };
            group.pending.next = at(read_u32({object.bytes.get(), object.size}, 0x704));
            for (size_t index = 0; index < 3; ++index)
            {
                const size_t offset = 0x100 + index * 0x100;
                const std::span<const u8> bytes{object.bytes.get() + offset, 0xB4};
                sounds[index].previous = at(read_u32(bytes, 0));
                sounds[index].next = at(read_u32(bytes, 4));
                sounds[index].position = {read_f32(bytes, 0x94), read_f32(bytes, 0x98), read_f32(bytes, 0x9C)};
                sounds[index].elapsed = read_f32(bytes, 0xAC);
            }
            sort_pending_actor_sounds(group, {read_f32(parameters, 0), read_f32(parameters, 4), read_f32(parameters, 8)});
            const auto address = [&](RuntimeSoundLink* link) -> u32
            {
                if (link == nullptr)
                {
                    return 0;
                }
                if (link == &group.pending)
                {
                    return 0x10000700;
                }
                return 0x10000100u + u32(static_cast<RuntimeActorSound*>(link) - sounds.data()) * 0x100u;
            };
            const u32 head = address(group.pending.next);
            std::memcpy(object.bytes.get() + 0x704, &head, 4);
            for (size_t index = 0; index < 3; ++index)
            {
                const size_t offset = 0x100 + index * 0x100;
                const u32 previous = address(sounds[index].previous);
                const u32 next = address(sounds[index].next);
                std::memcpy(object.bytes.get() + offset, &previous, 4);
                std::memcpy(object.bytes.get() + offset + 4, &next, 4);
                std::memcpy(object.bytes.get() + offset + 0xAC, &sounds[index].elapsed, 4);
            }
            result = true;
        }
        else if (mode == 78)
        {
            const float freeze = read_f32(parameters, 4);
            const float dt = read_f32(parameters, 8);
            const float scale = read_f32(parameters, 12);
            const u32 zero = 0;
            const RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            advance_scene_gameplay_time({object.bytes.get(), 0x14}, clock, read_u32(parameters, 0));
            result = true;
        }
        else if (mode == 77)
        {
            Actor actor;
            actor.animation_rate = read_f32(parameters, 0x20);
            actor.animation_pause = read_f32(parameters, 0x24);
            ActorMatrixSetup matrices{};
            matrices.matrices.count = 3;
            matrices.matrices.node = std::span<const u8>{parameters}.subspan(0x300);
            matrices.matrices.instance = {object.bytes.get() + 0x800, 0x200};
            RuntimeBackgroundLifetime lifetime{};
            lifetime.animation = object.bytes.get() + 0x100;
            RuntimeBackgroundAnimation animation{};
            std::array<RuntimePointGraph, 3> graphs{};
            for (size_t index = 0; index < graphs.size(); ++index)
            {
                graphs[index].fields = {object.bytes.get() + 0x100 + index * 0x1E0, 0x1E0};
            }
            animation.rows = graphs;
            animation.animation_block = std::span<const u8>{parameters}.subspan(0x400);
            animation.header = std::span<const u8>{parameters}.subspan(0x500);
            animation.matrix_count = 3;
            animation.matrix_node = matrices.matrices.node;
            RuntimeBackgroundConstruction background{{object.bytes.get(), object.size}, actor, matrices,
                lifetime, animation, {}, {}};
            const float freeze = read_f32(parameters, 0x40);
            const float dt = read_f32(parameters, 0x44);
            const float scale = read_f32(parameters, 0x48);
            const u32 zero = 0;
            const RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            const std::function<std::span<const u8>(u32, size_t)> resolve = [&](u32 address, size_t size)
            {
                return std::span<const u8>{parameters}.subspan(address - 0x10002000u, size);
            };
            RuntimeShaderMatrixBoundaries boundaries{};
            RuntimeBackgroundUpdate context{&background, clock, resolve, nullptr, boundaries};
            update_background_animation(background, context);
            result = true;
        }
        else if (mode == 72 || mode == 73)
        {
            i32 free_head = i32(read_u32(parameters, 0));
            const u32 frame = read_u32(parameters, 4);
            if (mode == 72)
            {
                std::vector<u8> particles(50 * 0x74);
                for (size_t index = 0; index < 50; ++index)
                {
                    std::memcpy(particles.data() + index * 0x74 + 0x70,
                        parameters.data() + 0x800 + index * 4, 4);
                }
                RuntimeEffectDebris context{particles, {}, free_head, frame, random};
                const u32 index = u32(oldest_effect_debris(context));
                std::memcpy(object.bytes.get() + 0xB80, &index, 4);
            }
            else
            {
                RuntimeEffectDebris context{{object.bytes.get(), object.size}, {}, free_head, frame, random};
                const auto particle = allocate_effect_debris(context);
                const u32 address = particle.empty() ? 0 : 0x10000000u + u32(particle.data() - object.bytes.get());
                std::memcpy(object.bytes.get() + 0xB80, &free_head, 4);
                std::memcpy(object.bytes.get() + 0xB84, &address, 4);
            }
            result = true;
        }
        else if (mode == 70 || mode == 71 || mode == 74 || mode == 75 || mode == 76)
        {
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
            RuntimeActorSoundQueues sounds{};
            Vec3 origin{};
            ActorEffectStreamContext effects{models, sounds, random, origin, origin};
            AudioCoreSources sources{};
            AudioCoreCues cues{sources};
            AudioEventVoices voices{cues, {}, {}, {}, 0, origin, origin};
            i32 free_head = -1;
            const u32 frame = 0;
            RuntimeEffectDebris debris{{}, {}, free_head, frame, random};
            FootstepSoundNames footsteps{};
            RuntimeEffectCollision collision{};
            collision.kind = read_u32(parameters, 0);
            u32 callback = read_u32(parameters, 4);
            RuntimeEffectCollisionCommands context{collision, debris, voices, effects, footsteps, callback};
            ActorEffectNode owner{};
            owner.bytes = {object.bytes.get() + 0x400, 0x390};
            if (mode >= 74)
            {
                owner.bytes = {object.bytes.get(), object.size};
            }
            owner.script = std::span<const u8>{parameters}.subspan(0x400);
            const std::span<u8> particle{object.bytes.get() + 0x100, 0x150};
            u32 returned = 0;
            if (mode == 71 || mode >= 74)
            {
                const float freeze = read_f32(parameters, 0x20);
                const float dt = read_f32(parameters, 0x24);
                const float scale = read_f32(parameters, 0x28);
                const u32 zero = 0;
                const RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
                entity_detail::DisplayView view{};
                const u8 bounded = 0;
                RuntimeEffectCollisionContext movement{collision, clock, view, origin, bounded, origin, origin, random};
                const std::function<std::span<const u8>(u32)> resolve = [&](u32 address)
                {
                    return std::span<const u8>{parameters}.subspan(address - 0x10002000u);
                };
                const i32 pal = i32(read_u32(parameters, 0x10));
                const i32 override_pal = i32(read_u32(parameters, 0x14));
                RuntimeEffectSimulation simulation{movement, context, resolve, pal, override_pal};
                if (mode >= 74)
                {
                    const std::function<u32(const u8*)> address = [&](const u8* pointer)
                    {
                        return 0x10000000u + u32(pointer - object.bytes.get());
                    };
                    const Vec3 direction{read_f32(parameters, 0x90), read_f32(parameters, 0x94), read_f32(parameters, 0x98)};
                    RuntimeEffectPassScratch pass_scratch{{read_f32(parameters, 0x80), read_f32(parameters, 0x84), read_f32(parameters, 0x88)},
                        read_u32({object.bytes.get(), object.size}, 0xB88), read_f32({object.bytes.get(), object.size}, 0xB8C)};
                    u32 cooldown = 0;
                    RuntimeEffectTickContext tick{simulation, frame, cooldown, address, direction, pass_scratch};
                    if (mode == 74)
                    {
                        returned = tick_actor_effect(owner, read_f32(parameters, 0x18), tick);
                    }
                    else if (mode == 75)
                    {
                        tick_active_actor_effect_particles(owner, read_f32(parameters, 0x18), tick);
                    }
                    else
                    {
                        spawn_and_simulate_actor_effect_particle(owner, read_f32(parameters, 0x18), tick);
                    }
                }
                else
                {
                    u32 collided = read_u32({object.bytes.get(), object.size}, 0xB88);
                    float previous = read_f32({object.bytes.get(), object.size}, 0xB8C);
                    returned = simulate_actor_effect_particle(particle, owner, read_f32(parameters, 0x18),
                        collided, previous, simulation);
                    std::memcpy(object.bytes.get() + 0xB88, &collided, 4);
                    std::memcpy(object.bytes.get() + 0xB8C, &previous, 4);
                }
            }
            else if (read_u32(parameters, 8) != 0)
            {
                notify_effect_particle_collision(particle, owner, context);
            }
            else
            {
                returned = execute_effect_collision_commands(particle, owner, context);
            }
            std::memcpy(object.bytes.get() + 0xB80, &callback, 4);
            std::memcpy(object.bytes.get() + 0xB84, &returned, 4);
            result = true;
        }
        else if (mode == 68)
        {
            Vec3 source, position, euler;
            std::memcpy(&source, object.bytes.get() + 0x100, 12);
            const Vec3 normal{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
            sample_effect_collision_pose(source, normal, position, euler);
            std::memcpy(object.bytes.get() + 0x800, &position, 12);
            std::memcpy(object.bytes.get() + 0x810, &euler, 12);
            result = true;
        }
        else if (mode == 69)
        {
            i32 free_head = -1;
            const u32 frame = read_u32(parameters, 4);
            RuntimeEffectDebris context{{}, std::span<const u8>{parameters}.subspan(0x400), free_head, frame, random};
            set_effect_debris_phase({object.bytes.get() + 0x100, 0x74}, i32(read_u32(parameters, 0)), context);
            result = true;
        }
        else if (mode == 67)
        {
            ActorEffectNode owner{};
            owner.bytes = {object.bytes.get() + 0x400, 0x390};
            const std::function<std::span<const u8>(u32)> resolve = [&](u32 address)
            {
                return std::span<const u8>{parameters}.subspan(address - 0x10002000u);
            };
            u32 cursor = read_u32({object.bytes.get(), object.size}, 0x24C);
            const i32 pal = i32(read_u32(parameters, 0));
            const i32 override_pal = i32(read_u32(parameters, 4));
            const u32 returned = execute_actor_effect_commands({object.bytes.get() + 0x100, 0x150}, owner,
                cursor, resolve, random, pal, override_pal);
            std::memcpy(object.bytes.get() + 0x24C, &cursor, 4);
            std::memcpy(object.bytes.get() + 0xB80, &returned, 4);
            result = true;
        }
        else if (mode == 64)
        {
            entity_detail::DisplayView view{};
            std::memcpy(view.transform.data(), parameters.data() + 0x100, 48);
            view.pixel_aspect = read_f32(parameters, 0x80);
            view.focal_pixels = read_f32(parameters, 0x84);
            view.fitted_width = i32(read_u32(parameters, 0x88));
            view.fitted_height = i32(read_u32(parameters, 0x8C));
            const Vec3 plane{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
            Vec3 source, candidate, normal;
            std::array<float, 2> screen;
            std::memcpy(&source, object.bytes.get() + 0x100, 12);
            std::memcpy(&candidate, object.bytes.get() + 0x800, 12);
            std::memcpy(&normal, object.bytes.get() + 0x810, 12);
            std::memcpy(screen.data(), object.bytes.get() + 0x820, 8);
            const u32 options = read_u32(parameters, 0);
            Vec3* normal_output = (options & 1) != 0 ? ((options & 8) != 0 ? &candidate : &normal) : nullptr;
            const u32 hit = clip_runtime_camera_plane(view, plane, source, candidate, normal_output,
                (options & 2) != 0 ? &screen : nullptr);
            std::memcpy(object.bytes.get() + 0x800, &candidate, 12);
            std::memcpy(object.bytes.get() + 0x810, &normal, 12);
            std::memcpy(object.bytes.get() + 0x820, screen.data(), 8);
            std::memcpy(object.bytes.get() + 0xB80, &hit, 4);
            result = true;
        }
        else if (mode == 65)
        {
            const u32 subtype = read_u32(parameters, 0);
            const i32 pal = i32(read_u32(parameters, 4));
            const i32 override_pal = i32(read_u32(parameters, 8));
            const Vec3 first{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
            const Vec3 second{read_f32(parameters, 0x20), read_f32(parameters, 0x24), read_f32(parameters, 0x28)};
            const Vec3 rates{read_f32(parameters, 0x30), read_f32(parameters, 0x34), read_f32(parameters, 0x38)};
            const Vec3 position{read_f32(parameters, 0x40), read_f32(parameters, 0x44), read_f32(parameters, 0x48)};
            u32 returned = 0;
            if (subtype == 0)
            {
                returned = runtime_wrapped_angles_equal(first.x, second.x);
            }
            else if (subtype == 1)
            {
                set_runtime_bounded_channel_limits({object.bytes.get() + 0x100, 0x3C}, first, second, pal, override_pal);
            }
            else if (subtype == 2)
            {
                set_runtime_bounded_channel_rates({object.bytes.get() + 0x100, 0x3C}, rates, pal, override_pal);
            }
            else
            {
                configure_actor_effect_bounded_channels({object.bytes.get() + 0x100, 0x150}, first, second,
                    position, rates, i32(read_u32(parameters, 12)), pal, override_pal, random);
            }
            std::memcpy(object.bytes.get() + 0xB80, &returned, 4);
            result = true;
        }
        else if (mode == 66)
        {
            entity_detail::NavGrid grid;
            grid.width = 2;
            grid.depth = 2;
            grid.cell = 5;
            grid.cells.resize(4);
            std::memcpy(grid.cells.data(), parameters.data() + 0x800, 0x48);
            entity_detail::g_nav_grids[0] = &grid;
            entity_detail::g_nav_count = 1;
            entity_detail::g_nav_min_y[0] = 0.f;
            entity_detail::g_nav_max_y[0] = 10.f;
            entity_detail::g_nav_origin_x = 0;
            entity_detail::g_nav_origin_z = 0;
            entity_detail::g_world.segment = nullptr;
            RuntimeEffectCollision collision{read_u32({object.bytes.get(), object.size}, 0xB80),
                object.bytes[0xB84], read_u32({object.bytes.get(), object.size}, 0xB88), {}};
            std::memcpy(&collision.normal, object.bytes.get() + 0xB8C, 12);
            const float freeze = read_f32(parameters, 0x20);
            const float dt = read_f32(parameters, 0x24);
            const float scale = read_f32(parameters, 0x28);
            const u32 zero = 0;
            const RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            entity_detail::DisplayView view{};
            std::memcpy(view.transform.data(), parameters.data() + 0x100, 48);
            const Vec3 plane{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
            const Vec3 minimum{read_f32(parameters, 0x50), read_f32(parameters, 0x54), read_f32(parameters, 0x58)};
            const Vec3 maximum{read_f32(parameters, 0x60), read_f32(parameters, 0x64), read_f32(parameters, 0x68)};
            const u8 bounded = u8(read_u32(parameters, 4));
            RuntimeEffectCollisionContext context{collision, clock, view, plane, bounded, minimum, maximum, random};
            std::function<void()> callback;
            if (read_u32(parameters, 8) != 0)
            {
                callback = [&]
                {
                    const auto bytes = std::span<u8>{object.bytes.get(), object.size};
                    const u32 count = read_u32(bytes, 0xBA0) + 1;
                    std::memcpy(object.bytes.get() + 0xBA0, &count, 4);
                    const float factor = 2.f;
                    std::memcpy(object.bytes.get() + 0x140, &factor, 4);
                    object.bytes[0x156] |= 4;
                };
            }
            const u32 hit = move_actor_effect_particle({object.bytes.get() + 0x100, 0x150}, context, callback);
            std::memcpy(object.bytes.get() + 0xB80, &collision.kind, 4);
            object.bytes[0xB84] = collision.surface;
            std::memcpy(object.bytes.get() + 0xB88, &collision.flags, 4);
            std::memcpy(object.bytes.get() + 0xB8C, &collision.normal, 12);
            std::memcpy(object.bytes.get() + 0xB9C, &hit, 4);
            auto& pool = entity_detail::g_segment_slots[0].pool;
            if (pool.live)
            {
                engine_heap_free(pool.block);
                pool = {};
            }
            entity_detail::g_segment_count = 0;
            entity_detail::g_nav_grids[0] = nullptr;
            entity_detail::g_nav_count = 0;
            result = true;
        }
        else if (mode == 63)
        {
            entity_detail::NavGrid grid;
            grid.width = 2;
            grid.depth = 2;
            grid.cell = 5;
            grid.cells.resize(4);
            std::memcpy(grid.cells.data(), parameters.data() + 0x800, 0x48);
            entity_detail::g_nav_grids[0] = read_u32(parameters, 0x10) != 0 ? &grid : nullptr;
            entity_detail::g_nav_count = 1;
            entity_detail::g_nav_min_y[0] = 0.f;
            entity_detail::g_nav_max_y[0] = 10.f;
            entity_detail::g_nav_origin_x = 0;
            entity_detail::g_nav_origin_z = 0;
            entity_detail::g_world.segment = nullptr;
            RuntimeEffectCollision collision{read_u32({object.bytes.get(), object.size}, 0xB80),
                object.bytes[0xB84], read_u32({object.bytes.get(), object.size}, 0xB88), {}};
            std::memcpy(&collision.normal, object.bytes.get() + 0xB8C, 12);
            Vec3 source, candidate;
            std::memcpy(&source, object.bytes.get() + 0x100, 12);
            std::memcpy(&candidate, object.bytes.get() + 0x800, 12);
            u32 hit;
            if (read_u32(parameters, 0) == 0)
            {
                hit = u32(probe_actor_effect_world(source, candidate, read_f32(parameters, 0x20),
                    i32(read_u32(parameters, 8)), read_u32(parameters, 12), read_u32(parameters, 4), collision));
                std::memcpy(object.bytes.get() + 0x100, &source, 12);
            }
            else
            {
                hit = probe_actor_effect_particle({object.bytes.get() + 0x100, 0x150}, candidate, collision);
            }
            std::memcpy(object.bytes.get() + 0x800, &candidate, 12);
            std::memcpy(object.bytes.get() + 0xB80, &collision.kind, 4);
            object.bytes[0xB84] = collision.surface;
            std::memcpy(object.bytes.get() + 0xB88, &collision.flags, 4);
            std::memcpy(object.bytes.get() + 0xB8C, &collision.normal, 12);
            std::memcpy(object.bytes.get() + 0xB9C, &hit, 4);
            auto& pool = entity_detail::g_segment_slots[0].pool;
            if (pool.live)
            {
                engine_heap_free(pool.block);
                pool = {};
            }
            entity_detail::g_segment_count = 0;
            entity_detail::g_nav_grids[0] = nullptr;
            entity_detail::g_nav_count = 0;
            result = true;
        }
        else if (mode == 60)
        {
            const Vec3 normal{read_f32(parameters, 4), read_f32(parameters, 8), read_f32(parameters, 12)};
            reflect_actor_effect_particle({object.bytes.get() + 0x100, 0x150}, read_u32(parameters, 0), normal, random);
            result = true;
        }
        else if (mode == 61)
        {
            const u32 subtype = read_u32(parameters, 0);
            u32 returned;
            if (subtype == 0)
            {
                const Vec3 direction{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
                float pitch = read_f32({object.bytes.get(), object.size}, 0x800);
                returned = runtime_direction_pitch(direction, pitch);
                std::memcpy(object.bytes.get() + 0x800, &pitch, 4);
            }
            else if (subtype == 1)
            {
                returned = std::bit_cast<u32>(advance_runtime_random_fraction(random));
            }
            else if (subtype == 2)
            {
                returned = actor_effect_channels_are_active({object.bytes.get() + 0x100, 0x3C});
            }
            else
            {
                returned = actor_effect_command_condition(parameters, {object.bytes.get() + 0x100, 0x150});
            }
            std::memcpy(object.bytes.get() + 0xB80, &returned, 4);
            result = true;
        }
        else if (mode == 62)
        {
            const u32 subtype = read_u32(parameters, 0);
            auto particle = std::span<u8>{object.bytes.get() + 0x100, 0x150};
            if (subtype == 0)
            {
                target_actor_effect_color(particle, parameters[4], parameters[5], parameters[6], read_f32(parameters, 0x20));
            }
            else if (subtype == 1)
            {
                target_actor_effect_alpha(particle, parameters[4], read_f32(parameters, 0x20));
            }
            else
            {
                const Vec3 rate{read_f32(parameters, 0x10), read_f32(parameters, 0x14), read_f32(parameters, 0x18)};
                const Vec3 phase{read_f32(parameters, 0x20), read_f32(parameters, 0x24), read_f32(parameters, 0x28)};
                const Vec3 lower{read_f32(parameters, 0x40), read_f32(parameters, 0x44), read_f32(parameters, 0x48)};
                const Vec3 upper{read_f32(parameters, 0x50), read_f32(parameters, 0x54), read_f32(parameters, 0x58)};
                configure_actor_effect_oscillation(particle, rate, phase, lower, upper, i32(read_u32(parameters, 8)), random);
            }
            result = true;
        }
        else if (mode == 58)
        {
            tick_actor_effect_oscillation({object.bytes.get() + 0x100, 0x3C}, read_f32(parameters, 0));
            result = true;
        }
        else if (mode == 59)
        {
            ActorEffectNode effect{};
            effect.bytes = {object.bytes.get(), object.size};
            const std::function<std::span<const u8>(u32)> resolve = [&](u32 address)
            {
                return std::span<const u8>{parameters}.subspan(address - 0x10002000u);
            };
            const u32 fallback = find_actor_effect_fallback_command(
                std::span<const u8>{object.bytes.get() + 0x400, 0x150}, effect, resolve);
            std::memcpy(object.bytes.get() + 0xB80, &fallback, 4);
            result = true;
        }
        else if (mode == 57)
        {
            const u32 subtype = read_u32(parameters, 0);
            Actor actor;
            actor.position = {read_f32(parameters, 0x20), read_f32(parameters, 0x24), read_f32(parameters, 0x28)};
            actor.euler = {read_f32(parameters, 0x30), read_f32(parameters, 0x34), read_f32(parameters, 0x38)};
            std::memcpy(actor.xform, parameters.data() + 0x80, 48);
            ActorModel model;
            model.cpu = std::span<const u8>{parameters}.subspan(0x400);
            RuntimeModelInstance runtime{};
            std::array<ModelPoseBuffer, 2> poses;
            std::array<EntityBonePose, 2> shape_poses;
            for (size_t index = 0; index < 2; ++index)
            {
                poses[index].dirty = true;
                poses[index].matrices.resize(1);
                std::memcpy(poses[index].matrices[0].data(), parameters.data() + 0x100 + index * 0x40, 48);
                shape_poses[index].buffer = &poses[index];
            }
            runtime.primary = &poses[0];
            runtime.alternate = read_u32(parameters, 12) != 0 ? &poses[1] : nullptr;
            model.primary_shape_pose = &shape_poses[0];
            model.alternate_shape_pose = read_u32(parameters, 12) != 0 ? &shape_poses[1] : nullptr;
            Joint joint{};
            joint.world = {read_f32(parameters, 0x180), read_f32(parameters, 0x184), read_f32(parameters, 0x188)};
            RuntimeModelNode skeleton{};
            skeleton.joints = {&joint, 1};
            runtime.skeleton_node = &skeleton;
            model.runtime = &runtime;
            actor.model = read_u32(parameters, 8) != 0 ? &model : nullptr;
            const Actor* owner = read_u32(parameters, 4) != 0 ? &actor : nullptr;
            ActorEffectNode effect{};
            effect.bytes = {object.bytes.get(), object.size};
            effect.binding_actor = const_cast<Actor*>(owner);
            effect.binding_joint = i32(read_u32(effect.bytes, 0xD8));
            const auto pointer = [&](u32 value) -> const u8*
            {
                return value != 0 ? model.cpu.data() + value - 0x10002400u : nullptr;
            };
            effect.binding_shape = pointer(read_u32(effect.bytes, 0xCC));
            effect.binding_locator = pointer(read_u32(effect.bytes, 0xD4));
            const std::function<u32(const u8*)> address = [&](const u8* value)
            {
                return value != nullptr ? u32(0x10002400 + (value - model.cpu.data())) : 0u;
            };
            Vec3 position, euler;
            std::memcpy(&position, object.bytes.get() + 0x800, 12);
            std::memcpy(&euler, object.bytes.get() + 0x810, 12);
            float radius = read_f32(effect.bytes, 0x820);
            if (subtype == 0)
            {
                sample_actor_effect_shape(owner, read_u32(effect.bytes, 0xC8), effect.binding_shape, position, radius);
            }
            else if (subtype == 1)
            {
                sample_actor_effect_locator(owner, i16(read_u16(effect.bytes, 0xD0)), effect.binding_locator, position, euler);
            }
            else if (subtype == 2)
            {
                sample_actor_effect_joint(owner, effect.binding_joint, position, euler);
            }
            else if (subtype == 3)
            {
                update_actor_effect_size(effect, address);
            }
            else
            {
                prepare_actor_effect_binding(effect, read_f32(parameters, 0x18), address);
            }
            std::memcpy(object.bytes.get() + 0x800, &position, 12);
            std::memcpy(object.bytes.get() + 0x810, &euler, 12);
            std::memcpy(object.bytes.get() + 0x820, &radius, 4);
            const u32 shape = address(effect.binding_shape);
            const u32 locator = address(effect.binding_locator);
            std::memcpy(object.bytes.get() + 0xB80, &shape, 4);
            std::memcpy(object.bytes.get() + 0xB84, &locator, 4);
            result = true;
        }
        else if (mode == 56)
        {
            const u32 subtype = read_u32(parameters, 0);
            Actor actor;
            ActorModel model;
            const auto cpu = std::span<const u8>{parameters}.subspan(0x400);
            model.cpu = read_u32(parameters, 12) != 0 ? cpu : std::span<const u8>{};
            actor.model = read_u32(parameters, 4) != 0 ? &model : nullptr;
            const u32 previous = read_u32(parameters, 8);
            const u8* current = previous != 0 ? cpu.data() + 0x104 + (previous - 1) * 0x34 : nullptr;
            u32 returned = 0;
            if (subtype == 0)
            {
                returned = advance_runtime_model_binding(cpu.data() + 0x100, current, i16(read_u16(parameters, 16)));
            }
            else if (subtype == 1)
            {
                returned = advance_actor_model_binding(actor, current, i16(read_u16(parameters, 16)));
            }
            else if (subtype == 2)
            {
                const u8* block = actor_physical_shape_block(actor);
                returned = block != nullptr ? u32(0x10002400 + (block - cpu.data())) : 0;
            }
            else
            {
                const u8* shape = previous != 0 ? cpu.data() + 0x308 + (previous - 1) * 0x30 : nullptr;
                shape = select_actor_effect_shape(actor, shape, read_u32(parameters, 20));
                returned = shape != nullptr ? u32(0x10002400 + (shape - cpu.data())) : 0;
            }
            const u32 current_address = current != nullptr ? u32(0x10002400 + (current - cpu.data())) : 0;
            std::memcpy(object.bytes.get() + 0xB80, &returned, 4);
            std::memcpy(object.bytes.get() + 0xB84, &current_address, 4);
            result = true;
        }
        else if (mode == 53)
        {
            Vec3 point;
            std::memcpy(&point, object.bytes.get() + 0x800, 12);
            jitter_actor_effect_position(point, parameters, random);
            std::memcpy(object.bytes.get() + 0x800, &point, 12);
            result = true;
        }
        else if (mode == 54)
        {
            initialize_actor_effect_velocity({object.bytes.get() + 0x100, 0x150}, parameters, random);
            result = true;
        }
        else if (mode == 55)
        {
            Vec3 scratch;
            place_actor_effect_particle(parameters, {object.bytes.get() + 0x100, 0x150},
                std::span<const u8>{parameters}.subspan(0x400), scratch, random);
            result = true;
        }
        else if (mode == 52)
        {
            Vec3 extent{read_f32(parameters, 8), read_f32(parameters, 12), read_f32(parameters, 16)};
            Vec3 point;
            Vec3& output = read_u32(parameters, 4) != 0 ? extent : point;
            if (read_u32(parameters, 0) == 0)
            {
                sample_runtime_ellipsoid_position(extent, output, i32(read_u32(parameters, 20)),
                    read_f32(parameters, 24), read_f32(parameters, 28), read_f32(parameters, 32),
                    read_f32(parameters, 36), random);
            }
            else
            {
                sample_runtime_cylinder_position(extent, output, i32(read_u32(parameters, 20)),
                    read_f32(parameters, 24), read_f32(parameters, 28), random);
            }
            std::memcpy(object.bytes.get() + 0x800, &output, 12);
            result = true;
        }
        else if (mode == 50)
        {
            const float angle = sample_runtime_angle_range(read_f32(parameters, 0), read_f32(parameters, 4), random);
            std::memcpy(object.bytes.get() + 0x800, &angle, 4);
            result = true;
        }
        else if (mode == 51)
        {
            Vec3 extent{read_f32(parameters, 8), read_f32(parameters, 12), read_f32(parameters, 16)};
            Vec3 point;
            Vec3& output = read_u32(parameters, 4) != 0 ? extent : point;
            if (read_u32(parameters, 0) == 0)
            {
                sample_runtime_rectangle_position(extent, output, i32(read_u32(parameters, 20)), random);
            }
            else
            {
                sample_runtime_ellipse_position(extent, output, i32(read_u32(parameters, 20)), random);
            }
            std::memcpy(object.bytes.get() + 0x800, &output, 12);
            result = true;
        }
        else if (mode == 49)
        {
            const float freeze = read_f32(parameters, 0);
            const float dt = read_f32(parameters, 4);
            const float scale = read_f32(parameters, 8);
            const u32 zero = 0;
            const RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            approach_actor_effect_channels({object.bytes.get() + 0x100, 0x3C}, clock);
            result = true;
        }
        else if (mode == 48)
        {
            const u32 subtype = read_u32(parameters, 0);
            auto channels = std::span<u8>{object.bytes.get() + 0x100, 0x3C};
            const float step = read_f32(parameters, 4);
            if (subtype == 0)
            {
                decrease_runtime_bounded_channel(channels, step);
            }
            else if (subtype == 1)
            {
                increase_runtime_bounded_channel(channels, step);
            }
            else
            {
                std::array<float, 3> output;
                tick_runtime_bounded_channels(channels, step, read_u32(parameters, 8) != 0 ? output.data() : nullptr);
                if (read_u32(parameters, 8) != 0)
                {
                    std::memcpy(object.bytes.get() + 0x800, output.data(), 12);
                }
            }
            result = true;
        }
        else if (mode == 47)
        {
            std::array<RuntimePointGraph, 3> rows{};
            for (size_t index = 0; index < rows.size(); ++index)
            {
                rows[index].fields = {object.bytes.get() + 0x100 + index * 0x1E0, 0x1E0};
            }
            RuntimeBackgroundAnimation animation{};
            animation.rows = rows;
            animation.matrix_count = i32(read_u32(parameters, 0));
            animation.matrix_node = parameters;
            publish_background_graph_matrices(animation, {object.bytes.get() + 0x800, 0x200});
            result = true;
        }
        else if (mode == 46)
        {
            Vec3 extent{read_f32(parameters, 8), read_f32(parameters, 12), read_f32(parameters, 16)};
            Vec3 point;
            if (read_u32(parameters, 0) == 0)
            {
                sample_runtime_line_position(point, extent.x, random);
            }
            else if (read_u32(parameters, 4) != 0)
            {
                sample_runtime_box_position(extent, extent, i32(read_u32(parameters, 20)), random);
                point = extent;
            }
            else
            {
                sample_runtime_box_position(extent, point, i32(read_u32(parameters, 20)), random);
            }
            std::memcpy(object.bytes.get() + 0x800, &point, 12);
            result = true;
        }
        else if (mode == 45)
        {
            const u32 subtype = read_u32(parameters, 0);
            if (subtype == 0)
            {
                Quat quaternion;
                store_runtime_quaternion(quaternion, read_f32(parameters, 4), read_f32(parameters, 8),
                    read_f32(parameters, 12), read_f32(parameters, 16));
                std::memcpy(object.bytes.get() + 0x800, &quaternion, 16);
            }
            else if (subtype == 3)
            {
                Vec3 angles{read_f32(parameters, 4), read_f32(parameters, 8), read_f32(parameters, 12)};
                wrap_runtime_euler_angles(angles);
                std::memcpy(object.bytes.get() + 0x800, &angles, 12);
            }
            else
            {
                std::array<float, 16> matrix;
                const Quat quaternion{read_f32(parameters, 16), read_f32(parameters, 20),
                    read_f32(parameters, 24), read_f32(parameters, 28)};
                if (subtype == 1)
                {
                    runtime_graphics_quaternion_matrix(matrix.data(), quaternion);
                }
                else
                {
                    compose_runtime_graphics_matrix(matrix.data(),
                        {read_f32(parameters, 4), read_f32(parameters, 8), read_f32(parameters, 12)}, quaternion,
                        {read_f32(parameters, 32), read_f32(parameters, 36), read_f32(parameters, 40)});
                }
                std::memcpy(object.bytes.get() + 0x800, matrix.data(), 64);
            }
            result = true;
        }
        else if (mode == 44)
        {
            ActorEffectNode effect;
            effect.bytes = {object.bytes.get(), object.size};
            const u32 emitted = actor_effect_emission_count(effect, read_f32(parameters, 0));
            std::memcpy(object.bytes.get() + 0xB80, &emitted, 4);
            result = true;
        }
        else if (mode == 43)
        {
            std::array<Actor, 8> actors;
            std::array<RuntimeBodyCollisionOverride, 8> rows;
            for (size_t index = 0; index < actors.size(); ++index)
            {
                actors[index].model_flags = read_u32({object.bytes.get(), object.size}, 0x800 + index * 4);
                const size_t offset = 0x400 + index * 0x10;
                rows[index] = {&actors[(read_u32({object.bytes.get(), object.size}, offset) - 0x10006000u) / 0x240],
                    read_u32({object.bytes.get(), object.size}, offset + 4),
                    read_u32({object.bytes.get(), object.size}, offset + 8),
                    read_u32({object.bytes.get(), object.size}, offset + 0xC)};
            }
            GhoulySpawnPool pool{};
            pool.stride = sizeof(RuntimeBodyCollisionOverride);
            pool.begin = reinterpret_cast<u8*>(rows.data());
            pool.cursor = pool.begin + read_u32(parameters, 8) * pool.stride;
            pool.limit = pool.begin + rows.size() * pool.stride;
            auto* optional = read_u32(parameters, 4) != 0 ? &pool : nullptr;
            u32 returned = 0;
            if (read_u32(parameters, 0) == 0)
            {
                returned = expire_runtime_body_contact_overrides(optional, read_f32(parameters, 0xC));
            }
            else
            {
                const u32 target = read_u32(parameters, 0xC);
                forget_runtime_body_contact_actor(optional, target != 8 ? &actors[target] : nullptr);
            }
            const u32 count = optional != nullptr ? u32((pool.cursor - pool.begin) / pool.stride) : 0;
            std::memcpy(object.bytes.get() + 0xB80, &returned, 4);
            std::memcpy(object.bytes.get() + 0xB84, &count, 4);
            for (size_t index = 0; index < rows.size(); ++index)
            {
                const auto& row = rows[index];
                const std::array<u32, 4> words{0x10006000u + u32(row.actor - actors.data()) * 0x240u,
                    row.saved, row.retained, row.active};
                std::memcpy(object.bytes.get() + 0x400 + index * 0x10, words.data(), 16);
                std::memcpy(object.bytes.get() + 0x800 + index * 4, &actors[index].model_flags, 4);
            }
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
                std::array<BackgroundAnimationVolume, 1> rows{};
                std::memcpy(rows[0].fields.data(), object.bytes.get(), 0x9C);
                advance_background_part_stage(actor, (read_u32(parameters, 8) & 2) != 0 ? &secondary : nullptr,
                    rows, 0, 0, std::span<const u8>(parameters).subspan(0x700),
                    position, source, {reinterpret_cast<u32*>(object.bytes.get() + 0x940), 32}, context);
                std::memcpy(object.bytes.get(), rows[0].fields.data(), 0x9C);
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
                std::vector<BackgroundAnimationVolume> rows(read_u32(parameters, 4));
                for (size_t row = 0; row < rows.size(); ++row)
                {
                    std::memcpy(rows[row].fields.data(), parameters.data() + 0x400 + row * 0x9C, 0x9C);
                }
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
            const i32 found = find_runtime_aid_list_entry(read_u32(parameters, 0x400) != 0
                ? std::span<const u8>{parameters}.subspan(0x440) : std::span<const u8>{},
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
            ActorEffectNode* effect = nullptr;
            if (subtype == 2 || (subtype == 3 && (flags & 8) != 0))
            {
                effect = new (allocate(0x10009000, u32(sizeof(ActorEffectNode)))) ActorEffectNode{};
                effect->sound.enabled = parameters[0x20];
                effect->sound.previous_bone = read_u16(parameters, 0x22);
            }
            // The released node is freed, so its sound words are captured on the way out.
            RuntimeActorSound released_sound{};
            const std::function<void(u8*)> effect_free = [&](u8* pointer)
            {
                if (effect != nullptr && pointer == reinterpret_cast<u8*>(effect))
                {
                    std::memcpy(&released_sound.enabled, pointer + offsetof(ActorEffectNode, sound)
                        + offsetof(RuntimeActorSound, enabled), 1);
                    std::memcpy(&released_sound.previous_bone, pointer + offsetof(ActorEffectNode, sound)
                        + offsetof(RuntimeActorSound, previous_bone), 2);
                }
                heap_free(pointer);
            };
            RuntimeModelSoundRelease model_sound{sounds, registry, deferred, fence, shaders, effect_free};
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
                release_actor_effect_node(*effect, model_sound);
                object.bytes[0x750] = released_sound.enabled;
                std::memcpy(object.bytes.get() + 0x752, &released_sound.previous_bone, 2);
            }
            else
            {
                RuntimeBackgroundLifetime background{};
                background.events = pool;
                background.first_push_buffer = (flags & 2) != 0 ? allocate(0x10007000, 16) : nullptr;
                background.second_push_buffer = (flags & 4) != 0 ? allocate(0x10008000, 16) : nullptr;
                background.has_push_buffer = i32(read_u32({object.bytes.get(), object.size}, 0x9E0));
                background.effect = (flags & 8) != 0 ? effect : nullptr;
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
