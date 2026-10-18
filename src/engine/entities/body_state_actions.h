#pragma once
#include "gbtg/object_factory.h"
#include "gbtg/model_runtime.h"
#include "gbtg/sound_runtime.h"
#include "entity_types.h"
#include "body_animation_setup.h"
#include "player_attachment.h"
#include "body_action_selection.h"
#include <functional>

namespace gbtg
{
// Native views of the body a state action belongs to (state +8 is the body's address).
struct RuntimeBodyStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage*& bits; // Body +0x15C.
    AnimationController* animation; // Actor +0x50.
    entity_detail::ShapeProbe& shape; // Actor shape probe registered by 0x0003C770.
    RuntimeModelInstance* secondary; // Actor +0x310.
    i32& secondary_mode; // Actor +0x30C.
    RuntimeActorSoundQueues& sounds;
    std::span<u32> node_states; // DAT_005078D8.
    RuntimeBodyAttachedActor*& attached; // Body +0x4C.
    RuntimeBodyAnimationReceiver*& receiver; // Body +0x14.
    std::function<void(std::span<const u32>)> body_message; // Actual body vtable +0xC.
    RuntimeBodyActionSelection& actions; // Body animation selection (0x0001AED0, 0x0001B020).
    // XBE addresses held in executable parameters; size 0 views a NUL-terminated string.
    std::function<std::span<const u8>(u32 address, size_t size)> resolve;
};
u8 set_runtime_body_shape_mode(u8 mode, RuntimeBodyStateContext& context); // Xbox 0x0001FEB0, BL mode, EDI body
void actor_move_object_sizes(ObjectSizes& output); // Xbox 0x00095530
void stand_alert_object_sizes(ObjectSizes& output); // Xbox 0x000A2510
bool initialize_runtime_stand_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A2590
void destroy_runtime_stand_alert(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A2790
void walk_alert_object_sizes(ObjectSizes& output); // Xbox 0x000A55D0
bool initialize_runtime_walk_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A5650
void destroy_runtime_walk_alert(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A5950
// Walk-alert state +0x30 + 0x10 * index: the animation record 0x000A5650 found for speed band `index`. The
// record lives on the host, so the native word only says whether one was found.
const AnimationRecord* runtime_walk_alert_record(std::span<const u8> state, u16 index);
struct RuntimeHitStateLifecycle;
struct RuntimeDizzyStateContext;
struct RuntimeKnockdownContext;
struct RuntimeDieStateContext;
struct RuntimeAnimStateContext;
struct RuntimePickupStateContext;
// Installs type 9 actorMove (getter 0x0002B5A0, vtable 0x004DD72C) and the state classes whose lifecycles
// are translated: 0x64 (getter 0x0008C940, vtable 0x00505760), 0x7A (getter 0x0009C570, vtable 0x00505A20), 0x7C (getter 0x0009DDA0, vtable 0x00505A60), 0x73 entityStateHit (getter 0x00095740, vtable 0x00505940), 0x74 entityStateHitFurnace
// (getter 0x000966B0, vtable 0x00505960), 0x87 entityStateStandAlert
// (getter 0x000A2530, vtable 0x00505BC0) and 0x8E entityStateWalkAlert (getter 0x000A55F0, vtable
// 0x00505CA0). The context resolvers map a state's body address (+8) to its native body views.
void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of, std::function<RuntimeHitStateLifecycle(u32)> hit_of,
    std::function<RuntimeDizzyStateContext&(u32)> dizzy_of, std::function<RuntimeKnockdownContext&(u32)> knockdown_of,
    std::function<RuntimeDieStateContext&(u32)> die_of, std::function<RuntimeAnimStateContext&(u32)> anim_of,
    std::function<RuntimePickupStateContext&(u32)> pickup_of);
}
