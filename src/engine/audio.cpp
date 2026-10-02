#include "gbtg/engine_internal.hpp"

#include <chrono>
#include <cstring>

namespace gbtg
{
namespace
{

// Stream slots: names at 0x004DFFA0 (stride 0x80, six of them, ending at 0x004E02A0),
// state at 0x004E02A0, id at 0x004E02B8, timer at 0x004E02D0. Gate at 0x004E02E8.
// Banks live at 0x004DF370, stride 0x9C, twenty slots. 0x00038600 clears the slots
// (ids to -1) before the first tick.
constexpr int kXactStreamSlots = 6;
constexpr int kXactStreamName = 0x80;
constexpr int kXactBanks = 20;
constexpr int kXactChannels = 16;
constexpr int kXactChannelName = 0x82; // +0x02 through +0x83
constexpr int kXactVoices = 48;

constexpr const char* kXactSilence = "XACT_SILENCE";

struct XactStreamSlot
{
    char name[kXactStreamName] = {};
    int state = 0;
    int id = -1;
    float time = 0.f;
};

struct XactBank
{
    int id = 0;
    char name[kXactStreamName] = {};
    int channel = 0; // +0x84. Prepare writes -1 when no channel is bound.
    int refcount = 0; // +0x88
    int silence = 0;  // +0x8C. 1 when the name is XACT_SILENCE.
    int held = 0;     // +0x98
};

struct XactChannel
{
    i16 voice = 0; // +0x00. -1 means no voice.
    char name[kXactChannelName] = {}; // +0x02
    int mode = 0;  // +0x84. 3 keeps the stream slot alive.
    int owned = 0; // +0x88. Nonzero lets 0x00038C20 reap an idle voice.
};

// Sound source created by 0x0015C320. 0x000396F0 reads the cue through voice+0x1C.
struct XactSource
{
    int bound = 0;     // +0x28. Nonzero while an XACT cue is bound.
    int cue_state = 0; // Cue state from 0x0015C990. 1 means still playing.
    float position[3] = {};
    float scale[3] = {};
    int atten_mb = 0;
    float volume = 0.f;
    int applied = 0; // body+0x24. 0x0015C290 sets this after writing the curve.
};

struct XactVoice
{
    int state = 0;    // +0x00
    int stop = 0;     // +0x04. State 8 retires the source when this is nonzero.
    int follow = 0;   // +0x0C. Nonzero: refresh world position from the listener.
    float ahead = 0.f; // +0x14. Distance along listener matrix elements [7..9].
    int tracked = 0;  // +0x18. State 6 updates the source only when this is 1.
    XactSource* source = nullptr; // +0x1C
    int live = 0;     // +0x20
};

XactStreamSlot g_xact_slots[kXactStreamSlots];
XactBank g_xact_banks[kXactBanks];
XactChannel g_xact_channels[kXactChannels];
XactVoice g_xact_voices[kXactVoices];
int g_xact_gate = 0;
float g_xact_level = 0.f; // 0x004E0BCC. 0x00038C20 stores 1.0.

// Xbox 0x00421A38. Nine {millibel, volume} pairs. 0x004E13A8 picks one.
// Pair 0 is also the default tail of the 0x0015C290 call.
struct XactCurve
{
    int atten_mb;
    float volume;
};

constexpr XactCurve kXactCurve[] = {
    {0, 0.f},
    {-2800, 0.71f},
    {-5000, 0.40f},
    {-1800, 0.66f},
    {-4400, 0.64f},
    {-4000, 0.50f},
    {-5000, 0.60f},
    {-6000, 0.68f},
    {-1200, 0.15f},
};

constexpr int kXactCurveCount = 9;

int g_xact_curve = 0; // 0x004E13A8
// DAT_005077a8. Position is [0..2], quaternion xyzw is [3..6], forward is [7..9].
const float* g_xact_listener = nullptr;
float g_xact_distance = 1.f; // 0x004D97F0. Audio init stores 1.0; SetPosition scales by it.

// Values passed to the XACT listener. 0x0015BCB0 / 0x0015B7F0 / 0x0015B840.
struct XactListener
{
    float position[3] = {};
    float velocity[3] = {};
    float front[3] = {};
    float top[3] = {};
};

XactListener g_xact_ear;

// Xbox 0x004D9780 / 0x004D97EC / 0x004D9968. Engine stays null until audio init.
// Ready is 1 only after that init; CommitDeferredSettings runs only then.
void* g_xact_engine = nullptr;
int g_xact_ready = 0;
unsigned g_xact_ms = 0;
int g_xact_prepared = 0; // 0x004D9B70. Inc when a cue is prepared, dec on stop.

// Notification objects. Banks at 0x004D9808 (match +0x18, cues at +0x28).
// Waves at 0x004D97D8 (match +0x20). Cue id is +0x24.
struct XactCueInst
{
    XactCueInst* next = nullptr; // +0x04
    int id = 0;                  // +0x24
    int state = 0;               // +0x28. 0 start, 1 stop, 2 marker, 3 other.
    void (*notify)(XactCueInst* cue, int state) = nullptr; // +0x2C
    int marker = 0;              // +0x30
    int flags = 0;               // +0x34. Bit 1 drops later notes that are not stop.
    int seen = 0;                // +0x38. Bits 0, 1, 2, 3: start, stop, marker, other.
};

struct XactEngineBank
{
    XactEngineBank* next = nullptr; // +0x04
    int key = 0;                    // +0x18
    XactCueInst* cues = nullptr;    // +0x28
};

struct XactEngineWave
{
    XactEngineWave* next = nullptr; // +0x04
    int key = 0;                    // +0x20
    int state = 0;                  // +0x34. 7 when a type-3 note has no cue.
    void (*notify)(XactEngineWave* wave, int state) = nullptr; // +0x38
    int flags = 0;                  // +0x54. Bit 0x80 means that type-3 note already landed.
};

XactEngineBank* g_xact_sbanks = nullptr; // 0x004D9808
XactEngineWave* g_xact_waves = nullptr;  // 0x004D97D8

// Stand-in for IXACTEngine::GetNotification. The retail call fills one record
// per success; HRESULT nonzero ends the pump.
struct XactNote
{
    unsigned type = 0; // word at +0x00. 0 start, 1 stop, 2 marker, 3 other.
    int key = 0;       // +0x04. Bank +0x18 and wave +0x20.
    int cue_key = 0;   // +0x0C. Cue +0x24.
    int marker = 0;    // +0x18. Stored on the cue for a marker note.
};

constexpr int kXactNoteQueue = 32;
XactNote g_xact_notes[kXactNoteQueue];
int g_xact_note_count = 0;

// Xbox 0x00421C70. Passed to SetVelocity every listener update.
constexpr float kListenerVelocity[3] = {0.f, 1.f, 0.f};

void xact_copy_name(char* dst, const char* src)
{
    std::size_t i = 0;
    for (; i + 1 < kXactStreamName && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

XactBank* xact_find_bank(const char* name)
{
    // Xbox 0x00038460. First occupied bank whose name matches.
    for (XactBank& bank : g_xact_banks)
    {
        if (bank.id != 0 && std::strcmp(bank.name, name) == 0)
        {
            return &bank;
        }
    }
    return nullptr;
}

XactBank* xact_alloc_bank(int id, const char* name)
{
    // Xbox 0x000383C0 / 0x00038210. Empty bank, refcount 1, silence flag from the name.
    for (XactBank& bank : g_xact_banks)
    {
        if (bank.id != 0)
        {
            continue;
        }
        bank.id = id;
        bank.refcount = 1;
        bank.silence = std::strcmp(name, kXactSilence) == 0 ? 1 : 0;
        bank.held = 0;
        bank.channel = 0;
        xact_copy_name(bank.name, name);
        return &bank;
    }
    return nullptr;
}

void xact_acquire(const char* name, int id)
{
    // Xbox 0x00038910. Name in EAX, id on the stack. An existing bank only bumps the ref.
    // 0x000382C0 prepares a new bank. With no free channel, +0x84 is -1.
    if (XactBank* existing = xact_find_bank(name))
    {
        ++existing->refcount;
        return;
    }
    XactBank* bank = xact_alloc_bank(id, name);
    if (bank == nullptr || bank->silence != 0)
    {
        return;
    }
    bank->channel = -1;
}

// Xbox 0x00038EF0. AL is the bank channel. 0 while that channel is still playing.
int xact_channel_idle(int channel_index)
{
    if (channel_index < 0 || channel_index >= kXactChannels)
    {
        return 1;
    }
    const XactChannel& channel = g_xact_channels[channel_index];
    if (channel.mode == 3)
    {
        return 0;
    }
    const int voice = channel.voice;
    if (voice < 0 || voice >= kXactVoices || g_xact_voices[voice].live == 0)
    {
        return 1;
    }
    const int state = g_xact_voices[voice].state;
    if (state == 2 || state == 3 || state == 5 || state == 6 || state == 7)
    {
        return 0;
    }
    return 1;
}

void xact_release_slot(int index)
{
    // Xbox 0x00038810. EAX is the slot index. Id becomes -1 and the name is cleared.
    // While the slot is acquired, the bank ref drops and a zero ref frees the bank.
    XactStreamSlot& slot = g_xact_slots[index];
    slot.id = -1;
    if (slot.state == 2)
    {
        if (XactBank* bank = xact_find_bank(slot.name))
        {
            --bank->refcount;
            if (bank->refcount <= 0)
            {
                bank->id = 0;
                bank->refcount = 0;
                bank->name[0] = '\0';
            }
        }
    }
    slot.name[0] = '\0';
}

void xact_38640(float dt)
{
    // Xbox 0x00038640
    for (int i = 0; i < kXactStreamSlots; ++i)
    {
        XactStreamSlot& slot = g_xact_slots[i];
        if (slot.name[0] == '\0')
        {
            continue;
        }
        if (slot.state == 2)
        {
            const XactBank* bank = xact_find_bank(slot.name);
            int action = 1;
            if (bank != nullptr)
            {
                if (bank->silence != 0)
                {
                    action = 3;
                }
                else if (bank->held != 0 || bank->channel == -1)
                {
                    action = 2;
                }
                else if (xact_channel_idle(bank->channel) == 0)
                {
                    action = 3;
                }
                else
                {
                    action = 2;
                }
            }
            if (action == 2)
            {
                xact_release_slot(i);
            }
            else if (action == 3)
            {
                slot.time += dt;
            }
        }
        else if (g_xact_gate == 0)
        {
            xact_acquire(slot.name, slot.id);
            slot.time = 0.f;
            slot.state = 2;
        }
    }
}

void xact_38c20()
{
    // Xbox 0x00038C20. Sixteen channels at 0x004E030C, stride 0x8C.
    // Drop a channel when it still has a voice, +0x88 is set, and that voice is idle
    // (live is 0, or its state is outside 2, 3, 5, 6, 7). 0x00039690 then writes
    // state 10 if live is 0. Playing states never reach that call from here.
    for (XactChannel& channel : g_xact_channels)
    {
        const int voice_index = channel.voice;
        if (voice_index == -1)
        {
            continue;
        }
        XactVoice* voice = nullptr;
        bool idle = true;
        if (voice_index >= 0 && voice_index < kXactVoices)
        {
            voice = &g_xact_voices[voice_index];
            const int state = voice->state;
            idle = voice->live == 0
                || (state != 2 && state != 3 && state != 5 && state != 6 && state != 7);
        }
        if (!idle || channel.owned == 0)
        {
            continue;
        }
        channel.name[0] = '\0';
        if (voice != nullptr && voice->live == 0)
        {
            voice->state = 10;
        }
        channel.voice = -1;
        channel.mode = 0;
        channel.owned = 0;
    }
    g_xact_level = 1.f;
}

void xact_396f0()
{
    // Xbox 0x000396F0. Forty-eight voices at 0x004E0CE8, stride 0x24.
    // State 6 with tracked == 1 refreshes the source. State 10, and state 8
    // when stop is set, retire the source and clear the voice once the handle is gone.
    for (XactVoice& voice : g_xact_voices)
    {
        const int state = voice.state;
        if (state == 6)
        {
            if (voice.tracked != 1 || voice.source == nullptr)
            {
                continue;
            }
            XactSource& source = *voice.source;
            // 0x0015C930 writes the constant vector at 0x0042A840.
            source.scale[0] = 1.f;
            source.scale[1] = 1.f;
            source.scale[2] = 1.f;
            const int curve = (g_xact_curve >= 0 && g_xact_curve < kXactCurveCount) ? g_xact_curve : 0;
            // 0x0015C290: five leading zeros, the selected pair, then pair 0.
            source.atten_mb = kXactCurve[curve].atten_mb;
            source.volume = kXactCurve[curve].volume;
            source.applied = 1;
            if (voice.follow != 0 && g_xact_listener != nullptr)
            {
                const float* m = g_xact_listener;
                const float dist = voice.ahead;
                source.position[0] = m[7] * dist + m[0];
                source.position[1] = m[8] * dist + m[1];
                source.position[2] = m[9] * dist + m[2];
            }
            continue;
        }
        if (state == 8 && voice.stop == 0)
        {
            continue;
        }
        if (state != 8 && state != 10)
        {
            continue;
        }
        // 0x0015C9E0 audioSoundSourceDestroy. A cue still in state 1 stays bound.
        // Otherwise the cue is stopped and the voice drops the source.
        if (voice.source != nullptr && (voice.source->bound == 0 || voice.source->cue_state != 1))
        {
            voice.source->bound = 0;
            delete voice.source;
            voice.source = nullptr;
        }
        if (state != 0 && voice.source == nullptr)
        {
            voice.state = 0;
        }
    }
}

void audio_listener()
{
    // Xbox 0x0003A040. Front is the matrix axis. Top is quaternion [3..6] applied to (0, -1, 0).
    if (g_xact_listener == nullptr)
    {
        return;
    }
    const float* m = g_xact_listener;
    const float qx = m[3];
    const float qy = m[4];
    const float qz = m[5];
    const float qw = m[6];
    const float top_x = 2.f * (qz * qw - qy * qx);
    const float top_y = qx * qx + qz * qz - qy * qy - qw * qw;
    const float top_z = -2.f * (qw * qx + qy * qz);

    const float scale = g_xact_distance;
    g_xact_ear.position[0] = m[0] * scale;
    g_xact_ear.position[1] = m[1] * scale;
    g_xact_ear.position[2] = m[2] * scale;
    g_xact_ear.velocity[0] = kListenerVelocity[0];
    g_xact_ear.velocity[1] = kListenerVelocity[1];
    g_xact_ear.velocity[2] = kListenerVelocity[2];
    g_xact_ear.front[0] = m[7];
    g_xact_ear.front[1] = m[8];
    g_xact_ear.front[2] = m[9];
    g_xact_ear.top[0] = top_x;
    g_xact_ear.top[1] = top_y;
    g_xact_ear.top[2] = top_z;
}

unsigned xact_milliseconds()
{
    // Xbox 0x0015B780
    // XAPILIB::GetTickCount. KeTickCount in milliseconds, low 32 bits.
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now);
    return static_cast<unsigned>(ms.count());
}

XactEngineBank* xact_find_engine_bank(int key)
{
    // Xbox 0x0015B780
    // 0x0015CD70. First bank on 0x004D9808 whose +0x18 matches.
    for (XactEngineBank* bank = g_xact_sbanks; bank != nullptr; bank = bank->next)
    {
        if (bank->key == key)
        {
            return bank;
        }
    }
    return nullptr;
}

XactCueInst* xact_find_cue(XactEngineBank* bank, int cue_key)
{
    // Xbox 0x0015B780
    // 0x0015CD90. Cue list at bank+0x28, match +0x24.
    if (bank == nullptr)
    {
        return nullptr;
    }
    for (XactCueInst* cue = bank->cues; cue != nullptr; cue = cue->next)
    {
        if (cue->id == cue_key)
        {
            return cue;
        }
    }
    return nullptr;
}

XactEngineWave* xact_find_engine_wave(int key)
{
    // Xbox 0x0015B780
    // 0x0015D040. First wave on 0x004D97D8 whose +0x20 matches.
    for (XactEngineWave* wave = g_xact_waves; wave != nullptr; wave = wave->next)
    {
        if (wave->key == key)
        {
            return wave;
        }
    }
    return nullptr;
}

void xact_note_callback(XactCueInst* cue, XactEngineWave* wave)
{
    // Xbox 0x0015B780
    // Cue callback at +0x2C wins. A cue with no callback falls through to the wave.
    if (cue != nullptr && cue->notify != nullptr)
    {
        cue->notify(cue, cue->state);
        return;
    }
    if (wave != nullptr && wave->notify != nullptr)
    {
        wave->notify(wave, wave->state);
    }
}

void xact_apply_note(const XactNote& note)
{
    // Xbox 0x0015B780
    // Body of 0x0015D1E0 for one notification.
    XactEngineBank* bank = xact_find_engine_bank(note.key);
    XactEngineWave* wave = xact_find_engine_wave(note.key);
    XactCueInst* cue = nullptr;
    if (bank != nullptr)
    {
        cue = xact_find_cue(bank, note.cue_key);
        if (cue == nullptr)
        {
            return;
        }
        if ((cue->flags & 2) != 0 && note.type != 1)
        {
            return;
        }
    }

    if (note.type > 3)
    {
        xact_note_callback(cue, wave);
        return;
    }

    if (note.type == 0)
    {
        if (cue == nullptr || (cue->seen & 1) != 0)
        {
            return;
        }
        cue->seen |= 1;
        cue->state = 0;
    }
    else if (note.type == 1)
    {
        if (cue == nullptr || (cue->seen & 2) != 0)
        {
            return;
        }
        const int old_bit1 = (cue->flags >> 1) & 1;
        const int handle = cue->id;
        cue->flags |= 2;
        cue->seen |= 2;
        cue->state = 1;
        cue->id = 0;
        // IXACTSoundBank stop (0x001BC885): bank key, -1, flags 2, cue handle.
        // A handle of 0, or bit 1 already set on +0x34, skips the release.
        if (handle != 0 && old_bit1 == 0 && bank != nullptr)
        {
            --g_xact_prepared;
        }
    }
    else if (note.type == 2)
    {
        if (cue == nullptr || (cue->seen & 4) != 0)
        {
            return;
        }
        cue->state = 2;
        cue->marker = note.marker;
        cue->seen |= 4;
    }
    else if (cue != nullptr)
    {
        if ((cue->seen & 8) != 0)
        {
            return;
        }
        cue->seen |= 8;
        cue->state = 3;
    }
    else if (wave == nullptr || (wave->flags & 0x80) != 0)
    {
        return;
    }
    else
    {
        wave->flags |= 0x80;
        wave->state = 7;
    }
    xact_note_callback(cue, wave);
}

bool xact_take_note(XactNote& out)
{
    // Xbox 0x0015B780
    // One IXACTEngine::GetNotification record. Failure ends 0x0015D1E0.
    if (g_xact_note_count <= 0)
    {
        return false;
    }
    out = g_xact_notes[0];
    for (int i = 1; i < g_xact_note_count; ++i)
    {
        g_xact_notes[i - 1] = g_xact_notes[i];
    }
    --g_xact_note_count;
    return true;
}

void xact_dispatch_notifications()
{
    // Xbox 0x0015B780
    // 0x0015D1E0. GetNotification(engine, 0, &note) until the call fails.
    if (g_xact_engine == nullptr)
    {
        return;
    }
    XactNote note;
    while (xact_take_note(note))
    {
        xact_apply_note(note);
    }
}

void xact_commit_deferred()
{
    // Xbox 0x0015B780
    // IXACTEngine::CommitDeferredSettings(0x004D9780). Null engine returns.
    if (g_xact_engine == nullptr)
    {
        return;
    }
}

void xact_engine_dowork()
{
    // Xbox 0x0015B780
    // XACTEngineDoWork. Null engine returns.
    if (g_xact_engine == nullptr)
    {
        return;
    }
}

void audio_update()
{
    // Xbox 0x0015B780
    g_xact_ms = xact_milliseconds();
    if (g_xact_ready == 1)
    {
        xact_commit_deferred();
    }
    xact_dispatch_notifications();
    xact_engine_dowork();
}

} // namespace

void audio_tick(Game& g, float dt)
{
    // Xbox 0x0015B780, then 0x0003A040, 0x000396F0, 0x00038640, 0x00038C20.
    (void)g;
    audio_update();
    audio_listener();
    xact_396f0();
    xact_38640(dt);
    xact_38c20();
}

} // namespace gbtg
