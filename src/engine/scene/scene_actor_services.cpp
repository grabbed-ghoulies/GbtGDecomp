#include "scene_actor_services.h"
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
#include "../entities/entity_collision.h"
#include "../entities/aligned_heap.h"
#include "../entities/body_move_actions.h"
#include "../entities/body_state_actions.h"
#include "../entities/actor_callout_resources.h"
#include "../entities/actor_shape_bounds.h"
#include "../entities/player_messages.h"
#include "scene_actor_activation.h"
#include "scene_body_messages.h"
#include "scene_mind_messages.h"
#include "scene_object_requests.h"
#include "background_class_owner.h"
#include "background_update.h"
#include "scene_actor_update.h"
#include "scene_view_actors.h"
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
#include "../audio/bank_streams.h"
#include "scene_controls.h"
#include "scene_control_update.h"
#include "scene_object_requests.h"
#include "../frontend/frontend_slots.h"
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
#include "../entities/player_mind_setup.h"
#include "../navigation/navigation.h"
#include "../entities/entity.h"
#include "../audio/core_cue.h"
#include "../audio/event_voices.h"
#include "../frontend/brew_meter.h"
#include "../frontend/player_health_display.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_vm.h"
#include "../transitions/scene_transitions.h"
#include "../scripts/script_conditions.h"
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
extern u32 g_flag_ea98; // DAT_0050EA98.
extern i32 g_5462fc; // DAT_005462FC.
extern i32 g_545d1c; // DAT_00545D1C.
extern i32 g_545cc8; // DAT_00545CC8.
extern i32 g_script_30d0c; // DAT_00430D0C.

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
    u32 decal_enable = 0; // DAT_005462EC.
    u32 ambient_point_draw = 0; // DAT_005120F8.
    void* portal_list = nullptr; // DAT_0050E390.
    AudioCoreSources audio_sources{};
    AudioCoreCues audio_cues{audio_sources};
    AudioEventVoices voices{audio_cues, {}, {}, {}, 0, origin, default_cone};
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
    std::unordered_map<ActorFrameAttachment*, FactoryObject*> frame_objects;
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
    i32 excluded_count = 0; // DAT_0050E228, cleared by 0x0011C480.
    u8* navigation_nodes = nullptr; // DAT_0050E24C; the navigation loader that allocates it is not native yet.
    std::array<u16, 0x20> excluded_tags{}; // DAT_0050E22C.
    std::unordered_map<u32, Actor*> addresses;
    // Input (DAT_0050E0E8 and its devices).
    std::unordered_map<u32, std::span<u8>> input_addresses;
    RuntimeInputReleaseContext input_release{input_addresses};
    // 0x00033030, inline pool allocation at 0x00033274: four 0x50-byte input entries.
    GhoulySpawnPool* input_pool = allocate_runtime_shape_pool(0x50, 4);
    i32 input_selected = 0; // DAT_0050E0EC.
    std::span<u8> controllers = runtime_pad_state_views().controllers; // DAT_005079F8.
    std::span<u8> keyboard = runtime_pad_state_views().merged; // DAT_00507C98.
    std::array<u32, 5> port_masks{}; // DAT_0050E0F4..
    RuntimeInputActivationContext input_activation{*input_pool, input_selected, controllers, keyboard,
        port_masks.data() + 1, input_addresses};
    // Player construction and children.
    u32 player_count = 0; // DAT_00510578.
    u32 active_strategies = 0; // DAT_00510CEC.
    std::unordered_map<const u8*, RuntimePlayerAttachmentActor*> attachment_objects;
    std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attachment_actors;
    std::array<u16, 8> scale_exclusions{}; // DAT_0050E1B4.
    std::array<u8, 0x40> animation_classes{}; // DAT_0050E170.
    std::unordered_map<const u8*, const RuntimeScriptLookupTable*> action_tables;
    std::unordered_map<const u8*, const u8*> animation_sequences;
    std::array<RuntimeBodyResourceCacheEntry, 0x40> animation_cache{}; // DAT_004DD554.
    i32 animation_cache_count = 0; // DAT_004DD550.
    std::unordered_map<u32, const char*> action_names;
    RuntimeMindClassSettings mind_settings{0, {}}; // DAT_0050E148.
    u32 intro_enabled = 0; // DAT_00433BE8.
    float intro_scale = 0.f; // DAT_00433BEC.
    i16 intro_voice = 0; // DAT_00433BF0.
    RuntimePlayerIntroSound intro_sound;
    i32 camera_players = 0; // DAT_00545AF0.
    RuntimePlayerCameraContext camera;
    std::vector<FactoryObject> cameras;
    RuntimeStrategyClassRanges strategy_ranges{0, {}}; // DAT_0050E1C4.
    std::array<u32, 2> pause_references{}; // DAT_0050E764.
    RuntimePauseActor* first_pause = nullptr; // DAT_00511B38.
    RuntimeActorPauseGroups pause_groups{pause_references, first_pause};
    std::array<u32, 8> override_words{}; // DAT_005121E8..
    RuntimePlayerScriptOverrides overrides{override_words[0], override_words[1], override_words[2],
        override_words[3], override_words[4], override_words[5], override_words[6], override_words[7]};
    entity_detail::LatchNode*& latches = entity_detail::g_world.latch; // DAT_00511AA4.
    u32 decay_index = 0; // DAT_0051248C.
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
    // Player minds register their eligibility (body +0x95C), address and body query views by actor.
    std::unordered_map<Actor*, const RuntimePlayerAttachmentEligibility*> target_eligibility;
    std::unordered_map<Actor*, u32> target_addresses;
    std::unordered_map<const Actor*, RuntimeScriptMindQuery*> target_queries;
    RuntimeScriptMindQuery absent_target_query{}; // Actors whose +0x95C is not a player body.
    u32 attachment_notice = 0; // DAT_00433BD0.
    const void* script_records = nullptr; // DAT_0051108C.
    std::string goal_scratch; // DAT_00508EB8.
    std::vector<FactoryObject> callout_objects;
    RuntimeDialogueInputState input_state{keyboard, {}, 0, 0, 0, input_pool, 0, nullptr};
    u32 brews_taken = 0; // DAT_00506C88.
    u32 brew_accepted = 0; // DAT_00430D98.
    i32 prompt_lock = -1; // DAT_004E1C34, 0x0011C480.
    u32 prompt_owner = 0; // DAT_004EE1EC.
    i32 difficulty = 0; // DAT_005064D8.
    i32 brews_owned = 0; // DAT_00506C8C.
    i32 brews_required = 0; // DAT_00430D9C.
    u8 story_byte = 0; // **(DAT_00506C6C+8) +4.
    u8 unlock_flags = 0; // **(DAT_00506C70+8).
    u32 intro_pending = 0; // DAT_00545B38.
    std::array<char, 0x80> intro_name{}; // DAT_00545B44.
    entity_detail::ListHead& cases = entity_detail::g_world.case_head; // DAT_00511B3C; its next is DAT_00511B40.
    std::function<const EntityBitBlock*(const entity_detail::ListHead*)> case_body_bits;
    std::array<u32, 0x3C> queue_limits{}; // DAT_0050E570, 0x00116DA0 sets the first to 3.
    std::array<u32, 0x3C> class_queue_limits{}; // DAT_0050E660, 0x00116DA0 sets 99.
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
    i32 attachment_limit = 0; // DAT_00428684.
    std::array<u8*, 200> timed_attachments{}; // DAT_00506930, cleared by 0x0011C480.
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
    std::array<u8, 0x20> challenge_bits{}; // *(DAT_00506C54+8).
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
    u32 door_camera_hold = 0; // DAT_004DEA24.
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
    std::unique_ptr<AudioBankStreams> audio_banks; // DAT_004DF370, DAT_004E030C.
    void create_scene_objects(); // Xbox 0x0011C980
    RuntimeModelRenderContext model_renderer{}; // DAT_004370A0..
    RuntimeSceneFog fog{}; // DAT_00509AFC..
    CutsceneCameraImpulse camera_impulse{}; // DAT_0050E7BC..
    CutsceneCameraEventAsset camera_event_asset{}; // DAT_004E1C4C..
    Vec3 listener_position{};
    u32 movie_flag_38 = 0;
    u32 movie_flag_3c = 0;
    std::array<RuntimeMovieEventSlot, 4> movie_slots{};
    i32 movie_mode = 0; // DAT_0050DD8C.
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
    SceneD3DDevice d3d_device;
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
    std::array<float, 3> ambient_scale{}; // DAT_00509B20.
    u8 environment_world_space = 0; // DAT_004D9668.
    std::array<float, 4> retained_scene_color{}; // DAT_0032E4E4.
    std::array<u32, 20> light_records{};
    std::unordered_map<const RuntimeRenderLight*, std::array<u32, 4>> spot_parameters;
    SceneLightUploadBoundaries light_boundaries;
    std::unique_ptr<SceneLightUploadContext> light_upload;
    RuntimeSceneLightDirection light_direction{}; // DAT_00509B10..
    std::array<float, 4> override_base_color{}; // DAT_0042A570.
    std::array<u32, 16> environment_primary{}; // DAT_004D9710.
    std::array<u32, 16> environment_secondary{}; // DAT_004D9680.
    std::array<float, 3> scene_color{}; // DAT_00509B30.
    RuntimeRenderLight directional_light{}; // DAT_00509AB0.
    std::array<const SceneLocalLightView*, 40> local_lights{}; // DAT_00509B7C.
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
    const std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback> attachment_draws; // Attachment vtable +0x14.
    // Markers.
    const u8* markers = nullptr; // DAT_0051108C.
    RuntimeMarkerCameraStart camera_start{}; // DAT_00545AD0.
    std::unique_ptr<RuntimePlayerSpawnContext> spawn;
    std::unique_ptr<RuntimeMarkerPlayerStart> player_start;
    std::unique_ptr<RuntimeSceneMarkerBindings> marker_bindings;
    RuntimeSceneMarkerHandlers handlers;

    std::span<const u8> data(u32 address, size_t size) const
    {
        return image.memory(address, size);
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
    u32 address_of(const void* pointer);
    void refresh_mind_targets();
    // Engine-heap blocks (0x000F72D0 allocations) the scene's objects store: a token address per block.
    std::map<const void*, u32> heap_addresses;
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
    void activate_actors();
    RuntimeSceneActivation activation_context();
    RuntimeDirectorActor director_actor(NativePlayerInstance& player);
    void update_player(NativePlayerInstance& player); // Player vtable +0x10, 0x0002C2B0.
    void update_contact_prop(FactoryObject& object);
    void update_base_actor(FactoryObject& object);
    RuntimeSegmentContactActor& prop_contact_actor(entity_detail::SegmentProbe& probe);
    void latch_message(entity_detail::LatchNode& node, const Message& message);
    std::unique_ptr<RuntimeBackgroundConstructionServices> background_services(NativeBackgroundInstance& background,
        NativeActorFactoryInstance& view, RuntimeBackgroundAnimation*& first_animation);
    void background_message(NativeBackgroundInstance& background, std::span<u32> message);
    void background_update(NativeBackgroundInstance& background);
    void update_actor(u32 address);
    FactoryObject& object_of_actor(Actor& actor);
    void start_background();
    void actor_draw(Actor& actor, i32 view, i32 pass); // Actor vtable +0x14 by class.
    u32 actor_visible(Actor& actor, i32 pass, i32 view); // Actor vtable +0x18 by class.
    void draw_scene_actor(FactoryObject& object, Actor& actor, i32 kind); // 0x0003C9E0.
    void draw_player(FactoryObject& object, NativePlayerInstance& player, i32 kind); // 0x00028050.
    void draw_view_actors(i32 view); // 0x0011C240's actor bins for one view.
    // The ambient sampler's view of DAT_0050D770/DAT_0050D780 (filled by 0x000FEFD0), the DAT_00509B20..28 tint and
    // DAT_005120F0, refreshed before a view draws.
    void refresh_ambient_context();
    // The legacy host world draw stands in for the background's +0x14 (0x00042C10) while a view presents.
    const std::function<void()>* host_world_draw = nullptr;
    std::array<const u8*, 10> overlay_digits{}; // DAT_00545A80, BSS until 0x0012E070 loads the digit sprites.
    ActorDrawState actor_draw_state(FactoryObject& object, Actor& actor, std::unique_ptr<ActorDrawModelView>& model);
    // Host presentation of the recorded dialogue quads, with decoded images of their texture frames.
    std::unordered_map<const u8*, std::unique_ptr<RgbaImage>> texture_images;
    const RgbaImage& texture_image(const u8* record);
};

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
      intro_sound{intro_enabled, intro_scale, intro_voice, story_byte, sounds},
      camera{scene.factory, image.pools, image.string(0x00314E50), data(0x00314EB0, 0x30), g_flags_ea9c,
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
    weapon_names = strings(0x00434104, 5);
    class_names = strings(0x004340EC, 5);
    mind_goal_names = strings(0x003397BC, 0x31);
    decay_enabled = data(0x0030F144, 4);
    std::memcpy(&intro_enabled, data(0x00433BE8, 4).data(), 4);
    std::memcpy(&intro_scale, data(0x00433BEC, 4).data(), 4);
    std::memcpy(&intro_voice, data(0x00433BF0, 2).data(), 2);
    std::memcpy(&attachment_limit, data(0x00428684, 4).data(), 4);
    // 0x00116DA0, from 0x0011CAC0 before scene startup.
    queue_limits[0] = 3;
    class_queue_limits.fill(99);
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
    case_body_bits = [](const entity_detail::ListHead*) -> const EntityBitBlock*
    {
        unbound("case actor body bits (actor +0x95C +0x15C)");
    };
    word_messages.memory_at = [this](u32 address) -> std::span<const u8>
    {
        return scene.resolve_address(address, 0);
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
    actors->install(scene.types);
    players = std::make_unique<NativePlayerFactoryOwner>(NativePlayerFactoryBindings{*actors,
        NativePlayerSceneBindings{scene.factory, image.pools, resources.registry, resources.error, random,
        input_release, input_activation, g_flags_ea9c, g_flag_ea98, player_count, active_strategies,
        address_zero,
        [this](const void* pointer)
        {
            return address_of(pointer);
        },
        actor_tags.data(), statsheet.data(), statsheet, weapon_names.data(), class_names.data(),
        read_u32(data(0x00338EE0, 4), 0), matrix_boundaries, attachment_objects, attachment_actors,
        {}, 0, {}, 0, data(0x00433D68, 0x10), data(0x00433CA8, 0x10), resources.loaded_sizes, image.state_tables, image.attack_sequences,
        animation_cache, animation_cache_count, image.state_row_names, timed_attachments,
        [this](const Actor& actor)
        {
            return model_views_of(actor);
        },
        mind_settings, intro_sound, camera, cameras, g_scene_flags->bytes[0], g_script_30d0c,
        g_script_30d0c, game.scene_script.name.c_str(), strategy_ranges, pause_groups, overrides, latches, game, clock,
        resume, query,
        [](u32) -> RuntimeStrategyMessageActor*
        {
            unbound("strategy message actors");
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
        [](const char*, u32) -> u32
        {
            unbound("imp fire effects (0x0003DF40)");
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
        attachment_limit}});
    players->install(scene.types);
    players->minds.install_cinema(scene.types);
    players->defer_release = true;
    scene.released = [this](const u8* bytes)
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
        for (auto* player : players->instances())
        {
            if (player->actor_bytes.data() == bytes)
            {
                model_views.erase(&player->actor);
                players->release_player(bytes);
                return;
            }
        }
    };

    frame = std::make_unique<ActorFrameContext>(ActorFrameContext{effects, nullptr, node_cache, reset_matrix,
        matrix_boundaries, pause, frame_delta, time_scale, 0, pause_references[1], decay_enabled[decay_index]});
    emitter = std::make_unique<NativeSceneEmitterBindings>(NativeSceneEmitterBindings{*frame,
        timed_attachments, clock, display_view, challenge_bits, g_scene_flags->bytes[0], g_flags_ea9c,
        emitter_players, resume, story_byte, intro_notices, emitters,
        [](u32, RuntimeFxEmitterPlayer&, u32, float, u32)
        {
            unbound("powerup award 0x00123E70");
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
    classes = std::make_unique<NativeSceneActorClassOwner>(NativeSceneActorClassBindings{*actors, *marker_objects,
        effects, prompt_lock, area_dialogue, random, occupancy_initial,
        [this](FactoryObject& object) -> ActorBaseContext&
        {
            return base_of(object).base;
        },
        [this](u32 address)
        {
            return scene.resolve_address(address, 0);
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
        word_messages, *emitter});
    classes->install(scene.types);
    classes->defer_release = true;
    install_runtime_move_action_classes(scene.types);
    install_runtime_body_state_classes(scene.types, [this](u32 body_address)
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
            [](std::span<const u32>)
            {
                unbound("body message slot (vtable +0xC)");
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
            }};
    });
    scene.install_base_class();

    // Markers (0x0011CB80 loads them into DAT_0051108C) and the kind handlers (DAT_0042A720).
    scene.load_markers(game.scene_script.name.c_str());
    markers = scene.markers().data();
    script_records = markers;
    spawn = std::make_unique<RuntimePlayerSpawnContext>(RuntimePlayerSpawnContext{scene.factory, image.pools,
        game.scene_script.name.c_str(), std::span<const u8, 0x210>(data(0x00430DA0, 0x210).data(), 0x210),
        g_scene_flags->bytes[0], g_flags_ea9c, reinterpret_cast<const char*>(&g_byte_30d14)});
    player_start = std::make_unique<RuntimeMarkerPlayerStart>(RuntimeMarkerPlayerStart{*spawn, markers,
        address_zero, reinterpret_cast<const u32&>(g_5462fc), g_script_30d0c, g_scene_flags->bytes[0],
        g_flags_ea9c,
        []() -> bool
        {
            unbound("script end gate 0x00123B00");
        },
        scene.objects,
        [this](FactoryObject& object, std::span<const u32> message)
        {
            object_message(object, {const_cast<u32*>(message.data()), message.size()});
        }});
    marker_bindings = std::make_unique<RuntimeSceneMarkerBindings>(RuntimeSceneMarkerBindings{*player_start,
        *marker_objects, actor_tags.data(), camera_start, nullptr});
    handlers = bind_runtime_scene_marker_handlers(*marker_bindings);
    callout_names = strings(0x00421C9C, 0x55);
    flag_names = strings(0x00338FB0, 0x41);
    object_tags = strings(0x0042A850, 0xEA);
    surface_names = strings(0x004340A0, 0x12);
    shock_pass = {image.string(0x003168A4), image.string(0x00316890), image.string(0x0031687C),
        image.string(0x00316860)};
    shock_fail = {image.string(0x00316854), image.string(0x00316840), image.string(0x0031682C),
        image.string(0x00316810)};
    std::memcpy(death_actions.data(), data(0x00217EB8, 6).data(), 6);
    damage_tables = std::make_unique<RuntimeBodyDamageTables>(RuntimeBodyDamageTables{data(0x00433C9C, 0xC * 0x10),
        statsheet});
    footstep_kinds = strings(0x00423620, 1);
    footsteps = std::make_unique<FootstepSoundNames>(FootstepSoundNames{data(0x00314FBC, 0x20), surface_names});
    load_background_part_tables(part_tables, resources.registry, resources.loaded_sizes, resources.error);
    const std::function<u32(const u8*)> effect_address = [this](const u8* pointer)
    {
        return resources.memory.address_of(pointer);
    };
    initialize_effect_debris(*debris_pool, resources.registry, resources.error, effect_address);
    initialize_effect_trails(*trail_pool, resources.registry, resources.error, effect_address);
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
        door_shapes, data(0x00424994, 12 * 0x94)});
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
    audio_banks = std::make_unique<AudioBankStreams>(AudioBankStreams{{}, {}, {}, voices});
    initialize_tracked_audio_channels(*audio_banks); // Boot 0x00033030 runs 0x00038BF0 over DAT_004E030C.
    scene.controls->context.clock = &clock;
    scene.controls->context.audio = audio_banks.get();
    // The XACT engine boundaries behind the audio core: heap, sound sources and cue preparation.
    audio_sources.memory = native_audio_core_memory();
    audio_sources.boundaries = native_audio_core_source_boundaries();
    audio_cues.boundaries = native_audio_core_cue_boundaries(game, [](void* bank, u32 index) -> const char*
    {
        if (g_native_audio == nullptr)
        {
            unbound("audio core cue names before boot 0x0003A280");
        }
        static std::string name;
        name = native_audio_cue_name(*g_native_audio, bank, index);
        return name.c_str();
    });
    if (g_native_audio != nullptr)
    {
        // DAT_004E13B0, the sound banks 0x00039F50 created at boot.
        voices.banks = std::span<AudioCoreBank* const>(g_native_audio->banks.data(),
            size_t(g_native_audio->bank_count));
    }
    audio_cues.sound = native_sound_boundaries();
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
        cutscenes.get(), nullptr, door_instance_address, door_models, node_cache});
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
    // Boot display, input and text globals, in 0x00033030 order.
    layout_runtime_hud(entity_detail::g_aspect_exact, hud_size[0], hud_size[1], hud);
    initialize_display_views(hud_size[0], hud_size[1], render_width, render_height);
    port_masks = {0, 1, 2, 4, 8}; // 0x00033248: DAT_0050E0F4 0, then DAT_0050E0F8..DAT_0050E104 1, 2, 4, 8.
    input_state.selected_mask = port_masks[1];
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
        resources.text, {i32(challenge_bits.size()), challenge_bits.data()}, 0, 0, 0, 0, 0, 0, 0.f,
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
        read_u32(data(0x004273D4, 4), 0), read_u32(data(0x00425E6C, 4), 0), read_u32(data(0x004273C8, 4), 0),
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
    dialogue_outer = std::make_unique<RuntimeDialogueOuterDrawContext>(RuntimeDialogueOuterDrawContext{*dialogue_draw,
        render_targets, d3d_device.outer(), *sprites, {}, *display_setup, text_draw_count, capture_enabled,
        std::bit_cast<u8>(u8(0)), g_flags_ea9c, display_address, read_u32(data(0x004271F4, 4), 0),
        read_u32(data(0x00425E0C, 4), 0), read_u32(data(0x004271E8, 4), 0), line_retained});
    bind_scene_dialogue(game, *script_dialogue, *dialogue_outer);
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
        environment_world_space, retained_scene_color, light_records, spot_parameters, light_boundaries});
    ambient_context = std::make_unique<SceneAmbientContext>(SceneAmbientContext{ambient_grids, 0, light_override, {}, {}, 0,
        ambient_points});
    std::memcpy(ambient_context->override_color.data(), data(0x0042A560, 0x10).data(), 0x10);
    environment = std::make_unique<SceneEnvironmentSetupContext>(SceneEnvironmentSetupContext{*light_upload, fog,
        *ambient_context, light_direction,
        *reinterpret_cast<const std::array<float, 4>*>(background_ambient.light_color.data()), override_base_color,
        environment_primary, environment_secondary});
    light_construction = std::make_unique<SceneLightConstructionContext>(SceneLightConstructionContext{*environment,
        scene_color, retained_scene_color, 0.f, read_f32(data(0x0042A580, 4), 0), directional_light, local_lights,
        temporary_lights});
    model_draw = std::make_unique<SceneModelDrawContext>(SceneModelDrawContext{render_image, *light_construction,
        matrix_boundaries});
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
        d3d_device.set_state(0x10061, color); // D3D 0x001713D0.
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
            d3d_device.set_state(SceneD3DDevice::kPixelShader, shader);
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
            auto& draw = d3d_device.draw_indexed(primitive, count, indices);
            // D3D_g_DeferredTextureState (DAT_00181970): stage stride 0x20 words, ADDRESSU then ADDRESSV.
            for (u32 stage = 0; stage < 4; ++stage)
            {
                const u32 u = deferred_states[2 + stage * 0x20];
                const u32 v = deferred_states[2 + stage * 0x20 + 1];
                draw.address_u[stage] = u == 0 ? 1 : u;
                draw.address_v[stage] = v == 0 ? 1 : v;
            }
        }});
    install_runtime_push_buffer_dispatch(*push_buffers, node_callbacks);
    // Camera director (0x000AA250) and rig (0x000781F0) tunables are .data words read from the image.
    std::memcpy(rig_tunables.rig.data(), data(0x004274B8, 0x88).data(), 0x88);
    std::memcpy(rig_tunables.view.data(), data(0x00434008, 0x2C).data(), 0x2C);
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
        address_zero, clock, statsheet.data(), actor_tags.data(), *damage_tables, *footsteps, surface_names,
        object_tags, image.string(0x0031C354),
        {image.string(0x00315A98), image.string(0x00315AE0), image.string(0x00315AFC), image.string(0x00315AF4)},
        std::span<const char* const, 0x55>(callout_names.data(), 0x55),
        std::span<const char* const, 0x41>(flag_names.data(), 0x41), shock_pass, shock_fail, data(0x00217DE8, 0x40),
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
        }, body_slide_contacts, {reinterpret_cast<const float*>(data(0x00217EC0, 0x20).data()), 8},
        [this](u32 address) -> std::span<const u8>
        {
            return scene.resolve_tail(address);
        }});
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
            return actors->actors.at(scene.resolve_address(address, 0).data());
        },
        [this](u32 address) -> std::span<const u8>
        {
            if (const auto found = target_views.find(address); found != target_views.end())
            {
                return found->second->bytes;
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
        {reinterpret_cast<const float*>(data(0x00217EC0, 0x20).data()), 8},
        [this](u32 address) -> RuntimeBodyAttachedActor&
        {
            return players->player_at(address).self;
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
        targets, target_views, target_eligibility, target_addresses, target_queries, query, view_invert,
        data(0x002FFF24, 0x10), read_f32(data(0x004275B8, 4), 0), attachment_notice});
    active_services = this;
}

SceneActorServices::State::~State()
{

    // The dialogue's cutscene still holds rows over the scene's cutscene actors: close it before they go.
    if (game.script_dialogue == script_dialogue.get() && script_dialogue != nullptr)
    {
        unbind_scene_dialogue(game);
    }
    RuntimeDoorRelease door_release{*doors, door_camera_registered, door_paused, active_strategies,
        door_camera_hold, g_flags_ea9c};
    release_runtime_doors(door_release);
    for (auto* owned : {&cutscene_actors, &cameras, &callout_objects})
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
    engine_heap_free(input_activation.entries);
    scene.released = nullptr;
    if (active_services == this)
    {
        active_services = nullptr;
    }
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
    for (void* block : {static_cast<void*>(door_pool), static_cast<void*>(text_registry)})
    {
        if (block != nullptr)
        {
            engine_heap_free(block);
        }
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
    return found->second;
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
        row->target = std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, row->bytes, address, actor,
            actor_bytes, *query_view, occupancy, record});
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
    std::erase_if(mind_target_rows, [&live](const auto& row)
    {
        return !live.contains(row.first);
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

FactoryObject* SceneActorServices::State::object_of(const u8* bytes)
{
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
    if (type == 3 || type == 0x91)
    {
        scene.controls->message(object, message);
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
        slot = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{actor, 0, &view.body_node,
            [this, &actor, bytes = view.bytes.data()](void* payload)
            {
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

// Base actor vtable +0x10 (0x000420D0) over the scene's frame context.
void SceneActorServices::State::update_base_actor(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    update_actor_frame(view.frame, *frame);
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

void SceneActorServices::State::attach_player_messages(FactoryObject& object, NativePlayerInstance& player)
{
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

// Child vtable +0xC of a player's strategy, mind or body.
void SceneActorServices::State::child_message(FactoryObject& child, void* message)
{
    const std::span<const u32> words{static_cast<const u32*>(message), 0x42};
    switch (child.type)
    {
    case 0x4F:
        players->strategy_message(child, words);
        return;
    case 0x46:
        players->imp_fire_message(child, words);
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
        // Mind +0x10 is a DAT_00545988 target record.
        floor_hint = target_views.at(read_u32(mind_view, 0x10))->bytes.data();
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
        [](FactoryObject& action, u32) -> u16
        {
            switch (action.type)
            {
            case 0x87:
            case 0x8E:
                return runtime_body_action_next_key(action);
            case 0x5C:
                // The 0x5C class vtable (0x00505500) has six slots; +0x1C reads the next table's +4, 0x000CFFD0.
                return u16(accept_runtime_object_construction(action, {}) ? 1 : 0);
            default:
                unbound("body action vtable +0x1C of factory type " + std::to_string(action.type));
            }
        },
        path_search, floor_hint, collision_view, player.actor_bytes, player.body_bits, player.collision,
        player.body_selection,
        [](Actor&, RuntimeSegmentContactActor&, const Vec3&, GhoulySpawnPool*, const RuntimeSegmentContactRecord&,
            const Vec3&, u16) -> u32
        {
            unbound("player body contact callback 0x0001F8C0");
        },
        [](entity_detail::SegmentProbe&) -> RuntimeSegmentContactActor&
        {
            unbound("body contact walk actor view");
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
            return scene.resolve_address(address, size);
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

// Message slot 0x00043E50 with this background's part state (actor +0x940..+0x958) and the scene's tables.
void SceneActorServices::State::background_message(NativeBackgroundInstance& background, std::span<u32> message)
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
    const std::function<void(Actor&, const void*)> reply = [this](Actor& receiver, const void* words)
    {
        actor_message(receiver, words);
    };
    BackgroundMessageContext context{base.base, base_message, parts, address_zero, *score, resources.text, part_text,
        wide_scratch.data(), broken_parts, reinterpret_cast<const char*>(data(0x00421E58, 0x2D).data()),
        entity_detail::g_world.anchor, *doors, actor_at, views.resolve, reply};
    handle_background_message(actor, message, context);
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

void SceneActorServices::State::update_actor(u32 address)
{
    const u8* bytes = scene.resolve_address(address, 0).data();
    auto* object = object_of(bytes);
    auto& view = actors->view(*object);
    const auto found = actor_updates.find(&view.bat.actor);
    if (found == actor_updates.end())
    {
        throw Error("actor update slot is not translated for factory type " + std::to_string(object->type));
    }
    found->second(view.bat.actor);
}

// Scene startup ahead of the markers (0x0011CBD0): the door registration pool, the requested background
// (0x0011C8B0) and its activation (0x00123070), then the navigation build (0x00111820).
void SceneActorServices::State::start_background()
{
    // DAT_004EE2B8: 0x560 bytes, stride 0x154 and growth 4, so four rows after the header.
    door_pool = static_cast<entity_detail::NavigationCellRegistration*>(
        engine_heap_allocate(i32(sizeof(entity_detail::NavigationCellRegistration) + 4 * 0x154)));
    u8* rows = reinterpret_cast<u8*>(door_pool + 1);
    door_pool->begin = rows;
    door_pool->end = rows;
    door_pool->capacity = rows + 4 * 0x154;
    door_pool->stride = 0x154;
    door_pool->growth = 4;
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
}

SceneActorServices::SceneActorServices(SceneFactoryRuntime& scene, Game& game)
    : state_(std::make_unique<State>(scene, game))
{
}

SceneActorServices::~SceneActorServices() = default;

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
            // Type 4 (vtable 0x004E1644) +0x14 is the background draw 0x00042C10. It is not translated; the
            // legacy host world draw presents the background in its place.
            if (type == 4)
            {
                if (host_world_draw == nullptr)
                {
                    unbound("background draw 0x00042C10 outside a presented view");
                }
                // The host world draw presents the whole hall at once, so it runs for the first pass
                // (0x00121900's pass 0); passes 1 and 2 have nothing left for it to present.
                if (pass == 0)
                {
                    (*host_world_draw)();
                }
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
            // Type 0x1A (vtable 0x00504DD8) +0x14 is 0x0006EF20, a jump to the actor draw 0x0003C9E0; types 2, 0x1F
            // and 0x23 keep the base slot 0x0003C9E0 (getters 0x0003C080, 0x0006DBD0 and 0x00069620).
            if (type != 0x1A && type != 2 && type != 0x1F && type != 0x23)
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
        [&player](void*, i32)
        {
            // States 0x87 (entityStateStandAlert, getter 0x000A2530) and 0x8E (entityStateWalkAlert, getter
            // 0x000A55F0) have the empty slot 0x001628B0 at +0x18.
            if (player.second_action.type != 0x87 && player.second_action.type != 0x8E)
            {
                unbound("body +0x744 object vtable +0x18 draw for factory type "
                    + std::to_string(player.second_action.type));
            }
        }};
    draw_runtime_player({object.bytes.get(), object.size}, kind, draw);
}

// Actor vtable +0x18 by class. Types 2, 5, 0x1A, 0x1F and 0x23 keep the base actor slot 0x0003C830 (getters
// 0x0003C080, 0x00027F60, 0x0006EE20, 0x0006DBD0 and 0x00069620); the FX emitter (type 0xD, getter 0x00109DD0)
// uses 0x00109E30.
u32 SceneActorServices::State::actor_visible(Actor& actor, i32 pass, i32 view)
{
    FactoryObject& object = object_of_actor(actor);
    if (object.type == 0xD)
    {
        return classes->emitter_visible(object, u32(pass), u32(view)) ? 1u : 0u;
    }
    if (object.type != 2 && object.type != 5 && object.type != 0x1A && object.type != 0x1F && object.type != 0x23)
    {
        unbound("actor vtable +0x18 visibility for factory type " + std::to_string(object.type));
    }
    if (display_view == nullptr)
    {
        throw Error("actor visibility has no DAT_005077A8 display view");
    }
    ActorMatrixSetup* matrices = nullptr;
    if (i32(read_u32({object.bytes.get(), object.size}, 0x28C)) > 0)
    {
        unbound("actor +0x288 matrix setup for view visibility");
    }
    RuntimeActorViewVisibility visibility{actor, {object.bytes.get(), object.size}, matrices, *display_view};
    return runtime_actor_in_view(visibility, pass, view);
}

// 0x0011C240's actor bins for one view: 0x00121730 and 0x00121880 over DAT_00511AA8[view], 0x00121900 (passes 0
// and 1 with its world helpers), then 0x00121D40 (pass 2) over DAT_00511B1C and that bin. The host world draw
// stands in for the background's draw.
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

void SceneActorServices::State::draw_view_actors(i32 view)
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
            return actor_visible(actor, pass, slot);
        },
        [this](Actor& actor, i32 slot, i32 pass)
        {
            actor_draw(actor, slot, pass);
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
            if (ambient_point_draw != 0 && (ambient_points_enabled != 0 || g_flag_2104 != 0))
            {
                unbound("ambient point draw 0x001237E0");
            }
        },
        [this, first_background_model](u32 layer)
        {
            if (first_background_model().data() != nullptr && portal_list != nullptr)
            {
                unbound("portal draw 0x00115A80 layer " + std::to_string(layer));
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
    auto& bin = entity_detail::g_world.bins[size_t(view)];
    sort_runtime_view_bin(bin, context);
    classify_runtime_view_bin(bin, 0, view, context);
    auto& drawn = draw_runtime_view_passes(entity_detail::g_world.body_a, bin, view, 0, passes);
    draw_runtime_view_actors(entity_detail::g_world.body_a, &drawn, view, 0, context);
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
    const auto callback = [](u32 address, const char* slot) -> std::function<void(Actor&, i32, void*)>
    {
        if (address == 0)
        {
            return {};
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

// A bound texture is a frame record of a texture header: header + header size (28) + frame * 0x14
// (0x00053550). Headers sit at the start of texture resources and inside fonts (one per glyph); the
// registry's loaded rows own those bytes, and the header's offsets index that row's GPU data. Model
// textures are relocated: their records sit in the scene resource memory, and each record's data is
// the address its relocation registered.
const RgbaImage& SceneActorServices::State::texture_image(const u8* record)
{
    const auto cached = texture_images.find(record);
    if (cached != texture_images.end())
    {
        return *cached->second;
    }
    const u32 address = resources.memory.find_address(record);
    const auto registered = resources.d3d.resources.find(address);
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

void SceneActorServices::present_views(const std::function<void()>& draw_world)
{
    auto& state = *state_;
    const SceneD3DPresentContext present{state.resources.memory, state.resources.d3d, state.d3d_device.vertex_shaders,
        [&state](const u8* record) -> const RgbaImage&
        {
            return state.texture_image(record);
        }};
    bool presented = false;
    present_runtime_display_views([&state, &draw_world, &present, &presented](i32 slot)
    {
        presented = true;
        // The D3D11 renderer takes the slot's display camera for the host world draw; the display field
        // of view is in degrees there.
        const auto& view = *entity_detail::g_display_view;
        gfx_set_camera(view.position, view.rotation, view.field_of_view * (3.14159265f / 180.f));
        state.d3d_device.commands.clear();
        // 0x0011C240 for this view: the bin sort (0x00121730) and visibility (0x00121880), then the +0x934
        // owners and visible bin actors (0x00121D40). The background's +0x14 is the host world draw.
        state.host_world_draw = &draw_world;
        try
        {
            state.draw_view_actors(slot);
        }
        catch (...)
        {
            state.host_world_draw = nullptr;
            throw;
        }
        state.host_world_draw = nullptr;
        draw_scene_dialogue(state.game);
        auto commands = state.d3d_device.take_commands();
        present_scene_d3d_commands(commands, present);
        // Recycle the recording boundary's command capacity after synchronous presentation.
        commands.clear();
        state.d3d_device.commands = std::move(commands);
    }, *state.display_setup);
    // Legacy front-end owners still supply their own camera and draws. Starting the native
    // actor factory does not mean that an owner has registered a native display camera.
    if (!presented)
    {
        if (state.game.camera != nullptr)
        {
            const auto& camera = *state.game.camera;
            gfx_set_camera(camera.pose_position, camera.pose_quat, camera.pose_fov);
        }
        draw_world();
    }
}

void SceneActorServices::start_background()
{
    state_->start_background();
}

void SceneActorServices::start_scene(const std::function<void()>& script_event)
{
    auto& state = *state_;
    start_background();
    state.create_scene_objects(); // 0x0011C980
    start_markers();
    // 0x00115FF0: the pending scene controls join the active and update lists.
    const std::function<RuntimeActorLink&(RuntimeActorLink&)> update_link = [&state](RuntimeActorLink& link)
        -> RuntimeActorLink&
    {
        return state.scene.controls->instance_of(link).update;
    };
    const std::function<void(RuntimeActorLink&, std::span<const u32>)> control_message =
        [&state](RuntimeActorLink& link, std::span<const u32> message)
    {
        auto& control = state.scene.controls->instance_of_update(link);
        message_runtime_scene_control(control, message, g_scene_controls);
    };
    const std::function<void(RuntimeActorLink&)> control_update = [](RuntimeActorLink&)
    {
        throw Error("scene control update 0x001160F0 callback is not bound");
    };
    RuntimeActorLink*& pending = g_scene_controls.pending.next; // DAT_0050E440 itself.
    RuntimeSceneControlUpdates controls{pending, g_scene_controls.active, g_scene_controls.updates, update_link,
        control_message, control_update};
    activate_runtime_scene_controls(controls);
    activate_actors();
    script_event();
    send_scene_start_notice();
    notify_runtime_scene_controls(controls); // 0x00116090
    send_scene_start();
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

void SceneActorServices::update_audio()
{
    auto& state = *state_;
    dispatch_native_audio_notifications(state.voices.banks); // 0x0015B780
    tick_audio_event_voices(state.voices); // 0x000396F0
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
    for (size_t kind = 0; kind < handlers.handlers.size(); ++kind)
    {
        if (handlers.handlers[kind])
        {
            handlers.handlers[kind] = [kind, handler = handlers.handlers[kind]](u8* record)
            {
                try
                {
                    handler(record);
                }
                catch (const std::exception& error)
                {
                    u16 id;
                    std::memcpy(&id, record + 6, 2);
                    throw Error("marker kind " + std::to_string(kind) + " id " + std::to_string(id) + ": "
                        + error.what());
                }
            };
        }
    }
    start_runtime_scene_markers(state.markers, randomizer, handlers);
}

bool SceneActorServices::activation_pending() const
{
    return state_->live_actors.next != nullptr;
}

std::span<const u32> SceneActorServices::active_actor_addresses() const
{
    return state_->scene_nodes;
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

size_t SceneActorServices::player_total() const
{
    return state_->players->player_total();
}
}
