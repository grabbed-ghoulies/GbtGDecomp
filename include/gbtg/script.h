#pragma once

#include "gbtg/types.h"
#include <span>
#include <string>

namespace gbtg
{

// BNL type 24 (`aid_script_*`). Packed nodes, each
// `{u32 size, u32 type, u8 payload[size - 8]}`. `size` includes the header
// and is the byte count the binder adds to the node (`node + *(u32*)node`),
// which is the next node. A type of 0 ends the blob. Chapter `*_setup`
// scripts are a type-6 header plus that terminator.
// A node read in place: its offset in the resource, its size and type words, and its link (offset + size).
struct ScriptNode
{
    u32 offset = 0;
    u32 size = 0;
    u32 type = 0;
    u32 link = 0; // offset + size. The condition record stores this.
};

// The running script resource as 0x00117000 binds it (DAT_0050E760): the AID's bytes, held by the resource
// loader for the session, and its name. The VM walks and reads its nodes in place.
struct SceneScript
{
    std::string name;
    std::span<const u8> bytes;
};

} // namespace gbtg
