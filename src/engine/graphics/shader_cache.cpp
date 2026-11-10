#include "shader_cache.h"

#include "gbtg/host_config.h"

#include <system_error>

namespace gbtg
{
namespace
{
std::filesystem::path normalized(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

// <exe dir>/cache, or empty when it is the working directory's cache/ (or the executable cannot be found).
const std::filesystem::path& shipped_cache_dir()
{
    static const std::filesystem::path shipped = []()
    {
        const std::filesystem::path executable = executable_path({});
        if (executable.empty())
        {
            return std::filesystem::path{};
        }
        const std::filesystem::path dir = normalized(executable.parent_path() / "cache");
        std::error_code error;
        const std::filesystem::path local = normalized(std::filesystem::absolute("cache", error));
        return dir == local ? std::filesystem::path{} : dir;
    }();
    return shipped;
}
} // namespace

std::vector<std::filesystem::path> shader_cache_reads(const std::filesystem::path& entry)
{
    std::vector<std::filesystem::path> reads;
    if (!shipped_cache_dir().empty())
    {
        reads.push_back(shipped_cache_dir() / entry);
    }
    reads.push_back(shader_cache_write(entry));
    return reads;
}

std::filesystem::path shader_cache_write(const std::filesystem::path& entry)
{
    return std::filesystem::path("cache") / entry;
}

} // namespace gbtg
