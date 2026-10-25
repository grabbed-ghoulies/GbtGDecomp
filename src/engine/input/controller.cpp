#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

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
namespace
{

constexpr int kPadBitPorts = 4;
constexpr int kPadBitCount = 32;

// DAT_00434288. 0x00155470 copies DAT_0032B0F8 (1) before the first poll.
float g_pad_hold_delay = 1.f;

// DAT_00434290. 0x00155470 stores 0. The repeat timer pulses at this level.
float g_pad_repeat_gate = 0.f;

// One port of the 0x13c packet. hold[32] is +0x14. repeat[32] is +0x94.
// Thumb words sit past the repeat timers. +0x11c / +0x120 are the right stick.
struct PadBitPort
{
    u32 down = 0;      // +0x00 DAT_00434298
    u32 down_prev = 0; // +0x04 DAT_0043429C
    u32 pressed = 0;   // +0x08 DAT_004342A0. Set on the frame a bit goes down.
    u32 released = 0;  // +0x0C DAT_004342A4. Set on the frame a bit goes up.
    u32 pulse = 0;     // +0x10 DAT_004342A8
    float hold[kPadBitCount]{};
    float repeat[kPadBitCount]{};
    i32 thumb_lx = 0;    // +0x114 DAT_004343ac
    i32 thumb_ly = 0;    // +0x118 DAT_004343b0
    i32 thumb_rx = 0;    // +0x11c DAT_004343b4
    i32 thumb_ry = 0;    // +0x120 DAT_004343b8
    float axis_lx = 0.f; // +0x124 DAT_004343bc
    float axis_ly = 0.f; // +0x128 DAT_004343c0
    float axis_rx = 0.f; // +0x12c DAT_004343c4
    float axis_ry = 0.f; // +0x130 DAT_004343c8
    u8 face_a = 0;       // +0x134 DAT_004343cc
    u8 face_b = 0;       // +0x135 DAT_004343cd
    u8 face_x = 0;       // +0x136 DAT_004343ce
    u8 face_y = 0;       // +0x137 DAT_004343cf
    u8 black = 0;        // +0x138 DAT_004343d0
    u8 white = 0;        // +0x139 DAT_004343d1
    u8 trigger_l = 0;    // +0x13a DAT_004343d2
    u8 trigger_r = 0;    // +0x13b DAT_004343d3
};

static_assert(offsetof(PadBitPort, face_a) == 0x134);
static_assert(offsetof(PadBitPort, face_b) == 0x135);
static_assert(offsetof(PadBitPort, face_x) == 0x136);
static_assert(offsetof(PadBitPort, trigger_r) == 0x13b);
static_assert(sizeof(PadBitPort) == 0x13c);

PadBitPort g_pad_bits[kPadBitPorts];

// DAT_00434284. 0x00155470 installs 0x00155360. Null leaves x and y unchanged.
void (*g_pad_left_hook)(i32 port, i32* x, i32* y) = nullptr;

// DAT_00434280. 0x00155470 installs 0x001553B0. Null leaves x and y unchanged.
void (*g_pad_right_hook)(i32 port, i32* x, i32* y) = nullptr;

// DAT_0043428c. 0x00155470 installs 0x00155400. Null leaves the bytes unchanged.
void (*g_pad_analog_hook)(i32 port, i32 mask, u8* a, u8* b) = nullptr;

// DAT_00434910, stride 0x14. One slot per port. 0x001557B0 decays +0x10.
struct RumbleSlot
{
    u8 enabled = 0; // +0x00
    u8 open = 0;    // +0x01
    u8 active = 0;  // +0x02 DAT_00434912
    u8 idle = 0;    // +0x03
    u8 armed = 0;   // +0x04 DAT_00434914
    u8 port = 0;    // +0x05 DAT_00434915
    u8 pad_06 = 0;
    u8 pad_07 = 0;
    float left = 0.f;  // +0x08 DAT_00434918
    float right = 0.f; // +0x0C DAT_0043491C
    float timer = 0.f; // +0x10 DAT_00434920
};

static_assert(offsetof(RumbleSlot, active) == 0x02);
static_assert(offsetof(RumbleSlot, armed) == 0x04);
static_assert(offsetof(RumbleSlot, port) == 0x05);
static_assert(offsetof(RumbleSlot, left) == 0x08);
static_assert(offsetof(RumbleSlot, right) == 0x0c);
static_assert(offsetof(RumbleSlot, timer) == 0x10);
static_assert(sizeof(RumbleSlot) == 0x14);

RumbleSlot g_rumble[kPadBitPorts];

// DAT_00203A98. Scales a motor float into the 16-bit feedback word.
constexpr double kRumbleFullScale = 65535.0;

// Xbox packet stride. PadFeedback's tail alignment is not part of the record.
constexpr int kPadFeedbackStride = 0x46;

static_assert(offsetof(PadFeedback, status) == 0x00);
static_assert(offsetof(PadFeedback, event) == 0x04);
static_assert(offsetof(PadFeedback, left) == 0x42);
static_assert(offsetof(PadFeedback, right) == 0x44);

// 0x001673D8 (__ftol) of left/right * DAT_00203A98. The word keeps the low 16
// bits. A non-finite product, or one outside a signed 32-bit chop, stores 0.
u16 rumble_word(float level)
{
    const double scaled = static_cast<double>(level) * kRumbleFullScale;
    if (!std::isfinite(scaled))
    {
        return 0;
    }
    const double chopped = std::trunc(scaled);
    if (chopped >= 2147483648.0 || chopped < -2147483648.0)
    {
        return 0;
    }
    return static_cast<u16>(static_cast<i32>(chopped));
}

// 0x00154A70 compares the analog byte with this threshold. At or above counts as down.
constexpr u8 kAnalogPress = 0x1e;

// A and B in the button word. 0x001555B0 passes the analog bytes as a pair.
constexpr u32 kFaceABit = 0x100;
constexpr u32 kFaceBBit = 0x200;

// X and Y in the button word. 0x001555B0 passes the analog bytes as a pair.
constexpr u32 kFaceXBit = 0x400;
constexpr u32 kFaceYBit = 0x800;

// Left and right triggers in the button word. cam_155624 passes them as a pair.
constexpr u32 kTriggerLeftBit = 0x4000;
constexpr u32 kTriggerRightBit = 0x8000;

// Black and white in the button word. 0x001555B0 passes the analog bytes as a pair.
constexpr u32 kBlackBit = 0x1000;
constexpr u32 kWhiteBit = 0x2000;

// DAT_00203A8C. Full scale after the left-stick hook is ±74.
constexpr float kLeftThumbScale = 1.f / 74.f;

// DAT_00203A90. Full scale after the right-stick hook is ±59.
constexpr float kRightThumbScale = 1.f / 59.f;

float clamp_thumb(float value)
{
    if (value < -1.f)
    {
        return -1.f;
    }
    if (value > 1.f)
    {
        return 1.f;
    }
    return value;
}

// 0x00154A70. EAX is the port, DL the analog byte, ECX the bit, and the stack
// byte is the threshold. A byte at or above the threshold holds the bit in
// +0x00. The frame it rises against +0x04 sets +0x08 and clears +0x0c. A byte
// under the threshold clears the bit. The frame it falls sets +0x0c and
// clears +0x08. Holding either state clears the edge that is not firing.
void apply_analog_bit(PadBitPort& slot, u8 level, u32 mask)
{
    if (level >= kAnalogPress)
    {
        if ((slot.down_prev & mask) != 0)
        {
            slot.pressed &= ~mask;
        }
        else
        {
            slot.pressed |= mask;
        }
        slot.down |= mask;
        slot.released &= ~mask;
        return;
    }
    if ((slot.down_prev & mask) != 0)
    {
        slot.released |= mask;
    }
    else
    {
        slot.released &= ~mask;
    }
    slot.down &= ~mask;
    slot.pressed &= ~mask;
}

} // namespace

// Xbox 0x00154BA0
void controller_tick_button_repeat(float dt)
{
    // Xbox 0x00154BA0
    // 0x001555B0 calls this after it has written each port's button word.
    // A clear bit stores 0 in that hold timer and clears the pulse bit. A set
    // bit adds dt to the hold timer. While the hold is still under
    // DAT_00434288 the pulse bit stays as it was. The frame the hold reaches
    // that delay sets the pulse bit and stores 0 in the repeat timer. After
    // that, each call adds dt to the repeat timer. Under DAT_00434290 the
    // pulse bit clears. At that gate the pulse bit sets and the repeat timer
    // subtracts DAT_00434288. The button word is then copied to +0x04. A clear
    // bit leaves its repeat timer unchanged.
    const float delay = g_pad_hold_delay;
    const float gate = g_pad_repeat_gate;
    for (PadBitPort& port : g_pad_bits)
    {
        for (int bit_index = 0; bit_index < kPadBitCount; ++bit_index)
        {
            const u32 bit = 1u << bit_index;
            if ((port.down & bit) == 0)
            {
                port.hold[bit_index] = 0.f;
                port.pulse &= ~bit;
                continue;
            }
            const float previous = port.hold[bit_index];
            const float held = previous + dt;
            port.hold[bit_index] = held;
            if (held < delay)
            {
                continue;
            }
            if (previous < delay)
            {
                port.pulse |= bit;
                port.repeat[bit_index] = 0.f;
                continue;
            }
            const float next = port.repeat[bit_index] + dt;
            port.repeat[bit_index] = next;
            if (next < gate)
            {
                port.pulse &= ~bit;
                continue;
            }
            port.pulse |= bit;
            port.repeat[bit_index] = next - delay;
        }
        port.down_prev = port.down;
    }
}

// Xbox 0x00154C90
void controller_set_left_stick(i32 port, i32 x, i32 y)
{
    // Xbox 0x00154C90
    // 0x001555B0 sign-extends the thumb LX and LY words and passes the port.
    // When DAT_00434284 is set it rewrites those integers first (retail points
    // it at 0x00155360). The integers land at +0x114 and +0x118. Each is
    // multiplied by 1/74 and clamped to [-1, 1] into +0x124 and +0x128. A Y
    // below -1 stores -1 and returns; that store matches the clamp.
    if (g_pad_left_hook != nullptr)
    {
        g_pad_left_hook(port, &x, &y);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.thumb_lx = x;
    slot.thumb_ly = y;
    slot.axis_lx = clamp_thumb(float(x) * kLeftThumbScale);
    slot.axis_ly = clamp_thumb(float(y) * kLeftThumbScale);
}

// Xbox 0x00154D70
void controller_set_right_stick(i32 port, i32 x, i32 y)
{
    // Xbox 0x00154D70
    // 0x001555B0 sign-extends the thumb RX and RY words and passes the port.
    // When DAT_00434280 is set it rewrites those integers first (retail points
    // it at 0x001553B0). The integers land at +0x11c and +0x120. Each is
    // multiplied by DAT_00203A90 (1/59) and clamped to [-1, 1] into +0x12c and
    // +0x130. A Y below -1 stores -1 and returns; that store matches the clamp.
    if (g_pad_right_hook != nullptr)
    {
        g_pad_right_hook(port, &x, &y);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.thumb_rx = x;
    slot.thumb_ry = y;
    slot.axis_rx = clamp_thumb(float(x) * kRightThumbScale);
    slot.axis_ry = clamp_thumb(float(y) * kRightThumbScale);
}

// Xbox 0x00154E50
void controller_set_triggers(i32 port, u8 left, u8 right)
{
    // Xbox 0x00154E50
    // cam_155624 passes the left and right trigger bytes from the gamepad.
    // When DAT_0043428c is set it rewrites those bytes first (retail points
    // it at 0x00155400) and the mask is 0xC000. The bytes land at +0x13a and
    // +0x13b. 0x00154A70 then tests each against 30: left is bit 0x4000 and
    // right is bit 0x8000.
    if (g_pad_analog_hook != nullptr)
    {
        g_pad_analog_hook(port, 0xc000, &left, &right);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.trigger_l = left;
    slot.trigger_r = right;
    apply_analog_bit(slot, left, kTriggerLeftBit);
    apply_analog_bit(slot, right, kTriggerRightBit);
}

// Xbox 0x00154F00
void controller_set_buttons_ab(i32 port, u8 face_a, u8 face_b)
{
    // Xbox 0x00154F00
    // 0x001555B0 passes the A and B analog bytes from the gamepad (+2 and +3).
    // When DAT_0043428c is set it rewrites those bytes first (retail points
    // it at 0x00155400) and the mask is 0x0300. The bytes land at +0x134 and
    // +0x135. 0x00154A70 then tests each against 30: A is bit 0x0100 and B is
    // bit 0x0200.
    if (g_pad_analog_hook != nullptr)
    {
        g_pad_analog_hook(port, 0x300, &face_a, &face_b);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.face_a = face_a;
    slot.face_b = face_b;
    apply_analog_bit(slot, face_a, kFaceABit);
    apply_analog_bit(slot, face_b, kFaceBBit);
}

// Xbox 0x00154F80
void controller_set_buttons_xy(i32 port, u8 face_x, u8 face_y)
{
    // Xbox 0x00154F80
    // 0x001555B0 passes the X and Y analog bytes from the gamepad (+4 and +5).
    // When DAT_0043428c is set it rewrites those bytes first (retail points
    // it at 0x00155400) and the mask is 0x0C00. The bytes land at +0x136 and
    // +0x137. 0x00154A70 then tests each against 30: X is bit 0x0400 and Y is
    // bit 0x0800.
    if (g_pad_analog_hook != nullptr)
    {
        g_pad_analog_hook(port, 0xc00, &face_x, &face_y);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.face_x = face_x;
    slot.face_y = face_y;
    apply_analog_bit(slot, face_x, kFaceXBit);
    apply_analog_bit(slot, face_y, kFaceYBit);
}

// Xbox 0x00155000
void controller_set_buttons_black_white(i32 port, u8 black, u8 white)
{
    // Xbox 0x00155000
    // 0x001555B0 passes the black and white analog bytes from the gamepad
    // (+6 and +7). When DAT_0043428c is set it rewrites those bytes first
    // (retail points it at 0x00155400) and the mask is 0x3000. The bytes land
    // at +0x138 and +0x139. 0x00154A70 then tests each against 30: black is
    // bit 0x1000 and white is bit 0x2000.
    if (g_pad_analog_hook != nullptr)
    {
        g_pad_analog_hook(port, 0x3000, &black, &white);
    }
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.black = black;
    slot.white = white;
    apply_analog_bit(slot, black, kBlackBit);
    apply_analog_bit(slot, white, kWhiteBit);
}

// Xbox 0x001550A0
void controller_apply_pressed_edges(i32 port, u32 bits, u32 mask)
{
    // Xbox 0x001550A0
    // cam_155624 calls this with the port, the rising edge of the button word
    // ((current ^ previous) & current), and 0xFF. The probe is movzx from SI,
    // so only bits 0..15 can match. A mask bit that is clear in the edge
    // clears pressed (+0x08). A mask bit that is set ors pressed and the
    // button word (+0x00) and clears released (+0x0c). Control then jumps to
    // 0x00154B00, which writes 0 or 0xFF into the face and black/white bytes
    // from button bits 0x100, 0x200, 0x400, 0x800, 0x1000, and 0x2000.
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    u32 probe = 1u;
    for (int remaining = 0x20; remaining != 0; --remaining)
    {
        const u32 bit = probe & 0xffffu;
        if ((mask & bit) != 0)
        {
            if ((bits & bit) == 0)
            {
                slot.pressed &= ~bit;
            }
            else
            {
                slot.pressed |= bit;
                slot.down |= bit;
                slot.released &= ~bit;
            }
        }
        probe <<= 1;
    }
    slot.face_a = ((slot.down & kFaceABit) != 0) ? u8{0xff} : u8{0};
    slot.face_b = ((slot.down & kFaceBBit) != 0) ? u8{0xff} : u8{0};
    slot.face_x = ((slot.down & kFaceXBit) != 0) ? u8{0xff} : u8{0};
    slot.face_y = ((slot.down & kFaceYBit) != 0) ? u8{0xff} : u8{0};
    slot.black = ((slot.down & kBlackBit) != 0) ? u8{0xff} : u8{0};
    slot.white = ((slot.down & kWhiteBit) != 0) ? u8{0xff} : u8{0};
}

// Xbox 0x00155110
void controller_apply_released_edges(i32 port, u32 bits, u32 mask)
{
    // Xbox 0x00155110
    // cam_155624 calls this after 0x001550A0 with the port, the falling edge
    // of the button word ((current ^ previous) & previous), and 0xFF. The
    // probe is the walking mask itself, so bits 16..31 match when the mask
    // has them. A mask bit that is clear in the edge clears released (+0x0c).
    // A mask bit that is set ors released, clears the button word (+0x00),
    // and clears pressed (+0x08). Control then jumps to 0x00154B00, which
    // writes 0 or 0xFF into the face and black/white bytes from button bits
    // 0x100, 0x200, 0x400, 0x800, 0x1000, and 0x2000.
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    u32 probe = 1u;
    for (int remaining = 0x20; remaining != 0; --remaining)
    {
        if ((mask & probe) != 0)
        {
            if ((bits & probe) == 0)
            {
                slot.released &= ~probe;
            }
            else
            {
                slot.released |= probe;
                slot.down &= ~probe;
                slot.pressed &= ~probe;
            }
        }
        probe <<= 1;
    }
    slot.face_a = ((slot.down & kFaceABit) != 0) ? u8{0xff} : u8{0};
    slot.face_b = ((slot.down & kFaceBBit) != 0) ? u8{0xff} : u8{0};
    slot.face_x = ((slot.down & kFaceXBit) != 0) ? u8{0xff} : u8{0};
    slot.face_y = ((slot.down & kFaceYBit) != 0) ? u8{0xff} : u8{0};
    slot.black = ((slot.down & kBlackBit) != 0) ? u8{0xff} : u8{0};
    slot.white = ((slot.down & kWhiteBit) != 0) ? u8{0xff} : u8{0};
}

// Xbox 0x00155260
void controller_clear_port(i32 port)
{
    // Xbox 0x00155260
    // cam_155624 calls this when 0x00019140 returns non-zero, and when that
    // port's bit is clear. The button words at +0x00 through +0x10 become 0.
    // Each of the 32 hold timers at +0x14 is stored as 1. The repeat timers
    // at +0x94 are left unchanged.
    // Thumb words, the four stick axes, and the eight analog bytes become 0.
    if (port < 0 || port >= kPadBitPorts)
    {
        return;
    }
    PadBitPort& slot = g_pad_bits[port];
    slot.down = 0;
    slot.down_prev = 0;
    slot.pressed = 0;
    slot.released = 0;
    slot.pulse = 0;
    for (int bit_index = 0; bit_index < kPadBitCount; ++bit_index)
    {
        slot.hold[bit_index] = 1.f;
    }
    slot.thumb_lx = 0;
    slot.thumb_ly = 0;
    slot.thumb_rx = 0;
    slot.thumb_ry = 0;
    slot.axis_lx = 0.f;
    slot.axis_ly = 0.f;
    slot.axis_rx = 0.f;
    slot.axis_ry = 0.f;
    slot.face_a = 0;
    slot.face_b = 0;
    slot.face_x = 0;
    slot.face_y = 0;
    slot.black = 0;
    slot.white = 0;
    slot.trigger_l = 0;
    slot.trigger_r = 0;
}

// Xbox 0x001557B0
void controller_tick_feedback(PadFeedback* feedback, float dt)
{
    // Xbox 0x001557B0
    // cam_155624 calls this after 0x00154BA0 with the feedback base
    // DAT_00434788 and the frame step. Each of the four slots at DAT_00434910
    // whose timer is not zero loses dt. The decremented value is what stays in
    // the timer, so a result below zero keeps falling on later frames. When
    // that result is at or below zero, the active byte and both motor floats
    // become zero. An exact zero timer is left unchanged. A NaN timer stays
    // NaN and the motor floats keep their values.
    // The write loop then runs for every slot. Status and event at +0 and +4
    // become 0. Each motor float is multiplied by DAT_00203A98 (65535) and
    // chopped through 0x001673D8 into the words at +0x42 and +0x44. Packets
    // are 0x46 bytes apart.
    for (RumbleSlot& slot : g_rumble)
    {
        if (slot.timer != 0.f)
        {
            slot.timer -= dt;
            if (slot.timer <= 0.f)
            {
                slot.active = 0;
                slot.left = 0.f;
                slot.right = 0.f;
            }
        }
    }
    auto* bytes = reinterpret_cast<u8*>(feedback);
    for (const RumbleSlot& slot : g_rumble)
    {
        PadFeedback& packet = *reinterpret_cast<PadFeedback*>(bytes);
        packet.status = 0;
        packet.event = 0;
        packet.left = rumble_word(slot.left);
        packet.right = rumble_word(slot.right);
        bytes += kPadFeedbackStride;
    }
}

// Xbox 0x00019140
u8 controller_packet_rejected(i32 port, const void* packet)
{
    // Xbox 0x00019140
    // cam_155624 calls this after the port bit in DAT_0032E3B4 is set. EDI is
    // the port. ESI is the 0x16-byte gamepad (buttons at +0, analog bytes at
    // +2, thumbs at +0x0A) in the four-port table at DAT_004348B4. A non-zero
    // AL takes the same path as a clear bit: 0x00155260 clears that port's
    // 0x13c record. Retail is xor al, al; ret, so both arguments stay unread
    // and AL is 0. The stick and button writers then run for that port.
    (void)port;
    (void)packet;
    return 0;
}


// Xbox 0x00155930
void stop_pad_rumble(u8 port)
{
    g_rumble[port].active = 0; // DAT_00434912 + port * 0x14
    g_rumble[port].left = 0.f; // DAT_00434918
    g_rumble[port].right = 0.f; // DAT_0043491C
}

} // namespace gbtg
