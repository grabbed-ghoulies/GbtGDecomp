#include "frontend_sounds.h"
#include "../scene/executable_resources.h"

#include <bit>

namespace gbtg
{
FrontendPauseDitty g_pause_ditty;

// Xbox 0x000E5B40
void start_pause_ditty(FrontendPauseDitty& ditty, AudioBankStreams& streams)
{
    if (ditty.playing == 0)
    {
        acquire_tracked_audio_bank(ditty.name.data(), 1, streams);
        ditty.playing = 1;
    }
}

// Xbox 0x000E5B70
void stop_pause_ditty(FrontendPauseDitty& ditty, AudioBankStreams& streams)
{
    if (ditty.playing != 0)
    {
        // 0x00038970 inline.
        release_tracked_audio_bank(ditty.name.data(), streams);
        ditty.playing = 0;
    }
}

// Xbox 0x0013BAF0
i16 play_start_button_laugh(u32& random, const SceneExecutableResources& image, AudioEventVoices& voices)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
    const i16 index = allocate_audio_event_voice(0, voices);
    if (index != -1)
    {
        const float one = 1.f; // DAT_0032B0F8.
        const float scale = 0.f; // DAT_0032B100: retail scales by zero, so the first laugh always plays.
        const i32 pick = static_cast<i32>((unit - one) * scale);
        voices.voices[size_t(index)].stop = 1;
        prepare_audio_event_voice(index, image.string(0x00314D68u - u32(pick) * 0x80u), voices);
    }
    return index;
}
}
