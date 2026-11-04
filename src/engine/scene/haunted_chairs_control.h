#pragma once
#include "scene_control_fields.h"
#include "cutscene_scene_runtime.h"
#include "../entities/actor_render_registration.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"

#include <functional>

namespace gbtg
{
struct SceneControlInstance;
struct SceneControlLists;
// entitySceneControlChapter1Scene8 (factory type 0x93, getter 0x000824F0, vtable 0x005055A8) over the scene control
// (type 3): the gaming room's haunted chairs and their flip-off cutscene. Message 0x7D gathers up to nine chairs
// (class 0x89 on the DAT_00511B24 list) with their row in the cutscene (UNK_00301B30). Message 1 arms the control and
// 0x7E (while armed) hides each chair that stands free and drops the other chairs' cutscene rows. When the cutscene
// has ended (DAT_004E1C34 below 0) the tick shows the free chairs again at their markers, then the control retires.
// The destroy and +0x14 slots are the empty 0x001628B0.
struct HauntedChairsControlState
{
    std::array<Actor*, 9> chairs{}; // +0x24, the chairs' actors
    std::array<i32, 9> rows{}; // +0x48, each chair's cutscene row, -1 for none
    i32 count = 0; // +0x6C
    u32 phase = 0; // +0x70, 1 armed (message 1), 2 chairs hidden (0x7E), 3 shown again
};
struct HauntedChairsControlServices
{
    SceneCutsceneRuntime& cutscenes; // DAT_004E1C34, DAT_004E1C38 and the instances' actor rows.
    std::span<const u32, 9> chair_ids; // UNK_00301B30, matched against actor +0x1FC.
    const void* const& markers; // DAT_0051108C.
    std::function<u32(const Actor&)> actor_address;
    std::function<std::span<u8>(Actor&)> actor_bytes; // +0x93C, +0xAC4, +0xAD0, +0xAAC, +0x1FC, +0x1FE.
    std::function<ActorRenderRegistration&(Actor&)> render;
};
void haunted_chairs_control_object_sizes(ObjectSizes& output); // Xbox 0x000824D0
bool construct_runtime_haunted_chairs_control(HauntedChairsControlState& control); // Xbox 0x00082540
void message_runtime_haunted_chairs_control(std::span<const u32> message,
    HauntedChairsControlState& control, SceneControlInstance& instance, SceneControlLists& lists,
    HauntedChairsControlServices& services); // Xbox 0x00082560
void tick_runtime_haunted_chairs_control(SceneControlBase& base, HauntedChairsControlState& control,
    SceneControlInstance& instance, SceneControlLists& lists,
    HauntedChairsControlServices& services); // Xbox 0x00082680
void gather_runtime_haunted_chairs(HauntedChairsControlState& control,
    HauntedChairsControlServices& services); // Xbox 0x00082710, ECX state
void hide_runtime_haunted_chairs(const HauntedChairsControlState& control,
    HauntedChairsControlServices& services); // Xbox 0x00082780
void show_runtime_haunted_chairs(const HauntedChairsControlState& control,
    HauntedChairsControlServices& services); // Xbox 0x00082810
}
