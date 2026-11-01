#pragma once
#include "gbtg/anim.h"
#include "gbtg/xbe.h"
#include "../scripts/script_table_lookup.h"
#include "../entities/player_mind_goals.h"
#include "../entities/body_attachment_requests.h"
#include <unordered_set>

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
    // The XBE address of a pointer into file-backed XBE memory; 0 when the pointer is elsewhere.
    u32 address_of(const void* pointer) const;
    std::vector<ExecutableResourcePool> pools;
    // Pointer-width views of the animtable records (header +0 count, +4 records, +8 alias count,
    // +0xC aliases), keyed by each record's CPU bytes.
    AnimationTableResourceViews animation_tables;
    // Statetable records (header +0 count, +4 rows of 0x108), as the body action table view.
    std::unordered_map<const u8*, const RuntimeScriptLookupTable*> state_tables;
    // Attackdata records: the sequence words at +0x10 (+0xC words), keyed by each record's CPU bytes.
    std::unordered_map<const u8*, const u8*> attack_sequences;
    // Attackdata request tables (body +0x5C): rows +0/+8/+0x18/+0x20/+0x28 with counts +4/+6/+0x14/+0x1C/+0x24.
    std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*> attack_tables;
    // The strings statetable rows name at +0x104, by their XBE address.
    std::unordered_map<u32, const char*> state_row_names;
    // Scripttable records (header +0 count, +4 rows of 0x84, name pointer at row +0), as mind goal tables.
    std::unordered_map<const u8*, const RuntimeMindGoalTable*> goal_tables;

private:
    struct DecodedAnimationTable
    {
        std::vector<AnimationRecord> records;
        std::vector<AnimationAlias> aliases;
        AnimationTable table;
    };
    XbeInfo info_;
    std::vector<u8> bytes_;
    // NTSC-J only (ntsc_j_layout.h): the PAL ranges the view holds, and literals the NTSC-J code moved.
    bool ntsc_j_ = false;
    std::vector<std::pair<u32, u32>> covered_;
    std::unordered_map<u32, u32> redirects_;
    std::unordered_set<u32> missing_;
    u32 resolve(u32 address, size_t size) const;
    std::array<std::vector<ExecutableResourceRecord>, 7> records_;
    std::vector<std::unique_ptr<DecodedAnimationTable>> tables_;
    std::vector<std::unique_ptr<RuntimeScriptLookupTable>> state_tables_;
    std::vector<std::unique_ptr<RuntimeAttachmentRequestTables>> attack_tables_;
    struct DecodedGoalTable
    {
        std::vector<RuntimeMindGoalRow> rows;
        RuntimeMindGoalTable table;
    };
    std::vector<std::unique_ptr<DecodedGoalTable>> goal_tables_;
    void read_pool(size_t pool, const char* type, u32 address, u32 count, bool active);
    void decode_animation_table(const ExecutableResourceRecord& record);
};
}
