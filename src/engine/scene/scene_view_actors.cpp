#include "scene_view_actors.h"
#include "../runtime/link_exchange.h"

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
    if (mirror != 0)
    {
        // The mirror pass (0x0011F310, the plane at DAT_00511168 and the frustum at DAT_005110B8) is not
        // translated.
        throw Error("view passes 0x00121900 with the mirror flag are not translated");
    }
    auto& actors = context.actors;
    for (auto* node = list_next(bodies); node != nullptr; node = list_next(*node))
    {
        Actor& owner = actors.body(*node);
        context.store_mirror(owner, u32(mirror));
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
        context.background_shader_parameters();
        actors.draw(owner, view, 0);
        actors.draw(owner, view, 1);
        context.model_shader_parameters();
    }
    if (context.transition_guard < 1)
    {
        context.blob_shadows();
    }
    context.light_volumes();
    context.decals();
    context.ambient_points();
    context.portals(0);
    const u16 bit = u16(1u << (u32(view) & 0x1F));
    for (auto* node = list_next(bin); node != nullptr; node = list_next(*node))
    {
        const RuntimeSceneBinEntry entry = actors.entry(*node);
        context.store_mirror(entry.actor, u32(mirror));
        if ((entry.visible & bit) != 0)
        {
            actors.draw(entry.actor, view, 0);
            actors.draw(entry.actor, view, 1);
        }
    }
    context.door_shadows();
    context.portals(1);
    return bin;
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
