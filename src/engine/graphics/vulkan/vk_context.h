#pragma once

#include "gbtg/types.h"

#include <vulkan/vulkan.h>

#include "vma/vk_mem_alloc.h"

#include <string>

namespace gbtg::vk
{

// Throws gbtg::Error naming `what` when a Vulkan call fails.
void check(VkResult result, const char* what);

// One instance, device and graphics queue for the gfx backend. Validation is VK_LAYER_KHRONOS_validation, on
// when GBTG_VULKAN_VALIDATION=1 is set and the layer is installed; its messages go to the engine log.
struct Context
{
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE; // Only with a window.
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkDevice device = VK_NULL_HANDLE;
    u32 queue_family = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    // D24_UNORM_S8_UINT, the NV2A's depth-stencil, when the device renders to it; else D32_SFLOAT_S8_UINT.
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    bool validation = false;
    PFN_vkCmdPushDescriptorSetKHR push_descriptor_set = nullptr;
};

// A non-null `hwnd` (an HWND on Windows, an SDL_Window created with SDL_WINDOW_VULKAN elsewhere) creates a
// window surface for it and requires present support on the chosen queue.
void context_create(Context& context, void* hwnd);
void context_destroy(Context& context);
// Validation errors reported since context_create.
u32 context_validation_errors();

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags aspect = 0;
    int width = 0;
    int height = 0;
};

Image image_create(Context& context, int width, int height, VkFormat format, VkImageUsageFlags usage,
                   VkImageAspectFlags aspect);
void image_destroy(Context& context, Image& image);

// Persistently mapped, host-coherent buffer.
struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    u8* mapped = nullptr;
    VkDeviceSize size = 0;
};

// `readback` picks host-cached memory for buffers the CPU reads after a copy.
Buffer buffer_create(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, bool readback = false);
void buffer_destroy(Context& context, Buffer& buffer);

// Layout change with a full memory dependency on both sides. The backend submits once or twice per frame, so
// precise stage masks would not buy anything yet.
void image_barrier(VkCommandBuffer command, VkImage image, VkImageAspectFlags aspect, VkImageLayout from,
                   VkImageLayout to);
void memory_barrier(VkCommandBuffer command);

} // namespace gbtg::vk
