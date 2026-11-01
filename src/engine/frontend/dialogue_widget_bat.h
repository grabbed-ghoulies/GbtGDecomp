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
    const u8& configuration; // Byte +4 of **(DAT_00506C6C+8); 0x0013B580 tests bit 0x40.
    std::vector<FactoryObject>& objects;
    const std::unordered_map<const u8*, Actor*>& actors; // native factory instance pointers
};
void attach_dialogue_widget_bat(RuntimeDialogueWidget& widget, float scale,
    i32 selected, DialogueWidgetBatFactoryContext& context); // Xbox 0x00142860
}
