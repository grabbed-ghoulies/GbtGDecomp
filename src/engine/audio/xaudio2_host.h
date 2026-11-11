#pragma once

// The XAudio2, X3DAudio and XAudio2 reverb API that pcm_out.cpp drives.
//
// Windows includes the SDK headers. Elsewhere FAudio implements the same engine with binary-identical structs
// and constants but a C interface, so this header gives its engine and voices XAudio2's methods. Each voice
// object owns its FAudio voice; DestroyVoice releases both, as XAudio2's does.

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#define GBTG_HAS_XAUDIO2 1

#elif defined(GBTG_AUDIO_FAUDIO)

#include <F3DAudio.h>
#include <FAPO.h>
#include <FAudio.h>
#include <FAudioFX.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#define GBTG_HAS_XAUDIO2 1

using HRESULT = std::int32_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using UINT32 = std::uint32_t;
using BOOL = std::int32_t;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define SUCCEEDED(hr) (static_cast<HRESULT>(hr) >= 0)
#define FAILED(hr) (static_cast<HRESULT>(hr) < 0)
#define S_OK static_cast<HRESULT>(0)
#define E_FAIL static_cast<HRESULT>(0x80004005u)
#define E_INVALIDARG static_cast<HRESULT>(0x80070057u)

// The Win32 last-error slot pcm_out's host wave loading reports through, and the codes it uses.
#define ERROR_SUCCESS 0u
#define ERROR_INVALID_DATA 13u
#define ERROR_NOT_SUPPORTED 50u
#define ERROR_NOT_FOUND 1168u
inline thread_local DWORD g_xaudio2_host_last_error = 0;
inline DWORD GetLastError()
{
    return g_xaudio2_host_last_error;
}
inline void SetLastError(DWORD error)
{
    g_xaudio2_host_last_error = error;
}
// The XACT core's debug strings; there is no attached debugger to receive them.
inline void OutputDebugStringA(const char* text)
{
    std::fputs(text, stderr);
}
inline HRESULT HRESULT_FROM_WIN32(DWORD error)
{
    return error == 0 ? S_OK : static_cast<HRESULT>((error & 0xFFFFu) | 0x80070000u);
}

using WAVEFORMATEX = FAudioWaveFormatEx;
using XAUDIO2_BUFFER = FAudioBuffer;
using XAUDIO2_VOICE_STATE = FAudioVoiceState;
using XAUDIO2_VOICE_DETAILS = FAudioVoiceDetails;
using XAUDIO2FX_REVERB_PARAMETERS = FAudioFXReverbParameters;
using XAUDIO2FX_REVERB_I3DL2_PARAMETERS = FAudioFXReverbI3DL2Parameters;
using X3DAUDIO_HANDLE = F3DAUDIO_HANDLE;
using X3DAUDIO_VECTOR = F3DAUDIO_VECTOR;
using X3DAUDIO_LISTENER = F3DAUDIO_LISTENER;
using X3DAUDIO_EMITTER = F3DAUDIO_EMITTER;
using X3DAUDIO_DSP_SETTINGS = F3DAUDIO_DSP_SETTINGS;

#define XAUDIO2_DEFAULT_PROCESSOR FAUDIO_DEFAULT_PROCESSOR
#define XAUDIO2_END_OF_STREAM FAUDIO_END_OF_STREAM
#define XAUDIO2_LOOP_INFINITE FAUDIO_LOOP_INFINITE
#define XAUDIO2_MAX_LOOP_COUNT FAUDIO_MAX_LOOP_COUNT
#define XAUDIO2_PLAY_TAILS FAUDIO_PLAY_TAILS
#define XAUDIO2_SEND_USEFILTER FAUDIO_SEND_USEFILTER
using XAUDIO2_FILTER_PARAMETERS = FAudioFilterParameters;
inline constexpr FAudioFilterType LowPassOnePoleFilter = FAudioLowPassOnePoleFilter;
#define X3DAUDIO_SPEED_OF_SOUND 343.5f // x3daudio.h's value; F3DAudio takes it as an argument only.
#define WAVE_FORMAT_PCM FAUDIO_FORMAT_PCM
#define X3DAUDIO_CALCULATE_MATRIX F3DAUDIO_CALCULATE_MATRIX

// FAudio's reverb is an FAPO with C function pointers; this holds one reference the way an IUnknown does.
class IUnknown
{
  public:
    explicit IUnknown(FAPO* effect) : effect_(effect)
    {
    }
    FAPO* faudio() const
    {
        return effect_;
    }
    void Release()
    {
        effect_->Release(effect_);
        delete this;
    }

  private:
    FAPO* effect_;
};

inline HRESULT XAudio2CreateReverb(IUnknown** effect)
{
    FAPO* created = nullptr;
    if (FAudioCreateReverb(&created, 0) != 0 || created == nullptr)
    {
        *effect = nullptr;
        return E_FAIL;
    }
    *effect = new IUnknown(created);
    return S_OK;
}

class IXAudio2Voice;

struct XAUDIO2_SEND_DESCRIPTOR
{
    UINT32 Flags;
    IXAudio2Voice* pOutputVoice;
};

struct XAUDIO2_VOICE_SENDS
{
    UINT32 SendCount;
    XAUDIO2_SEND_DESCRIPTOR* pSends;
};

struct XAUDIO2_EFFECT_DESCRIPTOR
{
    IUnknown* pEffect;
    BOOL InitialState;
    UINT32 OutputChannels;
};

struct XAUDIO2_EFFECT_CHAIN
{
    UINT32 EffectCount;
    XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors;
};

class IXAudio2Voice
{
  public:
    explicit IXAudio2Voice(FAudioVoice* voice) : voice_(voice)
    {
    }
    IXAudio2Voice(const IXAudio2Voice&) = delete;
    IXAudio2Voice& operator=(const IXAudio2Voice&) = delete;
    virtual ~IXAudio2Voice() = default;

    FAudioVoice* faudio() const
    {
        return voice_;
    }
    void GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details)
    {
        FAudioVoice_GetVoiceDetails(voice_, details);
    }
    HRESULT SetOutputVoices(const XAUDIO2_VOICE_SENDS* sends)
    {
        if (sends == nullptr)
        {
            return HRESULT(FAudioVoice_SetOutputVoices(voice_, nullptr));
        }
        std::vector<FAudioSendDescriptor> descriptors(sends->SendCount);
        for (UINT32 index = 0; index < sends->SendCount; ++index)
        {
            descriptors[index].Flags = sends->pSends[index].Flags;
            descriptors[index].pOutputVoice = sends->pSends[index].pOutputVoice->faudio();
        }
        const FAudioVoiceSends list{sends->SendCount, descriptors.data()};
        return HRESULT(FAudioVoice_SetOutputVoices(voice_, &list));
    }
    HRESULT SetEffectChain(const XAUDIO2_EFFECT_CHAIN* chain)
    {
        if (chain == nullptr)
        {
            return HRESULT(FAudioVoice_SetEffectChain(voice_, nullptr));
        }
        std::vector<FAudioEffectDescriptor> descriptors(chain->EffectCount);
        for (UINT32 index = 0; index < chain->EffectCount; ++index)
        {
            descriptors[index].pEffect = chain->pEffectDescriptors[index].pEffect->faudio();
            descriptors[index].InitialState = chain->pEffectDescriptors[index].InitialState;
            descriptors[index].OutputChannels = chain->pEffectDescriptors[index].OutputChannels;
        }
        const FAudioEffectChain list{chain->EffectCount, descriptors.data()};
        return HRESULT(FAudioVoice_SetEffectChain(voice_, &list));
    }
    HRESULT SetEffectParameters(UINT32 index, const void* parameters, UINT32 size, UINT32 operation = 0)
    {
        return HRESULT(FAudioVoice_SetEffectParameters(voice_, index, parameters, size, operation));
    }
    HRESULT SetVolume(float volume, UINT32 operation = 0)
    {
        return HRESULT(FAudioVoice_SetVolume(voice_, volume, operation));
    }
    void GetVolume(float* volume)
    {
        FAudioVoice_GetVolume(voice_, volume);
    }
    HRESULT SetOutputMatrix(IXAudio2Voice* destination, UINT32 source_channels, UINT32 destination_channels,
        const float* levels, UINT32 operation = 0)
    {
        return HRESULT(FAudioVoice_SetOutputMatrix(voice_, destination != nullptr ? destination->faudio() : nullptr,
            source_channels, destination_channels, levels, operation));
    }
    void GetOutputMatrix(IXAudio2Voice* destination, UINT32 source_channels, UINT32 destination_channels,
        float* levels)
    {
        FAudioVoice_GetOutputMatrix(voice_, destination != nullptr ? destination->faudio() : nullptr,
            source_channels, destination_channels, levels);
    }
    HRESULT SetOutputFilterParameters(IXAudio2Voice* destination, const XAUDIO2_FILTER_PARAMETERS* parameters,
        UINT32 operation = 0)
    {
        return HRESULT(FAudioVoice_SetOutputFilterParameters(voice_,
            destination != nullptr ? destination->faudio() : nullptr, parameters, operation));
    }
    void DestroyVoice()
    {
        FAudioVoice_DestroyVoice(voice_);
        delete this;
    }

  private:
    FAudioVoice* voice_;
};

class IXAudio2SourceVoice : public IXAudio2Voice
{
  public:
    using IXAudio2Voice::IXAudio2Voice;
    HRESULT Start(UINT32 flags = 0, UINT32 operation = 0)
    {
        return HRESULT(FAudioSourceVoice_Start(faudio(), flags, operation));
    }
    HRESULT Stop(UINT32 flags = 0, UINT32 operation = 0)
    {
        return HRESULT(FAudioSourceVoice_Stop(faudio(), flags, operation));
    }
    HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER* buffer, const void* wma = nullptr)
    {
        (void)wma;
        return HRESULT(FAudioSourceVoice_SubmitSourceBuffer(faudio(), buffer, nullptr));
    }
    HRESULT FlushSourceBuffers()
    {
        return HRESULT(FAudioSourceVoice_FlushSourceBuffers(faudio()));
    }
    void GetState(XAUDIO2_VOICE_STATE* state, UINT32 flags = 0)
    {
        FAudioSourceVoice_GetState(faudio(), state, flags);
    }
};

class IXAudio2SubmixVoice : public IXAudio2Voice
{
  public:
    using IXAudio2Voice::IXAudio2Voice;
};

class IXAudio2MasteringVoice : public IXAudio2Voice
{
  public:
    using IXAudio2Voice::IXAudio2Voice;
    HRESULT GetChannelMask(DWORD* mask)
    {
        return HRESULT(FAudioMasteringVoice_GetChannelMask(faudio(), mask));
    }
};

class IXAudio2
{
  public:
    explicit IXAudio2(FAudio* audio) : audio_(audio)
    {
    }
    HRESULT CreateSourceVoice(IXAudio2SourceVoice** voice, const WAVEFORMATEX* format, UINT32 flags = 0,
        float max_frequency_ratio = FAUDIO_DEFAULT_FREQ_RATIO)
    {
        FAudioSourceVoice* created = nullptr;
        const HRESULT status = HRESULT(
            FAudio_CreateSourceVoice(audio_, &created, format, flags, max_frequency_ratio, nullptr, nullptr, nullptr));
        *voice = SUCCEEDED(status) ? new IXAudio2SourceVoice(created) : nullptr;
        return status;
    }
    HRESULT CreateSubmixVoice(IXAudio2SubmixVoice** voice, UINT32 channels, UINT32 sample_rate, UINT32 flags = 0,
        UINT32 stage = 0)
    {
        FAudioSubmixVoice* created = nullptr;
        const HRESULT status = HRESULT(
            FAudio_CreateSubmixVoice(audio_, &created, channels, sample_rate, flags, stage, nullptr, nullptr));
        *voice = SUCCEEDED(status) ? new IXAudio2SubmixVoice(created) : nullptr;
        return status;
    }
    HRESULT CreateMasteringVoice(IXAudio2MasteringVoice** voice)
    {
        FAudioMasteringVoice* created = nullptr;
        const HRESULT status = HRESULT(FAudio_CreateMasteringVoice(audio_, &created, FAUDIO_DEFAULT_CHANNELS,
            FAUDIO_DEFAULT_SAMPLERATE, 0, 0, nullptr));
        *voice = SUCCEEDED(status) ? new IXAudio2MasteringVoice(created) : nullptr;
        return status;
    }
    HRESULT StartEngine()
    {
        return HRESULT(FAudio_StartEngine(audio_));
    }
    void StopEngine()
    {
        FAudio_StopEngine(audio_);
    }
    void Release()
    {
        FAudio_Release(audio_);
        delete this;
    }

  private:
    FAudio* audio_;
};

inline HRESULT XAudio2Create(IXAudio2** engine, UINT32 flags, UINT32 processor)
{
    FAudio* audio = nullptr;
    const HRESULT status = HRESULT(FAudioCreate(&audio, flags, processor));
    *engine = SUCCEEDED(status) ? new IXAudio2(audio) : nullptr;
    return status;
}

inline HRESULT X3DAudioInitialize(UINT32 speaker_mask, float speed_of_sound, X3DAUDIO_HANDLE handle)
{
    return HRESULT(F3DAudioInitialize8(speaker_mask, speed_of_sound, handle));
}

inline void X3DAudioCalculate(const X3DAUDIO_HANDLE handle, const X3DAUDIO_LISTENER* listener,
    const X3DAUDIO_EMITTER* emitter, UINT32 flags, X3DAUDIO_DSP_SETTINGS* settings)
{
    F3DAudioCalculate(handle, listener, emitter, flags, settings);
}

#else
#define GBTG_HAS_XAUDIO2 0
#endif
