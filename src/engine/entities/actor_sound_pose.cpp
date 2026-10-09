#include "actor_sound_pose.h"
#include "gbtg/anim.h"
#include "actor_transform.h"

#include <bit>

namespace gbtg
{
// Xbox 0x00106770
void rotate_euler_vector(const Vec3& euler, const Vec3& input, Vec3& output)
{
    const float pitch = euler.x;
    const float yaw = euler.y;
    const float roll = euler.z;
    const float x = input.x;
    const float y = input.y;
    const float z = input.z;
    const float pitch_sin = float(retail_sin(pitch));
    const float pitch_cos = float(retail_cos(pitch));
    const float forward = pitch_sin * y + pitch_cos * z;
    output.y = float(static_cast<long double>(pitch_cos) * y - static_cast<long double>(pitch_sin) * z);
    const float yaw_sin = float(retail_sin(yaw));
    const float yaw_cos = float(retail_cos(yaw));
    output.x = forward * yaw_sin + yaw_cos * x;
    output.z = forward * yaw_cos - yaw_sin * x;
    const float roll_sin = float(retail_sin(roll));
    const float roll_cos = float(retail_cos(roll));
    const float first = output.x;
    const float second = output.y;
    output.x = first * roll_cos - second * roll_sin;
    output.y = float(static_cast<long double>(roll_cos) * second + static_cast<long double>(roll_sin) * first);
}

// Xbox 0x00041F10
void actor_sound_direction(const Actor& actor, i32 slot, Vec3& output)
{
    const i16 bone = std::bit_cast<i16>(actor.sound_slots[slot].bone);
    if (bone != -1)
    {
        Vec3 euler;
        actor_joint_world_euler(actor, u16(bone), euler);
        rotate_euler_vector(euler, Vec3{0.f, 0.f, 1.f}, output);
        return;
    }
    const Vec3 euler = actor_world_euler(actor);
    output = Vec3{0.f, 0.f, 0.f};
    add_sound_direction(output, euler.x, euler.y, 1.f);
}

// Xbox 0x00041E30
void update_actor_sound_pose(const Actor& actor, RuntimeActorSound& sound)
{
    const i32 slot = std::bit_cast<i16>(sound.request_id);
    const ActorSoundSlot& settings = actor.sound_slots[slot];
    if (settings.category != 0)
    {
        const i16 bone = std::bit_cast<i16>(settings.bone);
        Vec3 position;
        if (bone == -1)
        {
            position = actor_world_position(actor);
        }
        else
        {
            actor_transform_joint_point(actor, u16(bone), actor_transform_origin, position);
        }
        sound.position = position;
    }
    if (settings.mode != 0)
    {
        Vec3 direction;
        actor_sound_direction(actor, slot, direction);
        sound.direction = direction;
    }
}
}
