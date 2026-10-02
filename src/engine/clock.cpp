#include "gbtg/engine_internal.hpp"

#include <algorithm>

namespace gbtg
{

int clock_fps(const Clock& c)
{
    // Xbox 0x00033030 / 0x000FC030: 50 Hz only when DAT_005096E0 && !DAT_005096E4.
    return c.pal ? 50 : 60;
}

void clock_init(Clock& c)
{
    const int fps = clock_fps(c);
    c.dt = 1.f / float(fps);
    c.dt_raw = c.dt;
    const float t = c.dt * 300.f;
    c.ticks300 = i32(t);
    c.acc300 = t - float(c.ticks300);
    c.time = 0.f;
    c.frame = 0;
    c.frame2 = 0;
    c.delayed = {};
}

void clock_pad(Clock& c, float incoming_seconds)
{
    const int fps = clock_fps(c);
    const float cap = 5.f / float(fps);
    c.dt_raw = incoming_seconds;
    c.dt = std::min(incoming_seconds, cap);
    c.acc300 += c.dt * 300.f;
    c.ticks300 = i32(c.acc300);
}

void clock_frame_time(Clock& c)
{
    c.time += c.dt_raw;
    ++c.frame;
    ++c.frame2;
    for (auto& slot : c.delayed)
    {
        if (slot.frames_left == 0)
        {
            continue;
        }
        --slot.frames_left;
        if (slot.frames_left == 0 && slot.fn)
        {
            slot.fn(slot.ctx);
            slot.ctx = nullptr;
            slot.fn = nullptr;
        }
    }
}

bool clock_schedule(Clock& c, u32 frames, void* ctx, void (*fn)(void*))
{
    if (frames == 0 || fn == nullptr)
    {
        return false;
    }
    for (auto& slot : c.delayed)
    {
        if (slot.frames_left == 0)
        {
            slot.frames_left = frames;
            slot.ctx = ctx;
            slot.fn = fn;
            return true;
        }
    }
    return false;
}

void file_pump()
{
    // Xbox 0x000E9E60 walks DAT_0050661C; state 2 HeapFree's a finished job.
}

u8 game_init_wait(int /*stream_id*/, int* remain, int* total)
{
    // Xbox 0x00039D50: 12 slots of 0x14 at 0x00421A8C vs objects at 0x004E14F8.
    // Returns 0xEA while any matching slot still has remaining bytes.
    if (remain)
    {
        *remain = 0;
    }
    if (total)
    {
        *total = 0;
    }
    return 0;
}

void game_init_pump()
{
    int remain = 0;
    int total = 0;
    do
    {
        file_pump();
    } while (game_init_wait(0, &remain, &total) == kIoBusy);
}

void pad_tick(Game& g, const InputState& in)
{
    // Xbox 0x000FA3C0 / 0x000FAD80. Digital buttons arrive in InputState from the platform.
    (void)g;
    (void)in;
}

} // namespace gbtg
