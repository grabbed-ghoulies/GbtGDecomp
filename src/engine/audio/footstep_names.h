#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct FootstepSoundNames
{
    std::span<const u8> remap; // DAT_00314FBC
    std::span<const char* const> surfaces; // PTR_DAT_004340A0
};
void format_footstep_sound_name(char* output, u8 surface, const char* suffix,
    const char* movement, const FootstepSoundNames& context); // Xbox 0x0013DE80
}
