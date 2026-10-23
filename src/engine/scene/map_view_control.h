#pragma once
#include "gbtg/object_factory.h"
#include "../transitions/native_book.h"

#include <functional>
#include <span>

namespace gbtg
{
struct RuntimeActorPauseGroups;
struct SceneControlInstance;
struct SceneControlLists;
// entitySceneControlMapView (factory type 0x9E, getter 0x00083EA0, vtable 0x00505638) over the scene control (type 3):
// the house map in the book. It finds the current room in the map rooms (0x00427628), opens the book on that
// room's clip with the map from the room's spot, and pauses the actors while it shows. The book holds at its first
// page until it has finished, then closes the map and runs on; once it finishes again the control is disposed of.
// Past 0.7 of the clip the map is kept for the dialogue (DAT_004EE2B4). +0x14 is the empty 0x001628B0 and the
// message is the scene control's 0x00115EE0.
struct MapViewControlState
{
    RuntimeTransitionBook* book = nullptr; // +0x24, 0x00048260's book.
    u32 phase = 0; // +0x28: 0 holding at the map, 1 closing.
    u32 retained = 0; // +0x2C, set once the map was kept.
};
// One 0x88-byte row of the map rooms at 0x00427628: the room's name, its spot's name and the book clip.
struct MapViewRoom
{
    std::array<char, 0x80> room; // +0
    u32 spot; // +0x80, an XBE string address.
    i32 clip; // +0x84
};
static_assert(sizeof(MapViewRoom) == 0x88);
struct MapViewControlServices
{
    TransitionBookServices& books;
    std::span<const MapViewRoom, 6> rooms; // 0x00427628.
    const char* room; // DAT_00510E58, the current room.
    std::function<const char*(u32 address)> image_string; // An XBE string, or null for 0.
    std::function<RuntimeActorPauseGroups&()> pause_groups; // 0x00117400's DAT_00511B38 walk.
    u32& map_retained; // DAT_004EE2B4.
};
void map_view_control_object_sizes(ObjectSizes& output); // Xbox 0x00083E80
bool construct_runtime_map_view_control(MapViewControlState& control,
    MapViewControlServices& services); // Xbox 0x00083EF0
void destroy_runtime_map_view_control(MapViewControlState& control,
    MapViewControlServices& services); // Xbox 0x00083FC0
void tick_runtime_map_view_control(MapViewControlState& control, SceneControlInstance& instance,
    SceneControlLists& lists, MapViewControlServices& services); // Xbox 0x00083FE0
}
