#include "gbtg/anim.h"
#include "gbtg/engine_internal.h"
#include "gbtg/entity.h"
#include "gbtg/gfx.h"
#include "gbtg/language.h"
#include "gbtg/model.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "../transitions/transition_data.h"

#include <algorithm>
#include <array>
#include <span>
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
#include "cutscene_runtime.h"
#include "../scene/scene_loading.h"
#include "../scripts/script_samples.h"
#include "../scripts/script_vm.h"
#include "../scripts/script_actions.h"

namespace gbtg
{
using namespace scene_detail;


void release_props(Game& g)
{
    // The prop vector keeps its storage, so the next clip's meshes come back
    // at these addresses. The draw cache must not keep the previous clip.
    for (const Game::CutsceneProp& prop : g.cutscene_props)
    {
        gfx_release_mesh(prop.mesh);
    }
}

namespace
{

// The model CPU blob ends in {type, value} records (+0 offset, +4 count).
// 0x000320F0 takes its node mask from the type-1 record.
u32 model_node_animation_mask(std::span<const u8> cpu)
{
    if (cpu.size() < 8)
    {
        return 0;
    }
    const u32 offset = read_u32(cpu, 0);
    const u32 count = read_u32(cpu, 4);
    for (u32 index = 0; index < count; ++index)
    {
        const size_t at = size_t(offset) + size_t(index) * 8;
        if (at + 8 > cpu.size())
        {
            break;
        }
        if (read_u32(cpu, at) == 1)
        {
            return read_u32(cpu, at + 4);
        }
    }
    return 0;
}

// Xbox 0x00030C70 node-state tail. A clip with a state channel (MINA +0x20
// directory, entry +0xC0) passes channel 48 to 0x000320F0 every update.
// Cutscene actors play slot 0 with mix mode 1 (0x0004DC70), so the channel
// is sampled with the clamped frame.
void apply_cutscene_prop_node_states(Game::CutsceneProp& prop, const CutsceneActor& actor, float position)
{
    const std::span<const u8> mina(actor.mina);
    if (mina.size() < 0x28)
    {
        return;
    }
    const u32 directory = read_u32(mina, 0x20);
    if (directory == 0 || read_u32(mina, directory + 0xC0) == 0)
    {
        return;
    }
    MinaPlayback player;
    player.mina = mina;
    player.position = position;
    player.mode = 1;
    AnimationPose pose;
    sample_mina_auxiliary(player, pose.auxiliary);
    const std::array<ModelResourceBlock, 1> blocks{ModelResourceBlock{1, prop.node_mask}};
    ModelAnimationState model;
    model.resource_blocks = blocks;
    std::copy(prop.mesh.node_flags.begin(), prop.mesh.node_flags.end(), model.node_flags.begin());
    static ModelNodeFlagCache cache{}; // DAT_005078D8
    apply_model_node_animation(model, std::bit_cast<u32>(pose.auxiliary[48]), cache);
    std::copy_n(model.node_flags.begin(), prop.mesh.node_flags.size(), prop.mesh.node_flags.begin());
}

} // namespace

namespace scene_detail
{


void sync_cutscene_props(Game& g)
{
    const auto& q = g.cutscenes;
    if (!q.playing || q.items.empty() || !g.scene_bundle)
    {
        release_props(g);
        g.cutscene_props.clear();
        g.cutscene_props_index = static_cast<size_t>(-1);
        return;
    }
    if (g.cutscene_props_index == q.index)
    {
        return;
    }
    g.cutscene_props_index = q.index;
    release_props(g);
    g.cutscene_props.clear();
    const Cutscene& cut = q.items[q.index];
    std::cout << "title actors " << cut.actors.size() << "\n";
    for (size_t i = 0; i < cut.actors.size(); ++i)
    {
        const BundleEntry* entry = find_aid(g, cut.actors[i].model);
        if (!entry || entry->type != ResourceType::Model)
        {
            std::cout << "  actor missing " << cut.actors[i].model << "\n";
            continue;
        }
        try
        {
            Game::CutsceneProp prop;
            prop.mesh = parse_model(*entry);
            prop.actor = i;
            prop.node_mask = model_node_animation_mask(entry->cpu);
            u16 max_bone = 0;
            for (u16 b : prop.mesh.bones)
            {
                max_bone = std::max(max_bone, b);
            }
            std::cout << "  " << cut.actors[i].model << " mina " << cut.actors[i].mina.size() << " joints "
                      << prop.mesh.joints.size() << " skinned " << prop.mesh.bones.size() << " maxbone " << max_bone
                      << "\n";
            g.cutscene_props.push_back(std::move(prop));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  actor fail " << cut.actors[i].model << ": " << ex.what() << "\n";
        }
    }
}


float actor_curve(const CutsceneActor& actor, size_t index, float time, float duration, float fallback)
{
    if (index >= actor.curves.size() || actor.curves[index].keys.empty())
    {
        return fallback;
    }
    return sample_curve(actor.curves[index], time, duration);
}

} // namespace scene_detail


void draw_cutscene_props(Game& g)
{
    sync_cutscene_props(g);
    const auto& q = g.cutscenes;
    if (!q.playing || q.index >= q.items.size())
    {
        return;
    }
    const Cutscene& cut = q.items[q.index];
    for (auto& prop : g.cutscene_props)
    {
        if (prop.actor >= cut.actors.size())
        {
            continue;
        }
        const CutsceneActor& actor = cut.actors[prop.actor];
        const Vec3 origin{actor_curve(actor, 0, q.time, cut.duration, 0.f),
                          actor_curve(actor, 1, q.time, cut.duration, 0.f),
                          actor_curve(actor, 2, q.time, cut.duration, 0.f)};
        const Vec3 scale{actor_curve(actor, 3, q.time, cut.duration, 1.f),
                         actor_curve(actor, 4, q.time, cut.duration, 1.f),
                         actor_curve(actor, 5, q.time, cut.duration, 1.f)};
        apply_cutscene_prop_node_states(prop, actor, cut.duration > 0.f ? q.time / cut.duration : 0.f);
        float bones[128 * 16]{};
        if (!actor.mina.empty() && !prop.mesh.joints.empty() && !prop.mesh.bones.empty() && cut.duration > 0.f)
        {
            const float norm = q.time / cut.duration;
            const auto pose = sample_mina(actor.mina, norm);
            const int bone_n = int(std::min(pose.size(), std::min(prop.mesh.joints.size(), size_t(127))));
            if (bone_n > 0)
            {
                // Xbox 0x0002FAC0. The slot-to-joint bind is ndMtxArray (0x155E60),
                // applied when the mesh uploads the palette.
                skin_matrices(prop.mesh.joints, pose, bones, bone_n);
                gfx_set_bones(bones, bone_n);
            }
            else
            {
                gfx_set_bones(nullptr, 0);
            }
        }
        else
        {
            gfx_set_bones(nullptr, 0);
        }
        movie_apply(prop.mesh);
        gfx_draw_mesh(prop.mesh, origin, scale);
    }
    gfx_set_bones(nullptr, 0);
}

} // namespace gbtg
