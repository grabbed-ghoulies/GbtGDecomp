#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
void conjugate_display_quaternion(const Quat& input, Quat& output); // Xbox 0x00153D80
void normalize_display_quaternion(const Quat& input, Quat& output); // Xbox 0x001541B0
void make_display_quaternion_matrix(const Quat& input, float* output); // Xbox 0x00104910
void extract_display_euler(const float* matrix, u32 order, float& first,
    float& second, float& third); // Xbox 0x00154270
void extract_display_quaternion_euler(const Quat& input, u32 order,
    float& first, float& second, float& third); // Xbox 0x001546E0
void expand_display_affine(const float* input, float* output); // Xbox 0x00104D80
void transform_display_points(const float* matrix, const float* input,
    float* output, u32 count); // Xbox 0x00104A70
long double display_tangent(float angle); // Xbox 0x00152860
}
