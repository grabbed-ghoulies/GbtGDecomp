#pragma once
#include "gbtg/types.h"
#include "../audio/event_voices.h"

namespace gbtg
{
extern u8 g_amb_volume_enabled; // DAT_0050EA00.
extern float g_amb_color_lo[3]; // DAT_0050EA04..DAT_0050EA0C.
extern float g_amb_color_hi[3]; // DAT_0050EA10..DAT_0050EA18.
// Script type 0x8E's ambient phase: its gate, length, jitter, rate and colour, and the sound started ahead of the
// view when the gate is set and the name is not empty.
void start_ambient_phase(u32 gate, float length, float jitter, float rate, u32 color, const char* name,
    AudioEventVoices& voices); // Xbox 0x001189D0, ECX gate, XMM0 length, EAX the name
// The scene tick's ambience for the step: the wind and rain bed, the random steps, the extra wind voice, the
// varying light scale, the phase and its sound, and the base light colour (DAT_00509B3C) they give.
void tick_ambient(float step, u32& random, AudioEventVoices& voices); // Xbox 0x001185E0
}
