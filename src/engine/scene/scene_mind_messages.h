#pragma once
#include "../entities/player_factory_owner.h"
#include "../entities/cinema_mind_messages.h"
#include "../entities/player_mind_messages.h"
#include "../entities/player_input_binding.h"
#include "../scripts/script_actor_query.h"

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
};

// One mind's message graph: cinema minds (type 0x35, 0x0007C8B0) and player minds (type 0x34, 0x0007F1E0)
// over the shared parent-mind requests. Members alias the player's native state.
class SceneMindMessages
{
public:
    SceneMindMessages(SceneMindServices& services, NativePlayerInstance& player, NativePlayerMindSlots& slots);
    SceneMindMessages(const SceneMindMessages&) = delete;
    SceneMindMessages& operator=(const SceneMindMessages&) = delete;
    void refresh(); // Native snapshots of body fields read by the handlers.

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
};
}
