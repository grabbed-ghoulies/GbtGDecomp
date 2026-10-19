#pragma once
#include "actor_factory_owner.h"
#include "body_attachment_cleanup.h"
#include "body_construction.h"
#include "body_health.h"
#include "player_body_motion.h"
#include "player_construction.h"
#include "player_destruction.h"
#include "player_mind_owner.h"
#include "player_strategy.h"
#include "imp_fire_strategy.h"
#include "skeleton_strategy.h"
#include "butler_strategy.h"
#include "camera_actor_classes.h"

namespace gbtg
{
// Native views of the player actor's model (actor +0x204) that the body constructor reads.
struct NativePlayerModelViews
{
    const RuntimeBodyModelNodes* nodes = nullptr;
    const u8* channels = nullptr;
};

// Scene state the player classes share. Every member outlives the owner.
struct NativePlayerSceneBindings
{
    // Engine state.
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    const RuntimeInputReleaseContext& input_release;
    RuntimeInputActivationContext& input_activation;
    const u32& runtime_flags; // DAT_0050EA9C.
    const u32& story_flags; // DAT_0050EA98.
    u32& player_count; // DAT_00510578.
    u32& active_strategies; // DAT_00510CEC.
    std::span<const u8> address_zero;
    // Retail address of a native object (actor or child instance bytes); the scene owns the mapping.
    std::function<u32(const void*)> address_of;
    // Player construction (0x00028830).
    const char* const* actor_tags; // DAT_0042A854.
    const char* const* object_tags; // DAT_0042A850, includes objTag_Null for 0x00133A50.
    const u8* tag_rows; // DAT_00545AC4, stride 0x87.
    std::span<const u8> tags; // DAT_00545AC4 as a span.
    const char* const* weapon_names; // DAT_00434104.
    const char* const* class_names; // DAT_004340EC.
    u32 initial_channel_word; // DAT_00338EE0.
    const RuntimeShaderMatrixBoundaries& matrix_boundaries;
    const std::unordered_map<const u8*, RuntimePlayerAttachmentActor*>& attachment_objects;
    const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*>& attachment_actors;
    // Body construction (0x00024C50 and 0x00073450).
    std::span<const u16> scale_exclusions; // DAT_0050E1B4.
    i32 scale_exclusion_count;
    std::span<const u8> animation_classes; // DAT_0050E170.
    i32 animation_class_count;
    std::span<const u8> friction_curve; // DAT_00433D68.
    std::span<const u8> health_curve; // DAT_00433CA8.
    std::span<const RuntimeBodyLoadedResourceSize> loaded_sizes;
    const std::unordered_map<const u8*, const RuntimeScriptLookupTable*>& action_tables;
    const std::unordered_map<const u8*, const u8*>& animation_sequences;
    std::span<RuntimeBodyResourceCacheEntry> animation_cache; // DAT_004DD554.
    i32& animation_cache_count; // DAT_004DD550.
    const std::unordered_map<u32, const char*>& action_names;
    std::span<u8*> timed_attachments; // DAT_00506930.
    std::function<NativePlayerModelViews(const Actor&)> model_views;
    // Mind construction (0x00029F20 and 0x0007EF10).
    const RuntimeMindClassSettings& mind_settings;
    RuntimePlayerIntroSound& intro_sound;
    RuntimePlayerCameraContext& camera;
    std::vector<FactoryObject>& cameras;
    const u8& scene_flags; // **(DAT_00506C74+8).
    const i32& health; // DAT_00430D10.
    const i32& chapter_mode; // DAT_00430D0C.
    const char* scene_name; // DAT_00510E58.
    // Strategy construction (0x0002B740) and leaving (0x000BF410).
    const RuntimeStrategyClassRanges& strategy_ranges;
    RuntimeActorPauseGroups& pause;
    // Player destruction (0x00028A30).
    RuntimePlayerScriptOverrides& overrides;
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    // Strategy methods (vtable 0x00505FA0) and their base strategy, mind goal and event contexts.
    Game& game;
    const RuntimeActorClock& clock;
    const RuntimePlayerStrategyResume& resume;
    const RuntimeScriptActorQueryContext& query;
    std::function<RuntimeStrategyMessageActor*(u32)> message_actor;
    std::function<RuntimeMindTarget*(u32)> target_at;
    RuntimeMindTarget*& targets; // DAT_00545988.
    const void* const& script_records; // DAT_0051108C.
    std::span<const u8> mind_descriptors; // DAT_003398C8.
    std::span<const u8> mind_request_sizes; // DAT_00217F18.
    const char* const* mind_goal_names; // DAT_003397BC.
    std::string& goal_scratch; // DAT_00508EB8.
    // Decoded view of a mind's goal table (mind +0x60); the scene owns the decoded rows.
    std::function<const RuntimeMindGoalTable*(std::span<const u8>)> goal_table;
    ActorEffectStreamContext& effects;
    ModelNodeFlagCache& node_cache; // DAT_005078D8.
    const AnimationMatrix& reset_matrix;
    const SkeletonDispatchContext* skeleton;
    std::vector<FactoryObject>& callout_objects;
    const std::unordered_map<const u8*, const u8*>& linked_records;
    RuntimeBrewMeter& brew;
    RuntimePlayerHealthDisplay& health_display;
    const RuntimeDialogueInputState& input_state;
    std::span<u8> keyboard; // DAT_00507C98.
    AudioEventVoices& voices;
    u32& brews_taken; // DAT_00506C88.
    u32& brew_accepted; // DAT_00430D98.
    const i32& prompt_lock; // DAT_004E1C34.
    const u32& prompt_owner; // DAT_004EE1EC.
    const i32& difficulty; // DAT_005064D8.
    const u8& unlock_flags; // **(DAT_00506C70+8).
    const i32& brews_owned; // DAT_00506C8C.
    const i32& brews_required; // DAT_00430D9C.
    u8& story_byte; // Byte +4 of **(DAT_00506C6C+8).
    u32& intro_pending; // DAT_00545B38.
    std::span<char> intro_name; // DAT_00545B44.
    const entity_detail::ListHead& cases; // DAT_00511B3C.
    std::function<const EntityBitBlock*(const entity_detail::ListHead*)> case_body_bits;
    // Imp fire strategy (type 0x46, vtable 0x00505EA4) and its shared target queue and checks.
    std::span<ActorAttachmentSlot, 20> target_slots; // DAT_0050E284.
    std::span<const u32> queue_limits; // DAT_0050E570.
    std::span<const u32> class_queue_limits; // DAT_0050E660.
    std::function<const RuntimeMindTarget*(const ActorLiveAttachment*)> target_of; // Slot owner record view.
    std::function<std::span<const u8>(const void*)> actor_bytes_of; // Strategy link +8 actor bytes.
    const RuntimeActiveActorLink* const& active_actors; // DAT_00511B48.
    RuntimeTutorialNotices& notices;
    float& occupancy_initial; // Retained caller scratch read by 0x0013AA50 flag 0x10.
    std::span<const u8> imp_fire_parameters; // DAT_004287BC.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40 under scene ownership.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    std::function<RuntimeStrategyBodyView(u32)> body_at;
    std::function<RuntimeStrategyBodyView(const Actor&)> body_of; // Actor +0x95C.
    std::function<const RuntimeBodyModelNodes*(const Actor&)> model_of; // Actor +0x204.
    const i32& attachment_limit; // DAT_00428684.
    // Butler strategy (type 0x3B, vtable 0x00505D70).
    std::function<u32(const Actor&)> actor_address; // Retail address of an actor.
    std::function<std::span<const u8>(const Actor&)> actor_bytes; // An actor's bytes (+0x8F8 target record).
    std::function<std::span<const u8>(u32)> actor_bytes_at; // Actor bytes by retail address.
    std::function<const char*(u32)> string_at; // A NUL-terminated string a parameter block names.
    // Skeleton strategy (type 0x51, vtable 0x00505FD8).
    std::function<Actor*(u32)> actor_at; // A scene actor by retail address.
    const float& skeleton_fetch_delay; // DAT_004288AC.
};

// Native state and contexts of one player (type 5) and its body, mind and strategy children.
// Members are declared in dependency order; references point only at earlier members or bindings.
struct NativePlayerInstance
{
    NativePlayerInstance(NativePlayerSceneBindings& scene, NativeActorFactoryInstance& actor_instance,
        FactoryObject& actor_object);
    NativePlayerInstance(const NativePlayerInstance&) = delete;
    NativePlayerInstance& operator=(const NativePlayerInstance&) = delete;
    void refresh_body_views(); // After body construction or destruction.

    NativePlayerSceneBindings& scene;
    NativeActorFactoryInstance& actor_instance;
    Actor& actor;
    std::span<u8> actor_bytes;
    u32 actor_address;
    // Player actor fields.
    // Actual actor vtable +0xC (player 0x00028F60), attached with the player methods.
    std::function<void(std::span<const u32>)> actor_message;
    RuntimeBodyAttachedActor self;
    std::span<const u8> attribute_resource; // Actor +0x964.
    GhoulyController controller;
    RuntimePlayerSpawnController spawn{nullptr, 0, controller};
    FactoryObject body_object; // Actor +0x95C.
    FactoryObject mind_object; // Actor +0x958.
    FactoryObject strategy_object; // Actor +0x954.
    FactoryObject created_attachment;
    std::span<u8> input; // Actor +0x960.
    RuntimeActorMatrixState matrices{};
    std::array<entity_detail::ListHead, 4> links{}; // Actor +0x934/+0x93C/+0x944/+0x94C.
    entity_detail::NavigationCellRegistration* navigation = nullptr; // Actor +0xCE8.
    RuntimePlayerGoalRing* ring = nullptr; // Actor +0x980.
    RuntimePlayerChildContext child;
    RuntimePlayerAttributes attributes;
    // Body (types 6, 0x27, 0x2A) state.
    std::span<u8> body;
    u32 body_actor_address = 0; // Body +8.
    RuntimeBodyAttachedActor* attached = nullptr; // Body +0x48.
    RuntimeBodyAttachedActor* secondary = nullptr; // Body +0x4C.
    RuntimeBodyAnimationReceiver* first_receiver = nullptr; // Body +0x10.
    RuntimeBodyAnimationReceiver* second_receiver = nullptr; // Body +0x14.
    RuntimeBodyBitStorage* body_flags = nullptr; // Body +0x15C.
    EntityBitBlock body_bit_block{};
    EntityBitBlock* body_bits = nullptr; // Body +0x15C as the plain bit view.
    ActorEulerOut body_output{}; // Actor +0x95C body pose view.
    std::span<const u8> animation_resource; // Body +0x5C.
    RuntimeBodyAnimationGroup* animation_groups = nullptr; // Body +0x64.
    u8 animation_group_count = 0; // Body +0x68.
    RuntimeBodyBitStorage* animation_bits = nullptr; // Body +0x60.
    std::array<std::span<const u8>, 10> body_resources{};
    std::array<u32, 10> body_resource_counts{};
    RuntimeBodyResourceHash* body_hashes = nullptr;
    u32 body_hash_count = 0;
    RuntimeBodyCollisionState* collision = nullptr; // Body +0x2C8.
    std::span<const u8> body_attributes; // Body +0x268.
    RuntimeBodyShapeStorage shapes{};
    std::array<std::span<const u8>, 4> health_textures{};
    FactoryObject first_action; // Body +0x314.
    FactoryObject second_action; // Body +0x744.
    FactoryObject body_action_object; // Body +0x760.
    std::span<const u8> body_actions;
    const u8* action_row = nullptr; // Body +0x740.
    const RuntimeScriptLookupTable* action_table = nullptr; // Body +0x73C.
    RuntimeBodyMotionController* motion_controller = nullptr; // Actor +0x54.
    RuntimeBodyMotionController* motion_secondary = nullptr; // Actor +0x5C.
    u32 motion_selector = 0; // Actor +0x60.
    std::array<u8*, 6> joint_records{}; // Body +0x8AC.
    NativePlayerModelViews model;
    RuntimeBodyAnimationSetup body_animation;
    RuntimeBodyActorAttachment body_attachment;
    RuntimeBodyAttachmentLists body_lists;
    RuntimeBodyHealth body_health;
    RuntimeParentBodyCleanup body_cleanup;
    RuntimeBodyActionSelection body_selection;
    RuntimeBodyAction body_action;
    RuntimeParentBodyConstruction body_construction;
    RuntimePlayerBodyMotion body_motion;
    // Mind (types 7, 0x34) state.
    std::span<u8> mind;
    std::span<const u8> mind_script_table; // Mind +0x60.
    RuntimePlayerMindRequest request_anchor{}; // Mind +0x54; next is mind +0x58.
    RuntimePlayerMindRequests* mind_requests = nullptr; // Actor +0x958's request view.
    RuntimePlayerMindRequests requests;
    LiveScript event_anchor; // Mind +0x24; next is mind +0x28.
    RuntimeMindStorage* mind_storage = nullptr; // Mind +0x1C.
    RuntimePlayerCameraSource camera_source;
    RuntimePlayerMindSetup mind_setup;
    // Shared cinema mind (type 0x35) lifecycle, created with the mind.
    std::span<const u8> cinema_resource; // Mind +0x84.
    std::unique_ptr<RuntimeCinemaMindLifecycle> cinema_lifecycle;
    // Strategy (types 8, 0x4F) state.
    std::span<u8> strategy;
    RuntimeStrategyLink strategy_link{}; // Strategy +0x20.
    RuntimePlayerStrategySetup strategy_setup;
    // Player constructor and destructor.
    RuntimeBodyAttachedActor* mind_action = nullptr; // Mind +0x18.
    bool mind_present = false; // Actor +0x958 is set.
    std::function<u16()> body_action_key; // Actual body vtable +0x20, attached with the body methods.
    RuntimePlayerAttachment attachment;
    RuntimePlayerAttachmentCreation attachment_creation;
    RuntimePlayerConstruction construction;
    RuntimePlayerGoalCleanup goal_cleanup;
    RuntimePlayerDestruction destruction;
    // Strategy methods: base strategy entry, goals and messages, and the player strategy flow.
    const u8* carried = nullptr; // Body +0x268 object.
    const OccupancyRecord* allowed = nullptr; // Actor +0xCA4.
    const OccupancyRecord* room = nullptr; // Actor +0xCA0.
    const OccupancyRecord* current = nullptr; // Actor +0xCA8.
    const OccupancyRecord* goal_volume = nullptr; // Actor +0x98C.
    RuntimeBodyAnimationReceiver receiver; // Actor's own animation receiver for 0x0003AE00.
    std::function<u16(u32)> body_slot_key; // Actual body vtable slot by byte offset, with the body methods.
    std::array<RuntimeDialogueInputDevice, 4> input_children{};
    std::array<RuntimeInputAxisWriteDevice, 4> axis_children{};
    RuntimeDialogueInputDevice input_device{};
    RuntimeInputAxisWriteDevice axis_device{};
    RuntimeMindEvents mind_events;
    RuntimeMindGoalContext mind_goals;
    RuntimePlayerStrategyEntry strategy_entry;
    RuntimePlayerAttachmentActor pose;
    RuntimePlayerStrategyGoals strategy_goals;
    RuntimeBaseStrategyMessages strategy_messages;
    ActorAttachmentEventContext callout;
    RuntimePlayerStrategyFlow flow;
    // Imp fire strategy (type 0x46) methods and their shared target queue and checks.
    u16 strategy_type = 0; // Factory type of the strategy child: 0x4F player, 0x46 imp fire, 0x39 director or 0x3B butler.
    EntityCamNode camera_view{}; // Camera body (type 0x28) +0x894.
    RuntimeStrategyTargetChecks target_checks;
    RuntimeStrategyTargetQueue target_queue;
    RuntimeImpFireStrategy imp_fire;
    RuntimeButlerStrategy butler; // Type 0x3B.
    RuntimeStrategyActorViews strategy_views; // Other actors for the 0x00139xxx reactions.
    RuntimeSkeletonStrategy skeleton; // Type 0x51.
    void refresh_input_views(); // After the player constructor allocates actor +0x960.
    void refresh_views(); // Body, input, mind and strategy value views before a method runs.
};

struct NativePlayerFactoryBindings
{
    NativeActorFactoryOwner& actors;
    NativePlayerSceneBindings scene;
};

// Installs the player (type 5), body (6, 0x27, 0x28, 0x2A), mind (7, 0x34, 0x36) and strategy (8, 0x4F, 0x46,
// 0x39) classes
// over the actor owner's types 1 and 2, and keeps one NativePlayerInstance per player actor.
class NativePlayerFactoryOwner
{
public:
    explicit NativePlayerFactoryOwner(NativePlayerFactoryBindings bindings);
    ~NativePlayerFactoryOwner();
    NativePlayerFactoryOwner(const NativePlayerFactoryOwner&) = delete;
    NativePlayerFactoryOwner& operator=(const NativePlayerFactoryOwner&) = delete;
    void install(std::span<ObjectFactoryType> types);
    NativePlayerInstance& construct_player(FactoryObject& object);
    NativePlayerInstance& player(const FactoryObject& object);
    NativePlayerInstance& player_at(u32 actor_address);
    NativePlayerInstance* find_player_at(u32 actor_address); // Null when no player has that actor address.
    NativePlayerInstance* find_player(const FactoryObject& child) const; // Body, mind or strategy object.
    NativePlayerInstance* find_child_owner(const u8* child) const; // By the child's instance bytes.
    void release_player(const FactoryObject& object);
    void release_player(const u8* bytes);
    void bind_child(const FactoryObject& child, NativePlayerInstance& player);
    void release_child(const FactoryObject& child);
    size_t player_total() const;
    std::vector<NativePlayerInstance*> instances() const;
    size_t child_count() const;
    void strategy_message(FactoryObject& strategy, std::span<const u32> message); // Vtable +0xC, Xbox 0x000BEE10.
    void strategy_update(FactoryObject& strategy); // Vtable +0x10, Xbox 0x000BEF40.
    void imp_fire_message(FactoryObject& strategy, std::span<const u32> message); // Vtable +0xC, Xbox 0x000B46B0.
    void imp_fire_update(FactoryObject& strategy); // Vtable +0x10, Xbox 0x000B45D0.
    void butler_message(FactoryObject& strategy, std::span<const u32> message); // Vtable +0xC, Xbox 0x000AB170.
    void butler_update(FactoryObject& strategy); // Vtable +0x10, Xbox 0x000AB0C0.
    void skeleton_message(FactoryObject& strategy, std::span<const u32> message); // Vtable +0xC, Xbox 0x000BFE10.
    void skeleton_update(FactoryObject& strategy); // Vtable +0x10, Xbox 0x000BFAC0.
    NativePlayerFactoryBindings bindings;
    NativePlayerMindOwner minds;
    // Retail keeps the type 5 vtable through the whole destructor chain, so the base actor destructor still
    // messages the player. A scene that defers release calls release_player after its base destructor.
    bool defer_release = false;

private:
    std::unordered_map<const u8*, std::unique_ptr<NativePlayerInstance>> players_;
    std::unordered_map<u32, NativePlayerInstance*> addresses_;
    std::unordered_map<const u8*, NativePlayerInstance*> children_;
};
}
