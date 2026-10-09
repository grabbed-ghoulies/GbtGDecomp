#include "background_model_nodes.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x00042C30
void initialize_background_model_nodes(RuntimeModelNode& first, u8 below_matrix_array,
    RuntimeModelNode* shader, i32& has_background_push_buffer, const BackgroundNodeViews& views)
{
    auto* node = &first;
    do
    {
        RuntimeModelNode* selected_shader = shader;
        if (std::strncmp(node->type_name, "ndPushBuffer", 13) == 0)
        {
            views.at(node)[0x3F] = 0;
        }
        else
        {
            if (std::strncmp(node->type_name, "ndBGPushBuffer", 15) == 0)
            {
                auto bytes = views.at(node);
                if (below_matrix_array != 0)
                {
                    auto store = [&](size_t offset, float value)
                    {
                        std::memcpy(bytes.data() + offset, &value, sizeof(value));
                    };
                    store(0x48, read_f32(bytes, 0x48) - 70.f); // DAT_0032B2C4
                    store(0x4C, read_f32(bytes, 0x4C) - 70.f);
                    store(0x50, read_f32(bytes, 0x50) - 70.f);
                    store(0x54, read_f32(bytes, 0x54) + 70.f);
                    store(0x58, read_f32(bytes, 0x58) + 70.f);
                    store(0x5C, read_f32(bytes, 0x5C) + 70.f);
                }
                bytes[0x3F] = 0;
                has_background_push_buffer = 1;
            }
            else
            {
                if (std::strncmp(node->type_name, "ndShaderParam2", 15) == 0)
                {
                    const u32 zero = 0;
                    std::memcpy(views.at(node).data() + 8, &zero, sizeof(zero));
                    selected_shader = node;
                }
            }
        }
        if (below_matrix_array == 0)
        {
            const bool matrix = std::strncmp(node->type_name, "ndMtxArray", 11) == 0;
            if (matrix)
            {
                if (node->child != nullptr)
                {
                    initialize_background_model_nodes(*node->child, 1, selected_shader,
                        has_background_push_buffer, views);
                }
            }
            else if (node->child != nullptr)
            {
                initialize_background_model_nodes(*node->child, 0, selected_shader,
                    has_background_push_buffer, views);
            }
            node = node->sibling;
            if (node == nullptr)
            {
                return;
            }
            below_matrix_array = 0;
        }
        else
        {
            if (node->child != nullptr)
            {
                initialize_background_model_nodes(*node->child, 1, selected_shader,
                    has_background_push_buffer, views);
            }
            node = node->sibling;
            if (node == nullptr)
            {
                return;
            }
            below_matrix_array = 1;
        }
    }
    while (true);
}
}
