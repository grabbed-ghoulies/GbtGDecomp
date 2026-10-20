#pragma once

#include "gbtg/types.h"
#include <functional>
#include <vector>

namespace gbtg
{
struct ActorLoadRecord
{
    ActorLoadRecord* prev = nullptr; // Xbox +0
    ActorLoadRecord* next = nullptr; // Xbox +4
    u8 flags_2c = 0;                 // Xbox +0x2C, bit 0 marks completion
};

using ActorLoadList = ActorLoadRecord;

// DAT_00510568/0051056C, also cleared by 0x000F4F30.
extern ActorLoadList g_side_list;

// One DAT_00511B38 actor as 0x001412F0 reads it through its +0x934 node: the front kind of its +0x980 goal ring
// (node +0x4C), and its +0x984 (node +0x50) and +0x98C (node +0x58) words.
struct RuntimeLoadingActor
{
    u16 goal = 0;
    u32 word_984 = 0;
    u32 word_98c = 0;
};
// The native scene's DAT_00511B38 walk, in list order. Unset outside a native scene, where the host range links
// stand in.
extern std::function<std::vector<RuntimeLoadingActor>()> g_loading_actors;
bool actor_loading_complete(); // Xbox 0x001412F0
}
