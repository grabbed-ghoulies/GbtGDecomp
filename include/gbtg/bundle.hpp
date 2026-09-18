#pragma once

#include "gbtg/resource.hpp"

namespace gbtg {

struct BundleEntry {
    std::string name;
    u32 hash = 0;
    ResourceType type{};
    ResourceHeader header;
    std::vector<ResourceSubchunk> gpu_chunks;
    std::span<const u8> cpu;
    std::span<const u8> gpu;
};

struct Bundle {
    std::string path;
    u16 file_count = 0;
    std::vector<u8> cpu_heap;
    std::vector<u8> gpu_heap;
    std::vector<BundleEntry> entries;
};

Bundle load_bundle(const std::filesystem::path& path);
const BundleEntry* find_entry(const Bundle& b, std::string_view name);
std::vector<const BundleEntry*> entries_of_type(const Bundle& b, ResourceType type);

}  // namespace gbtg
