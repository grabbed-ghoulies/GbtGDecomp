#include "gbtg/debug_ui.h"
// The D3D11 renderer half of the debug UI. Empty unless the debug UI is enabled.
#if defined(GBTG_DEBUG_UI)

#include <d3d11.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"

namespace gbtg
{
namespace
{

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;

bool init_dx11(void*)
{
    return ImGui_ImplDX11_Init(g_device, g_context);
}

} // namespace

void debug_ui_init(void* hwnd, void* device, void* context)
{
    if (device == nullptr || context == nullptr)
    {
        return;
    }
    g_device = static_cast<ID3D11Device*>(device);
    g_context = static_cast<ID3D11DeviceContext*>(context);
    debug_ui_attach(hwnd, DebugUiRenderer{init_dx11, ImGui_ImplDX11_NewFrame, ImGui_ImplDX11_Shutdown, nullptr});
}

void debug_ui_draw()
{
    if (ImDrawData* data = debug_ui_draw_data())
    {
        ImGui_ImplDX11_RenderDrawData(data);
    }
}

} // namespace gbtg

#endif // GBTG_DEBUG_UI
