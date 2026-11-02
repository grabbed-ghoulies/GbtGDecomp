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
#include "script_samples.h"

namespace gbtg
{
using namespace scene_detail;

namespace scene_detail
{



// Opcode 0xF of the script-action switch. 0x00020DC0 reaches this through
// 0x00021220 (ESI = the action object) and 0x00021450 (ESI = command+0x58).
// EAX is the gate from 0x00020DC0: the body runs only when that gate is 0.
// The command and the out-block are the stack arguments. Success writes 1 at out+4.

struct ScriptHost
{
    u16 mode = 0;           // +0xCDC
    void* link = nullptr;   // +0x93C
    u16 sample_id = 0;      // +0xCF4
    void* marker = nullptr; // +0xCE0, tested by 0x00028D20
};


struct ScriptCmd
{
    u8 opcode = 0;                   // +0
    u8 kind = 0;                     // +1, EBX for 0x000238E0
    u32 param_34 = 0;                // +0x34 -> target+0x26C
    const ScriptHost* ref = nullptr; // +0x38 -> target+0x270
    u32 param_3c = 0;                // +0x3C -> target+0x274
    u16 mark = 0;                    // +0x64, CX for 0x000238E0
};


struct ScriptOut
{
    i32 reserved = 0; // +0
    i32 handled = 0;  // +4
};


struct ScriptRow
{
    u16 key = 0;           // +0, sort key, stride 0x108
    u8 head[4] = {};     // +2, compared with 'aid_'
    u8 mid[4] = {};      // +0x82
    void* extra = nullptr; // +0x104
};


struct ScriptTable
{
    std::vector<ScriptRow> rows; // +0 count, +4 record base
};


struct ScriptTarget
{
    ScriptHost* host = nullptr; // +8
    u16 counter = 0;             // +0x94
    u8* block = nullptr;         // *(*(target+0x15C)+8); byte +2 bit 1 blocks
    ScriptTable* table = nullptr; // +0x73C
    void* suppress = nullptr;    // +0x750, kind 0x3F forces the sample to 0
    u32 at_26c = 0;
    const ScriptHost* at_270 = nullptr;
    u32 at_274 = 0;
    u32 at_278 = 0;
    u32 at_27c = 0;
    i32 at_280 = 0;
    u16 at_162 = 0;
    void* slot_314 = nullptr;
    void* slot_744 = nullptr;
    const ScriptRow* bound = nullptr; // +0x740
    u16 bound_key = 0;                // +0x748
    void* slot_74c = nullptr;
    u8 flag_6c = 0;
    void* slot_84 = nullptr;
    void (*release)(void* block) = nullptr;                         // 0x001075C0
    void (*on_note)(ScriptTarget& self, u32 code, u16 id) = nullptr; // host vtable +0xC
    void (*on_extra)(ScriptTarget& self, void* extra) = nullptr;    // 0x0001AF10
};


struct ScriptAction
{
    bool live = false;
    i32 gate = 0;
    ScriptTarget target;
    ScriptCmd cmd;
    ScriptOut out;
};


ScriptAction g_script_action;

i32 g_script_sample_gate = 0; // DAT_00512200
const u8* g_script_sample_bank = nullptr; // DAT_00545AC4

u8 kind_pair(u8 kind, int odd)
{
    // Byte pair at 0x00217DE8 + kind*2. Odd selects the second byte.
    struct Pair
    {
        u8 kind;
        u8 lo;
        u8 hi;
    };
    static constexpr Pair k[] = {
        {0x1D, 64, 64}, {0x1F, 64, 64}, {0x21, 64, 64}, {0x25, 128, 63},
        {0x2B, 128, 63}, {0x33, 11, 42}, {0x34, 12, 43}, {0x35, 14, 45},
        {0x36, 21, 52}, {0x37, 23, 54}, {0x38, 22, 53}, {0x39, 25, 56},
        {0x3A, 26, 57}, {0x3B, 27, 58}, {0x3C, 28, 59}, {0x3D, 29, 60},
        {0x3E, 31, 62}, {0x3F, 31, 62}, {0x40, 32, 63}, {0x41, 33, 64},
        {0x42, 34, 65}, {0x43, 35, 66}, {0x44, 36, 67}, {0x45, 40, 71},
        {0x46, 11, 42}, {0x49, 10, 41}, {0x4A, 13, 44}, {0x4B, 10, 41},
        {0x4D, 30, 61}, {0x54, 18, 49}, {0x56, 37, 68}, {0x5C, 19, 50},
        {0x5E, 38, 69}, {0x64, 20, 51}, {0x66, 39, 70}, {0x68, 104, 0},
        {0x69, 105, 0}, {0x6A, 106, 0}, {0x6D, 128, 63}, {0x6F, 128, 63},
        {0x71, 128, 63}, {0x73, 128, 63}, {0x75, 128, 63}, {0x77, 128, 63},
        {0x79, 128, 63}, {0x7B, 128, 63}, {0x7D, 128, 63}, {0x7F, 128, 63},
        {0x81, 128, 63}, {0x83, 128, 63}, {0x84, 154, 153}, {0x85, 25, 62},
        {0x87, 128, 63}, {0x89, 128, 63}, {0x8B, 128, 63}, {0x8D, 128, 63},
        {0x8F, 128, 63}, {0x90, 27, 0}, {0x92, 42, 0}, {0x94, 40, 0},
        {0x98, 24, 0}, {0x9A, 4, 0}, {0x9C, 164, 2}, {0x9E, 144, 2},
        {0xA0, 28, 0}, {0xA2, 4, 0}, {0xA4, 24, 0}, {0xA6, 4, 0},
        {0xA8, 24, 0}, {0xAA, 4, 0}, {0xAC, 68, 0}, {0xAE, 4, 0},
        {0xB0, 84, 0}, {0xB2, 4, 0}, {0xB4, 24, 0}, {0xB6, 4, 0},
        {0xB8, 72, 0}, {0xBA, 4, 0}, {0xBC, 24, 0}, {0xBE, 4, 0},
        {0xC0, 24, 0}, {0xC2, 4, 0}, {0xC4, 28, 0}, {0xC6, 4, 0},
        {0xC8, 24, 0}, {0xCA, 4, 0}, {0xCC, 52, 0}, {0xCE, 16, 0},
        {0xD0, 64, 0}, {0xD2, 16, 0}, {0xD4, 48, 0}, {0xD6, 4, 0},
        {0xD8, 60, 0}, {0xDA, 4, 0}, {0xDC, 60, 0}, {0xDE, 4, 0},
        {0xE0, 24, 0}, {0xE2, 4, 0}, {0xE4, 36, 0}, {0xE6, 4, 0},
        {0xE8, 28, 0}, {0xEA, 4, 0}, {0xEC, 36, 0}, {0xEE, 4, 0},
        {0xF0, 24, 0}, {0xF2, 4, 0}, {0xF4, 24, 0}, {0xF6, 4, 0},
        {0xF8, 24, 0}, {0xFA, 4, 0}, {0xFC, 24, 0}, {0xFE, 4, 0},
    };
    for (const Pair& p : k)
    {
        if (p.kind == kind)
        {
            return odd != 0 ? p.hi : p.lo;
        }
    }
    return 0;
}


int trunc_sample(u8 index)
{
    // cvttss2si of the float at 0x00433CA4 + index*12. Missing entries are 0.
    struct Sample
    {
        u8 index;
        i32 value;
    };
    static constexpr Sample k[] = {
        {0x01, 200}, {0x02, 90}, {0x03, 90}, {0x04, 90}, {0x05, 90}, {0x06, 90},
        {0x07, 90}, {0x08, 90}, {0x09, 100}, {0x0A, 150}, {0x0B, 100}, {0x0C, 100},
        {0x0D, 150}, {0x0E, 100}, {0x0F, 2}, {0x10, 100}, {0x11, 10}, {0x12, 100},
        {0x13, 50}, {0x14, 2}, {0x15, 100}, {0x16, 100}, {0x17, 100}, {0x18, 5},
        {0x19, 100}, {0x1A, 100}, {0x1B, 100}, {0x1C, 100}, {0x1D, 100}, {0x1E, 150},
        {0x1F, 100}, {0x20, 100}, {0x21, 100}, {0x22, 100}, {0x23, 100}, {0x24, 100},
        {0x25, 100}, {0x26, 50}, {0x27, 2}, {0x28, 100}, {0x29, 150}, {0x2A, 100},
        {0x2B, 100}, {0x2C, 150}, {0x2D, 100}, {0x2E, 2}, {0x2F, 100}, {0x30, 10},
        {0x31, 100}, {0x32, 50}, {0x33, 2}, {0x34, 100}, {0x35, 100}, {0x36, 100},
        {0x37, 5}, {0x38, 100}, {0x39, 100}, {0x3A, 100}, {0x3B, 100}, {0x3C, 100},
        {0x3D, 150}, {0x3E, 100}, {0x3F, 100}, {0x40, 100}, {0x41, 100}, {0x42, 100},
        {0x43, 100}, {0x44, 100}, {0x45, 50}, {0x46, 2}, {0x47, 100}, {0x48, -1},
        {0x4A, -2}, {0x4B, 2}, {0x5B, (-2147483647 - 1)}, {0x5D, -5}, {0xF4, (-2147483647 - 1)}, {0xFD, (-2147483647 - 1)},
        {0xFE, 146},
    };
    for (const Sample& s : k)
    {
        if (s.index == index)
        {
            return s.value;
        }
    }
    return 0;
}


bool host_special(const ScriptHost& host)
{
    return host.mode == 2 || host.link != nullptr;
}


int sample_bank(u8 index, u16 id)
{
    // 0x001339C0 via 0x00133BD0 / 0x00028DA0. Byte at bank[id*0x87 + index - 0x48].
    // DAT_00545AC4 stays null until that bank is filled in.
    if (g_script_sample_bank == nullptr)
    {
        return 0;
    }
    (void)index;
    (void)id;
    return 0;
}


int sample_plain(const ScriptHost& host, u8 index)
{
    // 0x00028DA0. A zero id uses the float table; otherwise the bank.
    if (host.sample_id == 0)
    {
        return trunc_sample(index);
    }
    return sample_bank(index, host.sample_id);
}


bool host_marked(const ScriptHost& host)
{
    // 0x00028D20. Needs +0xCE0, then mode 2 or a live link.
    if (host.marker == nullptr)
    {
        return false;
    }
    return host_special(host);
}


const ScriptRow* find_row(const ScriptTable& table, u16 key)
{
    // 0x00027510. Signed binary search, unsigned key compare, stride 0x108.
    if (table.rows.empty())
    {
        return nullptr;
    }
    int lo = 0;
    int hi = static_cast<int>(table.rows.size()) - 1;
    while (lo <= hi)
    {
        const int mid = (lo + hi) >> 1;
        const u16 row_key = table.rows[static_cast<size_t>(mid)].key;
        if (row_key < key)
        {
            lo = mid + 1;
        }
        else if (row_key > key)
        {
            hi = mid - 1;
        }
        else
        {
            return &table.rows[static_cast<size_t>(mid)];
        }
    }
    return nullptr;
}


void release_slot(ScriptTarget& target, void*& slot)
{
    if (slot == nullptr)
    {
        return;
    }
    if (target.release != nullptr)
    {
        target.release(slot);
    }
    slot = nullptr;
}


void bind_row(ScriptTarget& target, const ScriptRow& row)
{
    // 0x000276A0. Drop the previous binds, keep this row, then 0x00027830 / 0x00027890
    // if the row text is an aid_ name. Those two allocate; the slots stay clear here.
    release_slot(target, target.slot_314);
    release_slot(target, target.slot_744);
    target.bound = nullptr;
    target.bound_key = 0;
    if (row.extra != nullptr && target.on_extra != nullptr)
    {
        target.on_extra(target, row.extra);
    }
    target.bound = &row;
    target.bound_key = row.key;
    if (target.slot_74c == nullptr)
    {
        target.flag_6c = 0;
        target.slot_84 = nullptr;
    }
    // 0x00027830 (row+0x82) and 0x00027890 (row+2) run when those bytes are the
    // dword 'aid_'. They allocate through 0x00107720 and store +0x314 / +0x744.
}


void apply_note(ScriptTarget& target, u16 id)
{
    // 0x00024B20. Notify host vtable+0xC with {0x61, id}, then bind that row.
    if (target.on_note != nullptr)
    {
        target.on_note(target, 0x61, id);
    }
    if (id == 0 || target.table == nullptr)
    {
        return;
    }
    if (const ScriptRow* row = find_row(*target.table, id))
    {
        bind_row(target, *row);
    }
}


int resolve_sample(const ScriptTarget& target, const ScriptCmd& cmd)
{
    // 0x000238E0. Kind 0x32 is a hard zero. Otherwise the host at target+8 picks
    // the bank path when it is special and the command mark is set.
    if (cmd.kind == 0x32 || target.host == nullptr)
    {
        return 0;
    }
    const ScriptHost& host = *target.host;
    const u8 pair_lo = kind_pair(cmd.kind, 0);
    const u8 pair_hi = kind_pair(cmd.kind, 1);
    if (host_special(host) && cmd.mark != 0)
    {
        return sample_bank(pair_hi, cmd.mark);
    }
    int got = sample_plain(host, pair_lo);
    int flagged = 0;
    if (cmd.ref != nullptr && host_marked(*cmd.ref) && g_script_sample_gate != 0)
    {
        flagged = 1;
    }
    if (got != 0 && flagged != 0)
    {
        return target.counter;
    }
    if (cmd.kind == 0x3F)
    {
        if (got != 0)
        {
            const int half = got / 2;
            got = half > 1 ? half : 1;
        }
        if (target.suppress != nullptr)
        {
            got = 0;
        }
        return got;
    }
    if (cmd.kind == 0x46 && !host_special(host))
    {
        return 0;
    }
    return got;
}


// Xbox 0x00021C50
void script_apply_sample_command(ScriptTarget& target, const ScriptCmd& cmd, ScriptOut& out, i32 gate)
{
    // Xbox 0x00021C50
    if (gate != 0)
    {
        return;
    }
    const int resolved = resolve_sample(target, cmd);
    if (resolved == 0)
    {
        return;
    }
    if (target.block == nullptr || (target.block[2] & 2) != 0)
    {
        return;
    }
    if (target.table == nullptr || find_row(*target.table, 0x81) == nullptr)
    {
        return;
    }
    target.at_274 = cmd.param_3c;
    target.at_26c = cmd.param_34;
    target.at_280 = resolved;
    target.at_278 = 0;
    target.at_27c = cmd.opcode;
    target.at_270 = cmd.ref;
    target.at_162 = 0;
    apply_note(target, 0x81);
    out.handled = 1;
}


void script_action_opcode_f()
{
    // 0x00020DC0 dispatches opcode 0xF into script_apply_sample_command once per action.
    // The slot stays clear until a command is filled in.
    if (!g_script_action.live)
    {
        return;
    }
    g_script_action.live = false;
    if (g_script_action.cmd.opcode != 0x0F)
    {
        return;
    }
    script_apply_sample_command(g_script_action.target, g_script_action.cmd, g_script_action.out, g_script_action.gate);
}

} // namespace scene_detail

} // namespace gbtg
