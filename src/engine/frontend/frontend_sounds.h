#pragma once
#include "../audio/bank_streams.h"

namespace gbtg
{
class SceneExecutableResources;
// The game-select pause ditty: a tracked bank stream held while the slot widget is open.
struct FrontendPauseDitty
{
    u32 playing = 0; // DAT_00429BB8, .data 0.
    std::array<char, 0x80> name{"XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE"}; // DAT_00429B38, .data.
};
extern FrontendPauseDitty g_pause_ditty;
void start_pause_ditty(FrontendPauseDitty& ditty, AudioBankStreams& streams); // Xbox 0x000E5B40
void stop_pause_ditty(FrontendPauseDitty& ditty, AudioBankStreams& streams); // Xbox 0x000E5B70
// Picks one of the Baron's start-button laughs (names at 0x00314D68, stride -0x80, scaled by DAT_0032B100) and
// plays it as a 2D event voice that stops itself. Returns the voice, or -1.
i16 play_start_button_laugh(u32& random, const SceneExecutableResources& image,
    AudioEventVoices& voices); // Xbox 0x0013BAF0
}
