#pragma once

#include "gbtg/entity.h"
#include "../camera/display_view.h"
#include "../entities/entity_types.h"
#include <functional>

namespace gbtg
{
// One record of a view bin: actor +0x124 + 0xC * (view + 4 * pass). Its +8 float is the view distance.
struct RuntimeSceneBinEntry
{
    Actor& actor;
    float& distance; // Record +8.
    u16& visible; // Actor +0x194, one bit per view.
};
struct RuntimeSceneViewActors
{
    const entity_detail::DisplayView& view; // DAT_005077A8.
    std::function<RuntimeSceneBinEntry(entity_detail::ListHead&)> entry;
    std::function<Actor&(entity_detail::ListHead&)> body; // Owner of a +0x934 list node.
    std::function<u32(Actor&, i32 pass, i32 view)> visible; // Actual vtable +0x18.
    std::function<void(Actor&, i32 view, i32 pass)> draw; // Actual vtable +0x14.
};
// Orders a view bin by descending squared distance from the view (stack bin, ECX view, EDX pass).
void sort_runtime_view_bin(entity_detail::ListHead& bin, RuntimeSceneViewActors& context); // Xbox 0x00121730
// Sets or clears each bin actor's view bit from vtable +0x18 (EAX bin).
void classify_runtime_view_bin(entity_detail::ListHead& bin, i32 pass, i32 view,
    RuntimeSceneViewActors& context); // Xbox 0x00121880
// Draws every +0x934 list owner, then the bin actors visible in this view, through vtable +0x14 with pass 2
// (EAX bodies, ECX bin, EBX view).
void draw_runtime_view_actors(entity_detail::ListHead& bodies, entity_detail::ListHead* bin, i32 view, i32 pass,
    RuntimeSceneViewActors& context); // Xbox 0x00121D40
}
