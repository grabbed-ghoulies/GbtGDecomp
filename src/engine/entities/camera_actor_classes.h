#pragma once
#include "strategy_fields.h"
#include "gbtg/object_factory.h"
#include "gbtg/entity.h"
#include "body_animation_setup.h"
#include "player_children.h"
#include "player_strategy_entry.h"
#include "player_messages.h"
#include "player_strategy_goals.h"
#include "player_mind_events.h"
#include "player_mind_feedback.h"

namespace gbtg
{
// The player's camera actor (aid_objparams_ghoulies_actor_cameron): camera body type 0x28 (getter
// 0x00073190, vtable 0x00504F80), camera mind type 0x36 (getter 0x0007AFE0, vtable 0x00505458) and
// director strategy type 0x39 (getter 0x000AAE60, vtable 0x00505D54).
extern u32 g_camera_actor; // DAT_00510F70, the camera body's actor address.
struct RuntimeCameraBodyContext
{
    std::span<u8> body;
    const Actor& actor; // Body +8.
    RuntimeBodyBitStorage* bits; // Body +0x15C.
    EntityCamNode& view; // Body +0x894.
};
void camera_body_object_sizes(ObjectSizes& output); // Native view of Xbox 0x00073170
bool initialize_runtime_camera_body(RuntimeCameraBodyContext& context); // Xbox 0x000731F0
void destroy_runtime_camera_body(RuntimeCameraBodyContext& context); // Xbox 0x00073300
void camera_mind_object_sizes(ObjectSizes& output); // Xbox 0x0007AFC0
void director_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AAE40
// The director strategy (factory type 0x39, 0x44 bytes).
struct DirectorStrategyState
{
    StrategyBase base;
    u32 unk_38 = 0; // +0x38, 0 at first; state 0x3F6's request takes it, and 0x000AA250 stops with no target and 0
    u32 first_update = 0; // +0x3C, 1 at first; 0x000AA250 clears it each update, and it holds `starting` off
    u32 starting = 0; // +0x40, 1 at first; the last request's +0x2A0
};
static_assert(sizeof(DirectorStrategyState) == 0x44);
bool initialize_runtime_director_strategy(std::span<u8> strategy); // Xbox 0x000AAEB0
struct RuntimeDirectorStrategy
{
    std::span<u8> strategy;
    RuntimePlayerMindRequest& requests; // Actor +0x958 mind +0x54 anchor.
    std::span<const u8> request_sizes; // DAT_00217F18.
    RuntimePlayerStrategyEntry& entry; // Base strategy state entry 0x0002BAF0.
};
void enter_runtime_director_state(u16 state, RuntimeDirectorStrategy& context); // Xbox 0x000AA3A0
void leave_runtime_director_state(std::span<u8> strategy, u16 state); // Xbox 0x000AA450
struct RuntimeDirectorMessages
{
    FactoryObject& strategy;
    Actor& actor; // Strategy +8.
    std::span<u8> mind; // Actor +0x958.
    RuntimePlayerStrategySetup& setup;
    RuntimeBaseStrategyMessages& base;
    std::function<bool(Vec3&, Vec3&)> camera_start; // 0x0013CC70.
    std::function<void()> actor_update; // Actor vtable +0x10.
};
void dispatch_runtime_director_message(std::span<const u32> message,
    RuntimeDirectorMessages& context); // Xbox 0x000AAED0
struct RuntimeCameraMindMessages
{
    RuntimeMindEvents& events; // Parent mind handler 0x0002A2E0.
    std::span<u8> mind;
    std::span<const u8> target; // Mind +0x10 object.
    i32 floor; // Mind +8 actor +0xCEC.
    std::span<entity_detail::NavGrid* const> grids; // DAT_0050E240.
    u8* nodes; // DAT_0050E24C.
    RuntimeMindStorage*& storage;
    std::function<u32(const void*)> address_of;
};
void dispatch_runtime_camera_mind_message(std::span<const u32> message,
    RuntimeCameraMindMessages& context); // Xbox 0x0007B040
}
