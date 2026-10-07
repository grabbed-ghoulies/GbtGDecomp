#pragma once

#include "../entities/entity_types.h"

namespace gbtg
{
namespace entity_detail
{
// 0x0032B298

// 0x00105AA0. fmod into one turn, then fold into [-pi, pi).
float wrap_half_turn(float angle);

// cvttss2si. Out of range and NaN become the indefinite integer.
i32 trunc_i32(float v);

} // namespace entity_detail
void entity_body_camera_sizeof(u32 out_sizes[2]);

void entity_body_camera_tick(Entity& body);

void entity_body_camera_shutdown(Entity& body);

bool entity_body_camera_message(Entity& body, const Message& msg);

// Xbox 0x00072B10
void camera_capture_actor_pose(Entity& body); // Xbox 0x00072B10

// namespace

// Xbox 0x00072CB0
bool camera_probe_actor_movement(Entity& body); // Xbox 0x00072CB0

} // namespace gbtg
