#include "executable_resources.h"
#include <cstring>

namespace gbtg
{
SceneExecutableResources::SceneExecutableResources(const std::filesystem::path& directory)
{
    const auto original = directory / "default.xbe.original";
    const auto path = std::filesystem::exists(original) ? original : directory / "default.xbe";
    info_ = parse_xbe(path);
    bytes_ = read_file(path);
    // Actual inactive registrations from 0x00034C30 and 0x00034BC0.
    // Keep their order: duplicate template names resolve in the first pool.
    read_parameters(0, 0x003D7928, 0x1F1);
    read_parameters(1, 0x00402CA0, 0x35C);
}

std::span<const u8> SceneExecutableResources::memory(u32 address, size_t size) const
{
    for (const auto& section : info_.sections)
    {
        if (address >= section.virt_addr)
        {
            const size_t offset = size_t(address - section.virt_addr);
            if (offset <= section.raw_size && size <= section.raw_size - offset)
            {
                const size_t file_offset = size_t(section.raw_addr) + offset;
                if (file_offset <= bytes_.size() && size <= bytes_.size() - file_offset)
                {
                    return std::span<const u8>(bytes_).subspan(file_offset, size);
                }
            }
        }
    }
    throw Error("executable objparams address is outside file-backed XBE memory: " + std::to_string(address));
}

void SceneExecutableResources::read_parameters(size_t pool, u32 address, u32 count)
{
    auto& records = records_[pool];
    records.reserve(count);
    const auto table = memory(address, size_t(count) * 0x8C);
    for (u32 index = 0; index < count; ++index)
    {
        const auto row = table.subspan(size_t(index) * 0x8C, 0x8C);
        const auto* end = static_cast<const u8*>(std::memchr(row.data(), 0, 0x80));
        if (end == nullptr)
        {
            throw Error("executable objparams name has no terminator");
        }
        std::string name(reinterpret_cast<const char*>(row.data()), size_t(end - row.data()));
        if (!name.starts_with("aid_objparams_"))
        {
            throw Error("executable objparams table does not match this retail XBE");
        }
        const u32 cpu_address = read_u32(row, 0x80);
        const u32 recorded_size = read_u32(row, 0x84);
        const auto cpu = memory(cpu_address, recorded_size);
        if (cpu.size() < 2 || read_u16(cpu, 0) != cpu.size())
        {
            throw Error("executable objparams size disagrees with its clone header: " + name);
        }
        records.push_back({std::move(name), cpu, {}, 0});
    }
    register_executable_resource_pool(pools, records, "objparams", false);
}
}
