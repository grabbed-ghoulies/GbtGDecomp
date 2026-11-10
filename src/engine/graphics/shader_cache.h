#pragma once

#include <filesystem>
#include <vector>

namespace gbtg
{

// Compiled shader bytecode (cache/dxbc, cache/spirv), keyed by a hash of its inputs. `entry` is relative to a cache
// directory, for example "spirv/<hash>.spv".
//
// Reads try the cache shipped beside the executable (<exe dir>/cache, staged by tools/build/stage_shader_cache.ps1)
// before the working directory's cache/, so a release's entries win over ones an older build left. The two are one
// directory when the game runs from its own folder on Windows.
std::vector<std::filesystem::path> shader_cache_reads(const std::filesystem::path& entry);
// New entries go to the working directory's cache/ (on Linux the data directory, which main makes the working
// directory). Failed writes are ignored: a read-only install compiles as it draws.
std::filesystem::path shader_cache_write(const std::filesystem::path& entry);

} // namespace gbtg
