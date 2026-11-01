#include "../cutscenes/door_registration.h"
#include "../scripts/script_event_sequence.h"
#include "../scripts/script_actions.h"
#include "gbtg/anim.h"
#include "../graphics/refraction_pass.h"
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
#include "../saves/runtime_profile.h"

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
#include "../entities/intro_notices.h"
#include "scene_loading.h"
#include "scene_tick_nodes.h"
#include "scene_controls.h"
#include "../transitions/scene_transitions.h"
#include "../scripts/script_actions.h"
#include "../scripts/script_vm.h"
#include "../entities/ghouly_requests.h"
#include "../saves/runtime_profile.h"
#include "../scripts/script_conditions.h"
#include "../navigation/occupancy.h"
#include "../entities/camera_director.h"
#include "gbtg/actor_spawn_factory.h"
#include "dialogue_scene_runtime.h"
#include "ambient.h"
#include "../entities/strategy_target_queue.h"
#include "../entities/actor_live_attachments.h"
#include "../audio/native_audio.h"
#include "../runtime/random_state.h"
#include "scene_pause.h"
#include "../camera/actor_camera_effects.h"
#include "../audio/listener_settings.h"
#include "../frontend/dialogue_activation.h"
#include "../transitions/scene_transition.h"

namespace gbtg
{
using namespace scene_detail;

namespace
{
// The boot scene services' pause views (0x0011CE10, 0x0011CE40, 0x0011CFB0, 0x0011CD90).
ScenePauseServices& scene_pause_services(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("the scene pause has no boot scene services");
    }
    return g.factory_scene->actor_services->pause_services();
}
}

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


// DAT_00430D0C. Cleared with DAT_00430D08 and DAT_00430D10.
i32 g_script_30d0c = 0;

// DAT_00430D94. Nonzero skips the clear of DAT_00506C8C, then becomes 0.
i32 g_script_30d94 = 0;

// DAT_00430D98. Cleared with DAT_00430D94.
i32 g_script_30d98 = 0;





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
    g_byte_30d14[0] = 0;
    if (g_script_30d94 == 0)
    {
        g_runtime_profile.times[5] = 0; // DAT_00506C8C, the scene's kill count
    }
    g_script_30d94 = 0;
    g_script_30d98 = 0;
    g_510f58 = mode;
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

// DAT_0050E438. The global names the link at object+8. Next is the pointer
// at +4. vtable +0x10 is called with the object. The port keeps the method
// and the next pointer on the node the global names.
SceneTickNode* g_50e438 = nullptr;


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
// Xbox 0x00038110
void scene_set_audio_level(float level)
{
    // Xbox 0x00038110
    if (g_transition_game == nullptr || g_transition_game->factory_scene == nullptr
        || g_transition_game->factory_scene->actor_services == nullptr)
    {
        throw Error("0x00038110 has no boot audio listener (the scene services are not built)");
    }
    AudioListenerSettings& listener = g_transition_game->factory_scene->actor_services->audio_listener();
    const float scaled = level * 127.f;
    auto take = [scaled, &listener](i32 category)
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
        set_audio_category_volume(listener, u32(category), bits); // 0x0015C1C0
    };
    take(g_4219ec);
    take(g_4219f4);
    take(g_421a0c);
    take(g_421a14);
    take(g_421a1c);
}


namespace
{
// The scene's actor services, which hold the intro notice's views (0x0013FD60 / 0x0013FFD0).
SceneActorServices& notice_scene_actors(Game& g)
{
    if (g.factory_scene == nullptr || g.factory_scene->actor_services == nullptr)
    {
        throw Error("the intro notice (DAT_00545B38) runs without the scene's actor services");
    }
    return *g.factory_scene->actor_services;
}
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
// 0x0004EBD0 ticks the native scene's cutscenes; 0x0005AA90 ticks the doors; 0x00057E60 ticks the scene dialogue.
// DAT_00545B38 3 raises at 0x0013FFD0. 0x00117140 runs, then 0x000D0300 ticks the event sequence and 0x00049DE0 the
// front end's state (its states 2 .. 6 raise); 0x00117140 runs again. DAT_00506E28 equal to 1 calls 0x000F4A00 (the
// ghoul intro) before DAT_0050E438 is loaded. The list walk saves the next link, then calls vtable +0x10.
//
// 0x00123020 takes DAT_00511B18 on the stack, then 0x00122FE0, 0x001128A0 (the attacker queues) and 0x0011B670
// run. 0x00117100 then runs, including its tail into 0x00117140. 0x000F4560 ticks the ghoul intro; 0x0013FD60
// raises when its gate opens.
//
// DAT_004312DC above 0 leaves the step at 0. Otherwise the step is DAT_005096C0 * DAT_004312C4, capped at
// DAT_0032B150. 0x00115BB0 takes the step times DAT_0032B6E0, then 0x001162F0 ages the actor snapshots.
//
// DAT_00510580 nonzero and DAT_00510650 clear adds DAT_0032B314 to DAT_00510648 and DAT_0051064C, then raises at
// 0x0011B7F0. 0x0012AA70 (the post models, raising with one loaded), 0x001185E0 (the ambience), 0x000FDDA0 (the light
// records) and 0x000FD5C0 (the scene fog) run.
//
// With records on the input stack (DAT_0050E0E8) and the top one's device nonzero, Start newly pressed (0x001083F0,
// 0x10) with DAT_00510CEC clear stores 6 at DAT_00510F60, or 1 when DAT_00510CE8 is nonzero.
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
        notice_scene_actors(g).finish_intro_notice(); // 0x0013FFD0
    }
    script_tick_fired_vm(g);
    tick_script_event_sequence(g); // 0x000D0300
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).tick_frontend_state(); // 0x00049DE0
    }
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
    resort_runtime_strategy_target_queues(g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f,
        g_actor_attachment_slots); // 0x001128A0
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).tick_live_spawns(); // 0x0011B670
    }
    script_vm_tick(g, 0.f);
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).update_ghoul_intro(); // 0x000F4560
    }
    if (g_545b38 != 0)
    {
        notice_scene_actors(g).tick_intro_notice(); // 0x0013FD60
    }

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
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).tick_background_cloths(paced); // 0x00115BB0
    }
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).age_actor_snapshots(); // 0x001162F0
    }

    if (g_scene_refraction.mesh != nullptr && g_flag_0650 == 0) // DAT_00510580, DAT_00510650
    {
        const float drift = std::bit_cast<float>(0x3b03126fu);
        g_scene_refraction.drift_u = g_scene_refraction.drift_u + drift; // DAT_00510648
        g_scene_refraction.drift_v = g_scene_refraction.drift_v + drift; // DAT_0051064C
        notice_scene_actors(g).rebuild_refraction_quad(); // 0x0011B7F0
    }
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).tick_post_models(); // 0x0012AA70
    }
    tick_ambient(g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f, g_shared_random,
        native_audio_runtime().streams, *g.executable_resources); // 0x001185E0
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).animate_light_records(); // 0x000FDDA0
    }
    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1)
    {
        g.factory_scene->start_actors(g).configure_scene_fog(); // 0x000FD5C0
    }

    if (g.factory_scene != nullptr && g.factory_scene->actor_start == 1
        && g.factory_scene->start_actors(g).start_pressed() && g_510cec == 0) // 0x001083F0
    {
        // DAT_00510F60: 1 with DAT_00510CE8 set, else 6.
        const i32 request = g_510ce8 != 0 ? 1 : 6;
        std::memcpy(scene + 0x108, &request, 4);
    }
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

// DAT_0050EA01 is a byte. The floats are DAT_0050EA7C through DAT_0050EA90 and DAT_0050EA94 is a dword. The
// background volume before them (DAT_0050EA00 .. DAT_0050EA18) is ambient.cpp's.
u8 g_50ea01 = 0;

float g_50ea7c = 0.f;

float g_50ea80 = 0.f;

float g_50ea84 = 0.f;

float g_50ea88 = 0.f;

float g_50ea8c = 0.f;

float g_50ea90 = 0.f;

i32 g_50ea94 = 0;

i32 g_504330 = 0;
 // DAT_00504330
// DAT_00512344 is the block 0x000663E0 releases. DAT_00512348 is cleared with it.
void* g_512344 = nullptr;

i32 g_512348 = 0;

i32 g_50e1c4 = 0;
 // DAT_0050E1C4
std::array<u8, 8 * 0xC> g_50e1c8{}; // DAT_0050E1C8: the 0x0010F6C0 class ranges, up to DAT_0050E228.
i32 g_50e228 = 0;
 // DAT_0050E228
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



// Xbox 0x0011C5A0
// ESI is the scene record. The dword at +0x118 is cleared. DAT_0051065C is 1 for the body and 0 on the way out. The
// host splits the teardown: the globals below run here as the scene state enters 4, and the scene's actors, controls,
// cutscenes, doors, effects and passes go in SceneFactoryRuntime::end_scene and load_scene_contents as the next scene
// loads; each step's note below names where it runs. 0x001593D0 only clears D3D device state (the presenter's).
// Xbox 0x0011C5A0
void scene_teardown(Game& g, u8* scene)
{
    // Xbox 0x0011C5A0
    g_51065c = 1;
    if (RuntimeDialogueActivationContext& dialogue = scene_pause_services(g).dialogue; dialogue.state.mode != 0)
    {
        set_runtime_dialogue_mode(0, dialogue); // DAT_004EE1EC, 0x000584D0
    }
    // 0x0004ECE0 (the scene cutscenes) and 0x0005B1E0 (the doors) run in SceneFactoryRuntime::end_scene (at the next
    // load). 0x00049EF0 releases the challenge's records, widget and input (DAT_004E17D8 .. DAT_004E19B0), which
    // nothing native creates (the challenge setup actions raise), so it is not called.
    g_50d780 = 0;
    g_50d770 = nullptr;
    g_50d774 = nullptr;
    g_50d778 = nullptr;
    g_50d77c = nullptr;
    // The scene controls (0x00115F00), message 0x80 to the active actors (0x00122C70) and the retired ones' removal
    // (0x001232D0), the contact slots (0x001222A0), the cloths (0x001143A0), the point-light pass (0x00123640) and
    // the blob shadows (0x001199F0) run in SceneFactoryRuntime::end_scene. The navigation grids (0x00110AB0) go with
    // the scene's NavigationSceneStorage and the mirrors (0x0011F610) with RuntimeSceneMirrors at the next start.
    if (g_scene_refraction.noise != 0)
    {
        // 0x001759F0 D3DResource_Release of DAT_00510584; the texture goes with the scene's resource memory.
        g_scene_refraction.noise = 0;
    }
    g_scene_refraction.mesh = nullptr; // DAT_00510580
    g_scene_refraction.model = {};
    // 0x001133B0 (the light grid) runs in SceneFactoryRuntime::end_scene.
    g_amb_volume_enabled = 0; // DAT_0050EA00, the background volume (ambient.cpp).
    g_50ea01 = 0;
    g_amb_color_lo[2] = 0.f;
    g_amb_color_lo[1] = 0.f;
    g_amb_color_lo[0] = 0.f;
    g_amb_color_hi[2] = 0.f;
    g_amb_color_hi[1] = 0.f;
    g_amb_color_hi[0] = 0.f;
    g_50ea84 = 0.f;
    g_50ea80 = 0.f;
    g_50ea7c = 0.f;
    g_50ea90 = 0.f;
    g_50ea8c = 0.f;
    g_50ea88 = 0.f;
    g_50ea94 = 0;
    // 0x000FD710 runs in the actor services' end of scene (SceneActorServices::State::end_scene).
    const u32 kept = (g_flags_ea9c >> 8) & 1u;
    g_flag_ea98 = 0; // DAT_0050EA98
    g_flags_ea9c = 0;
    if (kept != 0)
    {
        g_flags_ea9c = 0x100u;
    }
    *reinterpret_cast<i32*>(scene + 0x118) = 0;
    // 0x000ED450 (the global effect nodes) runs in SceneFactoryRuntime::end_scene.
    g_504330 = 0;
    if (g.factory_scene != nullptr && g.factory_scene->actor_services != nullptr)
    {
        g.factory_scene->actor_services->reset_sound_queues(); // 0x00129F70
    }
    g.factory_scene->actor_services->set_reverb_preset(0xD); // 0x0003A1F0, ESI 0xD
    reset_ambient_scene(native_audio_runtime().streams.voices); // 0x00118560
    if (g_512344 != nullptr)
    {
        engine_heap_free(g_512344);
        g_512344 = nullptr;
    }
    g_512348 = 0;
    g_flag_5d20 = 1; // DAT_00545D20: a player who starts in an exit volume must leave it first.
    g_flag_5d28 = 0; // DAT_00545D28.
    g.factory_scene->actor_services->clear_script_tag_tables(); // DAT_0050E148, DAT_0050E16C, DAT_0050E1B0
    g_50e1c4 = 0;
    g_50e228 = 0;
    // 0x000F44B0 (the ghoul intro) runs in SceneFactoryRuntime::end_scene.
    g_545b38 = 0;
    g_545b3c = 0;
    g_545b44[0] = 0;
    g_545bc4[0] = 0;
    g.factory_scene->actor_services->clear_post_models(); // 0x0012A9B0
    effects_clear_voice_slots();
    // 0x00132100 (the scoreboard) runs in SceneFactoryRuntime::end_scene.
    g.factory_scene->actor_services->release_actor_snapshots(); // 0x001163D0
    release_actor_camera_shakes(g.native_resources->registry); // 0x000FAE40
    entity_release_aid_records();
    if (g_51108c != nullptr)
    {
        engine_heap_free(g_51108c);
        g_51108c = nullptr;
    }
    // Xbox 0x00117330
    release_script_event_steps(); // 0x000D0490
    g_flags_ea9c = g_flags_ea9c & 0xFF7FFFFFu;
    g.factory_scene->actor_services->add_profile_scene_times(g_script_30d0c == 1, g_4312d0); // 0x000F2BE0
    g.factory_scene->actor_services->release_challenge_frontend(); // 0x00049FF0
    if ((g_scene_flags->bytes[0] & 8) != 0 || (g_flags_ea9c & 0x200000u) != 0)
    {
        g_game_progress.real_1744 = g_4312d0 + g_game_progress.real_1744; // DAT_004E1744
    }
    std::vector<GhoulyControllerEntry>().swap(g_ghouly_controllers); // DAT_00512B74, 0x000663E0
    release_scene_script_title(g); // 0x001289A0
    std::vector<ScriptCond>().swap(g_script_conds); // DAT_0051234C, 0x000663E0
    release_script_events(); // 0x00116790
    // Clearing DAT_0050E560 lets the next scene's node 0x45 make that scene's ambient tune again.
    if (g_flag_e560 != 0)
    {
        g_flag_e560 = 0;
    }
    g.factory_scene->actor_services->clear_score_awards(); // 0x0012DF90
    // DAT_0050E760 is the script AID 0x00117000 acquires; the host parses the script from the AID store
    // (load_scene_contents) and holds no registry reference for 0x00035C00 to release.
    restore_script_story_bits(g_script_30d0c == 1); // 0x001272B0, EAX 1 when DAT_00430D0C is 1
    g_4312cc = 0.f;
    g_4312d0 = 0.f;
    g_4312d8 = 0;
    g_4312d4 = 0;
    g_sim_scale = 1.f;
    release_actor_camera_effects(); // 0x00048F10
    g.factory_scene->actor_services->release_scene_dialogue(); // 0x00057DC0
    reset_scene_pause(scene_pause_services(g)); // 0x0011CD90
    release_movie_events(native_movie_runtime()); // 0x00106E30
    g_511a10 = 0;
    g.factory_scene->actor_services->clear_scene_light_cones(); // 0x0011AEA0
    g_50eaa0 = nullptr;
    g_50eacc = nullptr;
    g_50eaf8 = nullptr;
    g_50eb24 = nullptr;
    g_50eb50 = nullptr;
    g_50eb7c = nullptr;
    g_50eba8 = nullptr;
    g_50ebd4 = nullptr;
    // 0x00173F30 D3D_KickOffAndWaitForIdle.
    // 0x001593D0 only clears D3D device state, the presenter's.
    g.factory_scene->actor_services->flush_model_releases(); // 0x000F97F0
    // 0x00035D30 / 0x00036180 (unload what nothing references) run in load_scene_contents; 0x00034880 frees the
    // bundle pools, which the host AidStore keeps.
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
void scene_set_state(Game& g, u8* scene, i32 next)
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
            leave_scene_pause(scene_pause_services(g)); // 0x0011CE40
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
            scene_teardown(g, scene);
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
            select_scene_directory_row(reinterpret_cast<const char*>(scene)); // 0x001162D0
            *reinterpret_cast<i32*>(scene + 0x22c) = 4;
            scene_set_audio_level(0.f);
            scene_reset_runtime_parameters(g);
            *reinterpret_cast<i32*>(scene + 0x120) = 1;
            break;
        case 6:
            enter_scene_pause(scene_pause_services(g)); // 0x0011CE10
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
    sync_scene_dialogue_clock(g); // Host mirrors of the frame globals the transition's title reads.
    u8* scene = g_510e58;
    if (*reinterpret_cast<i32*>(scene + 0x110) == 0 &&
        *reinterpret_cast<u32*>(scene + 0x80) == kAidPrefixEd8)
    {
        scene_set_state(g, scene, 3);
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
        scene_set_state(g, scene, queued);
    }
    g.factory_scene->actor_services->refresh_master_pad(); // 0x00109470, EAX the record's +0x230 pad
    tick_scene_pause(scene_pause_services(g)); // 0x0011CFB0
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
            scene_set_state(g, scene, 4);
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
