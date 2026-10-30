#include "actor_loading.h"
#include "../entities/entity.h"

namespace gbtg
{
std::function<std::vector<RuntimeLoadingActor>()> g_loading_actors;

std::vector<RuntimeLoadingActor> runtime_loading_actors()
{
    if (!g_loading_actors)
    {
        throw Error("DAT_00511B38 is walked with no scene bound");
    }
    return g_loading_actors();
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
    const std::vector<RuntimeLoadingActor> actors = runtime_loading_actors();
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
