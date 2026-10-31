#pragma once

// Host debug UI (Dear ImGui). CMake compiles ImGui into every gbtg configuration with a D3D11 or Vulkan
// backend and defines GBTG_DEBUG_UI_AVAILABLE. Set GBTG_DEBUG_UI_ENABLED to 0 to build without the debug UI.
#ifndef GBTG_DEBUG_UI_ENABLED
#define GBTG_DEBUG_UI_ENABLED 1
#endif
#if GBTG_DEBUG_UI_ENABLED && defined(GBTG_DEBUG_UI_AVAILABLE) && !defined(GBTG_DEBUG_UI)
#define GBTG_DEBUG_UI
#endif

#if defined(GBTG_DEBUG_UI)

#include "gbtg/game.h"

#include <cstdint>

struct ImDrawData;

namespace gbtg
{

// The ImGui renderer half, which the gfx backend owning the window supplies. debug_ui_attach creates the ImGui
// context and the platform half (Win32, or SDL3 elsewhere), then calls init; a false return leaves the debug UI off.
struct DebugUiRenderer
{
    bool (*init)(void* user) = nullptr;
    void (*new_frame)() = nullptr;
    void (*shutdown)() = nullptr;
    void* user = nullptr;
};
void debug_ui_attach(void* hwnd, const DebugUiRenderer& renderer);
// What debug_ui_build last finished, for the backend to render; null while the debug UI is off.
ImDrawData* debug_ui_draw_data();

// D3D11 (debug_ui_d3d11.cpp; the inspector supplies its own): attach with the device and immediate context, and
// render into the bound target.
void debug_ui_init(void* hwnd, void* device, void* context);
void debug_ui_shutdown();
bool debug_ui_menu_open();
void debug_ui_toggle_menu();
void debug_ui_toggle_overlay();
#if defined(_WIN32)
// Forwards to the ImGui Win32 backend only while the menu bar is open.
bool debug_ui_wndproc(void* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
#else
// Forwards an SDL_Event to the ImGui SDL3 backend only while the menu bar is open.
bool debug_ui_sdl_event(const void* event);
#endif
// Builds widgets after game.tick. debug_ui_draw (D3D11) submits them just before Present; the Vulkan backend
// renders debug_ui_draw_data itself.
void debug_ui_build(Game& game, float host_dt);
void debug_ui_draw();

} // namespace gbtg

#endif
