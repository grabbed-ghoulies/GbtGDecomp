#pragma once

#include "ghouly_box_controller.h"
#include "ghouly_spawn_scheduler.h"
#include "actor_base_messages.h"
#include "actor_lookup.h"

namespace gbtg
{
struct GhoulyBoxMessageContext
{
    ActorBaseContext& base;
    const std::unordered_map<scene_detail::PulseNode*, Actor*>& actors;
    const std::unordered_map<scene_detail::PulseNode*, GhoulyController*>& controllers;
    const u8* scene_records;
};
void dispatch_ghouly_box_message(GhoulyBoxController& controller, void* message,
    GhoulyBoxMessageContext& context); // Xbox 0x0006BDF0
}
