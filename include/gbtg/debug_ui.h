#pragma once

// The inspector's ImGui hooks in the D3D11 backend. gbtg-inspector defines them (inspector_ui.cpp). The game's debug
// UI is the debug_ui plugin (src/plugins/debug_ui), which draws through the plugin overlay hooks instead.
#if defined(GBTG_INSPECTOR_APP)

#include <cstdint>

namespace gbtg
{

// Attaches with the device and immediate context; renders into the bound target.
void debug_ui_init(void* hwnd, void* device, void* context);
void debug_ui_shutdown();
bool debug_ui_menu_open();
void debug_ui_toggle_menu();
void debug_ui_toggle_overlay();
bool debug_ui_wndproc(void* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
void debug_ui_draw();

} // namespace gbtg

#endif
