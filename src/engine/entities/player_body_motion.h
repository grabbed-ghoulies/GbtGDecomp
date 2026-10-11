#pragma once
#include "body_motion_controller.h"
#include "player_state.h"
#include "actor_clock.h"
#include "player_mind_events.h"

namespace gbtg
{
struct RuntimeBodyBitStorage;
void add_runtime_player_body_movement(std::span<u8> body, const Vec3& movement,
    const Actor& actor, const RuntimeActorClock& clock); // Xbox 0x00075120
struct RuntimePlayerBodyMotion
{
    const ActorModel* model; // Actor +0x204.
    RuntimeBodyMotionController*& controller; // Actor +0x54.
    RuntimeBodyMotionController*& secondary; // Actor +0x5C.
    u32& selector; // Actor +0x60.
    std::span<u8*, 6> records; // Body +0x8AC, stride 0x14.
    const RuntimeBodyModelNodes* nodes;
    RuntimeBodyShapeStorage& shapes;
};
u8* append_runtime_player_joint_motion(std::span<const u8> parameters, u8*& cursor,
    float& first, float& second, float maximum, float acceleration,
    RuntimePlayerBodyMotion& context); // Xbox 0x00073A90
void initialize_runtime_player_joint_motion(std::span<u8> body, std::span<const u8> parameters,
    RuntimePlayerBodyMotion& context); // Xbox 0x000734F0
bool initialize_runtime_player_body(std::span<u8> body, std::span<const u8> parameters,
    RuntimePlayerBodyMotion& context); // Xbox 0x00073450
void release_runtime_player_joint_motion(std::span<u8> body,
    std::span<u8*, 6> records); // Xbox 0x00073760
void destroy_runtime_player_body(std::span<u8> body,
    RuntimeBodyShapeStorage& shapes, std::span<u8*, 6> records); // Xbox 0x000734B0
float ease_runtime_body_angle(float target, float current, float minimum,
    float maximum, float gain, float elapsed); // Xbox 0x00101280
void ease_runtime_body_rotation(std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock); // Xbox 0x000265D0
struct RuntimePlayerJointTargets
{
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage*& flags; // Body +0x15C, bytes at +8.
    std::span<u8*, 6> records; // Body +0x8AC, stride 0x14.
    const RuntimeActorClock& clock;
    u32 body_address; // Retail body word, reused by the selection stack.
};
float select_runtime_player_joint_target(std::span<u8> body, size_t slot,
    const Vec3& joint_position, u32& tracking,
    RuntimePlayerJointTargets& context); // Xbox 0x00073C30
void apply_runtime_player_joint_targets(std::span<u8> body,
    RuntimePlayerJointTargets& context); // Xbox 0x00073820
struct RuntimePlayerBodyRelayChild
{
    std::function<void(u32)> relay; // Actual child vtable +0x10.
};
void relay_runtime_parent_body(u32 message, RuntimePlayerBodyRelayChild*& action,
    RuntimePlayerBodyRelayChild*& attachment); // Xbox 0x00025EC0
void relay_runtime_player_body(std::span<u8> body, u32 message,
    RuntimePlayerBodyRelayChild*& action, RuntimePlayerBodyRelayChild*& attachment,
    RuntimePlayerJointTargets& context); // Xbox 0x000744A0
bool runtime_player_body_can_select_action(std::span<const u8> body,
    std::span<const u8> actor, const RuntimePlayerGoalRing* ring,
    const EntityBitBlock& flags, std::span<const u8> address_zero); // Xbox 0x000743E0
}
