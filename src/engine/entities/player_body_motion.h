#pragma once
#include "body_motion_controller.h"
#include "player_state.h"

namespace gbtg
{
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
}
