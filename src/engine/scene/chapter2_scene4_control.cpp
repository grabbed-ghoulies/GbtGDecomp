#include "chapter2_scene4_control.h"
#include "scene_control_fields.h"
#include "scene_controls.h"

#include <bit>

namespace gbtg
{
namespace
{
namespace parameter = chapter2_scene4_parameter;

RuntimeBackgroundMorphs& morphs_of(Chapter2Scene4ControlServices& services)
{
    if (services.morphs == nullptr)
    {
        throw Error("scene 2a4 control reads the DAT_0050E388 morph pool before the scene start (0x00114180) "
            "makes it");
    }
    return *services.morphs;
}

// Counts a timer down by the frame step, stopping at zero. Returns true when it reached zero this tick.
bool count_down(float& timer, float step)
{
    if (!(timer > 0.f))
    {
        return false;
    }
    const float left = timer - step;
    timer = left;
    if (0.f < left)
    {
        return false;
    }
    timer = 0.f;
    return true;
}
}

// Xbox 0x00082980
void chapter2_scene4_control_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(Chapter2Scene4ControlState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000829F0
bool construct_runtime_chapter2_scene4_control(Chapter2Scene4ControlState& state, std::span<const u8> parameters,
    SceneControlInstance& instance, SceneControlLists& lists, Chapter2Scene4ControlServices& services)
{
    if (services.restarting == 1 || (services.scene_flags & 8) != 0 || (services.runtime_flags & 0x200000) != 0)
    {
        // The inlined 0x00114050 with 1.
        set_runtime_background_morphs_playing(services.morphs, 1);
        state.phase = 4u;
        return true;
    }
    start_scene_cutscene(field_data(parameters, parameter::cutscene), 0, 1, services.cutscenes); // 0x0004EDA0
    set_runtime_chapter2_scene4_phase(state, 1, instance, lists, services);
    return true;
}

// Xbox 0x00082BB0
void destroy_runtime_chapter2_scene4_control(Chapter2Scene4ControlServices& services)
{
    services.reverb_index = 0;
}

// Xbox 0x00082A90
void tick_runtime_chapter2_scene4_control(Chapter2Scene4ControlState& state, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services)
{
    const u32 phase = state.phase;
    if (phase == 1)
    {
        const auto& cutscenes = services.cutscenes;
        if (cutscenes.index > -1)
        {
            const RuntimeCutsceneInstance& current = *cutscenes.instances[size_t(cutscenes.index)];
            if (!(current.time < current.duration))
            {
                set_runtime_chapter2_scene4_phase(state, 3, instance, lists, services);
            }
        }
    }
    else if (phase == 2)
    {
        if (count_down(state.wait, runtime_frame_step(services.clock)))
        {
            set_runtime_chapter2_scene4_phase(state, 3, instance, lists, services);
        }
    }
    else if (phase == 3)
    {
        auto& morphs = morphs_of(services);
        if (morphs.records.empty())
        {
            throw Error("scene 2a4 control reads the first morph of an empty DAT_0050E388 pool");
        }
        if (morphs.records.front().playing == 0)
        {
            set_runtime_chapter2_scene4_phase(state, 4, instance, lists, services);
        }
        else
        {
            count_down(state.morph_time, runtime_frame_step(services.clock));
        }
    }
    state.base.elapsed = state.base.elapsed + runtime_frame_step(services.clock);
}

// Xbox 0x00082BC0, EDI state
void set_runtime_chapter2_scene4_phase(Chapter2Scene4ControlState& state, u8 phase, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services)
{
    switch (phase)
    {
    case 1:
        services.reverb_index = 8;
        break;
    case 2:
    {
        services.random = services.random * 0x19660Du + 0x3C6EF35Fu;
        const float fraction = std::bit_cast<float>((services.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
        state.wait = fraction * 0.5f + 0.5f; // DAT_0032B0F8, DAT_0032B0F4
        break;
    }
    case 3:
        set_runtime_background_morphs_playing(services.morphs, 1); // 0x00114050, BL 1
        services.reverb_index = 0;
        state.morph_time = 0.5f; // DAT_0032B0F4
        break;
    case 4:
        if (services.cutscenes.index > 0)
        {
            pop_scene_cutscene(services.cutscenes); // 0x0004EAD0
        }
        else
        {
            release_scene_cutscenes(services.cutscenes); // 0x0004ECE0
        }
        ++services.script_gate;
        break;
    case 5:
        dispose_runtime_scene_control(instance, lists); // 0x00116140, EAX state
        break;
    default:
        break;
    }
    state.phase = u32(phase);
}
}
