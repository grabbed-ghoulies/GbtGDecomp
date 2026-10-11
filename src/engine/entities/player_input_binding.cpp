#include "player_input_binding.h"
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x0002CAB0
void bind_runtime_player_input(RuntimePlayerInputBinding& context)
{
    i32 count = 0;
    for (auto* node = static_cast<entity_detail::ListHead*>(context.link.prev);
        node != nullptr; node = static_cast<entity_detail::ListHead*>(node->prev))
    {
        ++count;
    }
    char name[32];
    std::sprintf(name, "player%d", count);
    if ((context.flags & 0x800) != 0)
    {
        switch (read_u32(context.input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(context.input);
            break;
        case 4:
            release_runtime_composite_input(context.input, context.release);
            break;
        case 5:
            release_runtime_actor_input(context.input, context.release);
            break;
        default:
            break;
        }
        count = 0;
        for (auto* node = static_cast<entity_detail::ListHead*>(context.link.prev);
            node != nullptr; node = static_cast<entity_detail::ListHead*>(node->prev))
        {
            ++count;
        }
        const u32 address = context.port_devices[size_t(count) + 1];
        auto device = context.activation.devices.at(address);
        put(device, 8, u32(count) - 1);
        put(device, 0xC, context.actor_address);
        put(device, 4, 1);
        context.input = device;
        put(context.actor, 0x960, address);
        return;
    }
    switch (read_u32(context.input, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(context.input);
        break;
    case 4:
        release_runtime_composite_input(context.input, context.release);
        break;
    case 5:
        release_runtime_actor_input(context.input, context.release);
        break;
    default:
        break;
    }
    context.input = create_runtime_selected_input(context.release);
    const u32 address = context.register_input(context.input);
    put(context.actor, 0x960, address);
    push_runtime_input_entry(address, name, context.activation);
}

// Xbox 0x0007F870
void register_runtime_controlled_player(RuntimePlayerInputBinding& context)
{
    if (context.link.prev == nullptr)
    {
        auto* tail = &context.controlled;
        if (context.controlled.next != nullptr)
        {
            auto* node = static_cast<entity_detail::ListHead*>(context.controlled.next);
            do
            {
                tail = node;
                node = static_cast<entity_detail::ListHead*>(tail->next);
            } while (node != nullptr);
        }
        tail->next = &context.link;
        context.link.prev = tail;
        context.link.next = nullptr;
        put(context.actor, 0x93C, context.address_of(tail));
        put(context.actor, 0x940, 0);
    }
    bind_runtime_player_input(context);
}
}
