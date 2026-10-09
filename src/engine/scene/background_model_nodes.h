#pragma once

#include "gbtg/model_runtime.h"

namespace gbtg
{
using BackgroundNodeViews = std::unordered_map<const RuntimeModelNode*, std::span<u8>>;
void initialize_background_model_nodes(RuntimeModelNode& first, u8 below_matrix_array,
    RuntimeModelNode* shader, i32& has_background_push_buffer,
    const BackgroundNodeViews& views); // Xbox 0x00042C30
}
