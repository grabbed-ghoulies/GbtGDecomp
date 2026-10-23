#include "dialogue_input.h"
#include "../entities/ghouly_spawn_record.h"
#include <cstring>
#include <bit>

namespace gbtg
{
// Native selection of the two retail jump tables used by button queries.
static const u8* button_state(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
        return device.record.data() + 8;
    case 3:
        return state.keyboard.data();
    case 5:
        return device.record.data() + 0x10;
    default:
        return nullptr;
    }
}

// Xbox 0x001554C0
u32 runtime_input_device_mask(const RuntimeDialogueInputState& state, i32 kind)
{
    return read_u32(state.device_masks, kind * 8);
}

// Xbox 0x00107CE0
u32 runtime_input_port_presence(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 8))
    {
    case 0:
        return runtime_input_device_mask(state, 0) & 0x10;
    case 1:
        return runtime_input_device_mask(state, 0) & 0x20;
    case 2:
        return runtime_input_device_mask(state, 0) & 0x40;
    case 3:
        return runtime_input_device_mask(state, 0) & 0x80;
    default:
        runtime_input_device_mask(state, 0);
        return 0;
    }
}

// Xbox 0x001082B0
u32 runtime_input_device_presence(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 2:
        return runtime_input_device_mask(state, 0) & state.selected_mask;
    case 3:
        return runtime_input_device_mask(state, 0);
    case 4:
    {
        i32 index = 0;
        while (device.children[index] == nullptr ||
            read_u32(device.children[index]->record, 4) == 0 ||
            runtime_input_port_presence(*device.children[index], state) != 0)
        {
            ++index;
            if (3 < index)
            {
                return 1;
            }
        }
        return 0;
    }
    case 5:
        return runtime_input_port_presence(device, state);
    default:
        return 0;
    }
}

// Xbox 0x00108300
u32 runtime_input_button_held(const RuntimeDialogueInputDevice& device, u16 mask,
    const RuntimeDialogueInputState& state)
{
    const u32 kind = read_u32(device.record, 0);
    switch (kind)
    {
    case 1:
    case 2:
    case 3:
    case 5:
        return (read_u16({button_state(device, state), 0x76}, 0x38) & mask) != 0;
    case 4:
    {
        i32 index = 0;
        do
        {
            const auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                if ((read_u16({button_state(child, state), 0x76}, 0x38) & mask) != 0)
                {
                    return 1;
                }
            }
            ++index;
            if (3 < index)
            {
                return 0;
            }
        }
        while (true);
    }
    default:
        return 0;
    }
}

// Xbox 0x001083F0
u32 runtime_input_button_edge(const RuntimeDialogueInputDevice& device, u16 mask,
    const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
    {
        const std::span<const u8> block{button_state(device, state), 0x76};
        if ((read_u16(block, 0x38) & mask) == 0 || (read_u16(block, 0x74) & mask) != 0)
        {
            return 0;
        }
        return 1;
    }
    case 4:
    {
        u32 result = 0;
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            const auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                const std::span<const u8> block{button_state(child, state), 0x76};
                u32 edge;
                if ((read_u16(block, 0x38) & mask) == 0 || (read_u16(block, 0x74) & mask) != 0)
                {
                    edge = 0;
                }
                else
                {
                    edge = 1;
                }
                result |= edge;
            }
            ++index;
            --remaining;
        }
        while (remaining != 0);
        return result;
    }
    default:
        return 0;
    }
}

// Xbox 0x00108500, EAX the device, BX the mask: a button let go this frame (held last frame, not now). A group
// (kind 4) answers for its first connected child that let go.
u32 runtime_input_button_release(const RuntimeDialogueInputDevice& device, u16 mask,
    const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
    {
        const std::span<const u8> block{button_state(device, state), 0x76};
        return (read_u16(block, 0x38) & mask) == 0 && (read_u16(block, 0x74) & mask) != 0 ? 1u : 0u;
    }
    case 4:
        for (i32 index = 0; index < 4; ++index)
        {
            const auto& child = *device.children[index];
            if (read_u32(child.record, 8) == 0xFFFFFFFFu)
            {
                continue;
            }
            const std::span<const u8> block{button_state(child, state), 0x76};
            if ((read_u16(block, 0x38) & mask) == 0 && (read_u16(block, 0x74) & mask) != 0)
            {
                return 1;
            }
        }
        return 0;
    default:
        return 0;
    }
}

// Native expression of the duplicated scalar normalization blocks in 0x001085F0.
static float normalize_input_axis(float value)
{
    const float positive_limit = std::bit_cast<float>(0x46FF7800u);
    const float negative_limit = std::bit_cast<float>(0xC6FF7800u);
    if (value > positive_limit)
    {
        value = positive_limit;
    }
    else if (negative_limit > value)
    {
        value = negative_limit;
        goto negative;
    }
    else if (!(value >= 0.f))
    {
        goto negative;
    }
    value = (value - std::bit_cast<float>(0x46192000u)) * std::bit_cast<float>(0x3837284Au);
    if (!(value > 1.f))
    {
        if (0.f > value)
        {
            value = 0.f;
        }
    }
    else
    {
        value = 1.f;
    }
    return value;
negative:
    value = ((value - std::bit_cast<float>(0xC6192000u)) * std::bit_cast<float>(0xB837284Au)) * -1.f;
    if (!(-1.f > value))
    {
        if (value > 0.f)
        {
            value = 0.f;
        }
    }
    else
    {
        value = -1.f;
    }
    return value;
}

// Xbox 0x00108AB0, EAX device, EDI output
void runtime_input_tertiary_axes(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state)
{
    const auto clamp = [](float value)
    {
        if (value > 1.f) // DAT_0032B0F8.
        {
            return 1.f;
        }
        if (-1.f > value) // DAT_0032B120.
        {
            return -1.f;
        }
        return value;
    };
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(output.data(), button_state(device, state) + 0x10, sizeof(output));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            const auto& child = *device.children[index];
            float horizontal = 0.f;
            float vertical = 0.f;
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                const u8* block = button_state(child, state);
                if (block == nullptr)
                {
                    throw Error("0x00108AB0 reads +0x10 of a group child's input state through address zero");
                }
                horizontal = clamp(read_f32({block + 0x10, 8}, 0));
                vertical = clamp(read_f32({block + 0x10, 8}, 4));
            }
            ++index;
            --remaining;
            output[0] = horizontal;
            output[1] = vertical;
        }
        while (remaining != 0);
        return;
    }
    default:
        output[1] = 0.f;
        output[0] = 0.f;
        return;
    }
}

// Xbox 0x001088D0
void runtime_input_secondary_axes(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(output.data(), button_state(device, state) + 8, sizeof(output));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            const auto& child = *device.children[index];
            float horizontal = 0.f;
            float vertical = 0.f;
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                const std::span<const u8> block{button_state(child, state) + 8, 8};
                vertical = read_f32(block, 4);
                horizontal = read_f32(block, 0);
                vertical = normalize_input_axis(vertical);
            }
            else
            {
                vertical = (vertical - std::bit_cast<float>(0x46192000u)) * std::bit_cast<float>(0x3837284Au);
                if (!(vertical > 1.f))
                {
                    if (0.f > vertical)
                    {
                        vertical = 0.f;
                    }
                }
                else
                {
                    vertical = 1.f;
                }
            }
            horizontal = normalize_input_axis(horizontal);
            ++index;
            --remaining;
            output[0] = horizontal;
            output[1] = vertical;
        }
        while (remaining != 0);
        return;
    }
    default:
        output[1] = 0.f;
        output[0] = 0.f;
        return;
    }
}

static u8* writable_button_state(RuntimeInputAxisWriteDevice& device, std::span<u8> keyboard)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
        return device.record.data() + 8;
    case 3:
        return keyboard.data();
    case 5:
        return device.record.data() + 0x10;
    default:
        return nullptr;
    }
}

// Xbox 0x00108FC0
void set_runtime_input_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(writable_button_state(device, keyboard), axes.data(), sizeof(axes));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                std::memcpy(writable_button_state(child, keyboard), axes.data(), sizeof(axes));
            }
            ++index;
            --remaining;
        }
        while (remaining != 0);
        return;
    }
    default:
        return;
    }
}

// Xbox 0x001090A0
void set_runtime_input_secondary_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(writable_button_state(device, keyboard) + 8, axes.data(), sizeof(axes));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                std::memcpy(writable_button_state(child, keyboard) + 8, axes.data(), sizeof(axes));
            }
            ++index;
            --remaining;
        }
        while (remaining != 0);
        return;
    }
    default:
        return;
    }
}

// Xbox 0x00109260
void set_runtime_input_vertical_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(writable_button_state(device, keyboard) + 0x10, axes.data(), sizeof(axes));
        return;
    case 4:
        {
            i32 index = 0;
            i32 remaining = 4;
            do
            {
                auto& child = *device.children[index];
                if (read_u32(child.record, 8) != 0xFFFFFFFFu)
                {
                    std::memcpy(writable_button_state(child, keyboard) + 0x10, axes.data(), sizeof(axes));
                }
                ++index;
                --remaining;
            }
            while (remaining != 0);
            return;
        }
    default:
        return;
    }
}

// Xbox 0x00109180
void set_runtime_input_trigger_axes(RuntimeInputAxisWriteDevice& device,
    const std::array<float, 2>& axes, std::span<u8> keyboard)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(writable_button_state(device, keyboard) + 0x30, axes.data(), sizeof(axes));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                std::memcpy(writable_button_state(child, keyboard) + 0x30, axes.data(), sizeof(axes));
            }
            ++index;
            --remaining;
        }
        while (remaining != 0);
        return;
    }
    default:
        return;
    }
}

// Xbox 0x001085F0
void runtime_input_axes(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        std::memcpy(output.data(), button_state(device, state), sizeof(output));
        return;
    case 4:
    {
        i32 index = 0;
        i32 remaining = 4;
        do
        {
            const auto& child = *device.children[index];
            float horizontal = 0.f;
            float vertical = 0.f;
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                const std::span<const u8> block{button_state(child, state), 8};
                vertical = read_f32(block, 4);
                horizontal = read_f32(block, 0);
                vertical = normalize_input_axis(vertical);
            }
            else
            {
                // The disconnected branch jumps straight to the nonnegative
                // transform at 0x001086E3; its zero falls inside the dead zone.
                vertical = (vertical - std::bit_cast<float>(0x46192000u)) * std::bit_cast<float>(0x3837284Au);
                if (!(vertical > 1.f))
                {
                    if (0.f > vertical)
                    {
                        vertical = 0.f;
                    }
                }
                else
                {
                    vertical = 1.f;
                }
            }
            horizontal = normalize_input_axis(horizontal);
            ++index;
            --remaining;
            output[0] = horizontal;
            output[1] = vertical;
        }
        while (remaining != 0);
        return;
    }
    default:
        output[1] = 0.f;
        output[0] = 0.f;
        return;
    }
}

namespace
{
// The state block a group child reads: kinds 4 and unknown select address zero, which retail dereferences.
const u8* analog_child_state(const RuntimeDialogueInputDevice& child, const RuntimeDialogueInputState& state)
{
    const u8* block = button_state(child, state);
    if (block == nullptr)
    {
        throw Error("input group child of kind " + std::to_string(read_u32(child.record, 0))
            + " reads its analog level from address zero");
    }
    return block;
}

float runtime_input_analog_level(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state, size_t offset)
{
    switch (read_u32(device.record, 0))
    {
    case 1:
    case 2:
    case 3:
    case 5:
        return read_f32({button_state(device, state), offset + 4}, offset);
    case 4:
    {
        float sum = 0.f;
        for (i32 index = 0; index < 4; ++index)
        {
            const auto& child = *device.children[index];
            if (read_u32(child.record, 8) != 0xFFFFFFFFu)
            {
                sum = read_f32({analog_child_state(child, state), offset + 4}, offset) + sum;
            }
        }
        constexpr float kLimit = std::bit_cast<float>(0x430C0000u); // DAT_0032B138, 140.
        if (sum > kLimit)
        {
            sum = kLimit;
        }
        return sum * std::bit_cast<float>(0x3BEA0EA1u); // DAT_0032B134, 1/140.
    }
    default:
        return std::bit_cast<float>(0xBF800000u); // DAT_0032B120, -1.
    }
}
}

// Xbox 0x00108DE0
float runtime_input_first_analog_level(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state)
{
    return runtime_input_analog_level(device, state, 0x30);
}

// Xbox 0x00108ED0
float runtime_input_second_analog_level(const RuntimeDialogueInputDevice& device,
    const RuntimeDialogueInputState& state)
{
    return runtime_input_analog_level(device, state, 0x34);
}

// Xbox 0x001429E0
void dialogue_input_direction(const RuntimeDialogueInputDevice& device,
    std::array<float, 2>& output, const RuntimeDialogueInputState& state)
{
    output[1] = 0.f;
    output[0] = 0.f;
    runtime_input_axes(device, output, state);
    if (0.2f > output[0] && output[0] > -0.2f)
    {
        output[0] = 0.f;
    }
    if (0.2f > output[1] && output[1] > -0.2f)
    {
        output[1] = 0.f;
    }
    if (runtime_input_button_held(device, 4, state) == 0)
    {
        if (runtime_input_button_held(device, 8, state) != 0)
        {
            output[0] = 1.f;
        }
    }
    else
    {
        output[0] = -1.f;
    }
    if (runtime_input_button_held(device, 2, state) != 0)
    {
        output[1] = -1.f;
        return;
    }
    if (runtime_input_button_held(device, 1, state) != 0)
    {
        output[1] = 1.f;
    }
}

// Xbox 0x000335A0
u32 activate_dialogue_input(RuntimeDialogueInputState& state)
{
    if (state.activation_mode == 1)
    {
        if (state.activated != 0)
        {
            state.activated = 1;
            return 1;
        }
        if ((state.devices->cursor - state.devices->begin) / state.devices->stride != 0)
        {
            const RuntimeDialogueInputDevice* device;
            const auto* row = state.devices->begin + (state.selected - 1) * state.devices->stride;
            if (state.device_views != nullptr)
            {
                // Resolve the retail address without widening the 0x50-byte records.
                const u32 address = read_u32({row, state.devices->stride}, 0);
                device = address == 0 ? nullptr : &state.device_views->at(address);
            }
            else
            {
                std::memcpy(&device, row, sizeof(device));
            }
            if (device != nullptr && read_u32(device->record, 4) != 0 &&
                runtime_input_device_presence(*device, state) == 0)
            {
                state.activated = 1;
                return 1;
            }
        }
    }
    return 0;
}
}
