#pragma once

#include "gbtg/model.hpp"

namespace gbtg
{

// One joint after 0x2E8A0. quat is xyzw, translation and scale are in joint space.
struct JointPose
{
    Quat rotation;
    Vec3 translation;
    Vec3 scale{1.f, 1.f, 1.f};
};

// Linear-interp sample of a MINA block (player+0x2B == 0, blend weight 1).
// `time_norm` is seconds/duration, the value the cutscene writes to the anim player at +0x14.
// `loop` is player+0x2A == 0. Cutscene init (0x31AC0 from 0x2D8E0) stores 1, so playback clamps.
std::vector<JointPose> sample_mina(std::span<const u8> mina, float time_norm, bool loop = false);

// Column-major 4x4 skin palette, one matrix per joint (Xbox stores the same
// transform as a 3x4). Rigid vertices index one slot. 0x2FAC0 concatenates
// parent * local; 0x2F720 then subtracts the joint bind point.
void skin_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, float* out, int max_joints);

// Column-major palette whose frame-0 pose is identity on the mesh. `delta` is the
// translation that puts skeleton bind points onto the vertices (0x2FAC0's bind
// point is not in the same space as the position stream).
void skin_draw_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& rest,
                        const std::vector<JointPose>& pose, Vec3 delta, float* out, int max_joints);

} // namespace gbtg
