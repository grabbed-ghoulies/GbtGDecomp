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
[[noreturn]] void unbound(const char* what)
{
    throw Error(std::string("scene actor service is not bound: ") + what);
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
    void refresh_shape_poses(FactoryObject& object);
    void attach_player_messages(FactoryObject& object, NativePlayerInstance& player);
    void child_message(FactoryObject& child, void* message);
    void activate_actors();
    void latch_message(entity_detail::LatchNode& node, const Message& message);
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
    footsteps = std::make_unique<FootstepSoundNames>(FootstepSoundNames{data(0x00314FBC, 0x20), surface_names});
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
    for (auto* owned : {&scene.objects, &cameras, &callout_objects})
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
    const u16 type = object.type;
    if (type == 0xD || type == 0x1F || type == 0x23)
    {
        classes->message(object, message);
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
            []()
            {
                unbound("camera actor update (actor vtable +0x10)");
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
    RuntimeSceneActivation activation{live_actors, latches, scene_nodes, scene_controls,
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
    activate_runtime_scene_actors(activation);
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
