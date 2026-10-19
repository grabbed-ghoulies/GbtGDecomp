#pragma once
#include "../entities/player_factory_owner.h"
#include "../entities/player_body_messages.h"
#include "../entities/body_damage_dispatch.h"
#include "../entities/body_effect_commands.h"
#include "../audio/body_sound_commands.h"
#include "../camera/camera_body_slots.h"
#include "../camera/camera_mind_rig.h"
#include "../entities/body_move_action_slots.h"
#include "../entities/player_body_collision.h"
#include "../entities/body_hit_state.h"
#include "../entities/body_dizzy_state.h"
#include "../entities/body_die_state.h"
#include "../entities/body_anim_state.h"
#include "../entities/body_knockdown_state.h"
#include "../entities/body_pickup_state.h"
#include "../entities/body_player_death_state.h"
#include "../entities/body_grabbed_state.h"
#include <optional>

namespace gbtg
{
struct NativeActorFactoryInstance;
// Scene state shared by every body's message graph.
struct SceneBodyServices
{
    Game& game;
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    std::vector<FactoryObject>& objects;
    RuntimeActorSoundQueues& sounds;
    ActorEffectStreamContext& effects;
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const entity_detail::ListHead& contact_players; // DAT_00511B1C, actor +0x934 nodes.
    entity_detail::ListHead& global_attachments; // DAT_00545984.
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    u32& random; // DAT_0054DD7C.
    std::span<const u8> address_zero;
    const RuntimeActorClock& clock;
    const u8* tag_rows; // DAT_00545AC4.
    const char* const* tag_names; // DAT_0042A850, from objTag_Null, as 0x00133A50 indexes it.
    const RuntimeBodyDamageTables& damage_tables;
    const FootstepSoundNames& footsteps;
    std::span<const char* const> surfaces; // DAT_004340A0, 0x12 names.
    std::span<const char* const> effect_tags; // DAT_0042A850, objTag_Null first.
    const char* effect_scene; // DAT_004DEA54 -> 0x0031C354.
    std::array<const char*, 4> effect_suffixes; // 0x00315A98, 0x00315AE0, 0x00315AFC, 0x00315AF4.
    std::span<const char* const, 0x55> callouts; // 0x00421C9C.
    std::span<const char* const, 0x41> flags; // 0x00338FB0.
    std::span<const char* const, 4> shock_pass; // 0x00023787 table: 0x003168A4, 0x00316890, 0x0031687C, 0x00316860.
    std::span<const char* const, 4> shock_fail; // 0x000237AF table: 0x00316854, 0x00316840, 0x0031682C, 0x00316810.
    std::span<const u8> amount_keys; // DAT_00217DE8..DAT_00217EB8, two key bytes per amount key (0x000238E0).
    std::span<const u16, 3> death_actions; // DAT_00217EB8.
    std::span<const u32, 2> player_blocked; // DAT_0050E764.
    std::span<const u8> player_modes; // DAT_0030F144.
    u32& player_mode; // DAT_0051248C.
    u32& player_damage; // DAT_00512200.
    u32& scene_flags; // DAT_0050EA9C.
    u32& story_flags; // DAT_0050EA98.
    u32& restart_requested; // DAT_00430D0C.
    std::string& name_scratch; // DAT_00508EB8.
    const char* empty_name; // DAT_00546208.
    float occupancy_initial;
    std::function<u32(const void*)> address_of;
    std::function<Actor*(u32)> actor_at;
    std::function<std::span<const u8>(u32)> bytes_at; // A scene actor's instance bytes.
    std::function<void(u32, std::span<const u32>)> message; // Any actor's vtable +0xC.
    std::function<const entity_detail::ListHead*(u32)> body_node; // Actor +0x934 node of a scene actor.
    std::function<RuntimeBodyAttachedActor&(u32)> source_at;
    std::function<std::span<const u8>(u32)> source_body_at;
    // Body update slots (+0x10..+0x1C).
    const RuntimeCameraRigTunables& rig_tunables; // DAT_004274B8.., DAT_00434008..
    const i32& look_profile; // DAT_00545AEC.
    const i32& motion_steps; // DAT_004312E0.
    std::span<u32> node_states; // DAT_005078D8.
    std::array<std::span<const u8>, 4> channel_keys; // DAT_00338F64, DAT_00338F88, DAT_00338EE0, DAT_00338EA4.
    float stand_input_threshold; // DAT_0032B22C.
    std::array<float, 8> movement_phases; // DAT_00339900, mutable cyclic phase curve.
    float movement_ratio_scale; // DAT_0032B4F8.
    float backward_motion_lower; // DAT_0032B3D8.
    float reverse_turn_threshold; // DAT_0032B140.
    RuntimeBodyShapePush& shape_push;
    std::function<std::span<const u8>()> door_rows;
    std::span<entity_detail::SlideRecord, 5> collision_contacts;
    std::span<const float> mode_scales; // DAT_00217EC0.
    std::function<std::span<const u8>(u32)> event_at; // Message 7 +0xC: the animation event's bytes.
    std::span<const u8> stand_tunables; // DAT_004275BC..DAT_004275CF, read by the type 0x5D stand action.
    const RuntimeCameraRigContext& camera_rig; // 0x0013CE80's tunables and actor views.
    // Trail request clock (0x0013B030, 0x0013B1B0).
    float& pause; // DAT_004312DC.
    float& frame_delta; // DAT_005096C0.
    float& time_scale; // DAT_004312C4.
    i32& pal; // DAT_005096E0.
    i32& pal_override; // DAT_005096E4.
    std::function<std::span<const u8>(u32)> target_at; // A DAT_00545988 target record's retail image.
    // A data record by address arena; `size` bounds a record in the XBE image (body +0x84 rows are 0x24).
    std::function<std::span<const u8>(u32 address, size_t size)> record_at;
    // 0x0003BE20 over an actor's +0x8C callout records with g_actor_creation_options and callback 0.
    std::function<void(u32 actor, u32 id)> actor_callouts;
    std::span<const u8> push_keys; // DAT_00217DB0..DAT_00217EB8, read by 0x000239C0 and 0x00023A50.
    std::function<RuntimeBodyDamageEntry&(u32 actor)> damage_at; // A player's body damage entry, by actor address.
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<NativeActorFactoryInstance&(u32 address)> actor_view; // A scene actor's native views.
    std::function<RuntimeBodyStateContext(u32 body_address)> state_context; // 0x0001FEB0's views of a body.
    const u8& play_bounded; // DAT_0050EA01.
    std::array<const float*, 6> play_bounds; // DAT_0050EA7C..DAT_0050EA90.
    std::span<const u8, 2> hit_bands; // DAT_00433D08.
    std::span<const float, 2> hit_damage; // DAT_00433D0C.
    // 0x0003BE20 over an actor's +0x8C records with an explicit options pointer (may be null) and callback 0.
    std::function<void(u32 actor, u32 id, const RuntimeActorCreationOptions* options)> actor_callouts_with;
    // 0x0013FC50 over the DAT_00511A1C slots the last probe filled.
    std::function<i32(i32 count, u32 player, u32 actor)> probe_messages;
    std::function<const char*(u32 address)> image_string; // An XBE string, or null for 0.
    std::span<const u8> dizzy_input_preset{}; // DAT_00428660, 0x00109810's tracking preset for the dizzy state.
    float dizzy_input_scale = 0.f; // DAT_00428670.
    // A scene weapon's views for 0x0001B190 (its bytes, actor, animation receiver and pose), by address.
    std::function<RuntimePlayerAttachmentActor&(u32 address)> weapon_attachment_at{};
    // An attached weapon's +0x998 collision state (null when it has none), by address.
    std::function<RuntimeBodyCollisionState*&(u32 address)> weapon_collision{};
    // The player's death (type 0x69): the camera pulse (DAT_0050447C), the streams, the scene dialogue,
    // DAT_004E17C8 and the transition start 0x0012C5D0.
    RuntimeCameraPulse* camera_pulse{};
    std::function<AudioBankStreams&()> streams{};
    RuntimeDialogueActivationContext* dialogue{};
    const i32* frontend_state{};
    std::function<void()> begin_transition{};
};

// One body's message graph: the parent body handler 0x000253B0 and the derived slots of types 0x27
// (0x00074080), 0x2A (0x00075B60) and 0x28 (0x00073320). Members alias the player's native state.
class SceneBodyMessages
{
public:
    SceneBodyMessages(SceneBodyServices& services, NativePlayerInstance& player);
    RuntimeDizzyStateContext& dizzy_context(); // The type 0x64 state's views of this body.
    RuntimeDieStateContext& die_context(); // The type 0x62 state's views of this body.
    RuntimeAnimStateContext& anim_context(); // The type 0x5E state's views of this body.
    RuntimeKnockdownContext& knockdown_context(); // The type 0x7A state's views of this body.
    RuntimePickupStateContext& pickup_context(); // The type 0x7C state's views of this body.
    RuntimePlayerDeathContext& player_death_context(); // The type 0x69 state's views of this body.
    RuntimeGrabbedStateContext& grabbed_context(); // The type 0x70 state's views of this body.
    SceneBodyMessages(const SceneBodyMessages&) = delete;
    SceneBodyMessages& operator=(const SceneBodyMessages&) = delete;
    void dispatch(FactoryObject& body, std::span<u32> message);
    RuntimeBodyDamageEntry& damage_entry(); // This body's damage entry, with its views refreshed.
    void tick(FactoryObject& body, u32 input); // Vtable +0x10.
    void animate(FactoryObject& body); // Vtable +0x14.
    void settle(FactoryObject& body); // Vtable +0x18.
    u32 supported(FactoryObject& body); // Vtable +0x1C.

private:
    void action_tick(FactoryObject& action, u32 input); // Body action child vtable +0x10.
    void action_step(FactoryObject& action); // Body action child vtable +0x14.
    RuntimeBodyChannelContext channel_context();
    SceneBodyServices& services_;
    NativePlayerInstance& player_;
    const u8* event_record_ = nullptr; // Actor +0x8C.
    std::unordered_map<const u8*, RuntimeBodyMessageChild*> children_;
    RuntimeBodyMessageChild empty_child_;
    RuntimeBodyMessageChild walk_alert_child_; // 0x8E vtable +0xC, 0x000A5A40.
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> stand_children_; // 0x5D, 0x0007FE70.
    RuntimeStandActionContext stand_context(); // Type 0x5D slots over this body.
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> hit_children_; // 0x73, 0x00095C90.
    std::span<const u8> mind_target(); // Actor +0x958 -> +0x14.
    u16 body_slot_key(u32 slot); // Actual body vtable +0x20 / +0x2C / +0x30 / +0x34.
    RuntimeBodyRecoveryAction& recovery_action(); // 0x00133F50 / 0x00134060 over this body.
    std::optional<RuntimeBodyRecoveryAction> recovery_;
    RuntimeDizzyStars dizzy_stars_;
    std::optional<RuntimeDizzyStateContext> dizzy_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> dizzy_children_; // 0x64, 0x0008CEA0.
    std::optional<RuntimeDieStateContext> die_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> die_children_; // 0x62, 0x0008C4D0.
    std::optional<RuntimeAnimStateContext> anim_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> anim_children_; // 0x5E, 0x00088960.
    std::optional<RuntimeKnockdownContext> knockdown_;
    std::optional<RuntimeBodyStateContext> knockdown_shape_;
    std::unordered_map<const u8*, std::unique_ptr<RuntimeBodyMessageChild>> knockdown_children_; // 0x7A, 0x0009D0E0.
    std::optional<RuntimePickupStateContext> pickup_;
    std::optional<RuntimePlayerDeathContext> player_death_;
    std::optional<RuntimeGrabbedStateContext> grabbed_;
    std::array<u8, 0x20> shape_scratch_{};
    RuntimeBodyTimerRecipient* first_timer_ = nullptr; // Body +0x760.
    RuntimeBodyTimerRecipient* second_timer_ = nullptr; // Body +0x7F0.
    std::array<char, 128> command_name_{};
    u32 effect_flags_ = 0;
    RuntimeActorProximityNode* proximity_ = nullptr;
    std::unordered_map<const u8*, RuntimeBodyAttachedActor*> created_;
    std::unordered_map<const u8*, RuntimeBodyTimerObject*> timers_;
    FactoryObject timer_object_;
    FactoryObject hold_timer_object_; // Body +0x7F0's object.
    std::optional<RuntimeBodyTimerCreation> hold_timer_creation_;
    std::optional<RuntimeBodyHoldTimerContext> hold_timer_;
    RuntimeBodyHoldTimerContext& hold_timer(); // 0x00027BE0's views: body +0x7F0, the actor and DAT_00511AA4.
    std::array<float, 4> parameter_scratch_{};
    RuntimeDamageAnimationRows damage_rows_{0, {}}; // Body +0x268 head, refreshed with the views.
    std::vector<RuntimeDamageAnimationRow> damage_row_storage_;
    void refresh_damage_rows();
    std::span<u8> body_view_;
    std::span<const u8> hit_view_; // Body +0x84: the attackdata request row.
    EntityMaskHost mask_{};
    ActorLiveAttachment* live_attachment_ = nullptr; // Actor +0x8F8.
    RuntimeBodyActionMessages actions_;
    RuntimeBodySoundCommandContext sound_commands_;
    RuntimeBodyProjectileCreation projectiles_;
    RuntimeBodyEffectCommands effect_commands_;
    std::function<void(RuntimeBodyAttachedActor&, std::span<char, 128>)> query_name_;
    RuntimeBodyAnimationDispatch animation_;
    std::optional<RuntimeBodyCommandAttachment> attached_shapes_; // Body +0x48's views for 0x0001A290.
    RuntimeBodySpatialNotifications spatial_;
    RuntimeBodyMessageDispatch parent_;
    RuntimeBodyDeathNotice death_notice_;
    RuntimeBodyDeathDrops death_drops_;
    RuntimeBodyDeathEffects death_;
    RuntimeBodyDamageProcessor processor_;
    RuntimeBodyTimerCreation timer_creation_;
    RuntimeBodyDamageEntry damage_;
    RuntimePlayerBodyMessages messages_;
    RuntimePlayerBodyControlMessages controls_;
    // Update slot views (actor +0x50 bindings, +0x204/+0x310 models, +0x8F8 bound, +0x8FC shapes, +0x964).
    RuntimeBodyChannelBindings* channel_bindings_ = nullptr;
    RuntimeBodyChannelBindings channel_binding_view_;
    std::vector<RuntimeBodyChannelBindings::Asset> channel_assets_;
    std::vector<RuntimeBodyChannelBindings::Resource> channel_resources_;
    std::vector<RuntimeBodyChannelBindings::Row> channel_animation_rows_;
    std::vector<std::array<u32, 48>> channel_directories_;
    RuntimeModelInstance* channel_model_ = nullptr;
    RuntimeModelInstance* secondary_model_ = nullptr;
    std::span<u8> channel_rows_;
    std::span<u8> actor_view_;
    std::span<u8> bound_;
    std::array<u8, 0x68> bound_record_{}; // Retail image of the actor +0x8F8 record during the body apply.
    GhoulySpawnPool* shapes_ = nullptr;
    const u8* actor_bound_ = nullptr;
    RuntimeBodyChannelChild first_step_;
    RuntimeBodyChannelChild second_step_;
};
}
