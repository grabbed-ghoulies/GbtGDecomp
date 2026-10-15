#pragma once

#include "gbtg/entity.h"
#include "../camera/display_view.h"
#include "../entities/entity_types.h"
#include <functional>
#include <span>

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
struct RuntimeSceneMirrors;
// The world passes of 0x00121900 around the actor draws.
struct RuntimeSceneViewPasses
{
    RuntimeSceneViewActors& actors;
    // Actor +0x204 model: the +0x15C word, the type-1 word of its resource header and 0x000F96F0 (AL value).
    std::function<void(Actor&, u32 mirror)> store_mirror;
    std::function<u32(Actor&)> model_flags;
    std::function<void(Actor&, u8 slot, u8 value)> node_state;
    std::function<void()> background_shader_parameters; // 0x001572C0.
    std::function<void()> model_shader_parameters; // 0x001572B0.
    const i32& transition_guard; // DAT_0050484C.
    std::function<void()> blob_shadows; // 0x00119AD0.
    std::function<void()> light_volumes; // 0x000FE960.
    std::function<void()> decals; // 0x00116460.
    std::function<void()> ambient_points; // 0x001237E0.
    std::function<void(u32)> portals; // 0x00115A80.
    std::function<void()> door_shadows; // 0x0005B020.
    // The mirror pass (flag 1): the camera side of the mirror plane (0x0011F310), an actor's view radius
    // (0x0003D860) and the mirror state 0x0011F690 left (plane DAT_00511168, reflected camera DAT_00511178,
    // sphere test DAT_00511160 over the frustum DAT_005110B8).
    std::function<u32(const Vec3&)> mirror_side;
    std::function<float(Actor&)> radius;
    RuntimeSceneMirrors* mirrors = nullptr;
};
// The +0x934 owners' passes 0 and 1, the world helpers, then passes 0 and 1 of every bin actor visible in this
// view (EAX bodies; stack bin, view, mirror). Returns the bin.
entity_detail::ListHead& draw_runtime_view_passes(entity_detail::ListHead& bodies, entity_detail::ListHead& bin,
    i32 view, i32 mirror, RuntimeSceneViewPasses& context); // Xbox 0x00121900
// Orders a view bin by descending squared distance from the view (stack bin, ECX view, EDX pass).
void sort_runtime_view_bin(entity_detail::ListHead& bin, RuntimeSceneViewActors& context); // Xbox 0x00121730
// Sets or clears each bin actor's view bit from vtable +0x18 (EAX bin).
void classify_runtime_view_bin(entity_detail::ListHead& bin, i32 pass, i32 view,
    RuntimeSceneViewActors& context); // Xbox 0x00121880
// Each +0x934 owner's vtable +0x38, then each record's of the view's bin (EBX view); `slot` makes the call for
// classes whose +0x38 is set.
void refresh_runtime_view_actors(entity_detail::ListHead& bodies, entity_detail::ListHead& bin, i32 view,
    const std::function<void(Actor&, i32)>& slot, RuntimeSceneViewActors& context); // Xbox 0x00121DC0
// One view's bin passes (ESI mirror, the view from DAT_005077A8): DAT_0051120C, 0x0012AB10 for the main pass,
// then 0x00121730, 0x00121880, 0x00121900 and 0x00121D40 over bin view + 4 * mirror.
struct RuntimeSceneViewBins
{
    RuntimeSceneViewPasses& passes;
    std::span<entity_detail::ListHead, 8> bins; // DAT_00511AA8.
    entity_detail::ListHead& bodies; // DAT_00511B1C.
    i32 view;
    u32& mirror_pass; // DAT_0051120C.
    std::function<void()> post_models; // 0x0012AB10.
};
void draw_runtime_view_bins(i32 mirror, RuntimeSceneViewBins& context); // Xbox 0x00120E80
// Draws every +0x934 list owner, then the bin actors visible in this view, through vtable +0x14 with pass 2
// (EAX bodies, ECX bin, EBX view).
void draw_runtime_view_actors(entity_detail::ListHead& bodies, entity_detail::ListHead* bin, i32 view, i32 pass,
    RuntimeSceneViewActors& context); // Xbox 0x00121D40
}
