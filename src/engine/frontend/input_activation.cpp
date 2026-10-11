#include "input_activation.h"
#include "gbtg/entity.h"
#include "../entities/ghouly_box_decoder.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00109620
bool runtime_input_is_selected(std::span<const u8> device, u32 address,
    const RuntimeInputActivationContext& context)
{
    u32 selected = 0;
    if ((context.entries->cursor - context.entries->begin) / context.entries->stride != 0)
    {
        selected = read_u32({context.entries->begin +
            (context.selected - 1) * context.entries->stride, context.entries->stride}, 0);
    }
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
    case 3:
    case 4:
        return selected == address;
    case 5:
        return selected == read_u32(device, 0xB8);
    default:
        return selected == 0;
    }
}

// Xbox 0x00107E20
void clear_runtime_input_states(std::span<u8> device)
{
    const u32 zero = 0;
    const u16 half = 0;
    for (u32 offset : {0x38u, 0x3Cu, 0xCu, 8u, 0x14u, 0x10u, 0x1Cu, 0x18u,
        0x34u, 0x30u, 0x2Cu, 0x28u, 0x24u, 0x20u})
    {
        std::memcpy(device.data() + offset, &zero, 4);
    }
    std::memcpy(device.data() + 0x40, &half, 2);
    std::memcpy(device.data() + 0x7C, &half, 2);
    for (u32 offset : {0x74u, 0x78u, 0x48u, 0x44u, 0x50u, 0x4Cu, 0x58u, 0x54u,
        0x70u, 0x6Cu, 0x68u, 0x64u, 0x60u, 0x5Cu})
    {
        std::memcpy(device.data() + offset, &zero, 4);
    }
}

// Xbox 0x00107B90
void clear_runtime_actor_input_states(std::span<u8> device)
{
    const u32 zero = 0;
    const u16 half = 0;
    for (u32 offset : {0x40u, 0x44u, 0x14u, 0x10u, 0x1Cu, 0x18u, 0x24u, 0x20u,
        0x3Cu, 0x38u, 0x34u, 0x30u, 0x2Cu, 0x28u})
    {
        std::memcpy(device.data() + offset, &zero, 4);
    }
    std::memcpy(device.data() + 0x48, &half, 2);
    std::memcpy(device.data() + 0x84, &half, 2);
    for (u32 offset : {0x7Cu, 0x80u, 0x50u, 0x4Cu, 0x58u, 0x54u, 0x60u, 0x5Cu,
        0x78u, 0x74u, 0x70u, 0x6Cu, 0x68u, 0x64u})
    {
        std::memcpy(device.data() + offset, &zero, 4);
    }
}

// Xbox 0x001079D0
void reset_runtime_composite_input(std::span<u8> device, const RuntimeInputReleaseContext& context)
{
    i32 offset = 8;
    i32 remaining = 4;
    do
    {
        const u32 child = read_u32(device, offset);
        if (child != 0)
        {
            reset_runtime_input_device(context.addresses.at(child), context);
        }
        offset += 4;
        --remaining;
    }
    while (remaining != 0);
    engine_heap_free(device.data());
}

// Xbox 0x00108270
void reset_runtime_input_device(std::span<u8> device, const RuntimeInputReleaseContext& context)
{
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
        clear_runtime_input_states(device);
        return;
    case 4:
        reset_runtime_composite_input(device, context);
        return;
    case 5:
        clear_runtime_actor_input_states(device);
        return;
    default:
        return;
    }
}

// Xbox 0x00107EF0
std::span<u8> create_runtime_keyboard_input(const RuntimeInputReleaseContext& context)
{
    std::span<u8> device{static_cast<u8*>(engine_heap_allocate(8)), 8};
    const u32 zero = 0;
    std::memcpy(device.data(), &zero, 4);
    std::memcpy(device.data() + 4, &zero, 4);
    const u32 active = 1;
    const u32 kind = 3;
    std::memcpy(device.data() + 4, &active, 4);
    std::memcpy(device.data(), &kind, 4);
    reset_runtime_input_device(device, context);
    return device;
}

// Xbox 0x00107D60
std::span<u8> create_runtime_selected_input(const RuntimeInputReleaseContext& context)
{
    std::span<u8> device{static_cast<u8*>(engine_heap_allocate(0xB0)), 0xB0};
    std::memset(device.data(), 0, 0xB0);
    const u32 active = 1;
    const u32 kind = 2;
    std::memcpy(device.data() + 4, &active, 4);
    std::memcpy(device.data(), &kind, 4);
    reset_runtime_input_device(device, context);
    return device;
}

// Xbox 0x00107830
std::span<u8> create_runtime_actor_base_input(const RuntimeInputReleaseContext& context)
{
    std::span<u8> device{static_cast<u8*>(engine_heap_allocate(0xB0)), 0xB0};
    std::memset(device.data(), 0, 0xB0);
    const u32 inactive = 0;
    const u32 kind = 1;
    std::memcpy(device.data() + 4, &inactive, 4);
    std::memcpy(device.data(), &kind, 4);
    reset_runtime_input_device(device, context);
    return device;
}

// Xbox 0x00109340
void push_runtime_input_entry(u32 address, const char* name, RuntimeInputActivationContext& context)
{
    const char* entry_name = name;
    const u32 zero = 0;
    if (context.selected != 0)
    {
        auto* selected = context.entries->begin + (context.selected - 1) * context.entries->stride;
        std::memcpy(selected + 0x48, &zero, 4);
    }
    if (context.entries->cursor == context.entries->limit)
    {
        context.entries = grow_ghouly_spawn_pool(context.entries);
    }
    auto* row = context.entries->cursor;
    context.entries->cursor += context.entries->stride;
    std::memcpy(row, &address, 4);
    std::memcpy(row + 0x48, &zero, 4);
    std::memcpy(row + 0x4C, &zero, 4);
    char unit;
    u8* output = row + 4;
    do
    {
        unit = *name++;
        *output++ = static_cast<u8>(unit);
    }
    while (unit != 0);
    ++context.selected;
    activate_runtime_input_entry(entry_name, context);
}

// Xbox 0x00109470
void refresh_runtime_input_device(u32 address, u32 frame, RuntimeInputActivationContext& context,
    const RuntimeInputReleaseContext& release)
{
    auto object = context.devices.at(address);
    const u32 selector = read_u32(object, 0) - 1;
    u32 key = 0;
    switch (selector)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        key = address;
        break;
    case 4:
        key = read_u32(object, 0xB8);
    }
    u8* record = nullptr;
    for (u8* row = context.entries->begin; row < context.entries->cursor; row += 0x50)
    {
        if (read_u32({row, 0x50}, 0) == key)
        {
            record = row;
            break;
        }
    }
    u32 kept = 0;
    switch (selector)
    {
    case 1:
    case 2:
    case 4:
        kept = address;
    }
    if (kept != key || read_u32({record, 0x50}, 0x4C) != frame)
    {
        if (read_u32({record, 0x50}, 0x48) == 0)
        {
            switch (selector)
            {
            case 0:
            case 1:
                clear_runtime_input_states(object);
                std::memcpy(record + 0x4C, &frame, 4);
                return;
            case 3:
                reset_runtime_composite_input(object, release);
                break;
            case 4:
                clear_runtime_actor_input_states(object);
                std::memcpy(record + 0x4C, &frame, 4);
                return;
            }
        }
        else
        {
            switch (selector)
            {
            case 0:
                reset_runtime_analog_input(object);
                std::memcpy(record + 0x4C, &frame, 4);
                return;
            case 1:
                reset_runtime_selected_input(object, context);
                std::memcpy(record + 0x4C, &frame, 4);
                return;
            case 4:
                reset_runtime_port_input(object, context);
                std::memcpy(record + 0x4C, &frame, 4);
                return;
            }
        }
        std::memcpy(record + 0x4C, &frame, 4);
    }
}

// Xbox 0x00107DD0
void release_runtime_simple_input(std::span<u8> device)
{
    const u32 zero = 0;
    std::memcpy(device.data() + 4, &zero, 4);
    engine_heap_free(device.data());
}

// Xbox 0x00107A40
void release_runtime_input_slot(std::span<u8> pool, i32 slot,
    const RuntimeInputReleaseContext& context)
{
    const u32 address = read_u32(pool, 8 + slot * 4);
    engine_heap_free(context.addresses.at(address).data());
    const u32 zero = 0;
    std::memcpy(pool.data() + 8 + slot * 4, &zero, 4);
}

// Xbox 0x00107B60
void release_runtime_actor_input(std::span<u8> device,
    const RuntimeInputReleaseContext& context)
{
    const u32 zero = 0;
    std::memcpy(device.data() + 4, &zero, 4);
    const i32 slot = static_cast<i32>(read_u32(device, 8));
    if (slot == -1)
    {
        engine_heap_free(device.data());
        return;
    }
    auto owner = context.addresses.at(read_u32(device, 0xC));
    std::memcpy(owner.data() + 0x960, &zero, 4);
    release_runtime_input_slot(context.addresses.at(read_u32(device, 0xB8)), slot, context);
}

// Xbox 0x00107960
void release_runtime_composite_input(std::span<u8> device,
    const RuntimeInputReleaseContext& context)
{
    const u32 zero = 0;
    std::memcpy(device.data() + 4, &zero, 4);
    i32 offset = 8;
    i32 remaining = 4;
    do
    {
        const u32 child = read_u32(device, offset);
        if (child != 0)
        {
            release_runtime_input_child(context.addresses.at(child), context);
            std::memcpy(device.data() + offset, &zero, 4);
        }
        offset += 4;
        --remaining;
    }
    while (remaining != 0);
    engine_heap_free(device.data());
}

// Xbox 0x001081E0
void release_runtime_input_child(std::span<u8> device,
    const RuntimeInputReleaseContext& context)
{
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(device);
        return;
    case 4:
        release_runtime_composite_input(device, context);
        return;
    case 5:
        release_runtime_actor_input(device, context);
        return;
    default:
        return;
    }
}

// Xbox 0x00108230
void release_runtime_input_device(std::span<u8> device,
    const RuntimeInputReleaseContext& context)
{
    switch (read_u32(device, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(device);
        return;
    case 4:
        release_runtime_composite_input(device, context);
        return;
    case 5:
        release_runtime_actor_input(device, context);
        return;
    default:
        return;
    }
}

namespace
{
void copy_state_bytes(u8* output, const u8* input)
{
    u32 remaining = 0x3C;
    if (output != input)
    {
        if ((reinterpret_cast<std::uintptr_t>(output) & 7) == 0
            && (reinterpret_cast<std::uintptr_t>(input) & 7) == 0)
        {
            i32 count = 7;
            do
            {
                std::memcpy(output, input, 4);
                std::memcpy(output + 4, input + 4, 4);
                output += 8;
                input += 8;
                remaining -= 8;
                --count;
            }
            while (count != 0);
        }
        if ((reinterpret_cast<std::uintptr_t>(output) & 3) == 0
            && (reinterpret_cast<std::uintptr_t>(input) & 3) == 0 && remaining > 3)
        {
            u32 count = remaining >> 2;
            do
            {
                std::memcpy(output, input, 4);
                output += 4;
                input += 4;
                remaining -= 4;
                --count;
            }
            while (count != 0);
        }
        if (remaining != 0)
        {
            do
            {
                *output++ = *input++;
                --remaining;
            }
            while (remaining != 0);
        }
    }
}
}

// Xbox 0x000FA110
void snapshot_runtime_input_state(u8* state)
{
    copy_state_bytes(state + 0x3C, state);
}

// Xbox 0x000FA190
void copy_runtime_input_state(u8* output, const u8* input)
{
    copy_state_bytes(output, input);
}

// Xbox 0x000FACD0
i32 runtime_controller_mask_index(u32 mask)
{
    switch (mask)
    {
    case 1: return 0;
    case 2: return 1;
    case 4: return 2;
    case 8: return 3;
    default: return -1;
    }
}

// Xbox 0x00107FF0
i32 runtime_player_mask_index(u32 mask)
{
    switch (mask)
    {
    case 0x10: return 0;
    case 0x20: return 1;
    case 0x40: return 2;
    case 0x80: return 3;
    default: return -1;
    }
}

// Xbox 0x00109720
i32 runtime_player_controller_index(u32 mask, const u32* ports)
{
    return runtime_controller_mask_index(ports[runtime_player_mask_index(mask)]);
}

// Xbox 0x000FABF0
const u8* runtime_input_state_for_mask(u32 mask, const RuntimeInputActivationContext& context)
{
    switch (mask)
    {
    case 1:
    case 2:
    case 4:
    case 8:
        return context.controllers.data() + runtime_controller_mask_index(mask) * 0xA8;
    case 0x10:
    case 0x20:
    case 0x40:
    case 0x80:
        return context.controllers.data() + runtime_player_controller_index(mask, context.port_masks) * 0xA8;
    case 0xFFFFFFFFu:
        return context.keyboard.data();
    default:
        return nullptr;
    }
}

// Xbox 0x00107C60
u32 runtime_input_port_mask(std::span<const u8> device, const RuntimeInputActivationContext& context)
{
    u32 mask;
    switch (read_u32(device, 8))
    {
    case 0: mask = 0x10; break;
    case 1: mask = 0x20; break;
    case 2: mask = 0x40; break;
    case 3: mask = 0x80; break;
    default: mask = 0; break;
    }
    return context.port_masks[runtime_player_mask_index(mask)];
}

// Xbox 0x001078A0
void reset_runtime_analog_input(std::span<u8> device)
{
    snapshot_runtime_input_state(device.data() + 8);
    const u16 zero = 0;
    std::memcpy(device.data() + 0x40, &zero, 2);
    constexpr u32 offsets[]{0x38, 0x3C, 0xC, 8, 0x14, 0x10, 0x1C, 0x18,
        0x34, 0x30, 0x2C, 0x28, 0x24, 0x20};
    const u32 word = 0;
    for (const u32 offset : offsets)
    {
        std::memcpy(device.data() + offset, &word, 4);
    }
}

// Xbox 0x00107EC0
void reset_runtime_selected_input(std::span<u8> device, const RuntimeInputActivationContext& context)
{
    snapshot_runtime_input_state(device.data() + 8);
    const auto* current = runtime_input_state_for_mask(context.port_masks[0], context);
    copy_runtime_input_state(device.data() + 8, current);
}

// Xbox 0x00107C30
void reset_runtime_port_input(std::span<u8> device, const RuntimeInputActivationContext& context)
{
    snapshot_runtime_input_state(device.data() + 0x10);
    const u32 mask = runtime_input_port_mask(device, context);
    const auto* current = runtime_input_state_for_mask(mask, context);
    copy_runtime_input_state(device.data() + 0x10, current);
}

// Xbox 0x001096A0
u8* find_runtime_input_entry(const char* name, RuntimeInputActivationContext& context)
{
    u8* row = context.entries->begin;
    while (row < context.entries->cursor)
    {
        const u8* candidate = row + 4;
        const auto* expected = reinterpret_cast<const u8*>(name);
        u8 a;
        u8 b;
        do
        {
            a = *candidate;
            if (a > 0x20)
            {
                a &= 0xDF;
            }
            b = *expected;
            if (b > 0x20)
            {
                b &= 0xDF;
            }
            if (a != b)
            {
                break;
            }
            ++candidate;
            ++expected;
            if (a == 0)
            {
                return row;
            }
        }
        while (true);
        if (a == b)
        {
            return row;
        }
        row += 0x50;
    }
    return nullptr;
}

// Xbox 0x001095B0
u32 activate_runtime_input_entry(const char* name, RuntimeInputActivationContext& context)
{
    auto* selected = find_runtime_input_entry(name, context);
    const u32 active = 1;
    const u32 inactive = 0;
    std::memcpy(selected + 0x48, &active, 4);
    for (u8* row = context.entries->begin; row < context.entries->cursor; row += 0x50)
    {
        if (row != selected)
        {
            std::memcpy(row + 0x48, &inactive, 4);
        }
    }
    return 1;
}

// Xbox 0x001093C0
void pop_runtime_input_entry(RuntimeInputActivationContext& context)
{
    const u32 stride = context.entries->stride;
    u8* removed = context.entries->begin + (context.selected - 1) * stride;
    if (removed < context.entries->cursor - stride)
    {
        std::memmove(removed, removed + stride, context.entries->cursor - stride - removed);
    }
    context.entries->cursor -= stride;
    --context.selected;
    if ((context.entries->cursor - context.entries->begin) / stride != 0)
    {
        auto* entry = context.entries->begin + (context.selected - 1) * stride;
        activate_runtime_input_entry(reinterpret_cast<const char*>(entry + 4), context);
        auto device = context.devices.at(read_u32({entry, stride}, 0));
        switch (read_u32(device, 0))
        {
        case 1:
            reset_runtime_analog_input(device);
            return;
        case 2:
            reset_runtime_selected_input(device, context);
            return;
        case 5:
            reset_runtime_port_input(device, context);
        }
    }
}
}
