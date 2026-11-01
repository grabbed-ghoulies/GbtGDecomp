#pragma once

#include "gbtg/types.h"

namespace gbtg
{

struct XbeSection
{
    std::string name;
    u32 flags = 0;
    u32 virt_addr = 0;
    u32 virt_size = 0;
    u32 raw_addr = 0;
    u32 raw_size = 0;
};

struct XbeLibrary
{
    std::string name;
    u16 major = 0, minor = 0, build = 0, qfe = 0;
};

struct XbeInfo
{
    std::filesystem::path path;
    u32 base = 0;
    u32 timestamp = 0; // Header +0x114. Tells the builds apart: PAL and NTSC share one, NTSC-J has its own.
    u32 entry_decoded = 0;
    u32 thunk_decoded = 0;
    u32 title_id = 0;
    std::string title;
    std::string debug_path;
    std::string debug_file;
    std::vector<XbeSection> sections;
    std::vector<XbeLibrary> libraries;
    std::vector<std::string> aid_strings;
    std::vector<std::string> interesting_strings;
};

XbeInfo parse_xbe(const std::filesystem::path& path);

// Overlay FPS fly cam on BanjoCam body pose (+0x89c); freeze sim dt while flying.
void apply_preview_freecam(const std::filesystem::path& xbe_path);

} // namespace gbtg
