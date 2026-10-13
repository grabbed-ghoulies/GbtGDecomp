#pragma once
#include "display_view.h"

namespace gbtg
{
bool clip_runtime_camera_plane(const entity_detail::DisplayView& view, const Vec3& plane,
    const Vec3& source, Vec3& candidate, Vec3* normal,
    std::array<float, 2>* screen); // Xbox 0x0013C0B0
}
