#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/transition_data.h"

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
#include "../scene/scene_loading.h"
#include "script_actions.h"
#include "script_conditions.h"

namespace gbtg
{
using namespace scene_detail;


constexpr const char* kScriptCondName[27] = {
    "scriptConditionId_TimeOut",
    "scriptConditionId_NumberGhouliesAlive",
    "scriptConditionId_ScareSourceProximity",
    "scriptConditionId_ObjectPickedUp",
    "scriptConditionId_NumberGhouliesKilled",
    "scriptConditionId_NumberOfKnockdowns",
    "scriptConditionId_PlayerApproachesDoor",
    "scriptConditionId_HauntedObjectActivated",
    "scriptConditionId_PlayerEntersRegion",
    "scriptConditionId_PlayerLeavesRegion",
    "scriptConditionId_SpecificGhouliesKilled",
    "scriptConditionId_SpecificGhouliesAlive",
    "scriptConditionId_ObjectByMarkerRemoved",
    "scriptConditionId_PlayerDeflectionContact",
    "scriptConditionId_PlayerHitGhouly",
    "scriptConditionId_GhoulyTriggered",
    "scriptConditionId_GhoulyExistsInScene",
    "scriptConditionId_ObjectByMarkerBroken",
    "scriptConditionId_ActorHasNoEnergy",
    "scriptConditionId_EveryNthKill",
    "scriptConditionId_NoGhoulySpawnsActive",
    "scriptConditionId_IsChallenge21Active",
    "scriptConditionId_IsChallenge21NotActive",
    "scriptConditionId_GhoulyEntersRegion",
    "scriptConditionId_GhoulyHasEnergyLEQ",
    "scriptConditionId_GhoulySignal",
    "scriptConditionId_ObjectByMarkerNotBroken",
};


std::vector<ScriptCond> g_script_conds;

std::vector<u32> g_script_fired;

static_assert(offsetof(LiveScript, word_c) == offsetof(LiveScript, type) + 4);

static_assert(offsetof(LiveScript, dword_10) == offsetof(LiveScript, type) + 8);

static_assert(offsetof(LiveScript, word_14) == offsetof(LiveScript, type) + 0xc);

LiveScript g_live_anchor{};

LiveScript*& g_live_scripts = g_live_anchor.next;


// Xbox 0x00116890
const u32* find_runtime_script_event_type(u32 type)
{
    LiveScript* node = g_live_scripts;
    while (node != nullptr)
    {
        if (node->type == type)
        {
            return &node->type;
        }
        node = node->next;
    }
    return nullptr;
}

// Xbox 0x00116950. These event IDs are the accepting arms of the message
// filter; scene startup (0x0011CBD0) submits 0x7D directly.
// Xbox 0x00116950
bool script_accepts_event(u32 type)
{
    switch (type)
    {
    case 0x7d:
    case 0x81:
    case 0xad:
    case 0xae:
    case 0xb4:
    case 0xb5:
    case 0xb7:
    case 0xb8:
    case 0xb9:
        return true;
    case 0xb6:
        return find_runtime_script_event_type(0xB6) == nullptr;
    default:
        throw Error("untranslated script message filter: " + std::to_string(type));
    }
}


// Xbox 0x00116810. Heap allocation is the 0x000F72D0 boundary; the linked
// insertion and message copy retain the original ownership and ordering.
// Xbox 0x00116810
void script_queue_event(Game& g, std::span<const u8> message)
{
    const u32 type = read_u32(message, 0);
    if (script_accepts_event(type))
    {
        auto* live = new LiveScript{};
        LiveScript* tail = &g_live_anchor;
        while (tail->next != nullptr)
        {
            tail = tail->next;
        }
        tail->next = live;
        live->prev = tail;
        std::copy(message.begin(), message.end(), live->message.begin());
        live->type = type;
        live->timestamp = g.time;
    }
}


// DAT_0050E568. Handler 2 writes this block. 0x0050E570 and 0x0050E660 are
// the bases of the scaled stores. 0x001077B0 returns at most 0xE9, which is
// the last index those stores reach.
constexpr u32 kSceneWordBase = 0x0050E568u;

constexpr u32 kSceneWordCount = ((0x0050E660u + 0xEAu * 4u) - kSceneWordBase) / 4u;

static_assert((0x0050E758u - kSceneWordBase) / 4u < kSceneWordCount);

static_assert((0x0050E660u + 0xE9u * 4u - kSceneWordBase) / 4u < kSceneWordCount);

u32 g_scene_words[kSceneWordCount]{};


u32& scene_word(u32 addr)
{
    return g_scene_words[(addr - kSceneWordBase) / 4u];
}


u32 cond_u32(const ScriptCond& rec, u32 at)
{
    return u32(rec.raw[at]) | (u32(rec.raw[at + 1]) << 8) | (u32(rec.raw[at + 2]) << 16) | (u32(rec.raw[at + 3]) << 24);
}


void cond_put_u32(ScriptCond& rec, u32 at, u32 v)
{
    rec.raw[at] = u8(v);
    rec.raw[at + 1] = u8(v >> 8);
    rec.raw[at + 2] = u8(v >> 16);
    rec.raw[at + 3] = u8(v >> 24);
}


float cond_f32(const ScriptCond& rec, u32 at)
{
    const u32 bits = cond_u32(rec, at);
    float v = 0.f;
    std::memcpy(&v, &bits, 4);
    return v;
}


void cond_put_f32(ScriptCond& rec, u32 at, float v)
{
    u32 bits = 0;
    std::memcpy(&bits, &v, 4);
    cond_put_u32(rec, at, bits);
}


u32 node_u32(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel + 4 > node.size || node.offset + rel + 4 > script.bytes.size())
    {
        return 0;
    }
    return read_u32(script.bytes, node.offset + rel);
}


const ScriptNode* script_node_at(const SceneScript& script, u32 offset)
{
    for (const ScriptNode& node : script.nodes)
    {
        if (node.offset == offset)
        {
            return &node;
        }
    }
    return nullptr;
}


const char* script_condition_name(u32 opcode)
{
    if (opcode >= 27)
    {
        return "scriptConditionId_Unknown";
    }
    return kScriptCondName[opcode];
}


void log_script_nodes(const SceneScript& script)
{
    const std::ios_base::fmtflags saved = std::cout.flags();
    for (const ScriptNode& node : script.nodes)
    {
        std::cout << "  node +0x" << std::hex << node.offset << " size 0x" << node.size << " type 0x" << node.type;
        if (!node.text.empty())
        {
            std::cout << " " << node.text;
        }
        std::cout << "\n";
    }
    std::cout.flags(saved);
}


void script_vm_reset()
{
    g_script_conds.clear();
    g_script_fired.clear();
}


// Xbox 0x00124C80. Grows DAT_0051234C by the 0x20 stride and copies `size` bytes.
void script_append_condition(const ScriptCond& src, u32 size, u32 opcode)
{
    ScriptCond rec{};
    if (size > sizeof(rec.raw))
    {
        size = u32(sizeof(rec.raw));
    }
    if (size != 0)
    {
        std::memcpy(rec.raw, src.raw, size);
    }
    const std::ios_base::fmtflags saved = std::cout.flags();
    std::cout << "script condition opcode " << opcode << " " << script_condition_name(opcode) << " size 0x" << std::hex
              << size << " link +0x" << cond_u32(rec, 4) << "\n";
    std::cout.flags(saved);
    g_script_conds.push_back(rec);
}


// Xbox 0x00132650. ucomiss of the float at +8 against 0. Equal, and not a
// NaN, returns 1 so the eval pass fires the link and drops the record.
// Xbox 0x00132650
int script_condition_timer_expired(const ScriptCond& rec)
{
    const float left = cond_f32(rec, 8);
    if (left == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132680. Opcode 1 in the table at 0x00314330. Walks DAT_00511B48.
// Each node whose word at [node+0x18]+0x94 is nonzero adds one. A count at
// or below the int at record+8, and a ucomiss of the float at record+0xc
// against 0 that is equal and not a NaN, returns 1.
// Xbox 0x00132680
int script_condition_actor_count_at_most(const ScriptCond& rec)
{
    int count = 0;
    for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
    {
        if (link->at_18->word_94 != 0)
        {
            count = count + 1;
        }
    }
    if (count <= static_cast<i32>(cond_u32(rec, 8)) && cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x000E9210. EAX is the DAT_0050E4DC node. A non-null prev stores next
// at prev+4. A non-null next stores prev at next+0. Both links are then
// cleared. The tail matches 0x000663E0: 0x000F7100 selects the heap and
// 0x0014D360 frees the node.
// Xbox 0x000E9210
void script_release_event(LiveScript* node)
{
    LiveScript* prev = node->prev;
    if (prev != nullptr)
    {
        prev->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->prev = prev;
    }
    node->prev = nullptr;
    node->next = nullptr;
    engine_heap_free(node);
}


// Xbox 0x001326D0. Opcode 2 in the table at 0x00314330. The record is not
// read. Walks DAT_0050E4DC. An empty list, or no node whose dword at +8 is
// 0xB8, returns 0. Xbox then adds 8 and returns 0 when that address is 0.
// Otherwise the walk restarts at the head. The next pointer is taken before
// 0x000E9210, and every node with 0xB8 at +8 is passed there. The walk
// returns 1.
// Xbox 0x001326D0
int script_condition_consume_ready_events(const ScriptCond&)
{
    LiveScript* found = g_live_scripts;
    for (;;)
    {
        if (found == nullptr)
        {
            return 0;
        }
        if (found->type == 0xb8u)
        {
            break;
        }
        found = found->next;
    }
    if (reinterpret_cast<std::uintptr_t>(found) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    LiveScript* cursor = g_live_scripts;
    do
    {
        LiveScript* next = cursor->next;
        if (cursor->type == 0xb8u)
        {
            script_release_event(cursor);
        }
        cursor = next;
    } while (cursor != nullptr);
    return 1;
}


// Xbox 0x00132720. Opcode 3 in the table at 0x00314330. Walks DAT_0050E4DC
// until a node whose dword at +8 is 0xAD. An empty list returns 0. The
// address of that dword is tested against 0. The word four bytes after it
// is compared with the word at record+8. A miss takes next, and returns 0
// when that pointer is 0. Otherwise it walks next until a node has the same
// dword at +8, returning 0 if the walk ends. The address of that dword is
// tested again. A hit ucomiss the float at record+0xC with the 0 at
// 0x0032B100. Equal, and not a NaN, passes the node (that address minus 8)
// to 0x000E9210 and returns 1. Every other path returns 0.
// Xbox 0x00132720
int script_condition_consume_matching_event(const ScriptCond& rec)
{
    LiveScript* node = g_live_scripts;
    for (;;)
    {
        if (node == nullptr)
        {
            return 0;
        }
        if (node->type == 0xadu)
        {
            break;
        }
        node = node->next;
    }
    if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    do
    {
        if (node->word_c == want)
        {
            if (cond_f32(rec, 0xc) != 0.f)
            {
                return 0;
            }
            script_release_event(node);
            return 1;
        }
        LiveScript* cursor = node->next;
        const u32 saved = node->type;
        if (cursor == nullptr)
        {
            return 0;
        }
        while (cursor->type != saved)
        {
            cursor = cursor->next;
            if (cursor == nullptr)
            {
                return 0;
            }
        }
        node = cursor;
    } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
    return 0;
}


// DAT_004E1C34. 0x001327A0 compares this dword with 0x80000000.
u32 g_4e1c34 = 0;


// Dwords 0x0012AE40 reads. The block starts at DAT_00512B78. The first
// dword that function reads is 0x00512C6C, one 0xF4 slot later. Six fields
// sit 0xF4 apart and the row stride is 0x5B8. Category n is 0x3A24 bytes
// later. 0x0012AF10 walks seven categories, so the last read is category 6.
constexpr u32 kStat512c6c = 0x00512B78u;

constexpr u32 kStat512c6cLast = 0x0051303Cu + 6u * 0x3A24u + 9u * 0x5B8u + 0xF4u;

i32 g_stat_512c6c[(kStat512c6cLast + 4u - kStat512c6c) / 4u]{};


i32& script_statistic_at_address(u32 addr)
{
    return g_stat_512c6c[(addr - kStat512c6c) / 4u];
}


// Xbox 0x0012AE40. ECX is the category. The caller clears it. ECX is
// multiplied by 0x3A24 and 0x0051303C is added. Ten rows each add the
// dwords at -0x3D0, -0x2DC, -0x1E8, -0xF4, +0xF4, and +0, then the cursor
// advances by 0x5B8.
// Xbox 0x0012AE40
int script_sum_statistics(i32 category)
{
    i32 sum = 0;
    u32 cursor = 0x0051303Cu + static_cast<u32>(category) * 0x3A24u;
    i32 rows = 10;
    do
    {
        const i32 edi = script_statistic_at_address(cursor - 0x2dcu);
        i32 esi = script_statistic_at_address(cursor - 0x3d0u);
        esi += edi;
        esi += script_statistic_at_address(cursor - 0x1e8u);
        esi += script_statistic_at_address(cursor - 0xf4u);
        esi += script_statistic_at_address(cursor + 0xf4u);
        esi += script_statistic_at_address(cursor);
        sum += esi;
        cursor += 0x5b8u;
        rows -= 1;
    } while (rows != 0);
    return sum;
}


// Xbox 0x001327A0. Opcode 4 in the table at 0x00314330. 0x0012AE40 is
// compared with the int at record+8. A smaller count returns 0. A zero
// dword at +0x10 stores 1 there and stores whether DAT_004E1C34 is below
// 0x80000000 at +0x14. A zero at +0x14 then ucomiss the float at +0xC
// with the 0 at 0x0032B100 and returns 0 when they differ or either is
// NaN. A nonzero +0x14 returns 0 while DAT_004E1C34 stays below
// 0x80000000. Every other path returns 1.
// Xbox 0x001327A0
int script_condition_statistics_latch(ScriptCond& rec)
{
    const i32 count = script_sum_statistics(0);
    if (count < static_cast<i32>(cond_u32(rec, 8)))
    {
        return 0;
    }
    if (static_cast<i32>(cond_u32(rec, 0x10)) == 0)
    {
        const u32 below = g_4e1c34 < 0x80000000u ? 1u : 0u;
        cond_put_u32(rec, 0x10, 1u);
        cond_put_u32(rec, 0x14, below);
    }
    if (static_cast<i32>(cond_u32(rec, 0x14)) == 0)
    {
        if (cond_f32(rec, 0xc) != 0.f)
        {
            return 0;
        }
    }
    else if (g_4e1c34 < 0x80000000u)
    {
        return 0;
    }
    return 1;
}


// Xbox 0x00132800. Opcode 5 in the table at 0x00314330. The word at
// record+0xC is shifted left by 2. Those dwords of DAT_005172F4 and
// DAT_0051696C are added and compared with the int at record+8. A smaller
// sum returns 0. Otherwise the float at record+0x10 is ucomiss'd with the
// 0 at 0x0032B100. Equal, and not a NaN, returns 1. Every other path
// returns 0.
// Xbox 0x00132800
int script_condition_pair_count(const ScriptCond& rec)
{
    const u16 index = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    const u32 scaled = u32(index) * 4u;
    const i32 sum = script_statistic_at_address(0x005172F4u + scaled) + script_statistic_at_address(0x0051696Cu + scaled);
    if (static_cast<i32>(cond_u32(rec, 8)) <= sum && cond_f32(rec, 0x10) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132840. Opcode 6 in the table at 0x00314330. DAT_00511B40 null,
// or equal to the sentinel 0x93C, returns 0. The word at node+0x20 + 0x94
// must be nonzero. 0x000412A0 then writes that actor's world position.
// 0x0005BEC0 takes the word at record+8, that position, and the address of
// the stack slot that held the record. A nonzero return comiss the float
// at record+0xC with the 0 at 0x0032B100. Below or equal, including
// unordered, replaces it with the 50 at 0x0032B1E0. A second comiss returns
// 1 only when that limit is above the float 0x0005BEC0 stored. 0x0005BEC0
// is not translated, so its nonzero return is not taken.
// Xbox 0x00132840
int script_condition_actor_limit(const ScriptCond&)
{
    if (g_actor_mark != nullptr && reinterpret_cast<std::uintptr_t>(g_actor_mark) != 0x93cu &&
        g_actor_mark->at_20->word_94 != 0)
    {
        const Vec3 world = actor_world_position(*g_actor_mark->actor);
        (void)world;
    }
    return 0;
}


// Xbox 0x001328D0. Opcode 7 in the table at 0x00314330. Walks DAT_0050E4DC
// until a node whose dword at +8 is 0xB4. An empty list returns 0. The
// address of that dword is tested against 0. A zero word at record+8, or a
// word four bytes after that dword matching the word at record+8, passes
// the node (that address minus 8) to 0x000E9210 and returns 1. A miss takes
// next, and returns 0 when that pointer is 0. Otherwise it walks next until
// a node has the same dword at +8, returning 0 if the walk ends. The
// address of that dword is tested again. Every other path returns 0.
// Xbox 0x001328D0
int script_condition_consume_tagged_event(const ScriptCond& rec)
{
    LiveScript* node = g_live_scripts;
    for (;;)
    {
        if (node == nullptr)
        {
            return 0;
        }
        if (node->type == 0xb4u)
        {
            break;
        }
        node = node->next;
    }
    if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    do
    {
        if (want == 0 || node->word_c == want)
        {
            script_release_event(node);
            return 1;
        }
        LiveScript* cursor = node->next;
        const u32 saved = node->type;
        if (cursor == nullptr)
        {
            return 0;
        }
        while (cursor->type != saved)
        {
            cursor = cursor->next;
            if (cursor == nullptr)
            {
                return 0;
            }
        }
        node = cursor;
    } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
    return 0;
}


// Xbox 0x00132940. Opcode 8 in the table at 0x00314330. The dword at
// record+8 is loaded first. DAT_00511B40 null, or equal to the sentinel
// 0x93C, returns 0. The word at node+0x20 + 0x94 must be nonzero.
// 0x000412A0 then writes that actor's world position. 0x00124860 takes the
// dword at record+8 and its return is this function's return. 0x00124860
// is not translated, so that return is not taken.
// Xbox 0x00132940
int script_condition_actor_query(const ScriptCond& rec)
{
    const u32 region = cond_u32(rec, 8);
    if (g_actor_mark != nullptr && reinterpret_cast<std::uintptr_t>(g_actor_mark) != 0x93cu &&
        g_actor_mark->at_20->word_94 != 0)
    {
        const Vec3 world = actor_world_position(*g_actor_mark->actor);
        (void)world;
        (void)region;
    }
    return 0;
}


// Xbox 0x001329A0. Opcode 9 in the table at 0x00314330. The dword at
// record+8 is loaded first. DAT_00511B40 null, or equal to the sentinel
// 0x93C, returns 1. Otherwise 0x000412A0 writes that actor's world position
// and 0x00124860 takes the dword at record+8. A zero return from that call
// returns 1. 0x00124860 is not translated, so that return is not taken.
// Xbox 0x001329A0
int script_condition_actor_query_clear(const ScriptCond& rec)
{
    const u32 region = cond_u32(rec, 8);
    if (g_actor_mark != nullptr && reinterpret_cast<std::uintptr_t>(g_actor_mark) != 0x93cu)
    {
        const Vec3 world = actor_world_position(*g_actor_mark->actor);
        (void)world;
        (void)region;
        return 0;
    }
    return 1;
}


// Xbox 0x00129240. EAX starts at 0. DAT_00511B48 null returns 0. A zero DX
// skips the class compare. A nonzero DX requires the word at node-0x93E
// (object+6, the actor class key) to match. The word at [node+0x18]+0x94
// nonzero adds one. Next is the pointer at node+4.
// Xbox 0x00129240
int script_count_actors_by_class(u16 key)
{
    int count = 0;
    for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
    {
        if (key != 0 && link->actor->class_key != key)
        {
            continue;
        }
        if (link->at_18->word_94 != 0)
        {
            count = count + 1;
        }
    }
    return count;
}


// Xbox 0x00132A00. Opcode 11 in the table at 0x00314330. The word at
// record+0xC is DX for 0x00129240. That count at or below the int at
// record+8, and a ucomiss of the float at record+0x10 against the 0 at
// 0x0032B100 that is equal and not a NaN, returns 1. Every other path
// returns 0.
// Xbox 0x00132A00
int script_condition_class_count(const ScriptCond& rec)
{
    const u16 key = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    const int count = script_count_actors_by_class(key);
    if (count <= static_cast<i32>(cond_u32(rec, 8)) && cond_f32(rec, 0x10) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132A30. Opcode 10 in the table at 0x00314330. The word at
// record+0xC is shifted left by 2. That dword of DAT_00512B78 is compared
// with the int at record+8. A smaller count returns 0. Otherwise the float
// at record+0x10 is ucomiss'd with the 0 at 0x0032B100. Equal, and not a
// NaN, returns 1. Every other path returns 0.
// Xbox 0x00132A30
int script_condition_category_count(const ScriptCond& rec)
{
    const u16 index = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    const u32 scaled = u32(index) * 4u;
    const i32 count = script_statistic_at_address(0x00512B78u + scaled);
    if (static_cast<i32>(cond_u32(rec, 8)) <= count && cond_f32(rec, 0x10) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132A60. Opcode 12 in the table at 0x00314330. A nonzero word at
// record+8 walks DAT_00511AA4. Next is the pointer at node+4. The short at
// node-0x704 (object+0x1FC) matching that word returns 0. Otherwise the
// float at record+0xC is ucomiss'd with the 0 at 0x0032B100. Equal, and
// not a NaN, returns 1. Every other path returns 0.
// Xbox 0x00132A60
int script_condition_actor_absent(const ScriptCond& rec)
{
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    if (want != 0)
    {
        for (PulseNode* node = g_pulse_nodes; node != nullptr; node = node->next)
        {
            if (node->word_1fc == want)
            {
                return 0;
            }
        }
    }
    if (cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132AB0. Opcode 17 in the table at 0x00314330. A nonzero word at
// record+8 walks DAT_00511AA4. Next is the pointer at node+4. The short at
// node-0x704 (object+0x1FC) matching that word selects the object at
// node-0x900. A zero word or a miss leaves the object null. vtable +0xC is
// called with that object and a two-dword message, 0x20 then 0. The callee
// writes the second dword. A zero dword at record+0x14 stores 1 there and
// stores that second dword at record+0x18. A zero at record+0x10, or a
// dword at record+0x18 other than 1, and a nonzero second dword, and a
// ucomiss of the float at record+0xC with the 0 at 0x0032B100 that is equal
// and not a NaN, returns 1. Every other path returns 0.
// Xbox 0x00132AB0
int script_condition_actor_message_latch(ScriptCond& rec)
{
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    PulseNode* obj = nullptr;
    if (want != 0)
    {
        for (PulseNode* node = g_pulse_nodes; node != nullptr; node = node->next)
        {
            if (node->word_1fc == want)
            {
                obj = node;
                break;
            }
        }
    }
    i32 message[2];
    message[0] = 0x20;
    message[1] = 0;
    reinterpret_cast<void (*)(PulseNode*, i32*)>(obj->on_message)(obj, message);
    if (static_cast<i32>(cond_u32(rec, 0x14)) == 0)
    {
        cond_put_u32(rec, 0x14, 1u);
        cond_put_u32(rec, 0x18, static_cast<u32>(message[1]));
    }
    if ((static_cast<i32>(cond_u32(rec, 0x10)) == 0 || static_cast<i32>(cond_u32(rec, 0x18)) != 1) &&
        message[1] != 0 && cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132B50. Opcode 26 in the table at 0x00314330. A nonzero word at
// record+8 walks DAT_00511AA4. Next is the pointer at node+4. The short at
// node-0x704 (object+0x1FC) matching that word selects the object at
// node-0x900. A zero word or a miss leaves the object null. vtable +0xC is
// called with that object and a two-dword message, 0x20 then 0. The callee
// writes the second dword. That dword equal to 0 returns 1. Any other value
// returns 0.
// Xbox 0x00132B50
int script_condition_actor_message(const ScriptCond& rec)
{
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    PulseNode* obj = nullptr;
    if (want != 0)
    {
        for (PulseNode* node = g_pulse_nodes; node != nullptr; node = node->next)
        {
            if (node->word_1fc == want)
            {
                obj = node;
                break;
            }
        }
    }
    i32 message[2];
    message[0] = 0x20;
    message[1] = 0;
    reinterpret_cast<void (*)(PulseNode*, i32*)>(obj->on_message)(obj, message);
    if (message[1] == 0)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x001168F0. EAX starts at DAT_0050E4DC. An empty list returns 0.
// Walks next at +4 until a node whose dword at +8 is 0xB6. The address of
// that dword is tested against 0. DX at or below 1 returns 1. Otherwise the
// word at that address +0xC (node+0x14) is compared with DX. A match returns
// 1. A miss takes next, and returns 0 when that pointer is 0. Otherwise it
// walks next until a node has the same dword at +8, returning 0 if the walk
// ends. The address of that dword is tested again. Every other path returns 0.
// Xbox 0x001168F0
int script_has_tagged_event(u16 key)
{
    LiveScript* node = g_live_scripts;
    for (;;)
    {
        if (node == nullptr)
        {
            return 0;
        }
        if (node->type == 0xb6u)
        {
            break;
        }
        node = node->next;
    }
    if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    if (key <= 1)
    {
        return 1;
    }
    do
    {
        if (node->word_14 == key)
        {
            return 1;
        }
        LiveScript* cursor = node->next;
        const u32 saved = node->type;
        if (cursor == nullptr)
        {
            return 0;
        }
        while (cursor->type != saved)
        {
            cursor = cursor->next;
            if (cursor == nullptr)
            {
                return 0;
            }
        }
        node = cursor;
    } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
    return 0;
}


// Xbox 0x00132BC0. Opcode 13 in the table at 0x00314330. The word at
// record+8 is DX for 0x001168F0. A nonzero return ucomiss the float at
// record+0xC with the 0 at 0x0032B100. Equal, and not a NaN, returns 1.
// Every other path returns 0.
// Xbox 0x00132BC0
int script_condition_tagged_event(const ScriptCond& rec)
{
    const u16 key = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    if (script_has_tagged_event(key) != 0 && cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132BF0. Opcode 14 in the table at 0x00314330. The word at
// record+8 at or below 1 sums the dwords at DAT_0051696C, DAT_005172F4,
// DAT_0051AD18, and DAT_0051A390. A larger word is shifted left by 2 and
// those four dwords are read at that offset. A nonzero sum ucomiss the
// float at record+0xC with the 0 at 0x0032B100. Equal, and not a NaN,
// returns 1. Every other path returns 0.
// Xbox 0x00132BF0
int script_condition_total_count(const ScriptCond& rec)
{
    const u16 index = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    i32 sum = 0;
    if (index <= 1)
    {
        sum = script_statistic_at_address(0x0051696Cu) + script_statistic_at_address(0x005172F4u) + script_statistic_at_address(0x0051AD18u) +
              script_statistic_at_address(0x0051A390u);
    }
    else
    {
        const u32 scaled = u32(index) * 4u;
        sum = script_statistic_at_address(0x0051AD18u + scaled) + script_statistic_at_address(0x0051A390u + scaled) +
              script_statistic_at_address(0x005172F4u + scaled) + script_statistic_at_address(0x0051696Cu + scaled);
    }
    if (sum != 0 && cond_f32(rec, 0xc) == 0.f)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132C70. Opcode 16 in the table at 0x00314330. DAT_00511B38 null
// returns 0. Next is the pointer at node+4. The short at node-0x92e
// (object+6) must match the word at record+8. The pointer at node+0x4c
// (object+0x980) null falls through to the float test. A ushort at span+6
// equal to the ushort at span+2 yields a null element, and any other pair
// is data + index * stride. A nonzero first short, and a zero dword at
// node+0x50, returns 0 when the dword at node+0x58 is 0 and skips the float
// test when it is not. A null element is still loaded, which faults on Xbox.
// Every other matching node ucomiss the float at record+0xC with the 0 at
// 0x0032B100. Equal, and not a NaN, stores 1. The walk returns that value.
// Xbox 0x00132C70
int script_condition_actor_state(const ScriptCond& rec)
{
    int result = 0;
    if (g_range_links == nullptr)
    {
        return 0;
    }
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
    {
        CamFocus* obj = link->host;
        if (obj->tag_6 != want)
        {
            continue;
        }
        EntityMaskSpan* span = obj->span_980;
        if (span != nullptr)
        {
            const u16* elem;
            if (span->end == span->mark)
            {
                elem = nullptr;
            }
            else
            {
                const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(span->data);
                const std::uintptr_t at = base + std::uintptr_t(span->index) * std::uintptr_t(span->stride);
                elem = reinterpret_cast<const u16*>(at);
            }
            if (*elem != 0 && obj->at_984 == 0)
            {
                if (obj->at_98c == 0)
                {
                    return 0;
                }
                continue;
            }
        }
        if (cond_f32(rec, 0xc) == 0.f)
        {
            result = 1;
        }
    }
    return result;
}


// Xbox 0x00132D00. Opcode 15 in the table at 0x00314330. The word at
// record+8 is kept when 1 is below it, and cleared otherwise. Walks
// DAT_0050E4DC until a node whose dword at +8 is 0xB7. An empty list returns
// 0. The address of that dword is tested against 0. A zero kept word, or a
// word four bytes after that dword matching the kept word, reads the int at
// record+0xC. Zero, or any value other than 1 or 2, takes the float test.
// 1 copies the dword at that address +8 (node+0x10). 2 stores whether that
// dword is zero. A nonzero copy takes the float test. A miss takes next, and
// joins the end test when that pointer is 0. Otherwise it walks next until a
// node has the same dword at +8, joining the end test if the walk ends. The
// address of that dword is tested again. The end test takes the float path
// when the copied value is still nonzero. The float at record+0x10 is
// ucomiss'd with the 0 at 0x0032B100. Equal, and not a NaN, returns 1.
// Every other path returns 0.
// Xbox 0x00132D00
int script_condition_state_event(const ScriptCond& rec)
{
    u32 state = 0;
    const u16 raw = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    const u16 key = 1 < raw ? raw : u16(0);
    LiveScript* node = g_live_scripts;
    for (;;)
    {
        if (node == nullptr)
        {
            return 0;
        }
        if (node->type == 0xb7u)
        {
            break;
        }
        node = node->next;
    }
    if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) == 0)
    {
        return 0;
    }
    int matched = 0;
    do
    {
        if (key == 0 || node->word_c == key)
        {
            const i32 mode = static_cast<i32>(cond_u32(rec, 0xc));
            if (mode == 0)
            {
                matched = 1;
                break;
            }
            if (mode == 1)
            {
                state = node->dword_10;
            }
            else
            {
                if (mode != 2)
                {
                    matched = 1;
                    break;
                }
                state = node->dword_10 == 0 ? 1u : 0u;
            }
            if (state != 0)
            {
                matched = 1;
                break;
            }
        }
        LiveScript* cursor = node->next;
        const u32 saved = node->type;
        if (cursor == nullptr)
        {
            break;
        }
        while (cursor->type != saved)
        {
            cursor = cursor->next;
            if (cursor == nullptr)
            {
                break;
            }
        }
        if (cursor == nullptr)
        {
            break;
        }
        node = cursor;
    } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
    if (matched != 0 || state != 0)
    {
        if (cond_f32(rec, 0x10) == 0.f)
        {
            return 1;
        }
    }
    return 0;
}


// Xbox 0x00132DB0. Opcode 18 in the table at 0x00314330. DAT_00511B38 null
// skips the walk. Next is the pointer at node+4. The short at node-0x92e
// (object+6) matching the word at record+8, and a nonzero word at
// [node+0x28]+0x94, returns 0. Otherwise the float at record+0xC is
// ucomiss'd with the 0 at 0x0032B100. Equal, and not a NaN, returns 1.
// Every other path returns 0.
// Xbox 0x00132DB0
int script_condition_actor_energy(const ScriptCond& rec)
{
    if (g_range_links != nullptr)
    {
        const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
        for (RangeLink* link = g_range_links; link != nullptr; link = link->next)
        {
            if (link->host->tag_6 == want && link->at_28->word_94 != 0)
            {
                return 0;
            }
        }
    }
    if (cond_f32(rec, 0xc) != 0.f)
    {
        return 0;
    }
    return 1;
}


// Xbox 0x00132E00. Opcode 19 in the table at 0x00314330. A zero word at
// record+0xC calls 0x0012AE40 with ECX cleared. Any other word loads the
// dword at DAT_00512B78 indexed by that word. While the int at record+0x18
// is below that count, it is incremented. A zero remainder from the signed
// division by the int at record+8 copies the dword at record+0x10 onto
// record+0x14 when the int at record+0x1C is zero, then increments that
// int. A nonzero int at record+0x1C whose float at record+0x14 ucomiss
// equals the 0 at 0x0032B100, and is not a NaN, decrements the int. A
// remaining nonzero value copies record+0x10 onto record+0x14 again and
// returns 2. Every other path returns 0.
// Xbox 0x00132E00
int script_condition_statistics_target(ScriptCond& rec)
{
    i32 count = 0;
    const u16 category = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    if (category == 0)
    {
        count = script_sum_statistics(0);
    }
    else
    {
        count = script_statistic_at_address(0x00512B78u + u32(category) * 4u);
    }
    if (static_cast<i32>(cond_u32(rec, 0x18)) < count)
    {
        do
        {
            const i32 next = static_cast<i32>(cond_u32(rec, 0x18)) + 1;
            cond_put_u32(rec, 0x18, static_cast<u32>(next));
            if (next % static_cast<i32>(cond_u32(rec, 8)) == 0)
            {
                const i32 pending = static_cast<i32>(cond_u32(rec, 0x1c));
                if (pending == 0)
                {
                    cond_put_u32(rec, 0x14, cond_u32(rec, 0x10));
                }
                cond_put_u32(rec, 0x1c, static_cast<u32>(pending + 1));
            }
        } while (static_cast<i32>(cond_u32(rec, 0x18)) < count);
    }
    if (static_cast<i32>(cond_u32(rec, 0x1c)) != 0 && cond_f32(rec, 0x14) == 0.f)
    {
        const i32 left = static_cast<i32>(cond_u32(rec, 0x1c)) - 1;
        cond_put_u32(rec, 0x1c, static_cast<u32>(left));
        if (left != 0)
        {
            cond_put_u32(rec, 0x14, cond_u32(rec, 0x10));
        }
        return 2;
    }
    return 0;
}


// DAT_00510570. +0 is followed until null. +4 of that last node is the chain
// counted below. A null +0 leaves the node as this head, so +4 is DAT_00510574.
struct AidChain
{
    AidChain* at_0 = nullptr;
    AidChain* at_4 = nullptr;
};

AidChain g_aid_chain{};


// Xbox 0x00132E90. Opcode 20 in the table at 0x00314330. The record is not
// read. Walks the pointer at DAT_00510570 through the dword at +0 until null,
// keeping the previous node (the address of DAT_00510570 when that pointer
// is null). The dword at +4 of that node is then walked through +4, counting
// nodes. A nonzero count returns 0. An empty walk returns 1.
// Xbox 0x00132E90
int script_condition_consume_pending_nodes()
{
    AidChain* tail = &g_aid_chain;
    for (AidChain* node = g_aid_chain.at_0; node != nullptr; node = node->at_0)
    {
        tail = node;
    }
    AidChain* chain = tail->at_4;
    i32 count = 0;
    if (chain != nullptr)
    {
        do
        {
            chain = chain->at_4;
            count = count + 1;
        } while (chain != nullptr);
        if (count != 0)
        {
            return 0;
        }
    }
    return 1;
}

u8 g_scene_flag_bytes[2]{};

// 0x000F2B60 at boot: kind 9, two bytes.
SceneFlagBlock g_scene_flag_block{9, 2, g_scene_flag_bytes};

SceneFlagBlock* g_scene_flags = &g_scene_flag_block;


extern u32 g_flags_ea9c;
 // DAT_0050EA9C

// Xbox 0x00132ED0. Opcode 21 in the table at 0x00314330. The record is not
// read. Bit 0x8 of the byte at [DAT_00506c74+8], or bit 0x200000 of
// DAT_0050EA9C, returns 1. Both clear returns 0.
// Xbox 0x00132ED0
int script_condition_scene_flag_set()
{
    if ((*g_scene_flags->bytes & 8) == 0 && (g_flags_ea9c & 0x200000u) == 0)
    {
        return 0;
    }
    return 1;
}


// Xbox 0x00132F00. Opcode 22 in the table at 0x00314330. The record is not
// read. Bit 0x8 of the byte at [DAT_00506c74+8] and bit 0x200000 of
// DAT_0050EA9C both clear returns 1. Either set returns 0.
// Xbox 0x00132F00
int script_condition_scene_flag_clear()
{
    if ((*g_scene_flags->bytes & 8) == 0 && (g_flags_ea9c & 0x200000u) == 0)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00132F30. Opcode 23 in the table at 0x00314330. Walks DAT_00511B48.
// A null node returns 0. The word at record+8 below 2, or equal to the word
// at node-0x93E (actor+6), calls 0x000412A0 with ESI = node-0x944 and EDI
// pointing at a 12-byte stack slot. 0x00124860 then takes the dword at
// record+0xC. A nonzero return returns 1. Next is the pointer at node+4.
// 0x00124860 is not translated, so its nonzero return is not taken.
// Xbox 0x00132F30
int script_condition_actor_position(const ScriptCond& rec)
{
    for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
    {
        const u16 key = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
        if (key < 2 || link->actor->class_key == key)
        {
            const Vec3 world = actor_world_position(*link->actor);
            (void)world;
            (void)cond_u32(rec, 0xc);
        }
    }
    return 0;
}


// Xbox 0x00132FA0. Opcode 24 in the table at 0x00314330. Walks DAT_00511B48.
// A null node returns 0. The word at record+8 below 2, or equal to the word
// at node-0x93E (actor+6), and the word at [node+0x18]+0x94 zero-extended
// then compared as a signed int at or below the int at record+0xC, returns 1.
// Next is the pointer at node+4. Every other path returns 0.
// Xbox 0x00132FA0
int script_condition_actor_word_limit(const ScriptCond& rec)
{
    for (AltLink* link = g_alt_links; link != nullptr; link = link->next)
    {
        const u16 key = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
        if ((key < 2 || link->actor->class_key == key) &&
            static_cast<i32>(u32(link->at_18->word_94)) <= static_cast<i32>(cond_u32(rec, 0xc)))
        {
            return 1;
        }
    }
    return 0;
}


// Xbox 0x00132FF0. Opcode 25 in the table at 0x00314330. Walks DAT_0050E4DC
// until a node whose dword at +8 is 0xB9. An empty list returns whether the
// int at record+0xC is at or below 0. A null next before that type is found
// returns whether that int is below 1. The address of the type dword is
// tested against 0. The word at record+8 below 2, or equal to the word four
// bytes after that dword, adds one. Next is the pointer four bytes before
// the type dword. A null next joins the end test. Otherwise it walks next
// until a node has the same dword at +8, returning the end test if the walk
// ends. The address of that dword is tested again. The end test returns
// whether the int at record+0xC is at or below the count.
// Xbox 0x00132FF0
int script_condition_event_count(const ScriptCond& rec)
{
    int count = 0;
    LiveScript* node = g_live_scripts;
    if (node != nullptr)
    {
        while (node->type != 0xb9u)
        {
            node = node->next;
            if (node == nullptr)
            {
                if (static_cast<i32>(cond_u32(rec, 0xc)) < 1)
                {
                    return 1;
                }
                return 0;
            }
        }
        if (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0)
        {
            const u16 key = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
            do
            {
                if (key < 2 || node->word_c == key)
                {
                    count = count + 1;
                }
                LiveScript* cursor = node->next;
                const u32 saved = node->type;
                if (cursor == nullptr)
                {
                    break;
                }
                while (cursor->type != saved)
                {
                    cursor = cursor->next;
                    if (cursor == nullptr)
                    {
                        if (static_cast<i32>(cond_u32(rec, 0xc)) <= count)
                        {
                            return 1;
                        }
                        return 0;
                    }
                }
                node = cursor;
            } while (reinterpret_cast<std::uintptr_t>(node) + offsetof(LiveScript, type) != 0);
        }
    }
    if (static_cast<i32>(cond_u32(rec, 0xc)) <= count)
    {
        return 1;
    }
    return 0;
}


// Xbox 0x00124BE0. A zero keeps the record. Any other value queues the link
// at +4. A return of exactly 1 also removes the record and the slot that
// slides down is evaluated next. Opcode 0 is 0x00132650. Opcode 1 is
// 0x00132680. Opcode 2 is 0x001326D0. Opcode 3 is 0x00132720. Opcode 4 is
// 0x001327A0. Opcode 5 is 0x00132800. Opcode 6 is 0x00132840. Opcode 7 is
// 0x001328D0. Opcode 8 is 0x00132940. Opcode 9 is 0x001329A0. Opcode 10 is
// 0x00132A30. Opcode 11 is 0x00132A00. Opcode 12 is 0x00132A60. Opcode 13 is
// 0x00132BC0. Opcode 14 is 0x00132BF0. Opcode 15 is 0x00132D00. Opcode 16 is
// 0x00132C70. Opcode 17 is 0x00132AB0. Opcode 18 is 0x00132DB0. Opcode 19 is
// 0x00132E00. Opcode 20 is 0x00132E90. Opcode 21 is 0x00132ED0. Opcode 22 is
// 0x00132F00. Opcode 23 is 0x00132F30. Opcode 24 is 0x00132FA0. Opcode 25 is
// 0x00132FF0. Opcode 26 is 0x00132B50.
// Xbox 0x00124BE0
void script_evaluate_conditions(Game& g)
{
    size_t i = 0;
    while (i < g_script_conds.size())
    {
        ScriptCond& rec = g_script_conds[i];
        const u32 opcode = cond_u32(rec, 0);
        int fired = 0;
        if (opcode == 0)
        {
            fired = script_condition_timer_expired(rec);
        }
        else if (opcode == 1)
        {
            fired = script_condition_actor_count_at_most(rec);
        }
        else if (opcode == 2)
        {
            fired = script_condition_consume_ready_events(rec);
        }
        else if (opcode == 3)
        {
            fired = script_condition_consume_matching_event(rec);
        }
        else if (opcode == 4)
        {
            fired = script_condition_statistics_latch(rec);
        }
        else if (opcode == 5)
        {
            fired = script_condition_pair_count(rec);
        }
        else if (opcode == 6)
        {
            fired = script_condition_actor_limit(rec);
        }
        else if (opcode == 7)
        {
            fired = script_condition_consume_tagged_event(rec);
        }
        else if (opcode == 8)
        {
            fired = script_condition_actor_query(rec);
        }
        else if (opcode == 9)
        {
            fired = script_condition_actor_query_clear(rec);
        }
        else if (opcode == 10)
        {
            fired = script_condition_category_count(rec);
        }
        else if (opcode == 11)
        {
            fired = script_condition_class_count(rec);
        }
        else if (opcode == 12)
        {
            fired = script_condition_actor_absent(rec);
        }
        else if (opcode == 13)
        {
            fired = script_condition_tagged_event(rec);
        }
        else if (opcode == 14)
        {
            fired = script_condition_total_count(rec);
        }
        else if (opcode == 15)
        {
            fired = script_condition_state_event(rec);
        }
        else if (opcode == 16)
        {
            fired = script_condition_actor_state(rec);
        }
        else if (opcode == 17)
        {
            fired = script_condition_actor_message_latch(rec);
        }
        else if (opcode == 18)
        {
            fired = script_condition_actor_energy(rec);
        }
        else if (opcode == 19)
        {
            fired = script_condition_statistics_target(rec);
        }
        else if (opcode == 20)
        {
            fired = script_condition_consume_pending_nodes();
        }
        else if (opcode == 21)
        {
            fired = script_condition_scene_flag_set();
        }
        else if (opcode == 22)
        {
            fired = script_condition_scene_flag_clear();
        }
        else if (opcode == 23)
        {
            fired = script_condition_actor_position(rec);
        }
        else if (opcode == 24)
        {
            fired = script_condition_actor_word_limit(rec);
        }
        else if (opcode == 25)
        {
            fired = script_condition_event_count(rec);
        }
        else if (opcode == 26)
        {
            fired = script_condition_actor_message(rec);
        }
        if (fired == 0)
        {
            ++i;
            continue;
        }
        g_script_fired.push_back(cond_u32(rec, 4));
        if (fired == 1)
        {
            g_script_conds.erase(g_script_conds.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
    (void)g;
}


u8 node_u8(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel >= node.size || node.offset + rel >= script.bytes.size())
    {
        return 0;
    }
    return script.bytes[node.offset + rel];
}


u16 node_u16(const SceneScript& script, const ScriptNode& node, u32 rel)
{
    if (rel + 2 > node.size || node.offset + rel + 2 > script.bytes.size())
    {
        return 0;
    }
    return read_u16(script.bytes, node.offset + rel);
}

} // namespace gbtg
