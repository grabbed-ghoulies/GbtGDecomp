#include "camera_sampling.h"
#include "quaternion_math.h"
#include "../audio/listener_settings.h"
#include "../entities/actor_sound_pose.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0004F770
void sample_cutscene_camera_position(const CutsceneCameraView& camera,
    float time, Vec3& output, i32 interpolate)
{
    if (interpolate != 0)
    {
        float first;
        float second;
        float fraction;
        fraction = sample_cutscene_curve_pair(camera.position[0], time, first, second);
        output.x = (1.f - fraction) * first + second * fraction;
        fraction = sample_cutscene_curve_pair(camera.position[1], time, first, second);
        output.y = (1.f - fraction) * first + second * fraction;
        fraction = sample_cutscene_curve_pair(camera.position[2], time, first, second);
        output.z = (1.f - fraction) * first + second * fraction;
        return;
    }
    {
        const auto& curve = camera.position[0];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.x = curve.values[key];
    }
    {
        const auto& curve = camera.position[1];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.y = curve.values[key];
    }
    {
        const auto& curve = camera.position[2];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.z = curve.values[key];
    }
}

// Xbox 0x0004F8C0
void sample_cutscene_camera_rotation(const CutsceneCameraView& camera,
    float time, Quat& output, i32 interpolate)
{
    if (interpolate != 0)
    {
        interpolate_cutscene_camera_rotation(camera.rotation, time, output);
        return;
    }
    {
        const auto& curve = camera.rotation[0];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.x = curve.values[key];
    }
    {
        const auto& curve = camera.rotation[1];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.y = curve.values[key];
    }
    {
        const auto& curve = camera.rotation[2];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.z = curve.values[key];
    }
    {
        const auto& curve = camera.rotation[3];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.w = curve.values[key];
    }
}

// Xbox 0x00051590
void interpolate_cutscene_camera_rotation(const std::array<CutsceneScalarCurveView, 4>& curves,
    float time, Quat& output)
{
    Quat first;
    Quat second;
    const float fraction = sample_cutscene_curve_pair(curves[0], time, first.x, second.x);
    sample_cutscene_curve_pair(curves[1], time, first.y, second.y);
    sample_cutscene_curve_pair(curves[2], time, first.z, second.z);
    sample_cutscene_curve_pair(curves[3], time, first.w, second.w);
    output = slerp(first, second, fraction);
}

// Xbox 0x0004F990
i32 sample_cutscene_camera_aspect(const CutsceneCameraView& camera, float time)
{
    if (camera.aspect.count != 0)
    {
        float first;
        float second;
        const float fraction = sample_cutscene_curve_pair(camera.aspect, time, first, second);
        const float value = (1.f - fraction) * first + second * fraction;
        if (std::bit_cast<float>(0x3FF33333u) < value)
        {
            return 2;
        }
        if (1.5f < value)
        {
            return 1;
        }
    }
    return 0;
}

// Xbox 0x0004E0E0
void transform_cutscene_camera_pose(const RuntimeCutsceneInstance& instance,
    const CutsceneCameraView* cameras, Vec3* position, Quat* rotation, float time)
{
    const auto& camera = cameras[instance.camera];
    if (position != nullptr)
    {
        sample_cutscene_camera_position(camera, time, *position, (instance.flags >> 5) & 1);
        position->x = instance.scale.x * position->x;
        position->y = instance.scale.y * position->y;
        position->z = instance.scale.z * position->z;
        rotate_euler_vector(instance.euler, *position, *position);
        position->x = instance.position.x + position->x;
        position->y = instance.position.y + position->y;
        position->z = instance.position.z + position->z;
    }
    if (rotation != nullptr)
    {
        sample_cutscene_camera_rotation(camera, time, *rotation, (instance.flags >> 5) & 1);
        Quat local;
        ordered_euler_quaternion(0, instance.euler, local);
        *rotation = animation_quaternion_product(local, *rotation);
    }
}
}
