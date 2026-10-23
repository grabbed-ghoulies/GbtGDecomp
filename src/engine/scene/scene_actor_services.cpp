#include "scene_actor_services.h"
#include "../entities/ghouly_spawn_trigger.h"
#include "../entities/actor_scare_requests.h"
#include "../frontend/frontend_sounds.h"
#include "../frontend/frontend_state.h"
#include "../frontend/gameselect_slots.h"
#include "../entities/body_timers.h"
#include "../entities/actor_lookup.h"
#include "../entities/actor_render_registration.h"
#include "../entities/bone_debris_actor.h"
#include "scene_point_lights.h"
#include "scene_light_grid.h"
#include "../graphics/scene_point_light_pass.h"
#include "../video/movie_startup.h"
#include "attract_control.h"
#include "gameselect_control.h"
#include "../graphics/actor_effect_draw.h"
#if defined(GBTG_INSPECTOR_APP)
#include "scene_inspection.h"
#include "../inspector/inspection_view.h"
#endif
#include "scene.h"
#include "../camera/camera.h"
#include "factory_scene_runtime.h"
#include "scene_marker_handlers.h"
#include "../entities/player_factory_owner.h"
#include "../entities/scene_actor_factory_owner.h"
#include "../entities/actor_base_messages.h"
#include "../entities/actor_event_messages.h"
#include "../entities/actor_live_attachments.h"
#include "../entities/entity_heap.h"
#include "../entities/segment_records.h"
#include "../entities/entity_collision.h"
#include "../entities/aligned_heap.h"
#include "../entities/body_move_actions.h"
#include "../entities/body_state_actions.h"
#include "../entities/body_walk_target_state.h"
#include "../entities/actor_callout_resources.h"
#include "../entities/actor_shape_bounds.h"
#include "../entities/player_messages.h"
#include "scene_actor_activation.h"
#include "scene_body_messages.h"
#include "../entities/aid_spawn_resources.h"
#include "scene_mind_messages.h"
#include "scene_object_requests.h"
#include "background_class_owner.h"
#include "background_update.h"
#include "scene_actor_update.h"
#include "scene_view_actors.h"
#include "scene_mirror.h"
#include "scene_view_present.h"
#include "../graphics/billboard_queue.h"
#include "../graphics/world_sprite_passes.h"
#include "../graphics/actor_effect_nodes.h"
#include "../entities/actor_runtime_queries.h"
#include "../entities/actor_pose_updates.h"
#include "scene_controls.h"
#include "background_cloths.h"
#include "../graphics/background_cloth_draw.h"
#include "lighting.h"
#include "../graphics/light_volume_draw.h"
#include "../entities/actor_view_visibility.h"
#include "../entities/player_overlay.h"
#include "../graphics/billboard_queue.h"
#include "../entities/actor_model_anchors.h"
#include "../audio/actor_sound_tick.h"
#include "../graphics/global_effect_tick.h"
#include "background_messages.h"
#include "ambient.h"
#include "../navigation/model_grid.h"
#include "../navigation/occupancy.h"
#include "../audio/bank_streams.h"
#include "scene_controls.h"
#include "scene_control_update.h"
#include "scene_actor_removal.h"
#include "scene_shadow_casters.h"
#include "scene_scare_timers.h"
#include "scene_gameplay_time.h"
#include "scene_object_disposal.h"
#include "../graphics/light_volume_records.h"
#include "scene_object_requests.h"
#include "../camera/cutscene_impulse.h"
#include "../cutscenes/event_assets.h"
#include "gbtg/language.h"
#include "dialogue_scene_runtime.h"
#include "../frontend/hud_layout.h"
#include "../frontend/dialogue_text_system.h"
#include "../frontend/dialogue_activation.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_bat.h"
#include "../frontend/dialogue_widget_update.h"
#include "../frontend/dialogue_page_setup.h"
#include "../frontend/dialogue_measurement.h"
#include "../frontend/dialogue_tag_actions.h"
#include "../frontend/dialogue_trophies.h"
#include "../frontend/dialogue_tags.h"
#include "../frontend/dialogue_sound_tags.h"
#include "../frontend/dialogue_resources.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/display_view.h"
#include "gbtg/gfx.h"
#include "gbtg/texture.h"
#include "../camera/display_clip_limits.h"
#include "../graphics/scene_d3d_device.h"
#include "../graphics/door_shadow_draw.h"
#include "../graphics/xg_shader_assembly.h"
#include "../graphics/xg_pixel_shader.h"
#include "../graphics/scene_d3d_present.h"
#include "../audio/pcm_backend.h"
#include "gbtg/engine_internal.h"
#include "../scripts/script_dialogue_actions.h"
#include "../graphics/scene_model_draw.h"
#include "../graphics/actor_snapshots.h"
#include "../graphics/scene_environment_setup.h"
#include "../graphics/model_tree_dispatch.h"
#include "../graphics/model_render_nodes.h"
#include "../graphics/model_push_buffer_draw.h"
#include "../graphics/model_renderer_boot.h"
#include "../graphics/model_skeleton_draw.h"
#include "../graphics/model_matrix_array_draw.h"
#include "../graphics/model_shader_states.h"
#include "../graphics/model_transform_draw.h"
#include "../graphics/model_vertex_shader_draw.h"
#include "../graphics/model_shader_parameter_draw.h"
#include "../graphics/model_blendshape_draw.h"
#include "../entities/button_bat_draw.h"
#include "../graphics/vector_normalize.h"
#include "../graphics/d3dx_matrix.h"
#include "../entities/actor_frame_render.h"
#include "../entities/actor_word_messages.h"
#include "../camera/camera_mind_requests.h"
#include "../camera/entity_camera.h"
#include "../entities/camera_director.h"
#include "../entities/player_update.h"
#include "../entities/contact_prop_actor.h"
#include "../entities/spider_web.h"
#include "../graphics/blob_shadows.h"
#include "../entities/fx_custom_electric.h"
#include "../entities/player_mind_setup.h"
#include "../navigation/navigation.h"
#include "../entities/entity.h"
#include "../audio/core_cue.h"
#include "../audio/event_voices.h"
#include "../graphics/texture_xpr.h"
#include "../transitions/native_book.h"
#include "../frontend/gameselect_contents.h"
#include "../frontend/gameselect_delete.h"
#include "../frontend/gameselect_features.h"
#include "../frontend/gameselect_scene_replay.h"
#include "../frontend/button_bat_menu.h"
#include "../frontend/input_stack.h"
#include "../frontend/brew_meter.h"
#include "../frontend/player_health_display.h"
#include "../frontend/scoreboard_update.h"
#include "../frontend/scoreboard_draw.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_event_sequence.h"
#include "live_spawn_tick.h"
#include "../scripts/script_vm.h"
#include "../transitions/scene_transitions.h"
#include "ghoul_intro.h"
#include "../entities/player_mind_feedback.h"
#include "cutscene_scene_runtime.h"
#include "../video/native_movies.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_event_tallies.h"
#include "../frontend/scoreboard_hud.h"
#include "../scripts/script_actor_query.h"
#include "../entities/player_mind_target_control.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_set>

extern "C"
{
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
}

namespace gbtg
{
// The scene's play bounds, scene.cpp (cleared by 0x0011C5A0).
extern u8 g_50ea01; // DAT_0050EA01.
extern float g_50ea7c; // DAT_0050EA7C.
extern float g_50ea80; // DAT_0050EA80.
extern float g_50ea84; // DAT_0050EA84.
extern float g_50ea88; // DAT_0050EA88.
extern float g_50ea8c; // DAT_0050EA8C.
extern float g_50ea90; // DAT_0050EA90.
extern u32 g_flag_ea98; // DAT_0050EA98.
extern i32 g_5462fc; // DAT_005462FC.
extern i32 g_545d1c; // DAT_00545D1C.
extern i32 g_545cc8; // DAT_00545CC8.
extern i32 g_script_30d0c; // DAT_00430D0C.
extern i32 g_510580; // DAT_00510580.
extern float& g_4312cc; // DAT_004312CC.
extern float& g_4312d0; // DAT_004312D0.
extern i32& g_4312d4; // DAT_004312D4.
extern i32& g_4312d8; // DAT_004312D8.
extern u32 g_flag_0650; // DAT_00510650.
extern i32 g_50ec00; // DAT_0050EC00.
extern u32 g_flag_10b4; // DAT_005110B4.
extern i32 g_510cf4; // DAT_00510CF4.
extern u8* g_510e30; // DAT_00510E30.
extern u8* g_510e24; // DAT_00510E24.

namespace
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

template <typename T>
T& field_at(FactoryObject& object, size_t offset)
{
    return *reinterpret_cast<T*>(object.bytes.get() + offset);
}
}

// Per-actor base message context (actor vtable +0xC base handler), built on first use.
struct SceneActorBase
{
    SceneActorBase(ActorTrailContext trail_context, RuntimeActorStartupState& state, ActorEffectStreamContext& effects,
        ActorScareContext& scares, AudioEventVoices& voices, FactoryObject& object, ActorAttachmentEventContext callout,
        NativeActorFactoryInstance& view, ActorLink& world_bind, ModelNodeFlagCache& node_cache,
        const AnimationMatrix& reset)
        : trails(std::move(trail_context)),
          events{state, effects, trails, scares, voices, {}, trail_timer, field_at<float>(object, 0x958),
              field_at<float>(object, 0x95C)},
          callouts(std::move(callout)),
          base{events, view.render, callouts, view.anchors, view.live_attachments, world_bind, watched, view.pose,
              nullptr, node_cache, reset}
    {
    }
    float trail_timer = 0.f; // Actor +0x1DC.
    void* watched = nullptr; // Actor +0x11C.
    EntityBonePose primary_pose; // Native view of model instance +0x164.
    EntityBonePose alternate_pose; // Native view of model instance +0x168.
    ActorTrailContext trails;
    ActorEventContext events;
    ActorAttachmentEventContext callouts;
    ActorBaseContext base;
};

// The player class's message slot (vtable +0xC, 0x00028F60) and its contexts, one per type 5 actor.
struct ScenePlayerMessages
{
    ScenePlayerMessages(RuntimePlayerActivation activation_context, RuntimePlayerPause pause_context,
        RuntimePlayerNavigationRegistration navigation_context, NativePlayerInstance& player, ActorBaseContext& base,
        RuntimePlayerChildMessages child_messages)
        : activation(std::move(activation_context)),
          pause(pause_context),
          navigation(navigation_context),
          context{player.self, base, player.actor_instance.animation.controller.get(), player.attribute_resource,
              player.body_bits, player.spawn, activation, pause, navigation},
          children(std::move(child_messages))
    {
    }
    RuntimePlayerActivation activation;
    RuntimePlayerPause pause;
    RuntimePlayerNavigationRegistration navigation;
    RuntimePlayerMessageContext context;
    RuntimePlayerChildMessages children;
};

// Native views of one model for the body constructors: the directory blocks (CPU +0 table, +4 count)
// and the instance channel word (+0x1CC).
struct SceneActorModelViews
{
    std::vector<RuntimeBodyModelNode> rows;
    RuntimeBodyModelNodes nodes;
    std::array<u8, 0x1D0> channels{};
};

// Per-background views its constructor reads: matrix-array bindings (ndMtxArray +0x20, count +0x24) and the
// model nodes 0x00042C30 writes.
struct SceneBackgroundViews
{
    ActorMatrixBindings matrix_bindings;
    BackgroundNodeViews nodes;
    // Retail words the background code resolves: offsets in its actor model's address space (below the
    // scene objects at 0x10000000), scene objects, then relocated resources from 0x40000000.
    std::function<std::span<const u8>(u32, size_t)> resolve;
    std::function<std::span<const u8>(u32)> source;
};

struct SceneActorServices::State
{
    State(SceneFactoryRuntime& owner, Game& owner_game);
    ~State();
    SceneFactoryRuntime& scene;
    Game& game;
    SceneExecutableResources& image;
    SceneDialogueResourceOwner& resources;

    // Retail globals. Zero-initialised storage is BSS whose scene-start value is zero; values read
    // from the executable name their initialised-data address.
    u32& random = scene.random; // DAT_0054DD7C.
    std::array<u8, 0x1000> address_zero{}; // Reads through a null retail pointer.
    const char empty_name[4]{}; // DAT_00546208.
    std::vector<const char*> actor_tags; // DAT_0042A854, 0xE9 names.
    std::vector<const char*> weapon_names; // DAT_00434104.
    std::vector<const char*> class_names; // DAT_004340EC.
    std::vector<const char*> mind_goal_names; // DAT_003397BC.
    std::span<const u8> statsheet; // DAT_00545AC4: aid_misc_ghoulies_statsheet_actors, rows of 0x87.
    u32 statsheet_rows = 0; // DAT_00545AC8.
    // Actor startup (0x0003E940).
    RuntimeModelDeferredPool deferred{};
    u32 fence = 0;
    RuntimeShaderRegistrationState shaders{};
    std::unique_ptr<RuntimeShaderAnimationScratch> scratch = std::make_unique<RuntimeShaderAnimationScratch>();
    std::function<void(u8*)> heap_free = [](u8* allocation)
    {
        engine_heap_free(allocation);
    };
    RuntimeActorLink live_actors{}; // DAT_00511B08.
    RuntimeActorStartupContext startup;
    // Sounds and effects.
    RuntimeActorSoundQueues sounds{};
    Vec3 origin{}; // DAT_0050DD80.
    Vec3 default_euler{}; // DAT_0050DD74.
    Vec3 door_added_scale{}; // DAT_0042A840, the scale 0x0005B540 gives a door it adds.
    Vec3 default_cone{}; // DAT_0042A840.
    ActorEffectModelContext effect_models;
    ActorEffectStreamContext effects;
    std::unique_ptr<RuntimeEffectDebrisStorage, std::function<void(RuntimeEffectDebrisStorage*)>> debris_pool
    {
        new RuntimeEffectDebrisStorage,
        [this](RuntimeEffectDebrisStorage* storage)
        {
            release_effect_debris(*storage, resources.registry);
            delete storage;
        }
    };
    std::unique_ptr<RuntimeEffectTrailStorage, std::function<void(RuntimeEffectTrailStorage*)>> trail_pool
    {
        new RuntimeEffectTrailStorage,
        [this](RuntimeEffectTrailStorage* storage)
        {
            release_effect_trails(*storage, resources.registry);
            delete storage;
        }
    };
    RuntimeEffectCollision effect_collision{}; // DAT_00506918..DAT_0050692C, BSS.
    u32 effect_callback_result = 0; // DAT_00506C50, BSS.
    u32 effect_sound_frame = 0; // DAT_005462D8, BSS.
    RuntimeEffectPassScratch* effect_pass_scratch = nullptr; // Native view of the caller's incoming stack bytes.
    // The scene tick's (0x0011E7F0) stack bytes under the effect pass. Retail leaves them uninitialised; this
    // native slot starts at zero and, like the reused stack slot, keeps what the previous frame left there.
    RuntimeEffectPassScratch frame_effect_scratch{};
    // World-pass gates of 0x00121900's helpers. No translated code writes them yet; they keep their BSS value.
    u32 blob_shadow_enable = 0; // DAT_0050EF18.
    i32 blob_shadow_count = 0; // DAT_0050EF10.
    u32 blob_shadow_suppress = 0; // DAT_0050EF1C.
    std::array<RuntimeBlobShadow, 0x20> blob_shadow_entries{}; // DAT_0050EC10.
    i32 blob_shadow_free = 0; // DAT_0050EF14.
    RuntimeBlobShadows blob_shadows{blob_shadow_entries, blob_shadow_count, blob_shadow_free, blob_shadow_enable};
    u32 decal_enable = 0; // DAT_005462EC.
    u32 ambient_point_draw = 0; // DAT_005120F8.
    // The boot-owned audio core (DAT_004E0CE8 and the rest); every scene requests through the same voices.
    NativeAudioRuntime& audio = native_audio_runtime();
    AudioEventVoices& voices = audio.voices;
    // Trails and scares.
    float pause = 0.f; // DAT_004312DC.
    float frame_delta = 0.f; // DAT_005096C0.
    float time_scale = 1.f; // DAT_004312C4, 0x0011C5A0 restores DAT_0032B0F8.
    i32 pal = 0;
    i32 pal_override = 0;
    u32 scare_flags = 0;
    u32 player_link = 0; // DAT_00511B40.
    ActorScareContext scares;
    // Shared actor state.
    std::unordered_map<ActorFrameAttachment*, const u8*> frame_objects;
    std::unordered_map<const u8*, const u8*> linked_records;
    std::unique_ptr<RuntimeCalloutResources> callout_resources;
    ActorLink world_bind{}; // DAT_00511B10.
    ModelNodeFlagCache node_cache{}; // DAT_005078D8.
    AnimationMatrix reset_matrix{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    RuntimeShaderMatrixBoundaries matrix_boundaries = d3dx_matrix_boundaries();
    std::unordered_map<const u8*, std::unique_ptr<SceneActorBase>> bases;
    std::unordered_map<const u8*, std::unique_ptr<ScenePlayerMessages>> player_messages;
    // Body message graphs (parent body handler 0x000253B0 and its derived slots).
    std::vector<const char*> callout_names; // 0x00421C9C.
    std::vector<const char*> flag_names; // 0x00338FB0.
    std::vector<const char*> object_tags; // DAT_0042A850.
    std::vector<const char*> surface_names; // DAT_004340A0.
    std::array<const char*, 4> shock_pass{};
    std::array<const char*, 4> shock_fail{};
    std::array<u16, 3> death_actions{}; // DAT_00217EB8.
    u32 player_damage = 0; // DAT_00512200.
    entity_detail::ListHead global_attachments{}; // DAT_00545984.
    std::unique_ptr<RuntimeBodyDamageTables> damage_tables;
    std::unique_ptr<FootstepSoundNames> footsteps;
    // Camera director and camera rig (player update 0x0002C2B0 for cameron).
    RuntimeCameraRigTunables rig_tunables; // DAT_004274B8.., DAT_00434008..
    i32 motion_steps = 0; // DAT_004312E0.
    std::array<entity_detail::ShapeProbe*, 8> collision_push_contacts{}; // DAT_0051118C.
    RuntimeBodyShapePush body_shape_push{entity_detail::g_world.shape, entity_detail::g_contact_slot,
        collision_push_contacts};
    std::array<entity_detail::SlideRecord, 5> body_slide_contacts{}; // DAT_00505040.
    u32 view_invert = 0; // DAT_005122D8.
    i32 render_width = 0; // DAT_0054DD84, set by display initialization 0x000F5900.
    i32 render_height = 0; // DAT_0054DD80.
    struct DirectorData
    {
        u32 keep_state; // DAT_00428708.
        i32 minimum_count; // DAT_0042870C.
        i32 minimum_priority; // DAT_00428710.
        u32 ground_heights; // DAT_00428714.
        float range_extension; // DAT_00428718.
        float lead_distance; // DAT_0042871C.
        u32 focus_targets; // DAT_00428720.
        u32 carried_targets; // DAT_00428724.
        i32 region_mode; // DAT_00428728.
        u32 skip_candidates; // DAT_0042872C.
        u32 input_heading; // DAT_00428730.
        float near_range; // DAT_0043400C.
    } director_data{};
    std::unique_ptr<RuntimeDirectorTunables> director_tunables;
    std::unique_ptr<RuntimeDirectorTargets> director_targets;
    std::unique_ptr<RuntimeCameraRigContext> camera_rig;
    std::unique_ptr<SceneBodyServices> body_services;
    std::unordered_map<const u8*, std::unique_ptr<SceneBodyMessages>> body_messages;
    // Mind message graphs (cinema 0x0007C8B0 and player 0x0007F1E0 over the parent-mind requests).
    std::vector<const char*> state_action_names; // 0x003390E4.
    std::vector<const char*> goal_flag_names; // 0x003397AC.
    std::array<u8, 0x20> story_bits{}; // *(DAT_00506C6C+8).
    std::array<u32, 5> port_devices{}; // DAT_0050E0F4.
    RuntimeCameraPulse pulse{}; // DAT_0050447C.
    std::unique_ptr<SceneMindServices> mind_services;
    std::unordered_map<const u8*, std::unique_ptr<SceneMindMessages>> mind_messages;
    // Active actors (DAT_00511AA4) and the scene-start lists 0x00123070 fills.
    std::unordered_map<u32, std::unique_ptr<entity_detail::LatchNode>> active_nodes;
    std::vector<u32> scene_nodes; // DAT_00511B5C.
    entity_detail::LatchNode* scene_controls = nullptr; // DAT_0050E438; 0x00116170 registers nothing natively yet.
    // The native scene controls (g_scene_controls) as 0x00115FF0, 0x00116090, 0x001160F0 and 0x001160C0 see them.
    std::function<RuntimeActorLink&(RuntimeActorLink&)> control_update_link =
        [this](RuntimeActorLink& link) -> RuntimeActorLink&
    {
        return scene.controls->instance_of(link).update;
    };
    std::function<void(RuntimeActorLink&, std::span<const u32>)> control_message =
        [this](RuntimeActorLink& link, std::span<const u32> message)
    {
        scene.controls->message(scene.controls->instance_of_update(link), message);
    };
    std::function<void(RuntimeActorLink&)> control_update = [this](RuntimeActorLink& link)
    {
        scene.controls->tick(scene.controls->instance_of_update(link)); // Control vtable +0x10.
    };
    RuntimeSceneControlUpdates control_updates();
    // 0x00123BA0's tables: the powerup rows and the scared actors with their times.
    std::array<RuntimePowerupRow, 20> powerup_rows{}; // DAT_00512140.
    RuntimeScoreboard* powerup_board = nullptr; // DAT_00545ABC.
    i32 scared_count = 0; // DAT_00512320.
    std::array<u32, 4> scared_actors{}; // DAT_00512324.
    std::array<float, 4> scared_times{}; // DAT_00512334.
    RuntimeSceneShadowActor shadow_actor(Actor& actor);
    Actor& bin_owner(entity_detail::ListHead& node);
    void update_first_actors(); // 0x00123270 over DAT_00511B5C.
    void remove_retired_actors(); // 0x001232D0 over DAT_00511B58.
    void update_shadow_casters(); // 0x00120EF0.
    i32 excluded_count = 0; // DAT_0050E228, cleared by 0x0011C480.
    u8* navigation_nodes = nullptr; // DAT_0050E24C; the navigation loader that allocates it is not native yet.
    std::array<u16, 0x20> excluded_tags{}; // DAT_0050E22C.
    std::unordered_map<u32, Actor*> addresses;
    // Input (DAT_0050E0E8 and its devices).
    std::unordered_map<u32, std::span<u8>> input_addresses;
    RuntimeInputReleaseContext input_release{input_addresses};
    // The boot-owned input stack (0x00033274's pool, DAT_0050E0EC, the "Scene Master Pad" entry).
    GhoulySpawnPool* input_pool = game.native_input->pool;
    i32& input_selected = game.native_input->selected; // DAT_0050E0EC.
    std::span<u8> controllers = runtime_pad_state_views().controllers; // DAT_005079F8.
    std::span<u8> keyboard = runtime_pad_state_views().merged; // DAT_00507C98.
    std::array<u32, 5>& port_masks = game.native_input->port_masks; // DAT_0050E0F4.., set at 0x00033248.
    RuntimeInputActivationContext input_activation{*input_pool, input_selected, controllers, keyboard,
        port_masks.data() + 1, input_addresses};
    // Player construction and children.
    u32 player_count = 0; // DAT_00510578.
    u32& active_strategies = reinterpret_cast<u32&>(g_510cec); // DAT_00510CEC.
    // The object create_runtime_player_attachment (0x0013A720) and its kin are attaching, before the scene keeps it,
    // and its bytes. object_of consults it only when no kept object owns the bytes.
    FactoryObject* attaching_object = nullptr;
    const u8* attaching_bytes = nullptr;
    std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attachment_actors;
    std::array<u16, 8> scale_exclusions{}; // DAT_0050E1B4.
    std::array<u8, 0x40> animation_classes{}; // DAT_0050E170.
    std::unordered_map<const u8*, const RuntimeScriptLookupTable*> action_tables;
    std::unordered_map<const u8*, const u8*> animation_sequences;
    std::array<RuntimeBodyResourceCacheEntry, 0x40> animation_cache{}; // DAT_004DD554.
    i32 animation_cache_count = 0; // DAT_004DD550.
    std::unordered_map<u32, const char*> action_names;
    RuntimeMindClassSettings mind_settings{0, {}}; // DAT_0050E148.
    float intro_scale = 0.f; // DAT_00433BEC.
    // DAT_00433BE8 and DAT_00433BF0 are the health display's health_style and scoreboard_globals'
    // health_second_voice: 0x001307D0 sets the style the heart display (0x001301A0) reads.
    RuntimePlayerIntroSound intro_sound;
    i32 camera_players = 0; // DAT_00545AF0.
    RuntimePlayerCameraContext camera;
    std::vector<FactoryObject> cameras;
    RuntimeStrategyClassRanges strategy_ranges{0, {}}; // DAT_0050E1C4.
    std::span<u32, 2> pause_references{scene_detail::g_pause_references}; // DAT_0050E764, DAT_0050E768.
    // DAT_00511B38 as 0x00117400 walks it: the players' +0x934 nodes, rebuilt from the list before each walk.
    std::vector<RuntimePauseActor> pause_nodes;
    bool pause_walking = false;
    RuntimePauseActor* first_pause = nullptr; // DAT_00511B38.
    RuntimeActorPauseGroups pause_groups{pause_references, first_pause};
    std::array<u32, 8> override_words{}; // DAT_005121E8..
    RuntimePlayerScriptOverrides overrides{override_words[0], override_words[1], override_words[2],
        override_words[3], override_words[4], override_words[5], override_words[6], override_words[7]};
    entity_detail::LatchNode*& latches = entity_detail::g_world.latch; // DAT_00511AA4.
    u32& decay_index = reinterpret_cast<u32&>(g_state_248c); // DAT_0051248C, the script's title mode.
    std::span<const u8> decay_enabled; // DAT_0030F144.
    RuntimeActorClock clock;
    RuntimePlayerStrategyResume resume;
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> query_views;
    RuntimeScriptActorQueryContext query{nullptr, query_views, address_zero.data()};
    RuntimeMindTarget* targets = nullptr; // DAT_00545988.
    // DAT_00545988 walks the +8 links of the live attachments on DAT_00545984 (linked by 0x0012AFE0). Each
    // record gets a retail-layout image (+0x10 actor, +0x14.. the record fields) and a target view, kept per
    // record so pointers held by mind contexts stay valid; refresh_mind_targets() rebuilds them each frame.
    struct SceneMindTargetRow
    {
        std::array<u8, 0x68> bytes{};
        std::unique_ptr<RuntimeMindTarget> target;
        RuntimePlayerTargetView view{};
    };
    std::unordered_map<const ActorLiveAttachment*, std::unique_ptr<SceneMindTargetRow>> mind_target_rows;
    std::unordered_map<u32, RuntimePlayerTargetView*> target_views;
    std::unordered_map<u32, RuntimeMindTarget*> target_records;
    // A freed record's last bytes, by its heap address: retail's heap keeps them until the block is reused, and a
    // mind's +0x10 can still name it until the mind next picks a target.
    struct RetiredTarget
    {
        std::array<u8, 0x68> bytes{};
        Actor actor; // The released actor as it was; its model view is gone, so model reads raise.
    };
    std::unordered_map<u32, std::unique_ptr<RetiredTarget>> retired_targets;
    // The copy keeps the actor's own pose, scales and cached matrix; every block it pointed at is freed or owned
    // elsewhere, so those pointers are cleared and reads through them raise or fall back as for an unbound actor.
    static std::unique_ptr<RetiredTarget> retire_target(const std::array<u8, 0x68>& bytes, const Actor& actor)
    {
        auto retired = std::make_unique<RetiredTarget>(RetiredTarget{bytes, actor});
        Actor& copy = retired->actor;
        copy.render_registration = nullptr;
        copy.model = nullptr;
        copy.attach = nullptr;
        copy.bind = nullptr;
        copy.link = nullptr;
        copy.euler_out = nullptr;
        copy.spawn_points = nullptr;
        copy.spawn_point_count = 0;
        copy.accepted_classes = nullptr;
        copy.accepted_class_count = 0;
        copy.spawn_owner = nullptr;
        copy.mask_span = nullptr;
        for (auto& slot : copy.sound_slots)
        {
            static_cast<RuntimeActorSound&>(slot) = RuntimeActorSound{};
        }
        return retired;
    }
    // Player minds register their eligibility (body +0x95C), address and body query views by actor.
    std::unordered_map<Actor*, const RuntimePlayerAttachmentEligibility*> target_eligibility;
    std::unordered_map<Actor*, u32> target_addresses;
    std::unordered_map<const Actor*, RuntimeScriptMindQuery*> target_queries;
    std::unordered_map<u32, std::pair<RuntimePlayerTargetView*, RuntimeMindTarget*>> embedded_targets;
    RuntimeScriptMindQuery absent_target_query{}; // Actors whose +0x95C is not a player body.
    u32 attachment_notice = 0; // DAT_00433BD0.
    const void* script_records = nullptr; // DAT_0051108C.
    std::string goal_scratch; // DAT_00508EB8.
    std::vector<FactoryObject> callout_objects;
    RuntimeDialogueInputState input_state{keyboard, {}, 0, 0, 0, input_pool, 0, nullptr};
    u32 brews_taken = 0; // DAT_00506C88.
    u32 brew_accepted = 0; // DAT_00430D98.
    i32 prompt_lock = -1; // DAT_004E1C34, 0x0011C480.
    u32& prompt_owner = reinterpret_cast<u32&>(dialogue_state.mode); // DAT_004EE1EC, the scene dialogue's mode.
    i32& difficulty = g_event_sequence_state; // DAT_005064D8, the event sequence state.
    std::unique_ptr<RuntimeScoreboardResources> event_board_resources;
    std::unique_ptr<ScriptEventSequenceContext> event_sequence;
    i32 brews_owned = 0; // DAT_00506C8C.
    i32 brews_required = 0; // DAT_00430D9C.
    u8 story_byte = 0; // **(DAT_00506C6C+8) +4.
    u8 unlock_flags = 0; // **(DAT_00506C70+8).
    u32 intro_pending = 0; // DAT_00545B38.
    std::array<char, 0x80> intro_name{}; // DAT_00545B44.
    entity_detail::ListHead& cases = entity_detail::g_world.case_head; // DAT_00511B3C; its next is DAT_00511B40.
    std::function<const EntityBitBlock*(const entity_detail::ListHead*)> case_body_bits;
    // DAT_0050E570 and DAT_0050E660 live in the script's scene words (handler 2 writes them); 0x00116DA0 sets
    // the first queue limit to 3 and the class limits to 99.
    std::span<u32, 0x3C> queue_limits{&scene_word(0x0050E570u), 0x3C};
    std::span<u32, 0x3C> class_queue_limits{&scene_word(0x0050E660u), 0x3C};
    std::unique_ptr<GhoulySpawnContext> ghouly_spawns; // 0x0011B670's spawn views.
    std::unique_ptr<AidSpawnQueueContext> aid_spawns;
    std::unordered_map<const GhoulyController*, Actor*> controller_actors;
    std::unordered_map<u32, Actor*> spawn_players; // Actor address to actor, for DAT_00511B40.
    u32 spawn_player_address = 0; // DAT_00511B40.
    const RuntimeActiveActorLink* active_actors = nullptr; // DAT_00511B48.
    std::array<u8, 8> notice_seen{};
    u32 notice_pending = 0; // DAT_00506E28.
    const Actor* notice_first = nullptr; // DAT_00506DFC.
    u32 notice_id = 0; // DAT_00506E00.
    std::array<RuntimeTutorialNotice, 4> notice_queue{}; // DAT_00506E04.
    i32 notice_count = 0; // DAT_00506E24.
    const Actor* notice_player = nullptr;
    RuntimeTutorialNotices notices{notice_seen, notice_pending, notice_first, notice_id, notice_queue,
        notice_count, notice_player};
    float occupancy_initial = 0.f;
    RuntimeGhoulIntro ghoul_intro;
    std::span<const u8> ghoul_intro_names; // DAT_00506E40, loaded once like 0x0011C3F0 does.
    std::unique_ptr<RuntimeGhoulIntroTunables> ghoul_intro_tunables;
    std::unique_ptr<RuntimeGhoulIntroContext> ghoul_intro_context;
    i32 attachment_limit = 0; // DAT_00428684.
    float skeleton_fetch_delay = 0.f; // DAT_004288AC.
    std::unordered_map<const Actor*, std::unique_ptr<SceneActorModelViews>> model_views;
    // HUD meters the player strategy drives.
    i32 next_widget_id = 0;
    RuntimeDialogueWidgetContext widget_context{resources.text, resources.metrics, {}, nullptr, 640.f,
        next_widget_id, random};
    u32 brew_state = 0; // DAT_00545990.
    float brew_intensity = 0.f; // DAT_00545998.
    RuntimeHudCounter brew_counter{}; // DAT_005459A8.
    std::array<std::span<const u8>, 3> brew_textures{}; // DAT_005459DC.
    std::array<float, 3> brew_half_widths{}; // DAT_005459E8.
    RuntimeDialogueWidget* brew_widget = nullptr; // DAT_005459F4.
    std::unique_ptr<RuntimeBrewMeter> brew;
    i32 health_active = 0; // DAT_00433BE0.
    i32 health_style = 0; // DAT_00433BE8.
    float health_intensity = 0.f; // DAT_00433C14.
    RuntimeHudCounter health_counter{}; // DAT_00433C38.
    u32 health_mode = 0; // DAT_00433C70.
    std::array<std::span<const u8>, 5> health_textures{}; // DAT_00433C7C.
    std::array<u32, 5> health_widths{}; // DAT_00433C90.
    u32 override_health = 0; // DAT_00512230.
    std::unique_ptr<RuntimePlayerHealthDisplay> health_display;
    RuntimeScoreboardGlobals scoreboard_globals;
    std::unique_ptr<RuntimeScoreboardHud> scoreboard_hud; // 0x00132040 / 0x00132100 / 0x00132290.
    std::unique_ptr<RuntimeScoreboardResources> scoreboard_resources;
    std::unique_ptr<RuntimeScoreboardUpdate> scoreboard_update; // 0x00132210.
    std::unique_ptr<RuntimeScoreboardDraw> scoreboard_draw; // 0x00132290's sprite queue and clocks.
    i32 challenge_mode = 0; // DAT_00506C58 (written by game-mode code not yet translated).
    u32 challenge_count = 0; // DAT_00506C84.
    std::unique_ptr<NativeActorFactoryOwner> actors;
    std::unique_ptr<NativePlayerFactoryOwner> players;
    // Scene classes (area, trigger, FX emitter).
    const RuntimeAreaDialoguePool* area_dialogue = nullptr;
    RuntimeActorWordBindings word_messages;
    std::unordered_map<u32, void*> message_references; // Native references carried by temporary Xbox message words.
    std::vector<RuntimeFxEmitterPlayer*> emitter_players;
    entity_detail::DisplayView*& display_view = entity_detail::g_display_view; // DAT_005077A8.
    entity_detail::ListHead emitters{}; // DAT_00511B2C.
    RuntimeIntroNotices intro_notices;
    std::unique_ptr<ActorFrameContext> frame;
    std::unique_ptr<NativeSceneEmitterBindings> emitter;
    std::unique_ptr<RuntimeMarkerObjects> marker_objects;
    std::unique_ptr<NativeSceneActorClassOwner> classes;
    RuntimeWeaponStatTables weapon_stats; // DAT_005461F4..DAT_005461FC, boot-loaded (0x00033030 -> 0x001486C0).
    std::unique_ptr<RuntimeWeaponServices> weapon_services;
    RuntimeActorCreationOptions candle_hit{}; // DAT_004E1C5C.
    RuntimeCandleFlameSpot candle_spot{}; // DAT_0050D740..DAT_0050D74C.
    std::unique_ptr<RuntimeCandleServices> candle_services;
    std::unordered_set<const void*> weapon_bound; // Native blocks a weapon message bound for its words.
    // Backgrounds (factory type 4, vtable 0x004E1644) and the scene globals their constructor and message
    // slot use. The part tables (0x00044CA0, boot) and sector settings (0x000FEF60) load with the services.
    std::array<char, 0x100> room{}; // DAT_00545D40.
    std::array<BackgroundRegion, 8> background_regions{}; // DAT_0050EAA0, stride 0x2C.
    i32 background_region_count = 0; // DAT_0050EC00.
    std::array<std::span<u8>, 4> sector_grids{}; // DAT_0050D770.
    i32 sector_count = 0; // DAT_0050D780.
    float sector_minimum = 0.f; // DAT_0050D784.
    float sector_maximum = 0.f; // DAT_0050D788.
    float sector_saturation = 0.f; // DAT_0050D78C.
    float sector_low = 0.f; // DAT_0050D790.
    float sector_high = 0.f; // DAT_0050D794.
    RuntimeBackgroundSectors sectors{sector_grids, sector_count, sector_minimum, sector_maximum, sector_saturation,
        sector_low, sector_high, {}, 0};
    RuntimeBackgroundAmbient background_ambient = runtime_background_ambient();
    i32 disable_framebuffer_clear = 0; // DAT_004DEA50.
    std::span<const u8> part_settings; // DAT_00546294.
    i32 part_setting_count = 0; // DAT_00546298.
    std::span<const u8> part_text; // DAT_0054629C.
    BackgroundPartTables part_tables{part_settings, part_setting_count, part_text};
    i32 part_sound_slot = 0; // DAT_00546290.
    u32 broken_parts = 0; // DAT_00545978.
    std::array<float, 24> sampling_scratch{}; // DAT_0050D7A0: matrix, then the dialogue's scratch matrix.
    float* sampling_active = nullptr; // DAT_0050DD70.
    std::array<char16_t, 0x400> wide_scratch{}; // DAT_00507E78.
    u32 score_total = 0; // DAT_0054597C.
    u32 score_running_total = 0; // DAT_00545980.
    std::array<char16_t, 0xA00> score_texts{}; // DAT_00432724.
    std::array<u32, 10> score_categories{}; // DAT_004326FC.
    std::array<u32, 10> score_points{}; // DAT_00433B24.
    u32 score_refresh = 0; // DAT_004326E8.
    std::unique_ptr<RuntimeScoreAwards> score;
    entity_detail::NavigationCellRegistration* door_pool = nullptr; // DAT_004EE2B8, allocated by 0x0011CBD0.
    std::vector<RuntimeDoorClips> door_rows;
    u32 door_camera_registered = 0; // DAT_004EE31C.
    u32 door_paused = 0; // DAT_004EE320.
    u32& door_camera_hold = reinterpret_cast<u32&>(g_4dea24); // DAT_004DEA24.
    EntityCamNode door_camera{}; // DAT_004EE2BC, the door clips' camera view.
    std::array<DoorRoomAlias, 2> door_aliases{}; // PTR_DAT_00434034.
    std::vector<const char*> door_variants; // PTR_DAT_00424970.
    std::vector<const char*> door_shapes; // PTR_DAT_00425118.
    std::unique_ptr<RuntimeDoorNames> door_names;
    std::function<u32(const RuntimeCutsceneInstance*)> door_instance_address;
    std::function<RuntimeDoorActorModels(Actor*)> door_models;
    std::unique_ptr<RuntimeDoorContext> doors;
    std::unordered_map<const u8*, std::unique_ptr<SceneBackgroundViews>> background_views;
    std::function<u32(u8*, u32)> background_allocation; // Engine-heap blocks stored as words.
    std::function<u32(const void*)> background_address;
    std::unique_ptr<RuntimeModelSoundRelease> effect_release;
    entity_detail::NavigationSceneStorage navigation;
    std::unique_ptr<NativeBackgroundClassOwner> backgrounds;
    // Cutscene runtime (door clips, scripted cutscenes): native storage of its retail globals.
    std::vector<const char*> footstep_kinds; // PTR_DAT_00423620.
    std::array<char, 0x80> replacement_source{}; // DAT_004E1AC0.
    std::array<char, 0x80> replacement_target{}; // DAT_004E1A40.
    std::vector<FactoryObject> cutscene_actors; // Track actors 0x0004DEC0 creates.
    std::function<FactoryObject*(Actor*)> actor_objects; // Native track actor -> its factory object.
    std::unordered_map<Actor*, u32> actor_object_addresses;
    std::unordered_map<Actor*, std::function<void(Actor&)>> actor_updates; // Actor vtable +0x10.
    std::unordered_map<Actor*, std::unique_ptr<RuntimeSegmentContactActor>> prop_contact_views;
    std::unordered_map<const u8*, Actor*> prop_contact_owners; // A prop contact view's object bytes and its key.
    std::unordered_map<const NativePlayerInstance*, std::unique_ptr<RuntimeSegmentContactActor>> player_contact_views;
    // Contact counts by actor tag, 0x3D words per tag (0xEA tags): DAT_0053E528 and DAT_00524E2C.
    std::vector<u32> contact_totals = std::vector<u32>(size_t(0xEA) * 0x3D);
    std::vector<u32> contact_session = std::vector<u32>(size_t(0xEA) * 0x3D);
    AudioBankStreams* audio_banks = &audio.streams; // DAT_004DF370, DAT_004E030C.
    std::unique_ptr<AttractControlServices> attract_services; // Factory type 0x9B.
    std::vector<TrophyHeadRow> trophy_head_rows; // DAT_00301920.
    std::unique_ptr<TrophyHeadsControlServices> trophy_heads_services; // Factory type 0x92.
    std::unique_ptr<HauntedChairsControlServices> haunted_chairs_services; // Factory type 0x93.
    std::unique_ptr<RelicsStoreControlServices> relics_store_services; // Factory type 0x94.
    std::unique_ptr<SceneFxControlServices> fx_services; // Factory type 0x97.
    std::unique_ptr<Chapter2Scene4ControlServices> chapter2_scene4_services; // Factory type 0x95.
    RuntimeBackgroundMorphs* background_morphs = nullptr; // DAT_0050E388 while the scene's cloths exist.
    RuntimeBackgroundCloths background_cloths; // DAT_0050E390, DAT_0050E394, DAT_0050E388 and DAT_0050E418.
    u32 cloth_ramp_texture = 0; // DAT_005043B8.
    RuntimeShadowProjector shadow_projector; // DAT_00504880..DAT_00504950.
    std::unique_ptr<RuntimeBackgroundClothContext> cloth_context;
    std::unique_ptr<RuntimeBackgroundClothScene> cloth_scene;
    RuntimeBackgroundClothScene& background_cloth_scene();
    std::unique_ptr<SuperScaryControlServices> super_scary_services; // Factory type 0x98.
    std::unique_ptr<MapViewControlServices> map_view_services; // Factory type 0x9E.
    std::vector<const char*> powerup_names; // DAT_0042ABF8, twenty "powerupId_" names.
    std::unique_ptr<RuntimePowerupTable> powerup_table;
    std::unique_ptr<RuntimePowerupCaption> powerup_caption;
    std::unique_ptr<RuntimeBonusCounters> bonus_counters;
    std::unique_ptr<SaveGameContext> challenge_saves; // 0x000F24C0's save of the current game.
    std::unique_ptr<RuntimeChallengeAward> challenge_award;
    std::unique_ptr<RuntimePowerupAwardContext> powerup_awards;
    u32 super_scary_auto = 0; // DAT_005122F0 (BSS; its writer is not native).
    void* super_scary_text = nullptr; // DAT_00512484 (BSS; its writer is not native).
    NativePlayerInstance& case_player(u32 address); // A player by its actor address.
    void bind_button_bat(FactoryObject& object);
    // Factory type 0x9A, game select (docs/NATIVE_FRONTEND.md).
    std::unique_ptr<GameSelectControlServices> gameselect_services;
    std::span<const u8> gameselect_text; // DAT_005462C4
    std::span<const u8> scene_order_held; // DAT_00511B64
    u32 scene_order_release = 0; // DAT_00511B68
    float gameselect_stream_wait = 0.f; // DAT_005065C4
    std::span<u8> frontend_input; // DAT_00429BBC, the boot keyboard device (0x000E5A80), mapped while the scene runs.
    u32 frontend_input_address = 0;
    std::array<char16_t, 0x200> gameselect_lookup{}; // DAT_00507E78, the 0x000FF0A0 text buffer.
    void create_scene_objects(); // Xbox 0x0011C980
    void bind_gameselect_services();
    void bind_book_services();
    RuntimeScreenTextureBoundaries screen_texture_boundaries();
    std::array<std::span<u8>, 2> book_pages{}; // DAT_005047D8, DAT_005047E4
    std::unique_ptr<TransitionBookServices> book_services;
    std::unique_ptr<SceneTransitionServices> transition_services; // The scene transition's (DAT_00546328).
    void bind_transition_services();
    RuntimeModelRenderContext model_renderer{}; // DAT_004370A0..
    RuntimeSceneFog fog{}; // DAT_00509AFC..
    CutsceneCameraImpulse camera_impulse{}; // DAT_0050E7BC..
    CutsceneCameraEventAsset camera_event_asset{}; // DAT_004E1C4C..
    Vec3 listener_position{};
    u32 movie_flag_38 = 0;
    u32 movie_flag_3c = 0;
    // The boot-owned movie event slots (DAT_0050DD90) and their mode (DAT_0050DD8C).
    std::array<RuntimeMovieEventSlot, 4>& movie_slots = game.native_movies->session->slots;
    i32& movie_mode = game.native_movies->mode;
    std::unique_ptr<CutsceneActorFactoryContext> cutscene_factory;
    std::unique_ptr<CutsceneActorInitializationContext> cutscene_initialization;
    std::unique_ptr<CutsceneRuntimeContext> cutscenes;
    // Scene dialogue runtime (activation 0x000584D0, update 0x00057E60, draw 0x00058150) and the boot
    // display/text globals it reads (0x00033030: 0x000FC310 -> 0x000FC190, 0x000F5900, 0x00060320).
    std::array<u16, 2> hud_size{640, 480}; // DAT_005096E8 / DAT_005096EA, the D3D display mode.
    RuntimeHudLayout hud{}; // DAT_00509708..
    std::span<const u8> fallback_texture; // DAT_004E1C58.
    i32 paragraph_disabled = 0; // DAT_0050444C.
    std::array<std::span<const u8>, 2> text_fonts{}; // DAT_00504444.
    RuntimeDialogueDrawState dialogue_draw_state{}; // DAT_005043D8.., then DAT_00424900.. data.
    GhoulySpawnPool* text_registry = nullptr; // DAT_005043CC.
    i32 text_registry_ready = 0; // DAT_005043D0.
    std::array<RuntimeDialogueTextTable, 2> text_tables{}; // DAT_005043BC.
    std::unique_ptr<RuntimeDialogueTextSystem> text_system;
    std::vector<const char*> widget_icon_names; // DAT_003150D0.
    RuntimeDialogueActivationState dialogue_state{};
    u32 frontend_pause = 0; // DAT_004E173C.
    std::array<char, 0x800> narrow_scratch{}; // DAT_00508EB8, the dialogue's view of the shared scratch.
    DialogueOwnedResources dialogue_owned{};
    std::array<i32, 4> trophy_thresholds{}; // DAT_004E17DC.
    u32 trophy_flags = 0; // DAT_00424958.
    std::array<char, 256> trophy_names{};
    DialogueTagState tag_state{};
    DialogueSoundTagState sound_tags{};
    std::unique_ptr<DialogueTrophyContext> trophies;
    std::unique_ptr<DialogueTagContext> tags;
    std::unique_ptr<DialogueMeasurementContext> measurement;
    DialoguePageLayout dialogue_page{};
    DialoguePageSettings page_settings{};
    std::vector<FactoryObject> dialogue_bats;
    std::unique_ptr<DialogueWidgetAllocationContext> widget_allocation;
    std::unique_ptr<DialogueWidgetBatFactoryContext> widget_bats;
    AudioListenerSettings listener = native_audio_listener_settings();
    std::function<DialogueWidgetBatView(Actor*)> bat_views; // Live button-bat actor views.
    std::unordered_map<u32, std::function<void()>> widget_callbacks;
    std::unique_ptr<DialogueWidgetUpdateContext> widget_update;
    std::unordered_map<u32, RuntimeDialogueInputDevice> input_views;
    std::unique_ptr<RuntimeDialogueActivationContext> dialogue;
    std::unique_ptr<RuntimeScriptDialogueContext> script_dialogue;
    // The device is boot-owned with the vertex shader cache whose handles it created.
    SceneD3DDevice& d3d_device = resources.device;
    std::unique_ptr<RuntimeBootVertexShaders> boot_vertex_shaders;
    std::unique_ptr<RuntimeBootPixelShaders> boot_pixel_shaders;
    std::vector<std::pair<u32, u32>> boot_pixel_allocations; // Handle and owned definition allocation.
    std::array<RuntimeModelRenderFrame, 32> render_frame_storage{}; // DAT_004392B0, retail stride 0xC4.
    std::unordered_map<RuntimeModelRenderFrame*, DialogueRenderFrame> render_frames;
    std::array<u32, 0x200> deferred_states{}; // DAT_00181968 and D3D render-state shadows.
    u32 light_override = 0; // DAT_00509B5C.
    std::array<float, 16> dialogue_projection{}; // DAT_00506F58.
    std::array<float, 16> dialogue_view{}; // DAT_00506F18.
    std::array<float, 16> dialogue_world{}; // DAT_00506ED8.
    std::array<float, 16> sprite_world{}; // DAT_00504378.
    std::unique_ptr<RuntimeDialogueDrawContext> dialogue_draw;
    DisplayClipBounds clip_bounds{}; // DAT_0050EA00.
    std::array<float, 16> device_projection{}; // DAT_005077AC.
    std::array<float, 16> device_view{}; // DAT_00506E98.
    std::array<float, 16> device_world{}; // DAT_00506E58.
    std::array<float, 16> projection_transpose{}; // DAT_00506FD8.
    std::array<float, 12> default_world{}; // DAT_0042A7C0.
    u32 display_address = 0x00507018; // DAT_005077A8, retail address of the current display record.
    std::unique_ptr<RuntimeDisplaySetupContext> display_setup;
    DialogueRenderTargetState render_targets{};
    std::array<RuntimeDialogueQueuedSprite, 64> sprite_records{}; // DAT_00511210.
    i32 sprite_count = 0; // DAT_00511A10.
    std::unique_ptr<RuntimeDialogueSpriteQueue> sprites;
    u32 text_draw_count = 0; // DAT_004E61C8.
    u32 capture_enabled = 0; // DAT_005077F8.
    u32 line_retained = 0; // DAT_005044B0.
    std::unique_ptr<RuntimeDialogueOuterDrawContext> dialogue_outer;
    // Scene model renderer (0x000F9080) and the retail globals it reads.
    std::array<const u8*, 4> ambient_grids{}; // DAT_0050D770 as the ambient sampler reads it.
    std::array<SceneAmbientPoint, 32> ambient_points{}; // DAT_00511B70.
    u32 ambient_points_enabled = 0; // DAT_005120F0.
    i32 ambient_point_free = 0; // DAT_005120F4.
    u32 ambient_point_fc = 0; // DAT_005120FC.
    u32 ambient_point_100 = 0; // DAT_00512100.
    RuntimeScenePointLights point_lights{ambient_points, ambient_points_enabled, ambient_point_free,
        ambient_point_draw, ambient_point_fc, ambient_point_100};
    RuntimeSceneLightGrid light_grid; // DAT_0050E374..DAT_0050E384.
    RuntimeSceneLightGridD3D light_grid_d3d;
    RuntimeScenePointLightTargets point_light_targets; // DAT_00504850..DAT_00504878.
    std::unique_ptr<RuntimeScenePointLightPass> point_light_pass;
    std::array<float, 3> ambient_scale{}; // DAT_00509B20.
    u8 environment_world_space = 0; // DAT_004D9668.
    std::array<float, 4> retained_scene_color{}; // DAT_0032E4E4.
    std::array<u32, 20> light_records{};
    SceneLightUploadBoundaries light_boundaries;
    std::unique_ptr<SceneLightUploadContext> light_upload;
    RuntimeSceneLightDirection& light_direction = g_scene_light_direction; // DAT_00509B10..
    std::array<float, 4> override_base_color{}; // DAT_0042A570.
    std::array<u32, 16> environment_primary{}; // DAT_004D9710.
    std::array<u32, 16> environment_secondary{}; // DAT_004D9680.
    std::array<float, 3> scene_color{}; // DAT_00509B30.
    RuntimeRenderLight directional_light{}; // DAT_00509AB0.
    std::array<RuntimeRenderLight, 2> temporary_lights{}; // DAT_005097B8.
    RuntimeRelocationImage render_image{{}, 0, {}};
    std::array<ModelTreeStackEntry, 1024> tree_stack{}; // DAT_004372A8.
    i32 tree_stack_index = -1; // DAT_0043F4B0.
    std::array<RuntimeShaderMatrix, 32> frame_combined{}; // Each render frame +0.
    std::array<const RuntimeShaderMatrix*, 32> frame_cameras{}; // Each render frame +0x40.
    u32 render_retained_204 = 0;
    u16 render_mask = 0;
    u32 fog_pixel_state = 0; // DAT_00181CE4.
    u32 cull_mode = 0; // DAT_0032E3D0.
    std::unordered_map<u32, std::function<void(u32, void*)>> node_callbacks;
    std::unique_ptr<ModelTreeDispatchContext> tree_dispatch;
    std::array<u32, 67> stream_descriptors{}; // Render state +0x78 up to the colour at +0x184.
    u32 active_pixel_shader = 0; // DAT_0054D948.
    u32& depth_function = deferred_states[(0x00181C54 - 0x00181968) / 4];
    u32& blend_enabled = deferred_states[(0x00181C5C - 0x00181968) / 4];
    std::unique_ptr<RuntimeModelShaderStates> model_shader_states;
    std::unique_ptr<RuntimeModelShaderDraw> model_shader_draw;
    std::unique_ptr<RuntimeModelShaderCombiner> model_shader_combiner;
    u32 shader_texture_stage = 0; // DAT_005078D0.
    std::unique_ptr<ModelRenderNodeContext> render_nodes;
    std::unordered_map<RuntimeModelInstance*, const u8*> flag_origins; // Model +0x294.
    std::unique_ptr<ModelPushBufferDrawContext> push_buffers;
    std::unique_ptr<SceneEnvironmentSetupContext> environment;
    std::unique_ptr<SceneLightConstructionContext> light_construction;
    std::unique_ptr<SceneAmbientContext> ambient_context;
    std::unique_ptr<SceneModelDrawContext> model_draw;
    std::unique_ptr<u8, void (*)(u8*)> model_matrix_allocation{nullptr, [](u8* block)
        {
            engine_heap_free(block);
        }};
    RuntimeModelMatrixScratch model_matrix_scratch;
    std::span<RuntimeShaderMatrix> published_model_matrices; // Native render state +0x190.
    std::unique_ptr<RuntimeModelMatrixArrayDraw> matrix_array_draw;
    std::unique_ptr<RuntimeModelTransformDraw> transform_draw;
    u16 model_projection_register = 0; // Render state +0x194, BSS until vertex shader draw.
    u16 model_world_register = 0; // Render state +0x196.
    u32 vertex_fragment_word; // Retained lookup word; real fragment lookup writes before use.
    std::unique_ptr<RuntimeShaderFragmentContext> vertex_fragments;
    std::unique_ptr<RuntimeModelVertexShaderCacheContext> vertex_cache;
    std::unique_ptr<RuntimeModelVertexShaderDraw> vertex_draw;
    std::array<float, 4> vertex_parameters{}; // Render state +0x198, BSS/renderer boot.
    std::array<float, 8> vertex_auxiliary{}; // Render state +0x1B0.
    RuntimeModelMaterialGlobals material_globals;
    RuntimeModelTextureRecords texture_records;
    std::unique_ptr<RuntimeModelMaterialBind> material_bind;
    std::unique_ptr<RuntimeModelBlendshapeDraw> blendshape_draw;
    std::unique_ptr<RuntimeScreenModelDraw> screen_model_draw;
    std::unique_ptr<RuntimeButtonBatDraw> button_bat_draw;
    u32 node_registry_count = 0;
    std::unique_ptr<RuntimeShaderFragmentTable> shader_fragments =
        std::make_unique<RuntimeShaderFragmentTable>();
    std::unordered_map<Actor*, RuntimeActorMatrixState> draw_matrices; // Actor +0x288 views for draws.
    std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback> attachment_draws; // Attachment vtable +0x14.
    std::unique_ptr<RuntimeFxCustomContext> fx_custom; // Types 0xC and 0x2E.
    std::unique_ptr<RuntimeGenerateGhoulyScene> generate_ghouly; // Type 0x6F's scene views.
    // Markers.
    const u8* markers = nullptr; // DAT_0051108C.
    RuntimeMarkerCameraStart camera_start{}; // DAT_00545AD0.
    std::unique_ptr<RuntimePlayerSpawnContext> spawn;
    std::unique_ptr<RuntimeMarkerPlayerStart> player_start;
    Vec3 marker_nearest_retained{}; // Retained local stack bytes in 0x0011EFA0.
    Vec3 marker_dropper_retained{}; // Retained local stack bytes in 0x00100600.
    std::unique_ptr<RuntimeMarkerAdditionalBindings> marker_additional;
    std::unique_ptr<RuntimeSceneMarkerBindings> marker_bindings;
    RuntimeSceneMarkerHandlers handlers;
    // The scene record's name (DAT_00510E58) the spawn and player bindings read, copied as each scene begins.
    std::array<char, 0x80> scene_name{};
    // The services are boot-owned like retail's globals: each scene begins (its markers and scene data) and ends
    // (0x0011C5A0's teardown of its actors, cutscenes, doors and effects) inside the same services.
    bool in_scene = false;
    bool first_scene = true;
    std::function<void(const u8*)> released_binding;
    void begin_scene();
    void begin_teardown(); // 0x0011C5A0's 0x000584D0, 0x0004ECE0 and 0x0005B1E0
    void retire_actors(); // 0x0011C5A0's 0x00122C70 and 0x001232D0
    void end_scene();
    void reset_scene_data();

    std::span<const u8> data(u32 address, size_t size) const
    {
        return image.memory(address, size);
    }
    // A shader record's constant pointer as its 0xC dwords in the image, or null when the pointer is 0.
    const u32* shader_constants_at(u32 pointer) const
    {
        const u32 address = read_u32(data(pointer, 4), 0);
        return address == 0 ? nullptr : reinterpret_cast<const u32*>(data(address, 0x30).data());
    }
    std::vector<const char*> strings(u32 table, size_t count) const
    {
        std::vector<const char*> names(count);
        const auto words = data(table, count * 4);
        for (size_t index = 0; index < count; ++index)
        {
            names[index] = image.string(read_u32(words, index * 4));
        }
        return names;
    }
    FactoryObject* object_of(const u8* bytes);
    bool release_actor_frame_object(Actor& actor, u32 object); // Xbox 0x0003DE30, ECX actor, ESI object
    u32 address_of(const void* pointer);
    // 0x0003BE20 over a scene actor's +0x8C callout records with callback 0 and value 0.
    void actor_callouts(u32 address, u32 id, const RuntimeActorCreationOptions* options);
    RuntimeBodyStateContext body_state_context(u32 body_address); // A state action's body views (state +8).
    void refresh_mind_targets();
    // The base strategy messages' view of the actor at a retail address: its +0x1FC / +0x1FE words, its +0x8F8 target
    // record and, for a player, the actor its body +0x274 names. Rebuilt on each call; kept per address.
    RuntimeStrategyMessageActor* strategy_message_actor(u32 address);
    std::unordered_map<u32, std::unique_ptr<RuntimeStrategyMessageActor>> strategy_message_actors;
    std::unordered_set<u32> strategy_message_building;
    // The emitters' DAT_00511B40 walk (0x0010A590): a view of each player on the list, in list order.
    void refresh_emitter_players();
    std::vector<std::unique_ptr<RuntimeFxEmitterPlayer>> emitter_player_views;
    // Engine-heap blocks (0x000F72D0 allocations) the scene's objects store: a token address per block.
    std::map<const void*, u32> heap_addresses;
    std::map<u32, std::span<const u8>> heap_blocks; // heap_address's blocks by address, for memory_at.
    u32 next_heap_address = 0x30000000;
    u32 heap_address(const void* pointer, size_t size);
    NativePlayerModelViews model_views_of(const Actor& actor);
    SceneActorBase& base_of(FactoryObject& object);
    void actor_message(Actor& actor, const void* message);
    void object_message(FactoryObject& object, std::span<u32> message);
    void route_object_message(FactoryObject& object, std::span<u32> message);
    void refresh_shape_poses(FactoryObject& object);
    void attach_player_messages(FactoryObject& object, NativePlayerInstance& player);
    void child_message(FactoryObject& child, void* message);
    // The player's body graph (its body vtable slots); built on first use, as the body's own messages build it.
    SceneBodyMessages& body_graph(NativePlayerInstance& player);
    void activate_actors();
    RuntimeSceneActivation activation_context();
    RuntimeDirectorActor director_actor(NativePlayerInstance& player);
    // An attachable actor by retail address: a player's own view, or a weapon (types 0x0B, 0xAA) as a mind's pending
    // pickup (+0x134) names it.
    RuntimeBodyAttachedActor& attached_actor_at(u32 address);
    std::unordered_map<u32, std::unique_ptr<RuntimeBodyAttachedActor>> weapon_attachments;
    // A weapon's views as 0x0001B190 attaches it: attached_actor_at's view, its native actor, animation and pose,
    // and its animation receiver (body +0x10 -> vtable +0xC). Also listed in attachment_actors.
    struct WeaponPickupView
    {
        RuntimeBodyAnimationReceiver receiver;
        std::optional<RuntimePlayerAttachmentActor> pose;
    };
    std::unordered_map<u32, std::unique_ptr<WeaponPickupView>> weapon_pickups;
    RuntimePlayerAttachmentActor& weapon_attachment_at(u32 address);
    std::vector<RuntimeDirectorActor> marked_actors(); // DAT_00511B38 (players' +0x934 nodes) in list order.
    RuntimeActorPauseGroups& refresh_pause_groups(); // pause_groups over the current DAT_00511B38 list.
    void update_player(NativePlayerInstance& player); // Player vtable +0x10, 0x0002C2B0.
    void update_contact_prop(FactoryObject& object);
    void update_knockdown_sphere(FactoryObject& object); // Type 0x19, vtable +0x10 0x0006EBA0.
    void update_spider_web(FactoryObject& object); // Type 0x1B, vtable +0x10 0x000705B0.
    // DAT_00511B38: the players' +0x934 nodes in list order.
    std::vector<RuntimeSegmentContactActor*> marked_players();
    void update_base_actor(FactoryObject& object);
    RuntimeSegmentContactActor& prop_contact_actor(entity_detail::SegmentProbe& probe);
    RuntimeSegmentContactActor& segment_contact_actor(entity_detail::SegmentProbe& probe);
    // A ghouly spawn record's controller (record +0x24) that is a player: its request goes to the actor's vtable +0xC.
    void player_controller_message(GhoulyController& controller, void* data);
    void dispatch_contact_words(NativePlayerInstance& player, std::span<u32> words, GhoulySpawnPool* contacts,
        size_t contacts_word);
    void latch_message(entity_detail::LatchNode& node, const Message& message);
    void unlink_active_node(const u8* bytes); // 0x0003EBA0's +0x900 unlink from DAT_00511AA4.
    std::unique_ptr<RuntimeBackgroundConstructionServices> background_services(NativeBackgroundInstance& background,
        NativeActorFactoryInstance& view, RuntimeBackgroundAnimation*& first_animation);
    void background_message(NativeBackgroundInstance& background, std::span<u32> message);
    // The background's message context (0x00043E50's part state and the scene's tables), for `use`.
    void with_background_context(NativeBackgroundInstance& background,
        const std::function<void(Actor&, BackgroundMessageContext&)>& use);
    NativeBackgroundInstance& background_at(u32 address); // A background by retail address; raises otherwise.
    void background_update(NativeBackgroundInstance& background);
    void update_actor(u32 address);
    // ndMtxArray bindings (+0x20, count +0x24) of an actor's model, for 0x000400D0; built once per model.
    const ActorMatrixBindings& actor_matrix_bindings(const Actor& actor);
    std::unordered_map<const u8*, std::unique_ptr<ActorMatrixBindings>> model_matrix_bindings;
    // DAT_00511B40 players as 0x00129420 reads them, rebuilt on each walk. The mind view snapshots the body's
    // +0x48 case actor, bytes +0x30..+0x96 and +0x15C bits; +0x5C (read only for flag 0x100000), the +0x958
    // reference (flags 0x400 / 0x40000) and the +0xCE0 marker's record are not viewed.
    struct SceneCaseActor
    {
        RuntimeScriptMindQuery mind{};
        u16 tag = 0;
    };
    std::vector<std::unique_ptr<SceneCaseActor>> case_actor_views;
    std::vector<RuntimeScriptActorQueryView> case_actors();
    FactoryObject& object_of_actor(Actor& actor);
    void start_background();
    void notify_body_list(u32 id); // Message {id} to each DAT_00511B20 (+0x934 list) node's actor.
    void message_body_list(std::span<const u32> words); // 0x00122F70 over DAT_00511B20.
    void actor_draw(Actor& actor, i32 view, i32 pass); // Actor vtable +0x14 by class.
    u32 actor_visible(Actor& actor, i32 pass, i32 view); // Actor vtable +0x18 by class.
    void draw_scene_actor(FactoryObject& object, Actor& actor, i32 kind); // 0x0003C9E0.
    void draw_player(FactoryObject& object, NativePlayerInstance& player, i32 kind); // 0x00028050.
    void present_view(i32 view); // 0x0011C240 for one view.
#if defined(GBTG_INSPECTOR_APP)
    void with_view_context(i32 view, const std::function<void(RuntimeSceneViewPresent&)>& present);
    std::map<size_t, u32> inspection_draw_owners;
#endif
    // The ambient sampler's view of DAT_0050D770/DAT_0050D780 (filled by 0x000FEFD0), the DAT_00509B20..28 tint and
    // DAT_005120F0, refreshed before a view draws.
    void refresh_ambient_context();
    RuntimeSceneMirrors mirrors; // DAT_00511090..DAT_00511180, filled by 0x0011F450 at scene start.
    u32 mirror_pass = 0; // DAT_0051120C.
    // Gates of 0x0011C240's passes whose writers are not translated; they hold their BSS value.
    u32 post_model_count = 0; // DAT_00512AF0 (0x0012A9B0 / 0x0012A9E0 / 0x0012AA70).
    u32 decal_count = 0; // DAT_00510560 (0x0011AEA0 / 0x0011AEE0 / 0x000BA840).
    u32 caption_camera = 0; // DAT_004DE9AC (0x00032AB0).
    u32 caption_widget = 0; // DAT_004DE9A0 (0x00032AB0).
    i32 frontend_state = 0; // DAT_004E17C8 (0x00033030, 0x0004A4D0).
    u32 frontend_widget = 0; // DAT_004E19A0 (0x00049DE0 .. 0x0004A4D0).
    u32 hud_enabled = 0; // DAT_005493E0 (0x00132040 in 0x0011CBD0).
    u32 screen_flash = 0; // DAT_00504478 (0x00060BC0 / 0x00060C80 / 0x00060D50).
    u32 screen_capture = 0; // DAT_005044A4 (0x00060C20 / 0x00060C80 / 0x00060D50).
    void view_refresh_slot(Actor& actor); // Actor vtable +0x38 by class (0x00121DC0).
    // 0x000EF2D0 on an effect node an actor owns (the bone debris' +0x934), with the scene's particle simulation.
    void tick_owned_effect(ActorEffectNode& effect, float step);
    void draw_anchor_sprites(); // 0x00121EC0.
    void draw_scene_controls(); // DAT_0050E438: each control's vtable +0x14.
    u32 bound_tested = 0; // DAT_00546310
    u32 bound_culled = 0; // DAT_0054630C
    u8 bound_plane = 0; // DAT_0043F4B8
    RuntimeModelBoundCulling bound_culling{bound_tested, bound_culled, bound_plane};
    std::array<const u8*, 10> overlay_digits{}; // DAT_00545A80, BSS until 0x0012E070 loads the digit sprites.
    ActorDrawState actor_draw_state(FactoryObject& object, Actor& actor, std::unique_ptr<ActorDrawModelView>& model);
    // Host presentation of the recorded dialogue quads, with decoded images of their texture frames.
    std::unordered_map<const u8*, std::unique_ptr<RgbaImage>> texture_images;
    const RgbaImage& texture_image(const u8* record);
    // Drops the decoded images of records in host bytes about to be freed, with their presenter uploads: the
    // allocator can hand the same bytes (and a new image the same address) to a different texture.
    void forget_texture_images(const u8* begin, size_t size);
    // D3DTexture_LockRect (0x00177950) for a movie frame: the texture's registered data, linear 32-bit rows.
    // The texture is marked so its decoded image is rebuilt, with a new serial, the next time it is drawn.
    RuntimeMovieLockedSurface lock_movie_texture(u32 record);
    // The recorded SetRenderTarget: draws go to the texture behind a surface until the back buffer returns, then
    // the rendered pixels are written to the texture's data (linear 32-bit) and it is decoded again.
    void present_render_target(std::uintptr_t color, std::uintptr_t depth);
    u32 render_target_depth_texture = 0; // A texture surface given as the open target's depth.
    void copy_back_buffer(u32 texture);
    u32 render_target_texture = 0;
    std::unordered_set<const u8*> movie_textures;
    // D3DDevice_CreateTexture2 records (0x14-byte headers in the resource arena), decoded from their arguments.
    std::unordered_set<u32> created_textures;
    std::vector<std::unique_ptr<RuntimeActorSnapshot>> actor_snapshots; // DAT_005462F4.
    i32 actor_snapshot_count = 0; // DAT_005462F0.
    RuntimeActorSnapshotD3D snapshot_d3d;
    std::unique_ptr<RuntimeActorSnapshots> snapshots;
    std::vector<SceneD3DCommand> snapshot_saved; // The frame's commands while a snapshot is recorded.
    RuntimeGlowTargets glow_targets;
    std::unique_ptr<RuntimeGlowPass> glow_pass;
    std::unique_ptr<RuntimeAvatarGlow> avatar_glow;
};

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
            return actor_world_position(*actors->actors.at(scene.resolve_address(address, 0).data()));
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
      scares{scene.factory, image.pools, scene.objects, scare_flags, player_link, default_euler},
      intro_sound{reinterpret_cast<u32&>(health_style), intro_scale,
          reinterpret_cast<i16&>(scoreboard_globals.health_second_voice), story_byte, sounds},
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
    std::memcpy(&attachment_limit, data(0x00428684, 4).data(), 4);
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
    for (size_t row = 0;; row += 0x87)
    {
        if (row + 0x87 > statsheet.size())
        {
            throw Error("statsheet has no objTag_Null row");
        }
        ++statsheet_rows;
        if (std::strcmp(reinterpret_cast<const char*>(statsheet.data() + row), null_tag) == 0)
        {
            break;
        }
    }
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
        brew_textures, brew_half_widths, brew_widget, widget_context, data(0x00312A1C, 0x14), resources.registry,
        resources.error, address_zero});
    health_display = std::make_unique<RuntimePlayerHealthDisplay>(RuntimePlayerHealthDisplay{health_active,
        health_style, health_intensity, health_counter, health_mode, health_textures, health_widths, override_health,
        story_byte, data(0x00313A40, 5 * 0x18), data(0x00313A18, 0x14), data(0x00313A2C, 0x14), resources.registry,
        resources.error});
    scoreboard_hud = std::make_unique<RuntimeScoreboardHud>(RuntimeScoreboardHud{scoreboard_globals, *brew,
        *health_display, overlay_digits, powerup_board, hud_enabled, g_flags_ea9c, score_refresh, score_points,
        health_style, difficulty, prompt_lock, prompt_owner, hud, hud_size[0], data(0x00313390, 12 * 0x80),
        read_u32(data(0x00314F48, 4), 0), resources.registry, resources.error,
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
        const auto constant = [this](u32 address)
        {
            float value;
            std::memcpy(&value, data(address, 4).data(), 4);
            return value;
        };
        scoreboard_resources = std::make_unique<RuntimeScoreboardResources>(RuntimeScoreboardResources{
            resources.registry, resources.error, data(0x00314170, 0x10)});
        scoreboard_update = std::make_unique<RuntimeScoreboardUpdate>(RuntimeScoreboardUpdate{*scoreboard_hud,
            *scoreboard_resources, audio.voices, g_510ce8, *reinterpret_cast<const float*>(g_4312c8), g_4312cc,
            g_scene_flags->bytes[0], challenge_mode, challenge_count, data(0x00314170, 0x10),
            data(0x00312A1C, 0x10), data(0x00312F8C, 0x10), constant(0x0032B110), constant(0x0032B248),
            constant(0x0032B244), constant(0x0032B210), constant(0x0032B254),
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
            intro_scale, story_byte, random, clock});
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
        for (const auto& [record, row] : mind_target_rows)
        {
            if (row->target != nullptr && &row->target->actor == &actor)
            {
                retired_targets[row->target->address] = retire_target(row->bytes, actor);
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
        read_u32(data(0x00338EE0, 4), 0), matrix_boundaries,
        [this](FactoryObject& object) -> RuntimePlayerAttachmentActor&
        {
            attaching_object = &object;
            attaching_bytes = object.bytes.get();
            return weapon_attachment_at(scene.find_address(object.bytes.get()));
        },
        attachment_actors,
        {}, 0, {}, 0, data(0x00433D68, 0x10), data(0x00433CA8, 0x10), resources.loaded_sizes, image.state_tables, image.attack_sequences,
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
            refresh_mind_targets();
            return address == 0 ? nullptr : target_records.at(address);
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
        brews_owned, brews_required, story_byte, intro_pending, intro_name, cases, case_body_bits,
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
        [](u32, std::span<const u32>)
        {
            unbound("imp fire effect messages");
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
        attachment_limit,
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
            // The words through +0xCDC that 0x000AB2D0 reads.
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
            return actors->actors.at(scene.resolve_address(address, 0).data());
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
    players->bindings.scene.audio_banks = &audio.streams;
    players->bindings.scene.blob_shadows = &blob_shadows;
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
            return actors->actors.at(scene.resolve_address(address, 0).data());
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
        emitter_players, resume, story_byte, intro_notices, emitters,
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
        data(0x0030C748, 0x34), read_f32(data(0x0032B17C, 4), 0), read_f32(data(0x0032B174, 4), 0),
        read_f32(data(0x0032B0F4, 4), 0), read_f32(data(0x0032B580, 4), 0), read_f32(data(0x0032B248, 4), 0),
        read_f32(data(0x0032B210, 4), 0), read_f32(data(0x0032B148, 4), 0)});
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
        display_view, occupancy_initial, default_euler, scoreboard_globals, data(0x00312F8C, 0x10),
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
        if (read_u32(player.body, 0x48) == 0)
        {
            return;
        }
        const u32 zero = 0;
        std::memcpy(player.body.data() + 0x10, &zero, 4);
        player.first_receiver = nullptr;
        player.body_flags->bytes[4] &= 0xFB;
        player.body[0x288] = 0;
        std::memcpy(player.body.data() + 0x48, &zero, 4);
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
        std::memcpy(player.body.data() + 0x4C, &weapon, 4);
        std::memcpy(player.body.data() + 0x14, &weapon, 4);
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
        empty_name});
    classes->install(scene.types);
    classes->defer_release = true;
    install_runtime_move_action_classes(scene.types, [this](u32 address) -> std::span<u8>
    {
        return scene.resolve_address(address, 0x33C); // Through +0x338.
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
        if (!player->mind.empty() && read_u32(player->mind, 0x14) != 0)
        {
            target = target_views.at(read_u32(player->mind, 0x14))->bytes;
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
            unbound("grabbed state whose body has no player owner");
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
        address_zero, reinterpret_cast<const u32&>(g_5462fc), g_script_30d0c, g_scene_flags->bytes[0],
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
    callout_names = strings(0x00421C9C, 0x55);
    flag_names = strings(0x00338FB0, 0x41);
    surface_names = strings(0x004340A0, 0x12);
    shock_pass = {image.string(0x003168A4), image.string(0x00316890), image.string(0x0031687C),
        image.string(0x00316860)};
    shock_fail = {image.string(0x00316854), image.string(0x00316840), image.string(0x0031682C),
        image.string(0x00316810)};
    std::memcpy(death_actions.data(), data(0x00217EB8, 6).data(), 6);
    // DAT_00433C9C holds 0x48 rows of 0xC, one per statsheet key.
    damage_tables = std::make_unique<RuntimeBodyDamageTables>(RuntimeBodyDamageTables{data(0x00433C9C, 0xC * 0x48),
        statsheet});
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
        door_shapes, data(0x00424994, 13 * 0x94)}); // Rows 0..12 (12: a room door 0x0005B540 adds); names follow.
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
    g_runtime_doors = doors.get();
    event_board_resources = std::make_unique<RuntimeScoreboardResources>(RuntimeScoreboardResources{
        resources.registry, resources.error, data(0x00314170, 0x10)});
    {
        const auto constant = [this](u32 address)
        {
            float value;
            std::memcpy(&value, data(address, 4).data(), 4);
            return value;
        };
        event_sequence = std::make_unique<ScriptEventSequenceContext>(ScriptEventSequenceContext{*doors,
            scoreboard_globals.score_board, *event_board_resources, audio.voices, prompt_owner,
            g_script_30d0c, data(0x0042890C, 0x14 * 0x13), constant(0x0032B24C),
            constant(0x0032B110), constant(0x0032B210),
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
                unbound("0x000D0200 dropping key candidate " + std::to_string(address)
                    + " (0x00112D10, message 0x80 and 0x0003ED70 on a scene actor)");
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
            }});
    }
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
    trophies = std::make_unique<DialogueTrophyContext>(DialogueTrophyContext{trophy_thresholds, dialogue_owned,
        trophy_flags, trophy_names.data(), g_retail_dialogue_token_names, resources.registry, resources.error});
    tags = std::make_unique<DialogueTagContext>(DialogueTagContext{tag_state, *trophies, sound_tags,
        g_retail_dialogue_tokens, g_retail_dialogue_sound_swaps, {}, 0, 0, story_byte, g_flags_ea9c});
    measurement = std::make_unique<DialogueMeasurementContext>(DialogueMeasurementContext{*tags, resources.metrics,
        resources.text, {g_runtime_profile.challenges.size, g_runtime_profile.challenges.bytes.data()}, 0, 0, 0, 0,
        0, 0, 0.f,
        read_f32(data(0x00424910, 4), 0), read_f32(data(0x0042493C, 4), 0), paragraph_disabled,
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
    dialogue = std::make_unique<RuntimeDialogueActivationContext>(RuntimeDialogueActivationContext{dialogue_state,
        g_flags_ea9c, active_strategies, frontend_pause, random, story_byte, hud_size[0], hud.inner_bottom,
        hud.text_width, read_u32(data(0x00421A1C, 4), 0), read_u32(data(0x00421A24, 4), 0),
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
    // The scratch allocation/binding sequence in 0x000F8740, with its actual
    // 0x00155D40 size and 0x20 heap alignment. Other boot services are owned separately.
    const u32 matrix_size = runtime_model_matrix_scratch_size();
    model_matrix_allocation.reset(static_cast<u8*>(engine_heap_allocate_aligned(i32(matrix_size), 0x20)));
    bind_runtime_model_matrix_scratch(reinterpret_cast<RuntimeShaderMatrix*>(model_matrix_allocation.get()),
        matrix_size, model_matrix_scratch);
    for (size_t index = 0; index < render_frame_storage.size(); ++index)
    {
        render_frames.emplace(&render_frame_storage[index], DialogueRenderFrame{{}, frame_cameras[index]});
    }
    dialogue_draw = std::make_unique<RuntimeDialogueDrawContext>(RuntimeDialogueDrawContext{*dialogue,
        dialogue_draw_state, resources.metrics, d3d_device.dialogue(), model_renderer, render_frames, deferred_states,
        light_override, fog.disabled, sampling_scratch, sampling_active, dialogue_projection, dialogue_view,
        dialogue_world, sprite_world,
        std::span<const u32, 4>(reinterpret_cast<const u32*>(&entity_detail::g_display_records[0].fitted_x), 4),
        render_height, render_width, hud_size[0], hud_size[1], read_u32(data(0x00314F48, 4), 0),
        read_u32(data(0x004273D4, 4), 0), read_u32(data(0x00425E6C, 4), 0), shader_constants_at(0x004273C8),
        {0, 0, 0}, read_u32(data(0x0042515C, 4), 0), {read_f32(data(0x0042514C, 8), 0), read_f32(data(0x0042514C, 8), 4)},
        {read_f32(data(0x00425154, 8), 0), read_f32(data(0x00425154, 8), 4)}, fallback_texture.data(), 0.f, 0,
        []() -> std::int64_t
        {
            std::int64_t counter = 0;
            QueryPerformanceCounter(&counter);
            return counter;
        }, read_f32(data(0x00424908, 4), 0), read_f32(data(0x0042490C, 4), 0)});
    display_setup = std::make_unique<RuntimeDisplaySetupContext>(RuntimeDisplaySetupContext{*dialogue_draw,
        clip_bounds, device_projection, device_view, device_world, projection_transpose, default_world, frontend_pause,
        display_address});
    sprites = std::make_unique<RuntimeDialogueSpriteQueue>(RuntimeDialogueSpriteQueue{sprite_records, sprite_count});
    scoreboard_draw = std::make_unique<RuntimeScoreboardDraw>(RuntimeScoreboardDraw{*sprites, g_4312cc,
        brew_accepted});
    scoreboard_hud->draw = scoreboard_draw.get();
    scoreboard_hud->counter_presets = {data(0x00314170, 0x14), data(0x00314184, 0x14)};
    scoreboard_hud->damage_preset = data(0x00312F8C, 0x14);
    dialogue_outer = std::make_unique<RuntimeDialogueOuterDrawContext>(RuntimeDialogueOuterDrawContext{*dialogue_draw,
        render_targets, d3d_device.outer(), *sprites, &resources.special_textures, *display_setup, text_draw_count, capture_enabled,
        std::bit_cast<u8>(u8(0)), g_flags_ea9c, display_address, read_u32(data(0x004271F4, 4), 0),
        read_u32(data(0x00425E0C, 4), 0), shader_constants_at(0x004271E8), line_retained});
    dialogue_outer->panel_line_width = read_u32(data(0x00425168, 4), 0);
    bind_scene_dialogue(game, *script_dialogue, *dialogue_outer);
    // The scene cutscene stack (DAT_004E1B40..DAT_004E1C54) that script node 0x13 starts (0x0004EDA0) and the
    // scene tick advances (0x0004EBD0), over this scene's cutscene context.
    install_scene_cutscene_runtime(game, SceneCutsceneBindings{*cutscenes, g_flags_ea9c, g_510cec, g_4dea24,
        g_50e7a0, g_sim_freeze, g_sim_dt, g_sim_scale});
    // The attract control (factory type 0x9B) over this scene's resources, text, input and cutscene stack.
    attract_services = std::make_unique<AttractControlServices>(AttractControlServices{resources.registry,
        resources.error, resources.text, text_registry, resources.metrics, widget_context.viewport_width,
        next_widget_id, random, hud.center_x, hud.bottom, text_fonts[0], g_510cec, input_activation, input_release,
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
        resources.registry, resources.error, scene_cutscene_runtime(game), voices, random, g_510cec, g_4dea24,
        g_flags_ea9c, g_scene_flags->bytes[0], g_runtime_profile.progress.bytes[4], g_runtime_profile.challenges.bytes,
        g_script_30d0c, pulse, trophy_head_rows,
        g_4312d0, g_4312d4, g_4312d8, prompt_owner, default_euler, g_sim_freeze, g_sim_dt, g_sim_scale,
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
        g_510cec, g_4dea24, g_flags_ea9c, pal, pal_override, g_4312d0, g_4312d4, g_4312d8, g_sim_freeze, g_sim_dt,
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
    // Cutscene event 0x13 calls the walk cutscene's +0x150 (0x00081630) with +0x154, the control's address.
    weapon_services->cutscenes().event_callbacks[0x00081630] = [this](RuntimeCutsceneInstance&, u32 owner, u32)
    {
        auto* object = object_of(scene.resolve_address(owner, 0).data());
        if (object == nullptr)
        {
            unbound("relics store pulse for an object outside the scene");
        }
        auto& control = scene.controls->instance(*object);
        pulse_runtime_relics_store_control(control.state, *control.relics_store, *relics_store_services); // 0x00081630
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
    // The Super Scary challenge (factory type 0x98) a trophy head's attack creates.
    super_scary_services = std::make_unique<SuperScaryControlServices>(SuperScaryControlServices{
        resources.registry, resources.error, scene_cutscene_runtime(game), voices, audio.streams, random, g_510cec,
        g_flags_ea9c, g_scene_flags->bytes[0], g_runtime_profile.progress.bytes, frontend_state, prompt_owner,
        super_scary_auto,
        override_health, data(0x00302198, 4 * 0x50), i32(read_u32(data(0x00427958, 4), 0)),
        read_u32(data(0x0042795C, 4), 0), read_u32(data(0x00314F48, 4), 0), read_u32(data(0x00314F30, 4), 0),
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
        [](u32)
        {
            throw Error("untranslated 0x0003CBB0 render toggle from the Super Scary challenge (+0x2BC)");
        },
        [](u32)
        {
            throw Error("untranslated 0x0003CAE0 render toggle from the Super Scary challenge (+0x2BC)");
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
        [](const Vec3&) -> float
        {
            throw Error("untranslated 0x00110BF0/0x00110EF0 floor height from the Super Scary challenge (mode 2/3)");
        },
        [this](u32 address)
        {
            auto& player = case_player(address);
            player.refresh_views();
            const auto* body_actor = actors->actors.at(scene.resolve_address(player.body_actor_address, 0).data());
            const Vec3 position = actor_world_position(*body_actor); // 0x000412A0, ESI body +8
            std::memcpy(player.body.data() + 0x18, &position, sizeof(position));
        },
        super_scary_text,
        []() -> bool
        {
            return scene_end_flag_set(); // 0x00049D10
        },
        [this]() -> i32
        {
            return scene_player_character(game); // 0x00087830
        }});
    scene.controls->context.super_scary = super_scary_services.get();
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
    // Boot 0x000620C0's glow targets over the book pages: book page 0's surface (DAT_005047E0) and, for the depth
    // surface (DAT_005047FC) and its texture alias (DAT_005047E8), book page 1, the scene's stand-in for the depth
    // memory DAT_005047E4 aliases as LIN_A8R8G8B8.
    const auto frame_record = [](std::span<u8> texture)
    {
        return texture.data() + read_u32(texture, 4) + u32(texture[0x12]) * 0x14;
    };
    glow_targets.page_texture = frame_record(book_pages[0]);
    glow_targets.depth_texture = frame_record(book_pages[1]);
    glow_targets.page_surface = d3d_device.outer().surface(
        DialogueSurfaceHandle(resources.memory.address_of(glow_targets.page_texture)), 0);
    glow_targets.depth_surface = d3d_device.outer().surface(
        DialogueSurfaceHandle(resources.memory.address_of(glow_targets.depth_texture)), 0);
    glow_pass = std::make_unique<RuntimeGlowPass>(RuntimeGlowPass{*dialogue_outer, glow_targets, g_50484c,
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
            // The first background's +0x950 row's +4: its point rows (stride 0x30), the first one's position.
            auto* first = backgrounds->first_background();
            if (first == nullptr || part >= first->state.graphs.size() || first->state.graphs[part].points.size() < 12)
            {
                unbound("point-light part centre without the background's point graph");
            }
            return *reinterpret_cast<const Vec3*>(first->state.graphs[part].points.data());
        },
        [this](const u8* record)
        {
            return resources.memory.address_of(record);
        }});
    bind_gameselect_services();
    bind_transition_services();
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
    light_construction = std::make_unique<SceneLightConstructionContext>(SceneLightConstructionContext{*environment,
        scene_color, retained_scene_color, g_509b4c, read_f32(data(0x0042A580, 4), 0), directional_light, shaders.records,
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
        }, {}});
    model_shader_combiner = std::make_unique<RuntimeModelShaderCombiner>(RuntimeModelShaderCombiner{
        *model_shader_draw, std::span<u32>(deferred_states).subspan((0x00181B70 - 0x00181968) / 4),
        deferred_states[0], deferred_states[(0x00181C44 - 0x00181968) / 4], shader_texture_stage,
        [this](u32 setter, u32 value)
        {
            d3d_device.set_state(setter == 0x001722A0 ? SceneD3DDevice::kZEnable : setter, value);
        }});
    bind_runtime_model_shader_callback([this](u32 shader)
    {
        post_runtime_model_shader(shader, *model_shader_combiner);
    }, *model_shader_draw);
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
            d3d_device.set_state(SceneD3DDevice::kVertexShader, handle);
        }});
    node_callbacks[0x00158510] = [this](u32 node, void*)
    {
        // Pointer-width view of the existing fog words at render state +0x198/+0x19C.
        vertex_parameters[0] = model_renderer.state.fog_reciprocal;
        vertex_parameters[1] = model_renderer.state.fog_scaled_end;
        dispatch_runtime_model_vertex_shader(node, *vertex_draw);
    };
    // DAT_00336130 starts with its .data value; 0x000F8740 binds DAT_004D9754 to 0x000F9BC0 and
    // DAT_004D9750 to 0x000F9B90.
    std::memcpy(material_globals.material_vector.data(), data(0x00336130, 0xC).data(), 0xC);
    material_bind = std::make_unique<RuntimeModelMaterialBind>(RuntimeModelMaterialBind{*tree_dispatch,
        material_globals, texture_records, deferred_states, deferred_states[(0x00181C60 - 0x00181968) / 4],
        deferred_states[(0x00181C58 - 0x00181968) / 4], deferred_states[(0x00181C64 - 0x00181968) / 4],
        environment_primary, environment_secondary,
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
        [this](u32)
        {
            apply_runtime_model_material_colors(*model_shader_combiner);
        },
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
        stream_descriptors, vertex_auxiliary});
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
    std::memcpy(rig_tunables.rig.data(), data(0x004274B8, 0x88).data(), 0x88);
    motion_steps = i32(read_u32(data(0x004312E0, 4), 0));
    {
        const auto words = data(0x00428708, 0x2C);
        director_data = DirectorData{read_u32(words, 0), i32(read_u32(words, 4)), i32(read_u32(words, 8)),
            read_u32(words, 0xC), read_f32(words, 0x10), read_f32(words, 0x14), read_u32(words, 0x18),
            read_u32(words, 0x1C), i32(read_u32(words, 0x20)), read_u32(words, 0x24), read_u32(words, 0x28),
            rig_tunables.real(0x0043400C)};
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
                if (read_u32(player->actor_bytes, 0x960) == address)
                {
                    player->refresh_input_views();
                    return &player->input_device;
                }
            }
            unbound("input device view for actor +0x960 word " + std::to_string(address));
        },
        input_state});
    camera_rig = std::make_unique<RuntimeCameraRigContext>(RuntimeCameraRigContext{rig_tunables, *director_tunables,
        clock, unlock_flags, view_invert, render_width, render_height, director_targets->actor_at,
        director_targets->input_device, input_state});
    body_services = std::make_unique<SceneBodyServices>(SceneBodyServices{game, scene.factory, image.pools,
        scene.objects, sounds, effects, latches, entity_detail::g_world.body_a, global_attachments, origin, default_euler, random,
        address_zero, clock, statsheet.data(), object_tags.data(), *damage_tables, *footsteps, surface_names,
        object_tags, image.string(0x0031C354),
        {image.string(0x00315A98), image.string(0x00315AE0), image.string(0x00315AFC), image.string(0x00315AF4)},
        std::span<const char* const, 0x55>(callout_names.data(), 0x55),
        std::span<const char* const, 0x41>(flag_names.data(), 0x41), shock_pass, shock_fail, data(0x00217DE8, 0xD0),
        death_actions, pause_references, decay_enabled, decay_index, player_damage, g_flags_ea9c, g_flag_ea98,
        reinterpret_cast<u32&>(g_script_30d0c), goal_scratch, empty_name, occupancy_initial,
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        [this](u32 address) -> Actor*
        {
            return actors->actors.at(scene.resolve_address(address, 0).data());
        },
        [this](u32 address) -> std::span<const u8>
        {
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
            return players->player_at(address).self;
        },
        [this](u32 address) -> std::span<const u8>
        {
            return players->player_at(address).body;
        },
        rig_tunables, entity_detail::g_look_profile, motion_steps, std::span<u32>(node_cache),
        {data(0x00338F64, 0x24), data(0x00338F88, 0x24), data(0x00338EE0, 0x84), data(0x00338EA4, 0x3C)},
        read_f32(data(0x0032B22C, 4), 0),
        [this]()
        {
            std::array<float, 8> phases;
            std::memcpy(phases.data(), data(0x00339900, 0x20).data(), 0x20);
            return phases;
        }(), read_f32(data(0x0032B4F8, 4), 0), read_f32(data(0x0032B3D8, 4), 0),
        read_f32(data(0x0032B140, 4), 0), body_shape_push,
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
        begin_scene_transition(reinterpret_cast<const char*>(g_510e58 + 0x80)); // 0x0012C5D0
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
    body_services->contact_words = [this](NativePlayerInstance& player, std::span<u32> words, GhoulySpawnPool* contacts,
        size_t contacts_word)
    {
        dispatch_contact_words(player, words, contacts, contacts_word);
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
            return player.body.empty() ? 0u : read_u32(player.body, 0x48); // Actor +0x95C -> +0x48.
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
            return actors->actors.at(scene.resolve_address(address, 0).data());
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
            refresh_mind_targets();
            return *target_records.at(address);
        },
        [](const RuntimeBodyAttachedActor&) -> std::span<const u8>
        {
            unbound("spawn attachment actor +0x8F8 link view");
        },
        [this]() -> u32
        {
            const auto* next = entity_detail::g_world.alt.next;
            return next == nullptr ? 0 : address_of(next);
        },
        targets, target_views, target_eligibility, target_addresses, target_queries, embedded_targets, query,
        view_invert,
        data(0x002FFF24, 0x10), read_f32(data(0x004275B8, 4), 0), attachment_notice,
        statsheet, data(0x00433D5C, 0xC), powerup_rows[11].active, player_damage,
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
    active_services = this;
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
            actors.push_back({read_u16(mark.bytes, 6), goal, read_u32(mark.bytes, 0x984), read_u32(mark.bytes, 0x98C)});
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
                read_u32(bytes, 0x93C), Vec3{read_f32(owner->body, 0x24), read_f32(owner->body, 0x28),
                    read_f32(owner->body, 0x2C)}});
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
                {0xAB, 4}, {0x82, 1}, {0x83, 2}, {0x84, 1}, {0x86, 5}, {0x8E, 2}, {0x8F, 2}};
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
        return RuntimeScriptKillCounters{&measurement->second_count, &brews_owned, &measurement->first_additional};
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
        if (player.body.empty() || read_u16(player.body, 0x94) == 0)
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
    // Markers (0x0011CB80 loads them into DAT_0051108C).
    scene.load_markers(scene_name.data());
    markers = scene.markers().data();
    script_records = markers;
    scene.released = released_binding;
    in_scene = true;
}

// The scene globals a scene starts from: what 0x0011C5A0, 0x0011C480 and 0x00116DA0 clear, and the per-scene
// storage the services hold for the scene's actors, backgrounds, doors and lights.
void SceneActorServices::State::reset_scene_data()
{
    effect_collision = {};
    effect_callback_result = 0;
    effect_sound_frame = 0;
    frame_effect_scratch = {};
    scare_flags = 0;
    player_link = 0;
    player_damage = 0;
    collision_push_contacts = {};
    body_slide_contacts = {};
    view_invert = 0;
    story_bits = {};
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
    scale_exclusions = {};
    animation_classes = {};
    mind_settings = {0, {}};
    camera_players = 0;
    strategy_ranges = {0, {}};
    std::fill(pause_references.begin(), pause_references.end(), 0u);
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
    weapon_attachments.clear();
    override_words = {};
    decay_index = 0;
    targets = nullptr;
    attachment_notice = 0;
    goal_scratch.clear();
    brews_taken = 0;
    brew_accepted = 0;
    prompt_lock = -1;
    difficulty = 0;
    brews_owned = 0;
    brews_required = 0;
    story_byte = 0;
    unlock_flags = 0;
    intro_pending = 0;
    intro_name = {};
    std::fill(queue_limits.begin(), queue_limits.end(), 0u);
    std::fill(class_queue_limits.begin(), class_queue_limits.end(), 0u);
    queue_limits[0] = 3; // 0x00116DA0
    std::fill(class_queue_limits.begin(), class_queue_limits.end(), 99u);
    notice_seen = {};
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
    override_health = 0;
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
    post_model_count = 0;
    decal_count = 0;
    caption_camera = 0;
    caption_widget = 0;
    frontend_state = 0;
    frontend_widget = 0;
    hud_enabled = 0;
    screen_flash = 0;
    screen_capture = 0;
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
    // 0x0011C5A0's 0x00123640 (0x001199F0, which also clears DAT_0050EC24..DAT_0050EF18, is not translated) and
    // 0x001133B0.
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
    release_runtime_scene_light_grid(light_grid, light_grid_d3d); // 0x001133B0
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

extern i32 g_429bc0; // DAT_00429BC0, scene.cpp.
extern u16 g_429bc4[0x80]; // DAT_00429BC4, scene.cpp.

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

void SceneActorServices::State::bind_book_services()
{
    // Boot 0x000620C0's two book page textures: DAT_005047D8, a back-buffer texture (LIN_X8R8G8B8), and
    // DAT_005047E4, retail's alias of the depth buffer's memory as LIN_A8R8G8B8. The host has no native boot
    // arena or shared depth memory, so each native scene makes both in its resource memory.
    const RuntimeScreenTextureBoundaries textures = screen_texture_boundaries();
    book_pages[0] = create_runtime_screen_texture(640, 480, 1, 0x1E, 0, textures); // 0x000533A0
    book_pages[1] = create_runtime_screen_texture(640, 480, 1, 0x12, 0, textures);
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
        dialogue_outer->capture_configuration, 0, read_u32(data(0x004231C4, 4), 0), // DAT_005462A4 is never written.
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
        resources.error, g_runtime_profile, g_flags_ea9c, gameselect_text, image.string(read_u32(data(0x00430CF8, 4),
        0)), scene_order_held, scene_order_release, g_510cec, input_activation, input_release, frontend_input_address,
        game.clock.frame, g_429bc0, gameselect_stream_wait, read_f32(data(0x0032B184, 4), 0),
        [this](i32 id) -> i32
        {
            return runtime_stream_bank_busy(id, nullptr, nullptr, audio.wavebank_streams); // 0x00039D50
        },
        audio.streams, g_pause_ditty, textures, *display_setup, g_sim_freeze, g_sim_dt, g_sim_scale, image,
        g_frontend, game.native_movies->session->startup(), hud_size, hud, reinterpret_cast<char16_t*>(g_429bc4),
        reinterpret_cast<i32*>(g_510e58 + 0x124), g_54632c, text_registry, resources.metrics,
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
        g_50e458, *cutscenes, *attract_services,
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
        port_masks.data() + 1, g_510ce8,
        [this](Actor& actor)
        {
            actor_updates.at(&actor)(actor); // Actor vtable +0x10.
        },
        replacement_source.data(), replacement_target.data()});
    scene.controls->context.gameselect = gameselect_services.get();
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
    // The scene-replay prompts' rows (0x003095D0, "ReplayChapterMenu"): A and the stick are 0x000E4E40 and
    // 0x000D8080 above.
    widget_callbacks[0x000E30E0] = [this]()
    {
        leave_runtime_game_scene_replay(*gameselect_services);
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
        g_spawn_trigger_players = nullptr;
        g_marked_actor_message = nullptr;
        g_actor_broadcast = nullptr;
        g_alt_node_body = nullptr;
        g_alt_node_class = nullptr;
        g_script_actor_bytes = nullptr;
        g_script_actor_message = nullptr;
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

// Retail address of a native pointer: a scene object, file-backed XBE memory, or a relocated resource.
u32 SceneActorServices::State::heap_address(const void* pointer, size_t size)
{
    auto [found, inserted] = heap_addresses.try_emplace(pointer, next_heap_address);
    if (inserted)
    {
        next_heap_address += u32((size + 15) & ~size_t(15));
    }
    // The block's bytes for memory_at; a later registration with a larger size widens the view (its address
    // range stays the first registration's).
    auto& block = heap_blocks[found->second];
    if (size > block.size())
    {
        block = {static_cast<const u8*>(pointer), size};
    }
    return found->second;
}

RuntimeStrategyMessageActor* SceneActorServices::State::strategy_message_actor(u32 address)
{
    if (address == 0)
    {
        unbound("strategy message actor at address zero");
    }
    auto* object = object_of(scene.resolve_address(address, 0).data());
    if (object == nullptr || !actors->actors.contains(object->bytes.get()))
    {
        unbound("strategy message actor that is no scene actor");
    }
    auto& view = actors->view(*object);
    refresh_mind_targets();
    // Actor +0x8F8: the first record on the actor's +0x8F4 live attachment list, or 0.
    RuntimeMindTarget* target = nullptr;
    if (auto* link = view.live_attachments.next; link != nullptr)
    {
        auto* record = reinterpret_cast<ActorLiveAttachment*>(link);
        if (const auto found = mind_target_rows.find(record); found != mind_target_rows.end())
        {
            target = found->second->target.get();
        }
    }
    auto& entry = strategy_message_actors[address];
    if (entry == nullptr)
    {
        entry = std::make_unique<RuntimeStrategyMessageActor>();
    }
    else if (strategy_message_building.contains(address))
    {
        return entry.get(); // A holder chain back to an actor being built: its view is the one in progress.
    }
    strategy_message_building.insert(address);
    *entry = RuntimeStrategyMessageActor{address, &view.bat.actor, {object->bytes.get(), object->size}, target,
        nullptr};
    // Body +0x274 is a player's; the other classes' +0x95C word is no body (0x00138B40 asks only about players).
    if (object->type == 5)
    {
        const u32 holder = read_u32(players->player(*object).body, 0x274);
        RuntimeStrategyMessageActor* own = entry.get();
        own->holder = holder == 0 ? nullptr : strategy_message_actor(holder);
    }
    strategy_message_building.erase(address);
    return strategy_message_actors.at(address).get();
}

void SceneActorServices::State::refresh_mind_targets()
{
    std::unordered_set<const ActorLiveAttachment*> live;
    target_views.clear();
    target_records.clear();
    RuntimeMindTarget* previous = nullptr;
    targets = nullptr;
    for (auto* link = static_cast<entity_detail::ListHead*>(global_attachments.next); link != nullptr;
         link = static_cast<entity_detail::ListHead*>(link->next))
    {
        auto* record = reinterpret_cast<ActorLiveAttachment*>(reinterpret_cast<u8*>(link)
            - offsetof(ActorLiveAttachment, global_link));
        live.insert(record);
        auto& row = mind_target_rows[record];
        if (row == nullptr)
        {
            row = std::make_unique<SceneMindTargetRow>();
        }
        Actor& actor = *record->actor;
        const u32 address = heap_address(record, 0x68);
        const u32 actor_address = address_of(&actor);
        std::memcpy(row->bytes.data() + 0x10, &actor_address, 4);
        std::memcpy(row->bytes.data() + 0x14, &record->mode, 0x54);
        std::span<const u8> actor_bytes;
        const OccupancyRecord* occupancy = nullptr;
        FactoryObject& object = object_of_actor(actor);
        actor_bytes = {object.bytes.get(), object.size};
        if (object.type == 5)
        {
            occupancy = players->player(object).current; // Actor +0xCA8.
        }
        RuntimeScriptMindQuery* query_view = &absent_target_query;
        if (const auto found = target_queries.find(&actor); found != target_queries.end())
        {
            query_view = found->second;
        }
        // A live record keeps its target object, so strategy and mind contexts holding it across a nested refresh
        // stay valid; only the reference members force a new one.
        if (row->target == nullptr || &row->target->actor != &actor || &row->target->body != query_view)
        {
            row->target = std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, row->bytes, address, actor,
                actor_bytes, *query_view, occupancy, record});
        }
        else
        {
            row->target->next = nullptr;
            row->target->bytes = row->bytes;
            row->target->address = address;
            row->target->actor_bytes = actor_bytes;
            row->target->occupancy = occupancy;
            row->target->record = record;
        }
        row->view = RuntimePlayerTargetView{row->bytes, address, &actor, row->target.get()};
        if (previous == nullptr)
        {
            targets = row->target.get();
        }
        else
        {
            previous->next = row->target.get();
        }
        previous = row->target.get();
        target_views[address] = &row->view;
        target_records[address] = row->target.get();
    }
    // Player minds' own +0xB8 records are not on DAT_00545988 but are addressed like its records.
    for (const auto& [address, embedded] : embedded_targets)
    {
        target_views[address] = embedded.first;
        target_records[address] = embedded.second;
    }
    std::erase_if(mind_target_rows, [this, &live](const auto& row)
    {
        if (live.contains(row.first))
        {
            return false;
        }
        // A record unlinked while its actor lives on keeps its last bytes too (a released actor's are kept by
        // the release hook).
        if (row.second->target != nullptr && !retired_targets.contains(row.second->target->address))
        {
            Actor* actor = &row.second->target->actor;
            const bool alive = std::any_of(actors->actors.begin(), actors->actors.end(), [actor](const auto& entry)
            {
                return entry.second == actor;
            });
            if (alive)
            {
                retired_targets[row.second->target->address] = retire_target(row.second->bytes, *actor);
            }
        }
        return true;
    });
    std::erase_if(retired_targets, [this](const auto& retired)
    {
        return target_views.contains(retired.first);
    });
}

u32 SceneActorServices::State::address_of(const void* pointer)
{
    if (pointer == nullptr)
    {
        return 0;
    }
    if (const u32 address = scene.find_address(pointer))
    {
        return address;
    }
    if (const u32 address = image.address_of(pointer))
    {
        return address;
    }
    // A scene actor's native Actor is the retail actor object.
    for (const auto& [bytes, actor] : actors->actors)
    {
        if (pointer == actor)
        {
            return scene.address_of(bytes);
        }
    }
    // World list heads (DAT_00511B34..DAT_00511B4C) and each player's list nodes (actor +0x934 + 8n).
    auto& world = entity_detail::g_world;
    const std::pair<const void*, u32> heads[]{{&world.body_a, 0x00511B1C}, {&world.body_b, 0x00511B24},
        {&world.body_c, 0x00511B2C}, {&world.mark, 0x00511B34}, {&world.case_head, 0x00511B3C},
        {&world.alt, 0x00511B44}, {&world.slot, 0x00511B4C}, {&world.child, 0x00511B54}};
    for (const auto& [head, address] : heads)
    {
        if (pointer == head)
        {
            return address;
        }
    }
    for (auto* player : players->instances())
    {
        // 0x00136700 assigns this real heap block before storing its address in mind +0x1C.
        if (player->mind_storage != nullptr && pointer == player->mind_storage)
        {
            return heap_address(pointer, sizeof(RuntimeMindStorage));
        }
        for (u32 index = 0; index < 4; ++index)
        {
            if (pointer == &player->links[index])
            {
                return player->actor_address + 0x934 + index * 8;
            }
        }
        // Actor +0x960, the 0x00107830 input device.
        if (!player->input.empty() && pointer == player->input.data())
        {
            const u32 address = heap_address(pointer, player->input.size());
            input_addresses[address] = player->input;
            return address;
        }
    }
    if (const auto found = heap_addresses.find(pointer); found != heap_addresses.end())
    {
        return found->second;
    }
    // Occupancy records (actor +0x98C, +0xCA0, +0xCA4) live in the record block of the DAT_00512344 pool.
    if (g_512344 != nullptr)
    {
        const auto& pool = *static_cast<const OccupancyPool*>(g_512344);
        const auto* begin = reinterpret_cast<const u8*>(pool.begin);
        const auto* limit = reinterpret_cast<const u8*>(pool.limit);
        const auto* bytes = static_cast<const u8*>(pointer);
        if (begin != nullptr && bytes >= begin && bytes < limit)
        {
            return heap_address(begin, size_t(limit - begin)) + u32(bytes - begin);
        }
    }
    // Model anchors are heap blocks from 0x0011C0B0 on the DAT_00510654 list.
    for (auto* node = static_cast<entity_detail::ListHead*>(g_actor_model_anchors.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        if (pointer == reinterpret_cast<u8*>(node) - offsetof(ActorModelAnchor, global_link))
        {
            return heap_address(pointer, sizeof(ActorModelAnchor));
        }
    }
    try
    {
        return resources.memory.address_of(static_cast<const u8*>(pointer));
    }
    catch (const std::exception&)
    {
        unbound("retail address of a native pointer outside scene objects, XBE data and resources");
    }
}

// Xbox 0x0003DE30, ECX actor, ESI object: the object on the actor's +0x1D8 list is told 0x14 and released.
bool SceneActorServices::State::release_actor_frame_object(Actor& actor, u32 object)
{
    auto& view = actors->view(object_of_actor(actor));
    for (auto* update = view.frame.updates; update != nullptr; update = update->next)
    {
        FactoryObject* owned = object_of(frame_objects.at(update));
        if (owned == nullptr)
        {
            unbound("0x0003DE30 frame object outside the scene");
        }
        if (address_of(owned->bytes.get()) != object)
        {
            continue;
        }
        // Message 0x14 names the released object by its retail address (the owner's body reads it, 0x00027B20).
        std::array<u32, 0x42> notice{0x14, object};
        actors->bindings.message(actor, notice.data());
        release_factory_object(scene.factory, *owned); // 0x001075C0
        return true;
    }
    return false;
}

FactoryObject* SceneActorServices::State::object_of(const u8* bytes)
{
    // A moved-from object keeps null bytes; an unresolved address must not match it.
    if (bytes == nullptr)
    {
        return nullptr;
    }
    for (auto* owned : {&scene.objects, &cameras, &callout_objects, &cutscene_actors, &dialogue_bats})
    {
        for (auto& object : *owned)
        {
            if (object.bytes.get() == bytes)
            {
                return &object;
            }
        }
    }
    for (auto* player : players->instances())
    {
        for (FactoryObject* object : {&player->created_attachment, &player->body_object,
            &player->mind_object, &player->strategy_object, &player->first_action,
            &player->second_action, &player->body_action_object})
        {
            if (object->bytes.get() == bytes)
            {
                return object;
            }
        }
    }
    // Only while its creation runs: the comparison never reads the pending object itself.
    if (attaching_bytes == bytes)
    {
        return attaching_object;
    }
    return nullptr;
}

// The body constructors read the model's block directory and the instance channel word (+0x1CC).
NativePlayerModelViews SceneActorServices::State::model_views_of(const Actor& actor)
{
    if (actor.model == nullptr)
    {
        return NativePlayerModelViews{nullptr, nullptr};
    }
    auto& views = model_views[&actor];
    views = std::make_unique<SceneActorModelViews>();
    const auto cpu = actor.model->cpu;
    const u32 table = read_u32(cpu, 0);
    const u32 count = read_u32(cpu, 4);
    for (u32 index = 0; index < count; ++index)
    {
        const u32 offset = read_u32(cpu, table + index * 8 + 4);
        views->rows.push_back({read_u32(cpu, table + index * 8), offset == 0 ? nullptr : cpu.data() + offset});
    }
    views->nodes.nodes = views->rows;
    const u32 channel = actor.model->runtime != nullptr && actor.model->runtime->auxiliary_targets[0] != nullptr;
    std::memcpy(views->channels.data() + 0x1CC, &channel, 4);
    return NativePlayerModelViews{&views->nodes, views->channels.data()};
}

SceneActorBase& SceneActorServices::State::base_of(FactoryObject& object)
{
    auto& slot = bases[object.bytes.get()];
    if (slot == nullptr)
    {
        auto& view = actors->view(object);
        const u32 address = scene.address_of(object.bytes.get());
        slot = std::make_unique<SceneActorBase>(
            ActorTrailContext{scene.factory, scene.objects, address, pause, frame_delta, time_scale, pal,
                pal_override, empty_name},
            view.startup, effects, scares, voices, object,
            ActorAttachmentEventContext{view.bat.actor, view.startup.attachments, effects, scene.factory,
                image.pools, scene.objects, address, linked_records, {}},
            view, world_bind, node_cache, reset_matrix);
    }
    return *slot;
}

// Actor +0x204's model instance poses (+0x164, +0x168), as the shape code reads them.
void SceneActorServices::State::refresh_shape_poses(FactoryObject& object)
{
    if (!actors->actors.contains(object.bytes.get()))
    {
        return;
    }
    auto& actor = actors->view(object).bat.actor;
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return;
    }
    auto& base = base_of(object);
    const auto& runtime = *actor.model->runtime;
    base.primary_pose.buffer = runtime.primary;
    base.alternate_pose.buffer = runtime.alternate;
    actor.model->primary_shape_pose = runtime.primary != nullptr ? &base.primary_pose : nullptr;
    actor.model->alternate_shape_pose = runtime.alternate != nullptr ? &base.alternate_pose : nullptr;
}

void SceneActorServices::State::object_message(FactoryObject& object, std::span<u32> message)
{
    refresh_shape_poses(object);
    // The pose routines read the actor's model instance through its native animation view.
    const u8* bytes = object.bytes.get();
    if (actors->actors.contains(bytes))
    {
        refresh_actor_model_animation_view(actors->view(object));
    }
    route_object_message(object, message);
    if (auto* live = object_of(bytes); live != nullptr && actors->actors.contains(bytes))
    {
        store_actor_model_animation_view(actors->view(*live));
    }
}

void SceneActorServices::State::route_object_message(FactoryObject& object, std::span<u32> message)
{
    const u16 type = object.type;
    if (type == 0x31)
    {
        message_fx_custom_outline({object.bytes.get(), object.size}, message); // 0x00077A80
        return;
    }
    if (type == 0x30)
    {
        message_fx_custom_fade_to_grey({object.bytes.get(), object.size}, message); // 0x00076DF0
        return;
    }
    if (type == 0x32)
    {
        message_fx_custom_solid_fade_in({object.bytes.get(), object.size}, message); // 0x000780C0
        return;
    }
    if (type == 0x19)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_knockdown_sphere(*object_of(bytes));
        };
        const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actors->actors.at(scene.resolve_address(address, 0).data());
        };
        auto bindings = word_messages;
        if (message[0] == 0x7C)
        {
            bindings.reference_at = [actor_at](u32 address) -> void*
            {
                return actor_at(address);
            };
        }
        handle_runtime_knockdown_sphere_message({object.bytes.get(), object.size}, view.prop_contacts,
            message, actor_at, base_of(object).base, bindings); // 0x0006EBD0
        return;
    }
    if (type == 0x1B)
    {
        // Getter 0x000704D0: +0x10 is 0x000705B0 (the actor update 0x000420D0 first) and +0xC is 0x000706E0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_spider_web(*object_of(bytes));
        };
        refresh_actor_model_animation_view(view);
        RuntimeSpiderWeb web{{object.bytes.get(), object.size}, view.bat.actor, view.animation, resources.registry,
            resources.error, random};
        RuntimeSpiderWebMessage handlers{
            [this, &object](std::span<u32> words)
            {
                dispatch_base_actor_word_message(base_of(object).base, words, word_messages); // 0x0003EF70
            },
            view.render, view.shape, word_messages.memory_at};
        dispatch_runtime_spider_web_message(web, message, handlers); // 0x000706E0
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x1A)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_contact_prop(*object_of(bytes));
        };
        const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actors->actors.at(scene.resolve_address(address, 0).data());
        };
        auto bindings = word_messages;
        if (message[0] == 0x7C)
        {
            bindings.reference_at = [actor_at](u32 address) -> void*
            {
                return actor_at(address);
            };
        }
        handle_runtime_contact_prop_message({object.bytes.get(), object.size}, view.prop_contacts,
            message, actor_at, base_of(object).base, bindings);
        return;
    }
    if (type == 0xAA || type == 0xA9 || type == 0xA7 || type == 0xA0 || type == 0xA1 || type == 0xA2
        || type == 0xA3)
    {
        // Getters 0x000CEF70, 0x000CD640 and 0x000CB570: +0x10 is the weapon update 0x0014A2E0; +0xC is the chair message
        // 0x000CF0D0 or the large projectile's 0x000CDBE0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_weapon(updated); // 0x0014A2E0
            store_actor_model_animation_view(updated_view);
        };
        refresh_shape_poses(object);
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x1D)
    {
        // Getter 0x0006EFB0: +0x10 is 0x0006F1C0, which ends with the actor update 0x000420D0 and the bound size
        // 0x0003D670 (mask 0x200).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor& actor)
        {
            auto& updated = *object_of(bytes);
            refresh_actor_model_animation_view(actors->view(updated));
            classes->update_scare_figure(updated);
            store_actor_model_animation_view(actors->view(updated));
            update_base_actor(updated); // 0x000420D0
            measure_runtime_actor_bound_size(actor, 0x200,
                reinterpret_cast<GhoulySpawnPool*&>(actors->view(updated).allocation_8fc)); // 0x0003D670
        };
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x21)
    {
        // Getter 0x00071820: +0x10 is 0x00071960, which ends each phase with the actor update 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_sss_dropper(updated, [this, &updated_view]()
                {
                    update_actor_frame(updated_view.frame, *frame); // 0x000420D0
                });
            store_actor_model_animation_view(updated_view);
        };
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x22)
    {
        // Getter 0x00072270: +0x10 is 0x00072480, which runs the actor update 0x000420D0 between the ditty and the
        // watch for the player.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            classes->update_super_scary_shock(updated, [this, &updated]()
                {
                    update_base_actor(updated); // 0x000420D0
                });
        };
        classes->message(object, message);
        return;
    }
    if (type == 0x26)
    {
        // Getter 0x0006BB30: +0x10 is 0x0006C0A0 itself, with no actor update 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            classes->update_swap_setup(*object_of(bytes));
        };
        classes->message(object, message);
        return;
    }
    if (type == 0x25 || type == 0x24 || type == 0x1E)
    {
        // Getter 0x0006B110: +0x10 is 0x0006B8B0; getter 0x0006A4F0: +0x10 is 0x0006ADC0; getter 0x0006CC70: +0x10
        // is 0x0006CFD0. Each runs the actor update 0x000420D0 first.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes, type](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            if (type == 0x25)
            {
                classes->update_cutscene_setup(updated);
            }
            else if (type == 0x24)
            {
                classes->update_bg_break_setup(updated);
            }
            else
            {
                classes->update_bg_break_key(updated);
            }
        };
        classes->message(object, message);
        return;
    }
    if (type == 0xD || type == 0x1F || type == 0x23)
    {
        // Vtable +0x10: the emitter's 0x0010A2E0 (getter 0x00109DD0), the trigger's 0x00069D20 (getter
        // 0x00069620) and, for areas (getter 0x0006DBD0 keeps the base slot), 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes, type](Actor&)
        {
            auto& updated = *object_of(bytes);
            if (type == 0x1F)
            {
                update_base_actor(updated);
                return;
            }
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            if (type == 0xD)
            {
                classes->update_emitter(updated);
            }
            else
            {
                classes->update_trigger(updated);
            }
            store_actor_model_animation_view(updated_view);
        };
        classes->message(object, message);
        return;
    }
    if (type == 4)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            backgrounds->update(*object_of(bytes));
        };
        backgrounds->message(object, message);
        return;
    }
    if (type == 5)
    {
        auto& player = players->player(object);
        actor_updates[&player.actor] = [this, player = &player](Actor&)
        {
            update_player(*player);
        };
        if (!player.actor_message)
        {
            attach_player_messages(object, player);
        }
        if (message[0] == 0x7C || message[0] == 0x8B)
        {
            auto& graph = *player_messages.at(object.bytes.get());
            ActorReferenceMessage native{message[0], message[1] == 0 ? nullptr :
                word_messages.reference_at(message[1])};
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = input_selected;
            return;
        }
        if (message[0] == 0xB)
        {
            // Retail words {0xB, slot, mix, name}: the player's handler and the base actor read the native message.
            auto& graph = *player_messages.at(object.bytes.get());
            ActorAnimationMixMessage native = decode_actor_animation_mix_message(message, word_messages);
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = input_selected;
            return;
        }
        if (message[0] == 0xE || message[0] == 0xF)
        {
            auto& graph = *player_messages.at(object.bytes.get());
            const u32 sender_address = message[1];
            RuntimeMessageReceiver sender{sender_address,
                [this, sender_address](std::span<const u32> words)
                {
                    auto* sender_object = object_of(scene.resolve_address(sender_address, 0).data());
                    object_message(*sender_object, {const_cast<u32*>(words.data()), words.size()});
                }};
            RuntimePlayerContactMessage native{message[0], &sender, {}, u16(message[8])};
            std::copy_n(message.data() + 2, 6, native.words.data());
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = input_selected;
            return;
        }
        player.self.message(message);
        return;
    }
    if (type == 3 || type == 0x91 || type == 0x92 || type == 0x93 || type == 0x97 || type == 0x98 || type == 0x9A)
    {
        scene.controls->message(object, message);
        return;
    }
    if (type == 0xF)
    {
        // Getter 0x00066900: +0x10 is 0x00066FE0 and +0xC is 0x00070370.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_motion_blur(updated);
            store_actor_model_animation_view(updated_view);
        };
        // Xbox 0x00070370
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (message[0] == 0x23)
        {
            unregister_actor_segment_probes(view.render);
            clear_actor_shape_probe(view.bat.actor, view.shape);
        }
        return;
    }
    if (type == 0x13)
    {
        // Getter 0x0006C260: +0x10 is 0x0006C5A0 (the actor update 0x000420D0 first) and +0xC is 0x0006C9B0, a jump
        // to the actor's 0x0003EF70.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            classes->update_animated_light(updated);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        return;
    }
    if (type == 0x12)
    {
        // Getter 0x00067E60: +0x10 is 0x00068060 and +0xC is 0x00068700 (the base 0x0003EF70 first).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_bone_debris(updated);
            store_actor_model_animation_view(updated_view);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        classes->message_bone_debris(object, message);
        return;
    }
    if (type == 0x10)
    {
        // Getter 0x000690A0: +0x10 is 0x00069250 and +0xC is 0x00069540 (the base 0x0003EF70 first).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_afterimage(updated);
            store_actor_model_animation_view(updated_view);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        classes->message_afterimage(object, message[0]);
        return;
    }
    if (type == 2 || type == 0x15)
    {
        if (type == 2)
        {
            // Getter 0x0003C080 keeps the base update 0x000420D0 at +0x10.
            auto& view = actors->view(object);
            const u8* bytes = object.bytes.get();
            actor_updates[&view.bat.actor] = [this, bytes](Actor&)
            {
                update_base_actor(*object_of(bytes));
            };
        }
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (type == 0x15 && message[0] == 0x11)
        {
            unregister_actor_render_bins(actors->view(object).render); // 0x0006D8A0's tail: 0x0003CBB0
        }
        return;
    }
    throw Error("scene object message for factory type " + std::to_string(type) + " has no class view");
}

void SceneActorServices::State::actor_message(Actor& actor, const void* message)
{
    for (const auto& [bytes, candidate] : actors->actors)
    {
        if (candidate == &actor)
        {
            auto* object = object_of(bytes);
            if (object == nullptr)
            {
                unbound("message to an actor whose object is still being created");
            }
            object_message(*object, {static_cast<u32*>(const_cast<void*>(message)), 0x42});
            return;
        }
    }
    throw Error("actor message has no scene actor");
}

RuntimeSegmentContactActor& SceneActorServices::State::prop_contact_actor(entity_detail::SegmentProbe& probe)
{
    auto& view = actors->view(probe);
    auto& actor = view.bat.actor;
    auto& slot = prop_contact_views[&actor];
    if (slot == nullptr)
    {
        prop_contact_owners[view.bytes.data()] = &actor;
        slot = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{actor, 0, &view.body_node,
            [this, &actor, bytes = view.bytes.data()](void* payload)
            {
                // The receiver reads a contact pool as its retail image: the header's three addresses, then the
                // records, bound to a scene address for the call.
                std::vector<u8> image;
                const auto bind_pool = [this, &image](const GhoulySpawnPool* pool) -> u32
                {
                    if (pool == nullptr)
                    {
                        return 0;
                    }
                    const size_t used = size_t(pool->cursor - pool->begin);
                    const size_t reserved = size_t(pool->limit - pool->begin);
                    image.resize(0x10 + reserved);
                    std::memcpy(image.data(), &pool->stride, 2);
                    std::memcpy(image.data() + 2, &pool->grow, 2);
                    std::memcpy(image.data() + 0x10, pool->begin, used);
                    const u32 address = scene.bind_address(image);
                    const std::array<u32, 3> addresses{address + 0x10, address + 0x10 + u32(used),
                        address + 0x10 + u32(reserved)};
                    std::memcpy(image.data() + 4, addresses.data(), sizeof(addresses));
                    return address;
                };
                const auto send = [this, bytes, &image](std::span<u32> words, u32 records)
                {
                    try
                    {
                        object_message(*object_of(bytes), words);
                    }
                    catch (...)
                    {
                        if (records != 0)
                        {
                            scene.unbind_address(image.data());
                        }
                        throw;
                    }
                    if (records != 0)
                    {
                        scene.unbind_address(image.data());
                    }
                };
                const u32 id = *static_cast<const u32*>(payload);
                if (id == 0x17)
                {
                    // {0x17, reply, sender, contacts, point} from a knock-down sphere (0x0006EC10).
                    auto& notice = *static_cast<RuntimeKnockdownSphereNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = records;
                    words[4] = std::bit_cast<u32>(notice.point.x);
                    words[5] = std::bit_cast<u32>(notice.point.y);
                    words[6] = std::bit_cast<u32>(notice.point.z);
                    send(words, records);
                    notice.reply = words[1];
                    return;
                }
                if (id == 8)
                {
                    // {8, reply, contacts, player, actor} from the knockdown sweep (0x0013FC50).
                    auto& sweep = *static_cast<RuntimeSweepNotice*>(payload);
                    const u32 records = bind_pool(sweep.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = sweep.id;
                    words[1] = sweep.reply;
                    words[2] = records;
                    words[3] = sweep.player;
                    words[4] = sweep.actor;
                    send(words, records);
                    sweep.reply = words[1];
                    return;
                }
                if (id == 0x15)
                {
                    // {0x15, reply, sender, velocity, contacts, point} from a player's contact (0x0001F8C0).
                    auto& notice = *static_cast<RuntimeContactNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = std::bit_cast<u32>(notice.velocity.x);
                    words[4] = std::bit_cast<u32>(notice.velocity.y);
                    words[5] = std::bit_cast<u32>(notice.velocity.z);
                    words[6] = records;
                    words[7] = std::bit_cast<u32>(notice.point.x);
                    words[8] = std::bit_cast<u32>(notice.point.y);
                    words[9] = std::bit_cast<u32>(notice.point.z);
                    send(words, records);
                    notice.reply = words[1];
                    return;
                }
                if (id == 0x19)
                {
                    // {0x19, kind, strength, style, reply, 0, 0, weapon, velocity, contacts, point, 0} from the
                    // large projectile's contact test (0x00141A30).
                    auto& notice = *static_cast<RuntimeProjectileHitNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.kind;
                    words[2] = notice.strength;
                    words[3] = notice.style;
                    words[4] = notice.reply;
                    words[5] = notice.unused_14;
                    words[6] = notice.unused_18;
                    words[7] = address_of(notice.weapon);
                    std::memcpy(words.data() + 8, &notice.velocity, 12);
                    words[0xB] = records;
                    std::memcpy(words.data() + 0xC, &notice.point, 12);
                    words[0xF] = notice.tail;
                    send(words, records);
                    notice.reply = words[4];
                    notice.unused_14 = words[5];
                    notice.unused_18 = words[6];
                    return;
                }
                if (id == 0x1D || id == 0x18)
                {
                    // {0x1D, kind, strength, style, reply, weapon, velocity, contacts, point} from a weapon's
                    // contact test (0x00141890), or 0x18 in the same layout from a spray (0x00141C00), as the player
                    // route lays it out.
                    auto& notice = *static_cast<RuntimeWeaponHitNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.kind;
                    words[2] = notice.strength;
                    words[3] = notice.style;
                    words[4] = notice.reply;
                    words[5] = address_of(notice.weapon);
                    std::memcpy(words.data() + 6, &notice.velocity, 12);
                    words[9] = records;
                    std::memcpy(words.data() + 0xA, &notice.point, 12);
                    send(words, records);
                    notice.reply = words[4];
                    return;
                }
                if (id != 0x1B)
                {
                    unbound("prop contact notice " + std::to_string(id));
                }
                auto& notice = *static_cast<RuntimePropContactNotice*>(payload);
                const u32 contacts = scene.bind_address({reinterpret_cast<u8*>(notice.contacts), sizeof(GhoulySpawnPool)});
                std::array<u32, 8> words{notice.id, notice.reply, address_of(notice.sender), notice.value, contacts,
                    std::bit_cast<u32>(notice.point.x), std::bit_cast<u32>(notice.point.y), std::bit_cast<u32>(notice.point.z)};
                try
                {
                    object_message(*object_of(bytes), words);
                }
                catch (...)
                {
                    scene.unbind_address(notice.contacts);
                    throw;
                }
                notice.reply = words[1];
                scene.unbind_address(notice.contacts);
            }});
    }
    slot->child_link = view.render.child.prev != nullptr ? 1u : 0u;
    return *slot;
}

// A player's actor message with a contact pool word: the pool is bound to a scene address while it runs.
void SceneActorServices::State::dispatch_contact_words(NativePlayerInstance& player, std::span<u32> words,
    GhoulySpawnPool* contacts, size_t contacts_word)
{
    const bool bound = contacts != nullptr && scene.find_address(contacts) == 0;
    if (contacts != nullptr)
    {
        words[contacts_word] = bound ? scene.bind_address({reinterpret_cast<u8*>(contacts), sizeof(GhoulySpawnPool)})
            : scene.find_address(contacts);
    }
    try
    {
        player.actor_message(words);
    }
    catch (...)
    {
        if (bound)
        {
            scene.unbind_address(contacts);
        }
        throw;
    }
    if (bound)
    {
        scene.unbind_address(contacts);
    }
}

// The contact walk's view of the actor whose segment probe it hit: a player (actor +0x934 node, message 0x15
// from 0x0001F8C0) or a prop.
RuntimeSegmentContactActor& SceneActorServices::State::segment_contact_actor(entity_detail::SegmentProbe& probe)
{
    for (auto* player : players->instances())
    {
        if (&player->actor_instance.segment != &probe)
        {
            continue;
        }
        auto& slot = player_contact_views[player];
        if (slot == nullptr)
        {
            slot = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{player->actor, 0,
                &player->links[0],
                [this, player](void* payload)
                {
                    if (*static_cast<const u32*>(payload) == 8)
                    {
                        // {8, reply, contacts, player, actor} from the knockdown sweep (0x0013FC50).
                        auto& sweep = *static_cast<RuntimeSweepNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = sweep.id;
                        words[1] = sweep.reply;
                        words[3] = sweep.player;
                        words[4] = sweep.actor;
                        dispatch_contact_words(*player, words, sweep.contacts, 2);
                        sweep.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x17)
                    {
                        // {0x17, reply, sender, contacts, point} from a knock-down sphere (0x0006EC10).
                        auto& notice = *static_cast<RuntimeKnockdownSphereNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.reply;
                        words[2] = address_of(notice.sender);
                        words[4] = std::bit_cast<u32>(notice.point.x);
                        words[5] = std::bit_cast<u32>(notice.point.y);
                        words[6] = std::bit_cast<u32>(notice.point.z);
                        dispatch_contact_words(*player, words, notice.contacts, 3);
                        notice.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x1B)
                    {
                        // {0x1B, reply, sender, value, contacts, point} from a contact prop's shapes (0x0006EB00).
                        auto& notice = *static_cast<RuntimePropContactNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.reply;
                        words[2] = address_of(notice.sender);
                        words[3] = notice.value;
                        words[5] = std::bit_cast<u32>(notice.point.x);
                        words[6] = std::bit_cast<u32>(notice.point.y);
                        words[7] = std::bit_cast<u32>(notice.point.z);
                        dispatch_contact_words(*player, words, notice.contacts, 4);
                        notice.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x19)
                    {
                        // {0x19, kind, strength, style, reply, 0, 0, weapon, velocity, contacts, point, 0} from the
                        // large projectile's contact test (0x00141A30).
                        auto& notice = *static_cast<RuntimeProjectileHitNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.kind;
                        words[2] = notice.strength;
                        words[3] = notice.style;
                        words[4] = notice.reply;
                        words[5] = notice.unused_14;
                        words[6] = notice.unused_18;
                        words[7] = address_of(notice.weapon);
                        std::memcpy(words.data() + 8, &notice.velocity, 12);
                        std::memcpy(words.data() + 0xC, &notice.point, 12);
                        words[0xF] = notice.tail;
                        dispatch_contact_words(*player, words, notice.contacts, 0xB);
                        notice.reply = words[4];
                        notice.unused_14 = words[5];
                        notice.unused_18 = words[6];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x1D || *static_cast<const u32*>(payload) == 0x18)
                    {
                        // {0x1D, kind, strength, style, reply, weapon, velocity, contacts, point} from a weapon's
                        // contact test (0x00141890), or 0x18 in the same layout from a spray (0x00141C00).
                        auto& notice = *static_cast<RuntimeWeaponHitNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.kind;
                        words[2] = notice.strength;
                        words[3] = notice.style;
                        words[4] = notice.reply;
                        words[5] = address_of(notice.weapon);
                        std::memcpy(words.data() + 6, &notice.velocity, 12);
                        std::memcpy(words.data() + 0xA, &notice.point, 12);
                        dispatch_contact_words(*player, words, notice.contacts, 9);
                        notice.reply = words[4];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) != 0x15)
                    {
                        unbound("player contact payload " + std::to_string(*static_cast<const u32*>(payload)));
                    }
                    auto& notice = *static_cast<RuntimeContactNotice*>(payload);
                    // {0x15, reply, sender, velocity, contacts, point}.
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = std::bit_cast<u32>(notice.velocity.x);
                    words[4] = std::bit_cast<u32>(notice.velocity.y);
                    words[5] = std::bit_cast<u32>(notice.velocity.z);
                    words[7] = std::bit_cast<u32>(notice.point.x);
                    words[8] = std::bit_cast<u32>(notice.point.y);
                    words[9] = std::bit_cast<u32>(notice.point.z);
                    dispatch_contact_words(*player, words, notice.contacts, 6);
                    notice.reply = words[1];
                }});
        }
        slot->child_link = player->actor_instance.render.child.prev != nullptr ? 1u : 0u;
        return *slot;
    }
    return prop_contact_actor(probe);
}

// Base actor vtable +0x10 (0x000420D0) over the scene's frame context.
void SceneActorServices::State::update_base_actor(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    update_actor_frame(view.frame, *frame);
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::update_knockdown_sphere(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor =
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return segment_contact_actor(probe);
        };
    RuntimeKnockdownSphereContacts contacts{contact_actor, entity_detail::g_world.body_a,
        [this]()
        {
            return marked_players();
        }};
    update_runtime_knockdown_sphere({object.bytes.get(), object.size}, view.prop_contacts, view.frame, *frame, clock,
        contacts);
    const u32 pool = view.prop_contacts != nullptr ? heap_address(view.prop_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x934, &pool, 4);
    store_actor_model_animation_view(view);
}

std::vector<RuntimeSegmentContactActor*> SceneActorServices::State::marked_players()
{
    std::vector<RuntimeSegmentContactActor*> marked;
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
        marked.push_back(&segment_contact_actor(owner->actor_instance.segment));
    }
    return marked;
}

void SceneActorServices::State::update_spider_web(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    RuntimeSpiderWeb web{{object.bytes.get(), object.size}, view.bat.actor, view.animation, resources.registry,
        resources.error, random};
    RuntimeSpiderWebUpdate update{clock,
        [this, &view]()
        {
            update_actor_frame(view.frame, *frame); // 0x000420D0
        },
        [this]()
        {
            return marked_players();
        }};
    update_runtime_spider_web(web, update); // 0x000705B0
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::update_contact_prop(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor =
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return prop_contact_actor(probe);
        };
    RuntimeActorShapeContacts contacts{contact_actor, entity_detail::g_world.body_a};
    update_runtime_contact_prop({object.bytes.get(), object.size}, view.prop_contacts,
        view.frame, *frame, clock, contacts);
    const u32 pool = view.prop_contacts != nullptr ? heap_address(view.prop_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x934, &pool, 4);
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::player_controller_message(GhoulyController& controller, void* data)
{
    for (auto* player : players->instances())
    {
        if (&player->controller != &controller)
        {
            continue;
        }
        // As the setups': the scheduler's 0xA7 / 0xA9 / 0xAA requests are shorter than a full message.
        auto* words = static_cast<u32*>(data);
        size_t count = 0x42;
        if (words[0] == 0xA7)
        {
            count = sizeof(GhoulySpawnMessage) / 4;
        }
        else if (words[0] == 0xA9 || words[0] == 0xAA)
        {
            count = sizeof(GhoulyVariantMessage) / 4;
        }
        object_message(object_of_actor(player->actor), {words, count});
        return;
    }
    unbound("ghouly spawn controller that is no player's");
}

void SceneActorServices::State::attach_player_messages(FactoryObject& object, NativePlayerInstance& player)
{
    player.controller.message = [](GhoulyController& controller, void* data)
    {
        active_services->player_controller_message(controller, data);
    };
    auto& slot = player_messages[object.bytes.get()];
    auto& instance = player.actor_instance;
    slot = std::make_unique<ScenePlayerMessages>(
        RuntimePlayerActivation{player.self, player.actor, player.links, entity_detail::g_world,
            [this](const void* pointer)
            {
                return address_of(pointer);
            },
            excluded_count, excluded_tags.data(), player.room, player.allowed, player.ring, address_zero, overrides},
        RuntimePlayerPause{player.actor, player.strategy_object, player.strategy_setup, query, player.goal_cleanup,
            resume},
        RuntimePlayerNavigationRegistration{player.body, player.actor, player.navigation}, player,
        base_of(object).base,
        RuntimePlayerChildMessages{player.strategy_object, player.mind_object, player.body_object,
            [this](FactoryObject& child, void* message)
            {
                try
                {
                    child_message(child, message);
                }
                catch (const std::exception& error)
                {
                    throw Error("player child type " + std::to_string(child.type) + " message "
                        + std::to_string(*static_cast<const u32*>(message)) + ": " + error.what());
                }
            }});
    auto* messages = slot.get();
    messages->context.event_at = [this](u32 address) -> std::span<const u8>
    {
        return scene.resolve_tail(address);
    };
    // 0x0003B3C0: an animation event becomes message {7, position, slot, event, phase} to actor vtable +0xC.
    instance.animation.message_handler = [this, &player](const ActorAnimationMessage& message)
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
            player.actor_message(words);
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
    player.actor_message = [this, messages, &player](std::span<const u32> words)
    {
        // Actor +0x50 is read when the message runs; the controller can be created after attachment.
        messages->context.animation = player.actor_instance.animation.controller.get();
        dispatch_runtime_player_message(const_cast<u32*>(words.data()), messages->context, messages->children);
        input_state.devices = input_activation.entries;
        input_state.selected = input_selected;
    };
}

SceneBodyMessages& SceneActorServices::State::body_graph(NativePlayerInstance& player)
{
    player.refresh_views();
    auto& graph = body_messages[player.body.data()];
    if (graph == nullptr)
    {
        graph = std::make_unique<SceneBodyMessages>(*body_services, player);
    }
    return *graph;
}

// Child vtable +0xC of a player's strategy, mind or body.
void SceneActorServices::State::child_message(FactoryObject& child, void* message)
{
    const std::span<const u32> words{static_cast<const u32*>(message), 0x42};
    // A strategy's goals read the body's vtable slots (body_slot_key), which a just-spawned player's body graph
    // has not attached yet.
    if (child.type == 0x4F || child.type == 0x46 || child.type == 0x3B || child.type == 0x51 || child.type == 0x52
        || child.type == 0x58 || child.type == 0x44 || child.type == 0x40 || child.type == 0x4C || child.type == 0x53
        || child.type == 0x42 || child.type == 0x47)
    {
        if (auto* player = players->find_player(child); player != nullptr && !player->body.empty())
        {
            body_graph(*player);
        }
    }
    switch (child.type)
    {
    case 0x4F:
        players->strategy_message(child, words);
        return;
    case 0x46:
        players->imp_fire_message(child, words);
        return;
    case 0x3B:
        players->butler_message(child, words);
        return;
    case 0x51:
        players->skeleton_message(child, words);
        return;
    case 0x58:
        players->zombie_message(child, words);
        return;
    case 0x52:
        players->spider_message(child, words);
        return;
    case 0x44:
        players->haunted_tv_message(child, words);
        return;
    case 0x40:
        players->haunted_chair_message(child, words);
        return;
    case 0x3F:
        players->grim_reaper_message(child, words);
        return;
    case 0x4C:
        players->mummy_message(child, words);
        return;
    case 0x53:
        players->ugly_girl_message(child, words);
        return;
    case 0x42:
        players->haunted_door_message(child, words);
        return;
    case 0x47:
        players->imp_flamethrower_message(child, words);
        return;
    case 0x39:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        RuntimeDirectorMessages director{child, player->actor, player->mind, player->strategy_setup,
            player->strategy_messages,
            [this](Vec3& position, Vec3& euler)
            {
                return read_runtime_marker_camera_start(position, euler, camera_start);
            },
            [this, player]()
            {
                update_player(*player);
            }};
        dispatch_runtime_director_message(words, director);
        return;
    }
    case 0x34:
    case 0x35:
    {
        auto& slots = players->minds.view(child);
        auto* player = players->find_player(child);
        player->refresh_views();
        auto& graph = mind_messages[child.bytes.get()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneMindMessages>(*mind_services, *player, slots);
        }
        graph->refresh();
        players->minds.message(child, message);
        return;
    }
    case 0x36:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        const u32 target = read_u32(player->mind, 0x10);
        if (words[0] == 0x7D && navigation_nodes == nullptr)
        {
            unbound("navigation node buffer DAT_0050E24C for 0x00136700");
        }
        RuntimeCameraMindMessages messages{player->mind_events, player->mind,
            target == 0 ? std::span<const u8>() : std::span<const u8>(scene.resolve_address(target, 0x24)),
            i32(read_u32(player->actor_bytes, 0xCEC)), g_nav_grids, navigation_nodes, player->mind_storage,
            [this](const void* pointer)
            {
                return address_of(pointer);
            }};
        dispatch_runtime_camera_mind_message(words, messages);
        return;
    }
    case 0x27:
    case 0x28:
    case 0x2A:
    case 0x2D:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        auto& graph = body_messages[child.bytes.get()];
        if (graph == nullptr)
        {
                graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        try
        {
            graph->dispatch(child, {static_cast<u32*>(message), 0x42});
        }
        catch (const std::exception& error)
        {
            throw Error("body type " + std::to_string(child.type) + " message "
                + std::to_string(words[0]) + ": " + error.what());
        }
        return;
    }
    default:
        throw Error("scene actor service is not bound: child message slot of factory type "
            + std::to_string(child.type));
    }
}

// The host latch mirrors an actor's +0x900 node; 0x0003EBA0 takes it off DAT_00511AA4 before the rest of the
// release, so later broadcasts during that release skip the actor.
void SceneActorServices::State::unlink_active_node(const u8* bytes)
{
    const auto found = active_nodes.find(scene.address_of(bytes));
    if (found == active_nodes.end())
    {
        return;
    }
    auto** link = &latches;
    while (*link != nullptr && *link != found->second.get())
    {
        link = &(*link)->next;
    }
    if (*link != nullptr)
    {
        *link = (*link)->next;
    }
    found->second->next = nullptr;
}

// Active-list adapter: the native Message carries the actor argument the retail words hold at +4.
void SceneActorServices::State::latch_message(entity_detail::LatchNode& node, const Message& message)
{
    for (const auto& [address, latch] : active_nodes)
    {
        if (latch.get() != &node)
        {
            continue;
        }
        u32 words[0x42]{};
        words[0] = u32(message.id);
        if (message.arg != nullptr)
        {
            const auto* argument = static_cast<const Actor*>(message.arg);
            bool found = false;
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (actor == argument)
                {
                    words[1] = scene.address_of(bytes);
                    found = true;
                }
            }
            if (!found)
            {
                throw Error("active-list message argument is not a scene actor");
            }
        }
        else
        {
            words[1] = u32(message.arg_i);
        }
        auto* object = object_of(scene.resolve_address(address, 0).data());
        object_message(*object, words);
        return;
    }
    throw Error("active-list node has no scene actor");
}

// Scene startup after the marker pass: 0x00123070 activates every pending actor.
void SceneActorServices::State::activate_actors()
{
    auto activation = activation_context();
    activate_runtime_scene_actors(activation);
}

RuntimeSceneActivation SceneActorServices::State::activation_context()
{
    return RuntimeSceneActivation{live_actors, latches, scene_nodes, scene_controls,
        [this](RuntimeActorLink& node)
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (&actors->view(*object_of(bytes)).startup.live == &node)
                {
                    return scene.address_of(bytes);
                }
            }
            throw Error("pending actor node has no scene actor");
        },
        [this](RuntimeActorLink& node) -> entity_detail::LatchNode&
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (&actors->view(*object_of(bytes)).startup.live == &node)
                {
                    const u32 address = scene.address_of(bytes);
                    auto& latch = active_nodes[address];
                    latch = std::make_unique<entity_detail::LatchNode>();
                    latch->tag = read_u16({bytes, 8}, 6);
                    latch->handle_message = [](entity_detail::LatchNode& self, const Message& message)
                    {
                        active_services->latch_message(self, message);
                    };
                    return *latch;
                }
            }
            throw Error("pending actor node has no scene actor");
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
                throw Error("activation actor " + std::to_string(address) + " type "
                    + std::to_string(object->type) + " message " + std::to_string(words[0])
                    + ": " + error.what());
            }
        },
        [this](u32 address) -> const Actor*
        {
            return actors->actors.at(scene.resolve_address(address, 0).data());
        }};
}

RuntimeDirectorActor SceneActorServices::State::director_actor(NativePlayerInstance& player)
{
    player.refresh_views();
    return RuntimeDirectorActor{player.actor_address, &player.actor, player.actor_bytes, player.strategy, player.mind,
        player.body, player.body_bits};
}

RuntimeBodyAttachedActor& SceneActorServices::State::attached_actor_at(u32 address)
{
    if (auto* player = players->find_player_at(address); player != nullptr)
    {
        return player->self;
    }
    const auto bytes = scene.resolve_address(address, 0);
    FactoryObject* object = object_of(bytes.data());
    if (object == nullptr || (object->type != 0x0B && object->type != 0xAA && object->type != 0xA9
        && object->type != 0xA7 && object->type != 0xA0 && object->type != 0xA1 && object->type != 0xA2
        && object->type != 0xA3))
    {
        unbound("attached actor at an address that is neither a player nor a weapon");
    }
    auto& view = weapon_attachments[address];
    if (view == nullptr)
    {
        view = std::make_unique<RuntimeBodyAttachedActor>();
    }
    view->bytes = {object->bytes.get(), object->size};
    view->address = address;
    view->message = [this, address](std::span<const u32> message)
    {
        std::array<u32, 0x42> words{};
        std::copy_n(message.begin(), std::min(message.size(), words.size()), words.begin());
        object_message(*object_of(scene.resolve_address(address, 0).data()), words);
    };
    view->actor = actors->actors.at(object->bytes.get());
    // Actor +0xD8C (Actor::spawn_owner): the holding player's own view, when the weapon is held.
    view->host = nullptr;
    for (auto* player : players->instances())
    {
        if (&player->actor == view->actor->spawn_owner)
        {
            view->host = &player->self;
        }
    }
    return *view;
}

RuntimePlayerAttachmentActor& SceneActorServices::State::weapon_attachment_at(u32 address)
{
    auto& object = attached_actor_at(address);
    if (players->find_player_at(address) != nullptr)
    {
        unbound("weapon pick-up views of a player");
    }
    auto& view = weapon_pickups[address];
    if (view == nullptr)
    {
        view = std::make_unique<WeaponPickupView>();
    }
    view->receiver.message = [this, address](const RuntimeBodyAnimationNotice& notice)
    {
        // Message 0x28 {id, actor, record, 0, position}. The holder's record goes as a bound word for the
        // weapon's animation table lookup (0x00144B80) and is released after the message.
        std::array<u32, 0x42> words{};
        words[0] = notice.id;
        words[1] = notice.actor;
        words[3] = notice.zero;
        std::memcpy(&words[4], &notice.position, 4);
        auto* record = const_cast<AnimationRecord*>(notice.record);
        const bool bind = record != nullptr && scene.find_address(record) == 0;
        if (record != nullptr)
        {
            words[2] = bind ? scene.bind_address({reinterpret_cast<u8*>(record), sizeof(AnimationRecord)})
                            : scene.find_address(record);
        }
        try
        {
            object_message(*object_of(scene.resolve_address(address, 0).data()), words);
        }
        catch (...)
        {
            if (bind)
            {
                scene.unbind_address(record);
            }
            throw;
        }
        if (bind)
        {
            scene.unbind_address(record);
        }
    };
    auto& instance = actors->view(*object_of(scene.resolve_address(address, 0).data()));
    view->pose.emplace(RuntimePlayerAttachmentActor{object, instance.bat.actor, view->receiver, instance.animation,
        instance.pose, instance.startup.bone, nullptr, node_cache, reset_matrix});
    attachment_actors[&object] = &*view->pose;
    return *view->pose;
}

std::vector<RuntimeDirectorActor> SceneActorServices::State::marked_actors()
{
    std::vector<RuntimeDirectorActor> marks;
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
        marks.push_back(director_actor(*owner));
    }
    return marks;
}

RuntimeActorPauseGroups& SceneActorServices::State::refresh_pause_groups()
{
    if (pause_walking)
    {
        // A walk is delivering 0x26/0x27: a nested 0x00117400 walks the same nodes.
        return pause_groups;
    }
    pause_nodes.clear();
    for (const auto& mark : marked_actors())
    {
        const u32 address = mark.address;
        pause_nodes.push_back(RuntimePauseActor{nullptr, mark.bytes, [this, address](u32 id)
        {
            std::array<u32, 0x42> words{id};
            object_message(*object_of(scene.resolve_address(address, 0).data()), words);
        }});
    }
    for (size_t index = 0; index + 1 < pause_nodes.size(); ++index)
    {
        pause_nodes[index].next = &pause_nodes[index + 1];
    }
    first_pause = pause_nodes.empty() ? nullptr : &pause_nodes.front();
    return pause_groups;
}

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
        [](const RuntimePlayerMindRequest& request) -> i32
        {
            unbound("camera mind request kind " + std::to_string(request.kind)
                + " through the parent mind requests 0x000295E0");
        }};
    const std::span<u8> mind_view = player.mind;
    entity_detail::NavigationPathSearch* path_search = nullptr;
    if (!mind_view.empty() && read_u32(mind_view, 0x1C) != 0)
    {
        // 0x00136700 allocates the search and stores its heap address at mind +0x1C.
        if (player.mind_storage == nullptr
            || heap_address(player.mind_storage, sizeof(RuntimeMindStorage)) != read_u32(mind_view, 0x1C))
        {
            unbound("mind +0x1C path search that 0x00136700 did not allocate");
        }
        path_search = player.mind_storage;
    }
    const u8* floor_hint = nullptr;
    if (!mind_view.empty() && read_u32(mind_view, 0x10) != 0)
    {
        // Mind +0x10 is a DAT_00545988 target record; the host views are rebuilt when a record is new.
        const u32 record = read_u32(mind_view, 0x10);
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
    if (read_u32(player.body, 0x710) != 0)
    {
        unbound("body +0x710 collision sphere view for 0x00025F00");
    }
    RuntimeBodyCollisionSphere* spheres = nullptr;
    const EntityMaskHost mask{player.actor.mask_flag, player.actor.mask_span, player.actor.mask_mode};
    RuntimeBodyCollision collision_view{player.body, player.actor, player.body_bits, spheres, mask, clock};
    RuntimePlayerRoomTracking room{player.self, player.body, player.allowed, player.current,
        player.strategy_object.bytes != nullptr};
    const RuntimeWorldBounds bounds{g_amb_volume_enabled, g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2],
        g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]};
    auto& instance = player.actor_instance;
    RuntimePlayerUpdate update{player.self, player.input, read_u32(player.actor_bytes, 0x960), input_activation,
        input_release, game.clock.frame, player.ring, address_zero, player.strategy_object, player.mind_object,
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
            case 0x3F:
                players->grim_reaper_update(strategy);
                return;
            case 0x4C:
                players->mummy_update(strategy);
                return;
            case 0x53:
                players->ugly_girl_update(strategy);
                return;
            case 0x42:
                players->haunted_door_update(strategy);
                return;
            case 0x47:
                players->imp_flamethrower_update(strategy);
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
        [&body_graph](FactoryObject& body, u32 input)
        {
            body_graph->tick(body, input);
        },
        [&body_graph](FactoryObject& body)
        {
            body_graph->animate(body);
        },
        [&body_graph](FactoryObject& body)
        {
            body_graph->settle(body);
        },
        [&body_graph](FactoryObject& body)
        {
            return body_graph->supported(body);
        },
        [&player](FactoryObject& action, u32) -> u16
        {
            switch (action.type)
            {
            case 0x8F:
            case 0x6E: // Getter 0x00091AB0 keeps 0x00091F70 at +0x1C too.
                return runtime_walk_target_next_key({action.bytes.get(), action.size},
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
            case 0x7F: // And getter 0x0009F060.
            case 0x7E: // And getter 0x0009EB40.
            case 0x6B: // And getter 0x00090430.
            case 0x77: // And getter 0x00097B20.
            case 0x6A: // And getter 0x0008F6F0.
            case 0x6D: // And getter 0x00090D30.
            case 0x81: // And getter 0x0009F6E0.
            case 0x76: // And getter 0x00097200.
            case 0x7B: // And getter 0x0009D8C0.
            case 0x83: // And getter 0x000A0C30.
            case 0x67: // And getter 0x0008E010.
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
        path_search, floor_hint, collision_view, player.actor_bytes, player.body_bits, player.collision,
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
        {
            return segment_contact_actor(probe);
        },
        entity_detail::g_world.body_a, instance.animation, instance.pose, instance.startup.attachments,
        instance.pending, instance.frame.updates, instance.shape, *frame, clock, player.navigation, room, bounds};
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
    RuntimeActorMatrixSampling sampling{{background.matrices.allocation,
        size_t(background.matrices.matrices.count) * 0x48}, background.matrices.matrices.node,
        std::span<float, 12>(sampling_scratch.data(), 12),
        sampling_active, origin, views.source};
    BackgroundPartDispatch dispatch{part_settings, part_setting_count, part_sound_slot,
        std::span<const u8, 5>(data(0x002FD200, 5).data(), 5), sounds, effects};
    // DAT_00511AF4: the first segment probe, stored at its actor +0x1E0.
    u32 first_segment = 0;
    if (entity_detail::g_world.segment != nullptr)
    {
        first_segment = scene.address_of(actors->view(*entity_detail::g_world.segment).bytes.data()) + 0x1E0;
    }
    BackgroundPartRemoval removal{background.state.volumes, background.animation.header, first_segment, views.source,
        resources.shape_lists, view.startup.resource.resource.data(), base.callouts};
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
        return actors->actors.at(scene.resolve_address(address, 0).data());
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
    BackgroundMessageContext context{base.base, base_message, parts, address_zero, *score, resources.text, part_text,
        wide_scratch.data(), broken_parts, reinterpret_cast<const char*>(data(0x00421E58, 0x2D).data()),
        entity_detail::g_world.anchor, *doors, actor_at, views.resolve, reply};
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
        RuntimeEffectDebris debris{{debris_pool->particles, 0x16A8}, debris_pool->types,
            debris_pool->free_head, game.clock.frame, random};
        const Vec3 plane{read_f32(data(0x00433FFC, 12), 0), read_f32(data(0x00433FFC, 12), 4),
            read_f32(data(0x00433FFC, 12), 8)};
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
    RuntimeEffectDebris debris{{debris_pool->particles, 0x16A8}, debris_pool->types,
        debris_pool->free_head, game.clock.frame, random};
    const Vec3 plane{read_f32(data(0x00433FFC, 12), 0), read_f32(data(0x00433FFC, 12), 4),
        read_f32(data(0x00433FFC, 12), 8)};
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
        mind.state = i16(read_u16(body, 0x30));
        mind.state_time = read_f32(body, 0x34);
        const u32 case_actor = read_u32(body, 0x48);
        mind.case_actor = case_actor == 0 ? nullptr : actors->actors.at(scene.resolve_address(case_actor, 0).data());
        mind.node = nullptr;
        mind.blocked = body[0x69];
        mind.active = body[0x6A];
        mind.suspended = body[0x6B];
        mind.permitted = body[0x6C];
        mind.timer = read_f32(body, 0x70);
        mind.energy = i16(read_u16(body, 0x94));
        mind.maximum_energy = i16(read_u16(body, 0x96));
        mind.bits = owner->body_bits;
        view->tag = read_u16(owner->actor_bytes, 6);
        void* marker = read_u32(owner->actor_bytes, 0xCE0) != 0 ? static_cast<void*>(owner->actor_bytes.data() + 0xCE0)
            : nullptr;
        views.push_back(RuntimeScriptActorQueryView{owner->actor, view->tag, &view->mind, nullptr, marker});
        case_actor_views.push_back(std::move(view));
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
    if (found == actor_updates.end() && object->type == 2)
    {
        // Vtable +0x10 is the class's slot (getter 0x0003C080 keeps 0x000420D0), so a base actor updates even
        // before its first message, as a star bound under its owner does (0x0008CEF0).
        const u8* object_bytes = object->bytes.get();
        found = actor_updates.emplace(&view.bat.actor, [this, object_bytes](Actor&)
        {
            update_base_actor(*object_of(object_bytes));
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
    // 0x0011CBD0 runs 0x001153A0, 0x00123560, 0x00119990 (not translated), then 0x0011F450.
    build_runtime_background_cloths(background_cloth_scene()); // 0x001153A0
    reset_runtime_scene_point_lights(background != nullptr && background->model != nullptr ? background->model->cpu
        : std::span<const u8>{}, point_lights); // 0x00123560
    setup_runtime_scene_mirrors(background != nullptr && background->model != nullptr ? background->model->cpu
        : std::span<const u8>{}, mirrors);
    // 0x00112D70 (after 0x0011F450 and the queue builds): the first background's light grid and parts.
    light_grid = {};
    if (auto* first = backgrounds->first_background(); first != nullptr && first->state.actor.model != nullptr)
    {
        light_grid.background = true;
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
    // TEMP DEBUG: background part kinds.
    if (auto* first = backgrounds->first_background(); first != nullptr && first->state.actor.model != nullptr
        && first->state.actor.model->runtime != nullptr)
    {
        const auto cpu = first->state.actor.model->runtime->source->cpu;
        const u32 header = read_u32(cpu, 0x10);
        std::fprintf(stderr, "PARTDBG header 0x%08X\n", header);
        if (header != 0)
        {
            const auto record = resources.memory.resolve(header, 0x10);
            const u32 table = read_u32(record, 0xC);
            std::fprintf(stderr, "PARTDBG table 0x%08X\n", table);
            if (table != 0)
            {
                const auto words = resources.memory.resolve(table, 8);
                const u32 count = read_u32(words, 0);
                const auto rows = resources.memory.resolve(read_u32(words, 4), count * 0x34);
                for (u32 index = 0; index < count; ++index)
                {
                    std::fprintf(stderr, "PARTDBG row %u kind %u\n", index, rows[index * 0x34 + 0x30]);
                }
            }
        }
    }
}

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
                RuntimeBoneDebrisDraw draw{state.model, read_u32(data(0x00427424, 4), 0), *model_draw};
                classes->draw_bone_debris(*object, pass, draw);
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
                RuntimeAvatarAfterimageDraw draw{{object->bytes.get(), object->size}, actor, state.model,
                    read_u32(data(0x00427428, 4), 0), *model_draw, *glow_pass};
                draw_runtime_avatar_afterimage(pass, draw);
                return;
            }
            // Types 0xAA and 0xA9 +0x14 is the weapon draw 0x001459C0: the actor draw 0x0003C9E0, then the hand prompt.
            if (type == 0xAA || type == 0xA9 || type == 0xA7 || type == 0xA0 || type == 0xA1 || type == 0xA2
                || type == 0xA3)
            {
                if (!classes->weapon_draw_hidden(*object))
                {
                    draw_scene_actor(*object, actor, pass);
                    classes->draw_weapon_prompt(*object, pass); // 0x00147560
                }
                if (type == 0xA3)
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
            // ghouly setups 0x24, 0x25 and 0x26, the bg-break key 0x1E and the spider web 0x1B (getters 0x0006A4F0,
            // 0x0006B110, 0x0006BB30, 0x0006CC70 and 0x000704D0).
            if (type != 0x1A && type != 2 && type != 0xF && type != 0x1F && type != 0x23 && type != 0x25
                && type != 0x24 && type != 0x26 && type != 0x1E && type != 0x19 && type != 0x1B)
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
                draw_runtime_generate_ghouly_state({player.second_action.bytes.get(), player.second_action.size}, kind,
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
                && type != 0x7E && type != 0x6B && type != 0x77
                && type != 0x6E && type != 0x6A && type != 0x6D && type != 0x81 && type != 0x76 && type != 0x7B
                && type != 0x67) // Getter 0x0008E010 keeps the empty 0x001628B0 at +0x18 too.
            {
                unbound("body +0x744 object vtable +0x18 draw for factory type "
                    + std::to_string(player.second_action.type));
            }
        }};
    draw_runtime_player({object.bytes.get(), object.size}, kind, draw);
}

// Actor vtable +0x18 by class. Types 2, 5, 0x15, 0x1A, 0x1B, 0x1F, 0x23 and 0xAA keep the base actor slot 0x0003C830
// (getters 0x0003C080, 0x00027F60, 0x0006D4A0, 0x0006EE20, 0x000704D0, 0x0006DBD0, 0x00069620 and 0x000CEF70); the FX emitter (type 0xD, getter
// 0x00109DD0) uses 0x00109E30.
u32 SceneActorServices::State::actor_visible(Actor& actor, i32 pass, i32 view)
{
    FactoryObject& object = object_of_actor(actor);
    if (object.type == 0xD)
    {
        return classes->emitter_visible(object, u32(pass), u32(view)) ? 1u : 0u;
    }
    if (object.type != 2 && object.type != 5 && object.type != 0xF && object.type != 0x10 && object.type != 0x12
        && object.type != 0x13 && object.type != 0x15
        && object.type != 0x1A && object.type != 0x1F && object.type != 0x23 && object.type != 0x25
        && object.type != 0x24 && object.type != 0x26 && object.type != 0x1E && object.type != 0x1D
        && object.type != 0x19 && object.type != 0x1B && object.type != 0x21 && object.type != 0xAA
        && object.type != 0xA9 && object.type != 0xA7 && object.type != 0xA0 && object.type != 0xA1
        && object.type != 0xA2 && object.type != 0xA3)
    {
        unbound("actor vtable +0x18 visibility for factory type " + std::to_string(object.type));
    }
    if (display_view == nullptr)
    {
        throw Error("actor visibility has no DAT_005077A8 display view");
    }
    ActorMatrixSetup* matrices = nullptr;
    if (object.type == 0x12)
    {
        // The bone debris keeps the base slot 0x0003C830; its +0x288 rows are the class's own.
        matrices = &classes->bone_debris_matrices(object);
    }
    else if (i32(read_u32({object.bytes.get(), object.size}, 0x28C)) > 0)
    {
        unbound("actor +0x288 matrix setup for view visibility");
    }
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
    ambient_context->scale = {g_509b20, g_509b24, g_509b28};
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
        g_50484c,
        [this]()
        {
            if (blob_shadow_enable != 0 && blob_shadow_count != 0 && blob_shadow_suppress == 0)
            {
                unbound("blob shadow draw 0x00119AD0");
            }
        },
        [this, first_background_model]()
        {
            RuntimeLightVolumeOrder order{g_light_filled, g_light_filled_gate, g_light_order};
            RuntimeLightVolumeDraw volumes{order, first_background_model(),
                [](u8 volume)
                {
                    unbound("light volume record draw 0x000FE5A0 for record " + std::to_string(volume));
                },
                [this]()
                {
                    bind_display_projection(*display_setup);
                }};
            draw_runtime_light_volumes(volumes);
        },
        [this]()
        {
            if (decal_enable != 0)
            {
                unbound("decal draw 0x00116460");
            }
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
                    cloth_ramp_texture,
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
            // 0x0012AB10. DAT_00512AF0 is filled by 0x0012A9B0 / 0x0012A9E0 / 0x0012AA70, which nothing native
            // calls yet.
            if (post_model_count != 0)
            {
                unbound("post-model pass 0x0012AB10 body");
            }
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
            // DAT_00506930..DAT_00506C50: 0x000ED390 for each nonzero word (the port's table holds 150 of them).
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
            draw_effect_debris(debris_pool->particles == nullptr ? std::span<const u8>{}
                : std::span<const u8>(debris_pool->particles, 0x16A8), debris_pool->textures, *display_setup);
        },
        [this]()
        {
            // 0x000EBCB0: live type-2 trail records queue billboards through 0x0005C1A0.
            if (trail_pool->particles != nullptr)
            {
                const std::span<const u8> particles(trail_pool->particles, 0x19C8);
                for (size_t offset = 0x18; offset < 0x18 + 50 * 0x84; offset += 0x84)
                {
                    if (i32(read_u32(particles, offset + 100)) == -2 && particles[offset + 0x58] == 2)
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
            // 0x0011AF90. DAT_00510560 is written by 0x0011AEA0 / 0x0011AEE0 / 0x000BA840 (not translated) and
            // cleared by 0x0011CBD0.
            if (decal_count != 0 && backgrounds->first_background() != nullptr)
            {
                unbound("decal draw 0x0011AF90 body");
            }
        },
        []()
        {
            // 0x00119230.
            if (g_50ec00 != 0 && g_50484c < 1)
            {
                unbound("shadow volume draw 0x00119230 body");
            }
        },
        []()
        {
            // 0x0011BA50. DAT_00510580 comes from 0x0011B7A0 in 0x0011CBD0, which the native start does not run.
            if (g_510580 != 0 && g_flag_0650 == 0)
            {
                unbound("refraction draw 0x0011BA50 body");
            }
        },
        [this]()
        {
            draw_screen_effects(RuntimeScreenEffects{screen_flash, screen_capture, g_50484c, line_retained},
                *dialogue_outer);
        },
        [this]()
        {
            draw_scene_dialogue_page(game);
        },
        caption_camera,
        []()
        {
            unbound("caption camera 0x00048BC0 / 0x0004DBD0 (DAT_004DE9AC)");
        },
        caption_widget,
        frontend_state,
        frontend_widget,
        [](u32)
        {
            unbound("front-end widget draw 0x00142720");
        },
        [this]()
        {
            flush_scene_dialogue_sprites(game);
        },
        [this]()
        {
            draw_runtime_scoreboard_hud(*scoreboard_hud); // 0x00132290
        },
        g_flags_ea9c,
        []()
        {
            draw_runtime_phase_overlay(g_510cf4, g_510e30, g_510e40, g_510e24);
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
            draw_runtime_dialogue_text_pools(*text_registry, g_flags_ea9c, *dialogue_draw); // 0x0005F330
        }};
#if defined(GBTG_INSPECTOR_APP)
    draw(present);
#else
    present_runtime_scene_view(present);
#endif
}

// Actor vtable +0x38, which 0x00121DC0 calls when set. Getters 0x0003C080 (type 2, the base every class below
// starts from, clears it), 0x00042BC0 (4), 0x00027F60 (5), 0x00109DD0 (0xD), 0x0006D4A0 (0x15), 0x0006EE20 (0x1A),
// 0x0006DBD0 (0x1F), 0x00069620 (0x23), 0x000704D0 (0x1B) and 0x000CEF70 (0xAA) leave it 0.
void SceneActorServices::State::view_refresh_slot(Actor& actor)
{
    for (const auto& [bytes, native] : actors->actors)
    {
        if (native == &actor)
        {
            const FactoryObject* object = object_of(bytes);
            const u16 type = object == nullptr ? 0 : object->type;
            if (type == 2 || type == 4 || type == 5 || type == 0xD || type == 0xF || type == 0x10 || type == 0x12
                || type == 0x13 || type == 0x15
                || type == 0x1A || type == 0x1F || type == 0x23 || type == 0x25 || type == 0x24 || type == 0x26
                || type == 0x1E || type == 0x1D || type == 0x19 || type == 0x1B || type == 0x21
                || type == 0xAA || type == 0xA9 || type == 0xA7
                || type == 0xA0 || type == 0xA1 || type == 0xA2 || type == 0xA3)
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
            const i32 tick = i32(float(texture[0x10]) * g_4312cc); // DAT_004312CC.
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
        reinterpret_cast<void*>(std::uintptr_t(read_u32(bytes, 0x1A8))), draw_matrices[&actor]};
}

void SceneActorServices::State::present_render_target(std::uintptr_t color, std::uintptr_t depth)
{
    if (color == SceneD3DDevice::kBackBuffer && render_target_depth_texture != 0)
    {
        // The target's depth surface aliases a LIN_A8R8G8B8 texture (boot 0x000620C0's DAT_005047E8): its texels
        // take the target's D24S8 words.
        std::vector<u32> words;
        gfx_read_native_target_depth(words);
        const auto& record = resources.d3d.resources.at(render_target_depth_texture);
        const size_t count = size_t(record.header[0]) * record.header[1];
        if (!words.empty())
        {
            if (words.size() != count || (record.header[4] != 0x12 && record.header[4] != 0x2E))
            {
                throw Error("render target depth texture is not a LIN_A8R8G8B8 or LIN_D24S8 texture of the target's size");
            }
            std::memcpy(resources.memory.tail(record.data).data(), words.data(), count * 4);
        }
        movie_textures.insert(resources.memory.pointer(render_target_depth_texture));
        render_target_depth_texture = 0;
    }
    if (color == SceneD3DDevice::kBackBuffer)
    {
        if (render_target_texture != 0 && created_textures.contains(render_target_texture))
        {
            // A created swizzled A8R8G8B8 (or linear) texture takes the rendered rows in its own texel order.
            RgbaImage rendered;
            gfx_end_native_target(rendered);
            const auto& record = resources.d3d.resources.at(render_target_texture);
            const auto texels = resources.memory.tail(record.data).first(size_t(record.header[0]) * record.header[1] * 4);
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
            movie_textures.insert(resources.memory.pointer(render_target_texture));
            render_target_texture = 0;
            return;
        }
        if (render_target_texture != 0)
        {
            RgbaImage rendered;
            gfx_end_native_target(rendered);
            const RuntimeMovieLockedSurface surface = lock_movie_texture(render_target_texture);
            const size_t row = size_t(rendered.width) * 4;
            for (int y = 0; y < rendered.height; ++y)
            {
                std::memcpy(surface.pixels + size_t(y) * size_t(surface.pitch), rendered.bgra.data() + size_t(y) * row,
                    row);
            }
            render_target_texture = 0;
        }
        return;
    }
    const auto found = d3d_device.surface_textures.find(color);
    if (found == d3d_device.surface_textures.end())
    {
        throw Error("render target surface has no texture");
    }
    if (render_target_texture != 0)
    {
        throw Error("render target set while another texture target is open");
    }
    const u32 texture = u32(found->second);
    const auto registered = resources.d3d.resources.find(texture);
    if (registered == resources.d3d.resources.end() || registered->second.kind != 3)
    {
        throw Error("render target surface is not a registered texture");
    }
    gfx_begin_native_target(i32(registered->second.header[0]), i32(registered->second.header[1]));
    render_target_texture = texture;
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

// D3DDevice_CopyRects(back buffer, null, 0, the texture's level 0, null): the 640x480 game viewport drawn so
// far (the host window's fitted 4:3 area) into the texture's linear 32-bit level.
void SceneActorServices::State::copy_back_buffer(u32 texture)
{
    RgbaImage frame;
    if (!gfx_readback_ui(frame) || frame.width <= 0 || frame.height <= 0)
    {
        throw Error("back-buffer copy readback failed");
    }
    const auto object = resources.memory.resolve(texture, 0x30);
    const u32 header = texture + read_u32(object, 4) + u32(object[0x12]) * 0x14;
    const auto registered = resources.d3d.resources.find(header);
    if (registered == resources.d3d.resources.end())
    {
        throw Error("back-buffer copy target is not a registered texture");
    }
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
        stage = "native actor view draw 0x0011C240";
    #if defined(GBTG_INSPECTOR_APP)
    if (state.game.inspection != nullptr)
        {
            state.game.inspection->drawing = true;
        }
#endif
        state.with_view_context(0, [](RuntimeSceneViewPresent& view)
        {
            view.mirror();
            view.refresh_actors();
            view.bins();
            view.anchor_sprites();
            view.billboards();
        });
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
void SceneActorServices::hold_boot_scene_order()
{
    auto& services = *state_->gameselect_services;
    hold_runtime_scene_order(services.scene_order, services); // 0x0011C480: 0x001234B0, EAX *DAT_00430CF8
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
    // 0x0011CBD0 then runs 0x0012A1E0 and the scoreboard set-up.
    set_up_runtime_scoreboard(*state.scoreboard_hud); // 0x00132040
    // 0x00049BA0 follows (not translated, like 0x0012A1E0). The player start latch DAT_005462FC, set by 0x0011CAC0,
    // ends here: a later kind 2 marker (0x00127FF0 after a walk-in) spawns its player.
    g_5462fc = 0;
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
    throw Error("scene actor update for an actor outside the scene");
}

void SceneActorServices::update_scene_actors()
{
    auto& state = *state_;
    auto* previous = state.effect_pass_scratch;
    state.effect_pass_scratch = &state.frame_effect_scratch;
    state.pause = g_sim_freeze;
    state.frame_delta = g_sim_dt;
    state.time_scale = g_sim_scale;
    state.motion_steps = g_4312e0;
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
        [this](const Actor& actor)
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
    const i32 run_state = i32(read_u32({g_510e58, 0x108}, 0x104)); // DAT_00510F5C, scene record +0x104.
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
        advance_scene_gameplay_time(g_4312c8, state.clock, g_flags_ea9c); // 0x0012B1F0
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
    // Held: 0x00120EF0 (State::update_shadow_casters, translated) registers the shadow volumes that present's
    // 0x000FE960 then draws through 0x000FE5A0, 0x000644F0 and 0x00064820, which are not translated yet.
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
    RuntimeEffectDebris debris{{state.debris_pool->particles, 0x16A8}, state.debris_pool->types,
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
    RuntimeEffectTrails trails{{state.trail_pool->particles, 0x19C8}, state.trail_pool->types,
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
    // DAT_00545C48/DAT_00545CCC (pair and group ids) are filled by script actions before startup.
    static const std::array<u16, 0x40> swap_ids{};
    static const std::array<u16, 0x28> group_ids{};
    const RuntimeSceneMarkerRandomizer randomizer{state.random, g_545cc8, swap_ids, g_545d1c, group_ids};
    if (g_545cc8 != 0 || g_545d1c != 0)
    {
        unbound("scene marker pair and group ids (DAT_00545C48/DAT_00545CCC)");
    }
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
        voices});
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
}
