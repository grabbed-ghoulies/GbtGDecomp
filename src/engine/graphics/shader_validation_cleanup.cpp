#include "shader_validation_cleanup.h"

namespace gbtg
{
// Xbox 0x0018E5BC
void destroy_shader_validation_channel_order(ShaderValidationChannelOrder& order)
{
    order.vtable_address = 0x00203794;
}

// Xbox 0x0018E43B
void destroy_shader_validation_channel_mask(ShaderValidationChannelMask& mask)
{
    mask.vtable_address = 0x00203790;
}

// Xbox 0x0018F281
void destroy_shader_validation_child_nodes(ShaderValidationArenaNode* node)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_child_nodes(node->first);
        destroy_shader_validation_child_nodes(node->second);
        node->vtable->destroy(*node, 1);
    }
}

// Xbox 0x001901D6
void clear_shader_validation_child_tree(ShaderValidationChildTree& tree)
{
    destroy_shader_validation_child_nodes(tree.root);
    tree.root = nullptr;
    tree.count = 0;
}

// Xbox 0x001909A6
void destroy_shader_validation_child_tree(ShaderValidationChildTree& tree)
{
    tree.vtable_address = 0x002037b8;
    clear_shader_validation_child_tree(tree);
}

// Xbox 0x0018F0AC
void release_shader_validation_allocation(void* allocation, const std::function<void(void*, u32)>& release)
{
    if (allocation != nullptr)
    {
        release(allocation, 0x24870000);
    }
}
}
