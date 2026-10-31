#include "vk_swapchain.h"

#include "gbtg/log.h"

#include <algorithm>

namespace gbtg::vk
{
namespace
{

void release_images(Context& context, Swapchain& swapchain)
{
    for (VkImageView view : swapchain.views)
    {
        vkDestroyImageView(context.device, view, nullptr);
    }
    for (VkSemaphore semaphore : swapchain.rendered)
    {
        vkDestroySemaphore(context.device, semaphore, nullptr);
    }
    swapchain.views.clear();
    swapchain.rendered.clear();
    swapchain.images.clear();
}

// Creates the swapchain for the surface's current extent. False when that extent is empty.
bool build(Context& context, Swapchain& swapchain)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physical, context.surface, &capabilities),
          "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xFFFFFFFFu && extent.height == 0xFFFFFFFFu)
    {
        extent.width = std::clamp(swapchain.window_extent.width, capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(swapchain.window_extent.height, capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0)
    {
        return false;
    }
    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical, context.surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical, context.surface, &format_count, formats.data());
    // D3D11 presents DXGI_FORMAT_B8G8R8A8_UNORM with no colour-space conversion.
    VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen = format;
        }
    }
    if (chosen.format != VK_FORMAT_B8G8R8A8_UNORM)
    {
        throw Error("vulkan: the window surface has no B8G8R8A8_UNORM format");
    }
    u32 mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.physical, context.surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.physical, context.surface, &mode_count, modes.data());
    const auto supports = [&](VkPresentModeKHR mode) { return std::find(modes.begin(), modes.end(), mode) != modes.end(); };
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (swapchain.vsync == 0)
    {
        mode = supports(VK_PRESENT_MODE_MAILBOX_KHR) ? VK_PRESENT_MODE_MAILBOX_KHR
            : supports(VK_PRESENT_MODE_IMMEDIATE_KHR) ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
    }

    VkSwapchainKHR previous = swapchain.swapchain;
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = context.surface;
    info.minImageCount = std::max(2u, capabilities.minImageCount);
    if (capabilities.maxImageCount != 0)
    {
        info.minImageCount = std::min(info.minImageCount, capabilities.maxImageCount);
    }
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = previous;
    check(vkCreateSwapchainKHR(context.device, &info, nullptr, &swapchain.swapchain), "vkCreateSwapchainKHR");
    if (previous != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context.device, previous, nullptr);
    }
    swapchain.format = chosen.format;
    swapchain.extent = extent;

    u32 count = 0;
    vkGetSwapchainImagesKHR(context.device, swapchain.swapchain, &count, nullptr);
    swapchain.images.resize(count);
    vkGetSwapchainImagesKHR(context.device, swapchain.swapchain, &count, swapchain.images.data());
    for (VkImage image : swapchain.images)
    {
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = swapchain.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView created = VK_NULL_HANDLE;
        check(vkCreateImageView(context.device, &view, nullptr, &created), "vkCreateImageView (swapchain)");
        swapchain.views.push_back(created);
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore rendered = VK_NULL_HANDLE;
        check(vkCreateSemaphore(context.device, &semaphore, nullptr, &rendered), "vkCreateSemaphore");
        swapchain.rendered.push_back(rendered);
    }
    swapchain.stale = false;
    return true;
}

} // namespace

void swapchain_create(Context& context, Swapchain& swapchain, int vsync)
{
    swapchain = {};
    swapchain.vsync = vsync;
    if (vsync > 1)
    {
        log::info("vulkan: VSync=%d presents every vblank (FIFO)", vsync);
    }
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check(vkCreateSemaphore(context.device, &semaphore, nullptr, &swapchain.acquired), "vkCreateSemaphore");
    swapchain.stale = !build(context, swapchain);
}

void swapchain_destroy(Context& context, Swapchain& swapchain)
{
    if (context.device == VK_NULL_HANDLE)
    {
        return;
    }
    vkDeviceWaitIdle(context.device);
    release_images(context, swapchain);
    if (swapchain.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context.device, swapchain.swapchain, nullptr);
    }
    if (swapchain.acquired != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(context.device, swapchain.acquired, nullptr);
    }
    swapchain = {};
}

bool swapchain_ready(Context& context, Swapchain& swapchain)
{
    if (!swapchain.stale && swapchain.swapchain != VK_NULL_HANDLE)
    {
        return true;
    }
    vkDeviceWaitIdle(context.device);
    release_images(context, swapchain);
    return build(context, swapchain);
}

} // namespace gbtg::vk
