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
