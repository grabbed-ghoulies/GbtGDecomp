#include "vk_debug_ui.h"

#include "gbtg/debug_ui.h" // Defines GBTG_DEBUG_UI when the debug UI is enabled.

#include <algorithm>

#if defined(GBTG_DEBUG_UI)
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#endif

namespace gbtg::vk
{

#if defined(GBTG_DEBUG_UI)
namespace
{

ImGui_ImplVulkan_InitInfo g_info{};
VkFormat g_color_format = VK_FORMAT_UNDEFINED;

bool init_vulkan(void*)
{
    g_info.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    g_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    g_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &g_color_format;
    return ImGui_ImplVulkan_Init(&g_info);
}

void check_result(VkResult result)
{
    check(result, "imgui vulkan");
}

} // namespace

void debug_ui_init(Context& context, void* hwnd, VkFormat color_format, u32 image_count)
{
    g_info = {};
    g_info.ApiVersion = VK_API_VERSION_1_3;
    g_info.Instance = context.instance;
    g_info.PhysicalDevice = context.physical;
    g_info.Device = context.device;
    g_info.QueueFamily = context.queue_family;
    g_info.Queue = context.queue;
    g_info.DescriptorPoolSize = 64;
    g_info.MinImageCount = 2;
    g_info.ImageCount = std::max(2u, image_count);
    g_info.UseDynamicRendering = true;
    g_info.CheckVkResultFn = check_result;
    g_color_format = color_format;
    debug_ui_attach(hwnd, DebugUiRenderer{init_vulkan, ImGui_ImplVulkan_NewFrame, ImGui_ImplVulkan_Shutdown, nullptr});
}

void debug_ui_render(VkCommandBuffer command)
{
    if (ImDrawData* data = debug_ui_draw_data())
    {
        ImGui_ImplVulkan_RenderDrawData(data, command);
    }
}

bool debug_ui_has_draws()
{
    const ImDrawData* data = debug_ui_draw_data();
    return data != nullptr && data->CmdListsCount > 0;
}
#else
void debug_ui_init(Context&, void*, VkFormat, u32)
{
}

void debug_ui_render(VkCommandBuffer)
{
}

bool debug_ui_has_draws()
{
    return false;
}
#endif

} // namespace gbtg::vk
