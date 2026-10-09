#include "dialogue_widget_bat.h"
#include "../entities/entity.h"

namespace gbtg
{
// Xbox 0x00142860
void attach_dialogue_widget_bat(RuntimeDialogueWidget& widget, float scale,
    i32 selected, DialogueWidgetBatFactoryContext& context)
{
    if (widget.bat == nullptr)
    {
        auto object = create_dialogue_button_bat(context.factory, context.resources,
            context.configuration, widget.positions[widget.selected], widget.icon_scale, -1.f);
        Actor* bat = context.actors.at(object.bytes.get());
        context.objects.push_back(std::move(object));
        widget.bat = bat;
        RuntimeActorLink& link = bat->render_registration->view;
        if (link.previous != nullptr)
        {
            // Resolve the native sentinel separately from actor link records.
            auto& world = entity_detail::g_world;
            if (link.previous == reinterpret_cast<RuntimeActorLink*>(&world.view))
            {
                world.view.next = link.next;
            }
            else
            {
                link.previous->next = link.next;
            }
        }
        if (link.next != nullptr)
        {
            link.next->previous = link.previous;
        }
        link.previous = nullptr;
        link.next = nullptr;
    }
    widget.bat_scale = scale;
    widget.selected = selected;
}
}
