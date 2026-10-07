#pragma once

#include "gbtg/resource.h"

namespace gbtg
{

struct BundleEntry
{
    std::string name;
    u32 hash = 0;
    ResourceType type{};
    ResourceHeader header;
    std::vector<ResourceSubchunk> gpu_chunks;
    std::span<const u8> cpu;
    std::span<const u8> gpu;
};

struct Bundle
{
    std::string path;
    u16 file_count = 0;
    u8 flag = 1;
    std::vector<u8> file_table;
    std::vector<u8> res_meta;
    std::vector<u8> cpu_heap;
    std::vector<u8> gpu_heap;
    std::vector<BundleEntry> entries;
};

Bundle load_bundle(const std::filesystem::path& path);
void save_bundle(const Bundle& b, const std::filesystem::path& path);
const BundleEntry* find_entry(const Bundle& b, std::string_view name);
std::vector<const BundleEntry*> entries_of_type(const Bundle& b, ResourceType type);
std::vector<std::string> parse_aidlist(std::span<const u8> cpu);

// Replace the first `replace_count` 128-byte AID names in a type-14 aidlist.
void patch_aidlist_prefix(Bundle& b, std::string_view list_name, std::string_view new_aid, size_t replace_count);

} // namespace gbtg
