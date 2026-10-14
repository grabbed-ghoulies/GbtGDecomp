#include "scene_actor_removal.h"

namespace gbtg
{
// Xbox 0x000F4970
void remove_runtime_notice_actor(const Actor& actor, RuntimeNoticeRemoval& context)
{
    auto& notices = context.notices;
    if (notices.count != 0 && 0 < notices.count)
    {
        i32 index = 0;
        do
        {
            if (&actor == notices.queued[size_t(index)].actor)
            {
                ++index;
                while (index < notices.count)
                {
                    notices.queued[size_t(index) - 1] = notices.queued[size_t(index)];
                    ++index;
                }
                --notices.count;
                break;
            }
            ++index;
        }
        while (index < notices.count);
    }
    if (notices.pending == 1)
    {
        if (notices.first == &actor)
        {
            context.stop();
            context.next();
            return;
        }
    }
    else if (notices.pending == 2 && notices.first == &actor)
    {
        notices.first = nullptr;
        context.finish();
        return;
    }
}

// Xbox 0x001232D0
void remove_runtime_retired_actors(RuntimeSceneActorRemoval& context)
{
    while (context.retired.next != nullptr)
    {
        auto& node = *static_cast<entity_detail::ListHead*>(context.retired.next);
        Actor& actor = context.actor(node);
        const u32 words[2]{0x7C, context.address(actor)};
        context.notify_actors(actor);
        context.notify_controls(words);
        context.detach_effects(actor);
        remove_runtime_notice_actor(actor, context.notices);
        context.unscare(actor);
        if (node.prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node.prev)->next = node.next;
        }
        if (node.next != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node.next)->prev = node.prev;
        }
        node.prev = nullptr;
        node.next = nullptr;
        context.release(actor);
    }
}
}
