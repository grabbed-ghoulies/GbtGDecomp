#pragma once
#include "shader_validation_arena.h"
#include "shader_validation_channels.h"
#include <functional>

namespace gbtg
{
struct ShaderValidationChildTree
{
    u32 vtable_address; // Retail +0x00.
    u32 retained; // Retail +0x04, untouched by these destructors.
    ShaderValidationArenaNode* root; // Retail +0x08.
    u32 count; // Retail +0x0c.
};

void destroy_shader_validation_channel_order(ShaderValidationChannelOrder& order); // Xbox 0x0018E5BC
void destroy_shader_validation_channel_mask(ShaderValidationChannelMask& mask); // Xbox 0x0018E43B
void destroy_shader_validation_child_nodes(ShaderValidationArenaNode* node); // Xbox 0x0018F281
void clear_shader_validation_child_tree(ShaderValidationChildTree& tree); // Xbox 0x001901D6
void destroy_shader_validation_child_tree(ShaderValidationChildTree& tree); // Xbox 0x001909A6
void release_shader_validation_allocation(void* allocation, const std::function<void(void*, u32)>& release); // Xbox 0x0018F0AC
}
