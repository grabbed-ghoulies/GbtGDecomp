#include "gbtg/anim.h"
#include "../entities/body_fields.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/model.h"
#include "gbtg/log.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../scene/factory_scene_runtime.h"
#include "../scene/lighting.h"
#include "../scene/scene_loading.h"
#include "../scene/actor_loading.h"
#include "../runtime/name_lookup.h"
#include "../camera/actor_camera_effects.h"
#include "../saves/runtime_profile.h"
#include "script_event_sequence.h"
#include "script_event_tallies.h"

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
#include "script_actions.h"
#include "script_conditions.h"
#include "script_vm.h"
#include "../scene/scene_pause.h"
#include "../entities/entity.h"
#include "../transitions/scene_transitions.h"

namespace gbtg
{
using namespace scene_detail;


// DAT_00512514. 0x00128AB0 stores 1 when the dword it kept at DAT_0051248C is 0.
// Type 0xF reads it. script_set_name does not write this dword.
i32 g_gate_2514 = 0;

// DAT_00512518. Type 0xF stores 0, then 1 when DAT_0051251C is already 0.
i32 g_gate_2518 = 0;

// DAT_0051251C. Types 0xF and 0x10 decrement it. Type 0x11 increments it.
i32 g_gate_251c = 0;

// DAT_0050E798 and DAT_0050E79C. Type 0x69 copies node+8 and node+0xC.
u32 g_script_e798 = 0;

u32 g_script_e79c = 0;

// DAT_0050E788. 0x00117140 sets it when 0x001288C0 empties a nonempty fired list.
i32 g_script_e788 = 0;

// DAT_0050E76C..DAT_0050E784. 0x00116E00 clears the flag.
Vec3 g_scene_entry_position{};
Vec3 g_scene_entry_euler{};
i32 g_scene_entry_set = 0;

// Xbox 0x00117580
void store_scene_entry_pose(const Vec3& position, const Vec3& euler)
{
    g_scene_entry_position = position;
    g_scene_entry_euler = euler;
    g_scene_entry_set = 1;
}

// DAT_0050E78C. Nonzero takes the fade at DAT_0050E790. 0x00116E00 clears it.
i32 g_script_e78c = 0;

// DAT_0050E790. The fade subtracts DAT_005096C0 * DAT_004312C4 while this is above 0.
float g_script_e790 = 0.f;

// DAT_0050E794. Nonzero skips the condition walk. The latch path increments it.
i32 g_script_e794 = 0;

bool g_scene_script_started = false;

extern i32 g_script_30d0c;

void request_scene_aid(const char* src, i32 mode);


// Xbox 0x000F29A0
bool test_runtime_story_bit(std::span<const u8> bits, u32 index)
{
    return (bits[index >> 3] & u8(1u << (index & 7))) != 0;
}

// Xbox 0x000F29D0
void set_runtime_story_bit(std::span<u8> bits, u32 index)
{
    bits[index >> 3] |= u8(1u << (index & 7));
}

// Xbox 0x000F29F0
void clear_runtime_story_bit(std::span<u8> bits, u32 index)
{
    bits[index >> 3] &= u8(~(1u << (index & 7)));
}

// Xbox 0x000F2B30
bool test_runtime_unlock_bit(std::span<const u8> bits, u32 index)
{
    return (bits[index >> 3] & u8(1u << (index & 7))) != 0;
}

// Xbox 0x000F2A20
bool test_and_set_runtime_story_bit(std::span<u8> bits, u32 index)
{
    const u8 mask = u8(1u << (index & 7));
    if ((bits[index >> 3] & mask) != 0)
    {
        return false;
    }
    bits[index >> 3] |= mask;
    return true;
}

// Xbox 0x00049D10.
bool scene_end_flag_set()
{
    return (*g_scene_flags->bytes & 8) != 0 || (g_flags_ea9c & 0x200000u) != 0;
}


// Xbox 0x00123B00.
bool script_end_gate_set()
{
    return g_script_30d0c == 1 || scene_end_flag_set();
}


u32 g_broken_background_parts = 0; // DAT_00545978.

u32 g_running_score = 0; // DAT_00545980.

// Xbox 0x0012AF10. Seven counters, each followed by 60 counters and a 60-by-61 matrix: the statistics block passed
// in EAX.
void script_initialize_statistics(std::span<u32> statistics)
{
    for (int group = 0; group < 7; ++group)
    {
        const size_t base = size_t(group) * 0xe89;
        for (int row = 0; row < 60; ++row)
        {
            const size_t at = base + 61 + size_t(row) * 61;
            std::fill_n(statistics.begin() + at + 1, 60, 0u);
            statistics[at] = 0;
        }
        std::fill_n(statistics.begin() + base + 1, 60, 0u);
        statistics[base] = 0;
    }
    g_broken_background_parts = 0;
}


// Xbox 0x00128AB0.
void script_activate_title(Game& g, std::string_view title, i32 mode, i32 id)
{
    if (mode != g_state_248c || id != g_state_2490)
    {
        constexpr std::array<u8, 4> decay{1, 0, 1, 0}; // DAT_0030F144
        const u8 old_decay = decay.at(size_t(g_state_248c));
        const u8 new_decay = decay.at(size_t(mode));
        g_state_248c = mode;
        g_state_2490 = id;
        g.scene_name = title;
        g_gate_2514 = mode == 0;
        g_gate_2518 = 0;
        g_gate_251c = 0;
        g_script_conds.clear();
        for (u32 type : {0xb7u, 0xb6u})
        {
            for (LiveScript* live = g_live_scripts; live != nullptr;)
            {
                LiveScript* next = live->next;
                if (live->type == type)
                {
                    script_release_event(live);
                }
                live = next;
            }
        }
        if (g_event_sequence_state != 3) // DAT_005064D8
        {
            script_initialize_statistics(g_script_tallies.second); // EAX DAT_00512B78
            g_running_score = 0;
        }
        g_decay_arm = new_decay;
        if (old_decay != new_decay)
        {
            script_update_decay_latches(0xffff, new_decay);
        }
    }
}


// Xbox 0x001289A0
void release_scene_script_title(Game& g)
{
    script_activate_title(g, kNoScript, 0, 0); // 0x00128AB0, EAX 0x0032A910
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("0x001289A0 has no scene services for the title pool (DAT_00512484)");
    }
    g.factory_scene->actor_services->free_script_title_pool(); // DAT_00512484, 0x0005F460
    end_cutscene_skip_input(g); // 0x00127B50
    const char16_t* source = u"No Name Available"; // 0x0032A558
    size_t index = 0;
    do
    {
        g_scene_pause.title[index] = source[index]; // lstrcpyW into DAT_00510D00
    }
    while (source[index++] != 0);
    std::vector<u32>().swap(g_script_fired); // DAT_00512488, 0x000663E0
}


// Xbox 0x00128BC0. Handler 0 of the table at 0x00312650. The dword at node+4
// is the type. The byte map at 0x00128D24 covers types 0 through 0x69. Slot 7
// of the table at 0x00128D04 returns 3, and every type in that range other
// than 0, 0xD, 0xE, 0xF, 0x10, 0x11, and 0x69 uses it. A type above 0x69
// returns 3. The second argument is the flag 0x00128A00 passes. The third
// argument is the value that walk passes.
//
// Type 0 returns 1.
//
// Type 0xD calls 0x00053170 with the address of node+8 and a stack 0, the
// effect stream by that name with no creation options. The case returns 2.
//
// Type 0xE calls 0x00123B00. The dword at node+0x90 equal to 1 with a
// nonzero return, equal to 2 with a zero return, or a nonzero dword at
// node+0x94 with a nonzero 0x00049D10, calls 0x000532B0 with ECX the node
// cursor and EDX 0xF (the cursor moves to the next type 0xF) and returns 2.
// Any other node+0x90 calls 0x00128AB0. EAX is the cstring at node+0x10, ECX
// is the dword at node+8, and EDX is the dword at node+0xC. A nonzero third
// argument with a zero dword at node+8 returns 0. A zero second argument and
// a zero third argument return 0. Otherwise the case returns 2.
//
// Type 0xF stores 0 at DAT_00512518. A zero DAT_00512514 returns 2. A zero
// DAT_0051251C stores 1 at DAT_00512518 and returns 0. Any other count
// decrements DAT_0051251C and returns 2.
//
// Type 0x10 with a nonzero third argument returns 2. A zero third argument
// returns 0 when DAT_0051251C is 0, and otherwise decrements it and returns 2.
//
// Type 0x11 with a zero third argument increments DAT_0051251C and returns 2.
// A nonzero third argument returns 2.
//
// Type 0x69 stores the dword at node+0xC into DAT_0050E79C and the dword at
// node+8 into DAT_0050E798, then returns 2.
// Xbox 0x000532B0
bool script_seek_node_type(const SceneScript& script, u32& cursor, u32 type)
{
    for (;;)
    {
        auto node = script_node_at(script, cursor);
        if (!node)
        {
            throw Error("script search cursor outside resource");
        }
        if (node->type == type)
        {
            return true;
        }
        if (node->type == 0)
        {
            return false;
        }
        cursor = node->link;
    }
}


// Xbox 0x00128BC0
int script_handle_title_actions(Game& g, const ScriptNode& node, u32& cursor, int live_7d, int ending)
{
    switch (node.type)
    {
    case 0:
        return 1;
    case 0xd:
    {
        // 0x00053170 with node+8 and a stack 0 (no creation options).
        const std::string key = node_cstr(g.scene_script, node, 8);
        if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
        {
            throw Error("script type 0xD runs an effect stream without the boot scene services");
        }
        g.factory_scene->actor_services->run_script_effect_stream(key.c_str());
        return 2;
    }
    case 0xe:
    {
        const u32 mode = node_u32(g.scene_script, node, 0x90);
        const bool alternate = script_end_gate_set();
        if ((mode == 1 && alternate) || (mode == 2 && !alternate) ||
            (node_u32(g.scene_script, node, 0x94) != 0 && scene_end_flag_set()))
        {
            script_seek_node_type(g.scene_script, cursor, 0xf);
            return 2;
        }
        const u32 at_8 = node_u32(g.scene_script, node, 8);
        const u32 at_c = node_u32(g.scene_script, node, 0xc);
        script_activate_title(g, node_cstr(g.scene_script, node, 0x10), i32(at_8), i32(at_c));
        if (ending != 0 && at_8 == 0)
        {
            return 0;
        }
        if (live_7d == 0 && ending == 0)
        {
            return 0;
        }
        return 2;
    }
    case 0xf:
        g_gate_2518 = 0;
        if (g_gate_2514 != 0)
        {
            if (g_gate_251c == 0)
            {
                g_gate_2518 = 1;
                return 0;
            }
            g_gate_251c = g_gate_251c - 1;
            return 2;
        }
        return 2;
    case 0x10:
        if (ending == 0)
        {
            if (g_gate_251c == 0)
            {
                return 0;
            }
            g_gate_251c = g_gate_251c - 1;
            return 2;
        }
        return 2;
    case 0x11:
        if (ending == 0)
        {
            g_gate_251c = g_gate_251c + 1;
            return 2;
        }
        return 2;
    case 0x69:
        g_script_e79c = node_u32(g.scene_script, node, 0xc);
        g_script_e798 = node_u32(g.scene_script, node, 8);
        return 2;
    default:
        return 3;
    }
}


// Xbox 0x00128A00. Walks fired file offsets. Handler 0 is 0x00128BC0.
// Handler 1 is 0x00124CC0.
// Handler 2 is 0x00124E50. Handler 3 is 0x00124F30. Handler 4 is 0x00124FC0.
// Handler 5 is 0x001251E0. Handler 6 is 0x00125450. Handler 7 is 0x001259B0.
// Handler 8 is 0x00126140. Handler 9 is 0x001269D0. Handler 10 is 0x00126A10.
// Handler 11 is 0x00126E10. Handler 12 is 0x00127340. Handler 13 is 0x001274B0.
// Handler 14 is 0x00127BC0. Handler 15 is 0x00128050. Handler 16 is 0x00128770.
// Handler 0 is the first slot of the table at 0x00312650. The flag passed to handler 4 is nonzero
// when DAT_0050E4DC has a node whose dword at +8 is 0x7D. Handler 0 receives that same flag.
// Handler 7 receives that same flag.
// Its third argument is 0: 0x00128A00 clears EDI whenever that flag is set,
// and the handler only reads the third argument on that path. Handler 0
// receives that same third argument. Handler 8
// receives that same third argument. Handler 13 receives it as well. Handler 14
// receives that same second argument and that same third argument. Handler 15
// receives that same second argument and that same third argument. Handler 16
// reads only the node. A node type none of them covers advances, as in the
// retail table walk. Return 1 stops on a type-0 node from handler 0, and on a
// type-0x3C node. Return 0 means
// the cursor left the blob, or stopped on an untranslated node, and the
// drain keeps that fired id.
// Xbox 0x00128A00
int script_execute_fired_action(Game& g, u32& offset)
{
    // The handlers' two flags (Ghidra's param_2 and param_3): a type 0x7D live event is pending, and, without one,
    // the scene's end gate (0x00123B00) is set.
    int live_7d = 0;
    for (LiveScript* live = g_live_scripts; live != nullptr; live = live->next)
    {
        if (live->type == 0x7d)
        {
            live_7d = 1;
            break;
        }
    }
    const int ending = live_7d == 0 && script_end_gate_set() ? 1 : 0;
    while (true)
    {
        auto node = script_node_at(g.scene_script, offset);
        if (!node || node->size == 0)
        {
            return 0;
        }
        int handler = script_handle_title_actions(g, *node, offset, live_7d, ending);
        if (handler == 3)
        {
            handler = script_handle_actor_state(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_parameters(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_actor_messages(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_pair_updates(g, *node, live_7d);
        }
        if (handler == 3)
        {
            handler = script_handle_condition_setup(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_condition_node(g, *node, offset);
        }
        if (handler == 3)
        {
            handler = script_handle_action_links(g, *node, live_7d, ending);
        }
        if (handler == 3)
        {
            handler = script_handle_scene_actions(g, *node, live_7d, ending);
        }
        if (handler == 3)
        {
            handler = script_handle_flag_bits(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_actor_actions(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_resource_actions(g, *node, live_7d);
        }
        if (handler == 3)
        {
            handler = script_handle_event_actions(g, *node);
        }
        if (handler == 3)
        {
            handler = script_handle_cutscene_actions(g, *node, ending);
        }
        if (handler == 3)
        {
            handler = script_handle_actor_effects(g, *node, live_7d, ending);
        }
        if (handler == 3)
        {
            handler = script_handle_world_actions(g, *node, live_7d, ending);
        }
        if (handler == 3)
        {
            handler = script_handle_broadcast_actions(g, *node);
        }
        if (handler == 1)
        {
            return 1;
        }
        if (handler != 2 && handler != 3)
        {
            return 0;
        }
        auto current = script_node_at(g.scene_script, offset);
        if (!current)
        {
            throw Error("script handler cursor outside resource");
        }
        offset = current->link;
    }
}


// Xbox 0x001288C0. Walks the fired ids from the back. A zero from 0x00128A00
// stops and leaves the remaining entries in place.
// Xbox 0x001288C0
void script_execute_fired_actions(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("0x001288C0 has no boot scene services for the title pool (DAT_00512484)");
    }
    g.factory_scene->actor_services->release_script_title_pool(); // DAT_00512484 once empty (0x0005F460)
    for (size_t i = g_script_fired.size(); i > 0;)
    {
        --i;
        const int ran = script_execute_fired_action(g, g_script_fired[i]);
        if (ran == 0)
        {
            return;
        }
        g_script_fired.erase(g_script_fired.begin() + static_cast<std::ptrdiff_t>(i));
        if (g_script_e79c != 0)
        {
            break;
        }
    }
}


// Xbox 0x0004B370. Chapter scripts share the setup script for their chapter.
std::string script_chapter_setup_name(std::string_view name)
{
    constexpr std::string_view prefix = "aid_script_ghoulies_chapter";
    if (!name.starts_with(prefix))
    {
        return {};
    }
    const size_t end = name.find('_', prefix.size());
    return std::string(name.substr(0, end)) + "_setup";
}


// Xbox 0x0004B440. Run the setup resource on the same fired-script VM.
void script_run_chapter_setup(Game& g, std::string_view name)
{
    const std::string setup = script_chapter_setup_name(name);
    if (setup.empty())
    {
        return;
    }
    const BundleEntry* entry = g.aids.load_aid(setup);
    if (entry == nullptr)
    {
        throw Error("chapter setup resource missing: " + setup);
    }
    SceneScript scene = std::move(g.scene_script);
    g.scene_script = SceneScript{setup, entry->cpu};
    g_script_fired.push_back(0); // The resource's first node.
    script_execute_fired_actions(g);
    if (!g_script_fired.empty())
    {
        throw Error("chapter setup script did not drain: " + setup);
    }
    g.scene_script = std::move(scene);
}


void script_vm_bind(Game& g)
{
    script_vm_reset();
    g_scene_script_started = false;
    g_scene_entry_set = 0;
    g_script_e788 = 0;
    g_script_e78c = 0;
    g_script_e794 = 0;
    g_script_e798 = 4;
    g_script_e79c = 0;
    g_flag_2470 = 0;
    g_flag_2474 = 0;
    while (g_live_scripts != nullptr)
    {
        script_release_event(g_live_scripts);
    }
    if (g.scene_script.bytes.empty())
    {
        return;
    }
    script_run_chapter_setup(g, g.scene_script.name);
    g_script_fired.push_back(0); // The resource's first node, as 0x00117000 pushes the resource pointer.
    script_execute_fired_actions(g);
}


// Xbox 0x00133070. Opcode 0 in the update table at 0x00314334. The int at
// +0xC, or a clear DAT_0050E768 while the byte at [DAT_0051248C]+0x30F144
// is set, subtracts a step from the float at +8. DAT_004312DC above 0 makes
// that step 0. Otherwise the step is DAT_005096C0 * DAT_004312C4. A result
// at or below 0 is stored as 0.
// Xbox 0x00133070
void script_tick_condition_timer(ScriptCond& rec)
{
    if (cond_u32(rec, 0xc) != 0 || (g_block_decay == 0 && g_decay_arm != 0))
    {
        float step = 0.f;
        if (g_sim_freeze <= 0.f)
        {
            step = g_sim_dt * g_sim_scale;
        }
        const float left = cond_f32(rec, 8);
        if (0.f < left)
        {
            float next = left - step;
            cond_put_f32(rec, 8, next);
            if (next <= 0.f)
            {
                cond_put_f32(rec, 8, 0.f);
            }
        }
    }
}


// Xbox 0x00133900. Opcode 19 in the update table at 0x00314334: the count (0x0012AE40 with ECX 0 for a zero word at
// +0xC, else the DAT_00512B78 dword it indexes) is tallied as 0x00132E00 tallies it, each +8 more arming +0x1C (the
// first copying +0x10 onto +0x14); while +0x1C is armed, DAT_0050E768 is clear and the byte at
// [DAT_0051248C]+0x30F144 is set, the float at +0x14 counts down by the frame step, stopping at 0.
void script_tick_statistics_target_condition(ScriptCond& rec)
{
    const u16 category = u16(rec.raw[0xc]) | (u16(rec.raw[0xd]) << 8);
    const i32 count = category == 0 ? script_sum_statistics(0)
                                    : script_statistic_at_address(0x00512B78u + u32(category) * 4u);
    while (static_cast<i32>(cond_u32(rec, 0x18)) < count)
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
    }
    if (cond_u32(rec, 0x1c) == 0 || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0x14);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0x14, next);
        if (!(0.f < next)) // COMISS; JB keeps a positive time.
        {
            cond_put_f32(rec, 0x14, 0.f);
        }
    }
}


// Xbox 0x00133230. Opcode 4 in the update table at 0x00314334: once the statistics sum (0x0012AE40, ECX 0) reaches
// the int at +8 and +0x14 is clear, while DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144 is set, the
// float at +0xC counts down by the frame step, stopping at 0.
void script_tick_statistics_latch_condition(ScriptCond& rec)
{
    if (script_sum_statistics(0) < static_cast<i32>(cond_u32(rec, 8)) || cond_u32(rec, 0x14) != 0
        || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xC);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xC, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0xC, 0.f);
        }
    }
}

// Xbox 0x001332B0. Opcode 5 in the update table at 0x00314334: once the DAT_005172F4 and DAT_0051696C counts the
// word at +0xC indexes add up to the int at +8, while DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144
// is set, the float at +0x10 counts down by the frame step, stopping at 0.
void script_tick_pair_count_condition(ScriptCond& rec)
{
    const u32 scaled = u32(u16(cond_u32(rec, 0xC))) * 4u;
    const i32 sum = script_statistic_at_address(0x005172F4u + scaled) + script_statistic_at_address(0x0051696Cu + scaled);
    if (sum < static_cast<i32>(cond_u32(rec, 8)) || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0x10);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0x10, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0x10, 0.f);
        }
    }
}

// Xbox 0x00133330. Opcode 11 (SpecificGhouliesAlive) in the update table at 0x00314334: once no more actors of the
// class the word at +0xC names are alive (0x00129240) than the int at +8, while DAT_0050E768 is clear and the byte at
// [DAT_0051248C]+0x30F144 is set, the float at +0x10 counts down by the frame step, stopping at 0.
void script_tick_ghoulies_alive_condition(ScriptCond& rec)
{
    if (script_count_actors_by_class(u16(cond_u32(rec, 0xC))) > static_cast<i32>(cond_u32(rec, 8))
        || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0x10);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0x10, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0x10, 0.f);
        }
    }
}

// Xbox 0x00133420. Opcode 12: unless the word at +8 names an actor still on DAT_00511AA4 (its +0x1FC, the latch node
// at +0x900 less 0x704), the float at +0xC counts down by the frame step while DAT_0050E768 is clear and the byte at
// [DAT_0051248C]+0x30F144 is set, stopping at 0.
void script_tick_actor_gone_condition(ScriptCond& rec)
{
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    if (want != 0)
    {
        if (!g_script_actor_by_id)
        {
            throw Error("0x00133420 walks DAT_00511AA4 with no scene bound");
        }
        if (g_script_actor_by_id(want) != 0)
        {
            return;
        }
    }
    if (g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xC);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xC, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0xC, 0.f);
        }
    }
}

// Xbox 0x001333A0. Opcode 10 (SpecificGhouliesKilled) in the update table at 0x00314334: once the DAT_00512B78 kill
// count the word at +0xC indexes reaches the int at +8, while DAT_0050E768 is clear and the byte at
// [DAT_0051248C]+0x30F144 is set, the float at +0x10 counts down by the frame step, stopping at 0.
void script_tick_ghoulies_killed_condition(ScriptCond& rec)
{
    const u16 index = u16(cond_u32(rec, 0xC));
    if (script_statistic_at_address(0x00512B78u + u32(index) * 4u) < static_cast<i32>(cond_u32(rec, 8))
        || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0x10);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0x10, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0x10, 0.f);
        }
    }
}

// Xbox 0x001334B0. Opcode 17 (ObjectByMarkerBroken) in the update table at 0x00314334: the DAT_00511AA4 object
// whose +0x1FC is the word at +8 is asked 0x20 (broken?). While it answers yes, DAT_0050E768 is clear and the byte at
// [DAT_0051248C]+0x30F144 is set, the float at +0xC counts down by the frame step, stopping at 0.
void script_tick_marker_broken_condition(ScriptCond& rec)
{
    const u16 want = u16(cond_u32(rec, 8));
    if (!g_script_actor_by_id || !g_script_object_message)
    {
        throw Error("0x001334B0 walks DAT_00511AA4 with no scene bound");
    }
    const u32 object = want == 0 ? 0u : g_script_actor_by_id(want);
    if (object == 0)
    {
        throw Error("0x001334B0 calls vtable +0xC of a null object: no DAT_00511AA4 actor has +0x1FC "
            + std::to_string(want));
    }
    std::array<u32, 2> message{0x20, 0};
    g_script_object_message(object, message); // Vtable +0xC.
    if (message[1] == 0 || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}

// Xbox 0x001336A0. Opcode 16 in the update table at 0x00314334. Walks DAT_00511B38 for actors whose +6 tag is the
// word at +8. One whose +0x980 goal ring front is nonzero with +0x984 clear ends the walk when +0x98C is clear and is
// passed over otherwise; for every other match, while DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144
// is set, the float at +0xC counts down by the frame step, stopping at 0.
// Xbox 0x001336A0
void script_tick_ghouly_exists_condition(ScriptCond& rec)
{
    const u16 want = u16(cond_u32(rec, 8));
    for (const RuntimeLoadingActor& actor : runtime_loading_actors())
    {
        if (actor.tag != want)
        {
            continue;
        }
        if (actor.goal != 0 && actor.word_984 == 0)
        {
            if (actor.word_98c == 0)
            {
                return;
            }
            continue;
        }
        if (g_block_decay != 0 || g_decay_arm == 0)
        {
            continue;
        }
        float step = 0.f;
        if (g_sim_freeze <= 0.f)
        {
            step = g_sim_dt * g_sim_scale;
        }
        const float left = cond_f32(rec, 0xc);
        if (0.f < left)
        {
            const float next = left - step;
            cond_put_f32(rec, 0xc, next);
            if (next <= 0.f)
            {
                cond_put_f32(rec, 0xc, 0.f);
            }
        }
    }
}

// Xbox 0x00133870. Opcode 18 (scriptConditionId_ActorHasNoEnergy) in the update table at 0x00314334: while any
// DAT_00511B38 actor whose +6 tag is the word at +8 still has energy (body +0x94) nothing moves; otherwise, while
// DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144 is set, the float at +0xC counts down by the frame
// step, stopping at 0.
// Xbox 0x00133870
void script_tick_actor_energy_condition(ScriptCond& rec)
{
    const u16 want = u16(cond_u32(rec, 8));
    for (const RuntimeLoadingActor& actor : runtime_loading_actors()) // DAT_00511B38.
    {
        if (actor.tag != want)
        {
            continue;
        }
        if (!actor.body)
        {
            throw Error("0x00133870 reads +0x94 of a DAT_00511B38 actor with no body at +0x95C");
        }
        if (actor.energy != 0)
        {
            return;
        }
    }
    if (g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (next <= 0.f)
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}


// Xbox 0x00133570. Opcode 13 in the update table at 0x00314334: once 0x001168F0 finds the tagged 0xB6 event for the
// word at +8, while DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144 is set, the float at +0xC counts
// down by the frame step, stopping at 0.
// Xbox 0x00133570
void script_tick_tagged_event_condition(ScriptCond& rec)
{
    if (script_has_tagged_event(u16(cond_u32(rec, 8))) == 0 || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (next <= 0.f)
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}


// Xbox 0x001335E0. Opcode 14 in the update table at 0x00314334: once the DAT_0051696C, DAT_005172F4, DAT_0051AD18 and
// DAT_0051A390 counts (all four totals for a word at +8 at or below 1, otherwise that word's row) are nonzero, the
// float at +0xC counts down like opcode 13's.
// Xbox 0x001335E0
void script_tick_total_count_condition(ScriptCond& rec)
{
    const u16 index = u16(cond_u32(rec, 8));
    i32 sum = 0;
    if (index < 2)
    {
        sum = script_statistic_at_address(0x0051696Cu) + script_statistic_at_address(0x005172F4u)
            + script_statistic_at_address(0x0051AD18u) + script_statistic_at_address(0x0051A390u);
    }
    else
    {
        const u32 scaled = u32(index) * 4u;
        sum = script_statistic_at_address(0x0051AD18u + scaled) + script_statistic_at_address(0x0051A390u + scaled)
            + script_statistic_at_address(0x005172F4u + scaled) + script_statistic_at_address(0x0051696Cu + scaled);
    }
    if (sum == 0 || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (next <= 0.f)
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}


// Xbox 0x001330E0. Opcode 1 in the update table at 0x00314334. While the
// DAT_00511B48 actors whose body +0x94 is nonzero number at most the int at
// +8, DAT_0050E768 is clear and the byte at [DAT_0051248C]+0x30F144 is set,
// the float at +0xC counts down by the frame step, stopping at 0; 0x00132680
// fires once it reaches 0.
// Xbox 0x001330E0
void script_tick_actor_count_condition(ScriptCond& rec)
{
    i32 count = 0;
    for (const auto* node = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
         node != nullptr; node = static_cast<const entity_detail::ListHead*>(node->next))
    {
        if (!g_alt_node_body)
        {
            throw Error("DAT_00511B48 node has no body view");
        }
        if (body_record(g_alt_node_body(node)).energy != 0)
        {
            count = count + 1;
        }
    }
    if (count > static_cast<i32>(cond_u32(rec, 8)) || g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (!(0.f < next))
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}


// Xbox 0x00133170. Opcode 3 in the update table at 0x00314334. Walks DAT_0050E4DC to the first event 0xAD, then
// along the events of that type to one whose word at +0xC is the word at record+8. With that event queued,
// DAT_0050E768 clear and the byte at [DAT_0051248C]+0x30F144 set, the float at +0xC counts down by the frame step
// (0 while DAT_004312DC is above 0), stopping at 0.
// Xbox 0x00133170
void script_tick_picked_up_condition(ScriptCond& rec)
{
    LiveScript* node = g_live_scripts;
    while (node != nullptr && node->type != 0xadu)
    {
        node = node->next;
    }
    if (node == nullptr)
    {
        return;
    }
    const u16 want = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    while (node->word_c != want)
    {
        const u32 type = node->type;
        node = node->next;
        while (node != nullptr && node->type != type)
        {
            node = node->next;
        }
        if (node == nullptr)
        {
            return;
        }
    }
    if (g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    const float step = 0.f < g_sim_freeze ? 0.f : g_sim_dt * g_sim_scale;
    const float left = cond_f32(rec, 0xc);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0xc, next);
        if (!(0.f < next))
        {
            cond_put_f32(rec, 0xc, 0.f);
        }
    }
}


// Xbox 0x00133770. Opcode 15 in the update table at 0x00314334. The word at record+8 is kept when 1 is below it.
// Walks DAT_0050E4DC to the first event 0xB7, then along the events of that type. An event whose word at +0xC is the
// kept word (or any, with no word) passes by the int at record+0xC: 0 always, 1 when its dword at +0x10 is nonzero,
// 2 when that dword is zero, any other value always. A passing event, with DAT_0050E768 clear and the byte at
// [DAT_0051248C]+0x30F144 set, counts the float at +0x10 down by the frame step (0 while DAT_004312DC is above 0),
// stopping at 0.
// Xbox 0x00133770
void script_tick_ghouly_signal_condition(ScriptCond& rec)
{
    const u16 raw = u16(rec.raw[8]) | (u16(rec.raw[9]) << 8);
    const u16 key = 1 < raw ? raw : u16(0);
    LiveScript* node = g_live_scripts;
    while (node != nullptr && node->type != 0xb7u)
    {
        node = node->next;
    }
    if (node == nullptr)
    {
        return;
    }
    for (;;)
    {
        if (key == 0 || node->word_c == key)
        {
            const i32 mode = static_cast<i32>(cond_u32(rec, 0xc));
            u32 state = 1;
            if (mode == 1)
            {
                state = node->dword_10;
            }
            else if (mode == 2)
            {
                state = node->dword_10 == 0 ? 1u : 0u;
            }
            if (state != 0)
            {
                break;
            }
        }
        const u32 type = node->type;
        node = node->next;
        while (node != nullptr && node->type != type)
        {
            node = node->next;
        }
        if (node == nullptr)
        {
            return;
        }
    }
    if (g_block_decay != 0 || g_decay_arm == 0)
    {
        return;
    }
    const float step = 0.f < g_sim_freeze ? 0.f : g_sim_dt * g_sim_scale;
    const float left = cond_f32(rec, 0x10);
    if (0.f < left)
    {
        const float next = left - step;
        cond_put_f32(rec, 0x10, next);
        if (!(0.f < next))
        {
            cond_put_f32(rec, 0x10, 0.f);
        }
    }
}


// Xbox 0x00128D90. Resume the scene's fired cursor at the matching title.
void script_resume_title(Game& g, i32 mode, i32 id)
{
    u32 offset = 0; // DAT_0050E760, the resource's first node.
    auto node = script_node_at(g.scene_script, offset);
    while (node.has_value() && node->type != 0)
    {
        if (node->type == 0xe && i32(node_u32(g.scene_script, *node, 8)) == mode &&
            i32(node_u32(g.scene_script, *node, 0xc)) == id)
        {
            g_script_fired.at(0) = offset;
            return;
        }
        offset = node->link;
        node = script_node_at(g.scene_script, offset);
    }
}


// Xbox 0x00117140. Drain the scene script, then request the next scene AID.
void script_tick_fired_vm(Game& g)
{
    // Xbox 0x00117140
    if (g_script_e78c == 0)
    {
        if (g_script_e79c != 0)
        {
            script_resume_title(g, g_script_e798, g_script_e79c);
            g_script_e79c = 0;
        }
        bool latched = false;
        if (g_script_e794 == 0)
        {
            script_evaluate_conditions(g);
            const size_t fired_before = g_script_fired.size();
            script_execute_fired_actions(g);
            if (fired_before != 0 && g_script_fired.empty())
            {
                g_script_e788 = 1;
                latched = true;
            }
        }
        if (!latched && g_script_e788 == 0)
        {
            return;
        }
        // 0x00035960, EAX &DAT_00511B64 (the held scene order), EBX DAT_00510E58 (the current scene AID, not
        // the title written by type 0xE), wrapping.
        const std::span<const u8> order = g.factory_scene->actor_services->held_scene_order();
        if (next_scene_order_name(order, g.scene_script.name.c_str(), 1) != nullptr)
        {
            if (g_scene_entry_set == 0) // DAT_0050E784
            {
                request_scene_aid(next_scene_order_name(order, g.scene_script.name.c_str(), 1), 1); // 0x0011EB30
                g_script_e790 = 0.f;
            }
            else
            {
                // Through a door: the player walks out to the next scene's entry pose, and the next scene is
                // requested when the second below runs out.
                g.factory_scene->actor_services->send_player_through_exit(g_scene_entry_position,
                    g_scene_entry_euler);
                g_script_e790 = 1.f; // DAT_0032B0F8
            }
        }
        g_script_e78c = 1;
        g_script_e794 = g_script_e794 + 1;
        return;
    }

    float step = 0.f;
    if (g_sim_freeze <= 0.f)
    {
        step = g_sim_dt * g_sim_scale;
    }
    if (0.f < g_script_e790)
    {
        g_script_e790 = g_script_e790 - step;
        if (g_script_e790 <= 0.f)
        {
            g_script_e790 = 0.f;
            // 0x00035960 (EAX DAT_00511B64, EBX DAT_00510E58, wrapping), then 0x0011EB30 in mode 1.
            const std::span<const u8> order = g.factory_scene->actor_services->held_scene_order();
            const char* next = next_scene_order_name(order, g.scene_script.name.c_str(), 1);
            if (next == nullptr)
            {
                throw Error("0x00117140's fade requests the next scene with no scene after this one");
            }
            request_scene_aid(next, 1);
        }
    }
}


// DAT_00509b20 through DAT_00509b2c. entity.cpp keeps a private copy of this tint.
float g_scene_ambient_tint_0 = 0.f; // DAT_00509B20

float g_scene_ambient_tint_1 = 0.f; // DAT_00509B24

float g_scene_ambient_tint_2 = 0.f; // DAT_00509B28

float g_scene_ambient_tint_3 = 0.f; // DAT_00509B2C

float g_scene_light_intensity = 0.f; // DAT_00509b4c
i32 g_player_start_enabled = 0; // DAT_005462FC
i32 g_marker_group_count = 0; // DAT_00545D1C
i32 g_marker_swap_count = 0; // DAT_00545CC8
std::array<u16, 0x40> g_marker_swap_ids{}; // DAT_00545C48: id pairs 0x001400A0 appends, DAT_00545CC8 of them.
std::array<u16, 0x28> g_marker_group_ids{}; // DAT_00545CCC: ten-id groups 0x001400D0 appends, DAT_00545D1C of them.

// Xbox 0x0011CAC0. Writes 1.0 (0x3f800000) to DAT_00509b2c, DAT_00509b20,
// DAT_00509b24, DAT_00509b28, and DAT_00509b4c. DAT_00509b50 becomes
// 0x3eb2b8c3. DAT_005462FC becomes 1. DAT_00509b54 becomes 0x3f32b8c3.
// DAT_00509b58 becomes 1. DAT_00509b70 becomes 0. DAT_00509b74 becomes 1.
// The light words are lighting.cpp's. DAT_00545D1C and DAT_00545CC8 are cleared after the 0x000FAEE0 call; then
// 0x0013CBD0, 0x00116E00 and the scene load 0x00124420.
// Xbox 0x0011CAC0
void scene_reset_runtime_parameters(Game& g)
{
    g_scene_ambient_tint_3 = 1.f;
    g_scene_ambient_tint_0 = 1.f;
    g_scene_ambient_tint_1 = 1.f;
    g_scene_ambient_tint_2 = 1.f;
    g_scene_light_intensity = 1.f;
    g_scene_light_direction.pitch = std::bit_cast<float>(0x3eb2b8c3u); // DAT_00509B50
    g_player_start_enabled = 1;
    g_scene_light_direction.yaw = std::bit_cast<float>(0x3f32b8c3u); // DAT_00509B54
    g_scene_light_direction.camera_relative = 1; // DAT_00509B58
    g_light_paused = 0; // DAT_00509B70
    g_light_running = 1; // DAT_00509B74
    reset_actor_camera_shakes(g_runtime_profile.unlocks.bytes[0], runtime_pad_rumble_commands()); // 0x000FAEE0
    g_marker_group_count = 0;
    g_marker_swap_count = 0;
    reset_runtime_camera_view_tunables(); // 0x0013CBD0
    reset_scene_script_runtime(g); // 0x00116E00
    // 0x00124420, the scene load (0x00035960, 0x00034050, 0x00124490, 0x00123A80, 0x00033FB0, 0x000341E0, then
    // 0x00033E00 with the script name), is the host load: scene_set_state's caller runs script_load as soon as
    // state 4 is entered (scene.cpp). Its loading-screen calls are not mapped onto the host load yet.
}


// Xbox 0x0011CB60. Start the loaded scene and run its script latch.
void script_queue_scene_start(Game& g)
{
    // Xbox 0x0011CB60
    // 0x0011CBD0 sends the scene-start message directly to 0x00116810,
    // before its entity broadcasts. This wakes the title action.
    const std::array<u8, 4> start{0x7d, 0, 0, 0};
    if (g.factory_scene == nullptr || g.factory_scene->actor_start != 0)
    {
        throw Error("0x0011CBD0 starts a scene that is not loaded natively or has already started");
    }
    // 0x00117000 ends with the scene's save; the script it acquires is the host load's.
    g.factory_scene->actor_services->save_scene_start();
    g.factory_scene->start_actors(g).start_scene([&g, &start]()
    {
        script_queue_event(g, start);
    });
    g.factory_scene->actor_start = 1;
    log::info("native scene start complete: %s at %.3f s", g.scene_name.c_str(), g.clock.time);
    g_scene_script_started = true;
    g.factory_scene->start_actors(g).tick_live_spawns(); // 0x0011B670
    script_tick_fired_vm(g); // 0x00117140
}


// Xbox 0x00117100: 0x00140E30, then the condition updates and the eval at 0x00117140. The update walk calls the
// table at 0x00314334: opcode 0 is 0x00133070, opcode 1 is 0x001330E0, opcode 3 is 0x00133170, opcode 15 is
// 0x00133770, opcode 16 is 0x001336A0, opcode 4 is 0x00133230, opcode 5 is 0x001332B0, opcode 10 is 0x001333A0, opcode 11 is 0x00133330, opcode 13 is
// 0x00133570, opcode 14 is 0x001335E0, opcode 17 is 0x001334B0, opcode 18 is 0x00133870 and opcode 19 is 0x00133900.
// Opcodes 2, 6..9 and 20..26 have the empty 0x001628B0; the others' updates
// are not translated and raise when a record reaches them.
void script_vm_tick(Game& g, float dt)
{
    (void)dt;
    scene_update_actor_marker(g); // 0x00140E30
    static constexpr u32 kUpdates[27] = {0x00133070, 0x001330E0, 0x001628B0, 0x00133170, 0x00133230,
        0x001332B0, 0x001628B0, 0x001628B0, 0x001628B0, 0x001628B0, 0x001333A0, 0x00133330, 0x00133420,
        0x00133570, 0x001335E0, 0x00133770, 0x001336A0, 0x001334B0, 0x00133870, 0x00133900, 0x001628B0,
        0x001628B0, 0x001628B0, 0x001628B0, 0x001628B0, 0x001628B0, 0x001628B0};
    for (ScriptCond& rec : g_script_conds)
    {
        const u32 opcode = cond_u32(rec, 0);
        if (opcode >= 27)
        {
            throw Error("script condition opcode " + std::to_string(opcode) + " outside the update table");
        }
        if (opcode == 0)
        {
            script_tick_condition_timer(rec);
        }
        else if (opcode == 1)
        {
            script_tick_actor_count_condition(rec);
        }
        else if (opcode == 3)
        {
            script_tick_picked_up_condition(rec);
        }
        else if (opcode == 15)
        {
            script_tick_ghouly_signal_condition(rec);
        }
        else if (opcode == 16)
        {
            script_tick_ghouly_exists_condition(rec);
        }
        else if (opcode == 13)
        {
            script_tick_tagged_event_condition(rec);
        }
        else if (opcode == 14)
        {
            script_tick_total_count_condition(rec);
        }
        else if (opcode == 4)
        {
            script_tick_statistics_latch_condition(rec);
        }
        else if (opcode == 5)
        {
            script_tick_pair_count_condition(rec);
        }
        else if (opcode == 10)
        {
            script_tick_ghoulies_killed_condition(rec);
        }
        else if (opcode == 11)
        {
            script_tick_ghoulies_alive_condition(rec);
        }
        else if (opcode == 17)
        {
            script_tick_marker_broken_condition(rec);
        }
        else if (opcode == 18)
        {
            script_tick_actor_energy_condition(rec);
        }
        else if (opcode == 19)
        {
            script_tick_statistics_target_condition(rec);
        }
        else if (opcode == 12)
        {
            script_tick_actor_gone_condition(rec);
        }
        else if (kUpdates[opcode] != 0x001628B0)
        {
            throw Error("script condition opcode " + std::to_string(opcode) + " has an update the table lacks");
        }
    }
    script_tick_fired_vm(g);
}

} // namespace gbtg
