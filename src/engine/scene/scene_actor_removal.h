#pragma once
#include "gbtg/entity.h"
#include "../entities/entity_types.h"
#include "../entities/strategy_target_selection.h"
#include <functional>

namespace gbtg
{
struct RuntimeNoticeRemoval
{
    RuntimeTutorialNotices& notices;
    std::function<void()> stop; // 0x000F4C10.
    std::function<u32()> next; // 0x000F4CC0.
    std::function<void()> finish; // 0x000F4B90.
};
// Drops a removed actor (ESI) from the first-sighting notice queue, and ends its notice when it is showing.
void remove_runtime_notice_actor(const Actor& actor, RuntimeNoticeRemoval& context); // Xbox 0x000F4970

struct RuntimeSceneActorRemoval
{
    entity_detail::ListHead& retired; // DAT_00511B54; its next, DAT_00511B58, is the first actor +0x908 node.
    std::function<Actor&(entity_detail::ListHead&)> actor; // Owner of a +0x908 node.
    std::function<void(const Actor&)> notify_actors; // Message {0x7C, actor} to each DAT_00511AA4 actor (+0xC).
    std::function<void(std::span<const u32>)> notify_controls; // The same words to each DAT_0050E438 control.
    std::function<void(const Actor&)> detach_effects; // 0x000ED5B0.
    RuntimeNoticeRemoval& notices; // 0x000F4970.
    std::function<void(const Actor&)> unscare; // 0x00123E00, EDX actor.
    std::function<u32(const Actor&)> address; // Retail actor address for the message word.
    std::function<void(Actor&)> release; // 0x001075C0, EAX actor.
};
// Each retired actor: message 0x7C to the actors and scene controls, effect and notice detach, the scare list,
// then its +0x908 node leaves the list and the object is released.
void remove_runtime_retired_actors(RuntimeSceneActorRemoval& context); // Xbox 0x001232D0
}
