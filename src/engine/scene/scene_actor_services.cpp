#include "scene_actor_services.h"
#include "factory_scene_runtime.h"
#include "scene_marker_handlers.h"
#include "../entities/player_factory_owner.h"
#include "../entities/scene_actor_factory_owner.h"
#include "../entities/actor_base_messages.h"
#include "../entities/actor_event_messages.h"
#include "../entities/actor_live_attachments.h"
#include "../entities/entity_heap.h"
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
#include "background_messages.h"
#include "ambient.h"
#include "../navigation/model_grid.h"
#include "../audio/bank_streams.h"
#include "../camera/cutscene_impulse.h"
#include "../cutscenes/event_assets.h"
#include "gbtg/language.h"
#include "../entities/actor_word_messages.h"
#include "../camera/camera_mind_requests.h"
#include "../camera/entity_camera.h"
#include "../entities/camera_director.h"
#include "../entities/player_update.h"
#include "../entities/player_mind_setup.h"
#include "../navigation/navigation.h"
#include "../entities/entity.h"
#include "../audio/core_cue.h"
#include "../audio/event_voices.h"
#include "../frontend/brew_meter.h"
#include "../frontend/player_health_display.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_actor_query.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

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
    u32 random = 0; // DAT_0054DD7C.
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
    std::array<u8, 4 * 0xA8> controllers{}; // DAT_005079F8.
    std::array<u8, 0x3C> keyboard{}; // DAT_00507C98.
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
    std::vector<RuntimeFxEmitterPlayer*> emitter_players;
    entity_detail::DisplayView* display_view = nullptr; // DAT_005077A8.
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
    std::array<float, 12> sampling_scratch{}; // DAT_0050D7A0.
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
    std::unordered_map<Actor*, FactoryObject*> actor_objects; // Native actor -> factory object.
    std::unordered_map<Actor*, u32> actor_object_addresses;
    std::unordered_map<Actor*, std::function<void(Actor&)>> actor_updates; // Actor vtable +0x10.
    std::unique_ptr<AudioBankStreams> audio_banks; // DAT_004DF370, DAT_004E030C.
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
    void latch_message(entity_detail::LatchNode& node, const Message& message);
    std::unique_ptr<RuntimeBackgroundConstructionServices> background_services(NativeBackgroundInstance& background,
        NativeActorFactoryInstance& view, RuntimeBackgroundAnimation*& first_animation);
    void background_message(NativeBackgroundInstance& background, std::span<u32> message);
    void start_background();
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
    word_messages.reference_at = [](u32) -> void*
    {
        unbound("actor reference words");
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
        [](u32) -> RuntimeMindTarget*
        {
            unbound("mind targets");
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
        [](const ActorLiveAttachment*) -> const RuntimeMindTarget*
        {
            unbound("strategy target records");
        },
        [](const void*) -> std::span<const u8>
        {
            unbound("strategy link actor bytes");
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
        [](u32) -> RuntimeStrategyBodyView
        {
            unbound("held body views");
        },
        [](const Actor&) -> RuntimeStrategyBodyView
        {
            unbound("target body views");
        },
        [](const Actor&) -> const RuntimeBodyModelNodes*
        {
            unbound("target model views");
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
        return instance == nullptr ? 0u : heap_address(instance, sizeof(RuntimeCutsceneInstance));
    };
    audio_banks = std::make_unique<AudioBankStreams>(AudioBankStreams{{}, {}, {}, voices});
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
        replacement_target.data(), nullptr, nullptr, movie_slots, movie_mode, {}, {}, {}, {}, engine_heap_free, {}});
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
        }});
    backgrounds->install(scene.types);
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
        {data(0x00338F64, 0x24), data(0x00338F88, 0x24), data(0x00338EE0, 0x84), data(0x00338EA4, 0x3C)}});
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
            return actors->actors.at(scene.resolve_address(address, 0).data());
        },
        [this](u32 address) -> std::span<const u8>
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            if (object == nullptr)
            {
                unbound("object view of a retail address outside the scene objects");
            }
            return {object->bytes.get(), object->size};
        }});
    active_services = this;
}

SceneActorServices::State::~State()
{
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
    for (const auto table : {part_settings, part_text, sectors.settings})
    {
        if (table.data() != nullptr)
        {
            release_animation_resource(resources.registry, table);
        }
    }
    if (door_pool != nullptr)
    {
        engine_heap_free(door_pool);
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
    for (auto* owned : {&scene.objects, &cameras, &callout_objects, &cutscene_actors})
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
    if (type == 0xD || type == 0x1F || type == 0x23)
    {
        classes->message(object, message);
        return;
    }
    if (type == 4)
    {
        backgrounds->message(object, message);
        return;
    }
    if (type == 5)
    {
        auto& player = players->player(object);
        if (!player.actor_message)
        {
            attach_player_messages(object, player);
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
    if (type == 2 || type == 0x15)
    {
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
    player.actor_message = [this, messages](std::span<const u32> words)
    {
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
    if (!mind_view.empty() && read_u32(mind_view, 0x1C) != 0)
    {
        unbound("mind +0x1C path search view for the navigation floor update");
    }
    entity_detail::NavigationPathSearch* path_search = nullptr;
    const u8* floor_hint = nullptr;
    if (!mind_view.empty() && read_u32(mind_view, 0x10) != 0)
    {
        floor_hint = scene.resolve_address(read_u32(mind_view, 0x10), 0x1A).data();
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
        input_release, g_4dea58, player.ring, address_zero, player.strategy_object, player.mind_object,
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
        [this, &player, &camera_requests](FactoryObject& mind, u32)
        {
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
        size_t(background.matrices.matrices.count) * 0x48}, background.matrices.matrices.node, sampling_scratch,
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

void SceneActorServices::start_background()
{
    state_->start_background();
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

size_t SceneActorServices::player_total() const
{
    return state_->players->player_total();
}
}
