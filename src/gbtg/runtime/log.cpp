#include "gbtg/log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace gbtg::log
{
namespace
{

std::atomic<bool> g_verbose = false;
std::mutex g_mutex;
std::FILE* g_file = nullptr;
std::filesystem::path g_path;
std::chrono::steady_clock::time_point g_boot = std::chrono::steady_clock::now();

const char* level_tag(Level level)
{
    switch (level)
    {
    case Level::Verbose:
        return "[VERBOSE]";
    case Level::Warn:
        return "[WARN]";
    case Level::Error:
        return "[ERROR]";
    case Level::Info:
    default:
        return "[INFO]";
    }
}

void emit(Level level, const std::string& text)
{
    const bool newline = text.empty() || text.back() != '\n';
    std::lock_guard<std::mutex> lock(g_mutex);
    std::FILE* console = level == Level::Info || level == Level::Verbose ? stdout : stderr;
    std::fwrite(text.data(), 1, text.size(), console);
    if (newline)
    {
        std::fputc('\n', console);
    }
    std::fflush(console);
    if (g_file != nullptr)
    {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_boot).count();
        std::fprintf(g_file, "%s [+%.3fs] ", level_tag(level), seconds);
        std::fwrite(text.data(), 1, text.size(), g_file);
        if (newline)
        {
            std::fputc('\n', g_file);
        }
        std::fflush(g_file);
    }
}

} // namespace

bool open(const std::filesystem::path& file)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != nullptr)
    {
        std::fclose(g_file);
        g_file = nullptr;
        g_path.clear();
    }
    g_boot = std::chrono::steady_clock::now();
#if defined(_WIN32)
    if (_wfopen_s(&g_file, file.c_str(), L"wb") != 0)
    {
        g_file = nullptr;
    }
#else
    g_file = std::fopen(file.c_str(), "wb");
#endif
    if (g_file == nullptr)
    {
        return false;
    }
    g_path = file;
    return true;
}

void close()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != nullptr)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_path.clear();
}

const std::filesystem::path& path()
{
    return g_path;
}

std::string vformat(const char* fmt, std::va_list args)
{
    char stack[512];
    std::va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(stack, sizeof(stack), fmt, copy);
    va_end(copy);
    if (n < 0)
    {
        return fmt;
    }
    if (static_cast<size_t>(n) < sizeof(stack))
    {
        return std::string(stack, static_cast<size_t>(n));
    }
    std::string text(static_cast<size_t>(n), '\0');
    std::vsnprintf(text.data(), text.size() + 1, fmt, args);
    return text;
}

std::string format(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    std::string text = vformat(fmt, args);
    va_end(args);
    return text;
}

void set_verbose(bool enabled)
{
    g_verbose = enabled;
}

bool verbose_enabled()
{
    return g_verbose;
}

void vwrite(Level level, const char* fmt, std::va_list args)
{
    if (level == Level::Verbose && !g_verbose)
    {
        return;
    }
    emit(level, vformat(fmt, args));
}

void write(Level level, const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    vwrite(level, fmt, args);
    va_end(args);
}

void verbose(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    vwrite(Level::Verbose, fmt, args);
    va_end(args);
}

void info(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    vwrite(Level::Info, fmt, args);
    va_end(args);
}

void warn(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    vwrite(Level::Warn, fmt, args);
    va_end(args);
}

void error(const char* fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    vwrite(Level::Error, fmt, args);
    va_end(args);
}

} // namespace gbtg::log
