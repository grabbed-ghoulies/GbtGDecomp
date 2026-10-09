#include "dialogue_widget_state.h"
#include "../entities/actor_render_registration.h"
#include "gbtg/entity.h"

namespace gbtg
{
// Xbox 0x00142070
void destroy_dialogue_widget(RuntimeDialogueWidget* widget,
    AnimationResourceRegistry& resources, GhoulySpawnPool& text_pools)
{
    if (widget->owns_text_resource != 0 && widget->text_resource.data() != nullptr)
    {
        release_animation_resource(resources, widget->text_resource);
        widget->text_resource = {};
    }
    i32 remaining = 10;
    i32 index = 0;
    do
    {
        if (widget->icons[index].data() != nullptr)
        {
            release_animation_resource(resources, widget->icons[index]);
            widget->icons[index] = {};
        }
        ++index;
        --remaining;
    }
    while (remaining != 0);
    if (widget->pool != nullptr)
    {
        release_dialogue_text_pool(widget->pool, text_pools);
        widget->pool = nullptr;
    }
    Actor* bat = widget->bat;
    widget->old_set = nullptr;
    widget->current_set = nullptr;
    widget->requested_set = nullptr;
    widget->count = 0;
    if (bat != nullptr)
    {
        activate_actor_bind_children(bat->link->child, *bat);
        ActorRenderRegistration& registration = *bat->render_registration;
        if (registration.child.prev == nullptr)
        {
            registration.message(registration, 0x80);
            if (registration.child.prev == nullptr)
            {
                register_actor_child_list(registration);
            }
        }
        widget->bat = nullptr;
    }
    engine_heap_free(widget);
}
}
