#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>

namespace gbtg
{
// Host execution of a bound NV2A vertex program (the microcode D3D 0x001733E0's caller hands to
// CreateVertexShader: a 0x2078 header word with the instruction count in its high half, then four
// words per instruction). The PC renderer has no NV2A, so the D3D boundary runs the program per
// vertex and draws its outputs.
using XboxVector = std::array<float, 4>;
struct XboxVertexProgramState
{
    std::array<XboxVector, 192> constants; // Hardware numbering: D3D register + 96.
    std::array<XboxVector, 16> inputs;
    std::array<XboxVector, 13> outputs; // oPos 0, oD0 3, oD1 4, oFog 5, oPts 6, oB0 7, oB1 8, oT0..oT3 9..12.
};
void run_xbox_vertex_program(std::span<const u32> program, XboxVertexProgramState& state);
}
