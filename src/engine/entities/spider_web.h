#pragma once
#include <array>
#include "actor_fields.h"
#include "actor_clock.h"
#include "body_contacts.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"
#include <functional>
#include <vector>

namespace gbtg
{
struct ActorAnimationState;
struct AnimationResourceRegistry;
struct ResourceErrorState;
struct ActorRenderRegistration;
namespace entity_detail
{
struct ShapeProbe;
}

// entityAvatarMiscSpiderWeb, factory type 0x1B (getter 0x000704D0, vtable 0x00504E8C) over the actor (type 2): the web
// a spider's spit leaves where it lands. Message 0x7A (from the spit, word 2 the address of its flight direction)
// sets it on the surface it hit; it expands, then a web on a floor holds for 5 s while one on a wall shrinks at once,
// and it releases itself once it has shrunk. While it lies flat, a player within its reach gets {0x4F, 0xC}.
namespace spider_web_parameter
{
constexpr u32 kSize = 0x334;
} // namespace spider_web_parameter
struct SpiderWebState
{
    ActorBase base{}; // +0
    u32 state = 0; // +0x934, 0 expanding, 1 holding, 2 shrinking
    i32 joint = 0; // +0x938, the model's joint 0 (0x00052FC0), -1 without one
    float timer = 0.f; // +0x93C, state 1's hold
    u32 flat = 0; // +0x940, the surface faces up (normal y above 0.9)
};
static_assert(sizeof(SpiderWebState) == 0x944);
struct RuntimeSpiderWeb
{
    std::span<u8> bytes;
    Actor& actor;
    ActorAnimationState& animation; // Actor +0x48 table, +0x50 controller.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
};
struct RuntimeSpiderWebUpdate
{
    const RuntimeActorClock& clock;
    std::function<void()> update_actor; // 0x000420D0.
    std::function<std::vector<RuntimeSegmentContactActor*>()> players; // DAT_00511B38's players in list order.
};
struct RuntimeSpiderWebMessage
{
    std::function<void(std::span<u32>)> base_message; // 0x0003EF70.
    ActorRenderRegistration& render; // 0x0003E850's probes.
    entity_detail::ShapeProbe& shape; // 0x0003C700's probe.
    std::function<std::span<const u8>(u32)> memory_at; // Message 0x7A's direction by address.
};
void spider_web_object_sizes(ObjectSizes& sizes); // Xbox 0x000704B0
bool initialize_runtime_spider_web(RuntimeSpiderWeb& context); // Xbox 0x00070510
void update_runtime_spider_web(RuntimeSpiderWeb& context, RuntimeSpiderWebUpdate& update); // Xbox 0x000705B0
void dispatch_runtime_spider_web_message(RuntimeSpiderWeb& context, std::span<u32> message,
    RuntimeSpiderWebMessage& handlers); // Xbox 0x000706E0
// EAX state, ESI web: state 0 starts "expand", 1 arms the 5 s hold and 2 starts "shrink".
void set_runtime_spider_web_state(RuntimeSpiderWeb& context, u32 state); // Xbox 0x00070720
// Sends {0x4F, 0xC} to each player within the web's reach while it lies flat.
void snare_runtime_spider_web_players(RuntimeSpiderWeb& context,
    const std::vector<RuntimeSegmentContactActor*>& players); // Xbox 0x00070780
// EAX message: sets the web on the surface along the spit's direction, or leaves it where it is when nothing is hit.
void place_runtime_spider_web(RuntimeSpiderWeb& context, std::span<const u32> message,
    const std::function<std::span<const u8>(u32)>& memory_at); // Xbox 0x000709F0
}
