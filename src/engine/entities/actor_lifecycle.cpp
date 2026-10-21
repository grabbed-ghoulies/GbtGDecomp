#include "actor_lifecycle.h"
#include "actor_bind_tree.h"
#include "entity.h"

namespace gbtg
{
// Xbox 0x0003DE90
void release_actor_frame_attachments(ActorFrameAttachment* head,
    ActorFrameReleaseContext& context)
{
    while (head != nullptr)
    {
        ActorFrameAttachment* next = head->next;
        FactoryObject* owned = context.object_of(context.objects.at(head));
        if (owned == nullptr)
        {
            throw Error("0x0003DE90 releases a frame object the scene does not hold");
        }
        FactoryObject& object = *owned;
        const ActorReferenceMessage message{0x14, object.bytes.get()};
        context.message(message);
        release_factory_object(context.factory, object);
        head = next;
    }
}

// Xbox 0x0003EBA0
void release_runtime_actor(ActorReleaseContext& context)
{
    auto& state = context.state;
    auto& actor = state.actor;
    auto& startup = context.startup;
    if (actor.bind != nullptr)
    {
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
    }
    if (context.unlink_active_node)
    {
        context.unlink_active_node();
    }
    unlink_actor_view(context.render.view);
    unregister_actor_render_bins(context.render);
    auto& child = context.render.child;
    if (child.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(child.prev)->next = child.next;
    }
    if (child.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(child.next)->prev = child.prev;
    }
    child.prev = nullptr;
    child.next = nullptr;
    unlink_actor_bind_node(*actor.link);
    release_actor_frame_attachments(context.frame_head, context.frames);
    if (state.resource.resource.data() != nullptr)
    {
        release_animation_resource(startup.registry, state.resource.resource);
        state.resource.resource = {};
    }
    state.resource.name[0] = 0;
    if (context.allocation_8fc != nullptr)
    {
        engine_heap_free(context.allocation_8fc);
        context.allocation_8fc = nullptr;
    }
    set_runtime_actor_secondary_model(state.models, context.empty_name,
        startup.registry, startup.error, startup.models, startup.deferred,
        startup.fence, startup.types, startup.shaders, startup.scratch, startup.heap_free);
    if (state.word_28c != 0)
    {
        engine_heap_free(context.matrices);
        context.matrices = nullptr;
        state.word_288 = 0;
        state.word_28c = 0;
    }
    set_runtime_actor_model(state.models, context.empty_name,
        startup.registry, startup.error, startup.models, startup.deferred,
        startup.fence, startup.types, startup.shaders, startup.scratch, startup.heap_free);
    actor.model = nullptr; // Native mapping of the released actor+0x204 model.
    if (state.animation.table != nullptr)
    {
        if (state.animation.table_owned != 0)
        {
            release_animation_resource(startup.registry, state.animation.table_resource);
        }
        state.animation.table = nullptr;
    }
    clear_actor_animation(state.animation, actor.model != nullptr ? &context.pose : nullptr,
        startup.registry);
    auto& shape = context.render.shape;
    auto& world = entity_detail::g_world;
    auto* sentinel = reinterpret_cast<entity_detail::ShapeProbe*>(&world.shape_prev);
    if (shape.prev != nullptr)
    {
        auto** next = shape.prev == sentinel ? &world.shape : &shape.prev->next;
        *next = shape.next;
    }
    if (shape.next != nullptr)
    {
        shape.next->prev = shape.prev;
    }
    shape.prev = nullptr;
    shape.next = nullptr;
    unlink_actor_segment_lists(context.render);
    u32 count = 8;
    u32 slot = 0;
    do
    {
        release_actor_sound(actor.sound_slots[slot], context.sounds);
        ++slot;
        --count;
    }
    while (count != 0);
    auto* anchor = static_cast<entity_detail::ListHead*>(context.anchors.next);
    while (anchor != nullptr)
    {
        auto* next = static_cast<entity_detail::ListHead*>(anchor->next);
        release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(anchor));
        anchor = next;
    }
    auto* attachment = static_cast<entity_detail::ListHead*>(context.live_attachments.next);
    while (attachment != nullptr)
    {
        auto* next = static_cast<entity_detail::ListHead*>(attachment->next);
        release_actor_live_attachment(*reinterpret_cast<ActorLiveAttachment*>(attachment));
        attachment = next;
    }
    clear_actor_attachment_effects(state.attachments, context.pending);
}
}
