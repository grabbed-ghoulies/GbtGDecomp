#include "actor_attachment_requests.h"
#include "actor_model_anchors.h"

#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
T read(const ActorAttachmentNode& node, size_t offset)
{
    T value;
    std::memcpy(&value, node.bytes.data() + offset, sizeof(T));
    return value;
}
template<class T>
void write(ActorAttachmentNode& node, size_t offset, const T& value)
{
    std::memcpy(node.bytes.data() + offset, &value, sizeof(T));
}
void unlink_temporary_attachment(ActorAttachmentNode& node)
{
    if (node.previous != nullptr)
    {
        node.previous->next = node.next;
    }
    if (node.next != nullptr)
    {
        node.next->previous = node.previous;
    }
    node.previous = nullptr;
    node.next = nullptr;
}
}

// Xbox 0x0003D0A0
void clear_actor_attachment_effects(ActorAttachmentList& active,
    ActorAttachmentNode& pending)
{
    ActorAttachmentNode* node = pending.next;
    while (node != nullptr)
    {
        unlink_temporary_attachment(*node);
        engine_heap_free(node);
        node = pending.next;
    }
    node = active.sentinel.next;
    while (node != nullptr)
    {
        const i32 index = read<i32>(*node, 8);
        if (index != -1)
        {
            auto& effect = *g_actor_effect_nodes[index];
            effect.bytes[0x18] |= 8;
            const u32 lifetime = 0xBF800000; // DAT_0032B120.
            std::memcpy(effect.bytes.data() + 8, &lifetime, sizeof(lifetime));
        }
        unlink_temporary_attachment(*node);
        engine_heap_free(node);
        node = active.sentinel.next;
    }
}

// Xbox 0x0003D250
ActorAttachmentNode* create_actor_joint_attachment(ActorAttachmentList& list,
    const char* name, u32 flags, i16 joint, const RuntimeActorCreationOptions* options)
{
    ActorAttachmentNode* node = actor_create_attachment(list, name, 0, static_cast<i32>(flags));
    if (node == nullptr)
    {
        return nullptr;
    }
    if (options != nullptr)
    {
        if ((options->flags & 1) != 0)
        {
            node->bytes[0xA4] = 1;
            write(*node, 0xA8, options->position);
        }
        if ((options->flags & 4) != 0)
        {
            node->bytes[0xA5] = 1;
            write(*node, 0xB4, options->euler);
        }
        if ((options->flags & 0x40) != 0)
        {
            write(*node, 0xC0, options->scale);
            write(*node, 0xC, joint);
            return node;
        }
    }
    write(*node, 0xC, joint);
    return node;
}

// Xbox 0x0003D330
ActorAttachmentNode* create_actor_binding_attachment(Actor& actor, ActorAttachmentList& list,
    const char* name, u32 flags, i16 kind, const RuntimeActorCreationOptions* options)
{
    const u8* binding = find_actor_model_binding(actor, kind);
    if (binding != nullptr)
    {
        ActorAttachmentNode* node = actor_create_attachment(list, name, 1, static_cast<i32>(flags));
        if (node != nullptr)
        {
            if (options != nullptr)
            {
                if ((options->flags & 0x40) != 0)
                {
                    node->binding = reinterpret_cast<const ModelBoneBinding*>(binding);
                    write(*node, 0xC0, options->scale);
                    return node;
                }
            }
            node->binding = reinterpret_cast<const ModelBoneBinding*>(binding);
            return node;
        }
    }
    return nullptr;
}

// Xbox 0x0003D3A0
bool start_actor_attachment_effect(Actor& actor, ActorAttachmentNode& node,
    ActorEffectStreamContext& context)
{
    bool started = false;
    auto& options = g_actor_creation_options;
    options.flags = 0;
    if (read<i32>(node, 0x1C) == 2)
    {
        g_actor_effect_binding.mode = 0;
        g_actor_effect_binding.actor = &actor;
        g_actor_effect_binding.index = read<i32>(node, 0x14);
        options.user = &g_actor_effect_binding;
        options.flags = 8;
    }
    Vec3 position;
    Vec3 euler;
    actor_attachment_pose(actor, node, position, euler);
    if (node.bytes[0xA4] != 0)
    {
        position = read<Vec3>(node, 0xA8);
    }
    if (node.bytes[0xA5] != 0)
    {
        euler = read<Vec3>(node, 0xB4);
    }
    options.scale = read<float>(node, 0xC0);
    options.flags |= 0x45;
    options.position = position;
    options.euler = euler;
    const u32 result = load_actor_effect_stream(reinterpret_cast<const char*>(node.bytes.data() + 0x24), &options, context);
    if (result != 0)
    {
        const i32 slot = g_last_actor_effect;
        g_last_actor_effect = -1;
        write(node, 8, slot);
        if (slot != -1)
        {
            if ((node.bytes[0x20] & 8) == 0)
            {
                g_actor_effect_nodes[slot]->bytes[0x18] &= 0xF7;
            }
            else
            {
                g_actor_effect_nodes[slot]->bytes[0x18] |= 8;
            }
            started = true;
        }
    }
    return started && (node.bytes[0x20] & 8) == 0;
}

// Xbox 0x0003CE40
u16 request_actor_joint_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i16 joint, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return 0;
    }
    ActorAttachmentNode* node = create_actor_joint_attachment(list, name, flags, joint, options);
    if (node != nullptr)
    {
        if ((flags & 8) == 0)
        {
            return read<u16>(*node, 0x18);
        }
        unlink_temporary_attachment(*node);
        start_actor_attachment_effect(actor, *node, context);
        engine_heap_free(node);
    }
    return 0;
}

// Xbox 0x0003CEB0
u16 request_actor_binding_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i16 kind, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return 0;
    }
    ActorAttachmentNode* node = create_actor_binding_attachment(actor, list, name, flags, kind, options);
    if (node != nullptr)
    {
        if ((flags & 8) == 0)
        {
            return read<u16>(*node, 0x18);
        }
        unlink_temporary_attachment(*node);
        start_actor_attachment_effect(actor, *node, context);
        engine_heap_free(node);
    }
    return 0;
}

// Xbox 0x0003CF30
u16 request_actor_index_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i32 index, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return 0;
    }
    ActorAttachmentNode* node = actor_create_attachment(list, name, 2, static_cast<i32>(flags));
    if (node != nullptr)
    {
        if (options != nullptr && (options->flags & 0x40) != 0)
        {
            write(*node, 0xC0, options->scale);
        }
        write(*node, 0x14, index);
        if ((flags & 8) == 0)
        {
            return read<u16>(*node, 0x18);
        }
        unlink_temporary_attachment(*node);
        start_actor_attachment_effect(actor, *node, context);
        engine_heap_free(node);
    }
    return 0;
}

// Xbox 0x0013B6B0
RuntimeActorCreationOptions& actor_world_creation_options(Actor& actor)
{
    auto& options = g_actor_creation_options;
    options.flags = 0;
    options.position = actor_world_position(actor);
    options.flags |= 1;
    options.euler = actor_world_euler(actor);
    options.flags |= 4;
    const Vec3 euler = actor_world_euler(actor);
    Vec3 direction{};
    add_sound_direction(direction, euler.x, euler.y, 1.f);
    std::memcpy(options.retained.data(), &direction, sizeof(direction));
    options.flags |= 0x20;
    return options;
}
}
