#include "vertex_shader_code.h"

namespace gbtg
{
// Xbox 0x0018850C
u8 runtime_vertex_shader_code_type(const u16* code)
{
    const u16 signature = *code;
    if (signature == 0x2078)
    {
        return 1;
    }
    else if (signature == 0x7378)
    {
        return 3;
    }
    return static_cast<u8>(((static_cast<u32>(signature == 0x7778) - 1) & 0xFD) + 2);
}
// Xbox 0x00186FD5
void unpack_runtime_vertex_instruction(const u32* input, u32* output)
{
    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    const u32 last = input[3];
    const u32 middle = input[2];
    output[2] = (input[1] << 4) | (middle >> 28);
    output[1] = ((middle & 0x0FFFFFFF) << 2) | (last >> 30) | (output[1] & 0xC0000000);
    output[0] = output[0] ^ ((output[0] ^ last) & 0x3FFFFFFF);
}
}
