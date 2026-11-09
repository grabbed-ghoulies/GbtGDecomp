#pragma once
// Internal to the scene actor services (scene_actor_services*.cpp): the State behind SceneActorServices,
// split across files by responsibility. Not for use outside those files.
#include "../entities/mind_target_policy.h"
#include "../entities/mind_fields.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "gbtg/clock.h"
#include "chapter5_countdown.h"
#include "../saves/save_summary.h"
#include "../camera/actor_camera_effects.h"
#include "../entities/powerup_flags.h"
#include "scene_actor_services.h"
#include "../entities/ghouly_spawn_trigger.h"
#include "../entities/body_death_effects.h"
#include "../entities/weapon_breakage.h"
#include "../entities/actor_template_creation.h"
#include "../frontend/app_error_screen.h"
#include "../frontend/dialogue_input.h"
#include "../frontend/scoreboard_messages.h"
#include "../entities/actor_scare_requests.h"
#include "../entities/physical_shape_events.h"
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
#include "../graphics/refraction_pass.h"
#include "../graphics/shadow_volume_pass.h"
#include "../video/movie_startup.h"
#include "attract_control.h"
#include "gameselect_control.h"
#include "../graphics/actor_effect_draw.h"
#if defined(GBTG_INSPECTOR_APP)
#include "scene_inspection.h"
#include "../inspector/inspection_view.h"
#include "scene_model_preview.h"
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
#include "../entities/body_hit_messages.h"
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
#include "scene_pause.h"
#include "background_morphs.h"
#include <bit>
#include "../frontend/dialogue_text_metrics.h"
#include "../frontend/gameselect_progress.h"
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
#include "scene_post_models.h"
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
#include "../entities/furnace_actor.h"
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
#include "../frontend/gameselect_challenges.h"
#include "../frontend/gameselect_options.h"
#include "challenge_runtime.h"
#include "super_scary_control.h"
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
#include "../entities/player_mind_messages.h"
#include "../entities/player_attachment.h"
#include "../entities/player_mind_target_control.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_set>

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
extern i32 g_player_start_enabled; // DAT_005462FC.
extern u16 g_challenge_game_name[0x40]; // DAT_004E19B4, scene.cpp.
extern i32 g_marker_group_count; // DAT_00545D1C.
extern i32 g_marker_swap_count; // DAT_00545CC8.
extern std::array<u16, 0x40> g_marker_swap_ids; // DAT_00545C48.
extern std::array<u16, 0x28> g_marker_group_ids; // DAT_00545CCC.
extern i32 g_script_30d0c; // DAT_00430D0C.
extern float& g_scene_hold_time; // DAT_004312CC.
extern float& g_scene_gameplay_time; // DAT_004312D0.
extern i32& g_scene_gameplay_mark; // DAT_004312D4.
extern i32& g_scene_gameplay_tick; // DAT_004312D8.
extern u32 g_flag_0650; // DAT_00510650.
extern u32 g_flag_10b4; // DAT_005110B4.
void scene_set_audio_level(float level); // Xbox 0x00038110, scene.cpp.

struct FactoryObject;
namespace scene_services_detail
{
[[noreturn]] void unbound(const std::string& what);
// D3DX matrix routines the shader matrix code calls (D3D host boundary), on row-major 4x4 floats.
RuntimeShaderMatrixBoundaries d3dx_matrix_boundaries();
extern SceneActorServices::State* active_services;
template <typename T>
T& field_at(FactoryObject& object, size_t offset)
{
    return *reinterpret_cast<T*>(object.bytes.get() + offset);
}
} // namespace scene_services_detail
using namespace scene_services_detail;

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
    // World-pass gates of 0x00121900's helpers: 0x00119990 sets them at the scene start (DAT_0050EF18 from the first
    // background's model) and script type 0xA4 writes DAT_0050EF1C.
    u32 blob_shadow_enable = 0; // DAT_0050EF18.
    i32 blob_shadow_count = 0; // DAT_0050EF10.
    u32& blob_shadow_suppress = g_flag_ef1c; // DAT_0050EF1C.
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
    // Released scene actors as their freed memory last held them, by address (never reused): retail keeps reading a
    // stale body +0x26C source, or a kept weapon's dead holder, from the freed actor.
    struct ReleasedActor
    {
        std::vector<u8> bytes;
        Vec3 position;
    };
    std::unordered_map<u32, ReleasedActor> released_actors;
    // Body message graphs (parent body handler 0x000253B0 and its derived slots).
    std::vector<const char*> callout_names; // 0x00421C9C.
    std::vector<const char*> flag_names; // 0x00338FB0.
    std::vector<const char*> object_tags; // DAT_0042A850.
    std::vector<const char*> surface_names; // DAT_004340A0.
    std::array<const char*, 4> shock_pass{};
    std::array<const char*, 4> shock_fail{};
    std::array<u16, 3> death_actions{}; // DAT_00217EB8.
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
    std::span<u8>& story_bits = g_runtime_profile.progress.bytes; // *(DAT_00506C6C+8), the profile's 8 bytes.
    u8& story_byte = g_runtime_profile.progress.bytes[4]; // Byte +4 of **(DAT_00506C6C+8).
    u8& intro_bits = g_runtime_profile.progress.bytes[0]; // Byte 0 of **(DAT_00506C6C+8): the intros shown.
    u8& unlock_flags = g_runtime_profile.unlocks.bytes[0]; // **(DAT_00506C70+8), the profile's unlock byte.
    // DAT_00506C8C, the last of the profile's DAT_00506C78 block: the scene's kills credited to tags 4, 9 and 0xE
    // (0x0012ACF0). The scene request (0x0011EB30) clears it unless a death restart set DAT_00430D94, as do a new
    // game and a load.
    i32& scene_kills = g_runtime_profile.times[5];
    const i32 brew_kills_required = 1; // DAT_00430D9C: 1 in the XBE and only read.
    std::array<u32, 5> port_devices{}; // DAT_0050E0F4.
    RuntimeCameraPulse pulse{}; // DAT_0050447C.
    std::unique_ptr<SceneMindServices> mind_services;
    std::unordered_map<const u8*, std::unique_ptr<SceneMindMessages>> mind_messages;
    std::unordered_map<const u8*, std::unique_ptr<SceneCameraMindParent>> camera_mind_parents; // Type 0x36 minds.
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
    u32& active_strategies = reinterpret_cast<u32&>(g_pause_count); // DAT_00510CEC.
    // The object create_runtime_player_attachment (0x0013A720) and its kin are attaching, before the scene keeps it,
    // and its bytes. object_of consults it only when no kept object owns the bytes.
    FactoryObject* attaching_object = nullptr;
    const u8* attaching_bytes = nullptr;
    std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attachment_actors;
    std::array<u16, 8> scale_exclusions{}; // DAT_0050E1B4, script world action 0x6D's tags.
    i32 scale_exclusion_count = 0; // DAT_0050E1B0
    std::array<u8, 0x40> animation_classes{}; // DAT_0050E170: a tag and a scale per 8 bytes (0x0010F660).
    i32 animation_class_count = 0; // DAT_0050E16C
    std::array<u8, 0x20> mind_class_rows{}; // DAT_0050E14C: a tag and a word per 4 bytes (0x0010F690).
    std::unordered_map<const u8*, const RuntimeScriptLookupTable*> action_tables;
    std::unordered_map<const u8*, const u8*> animation_sequences;
    std::array<RuntimeBodyResourceCacheEntry, 0x40> animation_cache{}; // DAT_004DD554.
    i32 animation_cache_count = 0; // DAT_004DD550.
    std::unordered_map<u32, const char*> action_names;
    RuntimeMindClassSettings mind_settings{0, mind_class_rows}; // DAT_0050E148.
    float intro_scale = 0.f; // DAT_00433BEC.
    // DAT_00433BE8 and DAT_00433BF0 are the health display's health_style and scoreboard_globals'
    // health_second_voice: 0x001307D0 sets the style the heart display (0x001301A0) reads.
    RuntimePlayerIntroSound intro_sound;
    i32 camera_players = 0; // DAT_00545AF0.
    RuntimePlayerCameraContext camera;
    std::vector<FactoryObject> cameras;
    RuntimeStrategyClassRanges strategy_ranges{g_strategy_class_range_count, g_strategy_class_ranges}; // DAT_0050E1C4, DAT_0050E1C8.
    std::span<u32, 2> pause_references{scene_detail::g_pause_references}; // DAT_0050E764, DAT_0050E768.
    // DAT_00511B38 as 0x00117400 walks it: the players' +0x934 nodes, rebuilt from the list before each walk.
    std::vector<RuntimePauseActor> pause_nodes;
    bool pause_walking = false;
    RuntimePauseActor* first_pause = nullptr; // DAT_00511B38.
    RuntimeActorPauseGroups pause_groups{pause_references, first_pause};
    // The GhoulyFreeze (DAT_005121E8, kind 7) and TraitorFever (DAT_00512308, kind 0x13) powerup rows' first words.
    RuntimePlayerScriptOverrides overrides{powerup_rows[7].active, reinterpret_cast<u32&>(powerup_rows[7].amount),
        powerup_rows[7].extra, powerup_rows[7].player, powerup_rows[0x13].active,
        reinterpret_cast<u32&>(powerup_rows[0x13].amount), powerup_rows[0x13].extra, powerup_rows[0x13].player};
    entity_detail::LatchNode*& latches = entity_detail::g_world.latch; // DAT_00511AA4.
    u32& decay_index = reinterpret_cast<u32&>(g_state_248c); // DAT_0051248C, the script's title mode.
    std::span<const u8> decay_enabled; // DAT_0030F144.
    RuntimeActorClock clock;
    RuntimePlayerStrategyResume resume;
    RuntimeScriptActorQueryContext query{{}, address_zero.data()};
    RuntimeMindTarget* targets = nullptr; // DAT_00545988.
    // DAT_00545988 walks the +8 links of the live attachments on DAT_00545984 (linked by 0x0012AFE0). Each
    // record gets a retail-layout image (+0x10 actor, +0x14.. the record fields) and a target view, kept per
    // record so pointers held by mind contexts stay valid; refresh_mind_targets() rebuilds them each frame.
    struct SceneMindTargetRow
    {
        std::array<u8, 0x68> bytes{};
        std::vector<u8> bits; // The target body's +0x15C bits at the last refresh; empty for a non-player body.
        std::vector<u8> actor_bytes; // The target actor's bytes at the last refresh.
        RuntimeScriptMindQuery body{}; // The target's +0x95C body query at the last refresh.
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
        // The actor's bytes, its +0x95C body query and the body's +0x15C bits as they were, which 0x00029C80 and
        // the target policies read through the record (+0x10 actor).
        std::vector<u8> actor_bytes;
        RuntimeScriptMindQuery body{};
        std::vector<u8> bit_bytes;
        EntityBitBlock bits{};
        std::unique_ptr<RuntimeMindTarget> target;
    };
    std::unordered_map<u32, std::unique_ptr<RetiredTarget>> retired_targets;
    // Actors 0x001232D0 has released, by address, each copied as it was with its pointers cleared (as in
    // `retire_target`). A dead ghouly's mind does not clear its +0xC target on the 0x7C notice, so it keeps a
    // released actor's address: retail holds a dangling pointer, which 0x00028C10's caller compares with live actors
    // (the copy equals none) and the mind's gaze reads (the copy holds the last pose).
    std::unordered_map<u32, std::unique_ptr<Actor>> retired_actors;
    std::unique_ptr<Actor> retire_actor(const Actor& actor)
    {
        auto copy = std::make_unique<Actor>(actor);
        copy->render_registration = nullptr;
        copy->model = nullptr;
        copy->attach = nullptr;
        copy->bind = nullptr;
        copy->link = nullptr;
        copy->euler_out = nullptr;
        copy->spawn_points = nullptr;
        copy->spawn_point_count = 0;
        copy->accepted_classes = nullptr;
        copy->accepted_class_count = 0;
        copy->spawn_owner = nullptr;
        copy->goal_ring = nullptr;
        for (auto& slot : copy->sound_slots)
        {
            static_cast<RuntimeActorSound&>(slot) = RuntimeActorSound{};
        }
        return copy;
    }
    // The copy keeps the actor's own pose, scales and cached matrix; every block it pointed at is freed or owned
    // elsewhere, so those pointers are cleared and reads through them raise or fall back as for an unbound actor.
    std::unique_ptr<RetiredTarget> retire_target(const SceneMindTargetRow& row, const RuntimeMindTarget& live)
    {
        // The row's last refresh holds what the freed blocks held: the body and its bits are freed first.
        const Actor& actor = live.actor;
        auto retired = std::make_unique<RetiredTarget>(RetiredTarget{row.bytes, actor});
        retired->actor_bytes = row.actor_bytes;
        retired->body = row.body; // live.body names the freed body.
        retired->body.bits = nullptr;
        if (!row.bits.empty())
        {
            retired->bit_bytes = row.bits;
            retired->bits.bytes = retired->bit_bytes.data();
            retired->body.bits = &retired->bits;
        }
        retired->target = std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, retired->bytes, live.address,
            retired->actor, retired->actor_bytes, retired->body, live.occupancy, nullptr});
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
        copy.goal_ring = nullptr;
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
    const void* script_records = nullptr; // DAT_0051108C.
    std::string goal_scratch; // DAT_00508EB8.
    std::vector<FactoryObject> callout_objects;
    // Weapons a released player made (0x0013A720 into the object list): they outlive their holder. A deque, as they
    // arrive while the factory walks and releases other objects by reference.
    std::deque<FactoryObject> released_weapons;
    std::unordered_map<const Actor*, Vec3> released_positions; // An actor's position as its view is released.
    RuntimeDialogueInputState input_state{keyboard, {}, 0, 0, 0, input_pool, 0, nullptr};
    u32& brews_taken = reinterpret_cast<u32&>(g_runtime_profile.times[4]); // DAT_00506C88, the profile's total.
    u32 brew_accepted = 0; // DAT_00430D98.
    i32 prompt_lock = -1; // DAT_004E1C34, 0x0011C480: the scene cutscene stack's top, which its runtime binds.
    u32& prompt_owner = reinterpret_cast<u32&>(dialogue_state.mode); // DAT_004EE1EC, the scene dialogue's mode.
    i32& difficulty = g_event_sequence_state; // DAT_005064D8, the event sequence state.
    std::unique_ptr<RuntimeScoreboardResources> event_board_resources;
    std::unique_ptr<ScriptEventSequenceContext> event_sequence;
    u32& intro_pending = g_intro_notice_pending; // DAT_00545B38.
    std::array<char, 0x80>& intro_name = g_intro_notice_name; // DAT_00545B44.
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
    // The links DAT_00511B48 walks: one per node on the DAT_00511B44 list (a player's actor +0x944 link), with that
    // player's body view (+0x18, actor +0x95C) and goal ring (+0x3C, actor +0x980). refresh_active_actor_links.
    std::vector<RuntimeActiveActorLink> active_actor_links;
    u32 notice_pending = 0; // DAT_00506E28.
    const Actor* notice_first = nullptr; // DAT_00506DFC.
    u32 notice_id = 0; // DAT_00506E00.
    std::array<RuntimeTutorialNotice, 4> notice_queue{}; // DAT_00506E04.
    i32 notice_count = 0; // DAT_00506E24.
    const Actor* notice_player = nullptr;
    // The shown bits are the profile's story bits (*(DAT_00506C6C + 8)), so an intro is shown once per save.
    RuntimeTutorialNotices notices{g_runtime_profile.progress.bytes, notice_pending, notice_first, notice_id,
        notice_queue, notice_count, notice_player};
    float occupancy_initial = 0.f;
    RuntimeGhoulIntro ghoul_intro;
    std::span<const u8> ghoul_intro_names; // DAT_00506E40, loaded once like 0x0011C3F0 does.
    std::unique_ptr<RuntimeGhoulIntroTunables> ghoul_intro_tunables;
    std::unique_ptr<RuntimeGhoulIntroContext> ghoul_intro_context;
    JumpOnTunables jump_tunables{}; // DAT_00428680..
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
    std::unique_ptr<RuntimePlayerHealthDisplay> health_display;
    RuntimeScoreboardGlobals scoreboard_globals;
    std::unique_ptr<RuntimeScoreboardHud> scoreboard_hud; // 0x00132040 / 0x00132100 / 0x00132290.
    std::unique_ptr<RuntimeScoreboardResources> scoreboard_resources;
    std::unique_ptr<RuntimeScoreboardUpdate> scoreboard_update; // 0x00132210.
    std::unique_ptr<RuntimeScoreboardDraw> scoreboard_draw; // 0x00132290's sprite queue and clocks.
    i32& challenge_mode = g_chapter5_mode; // DAT_00506C58, the chapter 5 countdown's mode.
    u32& challenge_count = reinterpret_cast<u32&>(g_runtime_profile.times[3]); // DAT_00506C84, the profile's total.
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
    RuntimeCandleFlameSpot& candle_spot = g_light_candle_spot; // DAT_0050D740..DAT_0050D74C.
    std::unique_ptr<RuntimeCandleServices> candle_services;
    std::unordered_set<const void*> weapon_bound; // Native blocks a weapon message bound for its words.
    // Backgrounds (factory type 4, vtable 0x004E1644) and the scene globals their constructor and message
    // slot use. The part tables (0x00044CA0, boot) and sector settings (0x000FEF60) load with the services.
    std::array<char, 0x100> room{}; // DAT_00545D40.
    std::array<BackgroundRegion, 8> background_regions{}; // DAT_0050EAA0, stride 0x2C.
    RuntimeShadowVolumeTargets shadow_volume_targets{}; // DAT_0050EC04..DAT_0050EC0C.
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
    u32& broken_parts = g_broken_background_parts; // DAT_00545978.
    std::array<float, 24> sampling_scratch{}; // DAT_0050D7A0: matrix, then the dialogue's scratch matrix.
    float* sampling_active = nullptr; // DAT_0050DD70.
    std::array<char16_t, 0x400> wide_scratch{}; // DAT_00507E78.
    u32 score_total = 0; // DAT_0054597C.
    u32& score_running_total = g_running_score; // DAT_00545980.
    std::array<char16_t, 0xA00> score_texts{}; // DAT_00432724.
    std::array<u32, 10> score_categories{}; // DAT_004326FC.
    std::array<u32, 10> score_points{}; // DAT_00433B24.
    u32 score_refresh = 0; // DAT_004326E8.
    std::unique_ptr<RuntimeScoreAwards> score;
    entity_detail::NavigationCellRegistration* door_pool = nullptr; // DAT_004EE2B8, allocated by 0x0011CBD0.
    std::vector<RuntimeDoorClips> door_rows;
    u32 door_camera_registered = 0; // DAT_004EE31C.
    u32 door_paused = 0; // DAT_004EE320.
    u32& door_camera_hold = reinterpret_cast<u32&>(g_transition_pause_count); // DAT_004DEA24.
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
    // Contact counts by actor tag, 0x3D words per tag: DAT_0053E528 and DAT_00524E2C, the script tallies' kind 5.
    std::span<u32> contact_totals = script_contact_totals();
    std::span<u32> contact_session = script_contact_session();
    AudioBankStreams* audio_banks = &audio.streams; // DAT_004DF370, DAT_004E030C.
    std::unique_ptr<AttractControlServices> attract_services; // Factory type 0x9B.
    std::vector<TrophyHeadRow> trophy_head_rows; // DAT_00301920.
    std::unique_ptr<TrophyHeadsControlServices> trophy_heads_services; // Factory type 0x92.
    std::unique_ptr<HauntedChairsControlServices> haunted_chairs_services; // Factory type 0x93.
    std::unique_ptr<RelicsStoreControlServices> relics_store_services; // Factory type 0x94.
    std::unique_ptr<SceneFxControlServices> fx_services; // Factory type 0x97.
    std::unique_ptr<Chapter2Scene4ControlServices> chapter2_scene4_services; // Factory type 0x95.
    std::unique_ptr<Chapter5CountdownServices> chapter5_countdown_services; // Factory type 0x9D and script type 0x89.
    RuntimeBackgroundMorphs* background_morphs = nullptr; // DAT_0050E388 while the scene's cloths exist.
    RuntimeBackgroundCloths background_cloths; // DAT_0050E390, DAT_0050E394, DAT_0050E388 and DAT_0050E418.
    u32 cloth_ramp_texture = 0; // DAT_005043B8.
    float cloth_water_phase = 0.f; // DAT_005462B0, BSS.
    RuntimeShadowProjector shadow_projector; // DAT_00504880..DAT_00504950.
    // Boot 0x000620C0's light volume texture (DAT_00504844, 256 x 256 A8R8G8B8, a render target) and its surface
    // (DAT_00504848); the scene makes them once, on its first light volume.
    u32 light_volume_texture = 0;
    DialogueSurfaceHandle light_volume_surface = 0;
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
    std::unique_ptr<RuntimeActorPlacementSpawn> placement_spawn; // 0x001349E0's views.
    std::unique_ptr<RuntimeMiniCooperSpawn> minicooper_spawn; // Powerup kind 0xC.
    std::unique_ptr<RuntimePowerupActors> powerup_actors;
    void* super_scary_text = nullptr; // DAT_00512484 (BSS; its writer is not native).
    NativePlayerInstance& case_player(u32 address); // A player by its actor address.
    void bind_button_bat(FactoryObject& object);
    // Factory type 0x9A, game select (docs/NATIVE_FRONTEND.md).
    std::unique_ptr<GameSelectControlServices> gameselect_services;
    std::unique_ptr<FinalScoreServices> final_score_services; // Over gameselect_services.
    std::unique_ptr<ScenePauseServices> pause_services; // 0x0011CE10 .. 0x0011DB50.
    RuntimeDialogueTextPool* script_title_pool = nullptr; // DAT_00512484, script world action 0xA's.
    std::span<const u8> gameselect_text; // DAT_005462C4
    std::span<const u8> scene_order_held; // DAT_00511B64
    u32 scene_order_release = 0; // DAT_00511B68
    float gameselect_stream_wait = 0.f; // DAT_005065C4
    std::span<u8> frontend_input; // DAT_00429BBC, the boot keyboard device (0x000E5A80), mapped while the scene runs.
    u32 frontend_input_address = 0;
    std::array<char16_t, 0x200> gameselect_lookup{}; // DAT_00507E78, the 0x000FF0A0 text buffer.
    void create_scene_objects(); // Xbox 0x0011C980
    void bind_gameselect_services();
    void with_scene_save(const std::function<void(RuntimeSceneSave&)>& run);
    void bind_book_services();
    RuntimeScreenTextureBoundaries screen_texture_boundaries();
    std::array<std::span<u8>, 2> book_pages{}; // DAT_005047D8, DAT_005047E4
    std::unique_ptr<TransitionBookServices> book_services;
    std::unique_ptr<SceneTransitionServices> transition_services; // The scene transition's (DAT_00546328).
    void bind_transition_services();
    void bind_pause_services();
    RuntimeModelRenderContext model_renderer{}; // DAT_004370A0..
    RuntimeSceneFog fog{}; // DAT_00509AFC..
    CutsceneCameraImpulse camera_impulse = ambient_flash_state(); // DAT_0050E7BC..DAT_0050E7D0, the lightning flash.
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
    AudioListenerSettings& listener = native_audio_listener(); // DAT_004D9798.., shared with the boot.
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
    DisplayClipBounds clip_bounds{g_amb_volume_enabled, std::span<const Vec3, 8>(g_amb_palette)}; // DAT_0050EA00.
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
    NativeBackgroundInstance* light_grid_background = nullptr; // DAT_0050E374, recorded at scene start.
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
    std::unordered_map<Actor*, RuntimeActorMatrixState> draw_matrices; // Empty +0x288 views for classes without one.
    // Actor +0x288..+0x2A0 of the object's class instance (a weapon, bone debris, emitter or background), or null.
    ActorMatrixSetup* actor_matrix_setup(FactoryObject& object);
    // The +0x288 view the draw's 0x00040200 refreshes: the class instance's, a player's, or an empty one.
    RuntimeActorMatrixState& actor_draw_matrices(FactoryObject& object, Actor& actor);
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
    u8 marker_shape_retained = 0; // Unwritten shape +3 stack byte in 0x000FFA30.
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
    // A scene actor's render registration by its address, for the Super Scary challenge's player and head (+0x2BC).
    ActorRenderRegistration& super_scary_render(u32 address);
    void end_scene();
    void reset_scene_data();

    std::span<const u8> data(u32 address, size_t size) const
    {
        return image.memory(address, size);
    }
    // A shader record's constant pointer as its 0xC dwords in the image, or null when the pointer is 0.
    // DAT_00433FFC: the plane normal 0x0013C0B0 tests particles against, offset by DAT_0032B184. Inferred from that dump.
    Vec3 effect_collision_plane() const
    {
        const auto words = data(0x00433FFC, 12);
        return {read_f32(words, 0), read_f32(words, 4), read_f32(words, 8)};
    }
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
    // The scene object with these bytes, which must exist. Taken again after anything that can create objects: the
    // object vectors may reallocate, and a held FactoryObject& would then dangle.
    FactoryObject& scene_object(const u8* bytes);
    bool release_actor_frame_object(Actor& actor, u32 object); // Xbox 0x0003DE30, ECX actor, ESI object
    u32 address_of(const void* pointer);
    // 0x0003BE20 over a scene actor's +0x8C callout records with callback 0 and value 0.
    void actor_callouts(u32 address, u32 id, const RuntimeActorCreationOptions* options);
    RuntimeBodyStateContext body_state_context(u32 body_address); // A state action's body views (state +8).
    // A DAT_00545988 record by address: a live one, or a freed one's last state (retail reads the freed block).
    RuntimeMindTarget* mind_target_at(u32 address);
    void refresh_mind_targets();
    void refresh_active_actor_links();
    // The base strategy messages' view of the actor at a retail address: its +0x1FC / +0x1FE words, its +0x8F8 target
    // record and, for a player, the actor its body +0x274 names. Rebuilt on each call; kept per address.
    RuntimeStrategyMessageActor* strategy_message_actor(u32 address);
    // Actor +0x8F8: the address of the first record on the scene actor's +0x8F4 live attachment list (the record
    // minds hold as a target), or 0.
    u32 first_live_attachment_at(u32 actor_address);
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
    void update_furnace(FactoryObject& object); // Type 0x17, vtable +0x10 0x0006E260.
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
    std::vector<BackgroundListEntry> background_list(); // DAT_00511B20 in list order, for 0x0013B980.
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
    // DAT_00511B38 players as 0x00129420 reads them. Each player keeps its view, refreshed on every walk, so a
    // walk nested inside another's predicate leaves the outer views valid.
    struct SceneMarkedActor
    {
        RuntimeScriptMindQuery mind{};
        RuntimeScriptMindQueryNode node{};
        RuntimeScriptActorQueryBinding binding{};
        RuntimeScriptActorQueryReference reference{};
        u16 tag = 0;
    };
    std::unordered_map<const NativePlayerInstance*, std::unique_ptr<SceneMarkedActor>> marked_actor_query_views;
    std::vector<RuntimeScriptActorQueryView> marked_actor_views();
    Actor* actor_at_address(u32 address); // Null for address zero.
    // A read of an actor by address that may name one 0x001232D0 released (see `retired_actors`).
    Actor* actor_or_retired(u32 address);
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
    std::unique_ptr<SceneModelPreview> model_preview; // The asset viewer's model (SceneActorServices::preview_model).
#endif
    // The ambient sampler's view of DAT_0050D770/DAT_0050D780 (filled by 0x000FEFD0), the DAT_00509B20..28 tint and
    // DAT_005120F0, refreshed before a view draws.
    void refresh_ambient_context();
    RuntimeSceneMirrors mirrors; // DAT_00511090..DAT_00511180, filled by 0x0011F450 at scene start.
    u32 mirror_pass = 0; // DAT_0051120C.
    RuntimePostModels post_models; // DAT_00512AF0 (0x0012A9B0 / 0x0012A9E0 / 0x0012AA70 / 0x0012AB10).
    RuntimeIntroNoticeTick intro_notice_tick(); // 0x0013FD60 / 0x0013FFD0's views.
    RuntimePostModelContext post_model_context()
    {
        return RuntimePostModelContext{post_models, effect_models,
            [this](const void* pointer)
            {
                return resources.memory.address_of(static_cast<const u8*>(pointer));
            },
            clock, display_view, display_setup.get(), model_draw.get(),
            {&g_scene_ambient_tint_0, &g_scene_ambient_tint_1, &g_scene_ambient_tint_2, &g_scene_ambient_tint_3}, origin, 3500.f}; // DAT_0032B5C8.
    }
    RuntimeLightCones light_cones{}; // DAT_0050EF20 / DAT_00510560 (0x0011AEA0 / 0x0011AEE0 / 0x000BA840).
    i32& frontend_state = g_challenge_run.state; // DAT_004E17C8, the running challenge's state.
    std::unique_ptr<ChallengeRuntimeServices> challenge_services; // 0x0004A100 .. 0x0004B360.
    std::unique_ptr<RuntimeScoreCaptions> score_captions; // 0x0012DCD0 / 0x0012DDB0's text.
    u32 hud_enabled = 0; // DAT_005493E0 (0x00132040 in 0x0011CBD0).
    RuntimeScreenTint screen_tint{}; // DAT_00504450.
    RuntimeScreenLines screen_lines{0xFFFFFFFFu, 1.f, line_retained}; // DAT_005044A8 .. DAT_005044B0.
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
    void close_render_target();
    // The open target's pixels at the texture's size. A scene target also leaves the window-resolution frame it
    // was drawn at in `full` (empty otherwise).
    void end_render_target(RgbaImage& rendered, RgbaImage& full);
    // Presentation only: the texture's memory keeps its 640x480-sized texels, which native code reads, while draws
    // that sample it get `full`, the frame at the window's resolution (as the Xbox drew it at 640x480), so a page
    // drawn back over the screen is as sharp as the screen. Nothing happens for an empty `full`.
    void show_full_resolution(u32 texture, RgbaImage& full);
    u32 render_target_depth_texture = 0; // A texture surface given as the open target's depth.
    bool render_target_scene = false; // The open target shares the back buffer's depth-stencil.
    void copy_back_buffer(u32 texture);
    u32 render_target_texture = 0;
    // GBTG_OPTIMIZE step 12: a created texture target that ended on the GPU (gfx_end_native_target_into). Its
    // data is written once the presenter's commands are done (finish_render_target), before native code runs
    // again. The member exists in every build so the layout does not depend on the option.
    u32 pending_target_texture = 0;
    void finish_render_target();
    void write_target_texels(u32 texture, const RgbaImage& rendered);
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
} // namespace gbtg
