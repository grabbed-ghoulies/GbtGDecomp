#include "ntsc_j_layout.h"
#include <algorithm>
#include <cstring>

namespace gbtg
{
namespace
{
struct NtscjSection
{
    u32 address;
    u32 size;
};

struct NtscjBlock
{
    u32 pal;
    u32 ntscj;
    u32 size;
};

struct NtscjPair
{
    u32 first;
    u32 second;
};

#include "ntsc_j_address_map.inl"

const XbeSection* find_section(const XbeInfo& info, const char* name)
{
    for (const auto& section : info.sections)
    {
        if (section.name == name)
        {
            return &section;
        }
    }
    return nullptr;
}
}

bool is_ntsc_j_build(const XbeInfo& info)
{
    return info.timestamp == kNtscjTimestamp;
}

NtscjView build_ntsc_j_view(const XbeInfo& info, const std::vector<u8>& bytes)
{
    const XbeSection* rdata = find_section(info, ".rdata");
    const XbeSection* data = find_section(info, ".data");
    if (rdata == nullptr || data == nullptr)
    {
        throw Error("NTSC-J default.xbe has no .rdata/.data section");
    }

    NtscjView view;
    // PAL data sections first, at their PAL addresses, then the whole NTSC-J file behind the overflow base.
    u32 offset = 0;
    for (const auto& section : kPalDataSections)
    {
        view.sections.push_back({"pal", 0, section.address, section.size, offset, section.size});
        offset += section.size;
    }
    view.bytes.assign(offset, 0);
    view.bytes.insert(view.bytes.end(), bytes.begin(), bytes.end());
    for (const auto& section : info.sections)
    {
        XbeSection moved = section;
        moved.virt_addr += kNtscjOverflow;
        moved.raw_addr += offset;
        view.sections.push_back(moved);
    }

    const auto ntscj_at = [&](u32 address, size_t size) -> const u8*
    {
        for (const auto* section : {rdata, data})
        {
            if (address >= section->virt_addr && address - section->virt_addr + size <= section->raw_size)
            {
                return bytes.data() + section->raw_addr + (address - section->virt_addr);
            }
        }
        return nullptr;
    };
    const auto pal_at = [&](u32 address) -> u8*
    {
        for (size_t i = 0; i < std::size(kPalDataSections); ++i)
        {
            const auto& section = kPalDataSections[i];
            if (address >= section.address && address - section.address < section.size)
            {
                return view.bytes.data() + view.sections[i].raw_addr + (address - section.address);
            }
        }
        return nullptr;
    };

    for (const auto& block : kBlocks)
    {
        const u8* from = ntscj_at(block.ntscj, block.size);
        u8* to = pal_at(block.pal);
        if (from == nullptr || to == nullptr)
        {
            throw Error("NTSC-J address map block lies outside the data sections");
        }
        std::memcpy(to, from, block.size);
        view.covered.emplace_back(block.pal, block.pal + block.size);
    }
    std::sort(view.covered.begin(), view.covered.end());

    // A pointer into NTSC-J data goes to the PAL address its block landed at, or to the overflow when the
    // target has no PAL place. A pointer into code takes the PAL function from the same slot.
    std::vector<NtscjBlock> by_ntscj(std::begin(kBlocks), std::end(kBlocks));
    std::sort(by_ntscj.begin(), by_ntscj.end(), [](const NtscjBlock& a, const NtscjBlock& b)
    {
        return a.ntscj < b.ntscj;
    });
    const auto in_data = [&](u32 address)
    {
        return (address >= rdata->virt_addr && address - rdata->virt_addr < rdata->raw_size) ||
            (address >= data->virt_addr && address - data->virt_addr < data->raw_size);
    };
    for (const u32 position : kRelocations)
    {
        u8* at = pal_at(position);
        u32 value = 0;
        std::memcpy(&value, at, 4);
        if (!in_data(value))
        {
            continue;
        }
        auto next = std::upper_bound(by_ntscj.begin(), by_ntscj.end(), value, [](u32 v, const NtscjBlock& b)
        {
            return v < b.ntscj;
        });
        if (next != by_ntscj.begin() && value - std::prev(next)->ntscj < std::prev(next)->size)
        {
            value = std::prev(next)->pal + (value - std::prev(next)->ntscj);
        }
        else
        {
            value += kNtscjOverflow;
        }
        std::memcpy(at, &value, 4);
    }
    for (const auto& pointer : kCodePointers)
    {
        std::memcpy(pal_at(pointer.first), &pointer.second, 4);
    }

    for (const auto& redirect : kRedirects)
    {
        view.redirects[redirect.first] = kNtscjOverflow + redirect.second;
    }
    view.missing.insert(std::begin(kMissing), std::end(kMissing));
    return view;
}
}
