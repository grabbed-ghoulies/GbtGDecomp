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

// DAT_00545AEC. Profile 4 leaves the aim height alone; every other value adds 6.
extern i32 g_look_profile;

} // namespace entity_detail
i32 direction_to_yaw_pitch(float x, float y, float z, float& yaw, float& pitch); // Xbox 0x00105E20
void entity_body_camera_sizeof(u32 out_sizes[2]);
// Boot: the four camera slots lose their record lists, bound records and saved names, and the actor camera
// effect list (DAT_004E1734) empties.
void reset_camera_slots(); // Xbox 0x00048980

} // namespace gbtg
