#pragma once

#include "gbtg/types.h"

#include <array>

namespace gbtg
{

struct PerformanceCounterState
{
    std::int64_t baseline = 0; // DAT_0050E0D8 / DAT_0050E0DC
    std::int64_t frequency = 0; // DAT_0050E0E0
};
// Host QueryPerformanceCounter / QueryPerformanceFrequency boundaries.
std::int64_t host_performance_counter();
void initialize_performance_counter(PerformanceCounterState& counter);
long double performance_counter_seconds(const PerformanceCounterState& counter); // Xbox 0x001077D0
void wait_performance_counter(const PerformanceCounterState& counter, float seconds); // Xbox 0x00032680

// Xbox fs_init 0x00033030 / pad 0x000FC030 / frame_time 0x00033C80.
// NTSC 60 Hz unless pal; dt is clamped to 5/fps. Delayed callbacks live in 20
// slots of 12 bytes at 0x004DEA64..0x004DEB54.
constexpr u32 kDelayedCallbackSlots = 20;
constexpr u8 kIoBusy = 0xEA; // game_init_wait_39d50 / game_init_32630

struct DelayedCallback
{
    u32 frames_left = 0;
    void* ctx = nullptr;
    void (*fn)(void*) = nullptr;
};

struct Clock
{
    bool pal = false;
    float dt = 1.f / 60.f;     // DAT_005096C0, used by audio / systems
    float dt_raw = 1.f / 60.f; // DAT_005096B8, accumulated by frame_time
    float time = 0.f;          // DAT_004DEA60
    u32 frame = 0;             // DAT_004DEA58
    u32 frame2 = 0;            // DAT_004DEA5C
    i32 ticks300 = 0;          // DAT_005096C4
    float acc300 = 0.f;        // DAT_005096C8
    std::array<DelayedCallback, kDelayedCallbackSlots> delayed{};
};

int clock_fps(const Clock& c);
void clock_init(Clock& c);                         // 0x00033030 fragment
void clock_pad(Clock& c, float incoming_seconds);  // 0x000FC030
void clock_frame_time(Clock& c);                   // 0x00033C80
bool clock_schedule(Clock& c, u32 frames, void* ctx, void (*fn)(void*));

// Xbox 0x00032630 / 0x00039D50: pump async file jobs until no stream still
// reports remaining bytes (status 0xEA). PC loads are synchronous.
void file_pump(); // 0x000E9E60
u8 game_init_wait(int stream_id, int* remain, int* total); // 0x00039D50
void game_init_pump();                                     // 0x00032630

} // namespace gbtg
