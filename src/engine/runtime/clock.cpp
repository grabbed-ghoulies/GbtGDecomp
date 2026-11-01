#include "gbtg/engine_internal.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "rare_replay.h"
#endif

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
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

// Xbox 0x000FC030
void clock_pad(Clock& c, float incoming_seconds)
{
    const int fps = clock_fps(c);
    const float cap = 5.f / float(fps);
    c.dt_raw = incoming_seconds;
    c.dt = std::min(incoming_seconds, cap);
    // DAT_005096C8 keeps only the fraction; DAT_005096C4 is this step's whole 1/300 s ticks.
    c.acc300 += c.dt * 300.f;
    c.ticks300 = i32(c.acc300);
    c.acc300 -= float(c.ticks300);
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

RuntimePadRumbleCommand g_rumble[kPadPorts]; // DAT_00507D40
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
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // Rare Replay: the hold counts retail ticks.
        if (!rare_replay_retail_tick())
        {
            continue;
        }
#endif
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
    for (int index = 0; index < kPadPorts; ++index)
    {
        const InputState::PadPacket& packet = in.pads[size_t(index)];
        g_raw[index] = RawPort{packet.lx, packet.ly, packet.rx, packet.ry, packet.trigger_l, packet.trigger_r,
            packet.buttons};
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
    if (in.start)
    {
        pad.buttons = u16(pad.buttons | 0x0010);
    }
    if (in.page)
    {
        pad.buttons = u16(pad.buttons | 0x0400);
    }
    if (in.caret_left)
    {
        pad.buttons = u16(pad.buttons | 0x4000);
    }
    if (in.caret_right)
    {
        pad.buttons = u16(pad.buttons | 0x8000);
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
        const RuntimePadRumbleCommand& slot = g_rumble[port];
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

std::span<RuntimePadRumbleCommand, 4> runtime_pad_rumble_commands()
{
    return std::span<RuntimePadRumbleCommand, 4>(g_rumble);
}

RuntimePadStateViews runtime_pad_state_views()
{
    return {{reinterpret_cast<u8*>(g_pads), sizeof(PadSlot) * kPadPorts},
        {reinterpret_cast<u8*>(&g_pads[kPadPorts]), sizeof(PadSlot)}};
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

// Xbox 0x0016810F
void crt_unlock(int locknum)
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
// The terminate pointer is +0x68 and the translator is +0x70. Those are
// host function pointers, so they sit after the 0x88-byte image: a 64-bit
// pointer does not fit in the retail dword.
struct CrtTidBlock
{
    u8 prefix[0x80];
    i32 processing_throw;
    u8 tail[4];
    void* curexception;
    void* curcontext;
    void (*terminate_fn)();
    void (*translator)(u32 code, void* info);
};

static_assert(offsetof(CrtTidBlock, processing_throw) == 0x80);
static_assert(offsetof(CrtTidBlock, tail) == 0x84);
static_assert(offsetof(CrtTidBlock, curexception) == 0x88);

CrtTidBlock g_crt_tid{};

CrtTidBlock* crt_getptd()
{
    return &g_crt_tid;
}

} // namespace

// Xbox 0x0016BA2F
void crt_finish_frame_unwind()
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

namespace
{

// Xbox C++ EH image, magic 0x19930520. Pointer fields are host pointers.
// Retail offsets are called out where the x86 frame math depends on them.

constexpr u32 kCxxException = 0xE06D7363u;
constexpr u32 kEhMagic = 0x19930520u;
constexpr u32 kStatusBreakpoint = 0x80000003u;

struct CrtTypeInfo
{
    const void* vftable;
    void* spare;
    char name[1];
};

struct CrtPmd
{
    i32 mdisp;
    i32 pdisp;
    i32 vdisp;
};

struct CrtCatchableType
{
    u32 properties;
    const CrtTypeInfo* type;
    CrtPmd pmd;
    i32 size;
    void* copy;
};

struct CrtCatchableArray
{
    i32 count;
    const CrtCatchableType* types[1];
};

struct CrtThrowInfo
{
    u32 attributes;
    void (*dtor)(void* self);
    void* forward;
    const CrtCatchableArray* catchables;
};

struct CrtEhRecord
{
    u32 code;
    u32 flags;
    CrtEhRecord* next;
    void* address;
    u32 nparams;
    u32 magic;
    void* object;
    const CrtThrowInfo* throw_info;
};

struct CrtEhRegistration
{
    void* next;
    void* handler;
    i32 state;
};

struct CrtHandlerType
{
    u32 adjectives;
    const CrtTypeInfo* type;
    i32 disp_catch;
    void* (*address)();
};

struct CrtTryBlock
{
    i32 try_low;
    i32 try_high;
    i32 catch_high;
    i32 n_catches;
    const CrtHandlerType* handlers;
};

struct CrtUnwindEntry
{
    i32 to_state;
    void (*action)(void* registration);
};

struct CrtFuncInfo
{
    u32 magic;
    i32 max_state;
    const CrtUnwindEntry* unwind;
    i32 n_try_blocks;
    const CrtTryBlock* tries;
};

struct CrtEhPointers
{
    CrtEhRecord* record;
    void* context;
};

// 0x003381E8. BSS, so the image dword is 0. __inconsistency calls it once
// inside a handler that swallows every exception, then falls into terminate.
void (*g_crt_inconsistency_fn)() = nullptr;

// Xbox 0x0016C177
[[noreturn]] void crt_terminate()
{
    // Xbox 0x0016C177. A non-null terminate pointer at _tiddata+0x68 is
    // called inside __try. The filter returns 1, so a throw is swallowed.
    // KeBugCheck(0xC0000144) does not return.
    CrtTidBlock* ptd = crt_getptd();
    if (ptd->terminate_fn != nullptr)
    {
        try
        {
            ptd = crt_getptd();
            ptd->terminate_fn();
        }
        catch (...)
        {
        }
    }
    std::abort();
}

[[noreturn]] void crt_inconsistency()
{
    // Xbox 0x0016C1B3.
    if (g_crt_inconsistency_fn != nullptr)
    {
        try
        {
            g_crt_inconsistency_fn();
        }
        catch (...)
        {
        }
    }
    crt_terminate();
}

bool crt_is_cxx(const CrtEhRecord* except)
{
    return except->code == kCxxException && except->nparams == 3 && except->magic == kEhMagic;
}

void* crt_adjust_pointer(void* object, const CrtPmd& pmd)
{
    // Xbox 0x0016BA8F. mdisp is applied first. A negative pdisp is a
    // complete adjustment. Otherwise the vbptr at object+pdisp is followed
    // by vdisp, and that displacement plus pdisp is added as well.
    auto* base = static_cast<u8*>(object);
    u8* result = base + pmd.mdisp;
    if (pmd.pdisp < 0)
    {
        return result;
    }
    u8* vbptr = *reinterpret_cast<u8**>(base + pmd.pdisp);
    if (vbptr == nullptr)
    {
        return result;
    }
    result += pmd.pdisp;
    result += *reinterpret_cast<const i32*>(vbptr + pmd.vdisp);
    return result;
}

bool crt_type_match(const CrtHandlerType* handler, const CrtCatchableType* conv, const CrtThrowInfo* thrown)
{
    // Xbox 0x0016B8EC. A null type, or a type whose name is empty, is catch(...).
    // Retail reads the name at type_info+8. The host type_info name is the
    // field below. Identical type_info pointers match without comparing text.
    const CrtTypeInfo* want = handler->type;
    if (want == nullptr || want->name[0] == '\0')
    {
        return true;
    }
    const CrtTypeInfo* have = conv->type;
    if (have == nullptr)
    {
        return false;
    }
    if (want != have && std::strcmp(want->name, have->name) != 0)
    {
        return false;
    }
    if ((conv->properties & 0x2u) != 0 && (handler->adjectives & 0x8u) == 0)
    {
        return false;
    }
    if ((thrown->attributes & 0x1u) != 0 && (handler->adjectives & 0x1u) == 0)
    {
        return false;
    }
    if ((thrown->attributes & 0x2u) != 0 && (handler->adjectives & 0x2u) == 0)
    {
        return false;
    }
    return true;
}

const CrtTryBlock* crt_try_range(const CrtFuncInfo* info, int catch_depth, i32 cur_state, u32* start_out, u32* end_out)
{
    // Xbox 0x0016783E. __GetRangeOfTrysToCheck. CatchDepth counts catch
    // frames already entered. Each frame whose catchHigh covers curState
    // shrinks the range to the tries nested inside it. Walking off the
    // front of the map with depth still open is __inconsistency.
    if (info->n_try_blocks < 0)
    {
        crt_inconsistency();
    }
    const i32 n = info->n_try_blocks;
    u32 end_candidate = static_cast<u32>(n);
    u32 saved = end_candidate;
    i32 index = n;
    if (catch_depth >= 0)
    {
        for (;;)
        {
            if (index == -1)
            {
                crt_inconsistency();
            }
            --index;
            bool take = index < 0;
            if (!take)
            {
                if (info->tries == nullptr)
                {
                    crt_inconsistency();
                }
                const CrtTryBlock& entry = info->tries[index];
                take = entry.try_high < cur_state && cur_state <= entry.catch_high;
            }
            if (take)
            {
                end_candidate = saved;
                --catch_depth;
                saved = static_cast<u32>(index);
            }
            if (catch_depth < 0)
            {
                break;
            }
        }
    }
    const u32 start = static_cast<u32>(index + 1);
    *start_out = start;
    *end_out = end_candidate;
    if (end_candidate > static_cast<u32>(n) || start > end_candidate)
    {
        crt_inconsistency();
    }
    if (start < end_candidate && info->tries == nullptr)
    {
        crt_inconsistency();
    }
    if (info->tries == nullptr)
    {
        return nullptr;
    }
    return info->tries + static_cast<std::ptrdiff_t>(start);
}

void crt_destruct_exception(CrtEhRecord* except)
{
    // Xbox 0x0016BA4A. ___DestructExceptionObject. The unwind function is
    // ThrowInfo+4, called on the object at ExceptionInformation[1] as
    // thiscall through the thunk at 0x001675FB. The caller passes 1, so a
    // C++ exception from the destructor becomes terminate.
    if (except == nullptr || except->throw_info == nullptr || except->throw_info->dtor == nullptr)
    {
        return;
    }
    try
    {
        except->throw_info->dtor(except->object);
    }
    catch (...)
    {
        crt_terminate();
    }
}

void crt_build_catch_object(CrtEhRecord* except, CrtEhRegistration* rn, const CrtHandlerType* handler, const CrtCatchableType* conv)
{
    // Xbox 0x0016BC55. catch(...) and a handler with no catch object return
    // before copying. The catch slot is pRN + dispCatchObj + 0x0C, because
    // the displacement is relative to EBP and the registration node sits
    // 0xC bytes below that frame. Adjectives with the sign bit set keep pRN.
    // IsBadReadPtr / IsBadWritePtr / IsBadCodePtr (0x0016F172, 0x0016F18D,
    // 0x0016F1A8) failing is __inconsistency. A null pointer is that failure.
    const CrtTypeInfo* type = handler->type;
    if (type == nullptr || type->name[0] == '\0')
    {
        return;
    }
    if (handler->disp_catch == 0 && (handler->adjectives & 0x80000000u) == 0)
    {
        return;
    }

    if (rn == nullptr || except == nullptr || except->object == nullptr)
    {
        crt_inconsistency();
    }
    u8* buffer = reinterpret_cast<u8*>(rn);
    if (static_cast<i32>(handler->adjectives) >= 0)
    {
        buffer = buffer + handler->disp_catch + 0x0C;
    }
    void* object = except->object;

    try
    {
        if ((handler->adjectives & 0x08u) != 0)
        {
            *reinterpret_cast<void**>(buffer) = object;
            *reinterpret_cast<void**>(buffer) = crt_adjust_pointer(object, conv->pmd);
            return;
        }
        if ((conv->properties & 0x1u) != 0)
        {
            if (conv->size < 0)
            {
                crt_inconsistency();
            }
            std::memcpy(buffer, object, static_cast<std::size_t>(conv->size));
            if (conv->size == static_cast<i32>(sizeof(void*)))
            {
                void* stored = *reinterpret_cast<void**>(buffer);
                if (stored != nullptr)
                {
                    *reinterpret_cast<void**>(buffer) = crt_adjust_pointer(stored, conv->pmd);
                }
            }
            return;
        }
        void* src = crt_adjust_pointer(object, conv->pmd);
        if (conv->copy == nullptr)
        {
            if (conv->size < 0)
            {
                crt_inconsistency();
            }
            std::memcpy(buffer, src, static_cast<std::size_t>(conv->size));
            return;
        }
        if ((conv->properties & 0x4u) != 0)
        {
            auto* ctor = reinterpret_cast<void (*)(void*, void*, int)>(conv->copy);
            ctor(buffer, src, 1);
            return;
        }
        auto* ctor = reinterpret_cast<void (*)(void*, void*)>(conv->copy);
        ctor(buffer, src);
    }
    catch (...)
    {
        crt_terminate();
    }
}

void crt_unwind_to_state(CrtEhRegistration* rn, const CrtFuncInfo* info, i32 target)
{
    // Xbox 0x0016B97C. __FrameUnwindToState. Each unwind-map slot is 8 bytes:
    // the next state, then the action. A non-null action is called with the
    // registration node after the state dword is updated. The action runs
    // under the filter at 0x0016B95E. ProcessingThrow is incremented for the
    // walk and crt_finish_frame_unwind drops it when the target is reached.
    CrtTidBlock* ptd = crt_getptd();
    ++ptd->processing_throw;
    i32 state = rn->state;
    while (state != target)
    {
        if (state <= -1 || info->unwind == nullptr || state >= info->max_state)
        {
            crt_inconsistency();
        }
        const CrtUnwindEntry& entry = info->unwind[state];
        const i32 next = entry.to_state;
        if (entry.action != nullptr)
        {
            rn->state = next;
            try
            {
                entry.action(rn);
            }
            catch (...)
            {
                crt_getptd()->processing_throw = 0;
                crt_terminate();
            }
        }
        state = next;
    }
    crt_finish_frame_unwind();
    if (state != target)
    {
        crt_inconsistency();
    }
    rn->state = state;
}

void* crt_call_handler(CrtEhRecord* except, void* context, void* (*handler)())
{
    // Xbox 0x0016BAAE. The current exception and context replace
    // _curexception and _curcontext for the funclet, and the finally
    // restores both. __CallCatchBlock2 returns the continuation address.
    CrtTidBlock* ptd = crt_getptd();
    void* saved_ex = ptd->curexception;
    void* saved_ctx = ptd->curcontext;
    ptd = crt_getptd();
    ptd->curexception = except;
    ptd = crt_getptd();
    ptd->curcontext = context;
    void* continuation = nullptr;
    try
    {
        if (handler != nullptr)
        {
            continuation = handler();
        }
    }
    catch (...)
    {
        ptd = crt_getptd();
        ptd->curexception = saved_ex;
        ptd = crt_getptd();
        ptd->curcontext = saved_ctx;
        throw;
    }
    ptd = crt_getptd();
    ptd->curexception = saved_ex;
    ptd = crt_getptd();
    ptd->curcontext = saved_ctx;
    return continuation;
}

bool crt_catch_it(
    CrtEhRecord* except,
    CrtEhRegistration* rn,
    void* context,
    void* dispatcher,
    const CrtFuncInfo* info,
    const CrtHandlerType* handler,
    const CrtCatchableType* conv,
    const CrtTryBlock* entry,
    int catch_depth,
    CrtEhRegistration* marker)
{
    // Xbox 0x0016BDD1. A catchable type builds the catch object. Nested
    // frames unwind to the marker when one was passed, otherwise to this
    // registration node (0x00167602 clears EXCEPTION_UNWINDING on the
    // record). The frame unwinds to tryLow, the state becomes tryHigh+1,
    // and the handler runs. A non-null continuation is JumpToContinuation
    // (0x001675CB), which restores the establisher and does not return.
    (void)dispatcher;
    (void)catch_depth;
    if (conv != nullptr)
    {
        crt_build_catch_object(except, rn, handler, conv);
    }
    CrtEhRegistration* target = rn;
    if (marker != nullptr)
    {
        target = marker;
    }
    (void)target;
    except->flags &= ~2u;
    crt_unwind_to_state(rn, info, entry->try_low);
    rn->state = entry->try_high + 1;
    void* (*address)() = nullptr;
    if (handler != nullptr)
    {
        address = handler->address;
    }
    void* continuation = crt_call_handler(except, context, address);
    return continuation != nullptr;
}

void crt_foreign(
    CrtEhRecord* except,
    CrtEhRegistration* rn,
    void* context,
    void* dispatcher,
    const CrtFuncInfo* info,
    i32 cur_state,
    int catch_depth,
    CrtEhRegistration* marker)
{
    // Xbox 0x0016BE38. _FindHandlerForForeignException. A breakpoint
    // exception is ignored. A translator at _tiddata+0x70 is
    // _CallSETranslator (0x001676C5): it receives the exception code and
    // an EXCEPTION_POINTERS pair. A translator that returns did not handle
    // the exception. catch(...) is only the last handler of a try, and a
    // null or empty type name selects it.
    if (except->code == kStatusBreakpoint)
    {
        return;
    }
    CrtTidBlock* ptd = crt_getptd();
    if (ptd->translator != nullptr)
    {
        CrtEhPointers pointers{except, context};
        ptd->translator(except->code, &pointers);
    }
    u32 start = 0;
    u32 end = 0;
    const CrtTryBlock* entry = crt_try_range(info, catch_depth, cur_state, &start, &end);
    for (; start < end; ++start, ++entry)
    {
        if (cur_state < entry->try_low || cur_state > entry->try_high)
        {
            continue;
        }
        if (entry->n_catches <= 0 || entry->handlers == nullptr)
        {
            continue;
        }
        const CrtHandlerType* handler = entry->handlers + (entry->n_catches - 1);
        const CrtTypeInfo* type = handler->type;
        if (type != nullptr && type->name[0] != '\0')
        {
            continue;
        }
        if (crt_catch_it(except, rn, context, dispatcher, info, handler, nullptr, entry, catch_depth, marker))
        {
            return;
        }
    }
}

} // namespace

// Xbox 0x0016BEF6
void crt_find_exception_handler(
    CrtEhRecord* except_record,
    CrtEhRegistration* registration,
    void* context,
    void* dispatcher,
    const CrtFuncInfo* func_info,
    u8 recursive,
    int catch_depth,
    CrtEhRegistration* marker)
{
    // Xbox 0x0016BEF6
    // FindHandler. The registration state is the dword at +8. A state
    // below -1 or at or past FuncInfo.maxState (+4) is __inconsistency
    // (0x0016C1B3). Exception code 0xE06D7363 with three parameters and
    // magic 0x19930520 is a C++ exception. A null ThrowInfo is a rethrow:
    // _curexception / _curcontext are loaded from _getptd (0x0016AE40), a
    // null saved exception returns, and IsBadReadPtr (0x0016F172) failing
    // or a second rethrow record is __inconsistency. The byte written at
    // [ebp-1] on that path is never read. The caller's recursive argument
    // chooses destroy-versus-return and foreign-versus-terminate.
    //
    // __GetRangeOfTrysToCheck (0x0016783E) supplies the try entries. A try
    // whose low/high span covers the state walks its handlers (+0x10)
    // against ThrowInfo's catchable types. TypeMatch (0x0016B8EC) selects
    // CatchIt (0x0016BDD1). After a catch that returns, the next try is
    // the one that follows. No handler and a clear recursive flag returns.
    // A set flag destroys the exception (0x0016BA4A) and returns. Anything
    // that is not a C++ exception with recursive clear goes to
    // FindHandlerForForeignException (0x0016BE38). A set flag terminates
    // (0x0016C177).
    if (except_record == nullptr || registration == nullptr || func_info == nullptr)
    {
        crt_inconsistency();
    }

    const i32 cur_state = registration->state;
    if (cur_state < -1 || cur_state >= func_info->max_state)
    {
        crt_inconsistency();
    }

    CrtEhRecord* except = except_record;
    void* ctx = context;
    if (except->code == kCxxException)
    {
        if (crt_is_cxx(except) && except->throw_info == nullptr)
        {
            CrtTidBlock* ptd = crt_getptd();
            if (ptd->curexception == nullptr)
            {
                return;
            }
            ptd = crt_getptd();
            except = static_cast<CrtEhRecord*>(ptd->curexception);
            ptd = crt_getptd();
            ctx = ptd->curcontext;
            if (except == nullptr)
            {
                crt_inconsistency();
            }
            if (except->code != kCxxException)
            {
                if (recursive == 0)
                {
                    crt_foreign(except, registration, ctx, dispatcher, func_info, cur_state, catch_depth, marker);
                    return;
                }
                crt_terminate();
            }
            if (crt_is_cxx(except) && except->throw_info == nullptr)
            {
                crt_inconsistency();
            }
        }

        if (crt_is_cxx(except))
        {
            u32 start = 0;
            u32 end = 0;
            const CrtTryBlock* entry = crt_try_range(func_info, catch_depth, cur_state, &start, &end);
            const CrtThrowInfo* thrown = except->throw_info;
            const CrtCatchableArray* array = nullptr;
            if (thrown != nullptr)
            {
                array = thrown->catchables;
            }
            for (; start < end; ++start, ++entry)
            {
                if (entry->try_low > cur_state || cur_state > entry->try_high)
                {
                    continue;
                }
                if (array == nullptr || entry->n_catches <= 0 || entry->handlers == nullptr)
                {
                    continue;
                }
                const i32 n_types = array->count;
                const CrtCatchableType* const* types = &array->types[0];
                const CrtHandlerType* handler = entry->handlers;
                bool matched = false;
                for (i32 catches = entry->n_catches; catches > 0; --catches, ++handler)
                {
                    const CrtCatchableType* const* cursor = types;
                    for (i32 left = n_types; left > 0; --left, ++cursor)
                    {
                        const CrtCatchableType* conv = *cursor;
                        if (conv != nullptr && crt_type_match(handler, conv, thrown))
                        {
                            matched = true;
                            if (crt_catch_it(
                                    except,
                                    registration,
                                    ctx,
                                    dispatcher,
                                    func_info,
                                    handler,
                                    conv,
                                    entry,
                                    catch_depth,
                                    marker))
                            {
                                return;
                            }
                            break;
                        }
                    }
                    if (matched)
                    {
                        break;
                    }
                }
            }
            if (recursive == 0)
            {
                return;
            }
            crt_destruct_exception(except);
            return;
        }
    }

    if (recursive == 0)
    {
        crt_foreign(except, registration, ctx, dispatcher, func_info, cur_state, catch_depth, marker);
        return;
    }
    crt_terminate();
}

} // namespace gbtg
