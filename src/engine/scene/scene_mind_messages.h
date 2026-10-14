#pragma once
#include "../entities/player_factory_owner.h"
#include "../entities/cinema_mind_messages.h"
#include "../entities/cinema_mind_tick.h"
#include "../entities/actor_model_anchors.h"
#include "../entities/player_mind_messages.h"
#include "../entities/player_input_binding.h"
#include "../scripts/script_actor_query.h"
#include "../entities/player_mind_request_update.h"
#include "../entities/player_control_update.h"
#include "../entities/player_mind_target_control.h"
#include "../entities/player_mind_targets.h"
#include "../entities/body_request_selection.h"
#include "../entities/player_mind_control.h"
#include "../navigation/player_avoidance.h"

namespace gbtg
{
// Scene state shared by every mind's message graph.
struct SceneMindServices
{
    Game& game;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    std::span<const char* const, 0x41> flag_names; // 0x00338FB0.
    const char* const* action_names; // 0x003390E4, 0x174 names.
    const char* const* callout_names; // 0x00421C9C, 0x55 names.
    const char* const* goal_flag_names; // 0x003397AC, 3 names.
    const RuntimePlayerStrategyResume& resume;
    std::span<u8> story_bits; // *(DAT_00506C6C+8).
    const u32& story_word; // DAT_0050EA98.
    i32& allowed_count; // DAT_0050E148.
    std::span<const u8> allowed_rows; // DAT_0050E14C.
    std::span<entity_detail::NavGrid* const> grids; // DAT_0050E240.
    u8*& nodes; // DAT_0050E24C.
    std::span<const u8> event_ids; // DAT_002FFEEE.
    RuntimeInputActivationContext& activation;
    const RuntimeInputReleaseContext& release;
    std::span<const u32> port_devices; // DAT_0050E0F4 rows.
    const i32& chapter_mode; // DAT_00430D0C.
    const u32& runtime_flags; // DAT_0050EA9C.
    RuntimeCameraPulse& pulse;
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    std::function<u32(const void*)> address_of;
    std::function<Actor*(u32)> actor_at;
    std::function<std::span<const u8>(u32)> bytes_at;
    const RuntimeActorClock& clock;
    std::span<const u8> address_zero; // Reads through a null retail pointer.
    float& occupancy_initial;
    const RuntimeDialogueInputState& input_state;
    std::span<u8> keyboard; // DAT_00507C98.
    // Mind vtable +0x14 / +0x18 of this mind (0x0007C0E0 / 0x0007EBC0 for cinema minds).
    std::function<i32(FactoryObject&, RuntimePlayerMindRequest&)> update_request;
    std::function<u32(FactoryObject&, RuntimePlayerMindRequest&)> initialize_request;
    std::span<const u32, 2> pause_references; // DAT_0050E764..68.
    RuntimePlayerHealthDisplay& health;
    const void* const& markers; // DAT_0051108C.
    RuntimeActorSoundQueues& sounds;
    const RuntimeBodyDamageTables& damage_tables;
    std::span<const float> mode_scales; // DAT_00217EC0.
    std::function<RuntimeBodyAttachedActor&(u32)> attached_actor_at;
    std::function<RuntimeMindTarget&(u32)> target_at;
    std::function<std::span<const u8>(const RuntimeBodyAttachedActor&)> attachment_link;
    std::function<u32()> active_player_link; // DAT_00511B48, alt list's next pointer.
    // Player control (0x0007D130) views.
    RuntimeMindTarget* const& first_target; // DAT_00545988.
    const std::unordered_map<u32, RuntimePlayerTargetView*>& target_views;
    std::unordered_map<Actor*, const RuntimePlayerAttachmentEligibility*>& target_eligibility;
    std::unordered_map<Actor*, u32>& target_addresses;
    std::unordered_map<const Actor*, RuntimeScriptMindQuery*>& target_queries;
    const RuntimeScriptActorQueryContext& query;
    const u32& view_invert; // DAT_005122D8.
    std::span<const u8> boost_preset; // DAT_002FFF24.
    float angular_limit; // DAT_004275B8.
    u32& attachment_notice; // DAT_00433BD0.
    // 0x0007DF50's request selection (0x0001CDA0, 0x000243C0, 0x0001D130).
    std::span<const u8> class_table; // DAT_00545AC4.
    std::span<const u8> chain_limits; // DAT_00433D5C (minimum, maximum bytes), DAT_00433D60, DAT_00433D64.
    const u32& special_first; // DAT_00512248, the active word of powerup row 11.
    const u32& special_second; // DAT_00512200.
    std::function<const u8*(u32)> resolve; // A retail data address's bytes.
    std::function<std::span<const u8>(u32)> target_body; // An actor address's +0x95C body bytes.
    std::function<u32(const void*, size_t)> heap_address; // A host heap block's stable retail-style address.
};

// One mind's message graph: cinema minds (type 0x35, 0x0007C8B0) and player minds (type 0x34, 0x0007F1E0)
// over the shared parent-mind requests. Members alias the player's native state.
class SceneMindMessages
{
public:
    SceneMindMessages(SceneMindServices& services, NativePlayerInstance& player, NativePlayerMindSlots& slots);
    ~SceneMindMessages();
    SceneMindMessages(const SceneMindMessages&) = delete;
    SceneMindMessages& operator=(const SceneMindMessages&) = delete;
    void refresh(); // Native snapshots of body fields read by the handlers.
    // Views for the mind's vtable +0x10 tick: refresh(), then the gaze target (mind +0xC) and anchor (mind +0x30).
    void prepare_tick();

private:
    SceneMindServices& services_;
    NativePlayerInstance& player_;
    bool cinema_mind_ = false;
    const RuntimeAttachmentRequestTables* tables_ = nullptr; // Body +0x5C.
    const u8* body_state_ = nullptr; // Body +0x84.
    RuntimeScriptMindQuery body_query_{};
    const u8* event_record_ = nullptr; // Actor +0x8C.
    RuntimeBodyActionMessages actions_;
    RuntimeBodyAttachmentRequests attachment_;
    RuntimeParentMindRequests parent_;
    RuntimeMindBoost boost_;
    RuntimeCinemaMindMessages cinema_;
    RuntimePlayerMindFeedback feedback_;
    RuntimePlayerInputBinding binding_;
    std::span<const u8> target_; // Mind +0x10 object.
    std::span<const u8> action_target_; // Mind +0x14 object.
    std::span<const u8> event_source_; // Mind +0x88 object.
    std::span<const u8> actor_mind_; // Actor +0x958.
    std::span<const u8> body_state_view_; // Body +0x84 object.
    RuntimePlayerMindMessages player_messages_;
    // Cinema tick views. Mind +0xC and +0x30 hold retail addresses of an actor and a model anchor.
    Actor* gaze_target_ = nullptr;
    ActorModelAnchor* gaze_anchor_ = nullptr;
    std::unordered_map<ActorModelAnchor*, u32> anchor_addresses_;
    RuntimeMindTick mind_tick_;
    RuntimeCinemaMindAvoidance avoidance_;
    RuntimePlayerMindGaze gaze_;
    RuntimePlayerMindControl control_;
    RuntimeCinemaMindTick cinema_tick_;
    u32 active_player_link_ = 0;
    RuntimePlayerMindTick player_tick_;
    entity_detail::NavigationPathSearch* path_search_ = nullptr;
    RuntimePlayerNavigation navigation_;
    RuntimePlayerNavigationControl navigation_control_;
    RuntimePlayerAttachmentEligibility eligibility_;
    RuntimeMindEscapePosition escape_;
    RuntimeCinemaMindMovement movement_;
    RuntimeCinemaMindSpawnMovement spawn_;
    RuntimeCinemaMindRequestUpdate cinema_request_;
    // Player mind request slot 0x0007D360 and its control update 0x0007D130.
    Vec3 camera_direction_{}; // DAT_005077A8 +0x1C, copied before the mind runs.
    std::span<const u8> boost_preset_;
    float retained_cone_distance_ = 0.f; // 0x0007E600 caller-stack word.
    float retained_angle_ = 0.f; // 0x00137DE0 caller-stack word.
    float retained_direction_ = 0.f; // 0x00105F90 output word.
    RuntimeBodyAttachedActor* pending_attachment_ = nullptr; // Mind +0x134.
    RuntimePlayerTargetView embedded_target_{}; // Mind +0xB8.
    // 0x0007DF50 against one target: this body's attachment requests, chains and chain length against it.
    struct TargetAction
    {
        TargetAction(SceneMindMessages& owner, RuntimeMindTarget& target);
        RuntimeRequestBits bits; // Body +0x60.
        RuntimeAttachmentTargetSelection selection;
        RuntimeMindTargetRequestSelection mind;
        RuntimeAttachmentChainSelection chains;
        RuntimeBodyChainLength length;
        RuntimeMindChainRequestSelection requests;
        RuntimePlayerTargetActionBinding binding;
    };
    std::unordered_map<u32, std::unique_ptr<TargetAction>> target_actions_;
    // Body +0x64 / +0x68 in retail layout (8-byte rows: length, count, values address; then the value words),
    // built from the native groups and kept while they last, so chain addresses held in body +0x88 stay valid.
    RuntimeAttachmentChainRows chain_rows_{};
    std::vector<u8> chain_image_;
    u32 chain_image_base_ = 0;
    const RuntimeBodyAnimationGroup* chain_image_source_ = nullptr;
    void refresh_chain_image();
    const u8* resolve_chain_address(u32 address) const;
    RuntimePlayerTargetActionBinding& target_action(u32 target);
    std::unordered_map<RuntimeBodyAttachedActor*, std::span<const u8>> attachment_links_;
    RuntimePlayerMindFocus focus_;
    RuntimePlayerTargetSearch search_;
    RuntimePlayerMindShapeTarget shape_target_;
    RuntimePlayerMindProposal proposal_;
    RuntimePlayerMindTargetControl target_control_;
    RuntimePlayerMindActionControl action_control_;
    RuntimePlayerMindPendingAttachment pending_;
    RuntimePlayerMindInactivity inactivity_;
    RuntimePlayerNavigationAvoidance player_avoidance_;
    RuntimePlayerControlUpdate control_update_;
    RuntimePlayerMindRequestUpdate player_request_;
    void refresh_gaze();
    void refresh_control();
};
}
