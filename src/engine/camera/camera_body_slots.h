#pragma once
#include "camera_mind_rig.h"
#include "../entities/body_apply.h"

namespace gbtg
{
// The camera body (type 0x28) as its +0x18 and +0x1C slots read it.
struct RuntimeCameraBodySlots
{
    std::span<u8> body; // +0x18 slide point, +0x1CC contact, +0x2FC aim, +0x308 aim enable, +0x72C euler,
                        // +0x89C pose, +0x8F4 roll, +0x910 separation.
    Actor& actor; // Body +8.
    EntityBitBlock& bits; // Body +0x15C.
    const i32& look_profile; // DAT_00545AEC.
    const RuntimeCameraRigTunables& tunables; // DAT_004274B8 probe, DAT_004274C8 slide skip, DAT_0043401C/20.
    EntityCamNode& view; // Body +0x894: node +8 is body +0x89C and node +0x14 (Euler) is body +0x8A8.
};
void wrap_runtime_euler(const Vec3& angles, Vec3& output); // Xbox 0x00105AA0, ESI angles, EDI output
void aim_camera_body(RuntimeCameraBodySlots& camera); // Xbox 0x00072BA0
void capture_camera_body_pose(RuntimeCameraBodySlots& camera,
    RuntimeBodyApplyContext& apply); // Xbox 0x00072B10, vtable +0x18
u32 probe_camera_body_support(RuntimeCameraBodySlots& camera); // Xbox 0x00072CB0, vtable +0x1C
}
