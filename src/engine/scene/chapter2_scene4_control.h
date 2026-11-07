#pragma once
#include "scene_control_fields.h"
#include "background_morphs.h"
#include "cutscene_scene_runtime.h"
#include "../entities/actor_clock.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
struct SceneControlInstance;
struct SceneControlLists;
// entitySceneControlChapter2Scene4 (factory type 0x95, getter 0x000829A0, vtable 0x005055C0) over the scene control
// (type 3). Unless the scene is restarting (DAT_00430D0C 1, scene flag 8 or DAT_0050EA9C bit 0x200000; then every
// background morph is set playing and the control waits in phase 4), it starts the parameters' cutscene and enters
// phase 1. Phase 1 waits for the cutscene to end, then phase 3 sets the background morphs playing; once the first has
// stopped, phase 4 ends the cutscene and lets the script go on (DAT_0051251C). Phase 2 waits a random half second to
// a second, then enters phase 3; phase 5 disposes of the control. The message is the scene control's 0x00115EE0; the
// +0x14 slot is the empty 0x001628B0 and the destructor restores the reverb (DAT_004E13A8).
namespace chapter2_scene4_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x08> cutscene{};
constexpr u32 kSize = 0x88;
static_assert(retail_fits(kSize, cutscene));
} // namespace chapter2_scene4_parameter
struct Chapter2Scene4ControlState
{
    SceneControlBase base{}; // +0
    u32 phase = 0; // +0x24, 1 to 5
    std::array<u8, 8> unk_28{}; // +0x28, never read or written by translated code
    float wait = 0.f; // +0x30, phase 2's random wait
    float morph_time = 0.f; // +0x34, half a second from phase 3, counting down
};
static_assert(sizeof(Chapter2Scene4ControlState) == 0x38);
struct Chapter2Scene4ControlServices
{
    SceneCutsceneRuntime& cutscenes; // DAT_004E1C34 and the instances.
    const RuntimeActorClock& clock;
    u32& random; // DAT_0054DD7C.
    const i32& restarting; // DAT_00430D0C.
    const u8& scene_flags; // **(DAT_00506C74+8).
    const u32& runtime_flags; // DAT_0050EA9C.
    u32& reverb_index; // DAT_004E13A8.
    i32& script_gate; // DAT_0051251C.
    RuntimeBackgroundMorphs*& morphs; // DAT_0050E388.
};
void chapter2_scene4_control_object_sizes(ObjectSizes& output); // Xbox 0x00082980
bool construct_runtime_chapter2_scene4_control(Chapter2Scene4ControlState& state, std::span<const u8> parameters,
    SceneControlInstance& instance, SceneControlLists& lists,
    Chapter2Scene4ControlServices& services); // Xbox 0x000829F0
void destroy_runtime_chapter2_scene4_control(Chapter2Scene4ControlServices& services); // Xbox 0x00082BB0
void tick_runtime_chapter2_scene4_control(Chapter2Scene4ControlState& state, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services); // Xbox 0x00082A90
void set_runtime_chapter2_scene4_phase(Chapter2Scene4ControlState& state, u8 phase, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services); // Xbox 0x00082BC0, EDI state
}
