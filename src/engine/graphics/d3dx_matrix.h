#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
void store_runtime_quaternion(Quat& output, float x, float y, float z,
    float w); // Xbox 0x00170250
void runtime_graphics_quaternion_matrix(float* output,
    const Quat& quaternion); // Xbox 0x00184D48
void compose_runtime_graphics_matrix(float* output, Vec3 scale,
    Quat rotation, Vec3 translation); // Xbox 0x00153BE0
// D3DX row-vector matrices (16 floats, row major).
void multiply_runtime_graphics_matrices(float* output, const float* first,
    const float* second); // Xbox 0x001850CA, D3DXMatrixMultiply
void runtime_graphics_translation_matrix(float* output, float x, float y,
    float z); // Xbox 0x00184B7B, D3DXMatrixTranslation
void transform_runtime_graphics_normal(Vec3& output, const Vec3& normal,
    const float* matrix); // Xbox 0x0018462D, D3DXVec3TransformNormal
void runtime_graphics_scaling_matrix(float* output, float x, float y,
    float z); // Xbox 0x00184B1F, D3DXMatrixScaling
void scale_runtime_graphics_vector(Vec3& vector, float divisor); // Xbox 0x00184EBE, ECX vector
void transform_runtime_graphics_coord(Vec3& output, const Vec3& point,
    const float* matrix); // Xbox 0x00185015, D3DXVec3TransformCoord
void copy_runtime_graphics_vector(const float* input, float* output); // Xbox 0x00154840
void runtime_graphics_matrix_column_lengths(const float* matrix, float* output); // Xbox 0x00153AC0
void scale_runtime_graphics_matrix_rows(const float* input, float* output, float x, float y,
    float z); // Xbox 0x001537C0
}
