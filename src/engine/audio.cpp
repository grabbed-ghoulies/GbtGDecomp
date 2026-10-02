#include "gbtg/engine_internal.hpp"

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

struct XactVoice
{
    int state = 0; // +0x00
    int live = 0;  // +0x20
};

XactStreamSlot g_xact_slots[kXactStreamSlots];
XactBank g_xact_banks[kXactBanks];
XactChannel g_xact_channels[kXactChannels];
XactVoice g_xact_voices[kXactVoices];
int g_xact_gate = 0;
float g_xact_level = 0.f; // 0x004E0BCC. 0x00038C20 stores 1.0.

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

} // namespace

void audio_tick(Game& g, float dt)
{
    // Xbox 0x0015B780 audio(dt).
    // Xbox 0x0003A040 listener.
    // Xbox 0x000396F0 / 0x00038C20 XACT.
    (void)g;
    xact_38640(dt);
    xact_38c20();
}

} // namespace gbtg
