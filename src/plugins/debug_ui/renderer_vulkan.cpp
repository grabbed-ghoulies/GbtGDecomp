// The Vulkan renderer half of the debug UI. Built when gbtg has the Vulkan backend (DEBUG_UI_VULKAN). Draws with
// dynamic rendering into the swapchain images.
#include "renderer.h"

#if defined(DEBUG_UI_VULKAN)
#include <algorithm>
#include <string>

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#endif

namespace debug_ui
{

#if defined(DEBUG_UI_VULKAN)
namespace
{

VkDevice g_device = VK_NULL_HANDLE;
VkFormat g_color_format = VK_FORMAT_UNDEFINED;

void check_result(VkResult result)
{
    if (result < 0)
    {
        log(GBTG_LOG_ERROR, ("imgui vulkan: VkResult " + std::to_string(int(result))).c_str());
    }
}

bool init_vulkan(const GbtgGraphicsDevice& device)
{
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = device.vk_api_version;
    info.Instance = static_cast<VkInstance>(device.vk_instance);
    info.PhysicalDevice = static_cast<VkPhysicalDevice>(device.vk_physical_device);
    info.Device = static_cast<VkDevice>(device.vk_device);
    info.QueueFamily = device.vk_queue_family;
    info.Queue = static_cast<VkQueue>(device.vk_queue);
    info.DescriptorPoolSize = 64;
    info.MinImageCount = 2;
    info.ImageCount = std::max(2u, device.vk_image_count);
    info.UseDynamicRendering = true;
    info.CheckVkResultFn = check_result;
    g_color_format = VkFormat(device.vk_color_format);
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &g_color_format;
    if (!ImGui_ImplVulkan_Init(&info))
    {
        return false;
    }
    g_device = info.Device;
    return true;
}

void render_vulkan(ImDrawData* data, void* command)
{
    ImGui_ImplVulkan_RenderDrawData(data, static_cast<VkCommandBuffer>(command));
}

void shutdown_vulkan()
{
    // The last presented frame may still be drawing with ImGui's buffers.
    vkDeviceWaitIdle(g_device);
    ImGui_ImplVulkan_Shutdown();
    g_device = VK_NULL_HANDLE;
}

const Renderer g_renderer{init_vulkan, ImGui_ImplVulkan_NewFrame, render_vulkan, shutdown_vulkan};

} // namespace

const Renderer* vulkan_renderer()
{
    return &g_renderer;
}
#else
const Renderer* vulkan_renderer()
{
    return nullptr;
}
#endif

} // namespace debug_ui
