#pragma once
#include "dialogue_widget_state.h"
#include "gbtg/actor_spawn_factory.h"
#include "../entities/actor_render_registration.h"
#include <unordered_map>

namespace gbtg
{
struct DialogueWidgetBatFactoryContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    u8 configuration;
    std::vector<FactoryObject>& objects;
    const std::unordered_map<const u8*, Actor*>& actors; // native factory instance pointers
};
void attach_dialogue_widget_bat(RuntimeDialogueWidget& widget, float scale,
    i32 selected, DialogueWidgetBatFactoryContext& context); // Xbox 0x00142860
}
