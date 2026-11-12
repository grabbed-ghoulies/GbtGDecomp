#include "error_dialog.h"

#include "gbtg/log.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(GBTG_WITH_SDL)
#include <SDL3/SDL.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
// The game window's title (gfx_d3d11.cpp, gfx_vulkan.cpp).
constexpr const char* kTitle = "Grabbed by the Ghoulies";
bool g_enabled = false;
std::filesystem::path g_log_file;

std::string with_log_hint(const std::string& message)
{
    std::string text = message;
    if (!g_log_file.empty())
    {
        text += "\n\nDetails are in " + g_log_file.string();
    }
    return text;
}

#if defined(_WIN32)
std::wstring widen(const std::string& text)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0);
    std::wstring wide(size_t(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), int(text.size()), wide.data(), length);
    return wide;
}

void message_box(const std::string& text)
{
    MessageBoxW(nullptr, widen(text).c_str(), widen(kTitle).c_str(),
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
}

const char* crash_name(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return "an access violation";
    case EXCEPTION_STACK_OVERFLOW:
        return "a stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "an illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "a division by zero";
    default:
        return "an unhandled exception";
    }
}

LONG WINAPI report_crash(EXCEPTION_POINTERS* pointers)
{
    static LONG reported = 0;
    if (!g_enabled || InterlockedExchange(&reported, 1) != 0)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto& record = *pointers->ExceptionRecord;
    char module_name[MAX_PATH] = "?";
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<LPCSTR>(record.ExceptionAddress), &module) != FALSE)
    {
        char path[MAX_PATH];
        if (GetModuleFileNameA(module, path, MAX_PATH) != 0)
        {
            const char* slash = std::strrchr(path, '\\');
            std::snprintf(module_name, sizeof(module_name), "%s", slash != nullptr ? slash + 1 : path);
        }
    }
    char text[512];
    std::snprintf(text, sizeof(text), "The game stopped after %s (0x%08lX) at %s+0x%llX.", crash_name(record.ExceptionCode),
        static_cast<unsigned long>(record.ExceptionCode), module_name,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record.ExceptionAddress)
            - reinterpret_cast<std::uintptr_t>(module)));
    log::error("crash: %s", text);
    message_box(with_log_hint(text));
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
}

void enable_error_dialogs(const std::filesystem::path& log_file)
{
    g_enabled = true;
    g_log_file = log_file;
}

bool error_dialogs_enabled()
{
    return g_enabled;
}

void show_error_dialog(const std::string& message)
{
    if (!g_enabled)
    {
        return;
    }
#if defined(_WIN32)
    message_box(with_log_hint(message));
#elif defined(GBTG_WITH_SDL)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, with_log_hint(message).c_str(), nullptr);
#else
    static_cast<void>(message);
#endif
}

void install_crash_dialog()
{
#if defined(_WIN32)
    SetUnhandledExceptionFilter(report_crash);
#endif
}
}
