#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_context.h"

#include <vulkan/vulkan_win32.h>

#include "gbtg/log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace gbtg::vk
{
namespace
{

u32 validation_errors = 0;

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
    {
        ++validation_errors;
        log::error("vulkan validation: %s", data->pMessage);
    }
    else
    {
        log::warn("vulkan validation: %s", data->pMessage);
    }
    return VK_FALSE;
}

bool layer_available(const char* name)
{
    u32 count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::any_of(layers.begin(), layers.end(),
                       [&](const VkLayerProperties& layer) { return std::strcmp(layer.layerName, name) == 0; });
}

bool device_extension_available(VkPhysicalDevice device, const char* name)
{
    u32 count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    return std::any_of(extensions.begin(), extensions.end(),
                       [&](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, name) == 0; });
}

bool renders_depth(VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device, format, &properties);
    const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    return (properties.optimalTilingFeatures & need) == need;
}

} // namespace

void check(VkResult result, const char* what)
{
    if (result != VK_SUCCESS)
    {
        throw Error(std::string("vulkan: ") + what + " failed (VkResult " + std::to_string(int(result)) + ")");
    }
}

u32 context_validation_errors()
{
    return validation_errors;
}

void context_create(Context& context, void* hwnd)
{
    context = {};
    const char* requested = std::getenv("GBTG_VULKAN_VALIDATION");
    const bool want_validation = requested != nullptr && std::strcmp(requested, "0") != 0;
    context.validation = want_validation && layer_available("VK_LAYER_KHRONOS_validation");
    if (want_validation && !context.validation)
    {
        log::warn("vulkan: GBTG_VULKAN_VALIDATION is set but VK_LAYER_KHRONOS_validation is not installed");
    }

    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "GbtGDecomp";
    application.pEngineName = "GbtGDecomp";
    application.apiVersion = VK_API_VERSION_1_3;
    std::vector<const char*> extensions;
    if (hwnd != nullptr)
    {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    }
    std::vector<const char*> layers;
    if (context.validation)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &application;
    instance_info.enabledExtensionCount = u32(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();
    instance_info.enabledLayerCount = u32(layers.size());
    instance_info.ppEnabledLayerNames = layers.data();
    check(vkCreateInstance(&instance_info, nullptr, &context.instance), "vkCreateInstance");

    if (context.validation)
    {
        VkDebugUtilsMessengerCreateInfoEXT messenger{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        messenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger.pfnUserCallback = validation_message;
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create != nullptr)
        {
            check(create(context.instance, &messenger, nullptr, &context.messenger), "vkCreateDebugUtilsMessengerEXT");
        }
    }

    if (hwnd != nullptr)
    {
        VkWin32SurfaceCreateInfoKHR surface{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        surface.hinstance = GetModuleHandleW(nullptr);
        surface.hwnd = static_cast<HWND>(hwnd);
        check(vkCreateWin32SurfaceKHR(context.instance, &surface, nullptr, &context.surface), "vkCreateWin32SurfaceKHR");
    }

    // A Vulkan 1.3 device with push descriptors and a graphics queue (that presents, with a window).
    // Discrete GPUs are preferred, matching D3D11's default adapter on hybrid laptops more often than not.
    u32 count = 0;
    vkEnumeratePhysicalDevices(context.instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(context.instance, &count, devices.data());
    int best_score = -1;
    for (VkPhysicalDevice device : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3 || !device_extension_available(device, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
            || (hwnd != nullptr && !device_extension_available(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)))
        {
            continue;
        }
        u32 family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());
        for (u32 family = 0; family < family_count; ++family)
        {
            if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                continue;
            }
            if (context.surface != VK_NULL_HANDLE)
            {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, family, context.surface, &present);
                if (present != VK_TRUE)
                {
                    continue;
                }
            }
            const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2
                : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 0;
            if (score > best_score)
            {
                best_score = score;
                context.physical = device;
                context.properties = properties;
                context.queue_family = family;
            }
            break;
        }
    }
    if (context.physical == VK_NULL_HANDLE)
    {
        throw Error("vulkan: no Vulkan 1.3 device with push descriptors and a graphics queue");
    }
    context.depth_format = renders_depth(context.physical, VK_FORMAT_D24_UNORM_S8_UINT) ? VK_FORMAT_D24_UNORM_S8_UINT
                                                                                       : VK_FORMAT_D32_SFLOAT_S8_UINT;
    if (!renders_depth(context.physical, context.depth_format))
    {
        throw Error("vulkan: the device has no renderable depth-stencil format");
    }
    log::info("vulkan: %s (API %u.%u.%u), depth %s%s", context.properties.deviceName,
              VK_API_VERSION_MAJOR(context.properties.apiVersion), VK_API_VERSION_MINOR(context.properties.apiVersion),
              VK_API_VERSION_PATCH(context.properties.apiVersion),
              context.depth_format == VK_FORMAT_D24_UNORM_S8_UINT ? "D24S8" : "D32FS8",
              context.validation ? ", validation on" : "");

    const float priority = 1.f;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = context.queue_family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    std::vector<const char*> device_extensions{VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    if (hwnd != nullptr)
    {
        device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    // DXC lowers HLSL discard to OpDemoteToHelperInvocation for Vulkan 1.3.
    features13.shaderDemoteToHelperInvocation = VK_TRUE;
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &features13;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.pNext = &features;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue;
    device_info.enabledExtensionCount = u32(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.data();
    check(vkCreateDevice(context.physical, &device_info, nullptr, &context.device), "vkCreateDevice");
    vkGetDeviceQueue(context.device, context.queue_family, 0, &context.queue);
    context.push_descriptor_set = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
        vkGetDeviceProcAddr(context.device, "vkCmdPushDescriptorSetKHR"));
    if (context.push_descriptor_set == nullptr)
    {
        throw Error("vulkan: vkCmdPushDescriptorSetKHR is missing");
    }

    VmaAllocatorCreateInfo allocator{};
    allocator.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator.physicalDevice = context.physical;
    allocator.device = context.device;
    allocator.instance = context.instance;
    check(vmaCreateAllocator(&allocator, &context.allocator), "vmaCreateAllocator");

    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = context.queue_family;
    check(vkCreateCommandPool(context.device, &pool, nullptr, &context.command_pool), "vkCreateCommandPool");
}

void context_destroy(Context& context)
{
    if (context.device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(context.device);
        if (context.command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(context.device, context.command_pool, nullptr);
        }
        if (context.allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(context.allocator);
        }
        vkDestroyDevice(context.device, nullptr);
    }
    if (context.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    }
    if (context.messenger != VK_NULL_HANDLE)
    {
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr)
        {
            destroy(context.instance, context.messenger, nullptr);
        }
    }
    if (context.instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(context.instance, nullptr);
    }
    context = {};
}

Image image_create(Context& context, int width, int height, VkFormat format, VkImageUsageFlags usage,
                   VkImageAspectFlags aspect)
{
    Image image;
    image.format = format;
    image.aspect = aspect;
    image.width = width;
    image.height = height;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {u32(width), u32(height), 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    check(vmaCreateImage(context.allocator, &info, &allocation, &image.image, &image.allocation, nullptr), "vmaCreateImage");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = image.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    // A depth-stencil view samples nothing; attachments take both aspects.
    view.subresourceRange = {aspect, 0, 1, 0, 1};
    check(vkCreateImageView(context.device, &view, nullptr, &image.view), "vkCreateImageView");
    return image;
}

void image_destroy(Context& context, Image& image)
{
    if (image.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(context.device, image.view, nullptr);
    }
    if (image.image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(context.allocator, image.image, image.allocation);
    }
    image = {};
}

Buffer buffer_create(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, bool readback)
{
    Buffer buffer;
    buffer.size = size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO;
    allocation.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | (readback ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                                                     : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    allocation.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VmaAllocationInfo allocated{};
    check(vmaCreateBuffer(context.allocator, &info, &allocation, &buffer.buffer, &buffer.allocation, &allocated),
          "vmaCreateBuffer");
    buffer.mapped = static_cast<u8*>(allocated.pMappedData);
    return buffer;
}

void buffer_destroy(Context& context, Buffer& buffer)
{
    if (buffer.buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);
    }
    buffer = {};
}

void image_barrier(VkCommandBuffer command, VkImage image, VkImageAspectFlags aspect, VkImageLayout from,
                   VkImageLayout to)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {aspect, 0, 1, 0, 1};
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
}

void memory_barrier(VkCommandBuffer command)
{
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier,
                         0, nullptr, 0, nullptr);
}

} // namespace gbtg::vk
