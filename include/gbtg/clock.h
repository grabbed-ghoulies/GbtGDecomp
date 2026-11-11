#pragma once

#include "gbtg/types.h"

#include <array>
#include <span>

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

struct RuntimePadStateViews
{
    std::span<u8> controllers; // DAT_005079F8, four stride-0xA8 records.
    std::span<u8> merged; // DAT_00507C98, current and previous samples.
};
// Native byte views of the samples already produced by pad_tick.
RuntimePadStateViews runtime_pad_state_views();
// The device table 0x001554C0 reads (DAT_0032E3B4, stride 8 by device type): type 0, the gamepads, holds the
// connected ports, bit n for port n. 0x00155540 stores XGetDevices; 0x001555B0 applies XGetDeviceChanges.
std::span<const u8> runtime_input_device_table();

// DAT_00507D40, stride 0x10, one per port: the rumble 0x000FAD80 submits while +0x0C is set. The camera shakes
// (0x000FAF70) write the motors while +0x08 lets them.
struct RuntimePadRumbleCommand
{
    float left = 0.f; // +0x00
    float right = 0.f; // +0x04
    i32 shakes_allowed = 0; // +0x08
    i32 active = 0; // +0x0C
};
static_assert(sizeof(RuntimePadRumbleCommand) == 0x10);
std::span<RuntimePadRumbleCommand, 4> runtime_pad_rumble_commands();
// Boot: every port's rumble slot is on and quiet, and every rumble command is off.
void boot_pad_rumble(); // Xbox 0x000FAD20
// Boot: the pads open (the XInput boundary) and every pad's current and previous samples are cleared.
void reset_pad_samples(); // Xbox 0x000FA210

} // namespace gbtg
