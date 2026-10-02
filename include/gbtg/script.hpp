#pragma once

#include "gbtg/types.hpp"

namespace gbtg
{

// BNL type 24 (`aid_script_*`). Relocatable pointer blob: first u32 is often a root
// offset; later u32s that fall inside the buffer are pointers. Names (background
// model, objparams, aidlists) sit as NUL-terminated ASCII.
struct SceneScript
{
    std::string name;
    u32 root_offset = 0;
    std::vector<u32> words;
    std::vector<std::string> strings;
    std::vector<std::string> aids;
};

SceneScript parse_scene_script(std::span<const u8> cpu, std::string_view name = {});

} // namespace gbtg
