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

// Column-major 4x4 skin matrices, one per joint. Rigid vertices are in the
// authored model pose. `bind` is that pose (cutscene frame 0). The matrix is
// anim_world * inverse(bind_world), which is identity on the authored frame.
void skin_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, float* out, int max_joints,
                   const std::vector<JointPose>* bind = nullptr);

} // namespace gbtg
