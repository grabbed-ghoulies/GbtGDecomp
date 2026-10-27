#pragma once

#include <cstdarg>
#include <filesystem>
#include <string>

#if defined(_MSC_VER)
#include <sal.h>
#define GBTG_LOG_FMT _Printf_format_string_
#define GBTG_LOG_ATTR(fmt_index, args_index)
#else
#define GBTG_LOG_FMT
#define GBTG_LOG_ATTR(fmt_index, args_index) __attribute__((format(printf, fmt_index, args_index)))
#endif

// Host log. Each call writes one line: unprefixed to the console (Info and
// Verbose to stdout, Warn and Error to stderr) and, once open() has run, to the
// log file as "[INFO] [+1.234s] text". Verbose lines are dropped unless
// set_verbose(true) (gbtg --verbose). Reconstructed game code does not depend on it.
namespace gbtg::log
{

enum class Level
{
    Verbose,
    Info,
    Warn,
    Error,
};

void set_verbose(bool enabled);
bool verbose_enabled();

// Truncates the file and restarts the boot clock. Returns false and keeps
// logging to the console only when the file cannot be opened.
bool open(const std::filesystem::path& file);
void close();
// Empty until open() succeeds.
const std::filesystem::path& path();

void write(Level level, GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(2, 3);
void vwrite(Level level, const char* fmt, std::va_list args);
void verbose(GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(1, 2);
void info(GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(1, 2);
void warn(GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(1, 2);
void error(GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(1, 2);

// printf into a string, for lines assembled over several statements.
std::string format(GBTG_LOG_FMT const char* fmt, ...) GBTG_LOG_ATTR(1, 2);
std::string vformat(const char* fmt, std::va_list args);

} // namespace gbtg::log
