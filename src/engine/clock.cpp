#include "gbtg/engine_internal.hpp"

#include <algorithm>
#include <cstddef>

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

namespace
{

// Slots at 0x005079F8, stride 0xA8. Ports 0..3, then the merged pad at 0x00507C98.
// The first 0x3C bytes are the live sample; 0x000FA110 copies them onto +0x3C.
// Hold counters sit at +0x78 and count frames a bit in +0x38 stays down.
constexpr int kPadPorts = 4;
constexpr i32 kStickDeadzone = 9800;
constexpr i32 kStickLimit = 32700;
constexpr int kTriggerCap = 0x8C;
constexpr float kTriggerScale = 1.f / 140.f;
constexpr i32 kHoldCap = 0x7ffffffe;

// Digital word at pad+0x38. D-pad and Start come from XInput wButtons.
// Face buttons are the analog bytes once they pass 30 (0x00154A70):
// A 0x0100, B 0x0200, X 0x0400, Y 0x0800, Black 0x1000, White 0x2000.
constexpr u16 kHoldBits[12] = {
    0x0004, 0x0008, 0x0002, 0x0001, 0x1000, 0x8000, 0x4000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0010,
};

struct PadSample
{
    float lx = 0.f;
    float ly = 0.f;
    float rx = 0.f;
    float ry = 0.f;
    float menu_x = 0.f;
    float menu_y = 0.f;
    float lx_out = 0.f;
    float ly_out = 0.f;
    float rx_out = 0.f;
    float ry_out = 0.f;
    float spare0 = 0.f;
    float spare1 = 0.f;
    float trigger_l = 0.f;
    float trigger_r = 0.f;
    u16 buttons = 0;
    u16 pad = 0;
};

struct PadSlot
{
    PadSample cur{};
    PadSample prev{};
    i32 hold[12]{};
};

static_assert(sizeof(PadSample) == 0x3C);
static_assert(offsetof(PadSlot, hold) == 0x78);
static_assert(sizeof(PadSlot) == 0xA8);

struct RawPort
{
    i32 lx = 0;
    i32 ly = 0;
    i32 rx = 0;
    i32 ry = 0;
    u8 trigger_l = 0;
    u8 trigger_r = 0;
    u16 buttons = 0;
};

RawPort g_raw[kPadPorts];
PadSlot g_pads[kPadPorts + 1];

// Xbox 0x000FA4F0. ±9800 is zero; ±32700 is ±1.
float stick_axis(i32 raw)
{
    const float span = float(kStickLimit - kStickDeadzone);
    if (raw > kStickLimit)
    {
        raw = kStickLimit;
    }
    else if (raw < -kStickLimit)
    {
        raw = -kStickLimit;
    }
    if (raw >= kStickDeadzone)
    {
        return float(raw - kStickDeadzone) / span;
    }
    if (raw <= -kStickDeadzone)
    {
        return float(raw + kStickDeadzone) / span;
    }
    return 0.f;
}

float trigger_axis(int raw)
{
    if (raw > kTriggerCap)
    {
        raw = kTriggerCap;
    }
    return float(raw) * kTriggerScale;
}

void apply_dpad_menu(PadSample& sample)
{
    // D-pad writes the menu stick: up, else down, else right, else left.
    if ((sample.buttons & 0x0001) != 0)
    {
        sample.menu_x = 0.f;
        sample.menu_y = 1.f;
    }
    else if ((sample.buttons & 0x0002) != 0)
    {
        sample.menu_x = 0.f;
        sample.menu_y = -1.f;
    }
    else if ((sample.buttons & 0x0008) != 0)
    {
        sample.menu_x = 1.f;
        sample.menu_y = 0.f;
    }
    else if ((sample.buttons & 0x0004) != 0)
    {
        sample.menu_x = -1.f;
        sample.menu_y = 0.f;
    }
}

void write_axes(PadSample& sample, i32 lx, i32 ly, i32 rx, i32 ry, int trigger_l, int trigger_r, u16 buttons)
{
    sample.lx = stick_axis(lx);
    sample.ly = stick_axis(ly);
    sample.rx = stick_axis(rx);
    sample.ry = stick_axis(ry);
    sample.lx_out = sample.lx;
    sample.ly_out = sample.ly;
    sample.rx_out = sample.rx;
    sample.ry_out = sample.ry;
    sample.trigger_l = trigger_axis(trigger_l);
    sample.trigger_r = trigger_axis(trigger_r);
    sample.buttons = buttons;
    apply_dpad_menu(sample);
}

void update_holds(PadSlot& slot)
{
    // Xbox 0x000F9F60. A released bit clears its counter. A held bit increments, capped.
    for (int i = 0; i < 12; ++i)
    {
        if ((slot.cur.buttons & kHoldBits[i]) == 0)
        {
            slot.hold[i] = 0;
            continue;
        }
        if (slot.hold[i] < kHoldCap)
        {
            ++slot.hold[i];
        }
    }
}

void sample_port(PadSample& sample, const RawPort& raw)
{
    // Xbox 0x000FA4F0.
    write_axes(sample, raw.lx, raw.ly, raw.rx, raw.ry, raw.trigger_l, raw.trigger_r, raw.buttons);
}

void sample_merged(PadSample& sample, const RawPort raw[kPadPorts])
{
    // Xbox 0x000FA840. Sticks are summed, then run through the same curve. Triggers wrap as bytes.
    i32 lx = 0;
    i32 ly = 0;
    i32 rx = 0;
    i32 ry = 0;
    u8 trigger_l = 0;
    u8 trigger_r = 0;
    u16 buttons = 0;
    for (int port = 0; port < kPadPorts; ++port)
    {
        lx += raw[port].lx;
        ly += raw[port].ly;
        rx += raw[port].rx;
        ry += raw[port].ry;
        trigger_l = u8(trigger_l + raw[port].trigger_l);
        trigger_r = u8(trigger_r + raw[port].trigger_r);
        buttons = u16(buttons | raw[port].buttons);
    }
    write_axes(sample, lx, ly, rx, ry, trigger_l, trigger_r, buttons);
}

void pad_poll(float dt, const InputState& in)
{
    // Xbox 0x001555B0(dt) polls XInput into the four packets. Repeat timers
    // advance with dt there; the sample below reads the held button word.
    (void)dt;
    for (RawPort& port : g_raw)
    {
        port = {};
    }
    RawPort& pad = g_raw[0];
    if (in.up)
    {
        pad.buttons = u16(pad.buttons | 0x0001);
    }
    if (in.down)
    {
        pad.buttons = u16(pad.buttons | 0x0002);
    }
    if (in.left)
    {
        pad.buttons = u16(pad.buttons | 0x0004);
    }
    if (in.right)
    {
        pad.buttons = u16(pad.buttons | 0x0008);
    }
    if (in.accept)
    {
        pad.buttons = u16(pad.buttons | 0x0100);
    }
    if (in.back)
    {
        pad.buttons = u16(pad.buttons | 0x0200);
    }
}

void shift_sample(PadSlot& slot)
{
    // Xbox 0x000FA110.
    slot.prev = slot.cur;
    slot.cur = {};
}

// Xbox 0x000FA3C0
void pad_process(float dt, const InputState& in)
{
    pad_poll(dt, in);
    for (int port = 0; port < kPadPorts; ++port)
    {
        PadSlot& slot = g_pads[port];
        shift_sample(slot);
        sample_port(slot.cur, g_raw[port]);
        update_holds(slot);
    }
    PadSlot& merged = g_pads[kPadPorts];
    shift_sample(merged);
    sample_merged(merged.cur, g_raw);
    update_holds(merged);
}

} // namespace

void pad_tick(Game& g, const InputState& in)
{
    // Xbox 0x000FA3C0. 0x000FAD80 rumble is the following pass over these slots.
    pad_process(g.clock.dt, in);
}

} // namespace gbtg
