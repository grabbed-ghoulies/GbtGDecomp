#include "body_attachment_cleanup.h"
#include "entity_heap.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0003CFD0
bool remove_runtime_body_attachment(u16 token, RuntimeBodyAttachmentLists& lists)
{
    auto* node = lists.first;
    while (node != nullptr && read_u16(node->bytes, 0x18) != token)
    {
        node = node->next;
    }
    if (node == nullptr)
    {
        node = lists.second;
        while (node != nullptr && read_u16(node->bytes, 0x18) != token)
        {
            node = node->next;
        }
        if (node == nullptr)
        {
            return false;
        }
        const u32 index = read_u32(node->bytes, 8);
        if (index != 0xFFFFFFFFu)
        {
            u8* timed = lists.timed[index];
            timed[0x18] |= 8;
            const u32 minus_one = 0xBF800000;
            std::memcpy(timed + 8, &minus_one, 4);
        }
    }
    if (node->previous != nullptr)
    {
        node->previous->next = node->next;
    }
    if (node->next != nullptr)
    {
        node->next->previous = node->previous;
    }
    node->previous = nullptr;
    node->next = nullptr;
    engine_heap_free(node);
    return true;
}

// Xbox 0x00025090
void clear_runtime_body_attachments(std::span<u8> body, RuntimeBodyAttachmentLists& lists)
{
    i32 index = 0;
    if (std::bit_cast<i32>(read_u32(body, 0x2D8)) > 0)
    {
        do
        {
            remove_runtime_body_attachment(u16(read_u32(body, 0x2DC + size_t(index) * 4)), lists);
            ++index;
        } while (index < std::bit_cast<i32>(read_u32(body, 0x2D8)));
    }
    const u32 zero = 0;
    std::memcpy(body.data() + 0x2D8, &zero, 4);
}

// Xbox 0x0001B4C0
RuntimeBodyAttachedActor* detach_runtime_body_actor(std::span<u8> body, RuntimeBodyActorAttachment& context)
{
    auto* attached = context.attached;
    const u32 zero = 0;
    std::memcpy(body.data() + 0x10, &zero, 4);
    context.receiver = nullptr;
    context.flags->bytes[4] &= 0xFB;
    const u32 actor_address = context.actor_address;
    body[0x288] = 0;
    std::array<u32, 3> message{0x9D, actor_address, 0};
    attached->message(message);
    detach_actor_bind(*attached->actor);
    context.attached = nullptr;
    std::memcpy(body.data() + 0x48, &zero, 4);
    message = {0x9C, actor_address, 0};
    attached->message(message);
    if (attached->bytes[0x93D] == 3)
    {
        --body[0x30B];
    }
    message = {0x73, context.actor_address, attached->address};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data()));
    return attached;
}

namespace
{
void release_animation_storage(RuntimeBodyAnimationSetup& animation)
{
    if (animation.groups != nullptr)
    {
        if (release_runtime_body_cached_resource(animation.groups, animation.cache, animation.cache_count))
        {
            engine_heap_free(animation.groups);
        }
        animation.groups = nullptr;
    }
    if (animation.resource.data() != nullptr)
    {
        release_animation_resource(animation.registry, animation.resource);
        animation.resource = {};
    }
    if (animation.bits != nullptr)
    {
        engine_heap_free(animation.bits);
        animation.bits = nullptr;
    }
}
}

// Xbox 0x00024F70
void destroy_runtime_parent_body(std::span<u8> body, RuntimeParentBodyCleanup& context)
{
    release_animation_storage(context.animation);
    if (context.attachment.attached != nullptr)
    {
        detach_runtime_body_actor(body, context.attachment);
    }
    release_runtime_body_resource_hashes(context.resources, context.hashes,
        context.hash_count, context.animation.registry);
    clear_runtime_body_attachments(body, context.lists);
    if (context.collision != nullptr)
    {
        destroy_runtime_body_collision_state(context.collision);
        context.collision = nullptr;
    }
    if (context.attributes.data() != nullptr)
    {
        release_animation_resource(context.animation.registry, context.attributes);
        context.attributes = {};
    }
    // The dump repeats this sequence after callbacks/resource unloads; reread all three fields.
    release_animation_storage(context.animation);
    if (context.shapes.special != nullptr)
    {
        engine_heap_free(context.shapes.special);
        context.shapes.special = nullptr;
        context.shapes.special_count = 0;
    }
    release_runtime_body_health(context.health);
    release_runtime_body_actions(body, context.first_action, context.second_action,
        context.actions, context.factory, context.animation.registry);
    if (context.attachment.flags != nullptr)
    {
        engine_heap_free(context.attachment.flags);
        context.attachment.flags = nullptr;
    }
}
}
