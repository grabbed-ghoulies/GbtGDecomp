#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"
#include "background_geometry.h"
#include "background_ambient.h"
#include "ambient.h"
#include "../audio/core_source.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>

namespace gbtg
{
// 0x00118AE0 fills these from the loaded ambience. They stay zero until that
// pass, so the copies below are zero and the RNG still advances. The player
// update 0x0002C980 reads the same words as the world bounds.
u8 g_amb_volume_enabled = 0; // DAT_0050EA00
float g_amb_color_lo[3]{}; // DAT_0050ea04, +0x08, +0x0c
float g_amb_color_hi[3]{}; // DAT_0050ea10, +0x14, +0x18
Vec3 g_amb_palette[8]{}; // DAT_0050ea1c, stride 12

namespace
{

// DAT_0054dd7c. Shared LCG. The float is the mantissa of 1.0, so the unit
// value sits in [0, 1).
u32 g_amb_rng = 0;

u32 amb_lcg()
{
    // Xbox 0x00118470
    g_amb_rng = g_amb_rng * 0x19660Du + 0x3C6EF35Fu;
    return g_amb_rng;
}

float amb_rand01()
{
    // Xbox 0x00118470
    const u32 bits = (amb_lcg() & 0x7fffffu) | 0x3f800000u;
    return std::bit_cast<float>(bits) - 1.f;
}

// DAT_004df358. The 0x44-byte misc record
// aid_misc_ghoulies_audio_generalsettings. Game init loads it before cam_11c480.
struct AudioGeneralSettings
{
    float wind_lo = 12.f; // +0x1c
    float wind_hi = 20.f; // +0x20
    float step_lo = 80.f; // +0x24
    float step_hi = 90.f; // +0x28
    float span_lo = 1.f;  // +0x2c
    float span_hi = 1.5f; // +0x30
    i32 count_lo = 5;     // +0x34
    i32 count_hi = 10;    // +0x38
};

constexpr AudioGeneralSettings kAudioGeneral{};

static_assert(std::bit_cast<u32>(kAudioGeneral.wind_lo) == 0x41400000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.wind_hi) == 0x41a00000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.step_lo) == 0x42a00000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.step_hi) == 0x42b40000u);
static_assert(std::bit_cast<u32>(kAudioGeneral.span_hi) == 0x3fc00000u);
static_assert(std::bit_cast<u32>(120.f) == 0x42f00000u);
static_assert(std::bit_cast<u32>(300.f) == 0x43960000u);
static_assert(std::bit_cast<u32>(1.5f) == 0x3fc00000u);

// Wind and rain bed at DAT_0050e7a0. The two words are voice indices into
// the table at 0x004E0CE8 (stride 0x24). 0xFFFF means no voice.
i32 g_amb_counter = 0;     // DAT_0050e7a0. cam_11c480 increments this after the call.
i32 g_amb_hold = 0;        // DAT_0050e7a4
i32 g_amb_armed = 0;       // DAT_0050e7a8
float g_amb_wait = 0.f;    // DAT_0050e7ac
float g_amb_wait_scale = 0.f; // DAT_0050e7b0. Pool float DAT_0032b400 (120).
float g_amb_span = 0.f;    // DAT_0050e7b4. Pool float DAT_0032b180 (300).
float g_amb_level = 0.f;   // DAT_0050e7b8
float g_amb_blend = 0.f;   // DAT_0050e7bc
float g_amb_fade = 0.f;    // DAT_0050e7c0
float g_amb_pitch = 0.f;   // DAT_0050e7c4
float g_amb_goal = 0.f;    // DAT_0050e7c8
float g_amb_rate = 0.f;    // DAT_0050e7cc. Pool float DAT_0032b210 (1.5).
i32 g_amb_gate = 0;        // DAT_0050e7d0
u16 g_amb_rain = 0xFFFF;   // DAT_0050e7d4. XACT_SOUNDBANK_GENVIRONMENT_RAINEXTRA.
u8 g_amb_rain_vol = 0;     // DAT_0050e7d6
u16 g_amb_wind = 0xFFFF;   // DAT_0050e7d8. XACT_SOUNDBANK_GENVIRONMENT_WINDEXTRA.
u8 g_amb_wind_vol = 0;     // DAT_0050e7da
i32 g_amb_mark = 0;        // DAT_0050e7dc
u16 g_amb_extra = 0xFFFF;  // DAT_0050e7e0
float g_amb_extra_wait = 0.f; // DAT_0050e7e4
i32 g_amb_extra_index = 0; // DAT_0050e7e8
i32 g_amb_extra_prev = 0;  // DAT_0050e7ec
Vec3 g_amb_extra_color{};  // DAT_0050e7f0

float g_amb_step_wait = 0.f;  // DAT_0050e7fc
float g_amb_step_timer = 0.f; // DAT_0050e800
float g_amb_step_span = 0.f;  // DAT_0050e804
i32 g_amb_step_left = 0;      // DAT_0050e808
i32 g_amb_step_count = 0;     // DAT_0050e80c
Vec3 g_amb_step_from{};       // DAT_0050e810
Vec3 g_amb_step_to{};         // DAT_0050e81c
Vec3 g_amb_step_now{};        // DAT_0050e828

// Phase block. 0x001185E0 advances DAT_0050e968 under the two gates.
i32 g_amb_count = 0;          // DAT_0050e930
Vec3 g_amb_emitters[21]{}; // DAT_0050E834..DAT_0050E92F.
Vec3 g_amb_vary_point{}; // DAT_0050E938.
float g_amb_light_color[4]{}; // DAT_00509B3C.
Vec3 g_amb_phase_point{}; // DAT_0050E958.
float g_amb_model_matrix_scratch[12]{}; // DAT_004DD798.
i32 g_amb_vary = 0;           // DAT_0050e944
i32 g_amb_vary_wait = 0;      // DAT_0050E94C, frames to the next 0x001185E0 draw.
float g_amb_vary_scale = 0.f; // DAT_0050e948
i32 g_amb_phase_on = 0;       // DAT_0050e950
i32 g_amb_phase_gate = 0;     // DAT_0050e954
float g_amb_phase_a = 0.f;    // DAT_0050e964
float g_amb_phase = 0.f;      // DAT_0050e968
float g_amb_phase_len = 0.f;  // DAT_0050e96c
float g_amb_phase_jitter = 0.f; // DAT_0050e970. Pool float DAT_0032b268.
float g_amb_phase_rate = 0.f; // DAT_0050e974
u32 g_amb_phase_color = 0;    // DAT_0050e978. Dword at DAT_00314f4c.
char g_amb_phase_name[0x80]{}; // DAT_0050E97C, the phase sound 0x001189D0 copies in.

// 0x00039690. A live voice in states 2..7 is moved toward stop. The voice
// table is the XACT block, which this file does not hold. The init writes
// 0xFFFF first, so the call is skipped.
void amb_release(u16& handle)
{
    // Xbox 0x00118470
    if (handle == 0xFFFF)
    {
        return;
    }
    handle = 0xFFFF;
}

// 0x00117980. One unit random times the 120 scale, then the bed floats drop
// to zero. A live wind or rain voice is released and the word becomes 0xFFFF.
void amb_reset_beds()
{
    // Xbox 0x00118470
    g_amb_wait = amb_rand01() * g_amb_wait_scale;
    g_amb_armed = 0;
    g_amb_level = 0.f;
    g_amb_pitch = 0.f;
    g_amb_blend = 0.f;
    g_amb_fade = 0.f;
    g_amb_goal = 0.f;
    amb_release(g_amb_wind);
    amb_release(g_amb_rain);
}

// rand <= 0.5 keeps the low endpoint. COMISS of 0.5 against the unit random.
float amb_endpoint(float lo, float hi)
{
    // Xbox 0x00118470
    if (0.5f >= amb_rand01())
    {
        return lo;
    }
    return hi;
}

// 0x00118D10. ECX is the colour. trunc(rand * 3) picks the axis that is
// lerped; the other two take one endpoint. EAX returns the byte at
// DAT_0050ea00, which 0x00118290 ignores.
void amb_pick_color(Vec3& out)
{
    // Xbox 0x00118470
    const float* lo = g_amb_color_lo;
    const float* hi = g_amb_color_hi;
    const i32 choice = static_cast<i32>(amb_rand01() * 3.f);
    if (choice == 0)
    {
        const float t = amb_rand01();
        out.x = (hi[0] - lo[0]) * t + lo[0];
        out.y = amb_endpoint(lo[1], hi[1]);
        out.z = amb_endpoint(lo[2], hi[2]);
    }
    else if (choice == 1)
    {
        out.x = amb_endpoint(lo[0], hi[0]);
        const float t = amb_rand01();
        out.y = (hi[1] - lo[1]) * t + lo[1];
        out.z = amb_endpoint(lo[2], hi[2]);
    }
    else if (choice == 2)
    {
        out.x = amb_endpoint(lo[0], hi[0]);
        out.y = amb_endpoint(lo[1], hi[1]);
        const float t = amb_rand01();
        out.z = (hi[2] - lo[2]) * t + lo[2];
    }
}

// 0x00117F20. Lerps the step delay, span, and count out of the general
// settings, then copies two different palette rows (stride 12, eight rows).
void amb_sample_step()
{
    // Xbox 0x00118470
    const AudioGeneralSettings& settings = kAudioGeneral;
    g_amb_step_wait = (settings.step_hi - settings.step_lo) * amb_rand01() + settings.step_lo;
    const float span = (settings.span_hi - settings.span_lo) * amb_rand01() + settings.span_lo;
    g_amb_step_span = span;
    g_amb_step_timer = span;
    const float count = static_cast<float>(settings.count_hi - settings.count_lo) * amb_rand01()
        + static_cast<float>(settings.count_lo);
    const i32 count_i = static_cast<i32>(count);
    g_amb_step_count = count_i;
    g_amb_step_left = count_i;
    const i32 first = static_cast<i32>(amb_rand01() * 8.f);
    i32 second = first;
    do
    {
        second = static_cast<i32>(amb_rand01() * 8.f);
    }
    while (second == first);
    g_amb_step_from = g_amb_palette[first];
    g_amb_step_now = g_amb_palette[first];
    g_amb_step_to = g_amb_palette[second];
}

// 0x00118290. Lerps the wind-extra delay, then draws a cue index in [3, 11)
// that differs from the previous one. DAT_0050e7f0 is filled by 0x00118D10.
// The voice word is cleared to 0xFFFF.
void amb_sample_extra()
{
    // Xbox 0x00118470
    const AudioGeneralSettings& settings = kAudioGeneral;
    g_amb_extra_wait = (settings.wind_hi - settings.wind_lo) * amb_rand01() + settings.wind_lo;
    const i32 previous = g_amb_extra_prev;
    i32 index = previous;
    do
    {
        index = static_cast<i32>(amb_rand01() * 8.f + 3.f);
        g_amb_extra_index = index;
    }
    while (index == previous);
    g_amb_extra_prev = index;
    amb_pick_color(g_amb_extra_color);
    amb_release(g_amb_extra);
    g_amb_extra = 0xFFFF;
}

// DAT_0050eaa0, Xbox stride 0x2c, eight records. DAT_0050ec00 is the count
// that follows them. 0x00119160 clears only the object pointer at +0.
struct LookupRecord
{
    void* object = nullptr; // Xbox +0
};

constexpr i32 kLookupRecords = 8;

// Index i (12 bits) becomes A8R8G8B8 with R = low nibble, G = middle, B = high.
// ((i >> 4 & 0xf) | (i & 0xf) << 8) << 8 | (i >> 8 & 0xf)
constexpr u32 lookup_texel(u32 index)
{
    // Xbox 0x00119160
    const u32 lo = index & 0xfu;
    const u32 mid = (index >> 4) & 0xfu;
    const u32 hi = (index >> 8) & 0xfu;
    return (lo << 16) | (mid << 8) | hi;
}

static_assert(lookup_texel(0) == 0u);
static_assert(lookup_texel(1) == 0x00010000u);
static_assert(lookup_texel(0x10) == 0x00000100u);
static_assert(lookup_texel(0x100) == 0x00000001u);
static_assert(lookup_texel(0x123) == 0x00030201u);
static_assert(lookup_texel(0xfff) == 0x000f0f0fu);

LookupRecord g_lookup_records[kLookupRecords]{};
i32 g_lookup_count = 0; // DAT_0050ec00
u32* g_lookup_tex = nullptr; // DAT_0050ec04. 4096 A8R8G8B8 texels.

} // namespace

RuntimeBackgroundAmbient runtime_background_ambient()
{
    return {g_amb_emitters, g_amb_count, g_amb_vary_point, g_amb_vary,
        g_amb_vary_scale, g_amb_light_color, g_amb_phase_on, g_amb_phase_point,
        g_amb_model_matrix_scratch};
}

// Xbox 0x00118AE0
void initialize_background_volume(const Vec3& low, const Vec3& high)
{
    g_amb_volume_enabled = 1;
    g_amb_color_lo[0] = low.x - 25.f; // DAT_0032B170 = 0x41C80000
    g_amb_color_lo[1] = low.y - 25.f;
    g_amb_color_lo[2] = low.z - 25.f;
    g_amb_color_hi[0] = high.x + 25.f;
    g_amb_color_hi[1] = high.y + 25.f;
    g_amb_color_hi[2] = high.z + 25.f;
    g_amb_palette[0] = low;
    g_amb_palette[1] = Vec3{high.x, low.y, low.z};
    g_amb_palette[2] = Vec3{low.x, high.y, low.z};
    g_amb_palette[3] = Vec3{high.x, high.y, low.z};
    g_amb_palette[4] = Vec3{low.x, low.y, high.z};
    g_amb_palette[5] = Vec3{high.x, low.y, high.z};
    g_amb_palette[6] = Vec3{low.x, high.y, high.z};
    g_amb_palette[7] = high;
}

// Xbox 0x00118CB0, ECX low, EAX high
u8 read_background_volume(Vec3& low, Vec3& high)
{
    low = Vec3{g_amb_color_lo[0], g_amb_color_lo[1], g_amb_color_lo[2]}; // DAT_0050EA04
    high = Vec3{g_amb_color_hi[0], g_amb_color_hi[1], g_amb_color_hi[2]}; // DAT_0050EA10
    return g_amb_volume_enabled; // DAT_0050EA00
}

// Xbox 0x00118470
void ambient_initialize_beds()
{
    // Xbox 0x00118470
    g_amb_wait_scale = 120.f;
    g_amb_span = 300.f;
    g_amb_rate = 1.5f;
    g_amb_wind_vol = 0x7f;
    g_amb_rain_vol = 0x7f;
    g_amb_vary_scale = 0.f;
    g_amb_phase = 0.f;
    g_amb_phase_a = 0.f;
    g_amb_counter = 0;
    g_amb_hold = 0;
    g_amb_mark = 0;
    g_amb_gate = 0;
    g_amb_wind = 0xFFFF;
    g_amb_rain = 0xFFFF;
    g_amb_count = 0;
    g_amb_vary = 0;
    g_amb_phase_on = 0;
    g_amb_phase_gate = 0;
    g_amb_phase_len = 1.f;
    g_amb_phase_jitter = std::bit_cast<float>(0x3e99999au);
    g_amb_phase_rate = 1.f;
    g_amb_phase_color = 0xff000000u;
    g_amb_phase_name[0] = 0;
    g_amb_extra = 0xFFFF;
    g_amb_extra_prev = -1;
    amb_reset_beds();
    amb_sample_step();
    amb_sample_extra();
}

// Xbox 0x00119160
void ambient_initialize_lookup_texture()
{
    // Xbox 0x00119160
    // D3DDevice_CreateTexture2(0x1000, 1, 1, 1, 0, D3DFMT_A8R8G8B8, 3).
    // GetLevelDesc's 0x1c-byte result is unused. LockRect then writes 4096
    // texels. XGSwizzleRect(bits, 0, NULL, bits, 0x1000, 1, NULL, 4) runs
    // in place; a one-row NV2A swizzle leaves that order unchanged.
    constexpr u32 kWidth = 0x1000;
    u32* texels = static_cast<u32*>(std::malloc(kWidth * sizeof(u32)));
    g_lookup_tex = texels;
    if (texels != nullptr)
    {
        for (u32 i = 0; i < kWidth; ++i)
        {
            texels[i] = lookup_texel(i);
        }
    }
    for (LookupRecord& rec : g_lookup_records)
    {
        rec.object = nullptr;
    }
    g_lookup_count = 0;
}


// Xbox 0x001189D0, ECX gate, XMM0 length, EAX the name
void start_ambient_phase(u32 gate, float length, float jitter, float rate, u32 color, const char* name,
    AudioEventVoices& voices)
{
    g_amb_phase_len = length; // DAT_0050E96C
    g_amb_phase_jitter = jitter; // DAT_0050E970
    g_amb_phase_rate = rate; // DAT_0050E974
    g_amb_phase_gate = i32(gate); // DAT_0050E954
    g_amb_phase = 0.f; // DAT_0050E968
    g_amb_phase_color = color; // DAT_0050E978
    std::strcpy(g_amb_phase_name, name);
    if (gate == 0 || g_amb_phase_name[0] == 0)
    {
        return;
    }
    if (voices.listener == nullptr)
    {
        throw Error("0x001189D0 has no DAT_005077A8 view");
    }
    // The view's position (+0) plus its forward (+0x1C).
    const Vec3 position{voices.listener[7] + voices.listener[0], voices.listener[8] + voices.listener[1],
        voices.listener[9] + voices.listener[2]};
    const i16 index = allocate_audio_event_voice(1, voices); // 0x00039830, EBX 1
    if (index == -1)
    {
        return;
    }
    AudioEventVoice& voice = voices.voices[size_t(index)];
    voice.stop = 1; // DAT_004E0CEC
    if (voice.source != nullptr) // DAT_004E0D04
    {
        set_audio_core_source_position(*voice.source, position, voices.core.sources); // 0x0015C800
    }
    prepare_audio_event_voice(index, g_amb_phase_name, voices); // 0x00039A20, EAX the index
}

namespace
{
[[noreturn]] void ambient_unbound(const char* what)
{
    throw Error(std::string("ambient tick: ") + what + " is not translated");
}

// Xbox 0x00117A50: the wind and rain bed. Its wait arms the bed (or 0x00117980 resets it); an armed bed's body runs
// past here.
void tick_ambient_bed(float step)
{
    if (0.f < g_amb_wait && (g_amb_wait = g_amb_wait - step, g_amb_wait <= 0.f))
    {
        g_amb_wait = 0.f;
        if (g_amb_armed == 0)
        {
            g_amb_wait = g_amb_span; // DAT_0050E7B4
            g_amb_pitch = std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124
            g_amb_goal = -1.f; // DAT_0032B120
            g_amb_armed = 1;
            g_amb_level = 0.f;
            g_amb_blend = 0.f;
            g_amb_fade = 0.f;
            g_amb_gate = 0;
            ambient_unbound("0x00117A50's armed bed (the view's wind and rain voices)");
        }
        amb_reset_beds(); // 0x00117980
    }
    if (g_amb_armed == 0)
    {
        return;
    }
    ambient_unbound("0x00117A50's armed bed (the view's wind and rain voices)");
}

// Xbox 0x00118120: the random ambient steps.
void tick_ambient_steps(float step)
{
    if (g_amb_step_wait <= 0.f)
    {
        if (0.f < g_amb_step_timer && (g_amb_step_timer = g_amb_step_timer - step, g_amb_step_timer <= 0.f))
        {
            ambient_unbound("0x00118120's step (0x00118800 and XACT_SOUNDBANK_GRANDOMAMBIENCE_AMBSTEP)");
        }
        return;
    }
    g_amb_step_wait = g_amb_step_wait - step;
    if (g_amb_step_wait <= 0.f)
    {
        g_amb_step_wait = 0.f;
    }
}

// Xbox 0x00118380: the extra wind voice.
void tick_ambient_extra(float step)
{
    if (g_amb_extra == 0xFFFF)
    {
        if (0.f < g_amb_extra_wait && (g_amb_extra_wait = g_amb_extra_wait - step, g_amb_extra_wait <= 0.f))
        {
            ambient_unbound("0x00118380's extra wind voice (0x00118800, 0x00039B10)");
        }
        return;
    }
    ambient_unbound("0x00118380's playing extra wind voice (0x00118290)");
}
}

// Xbox 0x001185E0
void tick_ambient(float step, u32& random, AudioEventVoices& voices)
{
    tick_ambient_bed(step); // 0x00117A50
    tick_ambient_steps(step); // 0x00118120
    tick_ambient_extra(step); // 0x00118380
    if (g_amb_vary != 0 && (g_amb_vary_wait = g_amb_vary_wait - 1, g_amb_vary_wait < 1))
    {
        random = random * 0x19660Du + 0x3C6EF35Fu;
        g_amb_vary_scale = (std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f)
            * std::bit_cast<float>(0x3E99999Au) + std::bit_cast<float>(0x3F333333u); // DAT_0032B268, DAT_0032B264
        g_amb_vary_wait = 2;
    }
    if (g_amb_phase_on != 0 && g_amb_phase_gate != 0)
    {
        g_amb_phase = g_amb_phase + step; // DAT_0050E968
        if (g_amb_phase_len < g_amb_phase)
        {
            g_amb_phase = g_amb_phase - g_amb_phase_len;
            if (g_amb_phase_name[0] != 0)
            {
                if (voices.listener == nullptr)
                {
                    throw Error("0x00039480 has no DAT_005077A8 view");
                }
                // 0x00039480 with XMM3 1: the view's position plus its forward.
                const Vec3 position{voices.listener[7] + voices.listener[0], voices.listener[8] + voices.listener[1],
                    voices.listener[9] + voices.listener[2]};
                request_positioned_audio_event_voice(g_amb_phase_name, 1, position, voices); // 0x00039580
            }
        }
        const float half_turn = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148
        float angle = half_turn;
        if (g_amb_phase_len != 0.f)
        {
            const float place = (g_amb_phase / g_amb_phase_len) * half_turn;
            if (place <= half_turn)
            {
                angle = place < 0.f ? 0.f : place;
            }
        }
        const long double sine = std::fabs(retail_sin(angle)); // 0x00153320, 0x00152BB0
        g_amb_phase_a = float((long double)(g_amb_phase_rate - g_amb_phase_jitter) * sine + g_amb_phase_jitter);
    }
    float level;
    if (g_amb_vary == 0 && g_amb_phase_on == 0)
    {
        level = (g_amb_hold != 0 || g_amb_counter != 0 || g_amb_count < 1) ? 0.f : g_amb_level;
    }
    else
    {
        level = g_amb_phase_a + g_amb_vary_scale;
    }
    g_amb_light_color[2] = level; // DAT_00509B44
    g_amb_light_color[3] = 1.f; // DAT_00509B48
    g_amb_light_color[1] = level; // DAT_00509B40
    g_amb_light_color[0] = level; // DAT_00509B3C
}

} // namespace gbtg
