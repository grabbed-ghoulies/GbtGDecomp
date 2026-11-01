#include "save_root.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <utility>

namespace gbtg
{
namespace
{
std::filesystem::path g_save_root;
std::filesystem::path g_utility_root = "cache/xbox_utility";

// Xbox paths use backslashes; POSIX hosts need forward slashes. This is the one translation point.
std::string host_separators(std::string path)
{
#ifndef _WIN32
    for (char& character : path)
    {
        if (character == '\\')
        {
            character = '/';
        }
    }
#endif
    return path;
}

std::filesystem::path executable_directory()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (length != 0)
    {
        return std::filesystem::path(buffer.substr(0, length)).parent_path();
    }
#else
    std::error_code link_error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", link_error);
    if (!link_error)
    {
        return executable.parent_path();
    }
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}
}

const std::filesystem::path& save_root()
{
    if (g_save_root.empty())
    {
        g_save_root = executable_directory() / "saves";
    }
    std::error_code ec;
    std::filesystem::create_directories(g_save_root, ec);
    return g_save_root;
}

void set_save_root(const std::filesystem::path& root)
{
    g_save_root = root;
}

std::filesystem::path utility_root()
{
    return std::filesystem::absolute(g_utility_root).lexically_normal();
}

void set_utility_root(const std::filesystem::path& root)
{
    g_utility_root = root;
}

std::string xbox_host_path(const char* path)
{
    if (path == nullptr)
    {
        return {};
    }
    if ((path[0] == 'U' || path[0] == 'u') && path[1] == ':' && (path[2] == '\\' || path[2] == '\0'))
    {
        std::string host = save_root().string();
        const char* rest = path[2] == '\0' ? path + 2 : path + 3;
        host += '\\';
        host += rest;
        return host_separators(std::move(host));
    }
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // A disc file a plugin replaces or adds opens from the plugin's folder.
    return plugin_host_path(host_separators(path));
#else
    return host_separators(path);
#endif
}
}
