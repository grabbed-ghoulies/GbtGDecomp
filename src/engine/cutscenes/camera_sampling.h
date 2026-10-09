#pragma once
#include "curve_sampling.h"
#include "instance_construction.h"

namespace gbtg
{
struct CutsceneCameraView
{
    std::array<CutsceneScalarCurveView, 3> position; // +0, +C, +18
    std::array<CutsceneScalarCurveView, 4> rotation; // +24, +30, +3C, +48
    CutsceneScalarCurveView focal; // +54
    CutsceneScalarCurveView aspect; // +60
};
void sample_cutscene_camera_position(const CutsceneCameraView& camera,
    float time, Vec3& output, i32 interpolate); // Xbox 0x0004F770
void interpolate_cutscene_camera_rotation(const std::array<CutsceneScalarCurveView, 4>& curves,
    float time, Quat& output); // Xbox 0x00051590
void sample_cutscene_camera_rotation(const CutsceneCameraView& camera,
    float time, Quat& output, i32 interpolate); // Xbox 0x0004F8C0
i32 sample_cutscene_camera_aspect(const CutsceneCameraView& camera, float time); // Xbox 0x0004F990
void transform_cutscene_camera_pose(const RuntimeCutsceneInstance& instance,
    const CutsceneCameraView* cameras, Vec3* position, Quat* rotation,
    float time); // Xbox 0x0004E0E0
void update_cutscene_camera_view(const RuntimeCutsceneInstance& instance,
    const CutsceneCameraView* cameras, EntityCamNode& view); // Xbox 0x0004D7D0
}
