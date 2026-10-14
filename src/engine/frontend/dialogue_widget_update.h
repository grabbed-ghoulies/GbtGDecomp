#pragma once
#include "dialogue_widget_state.h"
#include "dialogue_input.h"
#include "../entities/button_bat_update.h"
#include "../audio/event_voices.h"
#include <functional>
#include <unordered_map>

namespace gbtg
{
struct DialogueWidgetBatView
{
    ButtonBatScreen& bat;
    ActorFrameState& frame;
    ActorFrameContext& context;
};
struct DialogueWidgetUpdateContext
{
    RuntimeDialogueWidgetContext& widget;
    RuntimeDialogueInputState& input;
    AudioEventVoices& voices;
    const std::function<DialogueWidgetBatView(Actor*)>& bats; // Native vtable +0x10 views of a widget bat.
    const std::unordered_map<u32, std::function<void()>>& callbacks; // native code pointers from row +0x84
    float freeze; // DAT_004312DC
    float dt; // DAT_005096C0
    float scene_scale; // DAT_004312C4
};
u32 update_dialogue_widget(RuntimeDialogueWidget& widget,
    const RuntimeDialogueInputDevice* device,
    DialogueWidgetUpdateContext& context); // Xbox 0x00142460
}
