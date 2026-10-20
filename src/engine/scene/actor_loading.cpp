#include "actor_loading.h"
#include "../camera/camera.h"
#include "../entities/entity.h"

namespace gbtg
{
std::function<std::vector<RuntimeLoadingActor>()> g_loading_actors;

namespace
{
// The host range links' view of DAT_00511B38, for runs without a native scene.
std::vector<RuntimeLoadingActor> host_loading_actors()
{
    std::vector<RuntimeLoadingActor> actors;
    for (const scene_detail::RangeLink* link = scene_detail::g_range_links; link != nullptr; link = link->next)
    {
        const scene_detail::CamFocus& actor = *link->host;
        const EntityMaskSpan* span = actor.span_980;
        u16 state = 0;
        if (span != nullptr)
        {
            const u16* element = nullptr;
            if (span->end != span->mark)
            {
                element = reinterpret_cast<const u16*>(span->data + size_t(span->index) * span->stride);
            }
            // Retail also dereferences a null element when end == mark.
            state = *element;
        }
        actors.push_back({state, u32(actor.at_984), u32(actor.at_98c)});
    }
    return actors;
}
}

// Xbox 0x001412F0
bool actor_loading_complete()
{
    int unfinished = 0;
    for (const ActorLoadRecord* record = g_side_list.next; record != nullptr; record = record->next)
    {
        if ((record->flags_2c & 1) == 0)
        {
            ++unfinished;
        }
    }
    if (unfinished != 0)
    {
        return false;
    }
    if (entity_detail::g_world.view.next != nullptr)
    {
        return false;
    }
    const std::vector<RuntimeLoadingActor> actors = g_loading_actors ? g_loading_actors() : host_loading_actors();
    for (const RuntimeLoadingActor& actor : actors)
    {
        switch (actor.goal)
        {
        case 0:
        case 7:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x13:
        case 0x14:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1E:
        case 0x24:
            break;
        default:
            if (actor.word_98c == 0 && actor.word_984 == 0)
            {
                return false;
            }
        }
    }
    return true;
}
}
