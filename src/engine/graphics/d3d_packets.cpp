#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#if !defined(_WIN32)
#include <cfenv>
#endif

#if defined(_WIN32)
#include <excpt.h>
#include <float.h>
#include <malloc.h>
extern "C"
{
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* module, const char* name);
    __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();
    __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(void* time);
    __declspec(dllimport) unsigned long __stdcall GetTickCount();
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(void* counter);
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
                                                        const std::uintptr_t* arguments);
    __declspec(dllimport) int __stdcall CloseHandle(void* handle);
    __declspec(dllimport) int __stdcall FlushFileBuffers(void* handle);
    __declspec(dllimport) unsigned long __stdcall GetLastError();
    __declspec(dllimport) int __stdcall ReadFile(void* handle, void* buffer, unsigned long bytes, unsigned long* read,
                                                 void* overlapped);
    __declspec(dllimport) int __stdcall WriteFile(void* handle, const void* buffer, unsigned long bytes,
                                                  unsigned long* written, void* overlapped);
    __declspec(dllimport) unsigned long __stdcall SetFilePointer(void* handle, long distance, long* high,
                                                                 unsigned long method);
    __declspec(dllimport) int __stdcall IsBadReadPtr(const void* lp, std::uintptr_t ucb);
    __declspec(dllimport) int __stdcall IsBadWritePtr(void* lp, std::uintptr_t ucb);
}
#endif


#include "../scene/scene_types.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{




// Transform programs from the D3D section. Each instruction is four dwords.
// 0x0017FC28 is used while DAT_00181CE4 is 0. 0x0017FB78 is used when that
// global is set and device flag bit 1 is set. 0x0017FAB8 is the other set.
constexpr u32 k_d3d_xform_mode0[] = {
    0x00000000u, 0x0020001Bu, 0x0836106Cu, 0x2F100FF8u, 0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u,
    0x00000000u, 0x002008FFu, 0x0836106Cu, 0x2070F828u, 0x00000000u, 0x0240081Bu, 0x1436186Cu, 0x2F20F824u,
    0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u, 0x00000000u, 0x00200200u, 0x0836106Cu, 0x2070F830u,
    0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u, 0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u,
    0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u, 0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u,
    0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u, 0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};

constexpr u32 k_d3d_xform_flag[] = {
    0x00000000u, 0x022000AAu, 0x0836086Cu, 0x201FF828u, 0x00000000u, 0x0020081Bu, 0x0836106Cu, 0x2070F820u,
    0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u, 0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u,
    0x00000000u, 0x0240021Bu, 0x14361800u, 0x2F20F834u, 0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u,
    0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u, 0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u,
    0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u, 0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u,
    0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};

constexpr u32 k_d3d_xform_main[] = {
    0x00000000u, 0x0020001Bu, 0x0836106Cu, 0x2F100FF8u, 0x00000000u, 0x0420061Bu, 0x083613FCu, 0x5011F818u,
    0x00000000u, 0x0400001Bu, 0x083613FCu, 0x2070F82Cu, 0x00000000u, 0x0240081Bu, 0x1436186Cu, 0x2F20F824u,
    0x00000000u, 0x0060201Bu, 0x2436106Cu, 0x3070F800u, 0x00000000u, 0x00200200u, 0x0836106Cu, 0x2070F830u,
    0x00000000u, 0x00200E1Bu, 0x0836106Cu, 0x2070F838u, 0x00000000u, 0x0020101Bu, 0x0836106Cu, 0x2070F840u,
    0x00000000u, 0x0020121Bu, 0x0836106Cu, 0x2070F848u, 0x00000000u, 0x0020141Bu, 0x0836106Cu, 0x2070F850u,
    0x00000000u, 0x0020161Bu, 0x0836106Cu, 0x2070F858u, 0x00000000u, 0x0020181Bu, 0x0836106Cu, 0x2070F861u,
};


// DAT_00181E08. BeginStateBig reads this device, not the fastcall argument.
// Vertex-shader constant file is 192 registers. Program cache is 136 slots.
constexpr u32 k_d3d_vs_const_regs = 192u;

constexpr u32 k_d3d_vs_program_slots = 136u;


struct D3dScreenDevice
{
    u32* cursor = nullptr; // +0
    u32* fence = nullptr;  // +4
    u32 flags = 0;         // +8
    u8* active = nullptr;  // +0x790. Byte +4 bit 1 allows the upload.
    u8* depth = nullptr;   // +0x19f8. Byte +0xD is the surface D3DFORMAT.
    float proj_clip = 0.f; // +0x938. Projection factor for sample mode 2.
    float span_x = 0.f;    // +0x940
    float span_y = 0.f;    // +0x944
    float scale_x = 0.f;   // +0x958
    float scale_y = 0.f;   // +0x95c
    i32 view_x = 0;        // +0xed0
    i32 view_y = 0;        // +0xed4
    i32 view_w = 0;        // +0xed8
    i32 view_h = 0;        // +0xedc
    float min_z = 0.f;     // +0xee0
    float max_z = 0.f;     // +0xee4
    float origin_x = 0.f;  // +0xee8
    float origin_y = 0.f;  // +0xeec
    float clip_min_screen = 0.f; // +0x1bf8
    float clip_max_screen = 0.f; // +0x1bfc
    float clip_min_view = 0.f;   // +0x1c00
    float clip_max_view = 0.f;   // +0x1c04
    u8 clip_lock = 0;            // +0x1c08. Bit 0 keeps the screen pair, bit 1 the viewport pair.
    // +0x1098. 136 instruction slots, four dwords each. The next device
    // dword is +0x1918. 0x00172EE0 writes here; 0x00176080 reads the head.
    u32 program[k_d3d_vs_program_slots * 4u]{};
    std::vector<u32> push;
    u32 cursor_index = 0;
    u32 fence_index = 0;
};


D3dScreenDevice* g_d3d_device = nullptr;

// DAT_00180d50. Four dwords per register. 192 * 16 ends at 0x00181950.
u32 g_d3d_vs_constants[k_d3d_vs_const_regs * 4u]{};

i32 g_d3d_program_mode = 0;
   // DAT_00181CE4
i32 g_d3d_sample_mode = 0;
    // DAT_00181DAC
i32 g_d3d_multisample_aa = 0;
 // DAT_00181DD0, D3DRS_MULTISAMPLEANTIALIAS
i32 g_d3d_yuv_enable = 0;
     // DAT_00181DF0, D3DRS_YUVENABLE

void d3d_bind_cursor(D3dScreenDevice& device)
{
    if (device.push.empty())
    {
        device.cursor = nullptr;
        device.fence = nullptr;
        return;
    }
    device.cursor = device.push.data() + device.cursor_index;
    device.fence = device.push.data() + device.fence_index;
}


u32* d3d_begin_state_big(u32 count)
{
    // Slice of 0x00177710 used by 0x00173900. The cursor must sit `count`
    // dwords before the fence plus 0x200 bytes. A miss grows through
    // 0x001775C0. DAT_001845AC starts at 0, so the grown size is
    // count*4+0x204. A null global device has nothing to write into.
    D3dScreenDevice* device = g_d3d_device;
    if (device == nullptr)
    {
        return nullptr;
    }
    const u32 end_byte = device->cursor_index * 4u + count * 4u;
    const u32 fence_limit = device->fence_index * 4u + 0x200u;
    if (end_byte >= fence_limit)
    {
        u32 bytes = count * 4u + 0x204u;
        const u32 have = device->fence_index * 4u;
        if (bytes < have)
        {
            bytes = have;
        }
        device->fence_index = (bytes + 3u) / 4u;
    }
    const u32 need = device->cursor_index + count;
    if (device->push.size() < need || device->push.size() < device->fence_index)
    {
        device->push.resize(std::max(need, device->fence_index));
    }
    d3d_bind_cursor(*device);
    return device->cursor;
}


float d3d_u32_float(i32 bits)
{
    return static_cast<float>(static_cast<u32>(bits));
}


u32 d3d_float_bits(float value)
{
    return std::bit_cast<u32>(value);
}


void d3d_bias_multisample(float& x, float& y, u32 flags)
{
    if ((flags & 0x8000u) != 0 && g_d3d_multisample_aa != 0)
    {
        x -= 0.5f;
        y -= 0.5f;
    }
}


// Xbox 0x001710E0
void d3d_viewport_offset_and_scale(float* offset, float* scale)
{
    // Xbox 0x001710E0
    // stdcall (ret 8). Only caller is 0x00171C20. Ghidra names the call
    // D3DDevice_GetViewportOffsetAndScale; the body reads DAT_00181E08,
    // not the fastcall device. A null global would fault on Xbox.
    //
    // Corner is view_x/view_y converted as unsigned pixels, times the
    // multisample scales, plus the viewport origin. Flag bit 0x8000 and
    // a nonzero D3DRS_MULTISAMPLEANTIALIAS subtract 0.5 from both axes.
    // Width and height become half extents (height is negated). Offset is
    // the biased corner plus those extents, with z = min_z * span_y.
    // Scale is (half width, negated half height, (max_z-min_z)*span_y, 0).
    if (offset == nullptr || scale == nullptr)
    {
        return;
    }
    if (g_d3d_device == nullptr)
    {
        offset[0] = offset[1] = offset[2] = offset[3] = 0.f;
        scale[0] = scale[1] = scale[2] = scale[3] = 0.f;
        return;
    }
    const D3dScreenDevice& device = *g_d3d_device;
    float x = d3d_u32_float(device.view_x) * device.scale_x + device.origin_x;
    float y = d3d_u32_float(device.view_y) * device.scale_y + device.origin_y;
    d3d_bias_multisample(x, y, device.flags);
    const float half_w = d3d_u32_float(device.view_w) * device.scale_x * 0.5f;
    const float half_h = d3d_u32_float(device.view_h) * device.scale_y * -0.5f;
    offset[0] = x + half_w;
    offset[1] = y - half_h;
    offset[2] = device.min_z * device.span_y;
    offset[3] = 0.f;
    scale[0] = half_w;
    scale[1] = half_h;
    scale[2] = (device.max_z - device.min_z) * device.span_y;
    scale[3] = 0.f;
}


void d3d_store_sample_clip(float& clip_min, float& clip_max, const D3dScreenDevice& device)
{
    clip_max = device.span_y;
    clip_min = (device.span_x * device.proj_clip) * device.span_y;
}


// Xbox 0x00171C20
u32* d3d_emit_viewport_packets(D3dScreenDevice* device, u32* packet)
{
    // Xbox 0x00171C20
    // Fastcall. Writes the viewport push-buffer words and returns the
    // cursor after them. Callers pass the device in ecx and the cursor
    // in edx, then store eax back at device+0. A null active record
    // would fault on Xbox; the port leaves the cursor alone.
    //
    // Byte +4 of the record at +0x790 selects the packet. Bits 0x12 clear
    // emits SET_VIEWPORT_OFFSET (0x100A20, four floats) of the unsigned
    // pixel corner (view_x, view_y) times scale plus origin, z and w zero,
    // with the same 0.5 bias as 0x001710E0. Either bit set calls 0x001710E0
    // on DAT_00181E08 and, unless flag bit 0x200 is set, emits that offset
    // plus SET_VIEWPORT_SCALE (0x100AF0, four floats).
    //
    // Both paths then emit SET_CLIP_MIN/SET_CLIP_MAX (0x00080394, two
    // floats). Bit 0 of +0x1c08 keeps the screen pair at +0x1bf8/+0x1bfc.
    // Bit 1 keeps the viewport pair at +0x1c00/+0x1c04. Otherwise sample
    // mode 2 stores (span_x * proj_clip * span_y, span_y). The screen path
    // otherwise stores (min_z * span_y, max_z * span_y). The viewport path
    // otherwise stores (0, span_y).
    if (device == nullptr || packet == nullptr || device->active == nullptr)
    {
        return packet;
    }

    float clip_min = 0.f;
    float clip_max = 0.f;
    if ((device->active[4] & 0x12) == 0)
    {
        float x = d3d_u32_float(device->view_x) * device->scale_x + device->origin_x;
        float y = d3d_u32_float(device->view_y) * device->scale_y + device->origin_y;
        d3d_bias_multisample(x, y, device->flags);
        packet[0] = 0x100a20u;
        packet[1] = d3d_float_bits(x);
        packet[2] = d3d_float_bits(y);
        packet[3] = 0;
        packet[4] = 0;
        packet += 5;
        if ((device->clip_lock & 1u) == 0)
        {
            if (g_d3d_sample_mode == 2)
            {
                d3d_store_sample_clip(device->clip_min_screen, device->clip_max_screen, *device);
            }
            else
            {
                device->clip_min_screen = device->min_z * device->span_y;
                device->clip_max_screen = device->max_z * device->span_y;
            }
        }
        clip_min = device->clip_min_screen;
        clip_max = device->clip_max_screen;
    }
    else
    {
        float offset[4]{};
        float scale[4]{};
        d3d_viewport_offset_and_scale(offset, scale);
        if ((device->flags & 0x200u) == 0)
        {
            packet[0] = 0x100a20u;
            packet[1] = d3d_float_bits(offset[0]);
            packet[2] = d3d_float_bits(offset[1]);
            packet[3] = d3d_float_bits(offset[2]);
            packet[4] = d3d_float_bits(offset[3]);
            packet[5] = 0x100af0u;
            packet[6] = d3d_float_bits(scale[0]);
            packet[7] = d3d_float_bits(scale[1]);
            packet[8] = d3d_float_bits(scale[2]);
            packet[9] = d3d_float_bits(scale[3]);
            packet += 10;
        }
        if ((device->clip_lock & 2u) == 0)
        {
            if (g_d3d_sample_mode == 2)
            {
                d3d_store_sample_clip(device->clip_min_view, device->clip_max_view, *device);
            }
            else
            {
                device->clip_min_view = 0.f;
                device->clip_max_view = device->span_y;
            }
        }
        clip_min = device->clip_min_view;
        clip_max = device->clip_max_view;
    }

    packet[0] = 0x00080394u;
    packet[1] = d3d_float_bits(clip_min);
    packet[2] = d3d_float_bits(clip_max);
    return packet + 3;
}


// Xbox 0x00171E10
u32* d3d_emit_control_packet(D3dScreenDevice* device, u32* packet)
{
    // Xbox 0x00171E10
    // Fastcall. Writes one NV097_SET_CONTROL0 word (0x40290: method 0x290,
    // count 1) and returns the cursor after it. Callers pass the device in
    // ecx and the cursor in edx, then store eax at device+0. A null device
    // or cursor would fault on Xbox; the port leaves the cursor alone.
    //
    // The data dword starts at 0x100001. Nonzero D3DRS_YUVENABLE
    // (DAT_00181DF0) replaces that with 0x10100001 (bit 24). Sample mode 2
    // (DAT_00181DAC) sets bit 0x10000. A depth surface at +0x19f8 whose
    // format byte +0xD is a float Z format sets bit 0x1000: 0x2B F24S8,
    // 0x2D F16, 0x2F LIN_F24S8, 0x31 LIN_F16.
    //
    // The sample-mode setter at 0x001722A0 calls this when mode 2 is entered
    // or left, then 0x00171C20. The YUV setter at 0x00172430 stores its
    // argument at DAT_00181DF0 first. Render-target setup at 0x00174335
    // calls it with the device in ecx.
    if (device == nullptr || packet == nullptr)
    {
        return packet;
    }

    u32 control = 0x100001u;
    if (g_d3d_yuv_enable != 0)
    {
        control = 0x10100001u;
    }
    if (g_d3d_sample_mode == 2)
    {
        control |= 0x10000u;
    }
    if (device->depth != nullptr)
    {
        const u8 format = device->depth[0x0d];
        if (format == 0x2bu || format == 0x2du || format == 0x2fu || format == 0x31u)
        {
            control |= 0x1000u;
        }
    }
    packet[0] = 0x40290u;
    packet[1] = control;
    return packet + 2;
}


// Xbox 0x00172720
void d3d_copy_push_words(u32* dest, const u32* src, int count)
{
    // Xbox 0x00172720
    // stdcall (ret 0xC). Copies `count` dwords from src to dest, low to high.
    // 0x001727B0 is the only caller. It takes this path when the push-buffer
    // run is at least 0x80 dwords, the dword at device+0xc44 is 1, and the
    // offset formed from device+0xbc0 is 0. Shorter runs there use rep movsd.
    //
    // The prefix length is ((-dest) & 0x1F) >> 2, the dwords until dest is
    // 32-byte aligned. A count greater than that prefix is a rep movsd of
    // the prefix, then 16-dword chunks, then a rep movsd tail. Each chunk
    // is movq into all eight MMX registers and movntq back out, with
    // prefetchnta at src+0x40 and src+0x60, so the chunk is read before it
    // is written. A count that does not exceed the prefix is one rep movsd.
    // EMMS runs before the return.
    if (count <= 0)
    {
        return;
    }

    const unsigned raw = static_cast<unsigned>(-static_cast<int>(reinterpret_cast<std::uintptr_t>(dest)));
    const int align = static_cast<int>((raw & 0x1Fu) >> 2);
    int left = count;
    if (align < left)
    {
        left -= align;
        for (int n = align; n != 0; --n)
        {
            *dest = *src;
            ++dest;
            ++src;
        }
        while (left >= 0x10)
        {
            u32 chunk[16];
            std::memcpy(chunk, src, sizeof(chunk));
            std::memcpy(dest, chunk, sizeof(chunk));
            dest += 16;
            src += 16;
            left -= 0x10;
        }
    }
    for (; left != 0; --left)
    {
        *dest = *src;
        ++dest;
        ++src;
    }
}


// DAT_0017FCE8. Each texture coordinate's size is a two-bit field starting
// at FVF bit 16: 0 is two floats, 1 is three, 2 is four, 3 is one.
constexpr u32 k_fvf_tex_components[4] = {2u, 3u, 4u, 1u};


// DAT_0017FA34. Bytes in one component, indexed by the low nibble of a
// D3DVSDT code: 0 color, 1 normalized short, 2 float, 4 packed byte,
// 5 short, 6 packed normal. Indices 8..15 are the bytes that follow the
// eight-byte table in the image.
constexpr u8 k_d3d_vsdt_bytes[16] = {
    0x01, 0x02, 0x04, 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x03,
};


// DAT_0017FA3C. Component count, indexed by the high nibble. Nibble 7 is
// FLOAT2H and counts as three floats. Indices 8..15 spill into the 1.0f
// constant at 0x0017FA48.
constexpr u8 k_d3d_vsdt_count[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f,
};


// One vertex-shader input register. type at +0 is the D3DVSDT code.
// source at +4 is 0 for a stream load, 1 for a tessellated normal, and 2
// for a tessellated UV. tess_in at +5 is the tessellator input register.
// stream at +8 and byte_offset at +0xc belong to the next register.
struct D3dFvfElement
{
    u32 type;
    u8 source;
    u8 tess_in;
    u16 unused6;
    u32 stream;
    u32 byte_offset;
};


// Parsed FVF declaration. 0x001739F0 keeps one at 0x00184478.
// element[0] is +0x1c. Sixteen registers are typed; further texture
// coordinates continue the same 0x10 stride. Register 0's stream index
// and vertex byte offset live at +0x14 and +0x18, the same slots each
// element keeps for the following register.
struct D3dFvfDecl
{
    u32 unused0;
    u32 flags; // +4
    u32 unused8;
    u32 unusedc;
    u32 tex_mask; // +0x10
    u32 reg0_stream; // +0x14
    u32 reg0_offset; // +0x18
    D3dFvfElement element[24];
};


static_assert(offsetof(D3dFvfDecl, flags) == 4);

static_assert(offsetof(D3dFvfDecl, tex_mask) == 0x10);

static_assert(offsetof(D3dFvfDecl, reg0_stream) == 0x14);

static_assert(offsetof(D3dFvfDecl, reg0_offset) == 0x18);

static_assert(offsetof(D3dFvfDecl, element) == 0x1c);

static_assert(offsetof(D3dFvfElement, source) == 4);

static_assert(offsetof(D3dFvfElement, tess_in) == 5);

static_assert(offsetof(D3dFvfElement, stream) == 8);

static_assert(offsetof(D3dFvfElement, byte_offset) == 0xc);

static_assert(sizeof(D3dFvfElement) == 0x10);


// Xbox 0x00172DA0
void d3d_expand_fvf_declaration(D3dFvfDecl* decl, u32 fvf)
{
    // Xbox 0x00172DA0
    // stdcall (ret 8). Expands an FVF into the declaration registers.
    // 0x001739F0 is the only caller: when the shader argument's low bit
    // is clear it passes the declaration at 0x00184478 and that FVF.
    // A null declaration would fault on Xbox; the port returns.
    //
    // +4 and +0x10 are cleared. The type dword of registers 0..15 is set
    // to 2. Every other dword is left as the caller left it.
    //
    // FVF & 0xE selects the position. 4 is XYZRHW: register 0 becomes
    // 0x42 (FLOAT4), the vertex is 16 bytes, and flag bit 1 is set. Any
    // other nonzero code is XYZ: register 0 becomes 0x32 (FLOAT3) and the
    // vertex is 12 bytes. Codes of 6 and up are XYZBn. The weight count
    // is ((code - 6) >> 1) + 1, stored in register 1 as (count << 4) | 2,
    // with byte offset 12 at register 0's +0xc. The running size becomes
    // count * 4 + 12.
    //
    // Bit 0x10 adds a normal at register 2 (FLOAT3, 12 bytes). Bit 0x40
    // adds diffuse at register 3 (0x40, D3DCOLOR, 4 bytes) and sets flag
    // 0x400. Bit 0x80 adds specular at register 4 (D3DCOLOR, 4 bytes) and
    // sets flag 0x800. Each attribute's vertex offset is the size
    // accumulated before it, written at the previous register's +0xc.
    //
    // Bits 11:8 are the texture-coordinate count. Coordinate i consumes
    // the next two bits from FVF bit 16, maps them through DAT_0017FCE8,
    // and writes register 9+i as (components << 4) | 2. The running size
    // grows by components * 4. +0x10 ORs components << (i * 8). The shift
    // is masked to five bits, matching x86 shl.
    if (decl == nullptr)
    {
        return;
    }

    decl->flags = 0;
    decl->tex_mask = 0;
    for (int reg = 0; reg < 16; ++reg)
    {
        decl->element[reg].type = 2;
    }

    const u32 position = fvf & 0xeu;
    u32 cursor = 0;
    if (position == 4u)
    {
        decl->flags |= 2u;
        decl->element[0].type = 0x42u;
        cursor = 0x10u;
    }
    else if (position != 0u)
    {
        decl->element[0].type = 0x32u;
        cursor = 0xcu;
        if (position >= 6u)
        {
            const u32 weights = ((position - 6u) >> 1) + 1u;
            decl->element[0].byte_offset = 0xcu;
            decl->element[1].type = (weights << 4) | 2u;
            cursor = weights * 4u + 0xcu;
        }
    }

    if ((fvf & 0x10u) != 0u)
    {
        decl->element[1].byte_offset = cursor;
        decl->element[2].type = 0x32u;
        cursor += 0xcu;
    }
    if ((fvf & 0x40u) != 0u)
    {
        decl->element[2].byte_offset = cursor;
        decl->element[3].type = 0x40u;
        decl->flags |= 0x400u;
        cursor += 4u;
    }
    if ((fvf & 0x80u) != 0u)
    {
        decl->element[3].byte_offset = cursor;
        decl->element[4].type = 0x40u;
        decl->flags |= 0x800u;
        cursor += 4u;
    }

    u32 tex_count = (fvf >> 8) & 0xfu;
    u32 tex_fmt = fvf >> 16;
    u32 shift = 0;
    u32 reg = 9;
    while (tex_count != 0u)
    {
        const u32 comps = k_fvf_tex_components[tex_fmt & 3u];
        decl->element[reg].type = (comps << 4) | 2u;
        decl->element[reg - 1u].byte_offset = cursor;
        cursor += comps * 4u;
        decl->tex_mask |= comps << (shift & 31u);
        tex_fmt >>= 2;
        shift += 8u;
        --tex_count;
        ++reg;
    }
}


// Xbox 0x00172EE0
void d3d_shadow_shader_program(u32* shader, u32 slot)
{
    // Xbox 0x00172EE0
    // stdcall (ret 8). Shadows a vertex-shader token stream into the
    // constant file and the transform-program cache. 0x001736A0 is the
    // only caller. It passes the shader (its handle minus one) and the
    // program slot, and only when device flag bit 0x10 is clear. A null
    // shader would fault on Xbox; the port returns.
    //
    // Tokens start at shader+0x114. A token is (count << 18) | method,
    // then count payload dwords. The word after each payload is the next
    // token, and a zero word ends the walk. The token under the cursor
    // is consumed even when its count is 0.
    //
    // Count 1 retargets the constant cursor and does not read the method.
    // The cursor becomes DAT_00180d50 + payload[0] * 16. It starts at the
    // shader object. Method 0xB00 (SET_TRANSFORM_PROGRAM) copies its
    // payload to device+0x1098 + slot * 16 and advances that cursor. Any
    // other method copies its payload to the constant cursor and advances
    // it. 0x00176080 later pushes the first eight program dwords back out
    // through 0x001738C0.
    //
    // A null device would fault on a 0xB00 copy; the port skips program
    // copies. A constant index of 192 or more would leave DAT_00180d50,
    // and a slot of 136 or more would leave the program cache; the port
    // keeps the previous constant cursor and drops program words that
    // do not fit.
    if (shader == nullptr)
    {
        return;
    }

    u32* token = shader + 0x45;
    u32* constants = shader;
    bool constants_in_file = false;
    u32* program = nullptr;
    u32 program_left = 0;
    if (g_d3d_device != nullptr && slot < k_d3d_vs_program_slots)
    {
        program = g_d3d_device->program + static_cast<size_t>(slot) * 4u;
        program_left = (k_d3d_vs_program_slots - slot) * 4u;
    }

    for (;;)
    {
        const u32 header = *token;
        const u32 count = header >> 18;
        const u32* data = token + 1;
        if (count == 1u)
        {
            const u32 index = data[0];
            if (index < k_d3d_vs_const_regs)
            {
                constants = g_d3d_vs_constants + static_cast<size_t>(index) * 4u;
                constants_in_file = true;
            }
        }
        else if ((header & 0xffffu) == 0xb00u)
        {
            if (program != nullptr && count != 0u)
            {
                u32 n = count;
                if (n > program_left)
                {
                    n = program_left;
                }
                if (n != 0u)
                {
                    std::memcpy(program, data, static_cast<size_t>(n) * sizeof(u32));
                    program += n;
                    program_left -= n;
                }
                if (n != count)
                {
                    program = nullptr;
                    program_left = 0;
                }
            }
        }
        else if (count != 0u && constants != nullptr)
        {
            u32 n = count;
            if (constants_in_file)
            {
                const u32* file_end = g_d3d_vs_constants + k_d3d_vs_const_regs * 4u;
                const u32 room = static_cast<u32>(file_end - constants);
                if (n > room)
                {
                    n = room;
                }
            }
            if (n != 0u)
            {
                std::memcpy(constants, data, static_cast<size_t>(n) * sizeof(u32));
            }
            if (n == count)
            {
                constants += count;
            }
            else
            {
                constants = nullptr;
                constants_in_file = false;
            }
        }
        token += 1u + count;
        if (*token == 0u)
        {
            break;
        }
    }
}


// Xbox 0x00172F90
u32* d3d_emit_transform_program(u32* dest, const u32* src, u32 count)
{
    // Xbox 0x00172F90
    // stdcall (ret 0xC). Writes `count` dwords from src as
    // SET_TRANSFORM_PROGRAM packets and returns the cursor after them.
    // 0x00173900 is the only caller. It passes the push-buffer cursor
    // after SET_TRANSFORM_PROGRAM_LOAD of slot 0, then stores the end
    // at device+0. A null dest would fault on Xbox; the port returns it.
    //
    // Each packet holds at most 0x20 payload dwords. The header is
    // (n << 18) | 0xB00, then those n dwords. The walk is a do-while, so
    // a zero count still writes one header of 0xB00 and does not read src.
    if (dest == nullptr)
    {
        return dest;
    }

    do
    {
        u32 n = count;
        if (n > 0x1fu)
        {
            n = 0x20u;
        }
        *dest = (n << 18) | 0xB00u;
        if (n != 0u)
        {
            std::memcpy(dest + 1, src, static_cast<size_t>(n) * sizeof(u32));
            src += n;
        }
        count -= n;
        dest += n + 1u;
    }
    while (count != 0u);
    return dest;
}


// Xbox 0x00173000
void d3d_parse_vertex_declaration(D3dFvfDecl* decl, const u32* tokens, int fixed_function)
{
    // Xbox 0x00173000
    // stdcall (ret 0xC). Walks a D3DVSD token stream into the declaration
    // registers. D3DDevice_CreateVertexShader (0x001732A0) calls it with
    // the new shader object, the declaration, and 1 when the shader
    // function pointer is null. A null declaration or token pointer would
    // fault on Xbox; the port returns. A register of 24 or more would
    // write past this declaration; the port skips that store.
    //
    // The top three bits select the token. 0 is NOP. 0x20000000 selects
    // the stream (low nibble) and resets the byte cursor. 0x40000000 with
    // bit 28 set skips: the count is bits 19:16, in dwords unless bit 27
    // is set, when the count is already in bytes. Bit 28 clear loads the
    // register in bits 4:0. Bits 23:16 are the D3DVSDT code, stored as the
    // register type. The stream index and byte cursor are stored in the
    // previous register (register 0 uses +0x14 and +0x18). The source byte
    // becomes 0.
    //
    // 0x60000000 is a tessellator token. The output register's stream and
    // offset are cleared, its type becomes 2, and byte +5 receives the
    // input register in bits 23:20. Bit 28 clear writes source 1 (normal)
    // and bit 28 set writes source 2 (UV).
    //
    // 0x80000000 is a constant-memory token. Bits 28:25 count 4-dword
    // vectors, and those dwords are skipped. Any other kind, including
    // D3DVSD_END (0xFFFFFFFF), returns.
    //
    // When fixed_function is set, registers 9..12 OR a component count
    // into +0x10. The count is the type's high nibble, except 7 becomes 4
    // (FLOAT2H) and type 0x16 becomes 3 (NORMPACKED3). The shift is
    // (register * 8 - 72), masked to five bits the way x86 shl masks its
    // count. Registers 3, 4, 7, and 8 set flag bits 0x400, 0x800, 0x1000,
    // and 0x2000. The cursor then grows by
    // DAT_0017FA3C[type >> 4] * DAT_0017FA34[type & 0xF].
    if (decl == nullptr || tokens == nullptr)
    {
        return;
    }

    u32 stream = 0;
    u32 cursor = 0;
    for (;;)
    {
        const u32 token = *tokens;
        ++tokens;
        const u32 kind = token & 0xe0000000u;
        if (kind > 0x60000000u)
        {
            if (kind != 0x80000000u)
            {
                return;
            }
            const u32 vectors = (token >> 0x19) & 0xfu;
            tokens += static_cast<size_t>(vectors) * 4u;
            continue;
        }
        if (kind == 0x60000000u)
        {
            const u32 reg = token & 0x1fu;
            if (reg < 24u)
            {
                if (reg == 0u)
                {
                    decl->reg0_stream = 0;
                    decl->reg0_offset = 0;
                }
                else
                {
                    decl->element[reg - 1u].stream = 0;
                    decl->element[reg - 1u].byte_offset = 0;
                }
                decl->element[reg].type = 2u;
                decl->element[reg].tess_in = static_cast<u8>((token >> 0x14) & 0xfu);
                decl->element[reg].source = (token & 0x10000000u) == 0u ? 1u : 2u;
            }
            continue;
        }
        if (kind == 0u)
        {
            continue;
        }
        if (kind == 0x20000000u)
        {
            stream = token & 0xfu;
            cursor = 0;
            continue;
        }

        if ((token & 0x10000000u) != 0u)
        {
            u32 skip = (token >> 16) & 0xfu;
            if ((token & 0x08000000u) == 0u)
            {
                skip <<= 2;
            }
            cursor += skip;
            continue;
        }

        const u32 type = (token >> 16) & 0xffu;
        const u32 reg = token & 0x1fu;
        if (reg < 24u)
        {
            if (reg == 0u)
            {
                decl->reg0_stream = stream;
                decl->reg0_offset = cursor;
            }
            else
            {
                decl->element[reg - 1u].stream = stream;
                decl->element[reg - 1u].byte_offset = cursor;
            }
            decl->element[reg].type = type;
            decl->element[reg].source = 0;
        }
        if (fixed_function != 0)
        {
            if (reg >= 9u && reg <= 0xcu)
            {
                u32 comps = (type >> 4) & 0xfu;
                if (comps == 7u)
                {
                    comps = 4u;
                }
                else if (type == 0x16u)
                {
                    comps = 3u;
                }
                const u32 shift = (reg * 8u - 0x48u) & 31u;
                decl->tex_mask |= comps << shift;
            }
            if (reg == 3u)
            {
                decl->flags |= 0x400u;
            }
            else if (reg == 4u)
            {
                decl->flags |= 0x800u;
            }
            else if (reg == 7u)
            {
                decl->flags |= 0x1000u;
            }
            else if (reg == 8u)
            {
                decl->flags |= 0x2000u;
            }
        }
        cursor += static_cast<u32>(k_d3d_vsdt_count[(type >> 4) & 0xfu]) *
                   static_cast<u32>(k_d3d_vsdt_bytes[type & 0xfu]);
    }
}


// Xbox 0x001731A0
u32 d3d_emit_declaration_constants(const u32* tokens, u32* dest)
{
    // Xbox 0x001731A0
    // stdcall (ret 8). Turns D3DVSD_CONST tokens into transform-constant
    // packets and returns how many dwords that is. D3DDevice_CreateVertexShader
    // (0x001732A0) calls it twice: first with a null dest, then shifts the
    // count by 2 to size the shader object; later it writes the packets
    // immediately after the program image that 0x00172F90 left at
    // shader+0x114. A null token pointer would fault on Xbox; the port
    // returns 0. A null dest is the size query and still walks the stream.
    //
    // Tokens run until D3DVSD_END (0xFFFFFFFF). A constant-memory token has
    // top bits 0x80000000. Bits 28:25 are the vector count and the low byte
    // is the constant index. Any other token, including NOP, is one dword
    // and does not change the index.
    //
    // The index starts at 0xDEADBEEF, so the first constant always emits
    // SET_TRANSFORM_CONSTANT_LOAD (0x41EA4) plus that index. A later token
    // emits the load only when its index is not the previous token's index
    // plus one. The stored index is the token's own index, not the index
    // after its vectors.
    //
    // Each run is a do-while of at most eight vectors. The header is
    // (n << 20) | 0xB80, SET_TRANSFORM_CONSTANT of n*4 dwords, then those
    // dwords. A count of zero still writes one 0xB80 header and reads no
    // payload. The dword count includes every header and payload dword.
    if (tokens == nullptr)
    {
        return 0;
    }

    u32 token = *tokens;
    ++tokens;
    u32 written = 0;
    u32 prev = 0xdeadbeefu;
    while (token != 0xffffffffu)
    {
        if ((token & 0xe0000000u) == 0x80000000u)
        {
            u32 count = (token >> 0x19) & 0xfu;
            const u32 address = token & 0xffu;
            if (address != prev + 1u)
            {
                if (dest != nullptr)
                {
                    dest[0] = 0x41ea4u;
                    dest[1] = address;
                    dest += 2;
                }
                written += 2u;
            }
            prev = address;
            do
            {
                u32 chunk = count;
                if (chunk >= 8u)
                {
                    chunk = 8u;
                }
                const u32 dwords = chunk * 4u;
                if (dest != nullptr)
                {
                    dest[0] = (chunk << 20) | 0xb80u;
                    if (dwords != 0u)
                    {
                        std::memcpy(dest + 1, tokens, static_cast<size_t>(dwords) * sizeof(u32));
                    }
                    dest += 1u + dwords;
                }
                tokens += dwords;
                written += dwords + 1u;
                count -= chunk;
            }
            while (count != 0u);
        }
        token = *tokens;
        ++tokens;
    }
    return written;
}


// Xbox 0x00173900
void d3d_reload_screen_transform(D3dScreenDevice* device)
{
    // Xbox 0x00173900
    // Fastcall. Reloads the screen-space transform program and constants 0
    // and 1. D3D_CommonSetMultiSampleModeAndScale (0x00171F40) calls it
    // after writing the multisample scales, the sample-mode setter at
    // 0x001722A0 calls it when mode 2 is entered or left, and the viewport
    // origin store at 0x00175060 calls it after writing +0xee8 / +0xeec.
    //
    // Byte +4 bit 1 on the record at +0x790 must be set. A null record
    // would fault on Xbox; the port returns.
    //
    // DAT_00181CE4 == 0 selects the 48-dword program at 0x0017FC28.
    // Otherwise flag bit 1 selects the 44-dword program at 0x0017FB78,
    // and a clear bit selects the 48-dword program at 0x0017FAB8.
    //
    // D3DDevice_BeginStateBig reserves program_dwords+0x1E. The packet is
    // SET_TRANSFORM_CONSTANT_LOAD (0x41EA4) of index 0, SET_TRANSFORM_CONSTANT
    // (0x200B80) of eight dwords, SET_TRANSFORM_PROGRAM_LOAD (0x41E9C) of
    // slot 0, then 0x00172F90 which prefixes each run of at most 0x20
    // program dwords with (count<<18)|0xB00 (SET_TRANSFORM_PROGRAM).
    // The cursor stored at device+0 becomes the end of that copy.
    //
    // Constant 0 is scale_x, scale_y, span_y, and either span_y*span_x
    // when DAT_00181DAC is 2 or 1.0. Constant 1 is the viewport origin
    // with a 0.5 bias subtracted when flag bit 0x8000 is set and
    // D3DRS_MULTISAMPLEANTIALIAS is nonzero, then two zeros.
    if (device == nullptr || device->active == nullptr || (device->active[4] & 2u) == 0)
    {
        return;
    }

    const u32* program = k_d3d_xform_mode0;
    u32 program_words = static_cast<u32>(sizeof(k_d3d_xform_mode0) / sizeof(k_d3d_xform_mode0[0]));
    if (g_d3d_program_mode != 0)
    {
        if ((device->flags & 2u) != 0)
        {
            program = k_d3d_xform_flag;
            program_words = static_cast<u32>(sizeof(k_d3d_xform_flag) / sizeof(k_d3d_xform_flag[0]));
        }
        else
        {
            program = k_d3d_xform_main;
            program_words = static_cast<u32>(sizeof(k_d3d_xform_main) / sizeof(k_d3d_xform_main[0]));
        }
    }

    u32* packet = d3d_begin_state_big(program_words + 0x1eu);
    if (packet == nullptr)
    {
        return;
    }

    float scale = 1.f;
    if (g_d3d_sample_mode == 2)
    {
        scale = device->span_y * device->span_x;
    }
    float bias = 0.f;
    if ((device->flags & 0x8000u) != 0 && g_d3d_multisample_aa != 0)
    {
        bias = 0.5f;
    }

    packet[0] = 0x41ea4u;
    packet[1] = 0;
    packet[2] = 0x200b80u;
    packet[3] = std::bit_cast<u32>(device->scale_x);
    packet[4] = std::bit_cast<u32>(device->scale_y);
    packet[5] = std::bit_cast<u32>(device->span_y);
    packet[6] = std::bit_cast<u32>(scale);
    packet[7] = std::bit_cast<u32>(device->origin_x - bias);
    packet[8] = std::bit_cast<u32>(device->origin_y - bias);
    packet[9] = 0;
    packet[10] = 0;
    packet[11] = 0x41e9cu;
    packet[12] = 0;

    u32* dest = d3d_emit_transform_program(packet + 0x0d, program, program_words);

    if (g_d3d_device != nullptr)
    {
        g_d3d_device->cursor_index = static_cast<u32>(dest - g_d3d_device->push.data());
        d3d_bind_cursor(*g_d3d_device);
        device->cursor = g_d3d_device->cursor;
    }
}


// Xbox 0x00176F10
void d3d_emit_transposed_matrix(u32* dest, u32 header, const u32* matrix)
{
    // Xbox 0x00176F10
    // stdcall (ret 0xC). Writes a 17-dword push-buffer packet. The first
    // dword is the method header. The next sixteen are the row-major 4x4
    // transposed, one source column per output row. Callers advance the
    // cursor by 0x44 themselves. A null dest or matrix would fault on Xbox.
    //
    // The header is (4 << 20) | method, sixteen floats of one matrix.
    // 0x001737A0 stores the constant 4x4 at 0x0017FA48 under 0x400840,
    // 0x400880, 0x4008C0, and 0x400900. 0x00174BC0 stores its two matrix
    // arguments under 0x400480 and 0x400680. The constant upload at
    // 0x001797A0 stores (device+0xdd0) * (device+0xc50) under 0x400480.
    // When DAT_00181D94 is zero, 0x400680 receives that product times the
    // matrix at device+0x970; otherwise it receives device+0x970. The
    // same upload then stores (device+0xe10), (device+0xe50), and
    // (device+0xe90), each times (device+0xc50), under 0x4004C0, 0x400500,
    // and 0x400540.
    dest[0] = header;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            dest[1 + col * 4 + row] = matrix[row * 4 + col];
        }
    }
}


// Xbox 0x00177AF0
void d3d_multiply_transform_matrix(float* out, const float* left, const float* right)
{
    // Xbox 0x00177AF0
    // stdcall (ret 0xC). SSE 4x4 product, out = left * right, row-major.
    // Output row r is left[r][0]*right.row0 + left[r][1]*right.row1
    // + left[r][2]*right.row2 + left[r][3]*right.row3.
    // D3D_UpdateProjectionViewportTransform (0x00171210) stores
    // (device+0xc90) * viewport at device+0x970. The constant upload at
    // 0x001797A0 calls the same product. Both matrices are read before
    // the first store, so out may alias left or right.
    float a[16];
    float b[16];
    std::memcpy(a, left, sizeof(a));
    std::memcpy(b, right, sizeof(b));
    for (int row = 0; row < 4; ++row)
    {
        const float x = a[row * 4 + 0];
        const float y = a[row * 4 + 1];
        const float z = a[row * 4 + 2];
        const float w = a[row * 4 + 3];
        out[row * 4 + 0] = x * b[0] + y * b[4] + z * b[8] + w * b[12];
        out[row * 4 + 1] = x * b[1] + y * b[5] + z * b[9] + w * b[13];
        out[row * 4 + 2] = x * b[2] + y * b[6] + z * b[10] + w * b[14];
        out[row * 4 + 3] = x * b[3] + y * b[7] + z * b[11] + w * b[15];
    }
}

} // namespace scene_detail

} // namespace gbtg
