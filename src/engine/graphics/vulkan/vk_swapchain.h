#pragma once

#include "vk_context.h"

#include <vector>

namespace gbtg::vk
{

// The window's swapchain. The backend renders into its own frame image and copies that onto an acquired image,
// so the swapchain follows the window's size while the game keeps rendering at the configured size.
struct Swapchain
{
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    // One per image: a present may still wait on it when the image is acquired again.
    std::vector<VkSemaphore> rendered;
    VkSemaphore acquired = VK_NULL_HANDLE;
    int vsync = 1;
    bool stale = false; // Rebuild before the next acquire (resize, out of date).
};

// D3D11's SyncInterval: 0 presents immediately (mailbox, else immediate); 1 and 2 use FIFO, which has no
// every-other-vblank mode.
void swapchain_create(Context& context, Swapchain& swapchain, int vsync);
void swapchain_destroy(Context& context, Swapchain& swapchain);
// Rebuilds when stale. False while the window has no area (minimized).
bool swapchain_ready(Context& context, Swapchain& swapchain);

} // namespace gbtg::vk
