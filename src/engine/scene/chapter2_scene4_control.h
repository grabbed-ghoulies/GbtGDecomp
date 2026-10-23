#pragma once
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
namespace chapter2_scene4_field
{
constexpr RetailField<u32, 0x24> phase{}; // 1 to 5
constexpr RetailField<float, 0x30> wait{}; // Phase 2's random wait.
constexpr RetailField<float, 0x34> morph_time{}; // Half a second from phase 3, counting down.
constexpr u32 kSize = 0x38;
static_assert(retail_fits(kSize, morph_time));
} // namespace chapter2_scene4_field
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
bool construct_runtime_chapter2_scene4_control(std::span<u8> state, std::span<const u8> parameters,
    SceneControlInstance& instance, SceneControlLists& lists,
    Chapter2Scene4ControlServices& services); // Xbox 0x000829F0
void destroy_runtime_chapter2_scene4_control(Chapter2Scene4ControlServices& services); // Xbox 0x00082BB0
void tick_runtime_chapter2_scene4_control(std::span<u8> state, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services); // Xbox 0x00082A90
void set_runtime_chapter2_scene4_phase(std::span<u8> state, u8 phase, SceneControlInstance& instance,
    SceneControlLists& lists, Chapter2Scene4ControlServices& services); // Xbox 0x00082BC0, EDI state
}
