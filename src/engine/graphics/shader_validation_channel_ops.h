#pragma once
#include "shader_validation_accessors.h"

namespace gbtg
{
// Channel orders hold source channels 0..3 per destination; 4 marks a missing channel.
// Position arguments are 0..3 at every retail call site.
bool shader_validation_channel_order_covers_all(const ShaderValidationChannelOrder& order); // Xbox 0x0018E6DF
bool shader_validation_channel_order_keeps_all(const ShaderValidationChannelOrder& order); // Xbox 0x0018F394
u32 find_shader_validation_uncovered_channel(const ShaderValidationChannelOrder& order); // Xbox 0x0018E5E2
u32 find_shader_validation_moved_channel(const ShaderValidationChannelOrder& order); // Xbox 0x0018E69C
bool shader_validation_channel_orders_compatible(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder other); // Xbox 0x0018E741
ShaderValidationChannelMask* invert_shader_validation_channel_mask(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask& result); // Xbox 0x0018E548
bool shader_validation_channel_repeats_before(const ShaderValidationChannelOrder& order, i32 position); // Xbox 0x0018E604
u32 find_shader_validation_repeated_channel(const ShaderValidationChannelOrder& order); // Xbox 0x0018E63F
void clear_shader_validation_repeated_channels(ShaderValidationChannelOrder& order); // Xbox 0x0018E661
ShaderValidationChannelOrder* invert_shader_validation_channel_order(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder& result); // Xbox 0x0018E700
ShaderValidationChannelMask* intersect_shader_validation_channel_masks(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask other, ShaderValidationChannelMask& result); // Xbox 0x0018E4EB
ShaderValidationChannelMask* unite_shader_validation_channel_masks(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask other, ShaderValidationChannelMask& result); // Xbox 0x0018E48C
ShaderValidationChannelMask* shader_validation_owner_channel_mask(const ShaderValidationOwner& owner,
    ShaderValidationChannelMask& result); // Xbox 0x0018F595
ShaderValidationChannelMask* shader_validation_operand_source_mask(const ShaderValidationOperand& operand,
    ShaderValidationChannelMask mask, ShaderValidationChannelMask& result); // Xbox 0x0018F86B
ShaderValidationChannelOrder* compose_shader_validation_channel_orders(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder other, ShaderValidationChannelOrder& result); // Xbox 0x0018F436
ShaderValidationChannelMask* shader_validation_channel_order_used_mask(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelMask& result); // Xbox 0x0018F355
}
