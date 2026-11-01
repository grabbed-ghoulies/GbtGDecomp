#include "player_attachment.h"
#include "actor_render_registration.h"
#include "../runtime/crt_format.h"
#include <cstdarg>
#include <cstring>

namespace gbtg
{
// Xbox 0x00020B30
void format_runtime_body_variant(std::span<u8> body, const char* format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    scene_detail::crt_format_arguments(reinterpret_cast<char*>(body.data() + 0x288), format, arguments);
    va_end(arguments);
}

// Xbox 0x0001B2B0
bool attach_runtime_player_actor(RuntimePlayerAttachmentActor& attached, i32 update_action,
    RuntimePlayerAttachment& context)
{
    const u16 host_class = read_u16(context.host.bytes, 6);
    const i32 count = i16(read_u16(attached.object.bytes, 0x944));
    i32 index = 0;
    if (count <= 0)
    {
        return false;
    }
    while (read_u16(attached.object.bytes, 0x948 + size_t(index) * 0x10) != host_class)
    {
        ++index;
        if (index >= count)
        {
            return false;
        }
    }
    if (index == -1 || context.ownership.attached != nullptr)
    {
        return false;
    }
    context.ownership.attached = &attached.object;
    context.ownership.receiver = &attached.receiver;
    std::memcpy(context.body.data() + 0x48, &attached.object.address, 4);
    std::memcpy(context.body.data() + 0x10, &attached.object.address, 4);
    context.ownership.flags->bytes[4] |= 4;
    const u32 kind = runtime_actor_case_class(attached.object.bytes, read_u16(context.host.bytes, 6));
    format_runtime_body_variant(context.body, "%s_%s", context.weapon_names[attached.object.bytes[0x93D]],
        context.class_names[kind]);
    if (runtime_attachment_class_value(attached.object.bytes, read_u16(context.host.bytes, 6)) == -1)
    {
        if (ensure_runtime_actor_bind(attached.actor) != nullptr)
        {
            attached.actor.bind->mode_parent = 1;
            attached.actor.bind->parent = &context.ownership.actor;
            attached.actor.bind->flags = 1;
        }
    }
    else
    {
        const i32 joint = runtime_attachment_class_value(attached.object.bytes, read_u16(context.host.bytes, 6));
        bind_runtime_actor_joint(attached.actor, &context.ownership.actor, joint, 1);
    }
    set_runtime_attachment_host(attached.object, context.host);
    if (update_action != 0)
    {
        const u16 key = context.body_action_key();
        select_runtime_body_action(context.body, key, context.actions);
    }
    notify_runtime_body_animation(context.actions);
    if (attached.object.bytes[0x93D] == 3)
    {
        ++context.body[0x30B];
    }
    update_actor_animation_pose(attached.actor, attached.animation, attached.model, attached.bone,
        context.actions.registry, attached.callbacks, attached.cache, attached.reset);
    notify_runtime_attachment_host(context.host, attached.object);
    return true;
}

// Xbox 0x0001B190
bool attach_runtime_player_weapon(RuntimePlayerAttachmentActor& attached, i32 update_action,
    RuntimePlayerAttachment& context, const Vec3& origin, const Vec3& euler)
{
    const u16 host_class = read_u16(context.host.bytes, 6);
    const i32 count = i16(read_u16(attached.object.bytes, 0x944));
    i32 index = 0;
    if (count <= 0)
    {
        return false;
    }
    while (read_u16(attached.object.bytes, 0x948 + size_t(index) * 0x10) != host_class)
    {
        ++index;
        if (index >= count)
        {
            return false;
        }
    }
    if (index == -1 || context.ownership.attached != nullptr)
    {
        return false;
    }
    context.ownership.attached = &attached.object;
    context.ownership.receiver = &attached.receiver;
    std::memcpy(context.body.data() + 0x48, &attached.object.address, 4);
    std::memcpy(context.body.data() + 0x10, &attached.object.address, 4);
    context.ownership.flags->bytes[4] |= 4;
    const u32 kind = runtime_actor_case_class(attached.object.bytes, read_u16(context.host.bytes, 6));
    format_runtime_body_variant(context.body, "%s_%s", context.weapon_names[attached.object.bytes[0x93D]],
        context.class_names[kind]);
    const i32 joint = runtime_attachment_class_value(attached.object.bytes, read_u16(context.host.bytes, 6));
    if (joint == -1)
    {
        bind_runtime_actor_parent_now(attached.actor, &context.ownership.actor, 1, origin, euler); // 0x0003B460
    }
    else
    {
        bind_runtime_actor_joint_now(attached.actor, &context.ownership.actor, joint, 1, origin,
            euler); // 0x0003B4C0
    }
    set_runtime_attachment_host(attached.object, context.host);
    if (update_action != 0)
    {
        const u16 key = context.body_action_key();
        select_runtime_body_action(context.body, key, context.actions);
    }
    notify_runtime_body_animation(context.actions);
    update_actor_animation_pose(attached.actor, attached.animation, attached.model, attached.bone,
        context.actions.registry, attached.callbacks, attached.cache, attached.reset);
    if (attached.object.bytes[0x93D] == 3)
    {
        ++context.body[0x30B];
    }
    notify_runtime_attachment_host(context.host, attached.object);
    return true;
}

// Xbox 0x0001B3E0
bool attach_runtime_player_secondary(RuntimePlayerAttachmentActor& attached, RuntimePlayerAttachment& context)
{
    const u16 host_class = read_u16(context.host.bytes, 6);
    const i32 count = i16(read_u16(attached.object.bytes, 0x944));
    i32 index = 0;
    if (count <= 0)
    {
        return false;
    }
    while (read_u16(attached.object.bytes, 0x948 + size_t(index) * 0x10) != host_class)
    {
        ++index;
        if (index >= count)
        {
            return false;
        }
    }
    if (index == -1)
    {
        return false;
    }
    context.secondary = &attached.object;
    context.actions.second_receiver = &attached.receiver;
    std::memcpy(context.body.data() + 0x4C, &attached.object.address, 4);
    std::memcpy(context.body.data() + 0x14, &attached.object.address, 4);
    index = 0;
    bool joint_found = false;
    if (i16(read_u16(attached.object.bytes, 0x944)) > 0)
    {
        do
        {
            const size_t offset = 0x948 + size_t(index) * 0x10;
            if (read_u16(attached.object.bytes, offset) == read_u16(context.host.bytes, 6))
            {
                if (i16(read_u16(attached.object.bytes, offset + 2)) != -1)
                {
                    const i32 joint = runtime_attachment_class_value(attached.object.bytes, read_u16(context.host.bytes, 6));
                    bind_runtime_actor_joint(attached.actor, &context.ownership.actor, joint, 1);
                    joint_found = true;
                }
                break;
            }
            ++index;
        }
        while (index < i16(read_u16(attached.object.bytes, 0x944)));
    }
    if (!joint_found)
    {
        if (ensure_runtime_actor_bind(attached.actor) != nullptr)
        {
            attached.actor.bind->mode_parent = 1;
            attached.actor.bind->parent = &context.ownership.actor;
            attached.actor.bind->flags = 1;
        }
    }
    set_runtime_attachment_host(attached.object, context.host);
    notify_runtime_body_animation(context.actions);
    update_actor_animation_pose(attached.actor, attached.animation, attached.model, attached.bone,
        context.actions.registry, attached.callbacks, attached.cache, attached.reset);
    notify_runtime_secondary_attachment_host(context.host, attached.object);
    return true;
}

// Xbox 0x0001B810
bool release_runtime_player_attachment(i32 force, u32 reason, RuntimePlayerAttachment& context)
{
    auto* secondary = context.secondary;
    auto* attached = context.ownership.attached;
    bool released = false;
    if (attached == nullptr)
    {
        if ((context.ownership.flags->bytes[5] & 0x40) != 0 && context.mind_present && context.mind_action != nullptr)
        {
            const u32 message[]{0x97, reason};
            context.mind_action->message(message);
        }
    }
    else
    {
        bool permitted = attached->bytes[0x93F] != 1;
        if (attached->bytes[0x93F] == 2)
        {
            const auto* host = attached->host;
            permitted = host != nullptr && (read_u16(host->bytes, 0xCDC) == 2 || read_u32(host->bytes, 0x93C) != 0);
        }
        if (permitted || force != 0)
        {
            detach_runtime_body_actor(context.body, context.ownership);
            const u32 message[]{0x97, reason};
            attached->message(message);
            released = true;
        }
    }
    if (secondary != nullptr && secondary != attached)
    {
        context.secondary = nullptr;
        context.actions.second_receiver = nullptr;
        const u32 zero = 0;
        std::memcpy(context.body.data() + 0x4C, &zero, 4);
        std::memcpy(context.body.data() + 0x14, &zero, 4);
        const u32 message[]{0x97, reason};
        secondary->message(message);
    }
    return released;
}

// Xbox 0x0001B6B0
bool let_go_runtime_player_attachment(RuntimePlayerAttachment& context)
{
    if (!release_runtime_player_attachment(0, 0, context)) // 0x0001B810, EBX the body
    {
        return false;
    }
    const u16 key = context.body_action_key(); // Body vtable +0x20
    select_runtime_body_action(context.body, key, context.actions); // 0x00027650, EAX the key
    return true;
}

// Xbox 0x0013A7C0
bool replace_runtime_player_attachment(RuntimePlayerAttachment& context)
{
    auto* attached = context.ownership.attached;
    if (attached == nullptr)
    {
        return false;
    }
    auto& actor = *context.actors.at(attached);
    if (!runtime_attachment_available(attached->bytes, attached->host))
    {
        detach_runtime_body_actor(context.body, context.ownership);
        activate_actor_bind_tree(actor.actor);
    }
    else
    {
        release_runtime_player_attachment(0, 0, context);
    }
    const u16 key = context.body_action_key();
    select_runtime_body_action(context.body, key, context.actions);
    auto* controller = context.host_animation.controller.get();
    if (controller != nullptr)
    {
        auto* player = controller->slots[0].player.get();
        if (player != nullptr)
        {
            if (player->old_pose != nullptr)
            {
                player->old_pose.reset();
            }
            player->blend_remaining = 0.f;
        }
        controller->dirty = true;
    }
    return true;
}

// Xbox 0x0003B370, ECX actor, EAX slot
void reset_runtime_animation_slot_blend(AnimationController* controller, u32 slot)
{
    if (controller == nullptr) // Actor +0x50.
    {
        return;
    }
    MinaPlayer* player = controller->slots.at(slot).player.get(); // +0x58, stride 0x28.
    if (player != nullptr)
    {
        if (player->old_pose != nullptr) // +0xB4.
        {
            player->old_pose.reset(); // 0x000663E0
        }
        player->blend_remaining = 0.f; // +0x110.
    }
    controller->dirty = true; // +0x5C.
}

// Xbox 0x0013A860
bool drop_runtime_player_attachment(RuntimePlayerAttachment& context)
{
    auto* attached = context.ownership.attached; // Body +0x48.
    if (attached == nullptr)
    {
        return false;
    }
    const u8 kind = attached->bytes[0x93F];
    if (kind != 1)
    {
        if (kind != 2)
        {
            return false;
        }
        const auto* host = attached->host; // +0xD8C.
        if (host != nullptr && (read_u16(host->bytes, 0xCDC) == 2 || read_u32(host->bytes, 0x93C) != 0))
        {
            return false;
        }
    }
    auto& actor = *context.actors.at(attached);
    detach_runtime_body_actor(context.body, context.ownership); // 0x0001B4C0, ESI body
    activate_actor_bind_tree(actor.actor); // 0x00112CC0, ESI the attached actor
    const u16 key = context.body_action_key(); // Body vtable +0x20.
    select_runtime_body_action(context.body, key, context.actions); // 0x00027650
    reset_runtime_animation_slot_blend(context.host_animation.controller.get(), 0); // 0x0003B370, EAX 0
    return true;
}

// Xbox 0x0013A720
FactoryObject create_runtime_player_attachment(const char* name, i32 replace,
    RuntimePlayerAttachmentCreation& context)
{
    auto clone = clone_executable_resource(context.child.resources, name);
    if (clone.bytes == nullptr)
    {
        return {};
    }
    FactoryObject object = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
    if (object.bytes != nullptr)
    {
        if (replace != 0)
        {
            replace_runtime_player_attachment(context.attachment);
        }
        auto& attached = context.view_of(object);
        if (!attach_runtime_player_actor(attached, 1, context.attachment))
        {
            release_factory_object(context.child.factory, object);
            clone.bytes.reset();
            return {};
        }
        const u32 message[]{0x74, context.child.actor_address, attached.object.address};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message));
    }
    clone.bytes.reset();
    return object;
}

// Xbox 0x0001A2D0
FactoryObject command_runtime_body_create_secondary(std::span<const u8> command,
    std::span<char, 128> name_scratch,
    const std::function<void(RuntimeBodyAttachedActor&, std::span<char, 128>)>& query_name,
    RuntimePlayerAttachmentCreation& context)
{
    if (read_u32(context.attachment.body, 0x4C) == 0)
    {
        if (read_u32(command, 0x10) == 0x5F646961)
        {
            const char* source = reinterpret_cast<const char*>(command.data() + 0x10);
            char* destination = name_scratch.data();
            char value;
            do
            {
                value = *source++;
                *destination++ = value;
            }
            while (value != 0);
        }
        else
        {
            auto* primary = context.attachment.ownership.attached;
            name_scratch[0] = 0;
            if (primary != nullptr && primary->bytes[0x93D] == 2)
            {
                query_name(*primary, name_scratch); // Actual actor vtable +0xC, message 0x99 with native output pointer.
            }
        }
        if (read_u32({reinterpret_cast<const u8*>(name_scratch.data()), name_scratch.size()}, 0) == 0x5F646961)
        {
            auto clone = clone_executable_resource(context.child.resources, name_scratch.data());
            if (clone.bytes != nullptr)
            {
                const float scale = context.attachment.host.actor->radius_xz * read_f32({clone.bytes.get(), clone.size}, 0x20); // Body +8 -> actor +0x38.
                std::memcpy(clone.bytes.get() + 0x20, &scale, 4);
                auto object = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
                clone.bytes.reset();
                if (object.bytes != nullptr)
                {
                    auto& attached = context.view_of(object);
                    if (!attach_runtime_player_secondary(attached, context.attachment))
                    {
                        release_factory_object(context.child.factory, object);
                    }
                }
                return object;
            }
        }
    }
    return {};
}

// Xbox 0x0001A6D0
FactoryObject command_runtime_body_create_attachment(const char* name,
    RuntimePlayerAttachmentCreation& context)
{
    auto clone = clone_executable_resource(context.child.resources, name);
    if (clone.bytes != nullptr)
    {
        auto object = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
        clone.bytes.reset();
        if (object.bytes != nullptr)
        {
            auto& attached = context.view_of(object);
            if (!attach_runtime_player_actor(attached, 0, context.attachment))
            {
                release_factory_object(context.child.factory, object);
            }
        }
        return object;
    }
    return {};
}
}
