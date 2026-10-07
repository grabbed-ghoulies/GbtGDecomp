#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "transition_data.h"

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
#include "../camera/camera.h"
#include "../scene/scene.h"
#include "scene_transitions.h"
#include "../scripts/script_actions.h"
#include "transition_book.h"

namespace gbtg
{
using namespace scene_detail;

static_assert(sizeof(TransitionSlot31478) == 0x110);

static_assert(offsetof(TransitionSlot31478, aid) == 4);

static_assert(offsetof(TransitionSlot31478, at_104) == 0x104);

static_assert(offsetof(TransitionSlot31478, at_10c) == 0x10c);

static_assert(sizeof(kTransition31478) / sizeof(kTransition31478[0]) == 18);


// DAT_00546328. Scene-order slot. Its address is the handler argument.
i32 g_546328 = 0;

// DAT_0054632C. 1 runs the countdown and the handler. 3 runs the handler.
// A nonzero value is the clear gate.
i32 g_54632c = 0;

// DAT_00546344. Steps while DAT_0050484C is above 0, and while this is not.
i32 g_546344 = 0;

// DAT_00546348. Nonzero counts down. Landing on 0 ORs 0x2000 into DAT_0050EA9C.
i32 g_546348 = 0;

// DAT_0054634C. The object at [this] holds the unsigned counts at +4 and +8.
u8** g_54634c = nullptr;

// DAT_0050484C. Incremented when it is below 1 and DAT_00546344 is above 0.
i32 g_50484c = 0;

// DAT_005096E8 and DAT_005096EA. HUD width and height, zero-extended to float.
u16 g_5096e8 = 0;

u16 g_5096ea = 0;

// DAT_0050E7A0. Counted when the scene state leaves or enters 1 or 2.
// entity.cpp keeps a private copy as g_amb_counter.
i32 g_50e7a0 = 0;

// DAT_00510CEC. Counted up when DAT_0054632C is 0 on entry to 0x0012C5D0.
i32 g_510cec = 0;

// DAT_004DEA24. Counted with that gate, and once more on the way out.
// gfx keeps the same address as present_insert.hold.
i32 g_4dea24 = 0;

// DAT_00510F80. Scene record +0x128. Cleared when the phase is 1.
i32& g_510f80 = *reinterpret_cast<i32*>(g_510e58 + 0x128);

// DAT_00510F7C. Scene record +0x124. Cleared when the phase is 3.
i32& g_510f7c = *reinterpret_cast<i32*>(g_510e58 + 0x124);

// DAT_00546330 and DAT_00546334. Both receive the dword at slot+0x104.
u32 g_546330 = 0;

u32 g_546334 = 0;

// DAT_00546340. Address of slot+4 when that dword is 'aid_', otherwise null.
const char* g_546340 = nullptr;


// DAT_0054937C. Xbox +0 is prev and +4 is next. Xbox +0x34 is the name at
// DAT_005493B0. DAT_004E1680 is the slot-0 sentinel. entity.cpp keeps the
// live list private, so the splice walks the copy held here.
struct ViewNode54937c
{
    ViewNode54937c* prev = nullptr;
    ViewNode54937c* next = nullptr;
    char name[0x21]{};
};

ViewNode54937c g_4e1680{};

ViewNode54937c g_54937c{};


// DAT_00507DF4. 0x0011CE40 stores 0.
i32 g_507df4 = 0;

// DAT_00510CE8. Nonzero clears bit 0x20 of DAT_0050EA9C, then stores 0.
i32 g_510ce8 = 0;

// DAT_00510E40. Nonzero is EBX for 0x00035C00, then stores 0.
void* g_510e40 = nullptr;

// DAT_00510E34. Nonzero decrements DAT_004DEA24, then stores 0.
i32 g_510e34 = 0;

// Xbox 0x0012C340
void transition_tick_active()
{
    // Xbox 0x0012C340
    if (0 < g_54632c && g_54632c < 4)
    {
        const i32 byte_off = g_546328 * 0x110;
        const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
        if (*reinterpret_cast<const i32*>(row + 0x10c) != 0 && g_54634c != nullptr)
        {
            const u8* obj = *g_54634c;
            const u32 at_4 = *reinterpret_cast<const u32*>(obj + 4);
            const u32 at_8 = *reinterpret_cast<const u32*>(obj + 8);
            if (at_8 <= at_4)
            {
                const float x = static_cast<float>(g_5096e8) * std::bit_cast<float>(0x3f000000u);
                const float y = static_cast<float>(g_5096ea) * std::bit_cast<float>(0x3f4ccccdu);
                // 0x0005F580 and 0x0005F550 are not translated.
                (void)x;
                (void)y;
            }
        }
    }
    if (g_54632c == 1)
    {
        if (g_546348 != 0)
        {
            g_546348 = g_546348 - 1;
            if (g_546348 == 0)
            {
                g_flags_ea9c = g_flags_ea9c | 0x2000u;
            }
        }
        if (g_50484c < 1 && 0 < g_546344)
        {
            g_50484c = g_50484c + 1;
        }
        else
        {
            g_546344 = g_546344 + 1;
        }
        const i32 byte_off = g_546328 * 0x110;
        const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
        const i32 handler = *reinterpret_cast<const i32*>(row);
        // Handler at DAT_003129AC + handler * 0x18 is not translated.
        // The argument is &DAT_00546328.
        if (handler == 1)
        {
            transition_initialize_chapter_title(*g_transition_game);
        }
        else if (handler == 2)
        {
            transition_tick_exit_cue(*g_transition_game);
        }
        else if (handler == 0 || handler == 3)
        {
            float time = std::bit_cast<float>(g_546330);
            if (0.f < time)
            {
                time -= g_sim_dt;
                g_546330 = std::bit_cast<u32>(time);
            }
            if (time <= 0.f && (handler != 0 || time != 0.f))
            {
                transition_select_clip(2);
            }
        }
    }
    else if (g_54632c == 3)
    {
        const i32 byte_off = g_546328 * 0x110;
        const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
        const i32 handler = *reinterpret_cast<const i32*>(row);
        // Handler at DAT_003129AC + handler * 0x18 is not translated.
        // The argument is &DAT_00546328.
        if (handler == 1)
        {
            transition_initialize_chapter_title(*g_transition_game);
        }
        else if (handler == 2)
        {
            transition_tick_exit_cue(*g_transition_game);
        }
        else if (handler == 0 || handler == 3)
        {
            float time = std::bit_cast<float>(g_546330);
            if (0.f < time)
            {
                time -= g_sim_dt;
                g_546330 = std::bit_cast<u32>(time);
            }
            if (time <= 0.f && time != 0.f)
            {
                transition_select_clip(0);
            }
        }
        return;
    }
}


// Xbox 0x0012C4E0
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +8 (DAT_003129B4) with &DAT_00546328. The image
// slots are 0x001628B0, 0x0012B5A0, 0x0012C1A0, and 0x001628B0. Those handlers
// are not translated. EAX is then DAT_0054937C for 0x00048C70.
// Phase 0 increments DAT_00510CEC and DAT_004DEA24. Phase 1 clears
// DAT_00510F80. Phase 3 clears DAT_00510F7C. Any other phase falls through.
// DAT_00510CEC and DAT_004DEA24 decrement. Bit 0x100 of DAT_0050EA9C clears.
// DAT_0054632C becomes 0. A nonzero DAT_0054634C would call 0x0005F460. That
// call is not translated. Bit 0x2000 then clears and the pointer becomes 0.
// DAT_004DEA24 decrements again.
// Xbox 0x0012C4E0
void transition_dispatch_handler()
{
    // Xbox 0x0012C4E0
    const i32 byte_off = g_546328 * 0x110;
    const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
    const i32 handler = *reinterpret_cast<const i32*>(row);
    // Handler at DAT_003129B4 + handler * 0x18 is not translated.
    // The argument is &DAT_00546328.
    if (handler == 1)
    {
        transition_initialize_book();
    }
    else if (handler == 2)
    {
        transition_release_texture();
    }
    if (g_54937c.prev != nullptr)
    {
        g_54937c.prev->next = g_54937c.next;
    }
    if (g_54937c.next != nullptr)
    {
        g_54937c.next->prev = g_54937c.prev;
    }
    g_54937c.prev = nullptr;
    g_54937c.next = nullptr;
    if (g_54632c == 0)
    {
        g_510cec = g_510cec + 1;
        g_4dea24 = g_4dea24 + 1;
    }
    else if (g_54632c == 1)
    {
        g_510f80 = 0;
    }
    else if (g_54632c == 3)
    {
        g_510f7c = 0;
    }
    g_510cec = g_510cec - 1;
    g_4dea24 = g_4dea24 - 1;
    g_flags_ea9c = g_flags_ea9c & 0xfffffeffu;
    g_54632c = 0;
    if (g_54634c != nullptr)
    {
        // 0x0005F460 is not translated. EBX is DAT_0054634C.
        g_flags_ea9c = g_flags_ea9c & 0xffffdfffu;
        g_54634c = nullptr;
    }
    g_4dea24 = g_4dea24 - 1;
}


// Xbox 0x0012C5D0
// A nonzero DAT_0054632C calls 0x0012C4E0 before the phase test. Phase 0
// increments DAT_00510CEC and DAT_004DEA24 and ORs 0x100 into DAT_0050EA9C.
// Phase 1 clears DAT_00510F80. Phase 3 clears DAT_00510F7C. Any other phase
// falls through. DAT_0054632C becomes 1.
//
// 0x0012C2D0 is not translated. ESI would be DAT_0054633C and EDI DAT_00546338.
// EAX would replace DAT_00546328. That return is not stored. The slot already
// in DAT_00546328 is scaled by 0x110. The dword at +0x104 is copied to
// DAT_00546334 and DAT_00546330. DAT_00546344 becomes 0. DAT_00546348 becomes
// 0xf. DAT_00546340 is the address of slot+4 when that dword is 'aid_'
// (0x5f646961), and null when it is not.
//
// 0x000496C0 is not translated. EDX would be this node, EAX the string
// "transition", and CL 0. That call would copy the name to +0x34. The node
// is inserted after the slot-0 record whose prev is null. A null DAT_004E16A0
// would then call camera_bind_named_view with slot 0 and that name. The name is
// written by 0x000496C0, and entity.cpp keeps the live slot, so that call is
// not made. 0x00048F10 is not translated. 0x0005F390 is not translated: the
// stack is 0, 1, 0x80 and ESI is DAT_0054634C. 0x000FF0A0 is not translated:
// the stack is aid_loctext_ghoulies_gameselect_gameselecttext, "gameselect",
// "loading", and DAT_00546350.
//
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +0xC (DAT_003129B8) with &DAT_00546328. Those
// handlers are not translated. DAT_004DEA24 increments on the way out.
// Xbox 0x0012C5D0
void transition_tick_queue()
{
    // Xbox 0x0012C5D0
    if (g_54632c != 0)
    {
        transition_dispatch_handler();
    }
    if (g_54632c == 0)
    {
        g_510cec = g_510cec + 1;
        g_4dea24 = g_4dea24 + 1;
        g_flags_ea9c = g_flags_ea9c | 0x100u;
    }
    else if (g_54632c == 1)
    {
        g_510f80 = 0;
    }
    else if (g_54632c == 3)
    {
        g_510f7c = 0;
    }
    g_54632c = 1;
    g_546328 = transition_set_clip_route(reinterpret_cast<const char*>(g_510e58 + 0x80), g_546338, g_54633c);
    const i32 byte_off = g_546328 * 0x110;
    const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
    const u32 bits = *reinterpret_cast<const u32*>(row + 0x104);
    g_546334 = bits;
    g_546330 = bits;
    g_546344 = 0;
    g_546348 = 0xf;
    const u32 marker = *reinterpret_cast<const u32*>(row + 4);
    const char* aid = reinterpret_cast<const char*>(row + 4);
    if (marker != kAidPrefixEd8)
    {
        aid = nullptr;
    }
    g_546340 = aid;
    // 0x000496C0 is not translated. EDX is this node, EAX is "transition", CL is 0.
    ViewNode54937c* cursor = &g_4e1680;
    if (cursor->prev != nullptr)
    {
        do
        {
            cursor = cursor->prev;
        } while (cursor->prev != nullptr);
    }
    ViewNode54937c* saved = cursor->next;
    cursor->next = &g_54937c;
    g_54937c.prev = cursor;
    g_54937c.next = saved;
    if (saved != nullptr)
    {
        saved->prev = &g_54937c;
    }
    // A null DAT_004E16A0 would call camera_bind_named_view(0, name). The name is
    // written by 0x000496C0, which is not translated, so that call is not made.
    // 0x00048F10 is not translated.
    // 0x0005F390 is not translated. ESI is DAT_0054634C. The stack is 0, 1, 0x80.
    // 0x000FF0A0 is not translated.
    const i32 handler = *reinterpret_cast<const i32*>(row);
    // Handler at DAT_003129B8 + handler * 0x18 is not translated.
    // The argument is &DAT_00546328.
    if (handler == 1)
    {
        transition_initialize_map_book(*g_transition_game);
    }
    else if (handler == 2)
    {
        transition_load_enter_texture(*g_transition_game);
    }
    else if (handler == 0)
    {
        g_546330 = g_546334 = 0x3c888889u; // Xbox 0x0012BCE0, DAT_0032B150
    }
    g_4dea24 = g_4dea24 + 1;
}


// Xbox 0x0012C770
// Phase 0 increments DAT_00510CEC and DAT_004DEA24 and ORs 0x100 into
// DAT_0050EA9C. Phase 1 clears DAT_00510F80. Phase 3 clears DAT_00510F7C.
// Any other phase falls through. The slot in DAT_00546328 is scaled by 0x110.
// The dword at +0x104 is copied to DAT_00546330, then DAT_0054632C becomes 3.
// DAT_0050484C decrements when it is above 0. DAT_00546340 is the address of
// slot+0x84 when that dword is 'aid_' (0x5f646961), and null when it is not.
// DAT_00546334 then receives DAT_00546330.
//
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +0x10 (DAT_003129BC) with &DAT_00546328. The
// image slots are 0x0012BD50, 0x0012B790, 0x0012C270, and 0x001628B0. Those
// handlers are not translated.
// Xbox 0x0012C770
void transition_finish_pending()
{
    // Xbox 0x0012C770
    if (g_54632c == 0)
    {
        g_510cec = g_510cec + 1;
        g_4dea24 = g_4dea24 + 1;
        g_flags_ea9c = g_flags_ea9c | 0x100u;
    }
    else if (g_54632c == 1)
    {
        g_510f80 = 0;
    }
    else if (g_54632c == 3)
    {
        g_510f7c = 0;
    }
    const i32 byte_off = g_546328 * 0x110;
    const u8* row = reinterpret_cast<const u8*>(kTransition31478) + byte_off;
    const u32 bits = *reinterpret_cast<const u32*>(row + 0x104);
    g_546330 = bits;
    g_54632c = 3;
    if (0 < g_50484c)
    {
        g_50484c = g_50484c - 1;
    }
    const u32 marker = *reinterpret_cast<const u32*>(row + 0x84);
    const char* aid = reinterpret_cast<const char*>(row + 0x84);
    if (marker != kAidPrefixEd8)
    {
        aid = nullptr;
    }
    g_546340 = aid;
    g_546334 = g_546330;
    const i32 handler = *reinterpret_cast<const i32*>(row);
    // Handler at DAT_003129BC + handler * 0x18 is not translated.
    // The argument is &DAT_00546328.
    if (handler == 1)
    {
        transition_release_book();
    }
    else if (handler == 2)
    {
        transition_load_exit_texture(*g_transition_game);
    }
    else if (handler == 0)
    {
        g_546330 = g_546334 = 0x3f000000u; // Xbox 0x0012BD50, DAT_0032B0F4
    }
}

} // namespace gbtg
