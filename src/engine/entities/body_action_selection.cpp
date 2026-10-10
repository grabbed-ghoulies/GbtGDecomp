#include "body_action_selection.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0001B020
void notify_runtime_body_animation(RuntimeBodyActionSelection& context)
{
    auto* first = context.first_receiver;
    auto* second = context.second_receiver;
    if (first != nullptr || second != nullptr)
    {
        RuntimeBodyAnimationNotice notice{0x28, context.child.actor_address, nullptr, 0, 0.f};
        auto* controller = context.animation.controller.get();
        if (controller != nullptr && controller->slots[0].player != nullptr)
        {
            notice.position = controller->slots[0].player->position;
        }
        controller = context.animation.controller.get();
        if (controller != nullptr && controller->slots[0].player != nullptr)
        {
            notice.record = controller->slots[0].record;
        }
        if (first != nullptr)
        {
            first->message(notice);
        }
        if (second != nullptr)
        {
            second->message(notice);
        }
    }
}

// Xbox 0x0001AF10
const AnimationRecord* select_runtime_body_animation(const char* name, RuntimeBodyActionSelection& context)
{
    const char* variant = nullptr;
    if (context.attached != nullptr)
    {
        variant = reinterpret_cast<const char*>(context.attached->bytes.data() + 0xDB0);
        if (variant == nullptr || *variant == '\0')
        {
            variant = nullptr;
        }
    }
    const AnimationRecord* record = nullptr;
    if (context.animation.table != nullptr)
    {
        record = find_actor_animation(context.animation.table, name, variant, context.random);
        if (record == nullptr && variant != nullptr)
        {
            record = find_actor_animation(context.animation.table, name, nullptr, context.random);
        }
        if (record != nullptr)
        {
            start_actor_animation(context.animation, *record, context.registry, context.error, context.random);
            notify_runtime_body_animation(context);
            return record;
        }
    }
    notify_runtime_body_animation(context);
    return nullptr;
}

namespace
{
ClonedExecutableResource action_parameters(const char* name, const std::span<const u8>* source,
    RuntimeBodyActionSelection& context)
{
    if (source == nullptr)
    {
        return clone_executable_resource(context.child.resources, name);
    }
    ClonedExecutableResource clone;
    clone.size = read_u16(*source, 0);
    clone.bytes.reset(new (std::nothrow) u8[clone.size]);
    if (clone.bytes != nullptr)
    {
        std::memcpy(clone.bytes.get(), source->data(), clone.size);
    }
    return clone;
}
}

// Xbox 0x00027830
void create_runtime_body_first_action(const char* name, const RuntimeBodyActionTemplates* parameters,
    RuntimeBodyActionSelection& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return;
    }
    auto clone = action_parameters(name, parameters != nullptr ? &parameters->first : nullptr, context);
    std::memcpy(clone.bytes.get() + 8, &context.body_address, 4);
    context.first = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
    clone.bytes.reset();
}

// Xbox 0x00027890
void create_runtime_body_second_action(const char* name, const RuntimeBodyActionTemplates* parameters,
    RuntimeBodyActionSelection& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return;
    }
    auto clone = action_parameters(name, parameters != nullptr ? &parameters->second : nullptr, context);
    std::memcpy(clone.bytes.get() + 8, &context.body_address, 4);
    context.second = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
    clone.bytes.reset();
}

// Xbox 0x000276A0
bool select_runtime_body_action_row(std::span<u8> body, const u8* row,
    const RuntimeBodyActionTemplates* parameters, RuntimeBodyActionSelection& context)
{
    const u16 key = read_u16({row, 0x108}, 0);
    if (context.first.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.first);
        context.first = {};
    }
    if (context.second.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.second);
        context.second = {};
    }
    context.row = nullptr;
    const u16 zero = 0;
    std::memcpy(body.data() + 0x748, &zero, 2);
    const u32 name = read_u32({row, 0x108}, 0x104);
    if (name != 0)
    {
        select_runtime_body_animation(context.names.at(name), context);
    }
    context.row = row;
    std::memcpy(body.data() + 0x748, &key, 2);
    create_runtime_body_first_action(reinterpret_cast<const char*>(row + 0x82), parameters, context);
    create_runtime_body_second_action(reinterpret_cast<const char*>(row + 2), parameters, context);
    if (read_u32(body, 0x74C) == 0)
    {
        body[0x6C] = 0;
        const u32 zero_word = 0;
        std::memcpy(body.data() + 0x84, &zero_word, 4);
    }
    return true;
}

// Xbox 0x00027650
bool select_runtime_body_action(std::span<u8> body, u16 key, RuntimeBodyActionSelection& context)
{
    if (key != 0 && context.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.table, key, parameters);
        if (row != nullptr)
        {
            // 0x00027510 writes a zero optional-template token on every match.
            select_runtime_body_action_row(body, row, nullptr, context);
            return true;
        }
    }
    return false;
}
}
