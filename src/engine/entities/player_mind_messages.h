#pragma once
#include "player_mind_requests.h"
#include "player_input_binding.h"
#include "../camera/camera_pulse.h"

namespace gbtg
{
struct Game;

// Player mind (vtable slot +0xC) message context. Retail object words in messages stay
// addresses; the resolvers map them to the owning native objects.
struct RuntimePlayerMindMessages
{
    RuntimeMindEvents& events;
    RuntimePlayerMindFeedback& feedback;
    RuntimeMindBoost& boost;
    RuntimeParentMindRequests& requests;
    RuntimePlayerInputBinding& binding;
    const RuntimePlayerStrategyResume& resume;
    Game& game;
    u32 actor_address; // Mind +8.
    std::span<const u8> actor_bytes;
    EntityBitBlock& flags; // Body +0x15C -> +8.
    std::span<const u8> target; // Mind +0x10 object.
    const std::span<const u8>* action_target; // Mind +0x14 object.
    const std::span<const u8>* event_source; // Mind +0x88 object.
    const std::span<const u8>* actor_mind; // Actor +0x958.
    const std::span<const u8>* body_state; // Body +0x84 object.
    std::span<entity_detail::NavGrid* const> grids; // DAT_0050E240.
    u8* nodes; // DAT_0050E24C.
    RuntimeMindStorage*& storage;
    const ActorAnimationState& animation; // Actor +0x50.
    const i32& chapter_mode; // DAT_00430D0C.
    const u32& runtime_flags; // DAT_0050EA9C.
    RuntimeCameraPulse& pulse;
    std::function<u32(const void*)> address_of;
    std::function<const char*(u32)> message_name; // Message +4 string address.
    std::function<const Actor*(u32)> actor_at; // Message +8 actor address.
};
bool runtime_resource_name_is_aid(const char* name); // Xbox 0x000357B0, EAX name
void clear_runtime_goal_message(std::span<u32, 9> message); // Xbox 0x00049DB0, EAX message
void dispatch_runtime_player_mind_message(RuntimePlayerMindMessages& context, std::span<const u32> message,
    std::span<const u8> message_target); // Xbox 0x0007F1E0
}
