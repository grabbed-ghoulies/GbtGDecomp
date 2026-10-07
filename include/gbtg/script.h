#pragma once

#include "gbtg/types.h"

namespace gbtg
{

// BNL type 24 (`aid_script_*`). Packed nodes, each
// `{u32 size, u32 type, u8 payload[size - 8]}`. `size` includes the header
// and is the byte count the binder adds to the node (`node + *(u32*)node`),
// which is the next node. A type of 0 ends the blob. Chapter `*_setup`
// scripts are a type-6 header plus that terminator.
struct ScriptNode
{
    u32 offset = 0;
    u32 size = 0;
    u32 type = 0;
    u32 link = 0; // offset + size. The condition record stores this.
    std::string text;
};

struct SceneScript
{
    std::string name;
    u32 root_offset = 0;
    std::vector<u8> bytes;
    std::vector<u32> words;
    std::vector<ScriptNode> nodes;
    std::vector<std::string> strings;
    std::vector<std::string> aids;
};

SceneScript parse_scene_script(std::span<const u8> cpu, std::string_view name = {});

} // namespace gbtg
