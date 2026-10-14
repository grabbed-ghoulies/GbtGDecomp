#pragma once

#include "core_cue.h"
#include "listener_settings.h"
#include "native_audio.h"

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
// Boot 0x0003A280 over the host: bundle AIDs, the XAudio2 device and the wave bank index. Fills g_native_audio.
void boot_native_audio(const Game& game);
// The XACT listener calls (position, velocity, orientation) over the host's X3DAudio listener.
void bind_native_audio_listener(AudioCoreListener& listener);
// The cue notifications XACTEngineDoWork delivers (0x0015B780): each live native cue's XAudio2 voice is polled
// and its callback receives 3 once prepared, 0 once playing and 1 when its buffers have drained.
void dispatch_native_audio_notifications(std::span<AudioCoreBank* const> banks);
}
