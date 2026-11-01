#pragma once

#include "gbtg/plugin_api.h"

struct ImDrawData;

namespace debug_ui
{

// ImGui's renderer half for the gfx backend that owns the window. debug_ui_plugin.cpp creates the ImGui context and
// the platform half (Win32, or SDL3 elsewhere), then calls init.
struct Renderer
{
    bool (*init)(const GbtgGraphicsDevice& device);
    void (*new_frame)();
    // D3D11: `command` is null and the back buffer is bound. Vulkan: the VkCommandBuffer inside dynamic rendering.
    void (*render)(ImDrawData* data, void* command);
    void (*shutdown)();
};

// Null when this build of the plugin has no renderer for that backend.
const Renderer* d3d11_renderer();
const Renderer* vulkan_renderer();

// Logs through the host.
void log(int level, const char* message);

} // namespace debug_ui
