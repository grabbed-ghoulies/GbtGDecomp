#include "shader_validation_channel_ops.h"
#include <cstdio>
#include <iostream>
using namespace gbtg;

int main()
{
    ShaderValidationChannelOrder order{0x00203794, {}};
    ShaderValidationChannelOrder other{0x00203794, {}};
    ShaderValidationChannelMask mask{0x00203790, {}};
    while (std::cin >> order.channels[0] >> order.channels[1] >> order.channels[2] >> order.channels[3])
    {
        for (auto& channel : other.channels)
        {
            std::cin >> channel;
        }
        for (auto& channel : mask.channels)
        {
            u32 value;
            std::cin >> value;
            channel = static_cast<u8>(value);
        }
        std::printf("%d %d %u %u %d", shader_validation_channel_order_covers_all(order) ? 1 : 0,
            shader_validation_channel_order_keeps_all(order) ? 1 : 0, find_shader_validation_uncovered_channel(order),
            find_shader_validation_moved_channel(order), shader_validation_channel_orders_compatible(order, other) ? 1 : 0);
        ShaderValidationChannelMask inverted{0, {9, 9, 9, 9}};
        const bool inverted_returned = invert_shader_validation_channel_mask(mask, inverted) == &inverted;
        std::printf(" %08X %02X%02X%02X%02X %d", inverted.vtable_address, inverted.channels[0], inverted.channels[1],
            inverted.channels[2], inverted.channels[3], inverted_returned ? 1 : 0);
        for (i32 position = 0; position <= 4; ++position)
        {
            std::printf(" %d", shader_validation_channel_repeats_before(order, position) ? 1 : 0);
        }
        std::printf(" %u", find_shader_validation_repeated_channel(order));
        ShaderValidationChannelOrder inverse{};
        const bool inverse_returned = invert_shader_validation_channel_order(order, inverse) == &inverse;
        std::printf(" %08X %08X %08X %08X %08X %d", inverse.vtable_address, inverse.channels[0], inverse.channels[1],
            inverse.channels[2], inverse.channels[3], inverse_returned ? 1 : 0);
        ShaderValidationChannelMask used{0, {9, 9, 9, 9}};
        const bool used_returned = shader_validation_channel_order_used_mask(order, used) == &used;
        std::printf(" %08X %02X%02X%02X%02X %d", used.vtable_address, used.channels[0], used.channels[1], used.channels[2],
            used.channels[3], used_returned ? 1 : 0);
        ShaderValidationChannelOrder cleared = order;
        clear_shader_validation_repeated_channels(cleared);
        std::printf(" %08X %08X %08X %08X\n", cleared.channels[0], cleared.channels[1], cleared.channels[2], cleared.channels[3]);
    }
}
