#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
void rotate_euler_vector(const Vec3& euler, const Vec3& input, Vec3& output); // Xbox 0x00106770
void actor_sound_direction(const Actor& actor, i32 slot, Vec3& output); // Xbox 0x00041F10
void update_actor_sound_pose(const Actor& actor, RuntimeActorSound& sound); // Xbox 0x00041E30
}
