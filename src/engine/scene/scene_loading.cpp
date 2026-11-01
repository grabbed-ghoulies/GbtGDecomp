#include "lighting.h"
#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/scene_transitions.h"
#include "../runtime/name_lookup.h"
#include "../entities/player_spawn.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_conditions.h"
#include "executable_resources.h"

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


#include "scene_types.h"
#include "factory_scene_runtime.h"
#include "../scripts/script_chapter.h"
#include "scene.h"
#include "scene_loading.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_vm.h"

namespace gbtg
{
using namespace scene_detail;





// Xbox 0x00124490. EBX is the scene record; the six rows at 0x00430FB0 are the scenes without a player.
bool scene_has_player(const char* scene, std::span<const u8> rows)
{
    u32 offset = 0;
    do
    {
        if (transition_aid_equal(scene, reinterpret_cast<const char*>(rows.data() + offset)))
        {
            return false;
        }
        offset += 0x80;
    } while (offset < 0x300);
    return true;
}

// The player bundle 0x00124420 adds to the scene's bundle set: the template 0x00123A80 selects, when
// 0x00124490 finds the scene has a player. The rest of 0x00124420 is the bundle stream itself, which the
// port's scene loader performs.
void load_scene_player_bundle(Game& g)
{
    const auto& image = *g.executable_resources;
    const char* scene = g.scene_script.name.c_str();
    if (!scene_has_player(scene, image.memory(0x00430FB0, 0x300)))
    {
        return;
    }
    const char* name = select_player_parameter_name(scene,
        std::span<const u8, 0x210>(image.memory(0x00430DA0, 0x210).data(), 0x210), g_scene_flags->bytes[0],
        g_flags_ea9c);
    if (const auto rel = aid_to_bundle_rel(name))
    {
        const auto& bundle = g.aids.load_bundle_rel(*rel);
        log::verbose("loaded player bundle %s entries %zu", rel->c_str(), bundle.entries.size());
    }
}

void load_scene_contents(Game& g, std::string_view name)
{
    if (g.factory_scene != nullptr)
    {
        g.factory_scene->end_scene(); // 0x0011C5A0
    }
    if (g.native_resources != nullptr)
    {
        // The tail of 0x0011C5A0: unload what nothing references, flushing both deferred lists, until nothing
        // more goes. 0x00034880 then frees the bundle pools; the host AidStore keeps its files.
        auto& registry = g.native_resources->registry;
        bool unloaded = false;
        do
        {
            unloaded = unload_unreferenced_animation_resources(registry); // 0x00035D30
            flush_animation_resource_unloads(registry); // 0x00036180
            flush_animation_resource_unloads(registry);
        }
        while (unloaded);
    }
    script_set_default_name(g);
    if (name.empty())
    {
        return;
    }
    script_set_name(g, name);
    if (g.executable_resources != nullptr && g.factory_scene == nullptr)
    {
        g.factory_scene = std::make_shared<SceneFactoryRuntime>(g.executable_resources, *g.native_resources);
        g.factory_scene->controls->context.chapter = [&g]() -> i32
        {
            return script_chapter_index(g.scene_script.name); // 0x0004B4D0
        };
    }
    if (const auto rel = aid_to_bundle_rel(g.scene_name))
    {
        const auto path = g.aids.bundle_path(*rel);
        if (g.aids.bundle_exists(*rel))
        {
            g.scene_bundle = &g.aids.load_bundle_rel(*rel);
            log::verbose("loaded %s entries %zu", rel->c_str(), g.scene_bundle->entries.size());
            auto scripts = entries_of_type(*g.scene_bundle, ResourceType::Script);
            if (scripts.empty())
            {
                log::verbose("%s", kNoScript);
            }
            for (const auto* e : scripts)
            {
                g.scene_script = SceneScript{e->name, e->cpu};
                log::verbose("  script %s bytes %zu", e->name.c_str(), e->cpu.size());
                log_script_nodes(g.scene_script);
                script_vm_bind(g);
            }
            for (const auto* e : entries_of_type(*g.scene_bundle, ResourceType::AidList))
            {
                auto names = parse_aidlist(e->cpu);
                log::verbose("  aidlist %s (%zu)", e->name.c_str(), names.size());
                for (size_t i = 0; i < names.size() && i < 12; ++i)
                {
                    log::verbose("    [%zu] %s", i, names[i].c_str());
                }
                if (names.size() > 12)
                {
                    log::verbose("    ...");
                }
            }
            if (g.executable_resources != nullptr)
            {
                load_scene_player_bundle(g);
            }
        }
        else
        {
            log::warn("scene bundle missing: %s", path.string().c_str());
        }
    }
}


void script_set_default_name(Game& g)
{
    // Xbox 0x001287F0: copy default into DAT_00512494 and zero script globals.
    script_vm_reset();
    g_light_offset = {0.f, 30.f, -30.f}; // DAT_0050D764; DAT_0032B108 and DAT_0032B3FC.
    g_light_filled_gate = 5; // DAT_0050D720.
    g_flag_10b4 = 1; // DAT_005110B4, the mirror pass gate.
    g_state_248c = 0;
    g_state_2490 = 0;
    g_gate_251c = 0;
    g_flag_2470 = 0;
    g_flag_2474 = 0;
    g_flag_2378 = 0; // DAT_00512378
    g_flag_2414 = 0; // DAT_00512414
    g_flag_2418 = 0; // DAT_00512418
    g.scene_name = kNoScript;
    g.scene_script = {};
}


void script_set_name(Game& g, std::string_view name)
{
    // Xbox 0x00128AB0: EAX = C string, copies into DAT_00512494.
    // Retail clears the condition pool at DAT_0051234C, not the fired cursor
    // pool at DAT_00512488. A type-0xE action names the scene while its fired
    // record is still being walked; clearing that record loses its resume point.
    g_script_conds.clear();
    g.scene_name = std::string(name);
}


void scene_order_go(Game& g, size_t index)
{
    // Xbox 0x0012C2D0 returns the sceneorder slot. 0x0012C5D0 loads that script.
    if (index >= g.scene_order.size())
    {
        return;
    }
    g.scene_index = index;
    log::info("scene order [%zu] %s", index, g.scene_order[index].c_str());
    load_scene_contents(g, g.scene_order[index]);
}


void script_load(Game& g, std::string_view name)
{
    // Xbox 0x0012C5D0. 0x0012C2D0 looks the requested aid up in the directory
    // at 0x0042AC80 (0x00116220, stride 0x90) and loads that scene. A name that
    // is not on the boot list is still a script: the directory is every aid_script.
    if (name.empty())
    {
        return;
    }
    for (size_t i = 0; i < g.scene_order.size(); ++i)
    {
        if (g.scene_order[i] == name)
        {
            scene_order_go(g, i);
            return;
        }
    }
    log::info("script %.*s", static_cast<int>(name.size()), name.data());
    load_scene_contents(g, name);
}


// DAT_00510ED8. Defined with the scene record. script_request copies into it.
extern char (&g_510ed8)[0x80];


void request_scene_aid(const char* src, i32 mode); // Xbox 0x0011EB30

// Every retail caller of 0x0011EB30 that requests a script by name (boot 0x00033030, the front-end
// slots 0x00134AE0..0x00134D20 and the scene end 0x00117140) passes mode 1. The mode lands at
// DAT_00510F58 (scene record +0x100), the state 0x0011E4C0 enters after loading; state 1 runs
// the scene tick 0x0011E7F0. Loading waits for 0x0011E4C0.
void script_request(Game& g, std::string_view name)
{
    (void)g;
    if (name.size() + 1 > sizeof(g_510ed8))
    {
        throw Error("scene request name exceeds the 0x80-byte scene record field");
    }
    const std::string aid(name);
    request_scene_aid(aid.c_str(), 1);
}


// Header in front of the records. Xbox stores begin, cursor, and limit as
// addresses in the 12 bytes before +0x10. Those dwords are byte offsets from
// the block here, so the records stay at +0x10.
struct Pool662a0
{
    u16 stride = 0;
    u16 grow = 0;
    u32 begin = 0;
    u32 cursor = 0;
    u32 limit = 0;
};

static_assert(sizeof(Pool662a0) == 0x10);

static_assert(offsetof(Pool662a0, begin) == 4);

static_assert(offsetof(Pool662a0, cursor) == 8);

static_assert(offsetof(Pool662a0, limit) == 0xc);


// Xbox 0x000662A0
// EAX is the pool block. used = (cursor - begin) / stride. The new count is
// used plus the u16 at +2. 0x000F73A0 reallocates EAX to stride * count + 0x10
// and adds the 4-byte tag 0x000F72D0 keeps. begin is the new block + 0x10,
// cursor stays on the same record, and limit is begin + stride * count.
// Xbox 0x000662A0
u8* engine_grow_record_pool(u8* block)
{
    Pool662a0* pool = reinterpret_cast<Pool662a0*>(block);
    const i32 stride = static_cast<i32>(pool->stride);
    const i32 used = (static_cast<i32>(pool->cursor) - static_cast<i32>(pool->begin)) / stride;
    const i32 count = static_cast<i32>(pool->grow) + used;
    const u32 bytes = static_cast<u32>(stride) * static_cast<u32>(count) + 0x10u;
    u8* fresh = static_cast<u8*>(std::realloc(block, static_cast<std::size_t>(bytes) + 4u));
    Pool662a0* grown = reinterpret_cast<Pool662a0*>(fresh);
    const i32 grown_stride = static_cast<i32>(grown->stride);
    const u32 base = 0x10u;
    grown->cursor = base + static_cast<u32>(grown_stride) * static_cast<u32>(used);
    grown->begin = base;
    grown->limit = base + static_cast<u32>(grown_stride) * static_cast<u32>(count);
    return fresh;
}


// DAT_004DEDA8. 0x00033CF0 allocates the header. The u16 at +0 is the stride.
// +2 is the grow count. +4 is begin and +8 is end.
struct Pool4deda8
{
    u16 stride = 0;
    u16 grow = 0;
    u8* begin = nullptr;
    u8* end = nullptr;
};


Pool4deda8* g_4deda8 = nullptr;
 // DAT_004DEDA8
i32 g_4deda4 = 0;
               // DAT_004DEDA4
i32 g_4ded9c = 0;
               // DAT_004DED9C
i32 g_4deda0 = 0;
               // DAT_004DEDA0
i32 g_4dedac = 0;
               // DAT_004DEDAC

// Xbox 0x000341E0. When DAT_004DEDA4 is nonzero, walk the pool from end
// backward by 0x2c. The dword at record+0x28 (end - 4 before the step) is
// compared with 0. Zero calls 0x000348D0 with EBX = the record and
// 0x00034110 with ESI = the record, then reloads DAT_004DEDA8. The flag is
// then cleared. When DAT_004DEDAC is 0 and DAT_004DEDA0 < DAT_004DED9C, walk
// from begin while the cursor is below end and the dword at +0x24 is nonzero,
// stepping by the stride u16. The slot where +0x24 is 0, or 0 when the walk
// finishes, is ESI for 0x00034290; a nonzero return calls 0x00034370 with
// that return on the stack and EAX = ESI. The function returns DAT_004DEDAC.
// The host loads synchronously, so nothing fills this pool (0x00033CF0 is not
// run) and both paths raise if they are ever reached: their callees are not
// translated.
// Xbox 0x000341E0
int scene_check_pending_records()
{
    Pool4deda8* pool = g_4deda8;
    if (g_4deda4 != 0)
    {
        const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(pool->begin);
        std::uintptr_t end = reinterpret_cast<std::uintptr_t>(pool->end);
        while (begin <= end - 0x2c)
        {
            const std::uintptr_t record = end - 0x2c;
            const i32 at_28 = *reinterpret_cast<const i32*>(record + 0x28);
            end = record;
            if (at_28 == 0)
            {
                throw Error("0x000341E0 completes a pending record through 0x000348D0 and 0x00034110, which are not "
                            "translated");
            }
        }
        g_4deda4 = 0;
    }
    if (g_4dedac == 0 && g_4deda0 < g_4ded9c)
    {
        std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(pool->begin);
        const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(pool->end);
        while (cursor < end && *reinterpret_cast<const i32*>(cursor + 0x24) != 0)
        {
            cursor = cursor + pool->stride;
        }
        throw Error("0x000341E0 starts a pending record through 0x00034290, which is not translated");
    }
    return g_4dedac;
}

} // namespace gbtg
