#include "background_morphs.h"

namespace gbtg
{
// Xbox 0x00114050
void set_runtime_background_morphs_playing(RuntimeBackgroundMorphs* morphs, u8 playing)
{
    if (morphs == nullptr)
    {
        return;
    }
    for (auto& record : morphs->records)
    {
        record.playing = playing;
    }
}
}
