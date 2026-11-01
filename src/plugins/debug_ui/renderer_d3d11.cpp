// The D3D11 renderer half of the debug UI. Built when gbtg has the D3D11 backend (DEBUG_UI_D3D11).
#include "renderer.h"

#if defined(DEBUG_UI_D3D11)
#include <d3d11.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#endif

namespace debug_ui
{

#if defined(DEBUG_UI_D3D11)
namespace
{

bool init_dx11(const GbtgGraphicsDevice& device)
{
    if (device.d3d11_device == nullptr || device.d3d11_context == nullptr)
    {
        return false;
    }
    return ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(device.d3d11_device),
        static_cast<ID3D11DeviceContext*>(device.d3d11_context));
}

void render_dx11(ImDrawData* data, void*)
{
    ImGui_ImplDX11_RenderDrawData(data);
}

const Renderer g_renderer{init_dx11, ImGui_ImplDX11_NewFrame, render_dx11, ImGui_ImplDX11_Shutdown};

} // namespace

const Renderer* d3d11_renderer()
{
    return &g_renderer;
}
#else
const Renderer* d3d11_renderer()
{
    return nullptr;
}
#endif

} // namespace debug_ui
