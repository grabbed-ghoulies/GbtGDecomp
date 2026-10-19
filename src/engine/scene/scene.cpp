#include "../cutscenes/door_registration.h"
#include "../scripts/script_event_sequence.h"
#include "gbtg/anim.h"
#include "cutscene_scene_runtime.h"
#include "../video/native_movies.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "factory_scene_runtime.h"
#include "scene_actor_services.h"

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


#include "scene_types.h"
#include "../camera/camera.h"
#include "scene.h"
#include "scene_loading.h"
#include "scene_tick_nodes.h"
#include "scene_controls.h"
#include "../transitions/scene_transitions.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_vm.h"
#include "../scripts/script_conditions.h"
#include "../navigation/occupancy.h"
#include "../entities/camera_director.h"
#include "gbtg/actor_spawn_factory.h"
#include "dialogue_scene_runtime.h"

namespace gbtg
{
using namespace scene_detail;



// DAT_00510CF4. Phase this tick switches on. 3 and 8 are the settled steps.
i32 g_510cf4 = 0;

// DAT_00510CFC. Nonzero is the object 0x00109470 takes in EAX.
void* g_510cfc = nullptr;

// DAT_00510E30. A zero dword at +0x94C clears the pointer after 0x00142070.
u8* g_510e30 = nullptr;

// DAT_00510CF8. Points at the object whose +4 and +8 are the unsigned counts.
u8** g_510cf8 = nullptr;

// DAT_00510E24. +0 indexes the stride-0x124 table at DAT_0042217C. +8 and
// +0x10 are flags. +0x24 is the clock (+0x2C over +0x34). +0x94 is the slot.
u8* g_510e24 = nullptr;

// DAT_00510E28. Phase 6 selects one of cases 1 through 7.
i32 g_510e28 = 0;

// DAT_004EE1EC. entity.cpp keeps a private copy. Nonzero skips phases 4 and 5.
i32 g_4ee1ec = 0;

// DAT_00506C90. entity.cpp keeps a private copy. Zero takes 0x0011EAD0.
u16 g_506c90 = 0;

// DAT_00429BC0, then the wide buffer at DAT_00429BC4 (0x100 bytes, next
// symbol DAT_00429CC4).
i32 g_429bc0 = 0;

u16 g_429bc4[0x80]{};

// DAT_004E19B4. Wide source of that copy. 0x80 bytes, next symbol DAT_004E1A34.
u16 g_4e19b4[0x40]{};

// DAT_00510E58. +0x80 is DAT_00510ED8. +0x100 is DAT_00510F58.
// +0x108 is DAT_00510F60. +0x124 is DAT_00510F7C. +0x128 is DAT_00510F80.
// The Xbox dword at +0x230 is the object 0x00109470 takes in EAX.
// DAT_0051108C (g_51108c) is the next symbol, kept separate. The host
// pointer at +0x230 is wider than that Xbox dword.
alignas(void*) u8 g_510e58[0x230 + sizeof(void*)]{};

// DAT_00510ED8. 0x80-byte aid. entity.cpp keeps a private copy of 0x0011EB30.
char (&g_510ed8)[0x80] = *reinterpret_cast<char(*)[0x80]>(g_510e58 + 0x80);

// DAT_00510F58. Mode dword stored by that copy. Scene record +0x100.
i32& g_510f58 = *reinterpret_cast<i32*>(g_510e58 + 0x100);


struct BindScene230
{
    BindScene230()
    {
        *reinterpret_cast<i32**>(g_510e58 + 0x230) = &g_refresh_kind;
    }
};

BindScene230 g_bind_scene_230{};

// DAT_00430D0C. Cleared with DAT_00430D08 and DAT_00430D10.
i32 g_script_30d0c = 0;

// DAT_00430D94. Nonzero skips the clear of DAT_00506C8C, then becomes 0.
i32 g_script_30d94 = 0;

// DAT_00430D98. Cleared with DAT_00430D94.
i32 g_script_30d98 = 0;

// DAT_00506C8C. Cleared when DAT_00430D94 was 0 on entry to the copy.
i32 g_506c8c = 0;


// Image at DAT_0042217C. Stride 0x124. +0 and +4 are the progress gates.
// +8 is the slot when the ratio is at or under +0. +0xC is the slot when
// the ratio is over +4. Indices 0 through 14 are the rows in the image.
struct ProgressRow42217c
{
    u32 low = 0;
    u32 high = 0;
    i32 low_slot = 0;
    i32 high_slot = 0;
    u8 pad[0x114]{};
};

static_assert(sizeof(ProgressRow42217c) == 0x124);

static_assert(offsetof(ProgressRow42217c, high) == 4);

static_assert(offsetof(ProgressRow42217c, low_slot) == 8);

static_assert(offsetof(ProgressRow42217c, high_slot) == 0xc);


constexpr ProgressRow42217c kProgress42217c[] = {
    {0x3f1eb852u, 0x3f547ae1u, 1, 2},
    {0x3ebd70a4u, 0x3ed1eb85u, 2, 1},
    {0x3ebbc01au, 0x3f547ae1u, 0, 2},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 0, 0},
    {0x3efae148u, 0x00000000u, 2, 2},
    {0x3ecccccdu, 0x00000000u, 2, 2},
};


// DAT_00322B8C. 10 dwords and one word: the aid plus its NUL.
constexpr char kGameselectAid[] = "aid_script_ghoulies_gameselect_gameselect";

static_assert(sizeof(kGameselectAid) == 42);


// Clock at player+0x24. The ratio is the float at +0x2C over the float at +0x34.
static float progress_ratio(const u8* player)
{
    const u8* clock = *reinterpret_cast<u8* const*>(player + 0x24);
    const float num = *reinterpret_cast<const float*>(clock + 0x2c);
    const float den = *reinterpret_cast<const float*>(clock + 0x34);
    return num / den;
}


// Shared tail at 0x0011D020. DAT_00510CF4 becomes 3. 0x00048940 is not
// translated: the stack dword is 0x3DFBE76D and ECX is DAT_00510E24.
// A zero dword at +8 would call 0x0004E640 with EAX = 1 and EDI = the
// clock at +0x24. That call is not translated. +8 is then stored as 1.
static void enter_phase_three()
{
    g_510cf4 = 3;
    i32* flag = reinterpret_cast<i32*>(g_510e24 + 8);
    if (*flag == 0)
    {
        *flag = 1;
    }
}


// 0x0014A961. The SEH wrapper around 0x001660E9. Each 16-bit unit is stored,
// then both pointers advance, until the unit just stored is zero.
// Xbox 0x0014A961
static u16* xapi_copy_wide_string(u16* dest, const u16* src)
{
    u16* out = dest;
    u16 unit = 0;
    do
    {
        unit = *src;
        *out = unit;
        ++out;
        ++src;
    } while (unit != 0);
    return dest;
}


// 0x0011EB30. EAX is the source aid. The stack dword is the mode. A first
// dword of 'aid_' clears the lead byte, then the source is copied through
// its NUL. DAT_00430D08, DAT_00430D0C, DAT_00430D10, and the byte at
// DAT_00430D14 become 0. A zero DAT_00430D94 also clears DAT_00506C8C.
// DAT_00430D94 and DAT_00430D98 become 0. The mode lands at DAT_00510F58.
// entity.cpp keeps a private copy of these addresses.
void request_scene_aid(const char* src, i32 mode)
{
    u32 head = 0;
    std::memcpy(&head, g_510ed8, sizeof(head));
    if (head == kAidPrefixEd8)
    {
        g_510ed8[0] = 0;
    }
    char* dest = g_510ed8;
    for (;;)
    {
        const char unit = *src;
        *dest = unit;
        ++src;
        ++dest;
        if (unit == 0)
        {
            break;
        }
    }
    g_flag_30d08 = 0;
    g_script_30d0c = 0;
    g_script_30d10 = 0;
    g_byte_30d14 = 0;
    if (g_script_30d94 == 0)
    {
        g_506c8c = 0;
    }
    g_script_30d94 = 0;
    g_script_30d98 = 0;
    g_510f58 = mode;
}


// Xbox 0x0011CFB0
// Phase 2 through 9 calls 0x00057E60. A nonzero DAT_00510CFC calls
// 0x00109470 with that pointer in EAX. The first switch then steps the
// phase. Phases 2 and 9 compare the clock ratio with 0.123 (DAT_0032B6CC,
// 0x3DFBE76D) and 0.815 (DAT_0032B634, 0x3F50A3D7). At or under the gate
// stays. Over it, and phases 4 and 5 when DAT_004EE1EC is 0, enter phase 3.
// Phase 6 copies aid_script_ghoulies_gameselect_gameselect into an 0x80
// stack buffer and switches on DAT_00510E28. Phase 7 enters phase 8 and
// ORs 0x2200 into DAT_0050EA9C. The second switch calls 0x00142460 for
// phases 1, 2, 6, 7, and 9 when DAT_00510E30 is set (EAX is 0), for phase
// 8 when that pointer is set (EAX is DAT_00510CFC), and for phase 3
// (EAX is DAT_00510CFC, or 0 when that pointer is clear). 0x00142460,
// 0x0011DEA0, 0x001083F0, and 0x0011DEE0 are not translated, so their
// returns are not taken. Phases 2 through 7, and phase 9, then mark a
// slot from the DAT_0042217C row and would call 0x000483E0.
// Xbox 0x0011CFB0
void scene_tick_load_phase()
{
    i32 phase = g_510cf4;
    if (1 < phase && phase < 10)
    {
        tick_scene_dialogue(*g_transition_game); // Xbox 0x00057E60
    }
    if (g_510cfc != nullptr)
    {
        script_refresh_object_record(g_510cfc);
    }

    phase = g_510cf4;
    switch (phase)
    {
    case 1:
        if (g_510e30 != nullptr && *reinterpret_cast<i32*>(g_510e30 + 0x94c) == 0)
        {
            // 0x00142070 is not translated. EAX is DAT_00510E30.
            g_510e30 = nullptr;
        }
        if (g_510cf8 != nullptr)
        {
            const u8* obj = *g_510cf8;
            const u32 at_4 = *reinterpret_cast<const u32*>(obj + 4);
            const u32 at_8 = *reinterpret_cast<const u32*>(obj + 8);
            if (at_8 <= at_4)
            {
                // 0x0005F460 is not translated. EAX is the object at [DAT_00510CF8].
                g_510cf8 = nullptr;
            }
        }
        break;
    case 2:
        if (g_510e24 != nullptr)
        {
            const float ratio = progress_ratio(g_510e24);
            const float gate = std::bit_cast<float>(0x3dfbe76du);
            if (ratio < gate || ratio == gate)
            {
                break;
            }
        }
        if (phase != 3)
        {
            enter_phase_three();
        }
        break;
    case 3:
    case 8:
        break;
    case 4:
    case 5:
        if (g_4ee1ec != 0 || phase == 3)
        {
            break;
        }
        enter_phase_three();
        break;
    case 6:
        if (g_510e24 == nullptr || *reinterpret_cast<i32*>(g_510e24 + 0x10) != 0)
        {
            char aid[0x80];
            std::memcpy(aid, kGameselectAid, sizeof(kGameselectAid));
            std::memset(aid + sizeof(kGameselectAid), 0, sizeof(aid) - sizeof(kGameselectAid));
            switch (g_510e28)
            {
            case 1:
                // 0x0011D490 is not translated. EAX is 1.
                break;
            case 2:
                // 0x000F2780 is not translated.
                if (g_506c90 == 0)
                {
                    scene_clear_flag_block();
                }
                else
                {
                    g_429bc0 = 3;
                    xapi_copy_wide_string(g_429bc4, g_4e19b4);
                    request_scene_aid(aid, 1);
                }
                break;
            case 3:
                // 0x0004A310 is not translated. 0x000F2780 is not translated.
                break;
            case 4:
                // 0x000F2780 is not translated.
                g_429bc0 = 6;
                xapi_copy_wide_string(g_429bc4, g_4e19b4);
                request_scene_aid(aid, 1);
                break;
            case 5:
                g_429bc0 = 10;
                xapi_copy_wide_string(g_429bc4, g_4e19b4);
                request_scene_aid(aid, 1);
                break;
            case 6:
                g_429bc0 = 0xb;
                xapi_copy_wide_string(g_429bc4, g_4e19b4);
                request_scene_aid(aid, 1);
                break;
            case 7:
                // 0x000F2780 is not translated.
                g_429bc0 = 9;
                xapi_copy_wide_string(g_429bc4, g_4e19b4);
                request_scene_aid(aid, 1);
                break;
            }
        }
        break;
    case 7:
        if ((g_510e24 == nullptr || *reinterpret_cast<i32*>(g_510e24 + 0x10) != 0) && phase != 8)
        {
            g_510cf4 = 8;
            // 0x0011DD10 is not translated. EAX is 0xB.
            g_flags_ea9c = g_flags_ea9c | 0x2200u;
        }
        break;
    case 9:
        if (g_510e24 != nullptr)
        {
            const float ratio = progress_ratio(g_510e24);
            const float gate = std::bit_cast<float>(0x3f50a3d7u);
            if (ratio < gate || ratio == gate)
            {
                break;
            }
        }
        if (phase != 3)
        {
            enter_phase_three();
        }
        break;
    default:
        break;
    }

    phase = g_510cf4;
    if (phase >= 1 && phase <= 9)
    {
        switch (phase)
        {
        case 1:
        case 2:
        case 6:
        case 7:
        case 9:
            if (g_510e30 != nullptr)
            {
                // 0x00142460 is not translated. EAX is 0.
            }
            break;
        case 3:
            if (g_510cfc != nullptr)
            {
                // 0x00142460 is not translated. EAX is DAT_00510CFC.
                // A zero return would call 0x0011DEA0. A nonzero return
                // from that would call 0x001083F0 with EAX = DAT_00510CFC
                // and a stack 0x10. A nonzero return from that would call
                // 0x0011DEE0. Those returns are not taken.
            }
            else
            {
                // 0x00142460 is not translated. EAX is 0. DAT_00510E30 is
                // not tested on this path.
            }
            break;
        case 8:
            if (g_510e30 != nullptr)
            {
                // 0x00142460 is not translated. EAX is DAT_00510CFC.
            }
            break;
        default:
            break;
        }
    }

    phase = g_510cf4;
    if (phase < 2)
    {
        return;
    }
    if (7 < phase && phase != 9)
    {
        return;
    }
    if (g_510e24 == nullptr)
    {
        return;
    }

    u8* player = g_510e24;
    const i32 index = *reinterpret_cast<const i32*>(player);
    const u8* row = reinterpret_cast<const u8*>(kProgress42217c) +
                    static_cast<std::ptrdiff_t>(index) * 0x124;
    const u8* clock = *reinterpret_cast<u8* const*>(player + 0x24);
    const float num = *reinterpret_cast<const float*>(clock + 0x2c);
    const float den = *reinterpret_cast<const float*>(clock + 0x34);
    const float ratio = num / den;
    const float gate_low = std::bit_cast<float>(*reinterpret_cast<const u32*>(row + 0));
    if (ratio < gate_low || ratio == gate_low)
    {
        const i32 slot = *reinterpret_cast<const i32*>(row + 8);
        if (slot != -1)
        {
            *reinterpret_cast<i32*>(player + 0x94 + static_cast<std::ptrdiff_t>(slot) * 16) = 1;
        }
    }
    else
    {
        const float num_again = *reinterpret_cast<const float*>(clock + 0x2c);
        const float den_again = *reinterpret_cast<const float*>(clock + 0x34);
        const float ratio_again = num_again / den_again;
        const float gate_high = std::bit_cast<float>(*reinterpret_cast<const u32*>(row + 4));
        if (ratio_again < gate_high || ratio_again == gate_high)
        {
            // At or under the high gate. The slot at +0xC stays clear.
        }
        else
        {
            const i32 slot = *reinterpret_cast<const i32*>(row + 0xc);
            if (slot != -1)
            {
                *reinterpret_cast<i32*>(player + 0x94 + static_cast<std::ptrdiff_t>(slot) * 16) = 1;
            }
        }
    }
    // 0x000483E0 is not translated. EAX and the stack dword are DAT_00510E24.
}


// DAT_004E02E8. Cleared when the counter at scene+0x22C lands on 2.
// audio.cpp keeps a private copy of this address.
i32 g_4e02e8 = 0;

// DAT_00506604. 0x0011E4C0 stores 0 when 0x000341E0 returns 0.
i32 g_506604 = 0;

// DAT_004312E4. Fractional part after the 300x step is truncated into
// DAT_004312E0.
float g_4312e4 = 0.f;

i32 g_4312e0 = 0;

// DAT_00545B38. 3 would call 0x0013FFD0.
i32 g_545b38 = 0;

// DAT_0050E438. The global names the link at object+8. Next is the pointer
// at +4. vtable +0x10 is called with the object. The port keeps the method
// and the next pointer on the node the global names.
SceneTickNode* g_50e438 = nullptr;

// DAT_00510580. Nonzero, with DAT_00510650 clear, drifts the two floats.
i32 g_510580 = 0;

// DAT_00510648 and DAT_0051064C.
float g_510648 = 0.f;

float g_51064c = 0.f;

// DAT_0050E0EC. One-based index into the span at DAT_0050E0E8.
i32 g_50e0ec = 0;

// DAT_004219EC, DAT_004219F4, DAT_00421A0C, DAT_00421A14, DAT_00421A1C.
// Image dwords. 0x00038110 loads each as the first argument of 0x0015C1C0.
i32 g_4219ec = 0;

i32 g_4219f4 = 1;

i32 g_421a0c = 5;

i32 g_421a14 = 6;

i32 g_421a1c = 2;


// Xbox 0x00038110. XMM0 is multiplied by DAT_0032B1F0 (127) and saved.
// Before each call the saved product is clamped to [0, 127] and truncated
// with cvttss2si. Above 127 stores 127. Below 0 stores 0. A NaN fails both
// compares, and cvttss2si yields the indefinite integer. The five calls pass
// DAT_004219EC, DAT_004219F4, DAT_00421A0C, DAT_00421A14, and DAT_00421A1C.
// 0x0015C1C0 is not translated, so those calls are not made.
// Xbox 0x00038110
void scene_set_audio_level(float level)
{
    // Xbox 0x00038110
    const float scaled = level * 127.f;
    auto take = [scaled](i32 category)
    {
        float clamped = scaled;
        if (127.f < scaled)
        {
            clamped = 127.f;
        }
        else if (scaled < 0.f)
        {
            clamped = 0.f;
        }
        i32 bits = static_cast<i32>(0x80000000u);
        if (clamped >= -2147483648.f && clamped < 2147483648.f)
        {
            bits = static_cast<i32>(clamped);
        }
        // 0x0015C1C0 is not translated. The arguments are category and bits.
        (void)category;
        (void)bits;
    };
    take(g_4219ec);
    take(g_4219f4);
    take(g_421a0c);
    take(g_421a14);
    take(g_421a1c);
}


// DAT_004E173C. 0x0011CE10 increments it. The image leaves it at 0.
i32 g_4e173c = 0;


// Xbox 0x0011CE10
// EAX is 2 for 0x0011D490. That call is not translated.
// 0x00038910 takes the name at 0x0030F898,
// XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE, and a stack 0x10.
// xact_acquire in audio.cpp is that body. It is not visible here, so
// the call is not made. XMM0 is 0 for 0x00038110. DAT_004E173C increments.
// Xbox 0x0011CE10
void scene_begin_audio_pause()
{
    // Xbox 0x0011CE10
    // 0x0011D490 is not translated. EAX is 2.
    // 0x00038910. Name is XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE, id is 0x10.
    scene_set_audio_level(0.f);
    g_4e173c = g_4e173c + 1;
}


// Xbox 0x0011E7F0. ECX is the scene record (DAT_00510E58 when 0x0011E4C0
// calls). A nonzero dword at +0x22C is decremented and stored. Landing on 2
// clears DAT_004E02E8. The float passed to 0x00038110 is
// clamp(1 - counter * 0.5, 0, 1), from DAT_0032B0F8 and DAT_0032B0F4.
//
// DAT_004312DC above 0 subtracts DAT_005096C0 * DAT_004312C4 and stores 0
// when the result is at or below 0. That product, times 300 (DAT_0032B180),
// is added to DAT_004312E4. The truncated integer lands at DAT_004312E0 and
// the fraction stays at DAT_004312E4.
//
// 0x0004EBD0 ticks the native scene's cutscenes; 0x0005AA90 ticks the
// doors; 0x00057E60 ticks the scene dialogue. DAT_00545B38
// equal to 3 would call 0x0013FFD0. That call is not translated. 0x00117140
// runs, then 0x000D0300 ticks the event sequence and 0x00049DE0 would
// run; that one is not translated. 0x00117140 runs again. DAT_00506E28 equal to 1 calls
// 0x000F4A00 (the ghoul intro) before DAT_0050E438 is loaded.
// The list walk saves the next link, then calls vtable +0x10.
//
// 0x00123020 takes DAT_00511B18 on the stack, then 0x00122FE0 runs. 0x001128A0
// and 0x0011B670 are not translated. 0x00117100 then runs, including its
// tail into 0x00117140. 0x000F4560 ticks the ghoul intro; 0x0013FD60 is not translated.
//
// DAT_004312DC above 0 leaves the step at 0. Otherwise the step is
// DAT_005096C0 * DAT_004312C4, capped at DAT_0032B150 when that product is
// greater. The stack float for 0x00115BB0 is the step times DAT_0032B6E0.
// 0x00115BB0 and 0x001162F0 are not translated.
//
// DAT_00510580 nonzero and DAT_00510650 clear adds DAT_0032B314 to
// DAT_00510648 and DAT_0051064C, then would call 0x0011B7F0. That call is
// not translated. 0x0012AA70, 0x001185E0, 0x000FDDA0, and 0x000FD5C0 are
// not translated.
//
// A nonzero record count in DAT_0050E0E8, and a nonzero dword at
// (DAT_0050E0EC - 1) * stride + begin, would call 0x001083F0 with that
// dword in EAX and 0x10 on the stack. 0x001083F0 is not translated, so its
// return is not taken. A nonzero return with DAT_00510CEC clear would store
// 6 at DAT_00510F60, or 1 when DAT_00510CE8 is nonzero.
// Xbox 0x0011E7F0
void scene_tick_pause_counter(Game& g, u8* scene)
{
    // Xbox 0x0011E7F0
    i32 counter = *reinterpret_cast<i32*>(scene + 0x22c);
    if (counter != 0)
    {
        counter = counter - 1;
        *reinterpret_cast<i32*>(scene + 0x22c) = counter;
        if (counter == 2)
        {
            g_4e02e8 = 0;
        }
        float faded = 1.f - static_cast<float>(counter) * 0.5f;
        if (faded < 0.f)
        {
            faded = 0.f;
        }
        else if (1.f < faded)
        {
            faded = 1.f;
        }
        scene_set_audio_level(faded);
    }

    const float scaled = g_sim_dt * g_sim_scale;
    if (0.f < g_sim_freeze)
    {
        g_sim_freeze = g_sim_freeze - scaled;
        if (g_sim_freeze <= 0.f)
        {
            g_sim_freeze = 0.f;
        }
    }
    g_4312e4 = scaled * 300.f + g_4312e4;
    g_4312e0 = static_cast<i32>(g_4312e4);
    g_4312e4 = g_4312e4 - static_cast<float>(g_4312e0);

    // 0x0004EBD0 over the native scene's cutscene stack, then the doors (0x0005AA90).
    if (auto* cutscenes = find_scene_cutscene_runtime(g); cutscenes != nullptr)
    {
        tick_scene_cutscenes(*cutscenes);
    }
    if (g_runtime_doors != nullptr)
    {
        update_runtime_doors(*g_runtime_doors, g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f);
    }
    tick_scene_dialogue(g); // Xbox 0x00057E60
    if (g_545b38 == 3)
    {
        // 0x0013FFD0 is not translated.
    }
    script_tick_fired_vm(g);
    tick_script_event_sequence(g); // 0x000D0300
    // 0x00049DE0 is not translated.
    script_tick_fired_vm(g);
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).start_pending_ghoul_intro(); // 0x000F4A00 while DAT_00506E28 is 1
    }
    tick_runtime_scene_control_list(); // DAT_0050E438, each active control's vtable +0x10.
    // 0x00123020 over DAT_00511B18, then 0x00122FE0. The native actors exist once the scene's actor
    // services have started (the same gate the per-view draw uses).
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).update_scene_actors();
    }
    // 0x001128A0 is not translated.
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).tick_live_spawns(); // 0x0011B670
    }
    script_vm_tick(g, 0.f);
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).update_ghoul_intro(); // 0x000F4560
    }
    // 0x0013FD60 is not translated.

    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
        const float cap = std::bit_cast<float>(0x3c888889u);
        if (cap < step)
        {
            step = cap;
        }
    }
    const float paced = step * std::bit_cast<float>(0x40e66666u);
    // 0x00115BB0 is not translated. The stack float is paced.
    // 0x001162F0 is not translated.
    (void)paced;

    if (g_510580 != 0 && g_flag_0650 == 0)
    {
        const float drift = std::bit_cast<float>(0x3b03126fu);
        g_510648 = g_510648 + drift;
        g_51064c = g_51064c + drift;
        // 0x0011B7F0 is not translated.
    }
    // 0x0012AA70, 0x001185E0, 0x000FDDA0, and 0x000FD5C0 are not translated.

    SceneSpan* span = g_span_50e0e8;
    const i32 delta = static_cast<i32>(
        reinterpret_cast<const u8*>(span->end) - reinterpret_cast<const u8*>(span->begin));
    const i32 count = delta / static_cast<i32>(span->stride);
    if (count != 0)
    {
        const i32 slot = g_50e0ec - 1;
        const i32 byte_off = slot * static_cast<i32>(span->stride);
        const u8* record = reinterpret_cast<const u8*>(span->begin) + byte_off;
        const i32 word = *reinterpret_cast<const i32*>(record);
        if (word != 0)
        {
            // 0x001083F0 is not translated. EAX is word and the stack dword
            // is 0x10. A nonzero return is not taken, so DAT_00510F60 is
            // not written from DAT_00510CEC and DAT_00510CE8.
            (void)word;
        }
    }
}


// Xbox 0x0011CE40
// DAT_00510CF4 other than 1 copies the unsigned count at [*DAT_00510CF8]+4
// onto +8. ESI is DAT_00510E30 for 0x00142AA0. EAX is 1 for 0x0011D490.
// Those calls are not translated.
// EAX is &DAT_00510E18 and ECX is &DAT_00510E0C for 0x00118AE0. That call
// is not translated.
// DAT_004EE1EC nonzero would call 0x000584D0 with a stack 0. That call is
// not translated. DAT_004E173C decrements. DAT_00507DF4 becomes 0.
// A nonzero DAT_00510E24 is EAX for 0x00048860, then the pointer becomes 0.
// That call is not translated.
// A nonzero DAT_00510CE8 clears bit 0x20 of DAT_0050EA9C and stores 0.
// A nonzero dword at [DAT_00510CFC]+4 would call 0x001093C0. That call is
// not translated. The dword at [DAT_00510CFC]+0 selects 0x00107DD0 for 1,
// 2, and 3, 0x00107960 for 4, and 0x00107B60 for 5. EAX is DAT_00510CFC.
// Those calls are not translated. DAT_00510CFC then becomes 0.
// A nonzero DAT_00510E40 is EBX for 0x00035C00, then the pointer becomes 0.
// That call is not translated.
// A nonzero DAT_00510E34 decrements DAT_004DEA24 and stores 0.
// 0x00038460 would take XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE. The dword at
// bank+0x88 decrements. A result at or below 0, with a bank id other than
// 0x12 and a nonzero return from 0x00038590, would call 0x00038360 with
// that return in EAX. 0x00038260 then takes the bank in EAX and the id is
// stored as 0. Those four calls are not visible here, so those stores are
// not made. The tail jumps to 0x00038110. XMM0 is DAT_0032B0F8 (1.0).
// Xbox 0x0011CE40
void scene_tick_audio_pause()
{
    // Xbox 0x0011CE40
    if (g_510cf4 != 1)
    {
        u8* obj = *g_510cf8;
        const u32 count = *reinterpret_cast<u32*>(obj + 4);
        *reinterpret_cast<u32*>(obj + 8) = count;
        // 0x00142AA0 is not translated. ESI is DAT_00510E30.
        // 0x0011D490 is not translated. EAX is 1.
    }
    // 0x00118AE0 is not translated. EAX is &DAT_00510E18. ECX is &DAT_00510E0C.
    if (g_4ee1ec != 0)
    {
        // 0x000584D0 is not translated. The stack argument is 0.
    }
    g_4e173c = g_4e173c - 1;
    g_507df4 = 0;
    if (g_510e24 != nullptr)
    {
        // 0x00048860 is not translated. EAX is DAT_00510E24.
        g_510e24 = nullptr;
    }
    if (g_510ce8 != 0)
    {
        g_flags_ea9c = g_flags_ea9c & 0xffffffdfu;
        g_510ce8 = 0;
    }
    u8* rec = static_cast<u8*>(g_510cfc);
    if (*reinterpret_cast<i32*>(rec + 4) != 0)
    {
        // 0x001093C0 is not translated.
    }
    rec = static_cast<u8*>(g_510cfc);
    switch (*reinterpret_cast<i32*>(rec))
    {
    case 1:
        // 0x00107DD0 is not translated. EAX is DAT_00510CFC.
        break;
    case 2:
        // 0x00107DD0 is not translated. EAX is DAT_00510CFC.
        break;
    case 3:
        // 0x00107DD0 is not translated. EAX is DAT_00510CFC.
        break;
    case 4:
        // 0x00107960 is not translated. EAX is DAT_00510CFC.
        break;
    case 5:
        // 0x00107B60 is not translated. EAX is DAT_00510CFC.
        break;
    }
    g_510cfc = nullptr;
    if (g_510e40 != nullptr)
    {
        // 0x00035C00 is not translated. EBX is DAT_00510E40.
        g_510e40 = nullptr;
    }
    if (g_510e34 != 0)
    {
        g_4dea24 = g_4dea24 - 1;
        g_510e34 = 0;
    }
    // 0x00038460, 0x00038590, 0x00038360, and 0x00038260 are not visible
    // here. The name is XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE.
    scene_set_audio_level(1.f);
}


// Xbox 0x00140E30. The DAT_00511B40 player (actor = link - 0x93C) with a live body (+0x94) and no park (+0xCD8 below
// 1) or body +0x258 hold queues {0xB0, door} once on entering an exit volume (0x001248D0 mask 1) and sends {0xB1,
// door} to itself once on entering a mask-8 volume, whose mask bit 0x00124970 then clears; the door word is
// 0x0011F2E0 on the volume's +0xA key. The mask-0x10 volume at the same point, or at the first DAT_00511AA4 actor
// with tag 6 (0x0013D610), picks the camera tune indexes (+0xC..+0x18); leaving every one restores the saved ones.
// Xbox 0x00140E30
void scene_update_actor_marker(Game& g)
{
    SceneActorServices* actors = script_scene_actors(g);
    const u32 link = actors == nullptr ? 0 : actors->player_link();
    if (link == 0 || link == 0x93Cu)
    {
        return;
    }
    std::vector<RuntimeDirectorActor> marks = actors->marked_actors();
    const RuntimeDirectorActor* player = nullptr;
    for (const auto& mark : marks)
    {
        if (mark.address == link - 0x93C)
        {
            player = &mark;
        }
    }
    if (player == nullptr)
    {
        throw Error("DAT_00511B40 player is not on DAT_00511B38");
    }
    if (read_u16(player->body, 0x94) == 0)
    {
        g_flag_5d20 = 0;
        g_flag_5d24 = 0;
        return;
    }
    // The point is a stack slot that only the paths below write; the mask-0x10 lookup reads it either way, so it
    // keeps the last point written.
    static Vec3 point{};
    const auto door_of = [actors](const OccupancyRecord& volume) -> u16
    {
        i16 key;
        std::memcpy(&key, reinterpret_cast<const u8*>(&volume) + 0xA, 2);
        return key == 0 ? u16(0) : find_scene_spawn_type(actors->marker_records(), key); // 0x0011F2E0
    };
    Actor& actor = *player->actor;
    if (actor.parked_gate < 1 && read_u32(player->body, 0x258) == 0)
    {
        point = actor_world_position(actor); // 0x000412A0
        const OccupancyRecord* exit = actor.parked_gate < 1 ? find_occupancy_record(point, 0.f, 1, nullptr, 0) : nullptr;
        if (exit != nullptr)
        {
            if (g_flag_5d20 == 0)
            {
                const u32 message[2]{0xB0, door_of(*exit)};
                script_queue_event(g, {reinterpret_cast<const u8*>(message), sizeof(message)}); // 0x00116810
            }
            g_flag_5d20 = 1;
        }
        else
        {
            g_flag_5d20 = 0;
        }
        const OccupancyRecord* notice = find_occupancy_record(point, 0.f, 8, nullptr, 0);
        if (notice == nullptr)
        {
            g_flag_5d24 = 0;
        }
        else
        {
            if (g_flag_5d24 == 0)
            {
                i16 key;
                std::memcpy(&key, reinterpret_cast<const u8*>(notice) + 0xA, 2);
                if (key != 0)
                {
                    std::array<u32, 0x42> message{};
                    message[0] = 0xB1;
                    message[1] = find_scene_spawn_type(actors->marker_records(), key); // 0x0011F2E0
                    actors->send_actor_message(player->address, message); // Actor vtable +0xC.
                }
                clear_occupancy_record_mask(const_cast<OccupancyRecord&>(*notice), 8); // 0x00124970
            }
            g_flag_5d24 = 1;
        }
    }
    const OccupancyRecord* tune = find_occupancy_record(point, 0.f, 0x10, nullptr, 0);
    if (tune == nullptr)
    {
        if (Actor* listener = actors->first_latched_actor(6)) // 0x0013D610
        {
            point = actor_world_position(*listener); // 0x000412A0
            tune = find_occupancy_record(point, 0.f, 0x10, nullptr, 0);
        }
    }
    if (tune == nullptr)
    {
        if (g_flag_5d28 != 0)
        {
            g_tune_34008 = script_tune(0x00314de8u, g_tune_5d2c);
            g_tune_34014 = script_tune(0x00314dfcu, g_tune_5d2c);
            g_tune_5aec = g_tune_5d2c;
            g_tune_34010 = script_tune(0x00314e10u, g_tune_5d30);
            g_tune_5af0 = g_tune_5d30;
            g_tune_3402c = script_tune(0x00314e28u, g_tune_5d34);
            g_tune_5af4 = g_tune_5d34;
            g_tune_34030 = script_tune(0x00314e38u, g_tune_5d38);
            g_tune_5af8 = g_tune_5d38;
            g_flag_5d28 = 0;
        }
        return;
    }
    if (g_flag_5d28 == 0)
    {
        g_tune_5d2c = g_tune_5aec;
        g_tune_5d30 = g_tune_5af0;
        g_tune_5d34 = g_tune_5af4;
        g_tune_5d38 = g_tune_5af8;
    }
    if (tune->value_0 != u32(g_flag_5d28))
    {
        const auto word = [tune](size_t offset)
        {
            u32 value;
            std::memcpy(&value, reinterpret_cast<const u8*>(tune) + offset, 4);
            return value;
        };
        g_tune_5aec = word(0xC);
        g_tune_5af0 = word(0x10);
        g_tune_5af4 = word(0x14);
        g_tune_5af8 = word(0x18);
        g_tune_34008 = script_tune(0x00314de8u, g_tune_5aec);
        g_tune_34014 = script_tune(0x00314dfcu, g_tune_5aec);
        g_tune_34010 = script_tune(0x00314e10u, g_tune_5af0);
        g_tune_3402c = script_tune(0x00314e28u, g_tune_5af4);
        g_flag_5d28 = i32(tune->value_0);
        g_tune_34030 = script_tune(0x00314e38u, g_tune_5af8);
    }
}


// Xbox 0x00106E90
// Each movie event slot (DAT_0050DD90) whose movie plays (state 2) gives back its play count (+0x18) and
// waits (state 3). The mode DAT_0050DD8C becomes 3.
void scene_enable_overlays()
{
    auto& movies = native_movie_runtime();
    for (auto& slot : movies.session->slots)
    {
        if (slot.movie != nullptr && slot.state == 2)
        {
            --slot.movie->play_count;
            slot.state = 3;
        }
    }
    movies.mode = 3;
}


// Xbox 0x00106EC0
// Each waiting slot (state 3) plays again (state 2, play count +1). The mode DAT_0050DD8C becomes 2.
void scene_disable_overlays()
{
    auto& movies = native_movie_runtime();
    for (auto& slot : movies.session->slots)
    {
        if (slot.movie != nullptr && slot.state == 3)
        {
            ++slot.movie->play_count;
            slot.state = 2;
        }
    }
    movies.mode = 2;
}


// Xbox 0x001162D0
// EAX is DAT_00510E58. 0x00116220 is not translated, so its index is not
// scaled by 0x90 and added to the directory at 0x0042AC80. That pointer
// is not stored at DAT_005462E8.
// Xbox 0x001162D0
void scene_clear_runtime_state()
{
    // Xbox 0x001162D0
}


// DAT_0051065C. 0x0011C5A0 holds 1 across the teardown, then stores 0.
i32 g_51065c = 0;

// DAT_0050D770, +4, +8, +0xC, and the count at DAT_0050D780.
// entity.cpp keeps a private copy of this table.
void* g_50d770 = nullptr;

void* g_50d774 = nullptr;

void* g_50d778 = nullptr;

void* g_50d77c = nullptr;

i32 g_50d780 = 0;

// DAT_00510584. 0x001759F0 is the D3D resource release.
void* g_510584 = nullptr;

// DAT_0050EA00 and DAT_0050EA01 are bytes. The floats are DAT_0050EA04,
// +0x08, +0x0C, +0x10, +0x14, +0x18, then DAT_0050EA7C through DAT_0050EA90.
// DAT_0050EA94 is a dword. entity.cpp keeps the colour floats.
u8 g_50ea00 = 0;

u8 g_50ea01 = 0;

float g_50ea04 = 0.f;

float g_50ea08 = 0.f;

float g_50ea0c = 0.f;

float g_50ea10 = 0.f;

float g_50ea14 = 0.f;

float g_50ea18 = 0.f;

float g_50ea7c = 0.f;

float g_50ea80 = 0.f;

float g_50ea84 = 0.f;

float g_50ea88 = 0.f;

float g_50ea8c = 0.f;

float g_50ea90 = 0.f;

i32 g_50ea94 = 0;

i32 g_50ea98 = 0;
 // DAT_0050EA98
i32 g_504330 = 0;
 // DAT_00504330
// DAT_00512344 is the block 0x000663E0 releases. DAT_00512348 is cleared with it.
void* g_512344 = nullptr;

i32 g_512348 = 0;

i32 g_545d20 = 0;
 // DAT_00545D20
i32 g_545d28 = 0;
 // DAT_00545D28
i32 g_50e148 = 0;
 // DAT_0050E148
i32 g_50e16c = 0;
 // DAT_0050E16C
i32 g_50e1b0 = 0;
 // DAT_0050E1B0
i32 g_50e1c4 = 0;
 // DAT_0050E1C4
i32 g_50e228 = 0;
 // DAT_0050E228
i32 g_545b3c = 0;
 // DAT_00545B3C
u8 g_545b44 = 0;
  // DAT_00545B44
u8 g_545bc4 = 0;
  // DAT_00545BC4
i32 g_4dea5c = 0;
 // DAT_004DEA5C
// DAT_0051108C is the block 0x000663E0 releases.
void* g_51108c = nullptr;

alignas(4) u8 g_4312c8[0x14]{};
// DAT_004312C8..DAT_004312D8, the times 0x0012B1F0 advances.
float& g_4312cc = *reinterpret_cast<float*>(g_4312c8 + 4);
 // DAT_004312CC
float& g_4312d0 = *reinterpret_cast<float*>(g_4312c8 + 8);
 // DAT_004312D0
i32& g_4312d4 = *reinterpret_cast<i32*>(g_4312c8 + 0xC);
     // DAT_004312D4
i32& g_4312d8 = *reinterpret_cast<i32*>(g_4312c8 + 0x10);
     // DAT_004312D8
i32 g_511a10 = 0;
     // DAT_00511A10
// Object pointer at +0 of each DAT_0050EAA0 record. Stride 0x2C, eight
// records. DAT_0050EC00 is the count. entity.cpp keeps a private copy.
void* g_50eaa0 = nullptr;

void* g_50eacc = nullptr;

void* g_50eaf8 = nullptr;

void* g_50eb24 = nullptr;

void* g_50eb50 = nullptr;

void* g_50eb7c = nullptr;

void* g_50eba8 = nullptr;

void* g_50ebd4 = nullptr;

i32 g_50ec00 = 0;


// Xbox 0x0011C5A0
// ESI is the scene record. The dword at +0x118 is cleared. DAT_0051065C is
// 1 for the body and 0 on the way out.
//
// DAT_004EE1EC nonzero would call 0x000584D0 with a stack 0. That call is
// not translated. 0x0004ECE0, 0x00049EF0, and 0x0005B1E0 are not translated.
// 0x00115F00 is not translated. 0x00122C70 is not translated: EDI points at
// a stack dword holding 0x80. 0x001232D0, 0x001222A0, 0x00110AB0, 0x001143A0,
// 0x00123640, 0x001199F0, and 0x0011F610 are not translated.
//
// A nonzero DAT_00510584 is the argument of 0x001759F0, the D3D resource
// release. No D3D8 object is bound to that pointer, so the call is not made
// and the pointer is cleared. DAT_00510580 is cleared. 0x001133B0 is not
// translated. 0x000FD710 is not translated.
//
// Bit 8 of DAT_0050EA9C is written back as 0x100 after the dword is cleared.
// DAT_0050EA98 is cleared. 0x000ED450 is not translated. 0x00129F70 is not
// translated. 0x0003A1F0 is not translated: ESI is 0xD. 0x00118560 is not
// translated.
//
// A nonzero DAT_00512344 is released through 0x000663E0. DAT_00512348 is
// cleared. DAT_00545D20 becomes 1. 0x000F44B0 is not translated. 0x0012A9B0
// is not translated. 0x00123B30 runs. 0x001163D0 and 0x00132100 are not
// translated. 0x000FAE40 is not translated. 0x0011B630 runs. A nonzero
// DAT_0051108C is released through 0x000663E0. 0x00117330 is not translated.
// 0x001272B0 is not translated: EAX is 1 when DAT_00430D0C is 1, and 0
// otherwise.
//
// DAT_004312C4 becomes 1, the dword at DAT_0032B0F8. 0x00048F10, 0x00057DC0,
// and 0x0011CD90 are not translated; 0x00106E30 releases the movie event slots. 0x0011AEA0 is not
// translated.
// 0x00173F30 is D3D_KickOffAndWaitForIdle. 0x001593D0 and 0x000F97F0 are not
// translated. 0x00035D30 is not translated, so its return is not taken and
// the two 0x00036180 calls in that loop are not made. 0x00034880 is not
// translated.
// Xbox 0x0011C5A0
void scene_teardown(u8* scene)
{
    // Xbox 0x0011C5A0
    g_51065c = 1;
    if (g_4ee1ec != 0)
    {
        // 0x000584D0 is not translated. The stack argument is 0.
    }
    // 0x0004ECE0, 0x00049EF0, and 0x0005B1E0 are not translated.
    g_50d780 = 0;
    g_50d770 = nullptr;
    g_50d774 = nullptr;
    g_50d778 = nullptr;
    g_50d77c = nullptr;
    // 0x00115F00 is not translated.
    // 0x00122C70 is not translated. EDI points at a stack dword holding 0x80.
    // 0x001232D0, 0x001222A0, 0x00110AB0, 0x001143A0, 0x00123640, 0x001199F0,
    // and 0x0011F610 are not translated.
    if (g_510584 != nullptr)
    {
        // 0x001759F0 D3DResource_Release. The argument is DAT_00510584.
        g_510584 = nullptr;
    }
    g_510580 = 0;
    // 0x001133B0 is not translated.
    g_50ea00 = 0;
    g_50ea01 = 0;
    g_50ea0c = 0.f;
    g_50ea08 = 0.f;
    g_50ea04 = 0.f;
    g_50ea18 = 0.f;
    g_50ea14 = 0.f;
    g_50ea10 = 0.f;
    g_50ea84 = 0.f;
    g_50ea80 = 0.f;
    g_50ea7c = 0.f;
    g_50ea90 = 0.f;
    g_50ea8c = 0.f;
    g_50ea88 = 0.f;
    g_50ea94 = 0;
    // 0x000FD710 is not translated.
    const u32 kept = (g_flags_ea9c >> 8) & 1u;
    g_50ea98 = 0;
    g_flags_ea9c = 0;
    if (kept != 0)
    {
        g_flags_ea9c = 0x100u;
    }
    *reinterpret_cast<i32*>(scene + 0x118) = 0;
    // 0x000ED450 is not translated.
    g_504330 = 0;
    // 0x00129F70 is not translated.
    // 0x0003A1F0 is not translated. ESI is 0xD.
    // 0x00118560 is not translated.
    if (g_512344 != nullptr)
    {
        engine_heap_free(g_512344);
        g_512344 = nullptr;
    }
    g_512348 = 0;
    g_545d20 = 1;
    g_545d28 = 0;
    g_50e148 = 0;
    g_50e16c = 0;
    g_50e1b0 = 0;
    g_50e1c4 = 0;
    g_50e228 = 0;
    // 0x000F44B0 is not translated.
    g_545b38 = 0;
    g_545b3c = 0;
    g_545b44 = 0;
    g_545bc4 = 0;
    // 0x0012A9B0 is not translated.
    effects_clear_voice_slots();
    // 0x001163D0 and 0x00132100 are not translated.
    g_4dea5c = 0;
    // 0x000FAE40 is not translated.
    entity_release_aid_records();
    if (g_51108c != nullptr)
    {
        engine_heap_free(g_51108c);
        g_51108c = nullptr;
    }
    // 0x00117330 is not translated.
    // 0x001272B0 is not translated. EAX is 1 when DAT_00430D0C is 1, else 0.
    const i32 arg_1272b0 = g_script_30d0c == 1 ? 1 : 0;
    (void)arg_1272b0;
    g_4312cc = 0.f;
    g_4312d0 = 0.f;
    g_4312d8 = 0;
    g_4312d4 = 0;
    g_sim_scale = 1.f;
    // 0x00048F10, 0x00057DC0 and 0x0011CD90 are not translated.
    release_movie_events(native_movie_runtime()); // 0x00106E30
    g_511a10 = 0;
    // 0x0011AEA0 is not translated.
    g_50eaa0 = nullptr;
    g_50eacc = nullptr;
    g_50eaf8 = nullptr;
    g_50eb24 = nullptr;
    g_50eb50 = nullptr;
    g_50eb7c = nullptr;
    g_50eba8 = nullptr;
    g_50ebd4 = nullptr;
    g_50ec00 = 0;
    // 0x00173F30 D3D_KickOffAndWaitForIdle.
    // 0x001593D0 and 0x000F97F0 are not translated.
    // 0x00035D30 is not translated, so its return is not taken.
    // 0x00036180 is not translated. 0x00034880 is not translated.
    g_51065c = 0;
}


// Xbox 0x0011E6A0
// ESI is the scene record. EBX is the state being entered. Equal values
// return. Leaving 1 or 2 increments DAT_0050E7A0 and calls 0x00106E90.
// Leaving 4 decrements DAT_00510CEC and DAT_004DEA24. Leaving 5 calls
// 0x0012C4E0. Leaving 6 calls 0x0011CE40. Leaving 3 does nothing.
//
// Entering 1 or 2 decrements DAT_0050E7A0, stores 0 when the result is
// below 0, calls 0x00106EC0, stores EBX at +0x104, and returns. Entering 3
// stores 1 at +0x110 and 0 at +0x120 and +0x114, stores EBX, and returns.
// Entering 6 calls 0x0011CE10, stores EBX, and returns. Entering 4 calls
// 0x0011C5A0 with ESI, increments DAT_00510CEC and DAT_004DEA24, copies the
// C string at +0x80 onto +0, clears the byte at +0x80, calls 0x001162D0,
// stores 4 at +0x22C, calls 0x00038110 with XMM0 cleared, calls 0x0011CAC0,
// and stores 1 at +0x120. That path, state 5, and every other entry store
// EBX at +0x104.
// Xbox 0x0011E6A0
void scene_set_state(u8* scene, i32 next)
{
    // Xbox 0x0011E6A0
    const i32 state = *reinterpret_cast<i32*>(scene + 0x104);
    if (state != next)
    {
        switch (state)
        {
        case 1:
        case 2:
            g_50e7a0 = g_50e7a0 + 1;
            scene_enable_overlays();
            break;
        case 4:
            g_510cec = g_510cec + -1;
            g_4dea24 = g_4dea24 + -1;
            break;
        case 5:
            end_scene_transition(); // 0x0012C4E0
            break;
        case 6:
            scene_tick_audio_pause();
            break;
        }
        switch (next)
        {
        case 1:
        case 2:
            g_50e7a0 = g_50e7a0 + -1;
            if (g_50e7a0 < 0)
            {
                g_50e7a0 = 0;
            }
            scene_disable_overlays();
            *reinterpret_cast<i32*>(scene + 0x104) = next;
            return;
        case 3:
            *reinterpret_cast<i32*>(scene + 0x110) = 1;
            *reinterpret_cast<i32*>(scene + 0x120) = 0;
            *reinterpret_cast<i32*>(scene + 0x114) = 0;
            *reinterpret_cast<i32*>(scene + 0x104) = next;
            return;
        case 4:
            scene_teardown(scene);
            g_510cec = g_510cec + 1;
            g_4dea24 = g_4dea24 + 1;
            {
                char* pending = reinterpret_cast<char*>(scene + 0x80);
                char* cursor = pending;
                char unit = 0;
                do
                {
                    unit = *cursor;
                    *(cursor - 0x80) = unit;
                    cursor = cursor + 1;
                }
                while (unit != '\0');
                *pending = '\0';
            }
            scene_clear_runtime_state();
            *reinterpret_cast<i32*>(scene + 0x22c) = 4;
            scene_set_audio_level(0.f);
            scene_reset_runtime_parameters();
            *reinterpret_cast<i32*>(scene + 0x120) = 1;
            break;
        case 6:
            scene_begin_audio_pause();
            *reinterpret_cast<i32*>(scene + 0x104) = next;
            return;
        }
        *reinterpret_cast<i32*>(scene + 0x104) = next;
    }
}


// Xbox 0x0011E4C0
// EAX is DAT_00510E58. ESI keeps that record. frame_tick passes that address.
// A clear dword at +0x110 and 'aid_' (0x5f646961) at +0x80 calls 0x0011E6A0
// with EBX = 3. DAT_0054632C other than 1 and other than 2 then calls
// 0x0012C5D0. +0x104 is copied to +0x10C. When +0x108 differs from +0x104,
// +0x110 is clear, and DAT_00510CE8 is clear, 0x0011E6A0 runs with EBX
// equal to the dword at +0x108.
// EAX for 0x00109470 is the pointer at +0x230. 0x0011CFB0 follows.
// The switch is the dword at +0x104.
// State 2 stores 0 at DAT_00510F60, which is the dword at +0x108, calls
// 0x0011E7F0 with ECX = the record, then 0x0012C340.
// State 3 calls 0x0011E6A0 with EBX = 4 when +0x114 is nonzero, stores 1
// at +0x114, then 0x0012C340.
// State 4 stores 1 at DAT_004E02E8. A zero from 0x000341E0 stores 0 at
// DAT_00506604, and DAT_0054632C equal to 2 calls 0x0012C770, clears +0x110,
// calls 0x0011CB60, stores 5 at DAT_00510F60, then 0x0012C340.
// State 5 with DAT_0054632C clear stores the dword at +0x100 into
// DAT_00510F60 and calls 0x0012C340. A nonzero DAT_0054632C with
// DAT_00510F7C clear joins the epilogue. Otherwise state 5 falls into
// state 1. State 1 calls 0x0011E7F0 with ECX = the record.
// Every other state, and the paths that do not return above, call 0x0012C340.
void script_pump(Game& g)
{
    // Xbox 0x0011E4C0
    g_transition_game = &g;
    g_sim_dt = g.clock.dt;
    u8* scene = g_510e58;
    if (*reinterpret_cast<i32*>(scene + 0x110) == 0 &&
        *reinterpret_cast<u32*>(scene + 0x80) == kAidPrefixEd8)
    {
        scene_set_state(scene, 3);
        if (g_54632c != 1 && g_54632c != 2)
        {
            begin_scene_transition(reinterpret_cast<const char*>(scene + 0x80)); // 0x0012C5D0
        }
    }
    const i32 state = *reinterpret_cast<i32*>(scene + 0x104);
    const i32 queued = *reinterpret_cast<i32*>(scene + 0x108);
    *reinterpret_cast<i32*>(scene + 0x10c) = state;
    if (queued != state && *reinterpret_cast<i32*>(scene + 0x110) == 0 && g_510ce8 == 0)
    {
        scene_set_state(scene, queued);
    }
    void* object = *reinterpret_cast<void**>(scene + 0x230);
    script_refresh_object_record(object);
    scene_tick_load_phase();
    switch (*reinterpret_cast<i32*>(scene + 0x104))
    {
    case 2:
        *reinterpret_cast<i32*>(scene + 0x108) = 0;
        scene_tick_pause_counter(g, scene);
        tick_scene_transition(); // 0x0012C340
        return;
    case 3:
        if (*reinterpret_cast<i32*>(scene + 0x114) != 0)
        {
            scene_set_state(scene, 4);
            // 0x0011E6A0 copied the requested aid onto the scene record and
            // 0x001162D0 points the directory slot at it. That slot is a file,
            // so the host load runs with the name now at the record.
            const char* aid = reinterpret_cast<const char*>(scene);
            if (std::memcmp(aid, "aid_", 4) == 0)
            {
                script_load(g, aid);
            }
            // The file 0x001162D0 records is in. +0x110 is the hold 0x0011E4C0
            // tests before it accepts the next aid. Retail clears it from state
            // 4 once the transition phase is 2. That store belongs to the
            // handler this load was waiting on, and the handler is not
            // translated. The host read has returned, so the hold is clear.
        }
        *reinterpret_cast<i32*>(scene + 0x114) = 1;
        tick_scene_transition(); // 0x0012C340
        return;
    case 4:
        g_4e02e8 = 1;
        if (scene_check_pending_records() == 0)
        {
            g_506604 = 0;
            if (g_54632c == 2)
            {
                leave_scene_transition(); // 0x0012C770
                *reinterpret_cast<i32*>(scene + 0x110) = 0;
                script_queue_scene_start(g);
                *reinterpret_cast<i32*>(scene + 0x108) = 5;
                tick_scene_transition(); // 0x0012C340
                return;
            }
        }
        break;
    case 5:
        if (g_54632c == 0)
        {
            *reinterpret_cast<i32*>(scene + 0x108) = *reinterpret_cast<i32*>(scene + 0x100);
            tick_scene_transition(); // 0x0012C340
            return;
        }
        if (g_510f7c == 0)
        {
            break;
        }
        scene_tick_pause_counter(g, scene);
        break;
    case 1:
        scene_tick_pause_counter(g, scene);
        break;
    default:
        break;
    }
    tick_scene_transition(); // 0x0012C340
}

} // namespace gbtg
