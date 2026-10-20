#pragma once

// Host debug UI (Dear ImGui). CMake compiles ImGui into every D3D11 gbtg configuration and defines
// GBTG_DEBUG_UI_AVAILABLE. Set GBTG_DEBUG_UI_ENABLED to 0 to build without the debug UI.
#ifndef GBTG_DEBUG_UI_ENABLED
#define GBTG_DEBUG_UI_ENABLED 1
#endif
#if GBTG_DEBUG_UI_ENABLED && defined(GBTG_DEBUG_UI_AVAILABLE) && !defined(GBTG_DEBUG_UI)
#define GBTG_DEBUG_UI
#endif

#if defined(GBTG_DEBUG_UI)

#include "gbtg/game.h"

#include <cstdint>

namespace gbtg
{

void debug_ui_init(void* hwnd, void* device, void* context);
void debug_ui_shutdown();
bool debug_ui_menu_open();
void debug_ui_toggle_menu();
void debug_ui_toggle_overlay();
// Forwards to the ImGui Win32 backend only while the menu bar is open.
bool debug_ui_wndproc(void* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
// Builds widgets after game.tick. debug_ui_draw submits them just before Present.
void debug_ui_build(Game& game, float host_dt);
void debug_ui_draw();

} // namespace gbtg

#endif
