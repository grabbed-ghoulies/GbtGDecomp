#pragma once
#include "gbtg/anim.h"
#include "gbtg/xbe.h"

namespace gbtg
{
// Own the actual XBE bytes behind the executable resource pools that 0x00034C30 and
// 0x00034BC0 register. Objparams are cloned by 0x00107720; they are not substituted for
// bundle model/animation resources.
class SceneExecutableResources
{
public:
    explicit SceneExecutableResources(const std::filesystem::path& game_directory);
    SceneExecutableResources(const SceneExecutableResources&) = delete;
    SceneExecutableResources& operator=(const SceneExecutableResources&) = delete;
    std::span<const u8> memory(u32 address, size_t size) const;
    const char* string(u32 address) const; // A NUL-terminated string in file-backed XBE memory.
    std::vector<ExecutableResourcePool> pools;
    // Pointer-width views of the animtable records (header +0 count, +4 records, +8 alias count,
    // +0xC aliases), keyed by each record's CPU bytes.
    AnimationTableResourceViews animation_tables;

private:
    struct DecodedAnimationTable
    {
        std::vector<AnimationRecord> records;
        std::vector<AnimationAlias> aliases;
        AnimationTable table;
    };
    XbeInfo info_;
    std::vector<u8> bytes_;
    std::array<std::vector<ExecutableResourceRecord>, 7> records_;
    std::vector<std::unique_ptr<DecodedAnimationTable>> tables_;
    void read_pool(size_t pool, const char* type, u32 address, u32 count, bool active);
    void decode_animation_table(const ExecutableResourceRecord& record);
};
}
