#include "footstep_names.h"
#include "sound_names.h"
#include "bank_lookup.h"

namespace gbtg
{
// Xbox 0x0013DE80
void format_footstep_sound_name(char* output, u8 surface, const char* suffix,
    const char* movement, const FootstepSoundNames& context)
{
    if (context.remap[surface] != 0)
    {
        surface = context.remap[surface];
    }
    format_sound_name(output, "XACT_SOUNDBANK_GFOOTSTEP%s_%s%s",
        context.surfaces[surface], movement, suffix);
    if (output[0] != 0 && resolve_audio_bank_cue(output, nullptr, nullptr))
    {
        return;
    }
    format_sound_name(output, "XACT_SOUNDBANK_GFOOTSTEP%s_default", context.surfaces[surface]);
}
}
