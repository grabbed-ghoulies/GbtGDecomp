#include "scene_actor_services_state.h"
#include "../graphics/scene_d3d_present.h"
#include "../graphics/boot_shader_tables.h"
#include "../camera/wide_aspect.h"

// Actor drawing, view presentation, render targets, textures and the inspector's views.
namespace gbtg
{
// Actor vtable +0x14 (draw) for the factory class of a native actor.
void SceneActorServices::State::actor_draw(Actor& actor, i32 view, i32 pass)
{
    for (const auto& [bytes, native] : actors->actors)
    {
        if (native == &actor)
        {
            FactoryObject* object = object_of(bytes);
            const u16 type = object == nullptr ? 0 : object->type;
            // Type 0x15 (vtable 0x00504C34) +0x14 is the button bat draw 0x0006D6C0.
            if (type == 0x15)
            {
                if (actor.model == nullptr || actor.model->runtime == nullptr)
                {
                    throw Error("button bat draw has no model");
                }
                RuntimeModelInstance& instance = *actor.model->runtime;
                flag_origins[&instance] = instance.node_flags.data();
                auto& view = actors->view(*object);
                draw_runtime_button_bat(view.bat, view.animation, instance,
                    resources.memory.address_of(instance.source->cpu.data()), u32(pass), *button_bat_draw);
                return;
            }
            // Type 4 (vtable 0x004E1644) +0x14 is the background draw 0x00042C10: the actor draw unless
            // DAT_004EE1EC holds a dialogue page.
            if (type == 4)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                ActorFrameDrawContext attachments{attachment_draws};
                const i32 mode = game.script_dialogue == nullptr ? 0 : game.script_dialogue->dialogue.state.mode;
                draw_runtime_background(mode, state, pass, attachments, *model_draw);
                return;
            }
            // Type 5 +0x14 is the player draw 0x00028050 (getter 0x00027F60).
            if (type == 5)
            {
                draw_player(*object, players->player(*object), pass);
                return;
            }
            // Type 0xD +0x14 is the FX emitter draw 0x00109EE0 (getter 0x00109DD0).
            if (type == 0xD)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                ActorFrameDrawContext attachments{attachment_draws};
                RuntimeFxEmitterDraw draw{state, attachments, *model_draw, g_billboard_queue};
                classes->draw_emitter(*object, pass, draw);
                return;
            }
            // Type 0x12 +0x14 is the bone debris draw 0x000686A0 (getter 0x00067E60).
            if (type == 0x12)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                const u32 material_second = read_u32(data(0x00427424, 4), 0);
                RuntimeBoneDebrisDraw draw{state.model, material_second, *model_draw};
                classes->draw_bone_debris(*object, pass, draw);
                return;
            }
            // Type 0x1C +0x14 is 0x000702C0: the scare turns to the camera, then the actor draw 0x0003C9E0; with
            // the camera inside it draws nothing.
            if (type == 0x1C)
            {
                if (classes->face_scare(*object))
                {
                    draw_scene_actor(*object, actor, pass);
                }
                return;
            }
            // Type 0x13 +0x14 is 0x0006C7C0: the light's glow billboard, then the actor draw 0x0003C9E0.
            if (type == 0x13)
            {
                classes->queue_animated_light_glow(*object);
                draw_scene_actor(*object, actor, pass);
                return;
            }
            // Type 0x21 +0x14 is the Super Scary dropper's glow draw 0x00071DF0 (getter 0x00071820).
            if (type == 0x21)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                ActorFrameDrawContext attachments{attachment_draws};
                classes->draw_sss_dropper(state, pass, attachments, *model_draw, *glow_pass);
                return;
            }
            // Type 0x1D +0x14 is the scare figure draw 0x0006F600 (getter 0x0006EFB0).
            if (type == 0x1D)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                ActorFrameDrawContext attachments{attachment_draws};
                classes->draw_scare_figure(*object, state, pass, attachments, *model_draw, *glow_pass);
                return;
            }
            // Type 0x10 +0x14 is the after-image draw 0x00069410 (getter 0x000690A0).
            if (type == 0x10)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                const u32 material_second = read_u32(data(0x00427428, 4), 0);
                RuntimeAvatarAfterimageDraw draw{{object->bytes.get(), object->size}, actor, state.model,
                    material_second, *model_draw, *glow_pass};
                draw_runtime_avatar_afterimage(pass, draw);
                return;
            }
            // Type 0xA8 +0x14 is the homing projectile's draw 0x000CD090 (getter 0x000CC790): no weapon prompt.
            if (type == 0xA8)
            {
                std::unique_ptr<ActorDrawModelView> model;
                ActorDrawState state = actor_draw_state(*object, actor, model);
                ActorFrameDrawContext attachments{attachment_draws};
                classes->draw_homing_projectile(*object, state, pass, attachments, *model_draw, *glow_pass);
                return;
            }
            // Types 0xAA and 0xA9 +0x14 is the weapon draw 0x001459C0: the actor draw 0x0003C9E0, then the hand prompt.
            if (type == 0xAA || type == 0xA9 || type == 0xA7 || type == 0xA0 || type == 0xA1 || type == 0xA2
                || type == 0xA3 || type == 0xA5 || type == 0xA6)
            {
                if (!classes->weapon_draw_hidden(*object))
                {
                    draw_scene_actor(*object, actor, pass);
                    classes->draw_weapon_prompt(*object, pass); // 0x00147560
                }
                if (type == 0xA3 || type == 0xA5 || type == 0xA6)
                {
                    classes->draw_gun_effects(*object, u32(pass)); // 0x000CAD90 after 0x001459C0
                }
                if (type == 0xA1)
                {
                    classes->queue_candle_flame(*object); // 0x000C6BE0's 0x000C6C80 after 0x001459C0
                }
                return;
            }
            // Type 0x1A (vtable 0x00504DD8) +0x14 is 0x0006EF20, a jump to the actor draw 0x0003C9E0; types 2, 0x1F
            // and 0x23 keep the base slot 0x0003C9E0 (getters 0x0003C080, 0x0006DBD0 and 0x00069620), as do the
            // ghouly setups 0x24, 0x25 and 0x26, the bg-break key 0x1E, the ghouly key 0x18 and the spider web 0x1B
            // (getters 0x0006A4F0, 0x0006B110, 0x0006BB30, 0x0006CC70, 0x0006E950 and 0x000704D0).
            // Type 0x16's and 0x17's +0x14 are 0x0006EF20 as well.
            if (type != 0x1A && type != 2 && type != 0x14 && type != 0x16 && type != 0x17 && type != 0xF && type != 0x1F
                && type != 0x23
                && type != 0x25 && type != 0x24 && type != 0x26 && type != 0x1E && type != 0x18 && type != 0x19
                && type != 0x1B && type != 0x20) // 0x20: the SSS door, getter 0x000711A0.
            {
                char vtable[16];
                std::snprintf(vtable, sizeof(vtable), "0x%08X",
                    object == nullptr || object->vtable == nullptr ? 0u : object->vtable->address);
                unbound("actor draw (vtable " + std::string(vtable) + " +0x14) for factory type " + std::to_string(type) + ", view "
                    + std::to_string(view) + ", pass " + std::to_string(pass));
            }
            draw_scene_actor(*object, actor, pass);
            return;
        }
    }
    throw Error("actor draw for an actor outside the scene");
}

// 0x0003C9E0 over a native actor: its frame, model (+0x204), draw flags (+0x197, +0x298), material words
// (+0x198 / +0x19C) and draw callbacks (+0x1A0 / +0x1A4, payload +0x1A8).
void SceneActorServices::State::draw_scene_actor(FactoryObject& object, Actor& actor, i32 kind)
{
    std::unique_ptr<ActorDrawModelView> model;
    ActorDrawState state = actor_draw_state(object, actor, model);
    ActorFrameDrawContext attachments{attachment_draws};
    draw_runtime_actor(state, kind, attachments, *model_draw);
}

// Player vtable +0x14 (0x00028050): the actor draw, the body overlay (0x0001DF70), then the body +0x744
// object's vtable +0x18.
void SceneActorServices::State::draw_player(FactoryObject& object, NativePlayerInstance& player, i32 kind)
{
    player.refresh_views();
    std::unique_ptr<ActorDrawModelView> model;
    ActorDrawState state = actor_draw_state(object, player.actor, model);
    ActorFrameDrawContext attachments{attachment_draws};
    if (display_view == nullptr)
    {
        throw Error("player draw has no DAT_005077A8 display view");
    }
    RuntimeBodyOverlay overlay{player.body, player.actor, player.actor_bytes, player.attribute_resource,
        player.attached, player.health_textures[0].data(), player.health_textures[1].data(),
        player.health_textures[2].data(), player.health_textures[3].data(), overlay_digits.data(), *display_view,
        g_billboard_queue};
    RuntimePlayerDraw draw{state, attachments, *model_draw, g_flags_ea9c, overlay,
        player.second_action.bytes.get(),
        [this, &player](void*, i32 kind)
        {
            if (player.second_action.type == 0x6F)
            {
                // entityStateGenerateGhouly (getter 0x00093B60) draws its bolts at +0x18 (0x00094130).
                auto& graph = body_messages[player.body.data()];
                if (graph == nullptr)
                {
                    graph = std::make_unique<SceneBodyMessages>(*body_services, player);
                }
                draw_runtime_generate_ghouly_state(
                    body_state_view<GenerateGhoulyState>({player.second_action.bytes.get(), player.second_action.size}),
                    kind,
                    graph->generate_ghouly_context());
                return;
            }
            // States 0x5E (getter 0x00088520), 0x62 (entityStateDie, getter 0x0008C1E0), 0x64 (getter 0x0008C940), 0x73 (entityStateHit, getter 0x00095740), 0x74
            // (entityStateHitFurnace, getter 0x000966B0), 0x7A (getter 0x0009C570), 0x7C (getter 0x0009DDA0), 0x87 (entityStateStandAlert,
            // getter 0x000A2530), 0x8E (entityStateWalkAlert, getter 0x000A55F0), 0x8F (getter 0x000A6220), 0x7E
            // (entityStateScoopAndWhack, getter 0x0009EB40) and 0x5F (entityStateArcAttack, getter 0x000889B0) have the
            // empty slot 0x001628B0 at
            // +0x18; 0x88 (entityStateStruggle, getter 0x000A2B30) has the empty 0x000D4179 case there.
            const u32 type = player.second_action.type;
            if (type != 0x5E && type != 0x62 && type != 0x64 && type != 0x73 && type != 0x74 && type != 0x7A && type != 0x7C && type != 0x7D
                && type != 0x87 && type != 0x88 && type != 0x8A && type != 0x8B && type != 0x8E && type != 0x8F && type != 0x69 && type != 0x70
                && type != 0x66 && type != 0x78 && type != 0x5F && type != 0x80 && type != 0x7F && type != 0x83
                && type != 0x7E && type != 0x6B && type != 0x77 && type != 0x79 && type != 0x8D
                && type != 0x6E && type != 0x6A && type != 0x6D && type != 0x81 && type != 0x76 && type != 0x7B
                && type != 0x67 // Getter 0x0008E010 keeps the empty 0x001628B0 at +0x18 too.
                && type != 0x63 // So does getter 0x0008C5A0.
                && type != 0x84 // And getter 0x000A1450.
                && type != 0x86 // And getter 0x000A21F0.
                && type != 0x89 // And getter 0x000A31A0.
                && type != 0x75 // And getter 0x00096A50.
                && type != 0x8C // And getter 0x000A43A0.
                && type != 0x85 // And getter 0x000A1A10.
                && type != 0x90 // And getter 0x000A6A00.
                && type != 0x65 // And getter 0x0008D350.
                && type != 0x82 // And getter 0x000A03D0.
                && type != 0x61 // And getter 0x0008B200.
                && type != 0x60 // And getter 0x0008A300.
                && type != 0x71 // And getter 0x00095550.
                && type != 0x68 // And getter 0x0008E760.
                && type != 0x72) // And getter 0x000947A0.
            {
                unbound("body +0x744 object vtable +0x18 draw for factory type "
                    + std::to_string(player.second_action.type));
            }
        }};
    draw_runtime_player({object.bytes.get(), object.size}, kind, draw);
}

// Actor vtable +0x18 by class. Types 2, 5, 0x15, 0x18, 0x1A, 0x1B, 0x1F, 0x23 and 0xAA keep the base actor slot
// 0x0003C830 (getters 0x0003C080, 0x00027F60, 0x0006D4A0, 0x0006E950, 0x0006EE20, 0x000704D0, 0x0006DBD0, 0x00069620
// and 0x000CEF70); the FX emitter (type 0xD, getter
// 0x00109DD0) uses 0x00109E30.
u32 SceneActorServices::State::actor_visible(Actor& actor, i32 pass, i32 view)
{
    FactoryObject& object = object_of_actor(actor);
    if (object.type == 0xD)
    {
        return classes->emitter_visible(object, u32(pass), u32(view)) ? 1u : 0u;
    }
    if (object.type == 0x1C)
    {
        // The scare's getter 0x0006FCC0 puts 0x000CFFD0 at +0x18: it returns 1, always visible.
        return 1;
    }
    if (object.type != 2 && object.type != 5 && object.type != 0xF && object.type != 0x10 && object.type != 0x12
        && object.type != 0x13 && object.type != 0x14 && object.type != 0x15 && object.type != 0x16
        && object.type != 0x17
        && object.type != 0x1A && object.type != 0x1F && object.type != 0x23 && object.type != 0x25
        && object.type != 0x24 && object.type != 0x26 && object.type != 0x1E && object.type != 0x1D
        && object.type != 0x18 && object.type != 0x19 && object.type != 0x1B && object.type != 0x21 && object.type != 0x20
        && object.type != 0xAA
        && object.type != 0xA9 && object.type != 0xA8 && object.type != 0xA7 && object.type != 0xA0
        && object.type != 0xA1
        && object.type != 0xA2 && object.type != 0xA3 && object.type != 0xA5 && object.type != 0xA6)
    {
        unbound("actor vtable +0x18 visibility for factory type " + std::to_string(object.type));
    }
    if (display_view == nullptr)
    {
        throw Error("actor visibility has no DAT_005077A8 display view");
    }
    // Actor +0x288..+0x2A0 lives on the class's native instance (the bone debris keeps the base slot 0x0003C830).
    ActorMatrixSetup* matrices = actor_matrix_setup(object);
    RuntimeActorViewVisibility visibility{actor, {object.bytes.get(), object.size}, matrices, *display_view};
    return runtime_actor_in_view(visibility, pass, view);
}

// 0x0011C240's actor bins for one view: 0x00121730 and 0x00121880 over DAT_00511AA8[view], 0x00121900 (passes 0
// and 1 with its world helpers), then 0x00121D40 (pass 2) over DAT_00511B1C and that bin.
void SceneActorServices::State::refresh_ambient_context()
{
    for (size_t index = 0; index < ambient_grids.size(); ++index)
    {
        ambient_grids[index] = sector_grids[index].data();
    }
    ambient_context->grid_count = sector_count;
    ambient_context->scale = {g_scene_ambient_tint_0, g_scene_ambient_tint_1, g_scene_ambient_tint_2};
    ambient_context->points_enabled = ambient_points_enabled;
}

#if defined(GBTG_INSPECTOR_APP)
void SceneActorServices::State::present_view(i32 view)
{
    with_view_context(view, present_runtime_scene_view);
}

void SceneActorServices::State::with_view_context(i32 view,
    const std::function<void(RuntimeSceneViewPresent&)>& draw)
#else
void SceneActorServices::State::present_view(i32 view)
#endif
{
    if (display_view == nullptr)
    {
        throw Error("view actor draw has no DAT_005077A8 display view");
    }
    refresh_ambient_context();
    const auto entry = [this](entity_detail::ListHead& node) -> RuntimeSceneBinEntry
    {
        for (auto& [native, registration] : actors->render)
        {
            for (size_t index = 0; index < std::size(registration->bins); ++index)
            {
                if (&registration->bins[index] == &node)
                {
                    FactoryObject& object = object_of_actor(*native);
                    return RuntimeSceneBinEntry{*native, registration->bin_distances[index],
                        *reinterpret_cast<u16*>(object.bytes.get() + 0x194)};
                }
            }
        }
        throw Error("view bin record has no scene actor");
    };
    RuntimeSceneViewActors context{*display_view, entry,
        [this](entity_detail::ListHead& node) -> Actor&
        {
            if (auto* background = backgrounds->owner_of(node); background != nullptr)
            {
                return background->state.actor;
            }
            throw Error("scene +0x934 list node is not owned by a native background");
        },
        [this](Actor& actor, i32 pass, i32 slot)
        {
#if defined(GBTG_INSPECTOR_APP)
            try
            {
                return actor_visible(actor, pass, slot);
            }
            catch (const std::exception& error)
            {
                if (game.inspection != nullptr && game.inspection->drawing)
                {
                    const u32 address = address_of(&actor);
                    const auto found = game.inspection->owners.find(address);
                    if (found != game.inspection->owners.end())
                    {
                        throw SceneInspectionFailure(found->second, "visibility / actor " + std::to_string(address), error.what());
                    }
                }
                throw;
            }
#else
            return actor_visible(actor, pass, slot);
#endif
        },
        [this](Actor& actor, i32 slot, i32 pass)
        {
#if defined(GBTG_INSPECTOR_APP)
            try
            {
                const size_t first_command = d3d_device.commands.size();
                actor_draw(actor, slot, pass);
                if (game.inspection != nullptr && game.inspection->drawing)
                {
                    for (size_t command = first_command; command < d3d_device.commands.size(); ++command)
                    {
                        inspection_draw_owners[command] = address_of(&actor);
                    }
                }
            }
            catch (const std::exception& error)
            {
                if (game.inspection != nullptr && game.inspection->drawing)
                {
                    const u32 address = address_of(&actor);
                    const auto found = game.inspection->owners.find(address);
                    if (found != game.inspection->owners.end())
                    {
                        throw SceneInspectionFailure(found->second, "drawing / actor " + std::to_string(address), error.what());
                    }
                }
                throw;
            }
#else
            actor_draw(actor, slot, pass);
#endif
        }};
    const auto first_background_model = [this]() -> std::span<const u8>
    {
        // DAT_00511B20 -> object +0x204 -> its resource header.
        auto* background = backgrounds->first_background();
        if (background == nullptr || background->state.actor.model == nullptr)
        {
            return {};
        }
        return background->state.actor.model->cpu;
    };
    RuntimeSceneViewPasses passes{context,
        [](Actor& actor, u32 mirror)
        {
            if (actor.model != nullptr && actor.model->runtime != nullptr)
            {
                actor.model->runtime->mirror_pass = mirror;
            }
        },
        [](Actor& actor) -> u32
        {
            if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
            {
                throw Error("view pass reads the type-1 flags of an actor without a +0x204 model");
            }
            const auto cpu = actor.model->cpu;
            const u32 table = read_u32(cpu, 0);
            for (u32 index = 0; index < read_u32(cpu, 4); ++index)
            {
                if (read_u32(cpu, table + index * 8) == 1)
                {
                    return read_u32(cpu, table + index * 8 + 4);
                }
            }
            return 0;
        },
        [this](Actor& actor, u8 slot, u8 value)
        {
            set_runtime_model_node_state(*actor.model->runtime, slot, value, node_cache);
        },
        [this]()
        {
            select_runtime_background_shader_parameters(render_image);
        },
        [this]()
        {
            select_runtime_model_shader_parameters(render_image);
        },
        g_transition_guard,
        [this]()
        {
            draw_runtime_blob_shadows(blob_shadows, blob_shadow_suppress, *point_light_pass); // 0x00119AD0
        },
        [this, first_background_model]()
        {
            RuntimeLightVolumeOrder order{g_light_filled, g_light_filled_gate, g_light_order};
            RuntimeBackgroundClothDraw cloth{*point_light_pass, *display_setup, shadow_projector,
                cloth_ramp_texture, cloth_water_phase,
                [this](u32 address)
                {
                    return resources.memory.pointer(address);
                },
                [this]()
                {
                    return background_cloth_scene().background_model();
                }};
            RuntimeLightVolumeCasters casters{*model_draw, cull_mode, candle_spot, light_volume_texture,
                light_volume_surface,
                [this](RuntimeModelInstance& model)
                {
                    flag_origins[&model] = model.node_flags.data();
                    return resources.memory.address_of(model.source->cpu.data());
                }};
            const u32 color = u32(g_light_volume_color[0]) | u32(g_light_volume_color[1]) << 8
                | u32(g_light_volume_color[2]) << 16 | u32(g_light_volume_color[3]) << 24; // DAT_0050D760
            RuntimeLightVolumeDraw volumes{order, first_background_model(), cloth, casters, color,
                [this]()
                {
                    bind_display_projection(*display_setup);
                }};
            draw_runtime_light_volumes(volumes); // 0x000FE960
        },
        [this]()
        {
            RuntimeBackgroundClothDraw projector{*point_light_pass, *display_setup, shadow_projector,
                cloth_ramp_texture, cloth_water_phase,
                [this](u32 address)
                {
                    return resources.memory.pointer(address);
                },
                [this]()
                {
                    return background_cloth_scene().background_model();
                }};
            draw_runtime_actor_snapshot_decals(decal_enable, actor_snapshots, projector); // 0x00116460
        },
        [this]()
        {
            draw_runtime_scene_point_lights(*point_light_pass); // 0x001237E0
        },
        [this, first_background_model](u32 layer)
        {
            if (first_background_model().data() != nullptr && background_cloths.created)
            {
                RuntimeBackgroundClothDraw draw{*point_light_pass, *display_setup, shadow_projector,
                    cloth_ramp_texture, cloth_water_phase,
                    [this](u32 address)
                    {
                        return resources.memory.pointer(address);
                    },
                    [this]()
                    {
                        return background_cloth_scene().background_model();
                    }};
                draw_runtime_background_cloths(layer, background_cloths.list, draw); // 0x00115A80
            }
        },
        [this]()
        {
            RuntimeDoorShadowDraw shadow{door_pool, render_image, d3d_device.dialogue(),
                *model_shader_states, deferred_states,
                [this]()
                {
                    // The null SetModelView disables the fixed-function model/view override.
                    d3d_device.set_state(0x10070, 0);
                },
                [this](u32 index)
                {
                    bind_runtime_boot_vertex_shader(index, *boot_vertex_shaders);
                },
                [this](u32 handle)
                {
                    d3d_device.dialogue().pixel_shader(handle);
                },
                [this](std::span<const u8> row, const float* view, const float* projection)
                {
                    draw_runtime_door_shadow(row, view, projection, display_view->position,
                        render_image, d3d_device.dialogue());
                }};
            draw_runtime_door_shadows(shadow);
        }};
    passes.mirror_side = [this](const Vec3& position)
    {
        return runtime_mirror_side(position, mirrors);
    };
    passes.radius = [this](Actor& actor)
    {
        FactoryObject& object = object_of_actor(actor);
        return runtime_actor_view_radius(actor, {object.bytes.get(), object.size});
    };
    passes.mirrors = &mirrors;
    RuntimeSceneViewBins bins{passes, entity_detail::g_world.bins, entity_detail::g_world.body_a, view, mirror_pass,
        [this]()
        {
            RuntimePostModelContext context = post_model_context();
            draw_runtime_post_models(context); // 0x0012AB10
        }};
    RuntimeSceneMirrorPass mirror{mirrors, g_flag_10b4, *display_setup, d3d_device.outer(), cull_mode,
        [&bins](i32 flag)
        {
            draw_runtime_view_bins(flag, bins);
        }};
    BillboardFlushContext billboards{g_billboard_queue, *dialogue_draw, d3d_device.outer(), render_image};
    RuntimeSceneViewPresent present{
        [&mirror]()
        {
            draw_runtime_scene_mirror(mirror);
        },
        [this, &context, view]()
        {
            refresh_runtime_view_actors(entity_detail::g_world.body_a, entity_detail::g_world.bins[size_t(view)], view,
                [this](Actor& actor, i32)
                {
                    view_refresh_slot(actor);
                }, context);
        },
        [&bins]()
        {
            draw_runtime_view_bins(0, bins);
        },
        [this]()
        {
            draw_anchor_sprites();
        },
        [this]()
        {
            draw_scene_controls();
        },
        [this]()
        {
            // DAT_00506930..DAT_00506C50: 0x000ED390 for each nonzero word, all 200.
            for (ActorEffectNode* effect : g_actor_effect_nodes)
            {
                if (effect != nullptr)
                {
                    const u32 model_cpu = effect->model == nullptr ? 0u
                        : resources.memory.address_of(effect->model->source->cpu.data());
                    draw_actor_effect_particles(*effect, g_billboard_queue, *display_setup, *model_draw,
                        model_cpu); // 0x000ED390
                }
            }
        },
        [this]()
        {
            draw_effect_debris(debris_pool->particles == nullptr ? std::span<const EffectDebrisRecord>{}
                : std::span<const EffectDebrisRecord>(debris_pool->particles, kEffectDebrisCount),
            debris_pool->textures, *display_setup);
        },
        [this]()
        {
            // 0x000EBCB0: live type-2 trail records queue billboards through 0x0005C1A0.
            if (trail_pool->particles != nullptr)
            {
                const std::span<const EffectTrailRecord> particles(trail_pool->particles, kEffectTrailCount);
                for (const EffectTrailRecord& particle : particles)
                {
                    if (particle.next_free == -2 && particle.phase == 2)
                    {
                        unbound("trail billboard queue 0x000EBCB0 body");
                    }
                }
            }
        },
        [&billboards]()
        {
            flush_billboards(billboards);
        },
        [this]()
        {
            // 0x0011AF90 draws only with cones in use (DAT_00510560) and a first background (DAT_00511B20).
            if (light_cones.count != 0 && backgrounds->first_background() != nullptr)
            {
                RuntimeBackgroundClothDraw projector{*point_light_pass, *display_setup, shadow_projector,
                    cloth_ramp_texture, cloth_water_phase,
                    [this](u32 address)
                    {
                        return resources.memory.pointer(address);
                    },
                    [this]()
                    {
                        return background_cloth_scene().background_model();
                    }};
                RuntimeLightConeDraw draw{light_cones, projector,
                    [this](const char* name)
                    {
                        return acquire_named_animation_resource(resources.registry, name, resources.error);
                    }};
                draw_runtime_light_cones(draw); // 0x0011AF90
            }
        },
        [this]()
        {
            RuntimeBackgroundClothDraw draw{*point_light_pass, *display_setup, shadow_projector, cloth_ramp_texture,
                cloth_water_phase,
                [this](u32 address)
                {
                    return resources.memory.pointer(address);
                },
                [this]()
                {
                    return background_cloth_scene().background_model();
                }};
            RuntimeShadowVolumes volumes{background_regions, background_region_count, shadow_volume_targets,
                g_transition_guard};
            draw_runtime_shadow_volumes(volumes, draw); // 0x00119230

        },
        [this]()
        {
            RuntimeBackgroundClothDraw draw{*point_light_pass, *display_setup, shadow_projector, cloth_ramp_texture,
                cloth_water_phase,
                [this](u32 address)
                {
                    return resources.memory.pointer(address);
                },
                [this]()
                {
                    return background_cloth_scene().background_model();
                }};
            draw_runtime_refraction(g_scene_refraction, g_flag_0650, draw); // 0x0011BA50
        },
        [this]()
        {
            draw_screen_effects(RuntimeScreenEffects{screen_tint, pulse, g_transition_guard, screen_lines,
                glow_targets.page_texture, resources.memory.address_of(glow_targets.page_texture),
                read_u32(data(kScreenQuadVertexShader.handle, 4), 0), shader_constants_at(kScreenQuadVertexShader.constants),
                read_u32(data(kBlurQuadVertexShader.handle, 4), 0), shader_constants_at(kBlurQuadVertexShader.constants),
                read_u32(data(kBlurPixelShader, 4), 0), game.clock.time},
                *dialogue_outer); // 0x000615E0
        },
        [this]()
        {
            draw_scene_dialogue_page(game);
        },
        g_app_error.page,
        [this]()
        {
            draw_app_error_page(i32(dialogue_outer->current_display_address - 0x507018u) / 0x1E4); // DAT_005077A8
        },
        g_app_error.widget,
        []()
        {
            draw_app_error_prompts();
        },
        frontend_state,
        g_challenge_run.widget,
        [this](RuntimeDialogueWidget& widget)
        {
#if !defined(GBTG_RETAIL_GAME_ONLY)
            // The wide_aspect plugin's HUD shape (camera/wide_aspect.h), but not while a book shows (a
            // transition's or the pause's): the page it captures (0x00048550) is widened again by the book's redraw.
            const HudAspectScope hud(g_capture_book == nullptr && g_scene_pause.book == nullptr);
#endif
            draw_runtime_dialogue_widget(widget, 0, *dialogue_outer); // 0x00142720(0), EAX DAT_004E19A0
        },
        [this]()
        {
#if !defined(GBTG_RETAIL_GAME_ONLY)
            const HudAspectScope hud(g_capture_book == nullptr && g_scene_pause.book == nullptr);
#endif
            flush_scene_dialogue_sprites(game);
        },
        [this]()
        {
#if !defined(GBTG_RETAIL_GAME_ONLY)
            const HudAspectScope hud(g_capture_book == nullptr && g_scene_pause.book == nullptr);
#endif
            draw_runtime_scoreboard_hud(*scoreboard_hud); // 0x00132290
        },
        g_flags_ea9c,
        [this]()
        {
            draw_scene_pause(*pause_services); // 0x0011DB50
        },
        g_capture_book,
        [this](RuntimeTransitionBook& book)
        {
            draw_transition_book(book, *book_services); // 0x00048550
        },
        [this]()
        {
            draw_scene_transition_view(); // 0x0012C490
        },
        [this]()
        {
#if !defined(GBTG_RETAIL_GAME_ONLY)
            const HudAspectScope hud(g_capture_book == nullptr && g_scene_pause.book == nullptr);
#endif
            draw_runtime_dialogue_text_pools(*text_registry, g_flags_ea9c, *dialogue_draw); // 0x0005F330
        }};
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // The wide_aspect plugin's hook for the pause's page (wide_aspect.h).
    hold_wide_aspect_for_paused_page(g_scene_pause.book != nullptr,
        book_services->scene_paused != nullptr ? *book_services->scene_paused : 0,
        book_services->pause_phase != nullptr ? *book_services->pause_phase : 0);
#endif
#if defined(GBTG_INSPECTOR_APP)
    draw(present);
#else
    present_runtime_scene_view(present);
#endif
#if !defined(GBTG_RETAIL_GAME_ONLY)
    hold_wide_aspect_for_page(false);
#endif
}

// Actor vtable +0x38, which 0x00121DC0 calls when set. Getters 0x0003C080 (type 2, the base every class below
// starts from, clears it), 0x00042BC0 (4), 0x00027F60 (5), 0x00109DD0 (0xD), 0x0006D4A0 (0x15), 0x0006EE20 (0x1A),
// 0x0006DBD0 (0x1F), 0x00069620 (0x23), 0x000704D0 (0x1B), 0x000711A0 (0x20), 0x0006FCC0 (0x1C) and 0x000CEF70 (0xAA)
// leave it 0.
void SceneActorServices::State::view_refresh_slot(Actor& actor)
{
    for (const auto& [bytes, native] : actors->actors)
    {
        if (native == &actor)
        {
            const FactoryObject* object = object_of(bytes);
            const u16 type = object == nullptr ? 0 : object->type;
            if (type == 2 || type == 4 || type == 5 || type == 0xD || type == 0xF || type == 0x10 || type == 0x12
                || type == 0x13 || type == 0x14 || type == 0x15 || type == 0x16 || type == 0x17
                || type == 0x1A || type == 0x1F || type == 0x23 || type == 0x25 || type == 0x24 || type == 0x26
                || type == 0x1E || type == 0x1D || type == 0x18 || type == 0x19 || type == 0x1B || type == 0x21 || type == 0x20
                || type == 0x1C || type == 0xAA || type == 0xA9 || type == 0xA8 || type == 0xA7
                || type == 0xA0 || type == 0xA1 || type == 0xA2 || type == 0xA3 || type == 0xA5 || type == 0xA6)
            {
                return;
            }
            unbound("actor vtable +0x38 view refresh for factory type " + std::to_string(type));
        }
    }
    throw Error("view refresh for an actor outside the scene");
}

// Xbox 0x00121EC0
void SceneActorServices::State::draw_anchor_sprites()
{
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.anchor.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        Actor* owner = nullptr;
        for (auto& [native, registration] : actors->render)
        {
            if (&registration->anchor == node)
            {
                owner = native;
            }
        }
        if (owner == nullptr)
        {
            throw Error("DAT_00511AE8 node without a scene actor");
        }
        // Actor +0x204 -> its model header -> runtime header +8: the sprite table.
        if (owner->model == nullptr || owner->model->cpu.data() == nullptr)
        {
            throw Error("0x00121EC0 reads the sprite table of an actor without a +0x204 model");
        }
        const auto cpu = owner->model->cpu;
        const u32 sprites = read_u32(cpu, read_u32(cpu, 0x10) + 8);
        if (sprites == 0)
        {
            throw Error("0x00121EC0 reads +8 of a null sprite table");
        }
        if (read_u32(cpu, sprites + 8) == 0)
        {
            continue;
        }
        Vec3 scale;
        matrix_axis_scales(owner->xform, scale); // Actor +0x2AC.
        const auto& m = owner->xform;
        // The sprite textures are headers inside the model's own resource block. ActorModel::cpu holds that
        // block's bytes with its pointers as offsets from the block's scene address.
        if (owner->model->runtime == nullptr || owner->model->runtime->source == nullptr)
        {
            throw Error("0x00121EC0 sprite textures have no loaded model block");
        }
        const u32 block = resources.memory.address_of(owner->model->runtime->source->cpu.data());
        const u32 records = read_u32(cpu, sprites + 4);
        const u32 textures = read_u32(cpu, sprites + 0xC);
        for (u32 index = 0; index < read_u32(cpu, sprites); ++index)
        {
            const u32 record = records + index * 0x18;
            const i32 slot = i32(read_u32(cpu, record + 0xC));
            const float size[2]{scale.x * read_f32(cpu, record + 0x10), scale.y * read_f32(cpu, record + 0x10)};
            const u8* texture = resources.memory.pointer(block + read_u32(cpu, textures + (u32(slot) & 0x7FFFFFFFu) * 4));
            if (texture == nullptr)
            {
                throw Error("0x00121EC0 sprite texture is outside the scene resources");
            }
            const i32 tick = i32(float(texture[0x10]) * g_scene_hold_time); // DAT_004312CC.
            const Vec3 point{read_f32(cpu, record), read_f32(cpu, record + 4), read_f32(cpu, record + 8)};
            Vec3 position{m[1] * point.y + m[0] * point.x + m[2] * point.z + m[3],
                m[4] * point.x + m[6] * point.z + m[5] * point.y + m[7],
                m[10] * point.z + m[9] * point.y + m[8] * point.x + m[11]};
            if (slot < 0)
            {
                // Pulled 5 units (DAT_0032B184) toward the view.
                const Vec3 eye = display_view->position;
                float x = position.x - eye.x;
                float y = position.y - eye.y;
                float z = position.z - eye.z;
                const float squared = z * z + y * y + x * x;
                if (squared != 0.f)
                {
                    const long double reciprocal = retail_reciprocal_sqrt(squared);
                    x = float(x * reciprocal);
                    y = float(y * reciprocal);
                    z = float(z * reciprocal);
                }
                position = {position.x - x * 5.f, position.y - y * 5.f, position.z - z * 5.f};
            }
            queue_billboard(g_billboard_queue, texture, 0xFFFFFFFFu, u32(tick % i32(texture[0x11])), position, size,
                0.f); // DAT_00314F48.
        }
    }
}

// 0x0011C240's walk of DAT_0050E438: each active control's vtable +0x14.
void SceneActorServices::State::draw_scene_controls()
{
    for (RuntimeActorLink* link = g_scene_controls.active.next; link != nullptr; link = link->next)
    {
        if (g_scene_control_owner == nullptr)
        {
            throw Error("active scene control without a control owner");
        }
        g_scene_control_owner->draw(g_scene_control_owner->instance_of(*link));
    }
}

ActorMatrixSetup* SceneActorServices::State::actor_matrix_setup(FactoryObject& object)
{
    if (auto* background = backgrounds->find(object.bytes.get()); background != nullptr)
    {
        return &background->matrices;
    }
    return classes->actor_matrices(object.bytes.get());
}

RuntimeActorMatrixState& SceneActorServices::State::actor_draw_matrices(FactoryObject& object, Actor& actor)
{
    if (object.type == 5)
    {
        return players->player(object).matrices;
    }
    if (ActorMatrixSetup* setup = actor_matrix_setup(object); setup != nullptr)
    {
        return setup->matrices;
    }
    return draw_matrices[&actor];
}

ActorDrawState SceneActorServices::State::actor_draw_state(FactoryObject& object, Actor& actor,
    std::unique_ptr<ActorDrawModelView>& model)
{
    auto& view = actors->view(object);
    const std::span<const u8> bytes(object.bytes.get(), object.size);
    if (actor.model != nullptr && actor.model->runtime != nullptr)
    {
        RuntimeModelInstance& instance = *actor.model->runtime;
        flag_origins[&instance] = instance.node_flags.data();
        model = std::make_unique<ActorDrawModelView>(ActorDrawModelView{instance,
            resources.memory.address_of(instance.source->cpu.data()), instance.state_1c0, instance.state_1c4,
            instance.state_1c8});
    }
    std::array<u8, 4> flags = view.startup.flags;
    const auto callback = [this](u32 address, const char* slot) -> std::function<void(Actor&, i32, void*)>
    {
        if (address == 0)
        {
            return {};
        }
        if (address == 0x00066B90)
        {
            return [this](Actor& drawn, i32, void*)
            {
                begin_runtime_avatar_glow(drawn, *avatar_glow);
            };
        }
        if (address == 0x000770F0)
        {
            return [this](Actor&, i32, void*)
            {
                begin_runtime_fade_to_grey(*glow_pass); // 0x000770F0
            };
        }
        if (address == 0x000771F0)
        {
            // The third argument is the +0x1A8 word, the fade's own address.
            return [this](Actor&, i32, void* payload)
            {
                end_runtime_fade_to_grey(scene.resolve_address(u32(reinterpret_cast<std::uintptr_t>(payload)), 0x28),
                    *glow_pass);
            };
        }
        if (address == 0x00066C00)
        {
            // The third argument is the +0x1A8 word, the copy's own address.
            return [this](Actor&, i32, void* payload)
            {
                end_runtime_avatar_glow(scene.resolve_address(u32(reinterpret_cast<std::uintptr_t>(payload)), 0xA50),
                    *avatar_glow);
            };
        }
        return [slot](Actor&, i32, void*)
        {
            unbound(std::string("actor draw callback ") + slot);
        };
    };
    return ActorDrawState{view.frame, model.get(), bytes[0x197], bytes[0x298], view.startup.color,
        std::bit_cast<u32>(flags), callback(read_u32(bytes, 0x1A0), "+0x1A0"), callback(read_u32(bytes, 0x1A4), "+0x1A4"),
        reinterpret_cast<void*>(std::uintptr_t(read_u32(bytes, 0x1A8))), actor_draw_matrices(object, actor)};
}

static RgbaImage scale_to_texture(const RgbaImage& frame, int width, int height);

// The open texture target's texels (and its depth texture's words) written back, then the back buffer again.
void SceneActorServices::State::close_render_target()
{
#if GBTG_OPTIMIZE && !defined(GBTG_INSPECTOR_APP)
    // GBTG_OPTIMIZE step 28: the glow capture's depth (an own-depth scene target over DAT_005047E8) is written on the
    // GPU into the texture's image, as step 27 ends a book page, with no readback; the texture's data keeps its texels.
    // No native code reads them: the executable names DAT_005047E8 in six instructions, 0x000620C0 creating it,
    // 0x00061D00 (twice) and 0x00119800 setting it as a texture (0x001749E0), 0x001196DD taking its surface (0x00177900)
    // and 0x0006350D passing it to the back-buffer copy 0x0013DF80. The image takes the window-resolution words,
    // counted in the texture's texels, as show_full_resolution gives the readback's.
    if (render_target_depth_texture != 0 && render_target_scene)
    {
        const auto& header = resources.d3d.resources.at(render_target_depth_texture).header;
        int width = 0;
        int height = 0;
        gfx_scene_target_size(width, height);
        if (width > 0 && height > 0 && (header[4] == 0x12 || header[4] == 0x2E))
        {
            RgbaImage& image = const_cast<RgbaImage&>(
                texture_image(resources.memory.pointer(render_target_depth_texture)));
            image.width = width;
            image.height = height;
            image.bgra.resize(size_t(width) * size_t(height) * 4);
            image.texel_width = i32(header[0]);
            image.texel_height = i32(header[1]);
            ++image.serial;
            if (gfx_read_native_target_depth_into(image))
            {
                render_target_depth_texture = 0;
            }
            else
            {
                --image.serial;
            }
        }
    }
#endif
    if (render_target_depth_texture != 0)
    {
        // The target's depth surface aliases a LIN_A8R8G8B8 texture (boot 0x000620C0's DAT_005047E8): its texels
        // take the target's D24S8 words.
        std::vector<u32> words;
        int width = 0;
        int height = 0;
        gfx_read_native_target_depth(words, width, height);
        const auto& record = resources.d3d.resources.at(render_target_depth_texture);
        const size_t count = size_t(record.header[0]) * record.header[1];
        RgbaImage full;
        if (!words.empty())
        {
            if (record.header[4] != 0x12 && record.header[4] != 0x2E)
            {
                throw Error("render target depth texture is not a LIN_A8R8G8B8 or LIN_D24S8 texture");
            }
            if (render_target_scene)
            {
                // The window-resolution depth: the texture's memory takes the word at each texel's centre, its image
                // the whole picture (the words as BGRA bytes, as the linear format decodes them).
                full.width = width;
                full.height = height;
                full.bgra.resize(words.size() * 4);
                std::memcpy(full.bgra.data(), words.data(), full.bgra.size());
                const RgbaImage scaled = scale_to_texture(full, i32(record.header[0]), i32(record.header[1]));
                std::memcpy(resources.memory.tail(record.data).data(), scaled.bgra.data(), count * 4);
            }
            else
            {
                if (words.size() != count)
                {
                    throw Error("render target depth texture is not the target's size");
                }
                std::memcpy(resources.memory.tail(record.data).data(), words.data(), count * 4);
            }
            scene_d3d_memory_written();
        }
        movie_textures.insert(resources.memory.pointer(render_target_depth_texture));
        show_full_resolution(render_target_depth_texture, full);
        render_target_depth_texture = 0;
    }
    if (render_target_texture != 0 && created_textures.contains(render_target_texture))
    {
#if GBTG_OPTIMIZE
        // GBTG_OPTIMIZE step 12: the target is copied on the GPU into the texture its image samples as, so draws
        // after this one sample it without a readback. Its data is written by finish_render_target, after the
        // presenter's last command and before native code runs again; nothing reads the data in between.
        if (!render_target_scene && render_target_texture != pending_target_texture)
        {
            finish_render_target();
        }
        if (!render_target_scene)
        {
            const u8* key = resources.memory.pointer(render_target_texture);
            texture_image(key);
            RgbaImage& image = *texture_images.at(key);
            ++image.serial;
            if (gfx_end_native_target_into(image))
            {
#if !defined(GBTG_INSPECTOR_APP)
                // GBTG_OPTIMIZE step 24: the light volume texture (DAT_00504844) is only drawn into and sampled. The
                // executable references its handle and surface in four instructions: 0x000620C0 creates them,
                // 0x000FE5A0 sets the surface as the render target, and 0x000FE960 passes the texture to 0x00064820,
                // whose one use of it is D3DDevice_SetTexture(0, ...). No native code reads its texels, so they are
                // not read back and written. The inspector shows a texture from those texels, so it keeps them.
                if (render_target_texture == light_volume_texture)
                {
                    render_target_texture = 0;
                    return;
                }
#endif
                pending_target_texture = render_target_texture;
                render_target_texture = 0;
                return;
            }
            --image.serial;
        }
#endif
        // A created swizzled A8R8G8B8 (or linear) texture takes the rendered rows in its own texel order.
        RgbaImage rendered;
        RgbaImage full;
        end_render_target(rendered, full);
        write_target_texels(render_target_texture, rendered);
        movie_textures.insert(resources.memory.pointer(render_target_texture));
        show_full_resolution(render_target_texture, full);
        render_target_texture = 0;
        return;
    }
#if GBTG_OPTIMIZE && !defined(GBTG_INSPECTOR_APP)
    // GBTG_OPTIMIZE step 26: a texture target into a book page (scenes 3 and 6 draw into page 0 eight times a frame)
    // ends on the GPU into the page's image, with no readback; the page's data keeps its texels, as for step 25's
    // copies (copy_back_buffer has the evidence that no native code reads them). The linear 32-bit page formats decode
    // by copying the rows, so the image takes the target's pixels as a decode of the written rows would. The image
    // returns to the texture's own size, as that decode would make it after a step 25 copy.
    if (render_target_texture != 0 && !render_target_scene)
    {
        const u8* record = resources.memory.pointer(render_target_texture);
        if (book_page_record(record))
        {
            const auto& header = resources.d3d.resources.at(render_target_texture).header;
            RgbaImage& image = const_cast<RgbaImage&>(texture_image(record));
            image.width = i32(header[0]);
            image.height = i32(header[1]);
            image.bgra.resize(size_t(image.width) * size_t(image.height) * 4);
            image.texel_width = 0;
            image.texel_height = 0;
            ++image.serial;
            if (gfx_end_native_target_into(image))
            {
                render_target_texture = 0;
                return;
            }
            --image.serial;
        }
    }
    // GBTG_OPTIMIZE step 27: a book page drawn as a scene target (against the back buffer's depth-stencil, at the
    // window's resolution) ends on the GPU the same way. The image takes the window-resolution picture, counted in the
    // texture's texels, as show_full_resolution gives it, and the page keeps its texels.
    if (render_target_texture != 0 && render_target_scene)
    {
        const u8* record = resources.memory.pointer(render_target_texture);
        int width = 0;
        int height = 0;
        gfx_scene_target_size(width, height);
        if (book_page_record(record) && width > 0 && height > 0)
        {
            const auto& header = resources.d3d.resources.at(render_target_texture).header;
            RgbaImage& image = const_cast<RgbaImage&>(texture_image(record));
            image.width = width;
            image.height = height;
            image.bgra.resize(size_t(width) * size_t(height) * 4);
            image.texel_width = i32(header[0]);
            image.texel_height = i32(header[1]);
            ++image.serial;
            if (gfx_end_native_scene_target_into(image))
            {
                render_target_scene = false;
                render_target_texture = 0;
                return;
            }
            --image.serial;
        }
    }
#endif
    if (render_target_texture != 0)
    {
        RgbaImage rendered;
        RgbaImage full;
        end_render_target(rendered, full);
        const RuntimeMovieLockedSurface surface = lock_movie_texture(render_target_texture);
        const size_t row = size_t(rendered.width) * 4;
        for (int y = 0; y < rendered.height; ++y)
        {
            std::memcpy(surface.pixels + size_t(y) * size_t(surface.pitch), rendered.bgra.data() + size_t(y) * row,
                row);
        }
        show_full_resolution(render_target_texture, full);
        render_target_texture = 0;
    }
}

// Each of the texture's texels takes the frame's pixel at its centre, as the Xbox's 640x480 frame would hold it.
static RgbaImage scale_to_texture(const RgbaImage& frame, int width, int height)
{
    RgbaImage scaled;
    scaled.width = width;
    scaled.height = height;
    scaled.bgra.resize(size_t(width) * size_t(height) * 4);
    for (int y = 0; y < height; ++y)
    {
        const int source_y = std::clamp(int((float(y) + 0.5f) * float(frame.height) / float(height)), 0,
            frame.height - 1);
        for (int x = 0; x < width; ++x)
        {
            const int source_x = std::clamp(int((float(x) + 0.5f) * float(frame.width) / float(width)), 0,
                frame.width - 1);
            std::memcpy(scaled.bgra.data() + (size_t(y) * size_t(width) + size_t(x)) * 4,
                frame.bgra.data() + (size_t(source_y) * size_t(frame.width) + size_t(source_x)) * 4, 4);
        }
    }
    return scaled;
}

void SceneActorServices::State::show_full_resolution(u32 texture, RgbaImage& full)
{
    if (full.width <= 0 || full.height <= 0 || full.bgra.empty())
    {
        return;
    }
    const auto& record = resources.d3d.resources.at(texture);
    const u8* key = resources.memory.pointer(texture);
    // The texture's data was just written: decode it once (the image's linear and movie fields), then show the
    // window-resolution frame in its place, counted in the texture's own texels.
    RgbaImage& image = const_cast<RgbaImage&>(texture_image(key));
    image.width = full.width;
    image.height = full.height;
    image.bgra = std::move(full.bgra);
    image.texel_width = i32(record.header[0]);
    image.texel_height = i32(record.header[1]);
    ++image.serial;
}

// A created texture's data takes a rendered target's rows: swizzled for SZ_A8R8G8B8 (6), as they are otherwise.
void SceneActorServices::State::write_target_texels(u32 texture, const RgbaImage& rendered)
{
    const auto& record = resources.d3d.resources.at(texture);
    const auto texels = resources.memory.tail(record.data).first(size_t(record.header[0]) * record.header[1] * 4);
    scene_d3d_memory_written();
    if (rendered.width == i32(record.header[0]) && rendered.height == i32(record.header[1]))
    {
        if (record.header[4] == 6)
        {
            swizzle_nv2a_into(texels, rendered.bgra, record.header[0], record.header[1], 4);
        }
        else
        {
            std::memcpy(texels.data(), rendered.bgra.data(), texels.size());
        }
    }
}

// GBTG_OPTIMIZE step 12: the target that last ended on the GPU is read back and written to its texture's data,
// as close_render_target writes it otherwise. Its image already samples those pixels on the GPU (at the serial
// close_render_target gave it), so the image takes them too instead of being decoded again.
void SceneActorServices::State::finish_render_target()
{
    if (pending_target_texture == 0)
    {
        return;
    }
    const u32 texture = pending_target_texture;
    pending_target_texture = 0;
    RgbaImage rendered;
    gfx_read_ended_native_target(rendered);
    write_target_texels(texture, rendered);
    const u8* key = resources.memory.pointer(texture);
    const auto cached = texture_images.find(key);
    if (cached != texture_images.end() && cached->second->width == rendered.width
        && cached->second->height == rendered.height && cached->second->bgra.size() == rendered.bgra.size())
    {
        cached->second->bgra = std::move(rendered.bgra);
    }
    else
    {
        movie_textures.insert(key);
    }
}

void SceneActorServices::State::end_render_target(RgbaImage& rendered, RgbaImage& full)
{
    full = {};
    if (render_target_scene)
    {
        gfx_end_native_scene_target(full);
        render_target_scene = false;
        const auto& record = resources.d3d.resources.at(render_target_texture);
        if (full.width <= 0 || full.height <= 0)
        {
            // No device (the null backend): nothing was read back, so the texture's data is left as it is.
            rendered = {};
            full = {};
            return;
        }
        rendered = scale_to_texture(full, i32(record.header[0]), i32(record.header[1]));
        return;
    }
    gfx_end_native_target(rendered);
}

void SceneActorServices::State::present_render_target(std::uintptr_t color, std::uintptr_t depth)
{
    if (color == SceneD3DDevice::kBackBuffer)
    {
        close_render_target();
        return;
    }
    const auto found = d3d_device.surface_textures.find(color);
    if (found == d3d_device.surface_textures.end())
    {
        throw Error("render target surface has no texture");
    }
    if (render_target_texture != 0)
    {
        // A texture target set over an open one (0x00119230's book page 0, then page 1): the open one is written
        // back first.
        close_render_target();
    }
    const u32 texture = u32(found->second);
    const auto registered = resources.d3d.resources.find(texture);
    if (registered == resources.d3d.resources.end() || registered->second.kind != 3)
    {
        throw Error("render target surface is not a registered texture");
    }
    render_target_texture = texture;
    // A book page with the device's own depth surface (0x00119230's pages over DAT_0050EC0C) draws against the
    // back buffer's depth-stencil. Other texture targets keep a depth buffer of their own for now.
    const u8* record = resources.memory.pointer(texture);
    if (depth == SceneD3DDevice::kDepthBuffer
        && (record == glow_targets.page_texture || record == glow_targets.depth_texture
            || record == book_page_frame(1)))
    {
        gfx_begin_native_scene_target(texture, i32(registered->second.header[0]),
            i32(registered->second.header[1]));
        render_target_scene = true;
        return;
    }
    // The page drawn against a depth texture of its own also draws at the window's resolution, with a depth-stencil
    // of its own:
    // - The point lights' mode 1 pass (0x00062B00) pairs it with a LIN_D24S8 texture of the back buffer's size
    //   (0x00177840, the only such texture the translated code makes). Nothing reads that texture's words: the pass's
    //   reset (0x00123640) releases it unread, so its memory is left as it is.
    // - The glow capture (0x00062220: burning, goop, curses) pairs it with DAT_005047FC, the surface over the
    //   LIN_A8R8G8B8 texture DAT_005047E8, which 0x00061D00 then samples as the depth to stamp. close_render_target
    //   writes that texture's memory at its own size and gives its image the window-resolution words.
    if (record == glow_targets.page_texture)
    {
        const auto depth_surface = d3d_device.surface_textures.find(depth);
        if (depth_surface != d3d_device.surface_textures.end())
        {
            const u32 depth_texture = u32(depth_surface->second);
            const auto depth_record = resources.d3d.resources.find(depth_texture);
            const bool light_depth = depth_record != resources.d3d.resources.end()
                && depth_record->second.kind == 3 && depth_record->second.header[4] == 0x2E;
            const bool glow_depth = depth_record != resources.d3d.resources.end()
                && depth_record->second.kind == 3 && depth_record->second.header[4] == 0x12
                && resources.memory.pointer(depth_texture) == glow_targets.depth_texture;
            if (light_depth || glow_depth)
            {
                gfx_begin_native_scene_target(texture, i32(registered->second.header[0]),
                    i32(registered->second.header[1]), true);
                render_target_scene = true;
                if (glow_depth)
                {
                    render_target_depth_texture = depth_texture;
                }
                return;
            }
        }
    }
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 12: the backend keeps one readback of an ended target; another texture's target would
    // replace it.
    if (pending_target_texture != 0 && pending_target_texture != texture)
    {
        finish_render_target();
    }
#endif
    // A null depth surface (D3DDevice_SetRenderTarget(surface, NULL)) leaves the target without a depth buffer.
    gfx_begin_native_target(i32(registered->second.header[0]), i32(registered->second.header[1]), depth != 0);
    const auto depth_texture = d3d_device.surface_textures.find(depth);
    if (depth_texture != d3d_device.surface_textures.end())
    {
        const auto depth_record = resources.d3d.resources.find(u32(depth_texture->second));
        if (depth_record == resources.d3d.resources.end() || depth_record->second.kind != 3)
        {
            throw Error("render target depth surface is not a registered texture");
        }
        render_target_depth_texture = u32(depth_texture->second);
    }
}

bool SceneActorServices::State::book_page_record(const u8* record) const
{
    return record != nullptr && (record == book_pages[0].data() || record == book_pages[1].data()
        || record == glow_targets.page_texture || record == glow_targets.depth_texture
        || record == book_page_frame(1));
}

// Book page `index`'s frame record (the texture object's frames from its +4 offset, frame +0x12): its surfaces are
// the targets. Page 0's is DAT_005047DC; page 1's shares the depth surface's memory with DAT_005047E8.
const u8* SceneActorServices::State::book_page_frame(size_t index) const
{
    const std::span<u8> page = book_pages[index];
    if (page.empty())
    {
        return nullptr;
    }
    return page.data() + read_u32(page, 4) + u32(page[0x12]) * 0x14;
}

// D3DDevice_CopyRects(back buffer, null, 0, the texture's level 0, null): the 640x480 game viewport drawn so
// far (the host window's fitted 4:3 area) into the texture's linear 32-bit level.
void SceneActorServices::State::copy_back_buffer(u32 texture)
{
    // The target is a texture object (the book page captures) or its D3D frame record (DAT_005047DC's surface).
    const auto find_target = [this](u32 target, u32& header)
    {
        auto found = resources.d3d.resources.find(target);
        header = target;
        if (found == resources.d3d.resources.end())
        {
            const auto object = resources.memory.resolve(target, 0x30);
            header = target + read_u32(object, 4) + u32(object[0x12]) * 0x14;
            found = resources.d3d.resources.find(header);
        }
        if (found == resources.d3d.resources.end())
        {
            throw Error("back-buffer copy target is not a registered texture");
        }
        return found;
    };
#if GBTG_OPTIMIZE && !defined(GBTG_INSPECTOR_APP)
    // GBTG_OPTIMIZE step 25: the picture is copied on the GPU into the texture's image, with no readback, and the
    // texture's data keeps its texels. A byte scan of the XBE finds the book page textures and the surfaces over them
    // (DAT_005047D8 to DAT_005047E8) in 0x00048110, 0x0005DA50, 0x000615E0, 0x00061D00, 0x00061EE0, 0x000620C0,
    // 0x00062220, 0x00062B00, 0x00063320, 0x00071DF0, 0x000770F0, 0x00087FE0, 0x00119230, 0x0011BA50, 0x0012BA10 and
    // 0x0012BE60. They pass the handles to D3DDevice_SetTexture, SetRenderTarget and CopyRects (0x0013DF80 through
    // DAT_00504868), to the shader parameters and gallery and cloth records that bind them, or to DAT_00504850 and
    // DAT_00504854, which 0x00062B00 and 0x00063320 bind; none is locked (0x000620C0's one lock is of the depth surface
    // DAT_005047E4 is made over). No native code reads their texels. The inspector shows a texture from those texels,
    // so it keeps the readback.
    {
        u32 header = 0;
        const auto registered = find_target(texture, header);
        // Only the book pages (DAT_005047D8 and DAT_005047E4, made per scene) and their frame records.
        const u8* record = resources.memory.pointer(header);
        const bool book_page = book_page_record(record);
        int width = 0;
        int height = 0;
        gfx_ui_frame_size(width, height);
        if (book_page && width > 0 && height > 0)
        {
            if (pending_target_texture == header)
            {
                // A target this texture ended on the GPU is still to be written back; this copy replaces it.
                finish_render_target();
            }
            RgbaImage& image = const_cast<RgbaImage&>(texture_image(record));
            image.width = width;
            image.height = height;
            image.bgra.resize(size_t(width) * size_t(height) * 4);
            image.texel_width = i32(registered->second.header[0]);
            image.texel_height = i32(registered->second.header[1]);
            ++image.serial;
            if (gfx_copy_ui_into(image))
            {
                return;
            }
            --image.serial;
        }
    }
#endif
    RgbaImage frame;
    if (!gfx_readback_ui(frame) || frame.width <= 0 || frame.height <= 0)
    {
        throw Error("back-buffer copy readback failed");
    }
    u32 header = 0;
    const auto registered = find_target(texture, header);
    const i32 width = i32(registered->second.header[0]);
    const i32 height = i32(registered->second.header[1]);
    const RuntimeMovieLockedSurface surface = lock_movie_texture(header);
    for (i32 y = 0; y < height; ++y)
    {
        const i32 source_y = y * frame.height / height;
        u8* line = surface.pixels + size_t(y) * size_t(surface.pitch);
        for (i32 x = 0; x < width; ++x)
        {
            const i32 source_x = x * frame.width / width;
            std::memcpy(line + size_t(x) * 4, frame.bgra.data() + (size_t(source_y) * frame.width + source_x) * 4, 4);
        }
    }
    show_full_resolution(header, frame);
}

RuntimeMovieLockedSurface SceneActorServices::State::lock_movie_texture(u32 record)
{
    const auto registered = resources.d3d.resources.find(record);
    if (registered == resources.d3d.resources.end() || registered->second.kind != 3 || registered->second.data == 0)
    {
        throw Error("movie frame texture is not a registered texture");
    }
    const auto& header = registered->second.header; // width, height, levels, usage, format, pool
    // X_D3DFMT_LIN_A8R8G8B8 (0x12) and X_D3DFMT_LIN_X8R8G8B8 (0x1E): the decoder writes linear 32-bit rows.
    if (header[4] != 0x12 && header[4] != 0x1E)
    {
        char detail[80];
        std::snprintf(detail, sizeof(detail), "movie frame texture format 0x%X is not a linear 32-bit format",
            header[4]);
        throw Error(detail);
    }
    movie_textures.insert(resources.memory.pointer(record));
    scene_d3d_memory_written(); // The caller writes the surface.
    resources.keep_texture_original(registered->second.data, size_t(header[0]) * header[1] * 4);
    return RuntimeMovieLockedSurface{i32(header[0] * 4), resources.memory.pointer(registered->second.data)};
}

// A bound texture is a frame record of a texture header: header + header size (28) + frame * 0x14
// (0x00053550). Headers sit at the start of texture resources and inside fonts (one per glyph); the
// registry's loaded rows own those bytes, and the header's offsets index that row's GPU data. Model
// textures are relocated: their records sit in the scene resource memory, and each record's data is
// the address its relocation registered.
void SceneActorServices::State::forget_texture_images(const u8* begin, size_t size)
{
    for (auto entry = texture_images.begin(); entry != texture_images.end();)
    {
        if (begin <= entry->first && entry->first < begin + size)
        {
            gfx_release_sprite(*entry->second);
            movie_textures.erase(entry->first);
            entry = texture_images.erase(entry);
        }
        else
        {
            ++entry;
        }
    }
}

const RgbaImage& SceneActorServices::State::texture_image(const u8* record)
{
    const auto cached = texture_images.find(record);
    if (cached != texture_images.end())
    {
        if (movie_textures.erase(record) == 0)
        {
            return *cached->second;
        }
        // A movie frame rewrote the texture's data since it was decoded. The presenter keeps this image's
        // address across frames, so it is decoded again in place under a new serial.
        RgbaImage& image = *cached->second;
        const u32 serial = image.serial + 1;
        std::unique_ptr<RgbaImage> fresh = std::move(cached->second);
        texture_images.erase(cached);
        const RgbaImage& decoded = texture_image(record);
        image = decoded;
        image.serial = serial;
        texture_images[record] = std::move(fresh);
        return image;
    }
    const u32 address = resources.memory.find_address(record);
    const auto registered = resources.d3d.resources.find(address);
    if (created_textures.contains(address))
    {
        // A D3DDevice_CreateTexture2 texture: its creation arguments are its header.
        const auto& arguments = registered->second.header;
        TextureHeader header;
        header.format = XboxFormat(arguments[4]);
        header.width = u16(arguments[0]);
        header.height = u16(arguments[1]);
        header.data_size = arguments[0] * arguments[1] * 4;
        const auto data = resources.memory.tail(registered->second.data).first(header.data_size);
        auto image = std::make_unique<RgbaImage>(decode_xbox_texture(header, data));
        return *texture_images.emplace(record, std::move(image)).first->second;
    }
    if (address != 0 && registered != resources.d3d.resources.end() && registered->second.data != 0)
    {
        const auto block = resources.memory.tail(resources.memory.block_start(address));
        const size_t before = size_t(record - block.data());
        TextureHeader header;
        size_t frame = 0;
        while (before >= 28 + frame * 0x14 && !(parse_texture_header(block.subspan(before - 28 - frame * 0x14), header)
            && frame < std::max<size_t>(header.tile_count, 1)))
        {
            ++frame;
        }
        if (before < 28 + frame * 0x14)
        {
            throw Error("bound model texture record has no texture header");
        }
        const auto data = resources.memory.tail(registered->second.data);
        if (header.data_size > data.size())
        {
            throw Error("bound model texture data is outside its block");
        }
        TextureHeader tile = header;
        tile.texture_offset = 0;
        auto image = std::make_unique<RgbaImage>(decode_xbox_texture(tile, data.first(header.data_size)));
        return *texture_images.emplace(record, std::move(image)).first->second;
    }
    for (const auto& loaded : resources.registry.loaded)
    {
        const u8* begin = loaded.cpu.data();
        if (begin == nullptr || record < begin || record >= begin + loaded.cpu.size())
        {
            continue;
        }
        for (size_t frame = 0; size_t(record - begin) >= 28 + frame * 0x14; ++frame)
        {
            const size_t start = size_t(record - begin) - 28 - frame * 0x14;
            TextureHeader header;
            if (!parse_texture_header(loaded.cpu.subspan(start), header) || frame >= std::max<size_t>(header.tile_count, 1))
            {
                continue;
            }
            const size_t offset = size_t(header.texture_offset) + frame * header.data_size;
            if (offset > loaded.gpu.size() || header.data_size > loaded.gpu.size() - offset)
            {
                continue;
            }
            TextureHeader tile = header;
            tile.texture_offset = 0;
            auto image = std::make_unique<RgbaImage>(decode_xbox_texture(tile, loaded.gpu.subspan(offset, header.data_size)));
            return *texture_images.emplace(record, std::move(image)).first->second;
        }
        throw Error("bound texture record has no texture header");
    }
    throw Error("bound texture is outside every loaded resource");
}

#if defined(GBTG_INSPECTOR_APP)
InspectionView SceneActorServices::inspection_camera(std::string* source) const
{
    using namespace entity_detail;
    const auto* camera = g_cam_active[0];
    if (camera == nullptr)
    {
        InspectionView result;
        Vec3 euler;
        if (read_runtime_marker_camera_start(result.position, euler, state_->camera_start))
        {
            camera_euler_to_quaternion(euler, result.rotation);
            if (source != nullptr)
            {
                *source = "native camera-start marker";
            }
        }
        else if (source != nullptr)
        {
            *source = "editor origin";
        }
        return result; // Host inspection camera only; no camera is inserted into retail state.
    }
    if (source != nullptr)
    {
        *source = "active runtime camera";
    }
    // A copy avoids the normalization/clip stores made by retail camera setup.
    EntityCamNode copy = *camera;
    auto& setup = *state_->display_setup;
    const auto saved_record = g_display_records[0];
    auto* saved_pointer = g_display_view;
    const u32 saved_address = setup.current_display_address;
    const u32 saved_effects = setup.effects_disabled;
    const auto restore = [&]()
    {
        g_display_records[0] = saved_record;
        g_display_view = saved_pointer;
        setup.current_display_address = saved_address;
        setup.effects_disabled = saved_effects;
    };
    try
    {
        setup.effects_disabled = 1;
        select_display_camera(&copy, 0, setup);
        const auto& view = *g_display_view;
        InspectionView result;
        result.position = view.position;
        result.rotation = view.rotation;
        result.fov = view.field_of_view;
        restore();
        return result;
    }
    catch (...)
    {
        restore();
        throw;
    }
}

void SceneActorServices::inspect_view(const InspectionView& camera, InspectionFrame& frame)
{
    inspect_draw(camera, frame, "native actor view draw 0x0011C240", [this]()
    {
        state_->with_view_context(0, [](RuntimeSceneViewPresent& view)
        {
            view.mirror();
            view.refresh_actors();
            view.bins();
            view.anchor_sprites();
            view.billboards();
        });
    });
}

void SceneActorServices::inspect_draw(const InspectionView& camera, InspectionFrame& frame, const char* draw_stage,
    const std::function<void()>& draw)
{
    auto& state = *state_;
    auto& setup = *state.display_setup;
    using namespace entity_detail;
    // The editor owns its camera. Preserve native display bindings even when a draw raises.
    const auto saved_view = g_display_records[0];
    auto* saved_pointer = g_display_view;
    const u32 saved_address = setup.current_display_address;
    const auto saved_projection = setup.device_projection;
    const auto saved_device_view = setup.device_view;
    const auto saved_world = setup.device_world;
    const auto saved_transpose = setup.projection_transpose;
    const u32 saved_effects = setup.effects_disabled;
    const auto restore = [&]()
    {
    #if defined(GBTG_INSPECTOR_APP)
    if (state.game.inspection != nullptr)
        {
            state.game.inspection->drawing = false;
        }
#endif
        g_display_records[0] = saved_view;
        g_display_view = saved_pointer;
        setup.current_display_address = saved_address;
        setup.device_projection = saved_projection;
        setup.device_view = saved_device_view;
        setup.device_world = saved_world;
        setup.projection_transpose = saved_transpose;
        setup.effects_disabled = saved_effects;
    };
    const char* stage = "inspection display setup";
    try
    {
        state.d3d_device.commands.clear();
        state.inspection_draw_owners.clear();
        EntityCamNode node;
        node.position = camera.position;
        node.rotation = camera.rotation;
        node.focal = 35.f;
        setup.effects_disabled = 1;
        select_display_camera(&node, 0, setup);
        rebuild_display_view(setup);
        // Use the native focal conversion so projection and the visibility frustum agree.
        auto& view = *g_display_view;
        view.focal *= std::tan(view.field_of_view * 0.00872664626f)
            / std::tan(camera.fov * 0.00872664626f);
        rebuild_display_view(setup);
        view.near_clip = camera.near_clip;
        view.far_clip = camera.far_clip;
        rebuild_display_projection(setup);
        bind_display_projection(setup);
        frame.display = view;
        frame.projection = setup.device_projection;
        stage = draw_stage;
    #if defined(GBTG_INSPECTOR_APP)
    if (state.game.inspection != nullptr)
        {
            state.game.inspection->drawing = true;
        }
#endif
        draw();
    #if defined(GBTG_INSPECTOR_APP)
    if (state.game.inspection != nullptr)
        {
            state.game.inspection->drawing = false;
        }
#endif
        auto commands = state.d3d_device.take_commands();
        frame.draws.clear();
        const auto& actors = actor_addresses();
        for (const auto& command : commands)
        {
            InspectionDraw draw;
            if (const auto owner = state.inspection_draw_owners.find(frame.draws.size());
                owner != state.inspection_draw_owners.end())
            {
                draw.owner = owner->second;
                const auto actor = actors.find(draw.owner);
                if (actor != actors.end() && actor->second->model != nullptr)
                {
                    const auto* model = actor->second->model;
                    const auto cpu = model->runtime != nullptr && model->runtime->source != nullptr
                        ? model->runtime->source->cpu.data() : model->cpu.data();
                    const auto& registry = state.resources.registry;
                    for (const auto& record : registry.records)
                    {
                        if (record.resource_slot > 0 && record.resource_slot <= registry.loaded.size()
                            && registry.loaded[record.resource_slot - 1].cpu.data() == cpu)
                        {
                            draw.model_resource = record.name;
                            break;
                        }
                    }
                }
            }
            const SceneD3DIndexedDraw* indexed = std::get_if<SceneD3DIndexedDraw>(&command);
            if (const auto* inline_draw = std::get_if<SceneD3DInlineDraw>(&command))
            {
                indexed = &inline_draw->state;
                draw.kind = "Inline";
            }
            if (indexed != nullptr)
            {
                if (draw.kind.empty())
                {
                    draw.kind = "Indexed";
                }
                draw.primitive = indexed->primitive;
                draw.count = indexed->count;
                draw.index_address = indexed->indices;
                draw.streams.assign(indexed->streams.begin(), indexed->streams.end());
                const auto states = indexed->states.values();
                draw.states.assign(states.begin(), states.end());
                for (size_t stage = 0; stage < 4; ++stage)
                {
                    draw.textures[stage] = state.resources.memory.find_address(indexed->textures[stage]);
                    if (indexed->textures[stage] != nullptr)
                    {
                        draw.texture_images[stage] = &state.texture_image(indexed->textures[stage]);
                    }
                }
            }
            else if (const auto* quad = std::get_if<SceneD3DQuad>(&command))
            {
                draw.kind = "Quad";
                draw.count = 4;
                draw.textures[0] = state.resources.memory.find_address(quad->texture);
                if (quad->texture != nullptr)
                {
                    draw.texture_images[0] = &state.texture_image(quad->texture);
                }
                const auto states = quad->states.values();
                draw.states.assign(states.begin(), states.end());
            }
            else if (const auto* fixed = std::get_if<SceneD3DFixedDraw>(&command))
            {
                draw.kind = "Fixed (vertex words)";
                draw.primitive = fixed->primitive;
                draw.count = u32(fixed->vertices.size());
                const auto states = fixed->states.values();
                draw.states.assign(states.begin(), states.end());
            }
            else
            {
                draw.kind = std::holds_alternative<SceneD3DClear>(command) ? "Clear"
                    : std::holds_alternative<SceneD3DCopy>(command) ? "Back-buffer copy" : "Target";
            }
            frame.draws.push_back(std::move(draw));
        }
        const SceneD3DPresentContext present{state.resources.memory, state.resources.d3d,
            state.d3d_device.vertex_shaders, [&state](const u8* record) -> const RgbaImage&
            {
                return state.texture_image(record);
            }, [&state](std::uintptr_t color, std::uintptr_t depth)
            {
                state.present_render_target(color, depth);
            }, [&state](u32 texture)
            {
                state.copy_back_buffer(texture);
            }, [&state]()
            {
                state.finish_render_target();
            }};
        stage = "recorded D3D presentation";
        present_scene_d3d_commands(commands, present);
    }
    catch (const SceneInspectionFailure&)
    {
        state.d3d_device.commands.clear();
        restore();
        throw;
    }
    catch (const std::exception& error)
    {
        state.d3d_device.commands.clear();
        restore();
        throw Error(std::string(stage) + ": " + error.what());
    }
    restore();
}

#endif
} // namespace gbtg
