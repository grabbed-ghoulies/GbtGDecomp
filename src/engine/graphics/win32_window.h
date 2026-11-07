#pragma once
// The game window's style and placement on Windows, shared by the D3D11 and Vulkan backends. Host presentation only.
#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>

namespace gbtg
{

struct Win32WindowPlacement
{
    DWORD style = WS_OVERLAPPEDWINDOW;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 0; // The outer size, frame included.
    int height = 0;
};

// Windowed: a framed window whose client area is width x height. Fullscreen: a borderless window over the whole
// monitor the cursor is on (gbtg.ini [Video] Fullscreen, --fullscreen). The back buffer keeps its configured size;
// the backend scales it to the window as it does for a resized one.
inline Win32WindowPlacement win32_window_placement(bool fullscreen, int width, int height)
{
    Win32WindowPlacement placement;
    if (fullscreen)
    {
        POINT cursor{};
        GetCursorPos(&cursor);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor))
        {
            placement.style = WS_POPUP;
            placement.x = monitor.rcMonitor.left;
            placement.y = monitor.rcMonitor.top;
            placement.width = monitor.rcMonitor.right - monitor.rcMonitor.left;
            placement.height = monitor.rcMonitor.bottom - monitor.rcMonitor.top;
            return placement;
        }
    }
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, placement.style, FALSE);
    placement.width = rect.right - rect.left;
    placement.height = rect.bottom - rect.top;
    return placement;
}

} // namespace gbtg
#endif
