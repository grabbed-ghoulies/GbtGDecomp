#include "thirdparty_libraries.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <delayimp.h>
#include <string>
#else
#include <system_error>
#endif

std::filesystem::path thirdparty_library_dir()
{
#if defined(_WIN32)
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path() / L"thirdparty";
#else
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error)
    {
        return "thirdparty";
    }
    return executable.parent_path() / "thirdparty";
#endif
}

#if defined(_WIN32)
namespace
{
// The FFmpeg DLLs are delay-loaded (cmake/GbtgThirdParty.cmake): load each from thirdparty/. The altered search
// path resolves their imports of each other there too. Returning null falls back to the default search.
FARPROC WINAPI thirdparty_delay_load_hook(unsigned notification, PDelayLoadInfo info)
{
    if (notification != dliNotePreLoadLibrary || info == nullptr || info->szDll == nullptr)
    {
        return nullptr;
    }
    const std::filesystem::path path = thirdparty_library_dir() / info->szDll;
    return reinterpret_cast<FARPROC>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
}
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = thirdparty_delay_load_hook;
#endif
