#pragma once

#include "core_cue.h"
#include "listener_settings.h"

namespace gbtg
{
struct Game;
AudioListenerSettings native_audio_listener_settings();
// Host heap (DAT_004D9784 / DAT_004D9788) and XACT sound sources as XAudio2 submix voices.
AudioCoreMemory native_audio_core_memory();
AudioCoreSourceBoundaries native_audio_core_source_boundaries();
// Bank/cue indices resolve through the scene's native view of the retail bank.
// Prepared handles are live IXAudio2SourceVoice pointers, owned by pcm_out.
AudioCoreCueBoundaries native_audio_core_cue_boundaries(const Game& game,
    const std::function<const char*(void*, u32)>& cue_name);
}
