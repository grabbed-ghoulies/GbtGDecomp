#include "actor_word_messages.h"
#include <bit>
#include <cstring>

namespace gbtg
{
void dispatch_base_actor_word_message(ActorBaseContext& context, std::span<u32> message,
    const RuntimeActorWordBindings& bindings)
{
    const u32 id = message[0];
    if (id == 7)
    {
        const ActorAnimationMessage native{7, std::bit_cast<float>(message[1]), message[2],
            message[3] == 0 ? std::span<const u8>{} : bindings.memory_at(message[3]), i32(message[4])};
        dispatch_base_actor_message(&native, context);
    }
    else if (id == 0xB)
    {
        const auto bytes = std::as_bytes(message);
        const std::span<const u8> options{reinterpret_cast<const u8*>(bytes.data()) + 8, bytes.size() - 8};
        AnimationMix mix;
        mix.duration = read_f32(options, 4);
        mix.weight = read_f32(options, 8);
        mix.blend_mode = i32(read_u32(options, 0xC));
        mix.fade_duration = read_f32(options, 0x10);
        mix.position = read_f32(options, 0x14);
        mix.mode = i16(read_u16(options, 0x18));
        mix.priority = i32(read_u32(options, 0x1C));
        mix.old_pose_duration = read_f32(options, 0x20);
        mix.events = reinterpret_cast<const char*>(options.data() + 0x24);
        const u32 animation = read_u32(options, 0xA4);
        const u32 events = read_u32(options, 0xA8);
        const u32 record = read_u32(options, 0xAC);
        mix.borrowed_animation = animation == 0 ? std::span<const u8>{} : bindings.memory_at(animation);
        mix.borrowed_events = events == 0 ? std::span<const u8>{} : bindings.memory_at(events);
        mix.record = record == 0 ? nullptr : bindings.animation_record_at(record);
        const ActorAnimationMixMessage native{id, message[1], mix,
            reinterpret_cast<const char*>(message.data()) + 0xB8};
        dispatch_base_actor_message(&native, context);
    }
    else if (id == 0xC || id == 0xD)
    {
        ActorVectorMessage native{id, {}};
        std::memcpy(&native.value, message.data() + 1, 12);
        dispatch_base_actor_message(&native, context);
    }
    else if (id == 0x7C || id == 0x8B)
    {
        const ActorReferenceMessage native{id, message[1] == 0 ? nullptr : bindings.reference_at(message[1])};
        dispatch_base_actor_message(&native, context);
    }
    else
    {
        dispatch_base_actor_message(&id, context);
    }
}
}
