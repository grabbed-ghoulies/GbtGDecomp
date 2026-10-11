#pragma once
#include "player_mind_events.h"
#include "body_message_helpers.h"
#include "../camera/actor_camera_effects.h"
#include "../frontend/player_health_display.h"

namespace gbtg
{
float runtime_actor_direction_to_point(const Actor& actor, const Vec3& point); // Xbox 0x000419D0
void set_runtime_player_actor_rotation(Actor& actor, const Actor& body_actor,
    std::span<u8> body, const Vec3& rotation); // Xbox 0x0002B560
struct RuntimePlayerMindFeedback
{
    std::span<u8> mind;
    std::span<u8> body;
    Actor& actor;
    std::span<const u8> input; // Actor +0x960, actual device.
    u32 input_address;
    RuntimeInputActivationContext& activation;
    RuntimeBodyActionMessages& events;
    std::span<const u8> event_ids; // DAT_002FFEEE, words indexed by event kind.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
void clear_runtime_player_mind_body_event(std::span<u8> body); // Xbox 0x0007DAC0
void notify_runtime_player_mind_release(RuntimePlayerMindFeedback& context); // Xbox 0x0007DAE0
void notify_runtime_player_mind_impact(RuntimePlayerMindFeedback& context); // Xbox 0x0007DC40
void notify_runtime_player_mind_activation(RuntimePlayerMindFeedback& context); // Xbox 0x0007DC80
void notify_runtime_player_mind_event(RuntimePlayerMindFeedback& context, i32 kind); // Xbox 0x0007E420
void notify_runtime_player_mind_contact(RuntimePlayerMindFeedback& context,
    u32 payload, u32 target); // Xbox 0x0007DBE0
void notify_runtime_player_mind_source(RuntimePlayerMindFeedback& context,
    std::span<const u8> message); // Xbox 0x0007DCC0
void notify_runtime_player_mind_target(RuntimePlayerMindFeedback& context,
    std::span<const u8> message); // Xbox 0x0007DCE0
void shake_runtime_player_mind_camera(RuntimePlayerMindFeedback& context,
    const char* name); // Xbox 0x0007E470
void reset_runtime_player_mind_body_event(RuntimePlayerMindFeedback& context,
    std::span<const u8> target, const std::span<const u8>* actor_mind); // Xbox 0x0007DB20
void track_runtime_player_mind_source(std::span<u8> mind,
    const std::span<const u8>* body_state, u32 present, u32 source); // Xbox 0x0007DEE0
void record_runtime_player_mind_distance(std::span<u8> mind, float distance); // Xbox 0x0007E5C0
void record_runtime_player_mind_position(std::span<u8> mind, const Actor& actor,
    u32 source, const Vec3& position); // Xbox 0x0007F100
bool clear_runtime_player_mind_reference(std::span<u8> mind, size_t offset,
    u32 actor); // Xbox 0x00140A40
bool runtime_player_mind_uses_special_event(u32 kind); // Xbox 0x00123E50
u8 runtime_player_mind_active(const std::span<const u8>* mind); // Xbox 0x00028CA0
void select_runtime_player_mind_alert(std::span<u8> mind, std::span<const u8> body,
    const EntityBitBlock& flags, u32 player_link); // Xbox 0x00137340
bool initialize_runtime_player_mind_navigation(std::span<u8> mind, i32 floor,
    std::span<entity_detail::NavGrid* const> grids, u8* nodes, RuntimeMindStorage*& storage,
    const std::function<u32(const void*)>& address_of); // Xbox 0x00136700
void update_runtime_player_mind_health_display(std::span<const u8> mind,
    std::span<const u8> body, const EntityBitBlock& flags,
    RuntimePlayerHealthDisplay& display); // Xbox 0x0007FC30
struct RuntimePlayerMindTick
{
    RuntimeMindTick& base;
    std::span<const u8> actor_bytes;
    EntityBitBlock*& flags; // Body +0x15C -> bytes +8.
    const u32& player_link; // DAT_00511B48.
    std::span<const u32, 2> references; // DAT_0050E764 and DAT_0050E768.
    const RuntimeInputReleaseContext& input_release;
    RuntimePlayerHealthDisplay& health;
};
void tick_runtime_player_mind(RuntimePlayerMindTick& context,
    std::span<u8> input); // Xbox 0x0007FB10
}
