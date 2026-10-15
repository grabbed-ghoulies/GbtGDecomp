#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
// D3DX row-vector helpers the view passes use (16-float row-major matrices, x, y, z(, w) vectors).
void subtract_runtime_graphics_vectors(float* output, const float* first,
    const float* second); // Xbox 0x00184F74
void cross_runtime_graphics_vectors(float* output, const float* first,
    const float* second); // Xbox 0x00184F28
void transform_runtime_graphics_vector4(float* output, const float* vector,
    const float* matrix); // Xbox 0x0018468C, D3DXVec4Transform
void transpose_runtime_graphics_matrix(float* output, const float* input); // Xbox 0x00184725, D3DXMatrixTranspose
// Returns false and leaves the output unchanged when the reciprocal determinant is not finite.
bool inverse_runtime_graphics_matrix(float* output, float* determinant,
    const float* input); // Xbox 0x00184792, D3DXMatrixInverse
void plane_from_runtime_point_normal(float* output, const float* point,
    const float* normal); // Xbox 0x00185345, D3DXPlaneFromPointNormal
void normalize_runtime_graphics_plane(float* output, const float* plane); // Xbox 0x001852A1, D3DXPlaneNormalize
void reflect_runtime_graphics_matrix(float* output, const float* plane); // Xbox 0x0018537D, D3DXMatrixReflect
void look_at_runtime_graphics_matrix(float* output, const float* eye, const float* at,
    const float* up); // Xbox 0x001851AD, D3DXMatrixLookAtLH
void perspective_runtime_graphics_matrix(float* output, float field_of_view, float aspect, float near_plane,
    float far_plane); // Xbox 0x00184E2A, D3DXMatrixPerspectiveFovLH
// asin through the x87 FPATAN form the CRT uses.
long double runtime_arc_sine(long double value); // Xbox 0x001706E0
}
