#include "camera_sampling.h"
#include "quaternion_math.h"
#include "../camera/display_view.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0004D7D0
void update_cutscene_camera_view(const RuntimeCutsceneInstance& instance,
    const CutsceneCameraView* cameras, EntityCamNode& view)
{
    Vec3 position;
    Quat rotation;
    transform_cutscene_camera_pose(instance, cameras, &position, &rotation, instance.time);
    if (0.f < instance.blend_time)
    {
        float fraction = 1.f;
        float remaining = instance.blend_duration;
        if (remaining != 0.f)
        {
            remaining = (instance.blend_time - remaining) / (0.f - remaining);
            if (remaining <= 1.f)
            {
                fraction = 0.f;
                if (0.f <= remaining)
                {
                    fraction = remaining;
                }
            }
        }
        position.x = (position.x - instance.blend_position.x) * fraction + instance.blend_position.x;
        position.y = (position.y - instance.blend_position.y) * fraction + instance.blend_position.y;
        position.z = (position.z - instance.blend_position.z) * fraction + instance.blend_position.z;
        rotation = slerp(instance.blend_rotation, rotation, fraction);
    }
    view.position = position;
    view.rotation = rotation;
    const auto& camera = cameras[instance.camera];
    float first;
    float second;
    const float fraction = sample_cutscene_curve_pair(camera.focal, instance.time, first, second);
    float focal = (1.f - fraction) * first + second * fraction;
    i32 aspect = sample_cutscene_camera_aspect(camera, instance.time);
    i32 width, x, height, y;
    float fov;
    if ((instance.flags & 8) == 0)
    {
        view.aspect_slot = aspect;
        camera_fit_display_view(view.focal, view.aspect_slot, width, x, height, y,
            fov, view.pixel_aspect, view.focal_pixels);
    }
    else
    {
        if (aspect == 3)
        {
            aspect = entity_detail::g_aspect_preset;
        }
        constexpr float sensors[]
        {
            std::bit_cast<float>(0x4178B780u), std::bit_cast<float>(0x41C00625u),
            std::bit_cast<float>(0x41C00625u), 0.f
        };
        float sensor;
        if (view.aspect_slot == 3)
        {
            sensor = sensors[entity_detail::g_aspect_preset];
        }
        else
        {
            sensor = sensors[view.aspect_slot];
        }
        focal = (sensor * focal) / sensors[aspect];
    }
    view.focal = focal;
    camera_fit_display_view(focal, view.aspect_slot, width, x, height, y,
        fov, view.pixel_aspect, view.focal_pixels);
}
}
