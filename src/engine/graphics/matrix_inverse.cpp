#include "matrix_inverse.h"
#include "extended_float.h"

#include <array>
#include <bit>

namespace gbtg
{
// Xbox 0x0016744D
bool runtime_graphics_double_is_finite(double value)
{
    return (std::bit_cast<u64>(value) & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

// Xbox 0x00151FAD
void invert_runtime_graphics_matrix(std::span<float, 16> output, float* determinant,
    std::span<const float, 16> input)
{
    std::array<ExtendedFloat, 16> m;
    for (u32 index = 0; index < 16; ++index)
    {
        m[index] = input[index];
    }
    // The first six minors and two of the second six are spilled to float.
    const ExtendedFloat a0 = float(m[5] * m[0] - m[4] * m[1]);
    const ExtendedFloat a1 = float(m[9] * m[0] - m[8] * m[1]);
    const ExtendedFloat a2 = float(m[13] * m[0] - m[12] * m[1]);
    const ExtendedFloat a3 = float(m[9] * m[4] - m[8] * m[5]);
    const ExtendedFloat a4 = float(m[13] * m[4] - m[12] * m[5]);
    const ExtendedFloat a5 = float(m[13] * m[8] - m[12] * m[9]);
    const ExtendedFloat b0 = float(m[2] * m[7] - m[6] * m[3]);
    const ExtendedFloat b1 = float(m[2] * m[11] - m[10] * m[3]);
    const ExtendedFloat b2 = m[2] * m[15] - m[14] * m[3];
    const ExtendedFloat b3 = m[6] * m[11] - m[10] * m[7];
    const ExtendedFloat b4 = m[6] * m[15] - m[14] * m[7];
    const ExtendedFloat b5 = m[10] * m[15] - m[14] * m[11];
    std::array<float, 16> cofactors;
    cofactors[0] = float((b5 * m[5] - b4 * m[9]) + b3 * m[13]);
    cofactors[1] = float((b2 * m[9] - b1 * m[13]) - b5 * m[1]);
    cofactors[2] = float((b4 * m[1] - b2 * m[5]) + b0 * m[13]);
    cofactors[3] = float((b1 * m[5] - b0 * m[9]) - b3 * m[1]);
    cofactors[4] = float((b4 * m[8] - b3 * m[12]) - b5 * m[4]);
    cofactors[5] = float((m[0] * b5 - b2 * m[8]) + b1 * m[12]);
    cofactors[6] = float((b2 * m[4] - b0 * m[12]) - b4 * m[0]);
    cofactors[7] = float((b3 * m[0] - b1 * m[4]) + b0 * m[8]);
    cofactors[8] = float((a5 * m[7] - a4 * m[11]) + a3 * m[15]);
    cofactors[9] = float((a2 * m[11] - a1 * m[15]) - a5 * m[3]);
    cofactors[10] = float((a4 * m[3] - a2 * m[7]) + a0 * m[15]);
    cofactors[11] = float((a1 * m[7] - a0 * m[11]) - a3 * m[3]);
    cofactors[12] = float((m[10] * a4 - m[14] * a3) - m[6] * a5);
    cofactors[13] = float((m[2] * a5 - m[10] * a2) + m[14] * a1);
    cofactors[14] = float((m[6] * a2 - m[14] * a0) - m[2] * a4);
    cofactors[15] = float((m[2] * a3 - m[6] * a1) + m[10] * a0);
    const ExtendedFloat value = ((static_cast<ExtendedFloat>(cofactors[3]) * m[12] +
        static_cast<ExtendedFloat>(cofactors[2]) * m[8]) +
        static_cast<ExtendedFloat>(cofactors[1]) * m[4]) +
        static_cast<ExtendedFloat>(cofactors[0]) * m[0];
    if (determinant != nullptr)
    {
        *determinant = float(value);
    }
    const float reciprocal = float(ExtendedFloat{1.} / value);
    if (runtime_graphics_double_is_finite(double(reciprocal)))
    {
        for (u32 index = 0; index < 16; ++index)
        {
            output[index] = float(static_cast<ExtendedFloat>(cofactors[index]) * reciprocal);
        }
    }
}
}
