#pragma once
#include "gbtg/anim.h"
#include "gbtg/xbe.h"

namespace gbtg
{
// Own the actual XBE bytes behind inactive objparams pools. These are cloned
// by 0x00107720; they are not substituted for bundle model/animation resources.
class SceneExecutableResources
{
public:
    explicit SceneExecutableResources(const std::filesystem::path& game_directory);
    SceneExecutableResources(const SceneExecutableResources&) = delete;
    SceneExecutableResources& operator=(const SceneExecutableResources&) = delete;
    std::span<const u8> memory(u32 address, size_t size) const;
    std::vector<ExecutableResourcePool> pools;

private:
    XbeInfo info_;
    std::vector<u8> bytes_;
    std::array<std::vector<ExecutableResourceRecord>, 2> records_;
    void read_parameters(size_t pool, u32 address, u32 count);
};
}
