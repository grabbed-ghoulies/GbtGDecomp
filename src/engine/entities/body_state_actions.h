#pragma once
#include "gbtg/object_factory.h"
#include "gbtg/model_runtime.h"
#include "gbtg/sound_runtime.h"
#include "entity_types.h"
#include "body_animation_setup.h"
#include "player_attachment.h"
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
};
u8 set_runtime_body_shape_mode(u8 mode, RuntimeBodyStateContext& context); // Xbox 0x0001FEB0, BL mode, EDI body
void actor_move_object_sizes(ObjectSizes& output); // Xbox 0x00095530
void stand_alert_object_sizes(ObjectSizes& output); // Xbox 0x000A2510
bool initialize_runtime_stand_alert(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A2590
void destroy_runtime_stand_alert(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A2790
// Installs type 9 actorMove (getter 0x0002B5A0, vtable 0x004DD72C) and the state classes whose lifecycles
// are translated: 0x87 entityStateStandAlert (getter 0x000A2530, vtable 0x00505BC0). The context
// resolver maps a state's body address (+8) to its native body views.
void install_runtime_body_state_classes(std::span<ObjectFactoryType> types,
    std::function<RuntimeBodyStateContext(u32)> context_of);
}
