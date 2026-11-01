#pragma once

#include "render_optimize.h"

#include "gbtg/types.h"

#include <algorithm>
#include <span>

#if GBTG_OPTIMIZE
namespace gbtg
{

// GBTG_OPTIMIZE step 10: the registers of `vc` a translated program reads (xbox_vertex_hlsl: an operand of kind 3
// reads the instruction's constant index, plus the address register when the relative bit is set). Every
// constant-kind operand counts, whether or not its MAC/ILU operation uses it, and any relative read takes the whole
// file. The Vulkan backend copies only this range per draw; the render thread (step 11) queues only this range.
inline void program_constant_range(std::span<const u32> program, u32& first, u32& count)
{
    u32 lowest = 192;
    u32 highest = 0;
    for (size_t n = 1; n + 3 < program.size(); n += 4)
    {
        const u32* w = program.data() + n;
        const bool constant = ((w[2] >> 26) & 3) == 3 || ((w[2] >> 11) & 3) == 3 || ((w[3] >> 28) & 3) == 3;
        if (constant)
        {
            const u32 index = (w[1] >> 13) & 255;
            if ((w[3] & 2) != 0 || index >= 192)
            {
                first = 0;
                count = 192;
                return;
            }
            lowest = std::min(lowest, index);
            highest = std::max(highest, index);
        }
        if ((w[3] & 1) != 0)
        {
            break;
        }
    }
    first = lowest > highest ? 0 : lowest;
    count = lowest > highest ? 0 : highest - lowest + 1;
}

} // namespace gbtg
#endif
