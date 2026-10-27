#pragma once

#include "vk_context.h"

namespace gbtg::vk
{

// The Vulkan renderer half of the debug UI (gbtg/debug_ui.h). Draws with dynamic rendering into images of
// `color_format`. No-ops without GBTG_DEBUG_UI.
void debug_ui_init(Context& context, void* hwnd, VkFormat color_format, u32 image_count);
// Inside dynamic rendering onto the swapchain image.
void debug_ui_render(VkCommandBuffer command);
bool debug_ui_has_draws();

} // namespace gbtg::vk
