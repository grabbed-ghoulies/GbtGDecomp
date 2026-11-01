#pragma once

#include "core_cue.h"
#include "listener_settings.h"
#include "native_audio.h"

namespace gbtg
{
struct Game;
AudioListenerSettings native_audio_listener_settings();
// The one XACT category-volume and reverb state (DAT_004D9798..) the boot and the scene services share.
AudioListenerSettings& native_audio_listener();
// Host heap (DAT_004D9784 / DAT_004D9788) and XACT sound sources as XAudio2 submix voices.
AudioCoreMemory native_audio_core_memory();
AudioCoreSourceBoundaries native_audio_core_source_boundaries();
// Bank/cue indices resolve through the scene's native view of the retail bank.
// Prepared handles are live IXAudio2SourceVoice pointers, owned by pcm_out.
AudioCoreCueBoundaries native_audio_core_cue_boundaries(const Game& game,
    const std::function<const char*(void*, u32)>& cue_name);
// Boot 0x0003A280 over the host: bundle AIDs, the XAudio2 device and the wave bank index. Fills g_native_audio.
std::shared_ptr<NativeAudioRuntime> boot_native_audio(const Game& game);
#if defined(GBTG_INSPECTOR_APP)
// Host tooling lifecycle: release boot-owned bank allocations after every scene/cue owner is gone.
void release_native_audio_boot();
// Host session lifecycle. Stop mixing while inspecting; destroy voices before their PCM buffers.
void suspend_pcm_audio();
void shutdown_pcm_audio();
// Host tooling playback isolation. These operate only on prepared native cue handles.
void begin_pcm_inspection_preview(void* cue, float volume);
void pcm_set_inspection_output_volume(float volume); // Preview master gain; tests use 10%.
void release_pcm_inspection_preview(void* cue);
void volume_pcm_inspection_preview(void* cue, float volume);
bool pcm_inspection_preview_pending(void* cue, float& seconds);
float pcm_inspection_output_volume(); // Actual mastering-voice gain, including preview isolation.
#endif
// The XACT listener calls (position, velocity, orientation) over the host's X3DAudio listener.
void bind_native_audio_listener(AudioCoreListener& listener);
// The cue notifications XACTEngineDoWork delivers (0x0015B780): each live native cue's XAudio2 voice is polled
// and its callback receives 3 once prepared, 0 once playing and 1 when its buffers have drained.
void dispatch_native_audio_notifications(std::span<AudioCoreBank* const> banks);
// The frame's audio in 0x00033B50: the cue notifications 0x0015B780, the listener 0x0003A040, the event voices
// 0x000396F0, the stream slots 0x00038640 (gated by DAT_004E02E8) and the idle channel release 0x00038C20.
void tick_native_audio(float dt, i32 stream_gate);
}
