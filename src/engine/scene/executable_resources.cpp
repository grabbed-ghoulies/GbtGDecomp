#include "executable_resources.h"
#include "ntsc_j_layout.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace gbtg
{
SceneExecutableResources::SceneExecutableResources(const std::filesystem::path& directory)
{
    const auto original = directory / "default.xbe.original";
    const auto path = std::filesystem::exists(original) ? original : directory / "default.xbe";
    info_ = parse_xbe(path);
    bytes_ = read_file(path);
    if (is_ntsc_j_build(info_))
    {
        auto view = build_ntsc_j_view(info_, bytes_);
        bytes_ = std::move(view.bytes);
        info_.sections = std::move(view.sections);
        covered_ = std::move(view.covered);
        redirects_ = std::move(view.redirects);
        missing_ = std::move(view.missing);
        ntsc_j_ = true;
    }
    // The registrations of 0x00034C30 (through 0x00034B60) and then 0x00034BC0, in order.
    // Keep their order: duplicate template names resolve in the first pool.
    read_pool(0, "animtable", 0x0033AC88, 0x56, true);
    read_pool(1, "attackdata", 0x0033E100, 0x1D, true);
    read_pool(2, "hitreaction", 0x00343800, 0x27, true);
    read_pool(3, "objparams", 0x003D7928, 0x1F1, false);
    read_pool(4, "scripttable", 0x003E8A10, 0x23, false);
    read_pool(5, "objparams", 0x00402CA0, 0x35C, false);
    read_pool(6, "statetable", 0x004202F0, 0x29, true);
    for (const auto& record : records_[0])
    {
        decode_animation_table(record);
    }
    // Pointer-width adapters for the attackdata sequence pointer (+0x10) and the statetable rows (+4).
    for (const auto& record : records_[1])
    {
        const auto rows = [&](size_t pointer, size_t count, size_t stride)
        {
            const u16 total = read_u16(record.cpu, count);
            const u32 address = read_u32(record.cpu, pointer);
            return RuntimeAttachmentRequestRows{address == 0 ? nullptr
                : memory(address, size_t(total) * stride).data(), total};
        };
        auto tables = std::make_unique<RuntimeAttachmentRequestTables>(RuntimeAttachmentRequestTables{
            rows(0, 4, 0x24), rows(8, 6, 0x24), rows(0x18, 0x14, 0x28), rows(0x20, 0x1C, 0x2C),
            rows(0x28, 0x24, 0x24)});
        attack_tables[record.cpu.data()] = tables.get();
        attack_tables_.push_back(std::move(tables));
        const u32 sequences = read_u32(record.cpu, 0x10);
        if (sequences != 0)
        {
            attack_sequences[record.cpu.data()] = memory(sequences, size_t(read_u32(record.cpu, 0xC)) * 2 + 2).data();
        }
    }
    for (const auto& record : records_[4])
    {
        auto decoded = std::make_unique<DecodedGoalTable>();
        const i32 count = i32(read_u32(record.cpu, 0));
        const u32 rows = read_u32(record.cpu, 4);
        for (i32 row = 0; row < count; ++row)
        {
            const auto bytes = memory(rows + u32(row) * 0x84, 0x84);
            decoded->rows.push_back({string(read_u32(bytes, 0)), bytes});
        }
        decoded->table = RuntimeMindGoalTable{count, decoded->rows};
        goal_tables[record.cpu.data()] = &decoded->table;
        goal_tables_.push_back(std::move(decoded));
    }
    for (const auto& record : records_[6])
    {
        const i32 count = i32(read_u32(record.cpu, 0));
        const u32 rows = read_u32(record.cpu, 4);
        auto table = std::make_unique<RuntimeScriptLookupTable>(RuntimeScriptLookupTable{count,
            rows == 0 ? nullptr : memory(rows, size_t(count) * 0x108).data()});
        for (i32 row = 0; row < count; ++row)
        {
            const u32 name = read_u32({table->rows + size_t(row) * 0x108, 0x108}, 0x104);
            if (name != 0)
            {
                state_row_names[name] = string(name);
            }
        }
        state_tables[record.cpu.data()] = table.get();
        state_tables_.push_back(std::move(table));
    }
}

// PAL and NTSC read the file as it is. NTSC-J reads the PAL-layout view: a literal the NTSC-J code moved goes to
// its overflow address, and a read that starts where the view has no NTSC-J data is missing rather than zero.
// Only the start is checked: spans here are often wider than the retail table, and the bytes past a table's end
// may sit in a gap. Gap bytes inside a span read as zero.
u32 SceneExecutableResources::resolve(u32 address, size_t size) const
{
    if (!ntsc_j_)
    {
        return address;
    }
    if (const auto found = redirects_.find(address); found != redirects_.end())
    {
        return found->second;
    }
    if (address >= kNtscjOverflow)
    {
        return address;
    }
    const auto next = std::upper_bound(covered_.begin(), covered_.end(),
        std::pair<u32, u32>{address, 0xFFFFFFFFu});
    const bool covered = next != covered_.begin() && address < std::prev(next)->second;
    if (missing_.contains(address) || !covered)
    {
        std::ostringstream message;
        message << "NTSC-J has no data for PAL 0x" << std::hex << std::uppercase << address << " (+0x" << size
                << "); the code reading it differs between builds, see docs/REGIONS.md";
        throw Error(message.str());
    }
    return address;
}

std::span<const u8> SceneExecutableResources::memory(u32 address, size_t size) const
{
    address = resolve(address, size);
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

const char* SceneExecutableResources::string(u32 address) const
{
    const auto first = memory(address, 1);
    address = resolve(address, 1);
    for (const auto& section : info_.sections)
    {
        if (address >= section.virt_addr && address - section.virt_addr < section.raw_size)
        {
            const size_t available = section.raw_size - (address - section.virt_addr);
            if (std::memchr(first.data(), 0, available) == nullptr)
            {
                throw Error("executable string has no terminator in its section");
            }
            return reinterpret_cast<const char*>(first.data());
        }
    }
    throw Error("executable string is outside file-backed XBE memory");
}

u32 SceneExecutableResources::address_of(const void* pointer) const
{
    const auto* byte = static_cast<const u8*>(pointer);
    if (byte < bytes_.data() || byte >= bytes_.data() + bytes_.size())
    {
        return 0;
    }
    const size_t offset = size_t(byte - bytes_.data());
    for (const auto& section : info_.sections)
    {
        if (offset >= section.raw_addr && offset - section.raw_addr < section.raw_size)
        {
            return section.virt_addr + u32(offset - section.raw_addr);
        }
    }
    return 0;
}

void SceneExecutableResources::read_pool(size_t pool, const char* type, u32 address, u32 count, bool active)
{
    auto& records = records_[pool];
    records.reserve(count);
    const auto table = memory(address, size_t(count) * 0x8C);
    const std::string prefix = std::string("aid_") + type + "_";
    for (u32 index = 0; index < count; ++index)
    {
        const auto row = table.subspan(size_t(index) * 0x8C, 0x8C);
        const auto* end = static_cast<const u8*>(std::memchr(row.data(), 0, 0x80));
        if (end == nullptr)
        {
            throw Error("executable resource name has no terminator");
        }
        std::string name(reinterpret_cast<const char*>(row.data()), size_t(end - row.data()));
        if (!name.starts_with(prefix))
        {
            throw Error("executable resource table does not match this retail XBE: " + name);
        }
        const u32 cpu_address = read_u32(row, 0x80);
        const u32 recorded_size = read_u32(row, 0x84);
        const auto cpu = memory(cpu_address, recorded_size);
        if (pool == 3 || pool == 5)
        {
            if (cpu.size() < 2 || read_u16(cpu, 0) != cpu.size())
            {
                throw Error("executable objparams size disagrees with its clone header: " + name);
            }
        }
        records.push_back({std::move(name), cpu, {}, 0});
    }
    register_executable_resource_pool(pools, records, type, active);
}

// Pointer-width adapter for an animtable: each 0x118-byte record holds a name pointer at +0, the
// animation and events names inline at +4 and +0x84, four floats from +0x104 and four bytes from
// +0x114. Each 8-byte alias holds name and target pointers.
void SceneExecutableResources::decode_animation_table(const ExecutableResourceRecord& record)
{
    auto decoded = std::make_unique<DecodedAnimationTable>();
    const auto header = record.cpu;
    const u32 count = read_u32(header, 0);
    const u32 alias_count = read_u32(header, 8);
    if (count != 0)
    {
        const auto rows = memory(read_u32(header, 4), size_t(count) * 0x118);
        decoded->records.reserve(count);
        for (u32 index = 0; index < count; ++index)
        {
            const auto row = rows.subspan(size_t(index) * 0x118, 0x118);
            const auto inline_name = [&row](size_t offset)
            {
                const auto* text = reinterpret_cast<const char*>(row.data() + offset);
                return std::string_view(text, ::strnlen(text, 0x80));
            };
            decoded->records.push_back(AnimationRecord{string(read_u32(row, 0)), inline_name(4), inline_name(0x84),
                read_f32(row, 0x104), read_f32(row, 0x108), read_f32(row, 0x10C), read_f32(row, 0x110),
                std::int8_t(row[0x114]), std::int8_t(row[0x115]), std::int8_t(row[0x116]), std::int8_t(row[0x117])});
        }
    }
    if (alias_count != 0)
    {
        const auto rows = memory(read_u32(header, 0xC), size_t(alias_count) * 8);
        decoded->aliases.reserve(alias_count);
        for (u32 index = 0; index < alias_count; ++index)
        {
            decoded->aliases.push_back(AnimationAlias{string(read_u32(rows, index * 8)),
                string(read_u32(rows, index * 8 + 4))});
        }
    }
    decoded->table = AnimationTable{decoded->records, decoded->aliases};
    animation_tables.tables[record.cpu.data()] = &decoded->table;
    tables_.push_back(std::move(decoded));
}
}
