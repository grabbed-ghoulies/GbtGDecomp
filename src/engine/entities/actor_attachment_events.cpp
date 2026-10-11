#include "actor_attachment_events.h"
#include "../camera/actor_camera_effects.h"
#include "../audio/resource_requests.h"
#include "../runtime/name_lookup.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x0003C040
void dispatch_actor_resource_callout(const RuntimeActorResourceReference& resource, i32 id,
    const RuntimeActorCreationOptions* options, ActorAttachmentEventContext& context)
{
    if (context.actor_address + 0x8Cu == 0)
    {
        return;
    }
    dispatch_actor_attachment_records(resource.resource.data(), id, nullptr, options, context, 0);
}
namespace
{
template<class T>
T read(const u8* bytes, size_t offset)
{
    T value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}
// Xbox 0x00421C9C, original table order determines callout IDs.
const char* const callout_names[] =
{
#include "actor_callout_names.inl"
};
static_assert(std::size(callout_names) == 85);
}

// Xbox 0x0003BBF0
void request_attachment_joint_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    const i16 joint = read<i32>(record, 0xC8) == 0 ? -1 : read<i16>(record, 0xCC);
    request_actor_joint_attachment(context.actor, context.attachments,
        reinterpret_cast<const char*>(record + 0x48), read<u32>(record, 0xD0), joint, options, context.effects);
}

// Xbox 0x0003BC20
void request_attachment_binding_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    request_actor_binding_attachment(context.actor, context.attachments,
        reinterpret_cast<const char*>(record + 0x48), read<u32>(record, 0xCC), read<i16>(record, 0xC8), options, context.effects);
}

// Xbox 0x0003BC40
u16 request_attachment_index_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    const i32 index = read<i32>(record, 0xC8) == 0 ? read<i32>(record, 0xCC) : -1;
    return request_actor_index_attachment(context.actor, context.attachments,
        reinterpret_cast<const char*>(record + 0x48), read<u32>(record, 0xD0), index, options, context.effects);
}

// Xbox 0x0003BC70
void request_attachment_camera_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    if (options != nullptr && (options->flags & 1) != 0)
    {
        const Vec3 position = options->position;
        load_actor_camera_effect(reinterpret_cast<const char*>(record + 0x48), &position,
            context.effects.models.registry, context.effects.models.error);
        return;
    }
    const Vec3 position = actor_world_position(context.actor);
    load_actor_camera_effect(reinterpret_cast<const char*>(record + 0x48), &position,
        context.effects.models.registry, context.effects.models.error);
}

// Xbox 0x0003BCE0
void request_attachment_resource_object(const u8* record, ActorAttachmentEventContext& context)
{
    auto parameters = clone_executable_resource(context.resources, reinterpret_cast<const char*>(record + 0x48));
    if (parameters.bytes != nullptr)
    {
        std::memcpy(parameters.bytes.get() + 8, &context.actor_address, 4);
        context.objects.push_back(create_factory_object(context.factory, {parameters.bytes.get(), parameters.size}));
        parameters.bytes.reset();
        return;
    }
}

// Xbox 0x0003BD10
void request_attachment_camera_shake(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    Vec3 position;
    if (options == nullptr || (options->flags & 1) == 0)
    {
        position = actor_world_position(context.actor);
    }
    else
    {
        position = options->position;
    }
    load_actor_camera_shake(reinterpret_cast<const char*>(record + 0x48), position, 1.f,
        context.effects.models.registry, context.effects.models.error);
}

// Xbox 0x0003BD80
void request_attachment_sound_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context)
{
    const u32 slot = read<u32>(record, 0xC8);
    if (record[0x48] != 0)
    {
        queue_actor_sound(context.actor.sound_slots[slot], actor_sound_groups[slot], &context.actor,
            static_cast<u16>(slot), nullptr, nullptr, 1.f, reinterpret_cast<const char*>(record + 0x48), context.effects.sounds);
    }
    if (read<u32>(record, 0xCC) == 0x5F646961)
    {
        if (options == nullptr)
        {
            options = &actor_world_creation_options(context.actor);
        }
        const u32 result = load_actor_effect_stream(reinterpret_cast<const char*>(record + 0xCC), options, context.effects);
        if (result != 0)
        {
            const i32 slot = g_last_actor_effect;
            g_last_actor_effect = -1;
            if (slot != -1)
            {
                g_actor_effect_nodes[slot]->bytes[0x18] |= 8;
            }
        }
    }
}

// Xbox 0x0003BEC0
void dispatch_actor_attachment_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value)
{
    switch (read<u16>(record, 4))
    {
    case 1:
        request_attachment_sound_effect(record, options, context);
        return;
    case 4:
        request_attachment_joint_event(record, options, context);
        return;
    case 5:
        request_attachment_binding_event(record, options, context);
        return;
    case 6:
        request_attachment_index_event(record, options, context);
        return;
    case 7:
        request_attachment_camera_shake(record, options, context);
        return;
    case 8:
        request_attachment_resource_object(record, context);
        return;
    case 9:
        request_attachment_camera_effect(record, options, context);
        return;
    case 10:
        request_sound_resource(reinterpret_cast<const char*>(record + 0x48), 0x12);
        return;
    default:
        return;
    }
}

// Xbox 0x0003BE20
i32 dispatch_actor_attachment_records(const u8* record, i32 id,
    ActorAttachmentEventCallback callback, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value)
{
    if (callback == nullptr)
    {
        callback = dispatch_actor_attachment_event;
    }
    i32 found = 0;
    if (record != nullptr)
    {
        do
        {
            const char* name = reinterpret_cast<const char*>(record + 8);
            const char* end = name;
            do
            {
                ++end;
            }
            while (end[-1] != '\0');
            i32 callout;
            if (end == reinterpret_cast<const char*>(record + 9))
            {
                callout = 0;
            }
            else
            {
                callout = binary_search_names(85, callout_names, name);
                if (callout == -1)
                {
                    callout = 0;
                }
                else
                {
                    ++callout;
                }
            }
            if (callout == id)
            {
                callback(record, options, context, value);
                found = 1;
            }
            if (read<u16>(record, 4) == 0)
            {
                if (found != 0)
                {
                    return found;
                }
                record = context.linked_records.at(record);
            }
            else
            {
                record += read<u16>(record, 0);
            }
        }
        while (record != nullptr);
        return found;
    }
    return 0;
}
}
