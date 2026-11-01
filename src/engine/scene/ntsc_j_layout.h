#pragma once
#include "gbtg/xbe.h"
#include <unordered_map>
#include <unordered_set>

namespace gbtg
{
// NTSC-J default.xbe read in the PAL data layout. The port reads retail tables and constants at PAL addresses;
// NTSC-J is a later, re-linked build where that data moved. The view puts each NTSC-J data block at its PAL
// address and rewrites the pointers inside it, so PAL addresses and the pointers read through them agree.
// NTSC-J data with no PAL place stays readable at kNtscjOverflow + its NTSC-J address. See docs/REGIONS.md.
constexpr u32 kNtscjOverflow = 0x01000000;

struct NtscjView
{
    std::vector<u8> bytes;
    std::vector<XbeSection> sections;           // PAL .rdata/.data, then the NTSC-J sections at the overflow.
    std::vector<std::pair<u32, u32>> covered;   // PAL ranges [first, second) the view has NTSC-J data for.
    std::unordered_map<u32, u32> redirects;     // PAL literal -> overflow address the NTSC-J code uses instead.
    std::unordered_set<u32> missing;            // PAL literals with no NTSC-J answer.
};

bool is_ntsc_j_build(const XbeInfo& info);
// `bytes` is the whole NTSC-J default.xbe and `info` its parse.
NtscjView build_ntsc_j_view(const XbeInfo& info, const std::vector<u8>& bytes);
}
