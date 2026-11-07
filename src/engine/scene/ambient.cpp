#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include "background_geometry.h"
#include "background_ambient.h"
#include "ambient.h"
#include "../runtime/random_state.h"
#include "executable_resources.h"
#include "scene.h"
#include "../transitions/scene_transitions.h"
#include "../audio/core_source.h"
#include "../scripts/script_actions.h"

#include <array>
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

// The game's shared random word (DAT_0054DD7C), stepped inline as each ambience function does. The float is the
// mantissa of 1.0, so the unit value sits in [0, 1).
u32 amb_lcg()
{
    g_shared_random = g_shared_random * 0x19660Du + 0x3C6EF35Fu;
    return g_shared_random;
}

float amb_rand01()
{
    const u32 bits = (amb_lcg() & 0x7fffffu) | 0x3f800000u;
    return std::bit_cast<float>(bits) - 1.f;
}

// DAT_004DF358: the misc record aid_misc_ghoulies_audio_generalsettings, which boot 0x00033030 acquires after the
// audio boot (0x0003A280).
std::span<const u8> g_audio_general_settings;

namespace audio_general_settings
{
using wind_lo = RetailField<float, 0x1C>;
using wind_hi = RetailField<float, 0x20>;
using step_lo = RetailField<float, 0x24>;
using step_hi = RetailField<float, 0x28>;
using span_lo = RetailField<float, 0x2C>;
using span_hi = RetailField<float, 0x30>;
using count_lo = RetailField<i32, 0x34>;
using count_hi = RetailField<i32, 0x38>;
using thunder_wait_lo = RetailField<float, 0x3C>; // 0x00117A50's next flash delay
using thunder_wait_hi = RetailField<float, 0x40>;
}

std::span<const u8> audio_general()
{
    if (g_audio_general_settings.data() == nullptr)
    {
        throw Error("DAT_004DF358 (aid_misc_ghoulies_audio_generalsettings) is not loaded");
    }
    return g_audio_general_settings;
}

static_assert(std::bit_cast<u32>(120.f) == 0x42f00000u);
static_assert(std::bit_cast<u32>(300.f) == 0x43960000u);
static_assert(std::bit_cast<u32>(1.5f) == 0x3fc00000u);

// Wind and rain bed at DAT_0050e7a0. The two words are voice indices into
// the table at 0x004E0CE8 (stride 0x24). 0xFFFF means no voice.
// DAT_0050e7a0 is g_scene_active_count (scene_transitions): cam_11c480 increments it after the call.
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
// DAT_0050E924: the bed's sound point, the emitter nearest the view (0x001188C0) or the view. It is the last of the
// 21 slots above, as in retail, which no bound keeps the emitter count from reaching.
Vec3& g_amb_bed_point = g_amb_emitters[20];
float g_amb_bed_scale = 0.f; // DAT_0050E934, the bed's distance scale.
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

// A live voice word is stopped (0x00039690 with stack 0) and becomes 0xFFFF. The boot init (0x00118470) writes
// 0xFFFF first, so it has no voices to pass.
void amb_release(u16& handle, AudioEventVoices* voices)
{
    if (handle == 0xFFFF)
    {
        return;
    }
    if (voices == nullptr)
    {
        throw Error("0x00039690 has no DAT_004E0CE8 voices for an ambient voice word");
    }
    stop_audio_event_voice(i16(handle), 0, *voices); // 0x00039690
    handle = 0xFFFF;
}

// One unit random times the 120 scale, then the bed floats drop to zero. A live wind or rain voice is stopped and
// its word becomes 0xFFFF.
// Xbox 0x00117980
void amb_reset_beds(AudioEventVoices* voices)
{
    g_amb_wait = amb_rand01() * g_amb_wait_scale;
    g_amb_armed = 0;
    g_amb_level = 0.f;
    g_amb_pitch = 0.f;
    g_amb_blend = 0.f;
    g_amb_fade = 0.f;
    g_amb_goal = 0.f;
    amb_release(g_amb_wind, voices); // DAT_0050E7D8
    amb_release(g_amb_rain, voices); // DAT_0050E7D4
}

// rand <= 0.5 keeps the low endpoint. COMISS of 0.5 against the unit random.
float amb_endpoint(float lo, float hi)
{
    if (0.5f >= amb_rand01())
    {
        return lo;
    }
    return hi;
}

// 0x00118D10. ECX is the colour. trunc(rand * 3) picks the axis that is
// lerped; the other two take one endpoint. EAX returns the byte at
// DAT_0050ea00, which 0x00118290 ignores.
// Xbox 0x00118D10
void amb_pick_color(Vec3& out)
{
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
// Xbox 0x00117F20
void amb_sample_step()
{
    namespace settings = audio_general_settings;
    const std::span<const u8> general = audio_general(); // DAT_004DF358
    const float step_lo = get(general, settings::step_lo{});
    g_amb_step_wait = (get(general, settings::step_hi{}) - step_lo) * amb_rand01() + step_lo;
    const float span_lo = get(general, settings::span_lo{});
    const float span = (get(general, settings::span_hi{}) - span_lo) * amb_rand01() + span_lo;
    g_amb_step_span = span;
    g_amb_step_timer = span;
    const i32 count_lo = get(general, settings::count_lo{});
    const float count = static_cast<float>(get(general, settings::count_hi{}) - count_lo) * amb_rand01()
        + static_cast<float>(count_lo);
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
// A live voice word is stopped and cleared to 0xFFFF.
// Xbox 0x00118290
void amb_sample_extra(AudioEventVoices* voices)
{
    namespace settings = audio_general_settings;
    const std::span<const u8> general = audio_general(); // DAT_004DF358
    const float wind_lo = get(general, settings::wind_lo{});
    g_amb_extra_wait = (get(general, settings::wind_hi{}) - wind_lo) * amb_rand01() + wind_lo;
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
    amb_release(g_amb_extra, voices); // 0x00039690
    g_amb_extra = 0xFFFF;
}

} // namespace

CutsceneCameraImpulse ambient_flash_state()
{
    return {g_amb_blend, g_amb_fade, g_amb_goal, g_amb_rate, g_amb_gate};
}

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

// Xbox 0x00118560
void reset_ambient_scene(AudioEventVoices& voices)
{
    g_amb_vary_scale = 0.f; // DAT_0050E948
    g_amb_phase = 0.f; // DAT_0050E968
    g_amb_phase_a = 0.f; // DAT_0050E964
    g_amb_count = 0; // DAT_0050E930
    g_amb_vary = 0; // DAT_0050E944
    g_amb_phase_on = 0; // DAT_0050E950
    g_amb_phase_gate = 0; // DAT_0050E954
    g_amb_phase_len = 1.f; // DAT_0032B0F8
    g_amb_phase_jitter = std::bit_cast<float>(0x3E99999Au); // DAT_0032B268
    g_amb_phase_rate = 1.f;
    g_amb_phase_name[0] = 0; // DAT_0050E97C
    amb_sample_step(); // 0x00117F20
    amb_sample_extra(&voices); // 0x00118290
    g_amb_extra_wait = 1.5f; // DAT_0032B210, DAT_0050E7E4
}

// Xbox 0x00033030, after 0x0003A280
void load_audio_general_settings(AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    const std::span<const u8> resource =
        acquire_animation_resource(registry, "aid_misc_ghoulies_audio_generalsettings"); // 0x00035E90
    if (resource.data() == nullptr)
    {
        resource_error(error, 2); // 0x00032700
    }
    g_audio_general_settings = resource; // DAT_004DF358
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
    g_amb_wait_scale = 120.f;
    g_amb_span = 300.f;
    g_amb_rate = 1.5f;
    g_amb_wind_vol = 0x7f;
    g_amb_rain_vol = 0x7f;
    g_amb_vary_scale = 0.f;
    g_amb_phase = 0.f;
    g_amb_phase_a = 0.f;
    g_scene_active_count = 0; // DAT_0050E7A0
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
    amb_reset_beds(nullptr); // 0x00117980
    amb_sample_step(); // 0x00117F20
    amb_sample_extra(nullptr); // 0x00118290
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
// The emitter nearest the point (EBX out) and its distance; 0 when the scene has none.
// Xbox 0x001188C0, ECX the point, EBX the nearest emitter
u32 find_nearest_ambient_emitter(const Vec3& point, float* distance, Vec3& nearest)
{
    if (g_amb_count == 0) // DAT_0050E930
    {
        return 0;
    }
    i32 best = 0;
    float best_square = (point.z - g_amb_emitters[0].z) * (point.z - g_amb_emitters[0].z)
        + (point.y - g_amb_emitters[0].y) * (point.y - g_amb_emitters[0].y)
        + (point.x - g_amb_emitters[0].x) * (point.x - g_amb_emitters[0].x);
    for (i32 index = 1; index < g_amb_count; ++index)
    {
        const Vec3& emitter = g_amb_emitters[index];
        const float square = (point.z - emitter.z) * (point.z - emitter.z) + (point.y - emitter.y) * (point.y - emitter.y)
            + (point.x - emitter.x) * (point.x - emitter.x);
        if (square < best_square)
        {
            best = index;
            best_square = square;
        }
    }
    nearest = g_amb_emitters[best];
    if (distance != nullptr)
    {
        *distance = float(retail_square_root(best_square)); // 0x00152B30
    }
    return 1;
}

// The bed runs: nothing holds it (DAT_0050E7A4), nothing overlays the scene (DAT_0050E7A0) and the scene has emitters.
bool ambient_bed_live()
{
    return g_amb_hold == 0 && g_scene_active_count == 0 && 0 < g_amb_count;
}

// The bed's progress through its wait (DAT_0050E7AC), 0 at the start and 1 at the end, over the armed span or the
// idle scale. 0x001175E0, 0x001176C0 and 0x001177A0 each inline it.
float ambient_bed_progress()
{
    const float span = g_amb_armed != 0 ? g_amb_span : g_amb_wait_scale; // DAT_0050E7B4, DAT_0050E7B0
    if (span == 0.f)
    {
        return 1.f; // DAT_0032B0F8
    }
    const float progress = (g_amb_wait - span) / (0.f - span);
    if (progress > 1.f)
    {
        return 1.f;
    }
    if (0.f > progress)
    {
        return 0.f;
    }
    return progress;
}

float ambient_bed_unit(float value)
{
    if (value > 1.f)
    {
        return 1.f;
    }
    if (0.f > value)
    {
        return 0.f;
    }
    return value;
}

// The wind level: up over the first half of the wait and down over the second.
// Xbox 0x001175E0
float ambient_bed_wind_level()
{
    if (!ambient_bed_live())
    {
        return 0.f;
    }
    const float progress = ambient_bed_progress();
    const float curve = 0.5f > progress ? progress * 2.f // DAT_0032B0F4, DAT_0032B110
        : (progress - 1.f) * -2.f; // DAT_0032B14C
    return g_amb_bed_scale * ambient_bed_unit(curve);
}

// The thunder level: up from 0.2 to 0.5 of the wait and down from 0.5 to 0.8.
// Xbox 0x001176C0
float ambient_bed_thunder_level()
{
    if (!ambient_bed_live())
    {
        return 0.f;
    }
    const float progress = ambient_bed_progress();
    const float curve = 0.5f > progress
        ? (progress - std::bit_cast<float>(0x3E4CCCCDu)) * std::bit_cast<float>(0x40555555u) // DAT_0032B1AC, DAT_0032B3B4
        : (progress - std::bit_cast<float>(0x3F4CCCCDu)) * std::bit_cast<float>(0xC0555555u); // DAT_0032B244, DAT_0032B408
    return g_amb_bed_scale * ambient_bed_unit(curve);
}

// The rain level: up from 0.3 to 0.5 of the wait and down from 0.5 to 0.7.
// Xbox 0x001177A0
float ambient_bed_rain_level()
{
    if (!ambient_bed_live())
    {
        return 0.f;
    }
    const float progress = ambient_bed_progress();
    const float curve = 0.5f > progress
        ? (progress - std::bit_cast<float>(0x3E99999Au)) * std::bit_cast<float>(0x40A00001u) // DAT_0032B268, DAT_0032B3A8
        : (progress - std::bit_cast<float>(0x3F333333u)) * std::bit_cast<float>(0xC0A00001u); // DAT_0032B264, DAT_0032B404
    return g_amb_bed_scale * ambient_bed_unit(curve);
}

// A bed voice at the bed's point: stopped when its level is not above zero or it is switched off, otherwise started
// (once) and moved to the point.
// Xbox 0x00117880, EAX off, EBX the point, EDI the voice word, XMM0 the level
void update_ambient_bed_voice(const char* name, u32 off, const Vec3& point, u16& word, float level,
    AudioEventVoices& voices)
{
    i16 index = i16(word);
    if (!(level > 0.f) || off != 0) // DAT_0032B100
    {
        if (index != -1)
        {
            stop_audio_event_voice(index, 0, voices); // 0x00039690
            word = 0xFFFF;
        }
        return;
    }
    if (index == -1)
    {
        index = request_positioned_audio_event_voice(name, 0, point, voices); // 0x00039580
        if (index == -1)
        {
            word = u16(index);
            return;
        }
    }
    if (AudioCoreSource* source = voices.voices[size_t(index)].source; source != nullptr) // DAT_004E0D04
    {
        set_audio_core_source_position(*source, point, voices.core.sources); // 0x0015C800
    }
    word = u16(index);
}

// Xbox 0x00117900
void play_ambient_thunder(const Vec3& point, AudioEventVoices& voices)
{
    if (ambient_bed_live())
    {
        request_positioned_audio_event_voice("XACT_SOUNDBANK_GENVIRONMENT_THUNDER", 1, point, voices); // 0x00039580
    }
}

// A flash: its thunder lands at the rate times the level, kept within [1/15, 1] of the flash.
// Xbox 0x00117930, EAX the gate
void start_ambient_flash(float level, i32 gate)
{
    g_amb_goal = g_amb_rate * level; // DAT_0050E7C8, DAT_0050E7CC
    g_amb_fade = 1.f; // DAT_0032B0F8
    g_amb_blend = 1.f;
    if (1.f < g_amb_goal)
    {
        g_amb_goal = 1.f;
    }
    else if (g_amb_goal < std::bit_cast<float>(0x3D888889u)) // DAT_0032B128
    {
        g_amb_goal = std::bit_cast<float>(0x3D888889u);
    }
    g_amb_gate = gate; // DAT_0050E7D0
}

// The countdown helper: true on the step that brings a running wait to zero.
// Xbox 0x0012B2A0, EAX the wait
u32 ambient_wait_expired(float& wait, float step)
{
    if (0.f < wait && (wait = wait - step, wait <= 0.f))
    {
        wait = 0.f;
        return 1;
    }
    return 0;
}

// The flash light level over the flash (DAT_0030F6C8): eight (time, level) keys, searched and lerped.
float ambient_flash_light(float along, const SceneExecutableResources& image)
{
    std::array<float, 16> keys;
    std::memcpy(keys.data(), image.memory(0x0030F6C8, sizeof(keys)).data(), sizeof(keys));
    i32 low = 0;
    i32 high = 7;
    float key_high = std::bit_cast<float>(0x3C23D70Au); // DAT_0032B124
    float key_low = std::bit_cast<float>(0x3F7D70A4u); // DAT_0032B228
    if (key_high > along)
    {
        low = 7;
        high = 0;
    }
    else if (along > key_low)
    {
        low = 7;
        high = 0;
    }
    else
    {
        do
        {
            const i32 middle = (high - low) / 2 + low;
            if (along < keys[size_t(middle) * 2])
            {
                high = middle;
            }
            else
            {
                low = middle;
            }
        }
        while (low + 1 < high);
        key_low = keys[size_t(low) * 2];
        key_high = keys[size_t(high) * 2];
    }
    const float value_high = keys[size_t(high) * 2 + 1];
    const float value_low = keys[size_t(low) * 2 + 1];
    if (key_high == key_low)
    {
        return value_high;
    }
    const float value = (along - key_low) / (key_high - key_low) * (value_high - value_low) + value_low;
    if (value_high > value_low)
    {
        if (value > value_high)
        {
            return value_high;
        }
        if (value_low > value)
        {
            return value_low;
        }
        return value;
    }
    if (value_high > value)
    {
        return value_high;
    }
    if (value > value_low)
    {
        return value_low;
    }
    return value;
}

// The wind and rain bed. Its wait arms the bed for its span (or 0x00117980 resets it); an armed bed scales its
// wind, rain and thunder by the view's distance to the nearest emitter, and flashes the light with thunder.
// Xbox 0x00117A50
void tick_ambient_bed(float step, AudioEventVoices& voices, const SceneExecutableResources& image)
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
        }
        else
        {
            amb_reset_beds(&voices); // 0x00117980
        }
    }
    if (g_amb_armed == 0)
    {
        return;
    }
    if (voices.listener == nullptr)
    {
        throw Error("0x00117A50 has no DAT_005077A8 view");
    }
    const Vec3 view{voices.listener[0], voices.listener[1], voices.listener[2]};
    float scale;
    if (find_nearest_ambient_emitter(view, &g_amb_bed_scale, g_amb_bed_point) == 0) // 0x001188C0
    {
        g_amb_bed_point = view;
        scale = 0.75f; // DAT_0032B2AC
    }
    else if (g_amb_bed_scale > 100.f) // DAT_0032B12C
    {
        scale = 0.5f; // DAT_0032B0F4
    }
    else if (0.f > g_amb_bed_scale)
    {
        scale = 1.f; // DAT_0032B0F8
    }
    else
    {
        scale = 1.f - g_amb_bed_scale * std::bit_cast<float>(0x3C23D70Au) * 0.5f; // DAT_0032B124
        if (0.5f > scale)
        {
            scale = 0.5f;
        }
        else if (scale > 1.f)
        {
            scale = 1.f;
        }
    }
    g_amb_bed_scale = scale;
    const float thunder = ambient_bed_thunder_level(); // 0x001176C0
    const u32 wind_off = (ambient_bed_live() && (g_flag_ea98 & 4) == 0) ? 0 : 1; // DAT_0050EA98
    update_ambient_bed_voice("XACT_SOUNDBANK_GENVIRONMENT_WINDEXTRA", wind_off, g_amb_bed_point, g_amb_wind,
        ambient_bed_wind_level(), voices); // 0x00117880, 0x001175E0, the name at 0x0030F148
    const u32 rain_off = (ambient_bed_live() && (g_flag_ea98 & 2) == 0) ? 0 : 1;
    update_ambient_bed_voice("XACT_SOUNDBANK_GENVIRONMENT_RAINEXTRA", rain_off, g_amb_bed_point, g_amb_rain,
        ambient_bed_rain_level(), voices); // 0x00117880, 0x001177A0, the name at 0x0030F1C8
    const float fade = g_amb_fade; // DAT_0050E7C0
    if (!(g_amb_fade > 0.f))
    {
        if (0.f < g_amb_pitch && 0.f < thunder && ambient_wait_expired(g_amb_pitch, step) != 0) // 0x0012B2A0
        {
            start_ambient_flash(thunder, 0); // 0x00117930
        }
        return;
    }
    g_amb_fade = g_amb_fade - step;
    bool ended = false;
    if (!(0.f < g_amb_fade))
    {
        g_amb_fade = 0.f;
        ended = true;
    }
    if (!(g_amb_goal < g_amb_fade) && fade > g_amb_goal)
    {
        play_ambient_thunder(g_amb_bed_point, voices); // 0x00117900
    }
    if (ended)
    {
        if (g_amb_gate != 0)
        {
            g_amb_gate = 0;
            g_amb_level = 0.f;
            return;
        }
        namespace settings = audio_general_settings;
        const std::span<const u8> general = audio_general(); // DAT_004DF358
        const float wait_lo = get(general, settings::thunder_wait_lo{});
        const float wait_hi = get(general, settings::thunder_wait_hi{});
        g_amb_pitch = (wait_hi - wait_lo) * amb_rand01() + wait_lo; // DAT_0050E7C4
        g_amb_level = 0.f;
        return;
    }
    float along = 1.f;
    if (g_amb_blend != 0.f)
    {
        const float place = (g_amb_fade - g_amb_blend) / (0.f - g_amb_blend);
        if (!(place > 1.f))
        {
            along = 0.f > place ? 0.f : place;
        }
    }
    g_amb_level = ambient_flash_light(along, image); // DAT_0050E7B8
}

// Xbox 0x00118800: an ambient sound may start: the scene runs (state 1 or 2), nothing overlays it (DAT_0050E7A0) and
// the environment bank (id 2) is loaded.
bool ambient_voice_allowed(AudioBankStreams& banks)
{
    const i32 state = i32(read_u32({g_scene_record, 0x108}, 0x104)); // DAT_00510F5C
    if (0 < state && state < 3 && g_scene_active_count == 0)
    {
        if (const AudioTrackedBank* bank = find_tracked_audio_bank_by_id(2, banks); bank != nullptr) // 0x000384F0
        {
            return bank->pending == 0; // +0x94
        }
    }
    return false;
}

// Xbox 0x00118120: the random ambient steps, walked from one corner of the background volume to another.
void tick_ambient_steps(float step, AudioBankStreams& banks)
{
    if (g_amb_step_wait <= 0.f)
    {
        if (0.f < g_amb_step_timer && (g_amb_step_timer = g_amb_step_timer - step, g_amb_step_timer <= 0.f))
        {
            --g_amb_step_left;
            g_amb_step_timer = g_amb_step_span;
            const float along = 1.f - float(g_amb_step_left) / float(g_amb_step_count - 1); // DAT_0032B0F8
            if (ambient_voice_allowed(banks)) // 0x00118800
            {
                g_amb_step_now = Vec3{(g_amb_step_to.x - g_amb_step_from.x) * along + g_amb_step_from.x,
                    (g_amb_step_to.y - g_amb_step_from.y) * along + g_amb_step_from.y,
                    (g_amb_step_to.z - g_amb_step_from.z) * along + g_amb_step_from.z};
                request_positioned_audio_event_voice("XACT_SOUNDBANK_GRANDOMAMBIENCE_AMBSTEP", 1, g_amb_step_now,
                    banks.voices); // 0x00039580, the name at 0x0030F708
            }
            if (g_amb_step_left == 0)
            {
                amb_sample_step(); // 0x00117F20
            }
        }
        return;
    }
    g_amb_step_wait = g_amb_step_wait - step;
    if (g_amb_step_wait <= 0.f)
    {
        g_amb_step_wait = 0.f;
    }
}

// Xbox 0x00118380: the extra wind voice, at a point on the background volume (DAT_0050E7F0); once it has ended the
// next one is drawn (0x00118290).
void tick_ambient_extra(float step, AudioBankStreams& banks, const SceneExecutableResources& image)
{
    auto& voices = banks.voices;
    if (g_amb_extra == 0xFFFF)
    {
        if (0.f < g_amb_extra_wait && (g_amb_extra_wait = g_amb_extra_wait - step, !(0.f < g_amb_extra_wait)))
        {
            g_amb_extra_wait = 0.f;
            if (!ambient_voice_allowed(banks)) // 0x00118800
            {
                amb_sample_extra(&voices); // 0x00118290
                return;
            }
            const i16 voice = allocate_audio_event_voice(1, voices); // 0x00039830, EBX 1
            g_amb_extra = u16(voice);
            if (voice != -1)
            {
                if (AudioCoreSource* source = voices.voices[size_t(voice)].source; source != nullptr) // 0x00039B10
                {
                    set_audio_core_source_position(*source, g_amb_extra_color, voices.core.sources); // 0x0015C800
                }
                prepare_audio_event_voice(voice, image.string(0x0030F148 + u32(g_amb_extra_index) * 0x80),
                    voices); // 0x00039A20, WINDEXTRA and its variants
            }
        }
        return;
    }
    const AudioEventVoice& playing = voices.voices[size_t(i16(g_amb_extra))];
    if (playing.cue == nullptr || (playing.state != 2 && playing.state != 3 && playing.state != 5
        && playing.state != 6 && playing.state != 7))
    {
        amb_sample_extra(&voices); // 0x00118290
    }
}
}

// Xbox 0x001185E0
void tick_ambient(float step, u32& random, AudioBankStreams& banks, const SceneExecutableResources& image)
{
    AudioEventVoices& voices = banks.voices;
    tick_ambient_bed(step, voices, image); // 0x00117A50
    tick_ambient_steps(step, banks); // 0x00118120
    tick_ambient_extra(step, banks, image); // 0x00118380
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
        level = (g_amb_hold != 0 || g_scene_active_count != 0 || g_amb_count < 1) ? 0.f : g_amb_level;
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
