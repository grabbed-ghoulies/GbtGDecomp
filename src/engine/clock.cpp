#include "gbtg/engine_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <mutex>

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

// Commands at 0x00507D40, stride 0x10, one per port. Magnitudes are floats.
// 0x000FAD80 reads +0x00 and +0x04, and submits when the dword at +0x0C is set.
constexpr float kRumbleScale = 0.01f;

struct RumbleSlot
{
    float left = 0.f;
    float right = 0.f;
    i32 gate = 0;
    i32 active = 0;
};

static_assert(sizeof(RumbleSlot) == 0x10);

// Device records at 0x00434910, stride 0x14. Boot 0x000FAD20 calls 0x001558F0(1)
// before the first frame, so the ports are present when this pass runs.
struct RumbleDevice
{
    u8 enabled = 1;
    u8 attached = 0;
    u8 motors_on = 0;
    u8 pad3 = 0;
    u8 armed = 0;
    u8 port = 0;
    u16 pad6 = 0;
    float left = 0.f;
    float right = 0.f;
    float extra = 0.f;
};

static_assert(sizeof(RumbleDevice) == 0x14);
static_assert(offsetof(RumbleDevice, left) == 8);

RumbleSlot g_rumble[kPadPorts];
RumbleDevice g_rumble_dev[kPadPorts];

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

// Xbox 0x000FAD80
void pad_buttons()
{
    // 0x00155960 drops the sample when the port is disabled. The third argument
    // is 0, which arms the device. Both motors at 0 clear motors_on.
    for (int port = 0; port < kPadPorts; ++port)
    {
        const RumbleSlot& slot = g_rumble[port];
        if (slot.active == 0)
        {
            continue;
        }
        RumbleDevice& dev = g_rumble_dev[port];
        if (dev.enabled == 0)
        {
            continue;
        }
        const float left = slot.left * kRumbleScale;
        const float right = slot.right * kRumbleScale;
        dev.port = u8(port);
        dev.motors_on = (left == 0.f && right == 0.f) ? 0 : 1;
        dev.armed = 1;
        dev.left = left;
        dev.right = right;
        dev.extra = 0.f;
    }
}

} // namespace

void pad_tick(Game& g, const InputState& in)
{
    // Xbox 0x000FA3C0, then rumble submit 0x000FAD80.
    pad_process(g.clock.dt, in);
    pad_buttons();
}

namespace
{

// Image _locktable at 0x00337AB0. Thirty-six records, eight bytes each:
// critical-section pointer, then kind. Ghidra types the pointer as the
// element, so _unlock's index is locknum * 2.
constexpr u32 kCrtLockCount = 0x24;

struct CrtLockSlot
{
    std::recursive_mutex* cs = nullptr;
    u32 kind = 0;
};

CrtLockSlot g_locktable[kCrtLockCount];

} // namespace

void fn_16810f(int locknum)
{
    // Xbox 0x0016810F
    // _unlock. Loads the critical-section pointer at 0x00337AB0 + locknum*8
    // and calls RtlLeaveCriticalSection (kernel ordinal 294, thunk
    // 0x00202EA4). The callee is stdcall and pops its argument; the caller
    // of _unlock pops the cdecl lock number. An index outside the 36 slots
    // walks off the table; the port returns. A null pointer is the retail
    // fault; the port returns.
    if (locknum < 0 || static_cast<u32>(locknum) >= kCrtLockCount)
    {
        return;
    }
    std::recursive_mutex* cs = g_locktable[locknum].cs;
    if (cs == nullptr)
    {
        return;
    }
    cs->unlock();
}

namespace
{

// _tiddata from _getptd (0x0016AE40). Retail calloc is 0x88 bytes.
// _curexception is +0x78, _curcontext is +0x7C, _ProcessingThrow is +0x80.
struct CrtTidBlock
{
    u8 prefix[0x80];
    i32 processing_throw;
    u8 tail[4];
};

static_assert(offsetof(CrtTidBlock, processing_throw) == 0x80);
static_assert(sizeof(CrtTidBlock) == 0x88);

CrtTidBlock g_crt_tid{};

CrtTidBlock* crt_getptd()
{
    return &g_crt_tid;
}

} // namespace

void fn_16ba2f()
{
    // Xbox 0x0016BA2F
    // Outlined from __FrameUnwindToState (0x0016B97C). That routine
    // increments _ProcessingThrow before the unwind __try, sets the try
    // level to -1, then calls here. _getptd (0x0016AE40) returns the
    // 0x88-byte _tiddata. The int at +0x80 is decremented only when it
    // is greater than zero, and the decrement goes through a second
    // _getptd. The filter at 0x0016B95E zeroes this dword before
    // terminate (0x0016C177) when exception code 0xE06D7363 escapes an
    // unwind action, so a non-positive count must stay put.
    CrtTidBlock* ptd = crt_getptd();
    if (ptd->processing_throw > 0)
    {
        ptd = crt_getptd();
        --ptd->processing_throw;
    }
}

} // namespace gbtg
