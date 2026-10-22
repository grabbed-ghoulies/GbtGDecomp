#pragma once

#include "actor_frame_render.h"
#include "actor_frame_update.h"
#include "../graphics/billboard_queue.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/model_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace gbtg
{
// fxCustom (factory type 0xC, getter 0x000EC650, vtable 0x005068F4) over the base object (type 1): an effect that
// rides an actor. Its constructor (0x000EC6A0) appends it to the actor's +0x1D8 frame list (its node at +8), so the
// actor's frame update (0x000420D0) runs its vtable +0x10 and the actor's draw (0x0003DE00) its +0x14.
// Fields: +8 / +0xC the node's previous and next, +0x10 seconds alive, +0x14 the actor (the parameters' +8).
//
// entityFxCustomElectric (factory type 0x2E, getter 0x00076590, vtable 0x005053E0) over fxCustom: crackling sparks
// on the actor's contact shapes (model block 4, mask 0x20002, or 0x80 when a shape carries it), each a billboard of
// a random frame of the parameters' texture (+0x8C) that flickers, a kind-2 light (DAT_00509B7C) that follows the
// actor, and a sound (+0xC) in actor sound slot 4 (the haunted TV's attack).
// Fields: +0x18 the sound's name, +0x98 the texture, +0x9C the shape count, +0xA0 the {shape, frame} rows,
// +0xA4 the light's slot (-1 for none).
//
// entityFxCustomSolidFadeIn (factory type 0x32, getter 0x00077DB0, vtable 0x00505440) over fxCustom: the actor's
// tint (+0x198 colour and the +0x19C..+0x19E levels) fades in over +0x24 seconds, holds for +0x28 (or until message
// 5), fades out over +0x2C, then either waits +0x30 and starts again or releases itself (0x0003DE30). Message 3
// restarts the hold. Its +0x14 slot is the empty 0x001628B0 (the generate ghouly state's release effect).
// Fields: +0x18 the phase (0 in, 1 hold, 2 out, 3 wait), +0x1C its seconds left, +0x20 the level, +0x24..+0x30 the
// four times (the parameters' +0x14..+0x20), +0x34 the colour (+0xC), +0x38 the strength (+0x10), +0x50 set by
// message 5 to end the hold.
struct RuntimeFxCustomContext
{
    std::function<Actor*(u32)> actor_at; // The parameters' +8 actor.
    std::function<ActorFrameState&(Actor&)> frame_of; // That actor's frame list (+0x1D8).
    std::function<std::span<const u8>(const Actor&)> actor_bytes; // 0x0003D860's view of the actor.
    std::unordered_map<ActorFrameAttachment*, const u8*>& frame_objects; // The list nodes' objects.
    std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback>& draws; // The list nodes' vtable +0x14.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    RuntimeActorSoundQueues& sounds;
    RuntimeShaderRegistrationState& lights; // DAT_00509B7C / DAT_00509B78.
    u32& random; // DAT_0054DD7C.
    const float& pause; // DAT_004312DC.
    const float& frame_delta; // DAT_005096C0.
    const float& time_scale; // DAT_004312C4.
    std::function<BillboardFlushContext()> billboards; // The sprite pass 0x0005C410 .. 0x0005C8C0 draws through.
    std::function<RuntimeActorStartupState&(Actor&)> startup_of; // The actor's +0x198 colour and +0x19C levels.
    std::function<std::span<u8>(Actor&)> object_bytes; // The actor's own bytes (+0x1A0..+0x1A8).
    std::function<u32(const u8*)> address_of; // An effect's retail address.
    std::function<bool(Actor&, u32)> release_frame_object; // 0x0003DE30, ECX actor, ESI object.
};
void fx_custom_object_sizes(ObjectSizes& output); // Xbox 0x00095530
void fx_custom_electric_object_sizes(ObjectSizes& output); // Xbox 0x00076570
void fx_custom_solid_fade_in_object_sizes(ObjectSizes& output); // Xbox 0x00077D90
// entityFxCustomOutline (type 0x31, getter 0x00077300, vtable 0x00505428): a candle hit's burning outline.
void fx_custom_outline_object_sizes(ObjectSizes& output); // Xbox 0x000772E0
// Message 3 holds the outline at full level again, 5 lets its hold end.
void message_fx_custom_outline(std::span<u8> effect, std::span<const u32> message); // Xbox 0x00077A80
// Message 3 restarts the hold, 5 ends it.
void message_fx_custom_solid_fade_in(std::span<u8> effect, std::span<const u32> message); // Xbox 0x000780C0
// A light record (0x98 bytes, 0x00051BC0) of the kind at the position in the first free DAT_00509B7C slot.
std::int8_t allocate_runtime_light(u8 kind, const Vec3& position, u8* record,
    RuntimeShaderRegistrationState& lights); // Xbox 0x000FE080, ESI position
void move_runtime_light(std::int8_t slot, const Vec3& position,
    RuntimeShaderRegistrationState& lights); // Xbox 0x000FE0F0, AL slot, ECX position
// Every mask a shape list's records carry, ORed.
u32 runtime_shape_list_mask(const u8* list); // Xbox 0x00056C80, EDX the list
// Types 0xC (base 1), 0x2E and 0x32 (base 0xC) with their lifecycles.
void install_runtime_fx_custom_classes(std::span<ObjectFactoryType> types, RuntimeFxCustomContext& context);
}
