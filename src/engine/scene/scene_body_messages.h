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

namespace gbtg
{
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
    const char* const* tag_names; // DAT_0042A854.
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
    std::span<const u8> amount_keys; // DAT_00217DE8.
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
};

// One body's message graph: the parent body handler 0x000253B0 and the derived slots of types 0x27
// (0x00074080), 0x2A (0x00075B60) and 0x28 (0x00073320). Members alias the player's native state.
class SceneBodyMessages
{
public:
    SceneBodyMessages(SceneBodyServices& services, NativePlayerInstance& player);
    SceneBodyMessages(const SceneBodyMessages&) = delete;
    SceneBodyMessages& operator=(const SceneBodyMessages&) = delete;
    void dispatch(FactoryObject& body, std::span<u32> message);
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
    std::array<u8, 0x20> shape_scratch_{};
    RuntimeBodyTimerRecipient* first_timer_ = nullptr; // Body +0x760.
    RuntimeBodyTimerRecipient* second_timer_ = nullptr; // Body +0x7F0.
    std::array<char, 128> command_name_{};
    u32 effect_flags_ = 0;
    RuntimeActorProximityNode* proximity_ = nullptr;
    std::unordered_map<const u8*, RuntimeBodyAttachedActor*> created_;
    std::unordered_map<const u8*, RuntimeBodyTimerObject*> timers_;
    FactoryObject timer_object_;
    std::array<float, 4> parameter_scratch_{};
    RuntimeDamageAnimationRows damage_rows_{0, {}};
    std::span<u8> body_view_;
    EntityMaskHost mask_{};
    ActorLiveAttachment* live_attachment_ = nullptr; // Actor +0x8F8.
    RuntimeBodyActionMessages actions_;
    RuntimeBodySoundCommandContext sound_commands_;
    RuntimeBodyProjectileCreation projectiles_;
    RuntimeBodyEffectCommands effect_commands_;
    std::function<void(RuntimeBodyAttachedActor&, std::span<char, 128>)> query_name_;
    RuntimeBodyAnimationDispatch animation_;
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
    GhoulySpawnPool* shapes_ = nullptr;
    const u8* actor_bound_ = nullptr;
    RuntimeBodyChannelChild first_step_;
    RuntimeBodyChannelChild second_step_;
};
}
