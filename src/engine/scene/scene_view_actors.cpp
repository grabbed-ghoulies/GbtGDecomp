#include "scene_view_actors.h"
#include "../runtime/link_exchange.h"
#include "scene_mirror.h"
#include "../camera/view_frustum.h"
#include "../entities/actor_transform.h"

namespace gbtg
{
namespace
{
float view_distance(const entity_detail::DisplayView& view, const Actor& actor)
{
    const Vec3 position = actor_world_position(actor);
    const float dz = view.position.z - position.z;
    const float dy = view.position.y - position.y;
    const float dx = view.position.x - position.x;
    return (dz * dz + dy * dy) + dx * dx;
}

entity_detail::ListHead* list_next(const entity_detail::ListHead& node)
{
    return static_cast<entity_detail::ListHead*>(node.next);
}

entity_detail::ListHead* list_previous(const entity_detail::ListHead& node)
{
    return static_cast<entity_detail::ListHead*>(node.prev);
}
}

// Xbox 0x00121730
void sort_runtime_view_bin(entity_detail::ListHead& bin, RuntimeSceneViewActors& context)
{
    auto* first = list_next(bin);
    if (first == nullptr)
    {
        return;
    }
    {
        const RuntimeSceneBinEntry entry = context.entry(*first);
        entry.distance = view_distance(context.view, entry.actor);
    }
    auto* current = list_next(*first);
    while (current != nullptr)
    {
        const RuntimeSceneBinEntry entry = context.entry(*current);
        entry.distance = view_distance(context.view, entry.actor);
        auto* previous = list_previous(*current);
        auto* following = list_next(*current);
        while (previous->prev != nullptr && !(context.entry(*previous).distance >= entry.distance))
        {
            auto* before = list_previous(*previous);
            exchange_runtime_list_heads(*current, *previous);
            previous = before;
        }
        current = following;
    }
}

// Xbox 0x00121880
void classify_runtime_view_bin(entity_detail::ListHead& bin, i32 pass, i32 view, RuntimeSceneViewActors& context)
{
    const u16 bit = u16(1u << (u32(view) & 0x1F));
    for (auto* node = list_next(bin); node != nullptr; node = list_next(*node))
    {
        const RuntimeSceneBinEntry entry = context.entry(*node);
        if (context.visible(entry.actor, pass, view) != 0)
        {
            entry.visible = u16(entry.visible | bit);
        }
        else
        {
            entry.visible = u16(entry.visible & ~bit);
        }
    }
}

// Xbox 0x00121900
entity_detail::ListHead& draw_runtime_view_passes(entity_detail::ListHead& bodies, entity_detail::ListHead& bin,
    i32 view, i32 mirror, RuntimeSceneViewPasses& context)
{
    auto& actors = context.actors;
    if (mirror == 1 && (context.mirrors == nullptr || !context.mirror_side || !context.radius))
    {
        throw Error("view passes 0x00121900 with the mirror flag have no mirror state");
    }
    for (auto* node = list_next(bodies); node != nullptr; node = list_next(*node))
    {
        Actor& owner = actors.body(*node);
        context.store_mirror(owner, u32(mirror));
        if (mirror == 1)
        {
            const u32 side = context.mirror_side(actors.view.position);
            if ((context.model_flags(owner) & 0x40000000u) != 0)
            {
                context.node_state(owner, 0x1E, side == 0 ? 1 : 0);
            }
            if ((context.model_flags(owner) & 0x20000000u) != 0)
            {
                context.node_state(owner, 0x1D, u8(side));
            }
            if ((context.model_flags(owner) & 0x10000000u) != 0)
            {
                context.node_state(owner, 0x1C, 1);
            }
        }
        else
        {
            if ((context.model_flags(owner) & 0x10000000u) != 0)
            {
                context.node_state(owner, 0x1C, 0);
            }
            if ((context.model_flags(owner) & 0x20000000u) != 0)
            {
                context.node_state(owner, 0x1D, 0);
            }
            if ((context.model_flags(owner) & 0x40000000u) != 0)
            {
                context.node_state(owner, 0x1E, 0);
            }
        }
        context.background_shader_parameters();
        if (mirror == 0)
        {
            actors.draw(owner, view, 0);
        }
        actors.draw(owner, view, 1);
        context.model_shader_parameters();
    }
    if (mirror == 0)
    {
        if (context.transition_guard < 1)
        {
            context.blob_shadows();
        }
        context.light_volumes();
        context.decals();
        context.ambient_points();
        context.portals(0);
    }
    const u16 bit = u16(1u << (u32(view) & 0x1F));
    for (auto* node = list_next(bin); node != nullptr; node = list_next(*node))
    {
        const RuntimeSceneBinEntry entry = actors.entry(*node);
        context.store_mirror(entry.actor, u32(mirror));
        if ((entry.visible & bit) == 0)
        {
            continue;
        }
        if (mirror == 1)
        {
            const float radius = context.radius(entry.actor);
            const Vec3 position = actor_world_position(entry.actor);
            auto& mirrors = *context.mirrors;
            const auto& plane = mirrors.plane_equation;
            const float distance = position.z * plane[2] + position.y * plane[1] + position.x * plane[0] + plane[3];
            const float camera = (mirrors.camera.z * plane[2] + mirrors.camera.y * plane[1]
                + mirrors.camera.x * plane[0] + plane[3]) * distance;
            if (0.f < camera && radius * radius <= distance * distance)
            {
                continue;
            }
            if (mirrors.sphere_test != 0
                && classify_runtime_sphere_frustum(mirrors.frustum, position, radius, mirrors.plane) == 0)
            {
                continue;
            }
        }
        else if (mirror == 0)
        {
            actors.draw(entry.actor, view, 0);
        }
        actors.draw(entry.actor, view, 1);
    }
    if (mirror == 0)
    {
        context.door_shadows();
        context.portals(1);
    }
    return bin;
}

// Xbox 0x00121DC0
void refresh_runtime_view_actors(entity_detail::ListHead& bodies, entity_detail::ListHead& bin, i32 view,
    const std::function<void(Actor&, i32)>& slot, RuntimeSceneViewActors& context)
{
    for (auto* node = list_next(bodies); node != nullptr; node = list_next(*node))
    {
        slot(context.body(*node), view);
    }
    for (auto* node = list_next(bin); node != nullptr; node = list_next(*node))
    {
        slot(context.entry(*node).actor, view);
    }
}

// Xbox 0x00120E80
void draw_runtime_view_bins(i32 mirror, RuntimeSceneViewBins& context)
{
    context.mirror_pass = u32(mirror);
    if (mirror == 0)
    {
        context.post_models();
    }
    auto& bin = context.bins[size_t(context.view + mirror * 4)];
    auto& actors = context.passes.actors;
    sort_runtime_view_bin(bin, actors);
    classify_runtime_view_bin(bin, mirror, context.view, actors);
    auto& drawn = draw_runtime_view_passes(context.bodies, bin, context.view, mirror, context.passes);
    draw_runtime_view_actors(context.bodies, &drawn, context.view, mirror, actors);
}

// Xbox 0x00121D40
void draw_runtime_view_actors(entity_detail::ListHead& bodies, entity_detail::ListHead* bin, i32 view, i32 pass,
    RuntimeSceneViewActors& context)
{
    (void)pass; // Selects the bin record offset; the native entry callback resolves the owner directly.
    for (auto* node = list_next(bodies); node != nullptr; node = list_next(*node))
    {
        context.draw(context.body(*node), view, 2);
    }
    if (bin == nullptr)
    {
        return;
    }
    const u16 bit = u16(1u << (u32(view) & 0x1F));
    for (auto* node = list_next(*bin); node != nullptr; node = list_next(*node))
    {
        const RuntimeSceneBinEntry entry = context.entry(*node);
        if ((entry.visible & bit) != 0)
        {
            context.draw(entry.actor, view, 2);
        }
    }
}
}
