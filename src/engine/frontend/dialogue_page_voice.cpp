#include "dialogue_sound_tags.h"
#include "../audio/event_voices.h"

namespace gbtg
{
// Xbox 0x00057D10
void request_dialogue_page_voice(const DialogueSoundTagState& state,
    AudioEventVoices& voices)
{
    const i16 index = allocate_audio_event_voice(0, voices);
    if (index != -1)
    {
        voices.voices[index].stop = 1;
        prepare_audio_event_voice(index, state.cue.data(), voices);
    }
}
}
