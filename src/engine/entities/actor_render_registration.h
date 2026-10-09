#pragma once

#include "actor_world_lists.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct ActorRenderRegistration
{
    Actor& actor;
    entity_detail::ShapeProbe& shape;
    entity_detail::SegmentProbe& segment;
    RuntimeActorLink& view; // actor+0x900
    entity_detail::ListHead bins[8]{}; // actor+0x124, stride 0xC
    entity_detail::ListHead anchor{}; // actor+0x18C
    entity_detail::ListHead wide{}; // actor+0x1E8
    entity_detail::ListHead child{}; // actor+0x908
    void (*message)(ActorRenderRegistration& state, u32 id) = nullptr; // actor vtable+0xC
};
void register_actor_render_bins(ActorRenderRegistration& state); // Xbox 0x0003CAE0
void unlink_actor_view(RuntimeActorLink& link); // Xbox 0x0003EE70
void unregister_actor_render_bins(ActorRenderRegistration& state); // Xbox 0x0003CBB0
void unregister_actor_segment_probes(ActorRenderRegistration& state); // Xbox 0x0003E850
void unlink_actor_segment_lists(ActorRenderRegistration& state); // Xbox 0x0003E7A0
void register_actor_child_list(ActorRenderRegistration& state); // Xbox 0x0003ED70
void activate_actor_bind_children(ActorLink* node, Actor& source); // Xbox 0x00112D10
void activate_actor_bind_tree(Actor& actor); // Xbox 0x00112CC0
}
