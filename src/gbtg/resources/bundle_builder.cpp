#include "gbtg/bundle_builder.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace gbtg
{

namespace
{

std::string lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

void write_header(std::vector<u8>& table, size_t at, const ResourceHeader& h)
{
    write_u32(table, at + 0, u32(h.type));
    write_u32(table, at + 4, h.unknown);
    write_u32(table, at + 8, h.magic);
    write_u32(table, at + 12, h.chunk_count);
    write_u32(table, at + 16, h.file_chunk.offset);
    write_u32(table, at + 20, h.file_chunk.size);
    write_u32(table, at + 24, h.resource_chunk.offset);
    write_u32(table, at + 28, h.resource_chunk.size);
}

} // namespace

void BundleBuilder::add(const BundleEntry& entry, std::string_view name)
{
    Resource resource;
    resource.name = name.empty() ? entry.name : std::string(name);
    if (resource.name.size() >= 128)
    {
        throw Error("bundle builder: AID name too long: " + resource.name);
    }
    resource.header = entry.header;
    resource.cpu.assign(entry.cpu.begin(), entry.cpu.end());
    if (resource.cpu.size() != entry.header.file_chunk.size)
    {
        throw Error("bundle builder: " + resource.name + " has no CPU bytes in its source");
    }
    for (const ResourceSubchunk& chunk : entry.gpu_chunks)
    {
        if (size_t(chunk.offset) + chunk.size > entry.gpu.size())
        {
            throw Error("bundle builder: " + resource.name + " has a GPU chunk outside its source");
        }
        const auto bytes = entry.gpu.subspan(chunk.offset, chunk.size);
        resource.gpu.emplace_back(bytes.begin(), bytes.end());
    }
    resources_[lower(resource.name)] = std::move(resource);
}

void BundleBuilder::add_cpu(std::string_view name, ResourceType type, std::span<const u8> cpu)
{
    if (name.size() >= 128)
    {
        throw Error("bundle builder: AID name too long: " + std::string(name));
    }
    Resource resource;
    resource.name = std::string(name);
    resource.header.type = type;
    resource.header.magic = kResourceMagic;
    resource.header.chunk_count = 2;
    resource.header.file_chunk.size = u32(cpu.size());
    resource.cpu.assign(cpu.begin(), cpu.end());
    resources_[lower(name)] = std::move(resource);
}

bool BundleBuilder::contains(std::string_view name) const
{
    return resources_.contains(lower(name));
}

void BundleBuilder::remove(std::string_view name)
{
    resources_.erase(lower(name));
}

Bundle BundleBuilder::build() const
{
    // Retail sorts the table by name as written; the map is keyed in lower case, so sort the stored names.
    std::vector<const Resource*> order;
    order.reserve(resources_.size());
    for (const auto& [key, resource] : resources_)
    {
        order.push_back(&resource);
    }
    std::sort(order.begin(), order.end(), [](const Resource* a, const Resource* b)
    {
        return a->name < b->name;
    });
    if (order.size() > 0xFFFF)
    {
        throw Error("bundle builder: too many resources");
    }

    Bundle b;
    b.file_count = u16(order.size());
    b.flag = 1;
    b.file_table.assign(order.size() * 160, 0);
    for (size_t index = 0; index < order.size(); ++index)
    {
        const Resource& resource = *order[index];
        ResourceHeader header = resource.header;
        header.file_chunk.offset = u32(b.cpu_heap.size());
        header.file_chunk.size = u32(resource.cpu.size());
        b.cpu_heap.insert(b.cpu_heap.end(), resource.cpu.begin(), resource.cpu.end());
        if (resource.gpu.empty())
        {
            header.resource_chunk.offset = 0;
        }
        else
        {
            // The GPU table: its own size, the count, then each sub-chunk's offset in chunk 2 and size.
            header.resource_chunk.offset = u32(b.res_meta.size());
            const size_t at = b.res_meta.size();
            b.res_meta.resize(at + 8 + resource.gpu.size() * 8);
            write_u32(b.res_meta, at, u32(8 + resource.gpu.size() * 8));
            write_u32(b.res_meta, at + 4, u32(resource.gpu.size()));
            for (size_t k = 0; k < resource.gpu.size(); ++k)
            {
                write_u32(b.res_meta, at + 8 + k * 8, u32(b.gpu_heap.size()));
                write_u32(b.res_meta, at + 12 + k * 8, u32(resource.gpu[k].size()));
                b.gpu_heap.insert(b.gpu_heap.end(), resource.gpu[k].begin(), resource.gpu[k].end());
            }
        }
        std::memcpy(b.file_table.data() + index * 160, resource.name.data(), resource.name.size());
        write_header(b.file_table, index * 160 + 128, header);
    }
    index_bundle_entries(b);
    return b;
}

} // namespace gbtg
